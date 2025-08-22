<script setup>
import Title from 'ant-design-vue/es/typography/Title'
import { ref, onMounted } from 'vue'

const title = ref('')

onMounted(() => {
    fetchTitleData('/ty-json/options/Kasumi_Footer_Info')
})

// 获取标题数据的函数
async function fetchTitleData(url) {
    try {
        const response = await fetch(url)
        const data = await response.json()

        if (data.code === 200 && data.data && data.data.value) {
            title.value = data.data.value
        } else {
            // 如果状态码不是200或数据为空，则请求备用API
            await fetchBackupTitle()
        }
    } catch (error) {
        console.error('获取标题数据失败:', error)
        // 如果请求失败，则请求备用API
        await fetchBackupTitle()
    }
}

// 获取备用标题数据的函数
async function fetchBackupTitle() {
    try {
        const response = await fetch('/ty-json/options/title')
        const data = await response.json()

        if (data.code === 200 && data.data && data.data.value) {
            title.value = data.data.value + ' All right reserved'
        }
    } catch (error) {
        console.error('获取备用标题数据失败:', error)
        title.value = 'Kasumi' // 设置默认标题
    }
}
</script>

<template>
        <aside>
            <p>Copyright © {{ new Date().getFullYear() }} - All right reserved by {{ title }}</p>
        </aside>
</template>