import * as fs from 'fs'
import * as path from 'path'
import { resolve } from 'path'
import { uiConfig } from './ui' 

export class EntryFileGenerator {
    private entryFile: string
    private componentsDir: string

    constructor() {
        this.entryFile = resolve(__dirname, '../../src/config/entry-components.ts')
        this.componentsDir = resolve(__dirname, '../../src/components')
    }

    // 递归获取所有.vue文件
    public getAllVueFiles(dir: string): string[] {
        let results: string[] = []
        const list = fs.readdirSync(dir)
        list.forEach((file) => {
            file = resolve(dir, file)
            const stat = fs.statSync(file)
            if (stat && stat.isDirectory()) {
                results = [...results, ...this.getAllVueFiles(file)]
            } else if (file.endsWith('.vue')) {
                results.push(file)
            }
        })
        return results
    }

    // 生成入口文件内容
    public generateEntryContent(): string {
        const vueFiles = this.getAllVueFiles(this.componentsDir)

        // 生成统一入口文件的内容
        let entryContent = '';
        
        // 根据配置决定导入哪种样式
        if (uiConfig.framework === 'antdv') {
            entryContent += `import 'ant-design-vue/dist/reset.css';\n\n`
        } else if (uiConfig.framework === 'tailwindcss') {
            entryContent += `import '../styles/main.scss';\n\n`
        }

        vueFiles.forEach(file => {
            const relativePath = path.relative(this.componentsDir, file).replace(/\\/g, '/')
            const name = relativePath.replace('.vue', '').replace(/\//g, '_')
            entryContent += `import ${name} from '../components/${relativePath}'\n`
        })

        entryContent += `\n// 挂载所有组件的函数\nexport function mountAllComponents() {\n`

        vueFiles.forEach(file => {
            const relativePath = path.relative(this.componentsDir, file).replace(/\\/g, '/')
            const name = relativePath.replace('.vue', '').replace(/\//g, '_')
            entryContent += `  const ${name}Elements = document.querySelectorAll('[data-component="${name}"]');\n`
            entryContent += `  ${name}Elements.forEach(el => {\n`
            entryContent += `    const ${name}App = createApp(${name});\n`
            entryContent += `    ${name}App.mount(el);\n`
            entryContent += `  });\n`
        })

        entryContent += `}\n\n`

        entryContent += `import { createApp } from 'vue';\n\n`
        entryContent += `// 如果在浏览器环境中，立即挂载所有组件\n`
        entryContent += `if (typeof window !== 'undefined') {\n`
        entryContent += `  if (document.readyState === 'loading') {\n`
        entryContent += `    document.addEventListener('DOMContentLoaded', mountAllComponents);\n`
        entryContent += `  } else {\n`
        entryContent += `    mountAllComponents();\n`
        entryContent += `  }\n`
        entryContent += `}\n`

        return entryContent
    }

    // 写入入口文件
    public writeEntryFile(): void {
        const content = this.generateEntryContent()
        fs.writeFileSync(this.entryFile, content)
    }

    // 获取入口文件路径
    public getEntryFilePath(): string {
        return this.entryFile
    }
}