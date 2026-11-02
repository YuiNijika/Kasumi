# kasumi-plugin-sql

> 使用 Kasumi SQL 插件连接数据库并执行参数化语句。

## 安装

```bash
pnpm add kasumi-bridge kasumi-plugin-sql
```

## 用法

```ts
import { KSM } from "kasumi-bridge";
import { close, drivers, execute, open, query } from "kasumi-plugin-sql";

KSM.init();

const available = await drivers();
if (!available.drivers.includes("sqlite")) {
  throw new Error("当前宿主没有 SQLite 驱动");
}

const database = await open({
  driver: "sqlite",
  url: "app://data/example.db"
});

await execute({
  connectionId: database.connectionId,
  statement: "CREATE TABLE IF NOT EXISTS notes (id INTEGER PRIMARY KEY, body TEXT)"
});

const rows = await query({
  connectionId: database.connectionId,
  statement: "SELECT id, body FROM notes"
});

await close({ connectionId: database.connectionId });
console.log(rows.rows);
```

驱动是否可用以 `drivers()` 为准。SQLite 内置；MySQL、PostgreSQL 和 MongoDB
取决于宿主注册的驱动，详细边界见线上 SQL 插件文档。
