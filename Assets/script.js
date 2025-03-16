document.addEventListener('DOMContentLoaded', () => {
    /**
     * 注册 Vue 组件
     */
    if (typeof Vue === 'undefined' || typeof ArcoVue === 'undefined') {
        console.error('Vue 或 ArcoVue 未正确引入');
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
 * 注册灯箱
 */
window.ViewImage && ViewImage.init('#PostContentTypo img');