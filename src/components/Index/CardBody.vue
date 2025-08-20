<script setup>

</script>

<template>
    <div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
        <div class="lg:col-span-3 space-y-6">
            <IndexArticleList />

            <div class="join grid grid-cols-2 max-w-xs mx-auto">
                <button class="join-item btn btn-outline">上一页</button>
                <button class="join-item btn btn-outline">下一页</button>
            </div>
        </div>

        <div class="sidebar-container">
            <div class="sticky-sidebar space-y-6">
                <AppSidebar />
            </div>
        </div>
    </div>
</template>

<style scoped>

</style>