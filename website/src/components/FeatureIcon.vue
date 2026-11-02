<script setup lang="ts">
import type { FeatureIconName } from '@/data/site'

defineProps<{
  name: FeatureIconName
}>()
</script>

<template>
  <svg
    aria-hidden="true"
    focusable="false"
    viewBox="0 0 24 24"
    width="22"
    height="22"
    fill="none"
    stroke="currentColor"
    stroke-linecap="round"
    stroke-linejoin="round"
    stroke-width="1.5"
  >
    <template v-if="name === 'window'">
      <rect x="3.5" y="4.5" width="17" height="15" rx="2" />
      <path d="M3.5 8.5h17M7 6.5h.01M9.5 6.5h.01" />
    </template>
    <template v-else-if="name === 'folder'">
      <path d="M3.5 6.5h6l2 2h9v9a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2v-11Z" />
      <path d="M3.5 8.5v-2a2 2 0 0 1 2-2h4l2 2" />
    </template>
    <template v-else-if="name === 'bell'">
      <path d="M6.5 10a5.5 5.5 0 0 1 11 0c0 5 2 5.5 2 6.5H4.5c0-1 2-1.5 2-6.5Z" />
      <path d="M9.5 19a2.7 2.7 0 0 0 5 0" />
    </template>
    <template v-else-if="name === 'layers'">
      <path d="m12 3.5 8 4-8 4-8-4 8-4Z" />
      <path d="m4 12 8 4 8-4M4 16.5l8 4 8-4" />
    </template>
    <template v-else-if="name === 'plugin'">
      <path d="M8 4v4H4v5h4v4h5v-4h4V8h-4V4H8Z" />
      <path d="M13 20.5h5.5V15h-4" />
    </template>
    <template v-else-if="name === 'panels'">
      <rect x="3.5" y="4.5" width="17" height="15" rx="1.5" />
      <path d="M11 4.5v15M11 12h9.5" />
    </template>
    <template v-else-if="name === 'settings'">
      <path d="M4 7h10M18 7h2M4 17h2M10 17h10M4 12h4M12 12h8" />
      <circle cx="16" cy="7" r="2" />
      <circle cx="8" cy="17" r="2" />
      <circle cx="10" cy="12" r="2" />
    </template>
    <template v-else>
      <path d="m12 3.5 7 3.5v5c0 4.4-2.8 7.2-7 8.5-4.2-1.3-7-4.1-7-8.5V7l7-3.5Z" />
      <path d="M9 12h6M12 9v6" />
    </template>
  </svg>
</template>
