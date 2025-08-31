const BaseUIParser = require('./base.cjs')

class MduiUIParser extends BaseUIParser {
    convertToStaticTemplate(templateContent, componentData, compress = false) {
        // 移除 Vue 特定语法
        let result = templateContent
            .replace(/{{[^}]*}}/g, '')
            .replace(/@\w+="[^"]*"/g, '')
            .replace(/:w+="[^"]*"/g, '')
            .replace(/v-\w+="[^"]*"/g, '')

        // 使用基类的压缩方法处理最终输出
        return this.compressContent(result, compress)
    }
}

module.exports = MduiUIParser