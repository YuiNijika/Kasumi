<script setup>
import { ref, onMounted } from 'vue'

// 导航数据
const navItems = ref([])

onMounted(() => {
    // 获取导航数据
    fetch('/ty-json/options/Kasumi_Header_Navbar')
        .then(response => response.json())
        .then(data => {
            if (data.code === 200 && data.data.value) {
                // 解析导航数据
                const navData = parseNavData(data.data.value)
                if (navData.length > 0) {
                    navItems.value = navData
                }
            }
        })
        .catch(error => {
            console.error('获取导航数据失败:', error)
        })
})

// 解析导航数据 - 修改为支持逗号或换行空格分割
function parseNavData(navString) {
    if (!navString) return []

    // 先按换行符分割，再按逗号分割
    let items = []
    
    // 按换行符分割
    const lines = navString.split('\n')
    
    lines.forEach(line => {
        // 如果一行中包含逗号，则按逗号分割
        if (line.includes(',')) {
            const commaItems = line.split(',').map(item => item.trim()).filter(item => item)
            items = items.concat(commaItems)
        } else {
            // 否则直接添加整行（去除首尾空格）
            const trimmedLine = line.trim()
            if (trimmedLine) {
                items.push(trimmedLine)
            }
        }
    })
    
    // 解析每个项目为 {text, url} 格式
    return items.map(item => {
        const [text, url] = item.split('|')
        return { text: text?.trim() || '', url: url?.trim() || '#' }
    }).filter(item => item.text)
}

// 判断是否为外部链接
function isExternalLink(url) {
    try {
        // 如果是相对路径或锚点链接，则不是外部链接
        if (url.startsWith('/') || url.startsWith('#') || url.startsWith('?')) {
            return false
        }

        // 创建 URL 对象进行比较
        const currentDomain = window.location.origin
        const urlObj = new URL(url)

        // 比较域名
        return urlObj.origin !== currentDomain
    } catch (e) {
        // 如果 URL 格式不正确，假设为外部链接
        return true
    }
}

// 获取链接属性
function getLinkAttributes(url) {
    const isExternal = isExternalLink(url)
    return {
        href: url,
        ...(isExternal && {
            target: '_blank',
            rel: 'noopener noreferrer'
        })
    }
}

</script>

<template>
    <li v-for="(item, index) in navItems" :key="index">
        <a v-bind="getLinkAttributes(item.url)">
            {{ item.text }}
        </a>
    </li>
</template>