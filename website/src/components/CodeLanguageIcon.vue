<script setup lang="ts">
import type { CodeLanguage } from '@/data/site'

defineProps<{
  language: CodeLanguage
}>()

const iconClasses: Record<CodeLanguage, string> = {
  typescript: 'bg-[#3178c6]',
  cpp: 'bg-[#00599c]',
  kotlin: 'bg-[linear-gradient(135deg,#7f52ff_0_58%,#e44857_58%_78%,#ff8c00_78%)]',
  json: 'bg-[#5e5b55]',
}

const iconLabels: Record<CodeLanguage, string> = {
  typescript: 'TS',
  cpp: 'C++',
  kotlin: 'K',
  json: '{}',
}
</script>

<template>
  <span
    class="grid size-[18px] shrink-0 place-items-center rounded-[4px] font-mono text-[0.43rem] font-extrabold leading-none tracking-[-0.03em] text-white"
    :class="iconClasses[language]"
    aria-hidden="true"
    translate="no"
  >
    {{ iconLabels[language] }}
  </span>
</template>
