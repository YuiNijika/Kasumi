/* 
    Typecho Theme Development Framework
    TTDF · 一个面向小白简单易懂的Typecho主题模板开发框架
    @Author 鼠子Tomoriゞ
    @Link https://github.com/YuiNijika/Typecho-Theme-Development-Framework
*/

class KasumiMain {
    constructor() {
        // 主题名称映射
        this.themeNames = {
            'dracula': '黑夜模式',
            'autumn': '白天模式'
        };
        
        // 生成唯一通知ID
        this.themeNotificationId = `theme-switch-notification-${Date.now()}`;
        
        // 初始化主题系统
        this.initThemeSystem();
    }

    /**
     * 初始化主题系统
     */
    initThemeSystem() {
        // 等待DOM和motyf加载完成
        this.waitForDependencies().then(() => {
            this.initThemeToggle();
            this.applySavedTheme();
        }).catch(error => {
            console.error('初始化失败:', error);
        });
    }

    /**
     * 等待必要依赖加载
     */
    waitForDependencies() {
        return new Promise((resolve, reject) => {
            const maxAttempts = 10;
            let attempts = 0;

            const checkDependencies = () => {
                attempts++;
                
                // 检查DOM和motyf是否可用
                if (document.readyState === 'complete' && typeof motyf !== 'undefined') {
                    resolve();
                } else if (attempts >= maxAttempts) {
                    reject(new Error('依赖加载超时'));
                } else {
                    setTimeout(checkDependencies, 100);
                }
            };

            checkDependencies();
        });
    }

    /**
     * 初始化主题切换按钮
     */
    initThemeToggle() {
        const themeToggle = document.getElementById('theme-toggle');
        
        if (themeToggle) {
            // 添加点击事件
            themeToggle.addEventListener('click', () => {
                this.toggleTheme();
            });
            
            // 添加键盘可访问性
            themeToggle.addEventListener('keydown', (e) => {
                if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    this.toggleTheme();
                }
            });
        } else {
            console.warn('未找到主题切换按钮');
        }
    }

    /**
     * 应用已保存的主题
     */
    applySavedTheme() {
        const html = document.documentElement;
        const savedTheme = this.getPreferredTheme();
        
        html.setAttribute('data-theme', savedTheme);
        this.updateThemeMeta(savedTheme);
    }

    /**
     * 获取首选主题
     */
    getPreferredTheme() {
        // 从本地存储获取
        const savedTheme = localStorage.getItem('theme');
        if (savedTheme) return savedTheme;
        
        // 检测系统偏好
        const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
        return prefersDark ? 'dracula' : 'autumn';
    }

    /**
     * 切换主题
     */
    toggleTheme() {
        const html = document.documentElement;
        const currentTheme = html.getAttribute('data-theme');
        const newTheme = currentTheme === 'dracula' ? 'autumn' : 'dracula';
        
        // 应用新主题
        html.setAttribute('data-theme', newTheme);
        localStorage.setItem('theme', newTheme);
        this.updateThemeMeta(newTheme);
        
        // 显示通知
        this.showThemeNotification(newTheme);
    }

    /**
     * 更新主题meta标签
     */
    updateThemeMeta(theme) {
        // 更新主题色meta（可选）
        const themeColor = theme === 'dracula' ? '#282a36' : '#f8f8f2';
        let meta = document.querySelector('meta[name="theme-color"]');
        
        if (!meta) {
            meta = document.createElement('meta');
            meta.name = 'theme-color';
            document.head.appendChild(meta);
        }
        
        meta.content = themeColor;
    }

    /**
     * 显示主题切换通知
     */
    showThemeNotification(theme) {
        try {
            motyf({
                content: `主题已切换为${this.themeNames[theme]}`,
                type: 'success',
                time: 2000,
                id: this.themeNotificationId,
                position: 'bottom-right',
                autoClose: true
            });
        } catch (e) {
            console.error('通知显示失败:', e);
            // 回退方案
            alert(`主题已切换为${this.themeNames[theme]}`);
        }
    }
}

// 安全初始化
(function() {
    // 确保不会重复初始化
    if (!window.KasumiMain) {
        // 双重检查确保DOM已加载
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => {
                window.KasumiMain = new KasumiMain();
            });
        } else {
            window.KasumiMain = new KasumiMain();
        }
    }
})();