<script setup lang="ts">
defineProps<{
  eyebrow: string
  title: string
  intro: string
  titleId: string
  centered?: boolean
}>()
</script>

<template>
  <header
    class="section-heading"
    :class="{ 'section-heading--center': centered }"
  >
    <p class="eyebrow">{{ eyebrow }}</p>
    <h2 :id="titleId" class="section-title">{{ title }}</h2>
    <p class="section-intro">{{ intro }}</p>
  </header>
</template>
