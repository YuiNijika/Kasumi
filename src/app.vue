<script setup>

</script>

<template>
    <header>
        <AppHeader />
    </header>
    <main class="min-h-screen bg-base-200 pt-16">
        <div class="mx-auto py-10 px-4">
            <RouterView />
        </div>
    </main>
    <footer class="footer footer-horizontal footer-center text-base-content rounded p-5">
        <AppFooter />
    </footer>
</template>

<style scoped></style>