import { defineConfig } from 'vite'
import { getPluginsConfig } from './src/config/vite/plugins'
import { getBuildConfig } from './src/config/vite/build'
import { getServerConfig } from './src/config/vite/server'
import { getAliasConfig } from './src/config/vite/alias'
import { EntryFileGenerator } from './src/config/entry-generator'
import { resolve } from 'path'

// 创建入口文件生成器实例
const entryGenerator = new EntryFileGenerator()

// 生成入口文件
entryGenerator.writeEntryFile()

export default defineConfig({
    plugins: getPluginsConfig(),
    resolve: {
        alias: getAliasConfig()
    },
    build: getBuildConfig(),
    server: getServerConfig(),
    root: process.cwd()
})