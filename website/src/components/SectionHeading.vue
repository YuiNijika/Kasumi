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
    class="mb-[42px] max-w-[700px] max-md:mb-[34px]"
    :class="{ 'mx-auto text-center': centered }"
  >
    <p
      class="mb-3.5 inline-flex items-center gap-2 font-mono text-[0.7rem] font-bold leading-tight tracking-[0.06em] text-accent-bright"
      :class="{ 'justify-center': centered }"
    >
      <span class="h-px w-[18px] bg-current" aria-hidden="true"></span>
      {{ eyebrow }}
    </p>
    <h2
      :id="titleId"
      class="m-0 max-w-[18ch] text-balance font-display text-[clamp(2rem,3.8vw,3.3rem)] font-[650] leading-[1.08] tracking-[-0.04em] text-text max-sm:text-[clamp(1.85rem,9vw,2.55rem)]"
      :class="{ 'mx-auto': centered }"
    >
      {{ title }}
    </h2>
    <p
      class="mt-4 max-w-[60ch] text-pretty text-[clamp(0.96rem,1.3vw,1.04rem)] leading-[1.75] text-muted"
      :class="{ 'mx-auto': centered }"
    >
      {{ intro }}
    </p>
  </header>
</template>
