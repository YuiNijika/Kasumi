<script setup>

</script>

<template>
    <div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
        <div class="lg:col-span-3 space-y-6">
            <PostArticleCard />
            <PostComments />
        </div>
        <div class="sidebar-container">
            <PostSidebar />
        </div>
    </div>
</template>