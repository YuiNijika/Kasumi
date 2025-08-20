<script setup>

</script>

<template>
    <div class="container">
        <div class="card bg-base-100 shadow-xl">
            <header>
                <AppHeader />
            </header>
            <main class="card-body">
                <RouterView />
            </main>
            <footer>
                <AppFooter />
            </footer>
        </div>
    </div>
</template>

<style scoped>

</style>