<script setup>
const pageType = 'error';
</script>

<template>
    <IndexBody v-if="pageType === 'index'" />
    <PostBody v-else-if="pageType === 'post'" />
    <ErrorBody v-else-if="pageType === 'error'" />
</template>

<style scoped>

</style>