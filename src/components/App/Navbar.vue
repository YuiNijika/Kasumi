<script setup>
import { ref, onMounted } from 'vue'

const title = ref('')
const subTitle = ref('')
const url = ref('')
const KasumiData = ref({})

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
    if (typeof window !== 'undefined' && window.Kasumi && window.Kasumi.site?.title) {
        title.value = window.Kasumi.site.title
    } else {
        fetchTitleData('/ty-json/options/title')
    }
    if (typeof window !== 'undefined' && window.Kasumi && window.Kasumi.site?.subTitle) {
        subTitle.value = window.Kasumi.site.subTitle
    }
    if (typeof window !== 'undefined' && window.Kasumi && window.Kasumi.site?.url) {
        url.value = window.Kasumi.site.url
    } else {
        url.value = window.location.href
    }
    if (typeof window !== 'undefined' && window.Kasumi) {
        KasumiData.value = window.Kasumi
    }
})

// 获取标题数据的函数
async function fetchTitleData(url) {
    try {
        const response = await fetch(url)
        const data = await response.json()

        if (data.code === 200 && data.data && data.data.value) {
            title.value = data.data.value
        } else {
            // 如果状态码不是200或数据为空
            if (typeof window !== 'undefined' && window.Qmsg) {
                window.Qmsg.error('标题获取失败')
            }
            title.value = '标题获取失败'
        }
    } catch (error) {
        if (typeof window !== 'undefined' && window.Qmsg) {
            window.Qmsg.error('标题获取失败')
        }
        console.error('获取标题数据失败:', error)
        title.value = '标题获取失败'
    }
}
</script>

<template>
    <div class="drawer">
        <input id="my-drawer-3" type="checkbox" class="drawer-toggle" />
        <div class="navbar bg-base-100 shadow-sm fixed top-0 left-0 right-0 z-50">
            <div class="flex-none">
                <label for="my-drawer-3" aria-label="open sidebar" class="btn btn-square btn-ghost">
                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24"
                        class="inline-block h-6 w-6 stroke-current">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                            d="M4 6h16M4 12h16M4 18h16"></path>
                    </svg>
                </label>
            </div>
            <div class="mx-2 flex-1 px-2">{{ title }}</div>
            <div class="navbar-end">
                <div class="hidden flex-none lg:block">
                    <ul class="menu menu-horizontal">
                        <li><a>Navbar Item 1</a></li>
                        <li><a>Navbar Item 2</a></li>
                    </ul>
                </div>
                <button class="btn btn-ghost btn-circle" id="theme-toggle">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                        stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                            d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z" />
                    </svg>
                </button>
            </div>
        </div>

        <div class="drawer-side">
            <label for="my-drawer-3" aria-label="close sidebar" class="drawer-overlay"></label>
            <ul class="menu bg-base-200 min-h-full w-80 mt-16 p-4">
                <li><a>Sidebar Item 1</a></li>
                <li><a>Sidebar Item 2</a></li>
            </ul>
        </div>
    </div>
</template>