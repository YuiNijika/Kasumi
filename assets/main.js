// 检查是否已经在 head 中设置了主题
if (!document.documentElement.classList.contains('theme-initialized')) {
    const html = document.documentElement;
    const savedTheme = localStorage.getItem('theme') ||
        (window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dracula' : 'autumn');
    html.setAttribute('data-theme', savedTheme);
    html.classList.add('theme-initialized');
}

// DOM 加载完成后的其他操作
document.addEventListener('DOMContentLoaded', function () {
    const themeToggle = document.getElementById('theme-toggle');
    const html = document.documentElement;

    themeToggle.addEventListener('click', function () {
        const currentTheme = html.getAttribute('data-theme');
        const newTheme = currentTheme === 'dracula' ? 'autumn' : 'dracula';
        html.setAttribute('data-theme', newTheme);
        localStorage.setItem('theme', newTheme);
    });
});