class BaseUIParser {
    constructor() {
        this.componentMap = {}
        this.booleanProps = []
    }

    /**
     * 压缩HTML内容
     * @param {string} content - 需要压缩的HTML内容
     * @param {boolean} compress - 是否启用压缩
     * @returns {string} 压缩后的内容
     */
    compressContent(content, compress = false) {
        if (!compress) return content
        
        return content
            // 移除多余的空白字符
            .replace(/\s{2,}/g, ' ')
            // 移除标签间的换行符和空格
            .replace(/>\s+</g, '><')
            // 移除属性间多余空格
            .replace(/\s*=\s*/g, '=')
            // 移除行首行尾空白
            .trim()
    }

    /**
     * 获取组件对应的 HTML 元素
     * @param {string} componentName - 组件名称
     * @returns {string} HTML 元素标签名
     */
    getHtmlElementForComponent(componentName) {
        return this.componentMap[componentName] || 'div'
    }

    /**
     * 转换框架特定属性为 HTML 属性和类名
     * @param {string} attributes - 组件属性字符串
     * @param {string} componentName - 组件名称
     * @returns {string} 转换后的HTML属性字符串
     */
    convertAttributes(attributes, componentName) {
        throw new Error('convertAttributes method must be implemented by subclass')
    }

    /**
     * 转换组件开始标签
     * @param {string} componentName - 组件名称
     * @param {string} attributes - 组件属性
     * @returns {string} 转换后的HTML开始标签
     */
    convertStartTag(componentName, attributes) {
        const htmlElement = this.getHtmlElementForComponent(componentName)
        const htmlAttributes = this.convertAttributes(attributes, componentName)
        const tag = `<${htmlElement}${htmlAttributes ? ' ' + htmlAttributes : ''}>`
        return tag.trim() + '>'
    }

    /**
     * 转换组件结束标签
     * @param {string} componentName - 组件名称
     * @returns {string} 转换后的HTML结束标签
     */
    convertEndTag(componentName) {
        const htmlElement = this.getHtmlElementForComponent(componentName)
        return `</${htmlElement}>`
    }

    /**
     * 处理 v-for 循环的特殊组件
     * @param {string} templateContent - 模板内容
     * @param {object} componentData - 组件数据
     * @returns {string} 处理后的模板内容
     */
    expandVForLoops(templateContent, componentData) {
        // 默认实现，子类可以覆盖
        return templateContent
    }

    /**
     * 转换模板为静态HTML模板的基础方法
     * @param {string} templateContent - 模板内容
     * @param {object} componentData - 组件数据
     * @param {boolean} compress - 是否压缩输出
     * @returns {string} 转换后的静态模板
     */
    convertToStaticTemplate(templateContent, componentData, compress = false) {
        // 基础实现，子类应该覆盖此方法
        let result = templateContent
        return this.compressContent(result, compress)
    }
}

module.exports = BaseUIParser