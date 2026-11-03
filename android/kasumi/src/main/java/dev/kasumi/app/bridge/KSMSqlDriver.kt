package dev.kasumi.app.bridge

import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import android.os.CancellationSignal
import android.os.OperationCanceledException
import android.util.Base64
import java.io.Closeable
import java.util.concurrent.atomic.AtomicLong

private const val MAX_SQL_CONNECTIONS = 32
private const val MAX_SQL_RESULT_ROWS = 10000
private const val MAX_SQL_RESULT_BYTES = 16 * 1024 * 1024

private fun sqlValueBytes(value: Any?): Int {
    return when (value) {
        null, is Boolean -> 1
        is ByteArray -> value.size
        is String -> value.toByteArray(Charsets.UTF_8).size
        is Number -> 8
        else -> 0
    }
}

private fun validateSqlResult(result: KSMSqlQueryResult) {
    if (result.rows.size > MAX_SQL_RESULT_ROWS) {
        throw KSMHostException(
            "RESOURCE_LIMIT",
            "The SQL result exceeds the maximum row count."
        )
    }
    var bytes = 0
    for (row in result.rows) {
        for (value in row) {
            bytes += sqlValueBytes(value)
            if (bytes > MAX_SQL_RESULT_BYTES) {
                throw KSMHostException(
                    "RESOURCE_LIMIT",
                    "The SQL result exceeds the maximum payload size."
                )
            }
        }
    }
}

interface KSMSqlDriver {
    val name: String

    fun open(url: String): KSMSqlConnection
}

interface KSMSqlConnection : Closeable {
    fun query(
        statement: String,
        params: List<Any?>
    ): KSMSqlQueryResult

    fun execute(
        statement: String,
        params: List<Any?>
    ): KSMSqlExecuteResult

