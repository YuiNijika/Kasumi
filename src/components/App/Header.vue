<script setup>
import { ref, onMounted } from 'vue'

const log = `
       　  　▃▆█▇▄▖
　 　 　 ▟◤▖　　　◥█▎
   　 ◢◤　 ▐　　　 　▐▉
　 ▗◤　　　▂　▗▖　　▕█▎
　◤　▗▅▖◥▄　▀◣　　█▊
▐　▕▎◥▖◣◤　　　　◢██
█◣　◥▅█▀　　　　▐██◤
▐█▙▂　　     　◢██◤
◥██◣　　　　◢▄◤
 　　▀██▅▇▀    
`

onMounted(() => {
    console.log(log)
    // 主题切换功能
})

document.addEventListener('DOMContentLoaded', function () {
    const themeToggle = document.getElementById('theme-toggle');
    const html = document.documentElement;

    // 检查本地存储或系统偏好
    const savedTheme = localStorage.getItem('theme') ||
        (window.matchMedia('(prefers-color-scheme: dracula)').matches ? 'dracula' : 'autumn');
    html.setAttribute('data-theme', savedTheme);

    themeToggle.addEventListener('click', function () {
        const currentTheme = html.getAttribute('data-theme');
        const newTheme = currentTheme === 'dracula' ? 'autumn' : 'dracula';
        html.setAttribute('data-theme', newTheme);
        localStorage.setItem('theme', newTheme);
    });
});
</script>

<template>
    <div class="navbar bg-base-100 shadow-sm fixed top-0 left-0 right-0 z-50">
        <div class="navbar-start">
            <div class="dropdown">
                <div tabindex="0" role="button" class="btn btn-ghost lg:hidden">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                        stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                            d="M4 6h16M4 12h8m-8 6h16" />
                    </svg>
                </div>
                <ul tabindex="0" class="menu menu-sm dropdown-content bg-base-100 rounded-box z-1 mt-3 w-52 p-2 shadow">
                    <AppNavbar />
                </ul>
            </div>
            <RouterLink to="/" class="btn btn-ghost text-xl">网站名称</RouterLink>
        </div>
        <div class="navbar-center hidden lg:flex">
            <ul class="menu menu-horizontal px-1">
                <AppNavbar />
            </ul>
        </div>
        <div class="navbar-end">
            <button class="btn btn-ghost btn-circle" onclick="search.showModal()">
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                    stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                        d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
                </svg>
            </button>
            <button class="btn btn-ghost btn-circle" id="theme-toggle">
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                    stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                        d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z" />
                </svg>
            </button>
        </div>
    </div>
    <dialog id="search" class="modal">
        <div class="modal-box">
            <form method="post" role="search">
                <h3 class="font-bold text-lg">搜索内容</h3>
                <div class="py-4">
                    <input type="text" name="s" placeholder="输入关键词..." class="input input-bordered w-full" />
                </div>
                <div class="modal-action">
                    <button type="button" class="btn" onclick="search.close()">关闭</button>
                    <button class="btn btn-primary" type="submit">搜索</button>
                </div>
            </form>
            <button type="button" class="btn btn-sm btn-circle btn-ghost absolute right-2 top-2"
                onclick="search.close()">✕</button>
        </div>
    </dialog>
</template>

<style scoped></style>