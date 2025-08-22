<script setup>
const pageType = 'post'
</script>

<template>
    <IndexBody v-if="pageType === 'index'" />
    <PostBody v-else-if="pageType === 'post'" />
    <error v-else-if="pageType === 'error'" />
</template>

<style scoped>
</style>