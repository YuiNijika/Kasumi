const fs = require('fs')
const path = require('path')
const { parse } = require('@vue/compiler-sfc')
const { parse: babelParse } = require('@babel/parser')
const traverse = require('@babel/traverse').default
const UIParserFactory = require('./ui-parser/index.cjs') 

class VueToPhpConverter {
    constructor(uiParser = UIParserFactory.createParser()) { 
        this.distDir = path.resolve(__dirname, 'dist')
        this.componentsDir = path.resolve(__dirname, 'components')
        this.uiParser = uiParser // 注入UI解析器
    }

    // 确保目录存在
    ensureDistDir() {
        if (!fs.existsSync(this.distDir)) {
            fs.mkdirSync(this.distDir, { recursive: true })
        }
    }

    // 递归获取所有组件文件
    getComponentFiles(dir = this.componentsDir) {
        let results = [];
        const list = fs.readdirSync(dir);
        list.forEach((file) => {
            file = path.resolve(dir, file);
            const stat = fs.statSync(file);
            if (stat && stat.isDirectory()) {
                results = [...results, ...this.getComponentFiles(file)];
            } else if (file.endsWith('.vue')) {
                results.push(file);
            }
        });
        return results;
    }

    // 解析Vue文件
    parseVueFile(filePath) {
        const vueContent = fs.readFileSync(filePath, 'utf-8')
        const { descriptor } = parse(vueContent)

        let templateContent = ''
        let scriptContent = ''

        if (descriptor.template) {
            templateContent = descriptor.template.content.trim()
        }

        if (descriptor.scriptSetup) {
            scriptContent = descriptor.scriptSetup.content
        } else if (descriptor.script) {
            scriptContent = descriptor.script.content
        }

        return { templateContent, scriptContent }
    }

    // 提取组件数据
    extractComponentData(scriptContent) {
        const data = {}

        if (!scriptContent) return data

        try {
            const ast = babelParse(scriptContent, {
                sourceType: 'module',
                plugins: ['typescript']
            })

            traverse(ast, {
                CallExpression(path) {
                    if (path.node.callee.name === 'ref') {
                        const parent = path.parent
                        if (parent.type === 'VariableDeclarator') {
                            const varName = parent.id.name
                            const args = path.node.arguments

                            if (args.length > 0) {
                                if (args[0].type === 'ArrayExpression') {
                                    const arrayData = []
                                    args[0].elements.forEach(element => {
                                        if (element.type === 'ObjectExpression') {
                                            const obj = {}
                                            element.properties.forEach(prop => {
                                                if (prop.key && prop.value) {
                                                    const key = prop.key.name || prop.key.value
                                                    const value = prop.value.value
                                                    obj[key] = value
                                                }
                                            })
                                            arrayData.push(obj)
                                        }
                                    })
                                    data[varName] = arrayData
                                }
                            }
                        }
                    }
                }
            })
        } catch (error) {
            console.warn('Failed to parse script content:', error)
        }

        return data
    }

    // 转换 Vue 模板为静态 HTML 模板
    convertToStaticTemplate(templateContent, componentData) {
        // 使用UI解析器处理模板
        return this.uiParser.convertToStaticTemplate(templateContent, componentData)
    }

    // 生成PHP模板内容
    generatePhpTemplate(componentName, staticTemplateContent) {
        return `<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div data-component="${componentName}">
    ${staticTemplateContent}
</div>
`
    }

    // 处理单个组件文件
    processComponentFile(file) {
        const relativePath = path.relative(this.componentsDir, file);
        const componentName = relativePath.replace('.vue', '').replace(/\\/g, '/').replace(/\//g, '_'); // 使用下划线连接路径

        // 解析 Vue 文件
        const { templateContent, scriptContent } = this.parseVueFile(file)

        // 提取组件数据
        const componentData = this.extractComponentData(scriptContent)

        // 生成静态模板内容
        const staticTemplateContent = this.convertToStaticTemplate(templateContent, componentData)

        // 生成PHP模板
        const componentContent = this.generatePhpTemplate(componentName, staticTemplateContent)

        // 确保输出目录存在
        const outputFileDir = path.dirname(path.join(this.distDir, relativePath));
        if (!fs.existsSync(outputFileDir)) {
            fs.mkdirSync(outputFileDir, { recursive: true });
        }

        // 写入文件
        fs.writeFileSync(path.join(this.distDir, `${relativePath.replace('.vue', '.php')}`), componentContent)
    }

    // 执行转换过程
    convert() {
        this.ensureDistDir()
        const componentFiles = this.getComponentFiles()

        componentFiles.forEach(file => {
            this.processComponentFile(file)
        })

        console.log('✅ TTDF: Static PHP component templates generated successfully!')
    }
}

// 执行转换
const converter = new VueToPhpConverter()
converter.convert()