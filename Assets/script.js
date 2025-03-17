document.addEventListener('DOMContentLoaded', () => {
    /**
     * 注册 Vue 组件
     */
    if (typeof Vue === 'undefined' || typeof ArcoVue === 'undefined') {
        console.error('Vue组件未正确引入');
        return;
    }
    const { createApp } = Vue;
    const app = createApp({});
    app.use(ArcoVue);
    const mountPoint = document.querySelector('#app');
    if (!mountPoint) {
        console.error('未找到挂载点 #app');
        return;
    }
    app.mount(mountPoint);

    /**
     * 复制文章链接
     */
    document.getElementById('CopyPostUrl').addEventListener('click', function () {
        // 获取当前页面的链接
        const currentUrl = window.location.href;

        // 使用 Clipboard API 复制链接
        navigator.clipboard.writeText(currentUrl).then(function () {
            // 使用 Arco Notification 组件显示成功通知
            ArcoVue.Notification.success({
                title: '成功',
                content: '本文链接已复制到剪贴板！',
            });
        }).catch(function (err) {
            // 使用 Arco Notification 组件显示错误通知
            ArcoVue.Notification.error({
                title: '错误',
                content: '无法复制链接: ' + err.message,
            });
        });
    });
});

/**
 * 翻译触发器
 */
// 设置翻译请求监听器
translate.request.listener.delayExecuteTime = 500; // 设置请求完毕后等待500毫秒再翻译
translate.request.listener.minIntervalTime = 800; // 设置两次翻译的最小间隔时间为800毫秒
translate.request.listener.trigger = function (url) {
    // 这里可以自定义哪些URL会触发翻译
    return true; // 默认所有URL都触发翻译
};
// 启动翻译请求监听器
translate.request.listener.start();
const filterITagsContent = () => {
    const iTags = document.querySelectorAll('i');
    iTags.forEach(tag => {
        tag.classList.add('ignore-translate')
    })
};
// 设置忽略翻译的 class
translate.ignore.class = [
    'ignore-translate',
    'mdui-btn',
    'mdui-btn-icon',
    'mdui-list-item-icon',
    'mdui-icon',
    'material-icons'
];

// 查看当前忽略的 class
// console.log(translate.ignore.class);
translate.language.setDefaultTo('chinese_simplified');
translate.selectLanguageTag.show = false;
translate.service.use('client.edge');

translate.language.setUrlParamControl('lang'); // URL参数翻译
// translate.setAutoDiscriminateLocalLanguage(); // 默认翻译为当前国家语言
translate.execute(); // 整页翻译
// translate.selectionTranslate.start(); // 弃用

/**
 * 注册灯箱
 */
window.ViewImage && ViewImage.init('#PostContentTypo img');