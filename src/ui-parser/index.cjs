const AntdvParser = require('./antdv.cjs');
const TailwindParser = require('./tailwind.cjs');

const uiConfig = {
    framework: 'tailwind',
    compress: false // 全局控制输出是否压缩
};

class UIParserFactory {
    static createParser() {
        switch (uiConfig.framework) {
            case 'antdv':
                return new AntdvParser();
            case 'tailwind':
                return new TailwindParser();
            default:
                throw new Error(`Unsupported UI framework: ${uiConfig.framework}`);
        }
    }
}

module.exports = {
    uiConfig,
    UIParserFactory
};

// 为了保持向后兼容性，也导出默认的工厂方法
module.exports.createParser = UIParserFactory.createParser;