    fun query(
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlQueryResult {
        return query(statement, params)
    }

    fun execute(
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlExecuteResult {
        return execute(statement, params)
    }

    fun cancel(requestId: String) {}
}

class KSMSqlDriverRegistry {
    private val lock = Any()
    private val drivers = linkedMapOf<String, KSMSqlDriver>()
    private val connections = mutableMapOf<String, KSMSqlConnection>()
    private val activeRequests = mutableMapOf<String, KSMSqlConnection>()
    private val nextConnectionId = AtomicLong(1)
    private var pendingConnections = 0

    fun register(driver: KSMSqlDriver) {
        require(driver.name.isNotBlank()) {
            "A Kasumi SQL driver must have a name."
        }
        synchronized(lock) {
            require(drivers.putIfAbsent(driver.name, driver) == null) {
                "Kasumi SQL driver names must be unique."
            }
        }
    }

    fun drivers(): List<String> {
        return synchronized(lock) {
            drivers.keys.toList()
        }
    }

    fun open(driverName: String, url: String): String? {
        val driver = synchronized(lock) {
            drivers[driverName]
        } ?: return null
        synchronized(lock) {
            if (
                connections.size + pendingConnections >=
                MAX_SQL_CONNECTIONS
            ) {
                throw KSMHostException(
                    "RESOURCE_LIMIT",
                    "The SQL connection limit has been reached."
                )
            }
            pendingConnections += 1
        }
        val connection = try {
            driver.open(url)
        } catch (error: Exception) {
            synchronized(lock) {
                pendingConnections -= 1
            }
            throw error
        }
        val connectionId = "sql-${nextConnectionId.getAndIncrement()}"
        synchronized(lock) {
            pendingConnections -= 1
            connections[connectionId] = connection
        }
        return connectionId
    }

    fun query(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String? = null
    ): KSMSqlQueryResult {
        val connection = requireConnection(connectionId)
        registerRequest(requestId, connection)
        return try {
            val result = synchronized(connection) {
                connection.query(statement, params, requestId)
            }
            validateSqlResult(result)
            result
        } finally {
            unregisterRequest(requestId, connection)
        }
    }

    fun execute(
        connectionId: String,
        statement: String,
        params: List<Any?>,
        requestId: String? = null
    ): KSMSqlExecuteResult {
        val connection = requireConnection(connectionId)
        registerRequest(requestId, connection)
        return try {
            synchronized(connection) {
                connection.execute(statement, params, requestId)
            }
        } finally {
            unregisterRequest(requestId, connection)
        }
    }

    fun cancel(requestId: String) {
        if (requestId.isEmpty()) {
            return
        }
        val connection = synchronized(lock) {
            activeRequests[requestId]
        } ?: return
        connection.cancel(requestId)
    }

    fun close(connectionId: String): Boolean {
        val connection = synchronized(lock) {
            connections.remove(connectionId)
        } ?: return false
        synchronized(connection) {
            connection.close()
        }
        return true
    }

    fun closeAll() {
        val active = synchronized(lock) {
            val values = connections.values.toList()
            connections.clear()
            values
        }
        active.forEach { connection ->
            try {
                synchronized(connection) {
                    connection.close()
                }
            } catch (_: RuntimeException) {
                // Closing one failed database must not leak the others.
            }
        }
    }

    private fun requireConnection(connectionId: String): KSMSqlConnection {
        return synchronized(lock) {
            connections[connectionId]
        } ?: throw KSMHostException(
            "NOT_FOUND",
            "The SQL connection does not exist."
        )
    }

    private fun registerRequest(
        requestId: String?,
        connection: KSMSqlConnection
    ) {
        if (requestId.isNullOrEmpty()) {
            return
        }
        synchronized(lock) {
            if (activeRequests.putIfAbsent(requestId, connection) != null) {
                throw KSMHostException(
                    "BUSY",
                    "A SQL request with this ID is already in flight."
                )
            }
        }
    }

    private fun unregisterRequest(
        requestId: String?,
        connection: KSMSqlConnection
    ) {
        if (requestId.isNullOrEmpty()) {
            return
        }
        synchronized(lock) {
            if (activeRequests[requestId] === connection) {
                activeRequests.remove(requestId)
            }
        }
    }
}

class KSMSqliteDriver(
    private val fileSystem: KSMFileSystem?
) : KSMSqlDriver {
    override val name: String = "sqlite"

    override fun open(url: String): KSMSqlConnection {
        val database = try {
            if (url == ":memory:") {
                SQLiteDatabase.create(null)
            } else {
                if (
                    !KSMVirtualPath.isSafe(url) ||
                    KSMVirtualPath.isReadOnly(url) ||
                    KSMVirtualPath.isPicked(url)
                ) {
                    throw KSMHostException(
                        "PERMISSION_DENIED",
                        "SQLite databases must use a writable Kasumi virtual path."
                    )
                }
                val file = fileSystem?.resolveWritableFile(url)
                    ?: throw KSMHostException(
                        "PERMISSION_DENIED",
                        "The database path is outside the writable sandbox."
                    )
                file.parentFile?.mkdirs()
                SQLiteDatabase.openOrCreateDatabase(file, null)
            }
        } catch (error: SQLiteException) {
            throw KSMHostException(
                "IO_ERROR",
                error.message ?: "SQLite open failed."
            )
        }
        database.execSQL("PRAGMA foreign_keys = ON")
        return KSMSqliteConnection(database)
    }
}

private class KSMSqliteConnection(
    private val database: SQLiteDatabase
) : KSMSqlConnection {
    private val cancellationSignals =
        mutableMapOf<String, CancellationSignal>()

    override fun query(
        statement: String,
        params: List<Any?>
    ): KSMSqlQueryResult {
        return query(statement, params, null)
    }

    override fun query(
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlQueryResult {
        val args = params.map(::sqlArgument).toTypedArray()
        val signal = createCancellationSignal(requestId)
        return try {
            database.rawQuery(statement, args, signal).use { cursor ->
                val columns = cursor.columnNames.toList()
                val rows = mutableListOf<List<Any?>>()
                var resultBytes = 0
                while (cursor.moveToNext()) {
                    signal?.throwIfCanceled()
                    if (rows.size >= MAX_SQL_RESULT_ROWS) {
                        throw KSMHostException(
                            "RESOURCE_LIMIT",
                            "The SQL result exceeds the maximum row count."
                        )
                    }
                    val row = columns.indices.map { readValue(cursor, it) }
                    for (value in row) {
                        resultBytes += sqlValueBytes(value)
                        if (resultBytes > MAX_SQL_RESULT_BYTES) {
                            throw KSMHostException(
                                "RESOURCE_LIMIT",
                                "The SQL result exceeds the maximum payload size."
                            )
                        }
                    }
                    rows.add(row)
                }
                KSMSqlQueryResult(columns, rows)
            }
        } catch (_: OperationCanceledException) {
            throw KSMHostException(
                "CANCELLED",
                "The SQL request was cancelled."
            )
        } catch (error: SQLiteException) {
            if (signal?.isCanceled == true) {
                throw KSMHostException(
                    "CANCELLED",
                    "The SQL request was cancelled."
                )
            }
            throw KSMHostException(
                "INVALID_ARGS",
                error.message ?: "SQL query failed."
            )
        } finally {
            removeCancellationSignal(requestId, signal)
        }
    }

    override fun execute(
        statement: String,
        params: List<Any?>
    ): KSMSqlExecuteResult {
        return execute(statement, params, null)
    }

    override fun execute(
        statement: String,
        params: List<Any?>,
        requestId: String?
    ): KSMSqlExecuteResult {
        val args = params.map(::sqlArgument).toTypedArray()
        val signal = createCancellationSignal(requestId)
        return try {
            signal?.throwIfCanceled()
            database.execSQL(statement, args)
            signal?.throwIfCanceled()
            val changes = database.rawQuery(
                "SELECT changes()",
                null,
                signal
            ).use { cursor ->
                if (!cursor.moveToFirst()) {
                    return@use 0L
                }
                cursor.getLong(0)
            }
            val insertsRows = Regex(
                "^\\s*(insert|replace)\\b",
                RegexOption.IGNORE_CASE
            ).containsMatchIn(statement)
            val mutatesRows = insertsRows || Regex(
                "^\\s*(update|delete)\\b",
                RegexOption.IGNORE_CASE
            ).containsMatchIn(statement)
            val lastInsertId = if (insertsRows && changes > 0) {
                database.rawQuery("SELECT last_insert_rowid()", null).use { cursor ->
                    if (cursor.moveToFirst()) cursor.getLong(0) else null
                }
            } else {
                null
            }
            KSMSqlExecuteResult(
                rowsAffected = if (mutatesRows) changes else 0L,
                lastInsertId = lastInsertId
            )
        } catch (_: OperationCanceledException) {
            throw KSMHostException(
                "CANCELLED",
                "The SQL request was cancelled."
            )
        } catch (error: SQLiteException) {
            if (signal?.isCanceled == true) {
                throw KSMHostException(
                    "CANCELLED",
                    "The SQL request was cancelled."
                )
            }
            throw KSMHostException(
                "INVALID_ARGS",
                error.message ?: "SQL execution failed."
            )
        } finally {
            removeCancellationSignal(requestId, signal)
        }
    }

    override fun cancel(requestId: String) {
        synchronized(cancellationSignals) {
            cancellationSignals[requestId]
        }?.cancel()
    }

    override fun close() {
        database.close()
    }

    private fun readValue(cursor: Cursor, index: Int): Any? {
        return when (cursor.getType(index)) {
            Cursor.FIELD_TYPE_NULL -> null
            Cursor.FIELD_TYPE_INTEGER -> cursor.getLong(index)
            Cursor.FIELD_TYPE_FLOAT -> cursor.getDouble(index)
            Cursor.FIELD_TYPE_STRING -> cursor.getString(index)
            Cursor.FIELD_TYPE_BLOB -> Base64.encodeToString(
                cursor.getBlob(index),
                Base64.NO_WRAP
            )
            else -> null
        }
    }

    private fun sqlArgument(value: Any?): String? {
        return when (value) {
            null -> null
            is Boolean -> if (value) "1" else "0"
            is String -> value
            is Number -> value.toString()
            else -> throw KSMHostException(
                "INVALID_ARGS",
                "SQL parameters must be strings, numbers, booleans or null."
            )
        }
    }

    private fun createCancellationSignal(
        requestId: String?
    ): CancellationSignal? {
        if (requestId.isNullOrEmpty()) {
            return null
        }
        return CancellationSignal().also { signal ->
            synchronized(cancellationSignals) {
                cancellationSignals[requestId] = signal
            }
        }
    }

    private fun removeCancellationSignal(
        requestId: String?,
        signal: CancellationSignal?
    ) {
        if (requestId.isNullOrEmpty() || signal == null) {
            return
        }
        synchronized(cancellationSignals) {
            if (cancellationSignals[requestId] === signal) {
                cancellationSignals.remove(requestId)
            }
        }
    }
}
