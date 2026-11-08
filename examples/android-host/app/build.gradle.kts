import groovy.json.JsonSlurper

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val repositoryRoot = rootProject.rootDir.parentFile.parentFile
val applicationConfig = JsonSlurper().parse(
    repositoryRoot.resolve("kasumi.config.json")
) as Map<*, *>
val androidConfig =
    applicationConfig["android"] as? Map<*, *> ?: emptyMap<String, Any>()
val webViewConfig =
    androidConfig["webview"] as? Map<*, *> ?: emptyMap<String, Any>()

val kasumiVersion = rootProject.extra["kasumiVersion"] as String
val ksmPackageName = providers.gradleProperty("ksmPackageName")
    .orElse(androidConfig["packageName"] as? String ?: "dev.kasumi.example")
    .get()
val ksmVersion = providers.gradleProperty("ksmVersion")
    .orElse(androidConfig["version"] as? String ?: kasumiVersion)
    .get()

fun splitCsv(value: String): List<String> =
    value.split(',')
        .map { item -> item.trim() }
        .filter { item -> item.isNotEmpty() }

fun csvProperty(name: String, fallback: List<String> = emptyList()): List<String> =
    providers.gradleProperty(name).orNull?.let(::splitCsv) ?: fallback

fun configList(section: String, name: String): List<String> {
    val values =
        (androidConfig[section] as? Map<*, *>)?.get(name) as? List<*>
            ?: return emptyList()
    return values.mapNotNull { value -> value as? String }
}

fun protocolCommands(): List<String> {
    val protocol = JsonSlurper().parse(
        repositoryRoot.resolve("protocol/commands.json")
    ) as Map<*, *>
    val commands = protocol["commands"] as? Map<*, *> ?: return emptyList()
    return commands.entries.mapNotNull { entry ->
        val platforms = (entry.value as? Map<*, *>)?.get("platforms") as? List<*>
        (entry.key as? String)?.takeIf { platforms?.contains("android") == true }
    }
}

fun protocolPermissions(): List<String> {
    val schema = JsonSlurper().parse(
        repositoryRoot.resolve("protocol/permissions.schema.json")
    ) as Map<*, *>
    val properties = schema["properties"] as? Map<*, *> ?: return emptyList()
    return properties.keys.mapNotNull { key -> key as? String }
}

fun configuredCommands(): List<String> {
    val fallback = when (val value = androidConfig["commands"]) {
        "all" -> protocolCommands()
        is List<*> -> value.mapNotNull { command -> command as? String }
        else -> emptyList()
    }
    return csvProperty("ksmCommands", fallback)
}

fun configuredPermissions(): List<String> {
    val fallback = when (val value = androidConfig["permissions"]) {
        "all" -> protocolPermissions()
        is List<*> -> value.mapNotNull { permission -> permission as? String }
        else -> emptyList()
    }
    return csvProperty("ksmPermissions", fallback)
}

fun stringArrayLiteral(values: List<String>): String =
    values.joinToString(prefix = "{", postfix = "}", separator = ", ") { value ->
        val escaped = value
            .replace("\\", "\\\\")
            .replace("\"", "\\\"")
        "\"$escaped\""
    }

val ksmCommands = configuredCommands()
val ksmPermissions = configuredPermissions()
val ksmShellAllowedOrigins = csvProperty(
    "ksmShellAllowedOrigins",
    configList("shell", "allowedOrigins")
)
val ksmHttpAllowedOrigins = csvProperty(
    "ksmHttpAllowedOrigins",
    configList("http", "allowedOrigins")
)
val ksmWebViewProductionEntry = providers.gradleProperty("ksmWebViewProductionEntry")
    .orElse(
        webViewConfig["productionEntry"] as? String
            ?: "https://app.local/index.html"
    )
    .get()
val ksmWebViewDevelopmentEntries = csvProperty(
    "ksmWebViewDevelopmentEntries",
    (webViewConfig["developmentEntries"] as? List<*>)
        ?.mapNotNull { entry -> entry as? String }
        ?.takeIf { entries -> entries.isNotEmpty() }
        ?: listOf(
            "http://localhost:5173/index.html",
            "http://localhost:5173/",
            "http://localhost:5173"
        )
)
val ksmWebViewSupportedHtmlFiles = csvProperty(
    "ksmWebViewSupportedHtmlFiles",
    (webViewConfig["supportedHtmlFiles"] as? List<*>)
        ?.mapNotNull { file -> file as? String }
        ?.takeIf { files -> files.isNotEmpty() }
        ?: listOf("index.html")
)
val releaseKeystore = System.getenv("KASUMI_ANDROID_KEYSTORE")
    ?.takeIf { it.isNotBlank() }
    ?.let { file(it) }
    ?.takeIf { it.isFile }
val releaseStorePassword = System.getenv("KASUMI_ANDROID_KEYSTORE_PASSWORD")
val releaseKeyAlias = System.getenv("KASUMI_ANDROID_KEY_ALIAS")
val releaseKeyPassword = System.getenv("KASUMI_ANDROID_KEY_PASSWORD")
val hasReleaseSigning =
    releaseKeystore != null &&
        !releaseStorePassword.isNullOrBlank() &&
        !releaseKeyAlias.isNullOrBlank() &&
        !releaseKeyPassword.isNullOrBlank()

android {
    namespace = "dev.kasumi.example"
    compileSdk = 36

    defaultConfig {
        applicationId = ksmPackageName
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = ksmVersion
        buildConfigField(
            "String[]",
            "KSM_COMMANDS",
            stringArrayLiteral(ksmCommands)
        )
        buildConfigField(
            "String[]",
            "KSM_PERMISSIONS",
            stringArrayLiteral(ksmPermissions)
        )
        buildConfigField(
            "String[]",
            "KSM_SHELL_ALLOWED_ORIGINS",
            stringArrayLiteral(ksmShellAllowedOrigins)
        )
        buildConfigField(
            "String[]",
            "KSM_HTTP_ALLOWED_ORIGINS",
            stringArrayLiteral(ksmHttpAllowedOrigins)
        )
        buildConfigField(
            "String",
            "KSM_WEBVIEW_PRODUCTION_ENTRY",
            "\"${ksmWebViewProductionEntry.replace("\\", "\\\\").replace("\"", "\\\"")}\""
        )
        buildConfigField(
            "String[]",
            "KSM_WEBVIEW_DEVELOPMENT_ENTRIES",
            stringArrayLiteral(
                ksmWebViewDevelopmentEntries.ifEmpty {
                    listOf(
                        "http://localhost:5173/index.html",
                        "http://localhost:5173/",
                        "http://localhost:5173"
                    )
                }
            )
        )
        buildConfigField(
            "String[]",
            "KSM_WEBVIEW_SUPPORTED_HTML_FILES",
            stringArrayLiteral(
                ksmWebViewSupportedHtmlFiles.ifEmpty { listOf("index.html") }
            )
        )
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = releaseKeystore
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
                enableV1Signing = true
                enableV2Signing = true
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (hasReleaseSigning) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        buildConfig = true
    }

    sourceSets {
        getByName("main") {
            assets.srcDir("../../showcase")
        }
    }
}

dependencies {
    implementation("dev.kasumi:kasumi:$kasumiVersion")
}
