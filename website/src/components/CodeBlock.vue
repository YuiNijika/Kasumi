<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from 'vue'
import { createHighlighter } from 'shiki'

import type { CodeLanguage } from '@/data/site'

const props = defineProps<{
  code: string
  language: CodeLanguage
}>()

const highlightedCode = ref('')
const isLoading = ref(true)
let highlighterPromise: ReturnType<typeof createHighlighter> | undefined
let renderVersion = 0

function getHighlighter() {
  highlighterPromise ??= createHighlighter({
    themes: ['vesper'],
    langs: ['typescript', 'cpp', 'kotlin', 'json'],
  })

  return highlighterPromise
}

async function renderCode() {
  const version = ++renderVersion
  isLoading.value = true

  try {
    const highlighter = await getHighlighter()

    if (version !== renderVersion) {
      return
    }

    // Shiki escapes source text before wrapping it in token spans
    highlightedCode.value = highlighter.codeToHtml(props.code, {
      lang: props.language,
      theme: 'vesper',
    })
  } catch {
    highlightedCode.value = ''
  } finally {
    if (version === renderVersion) {
      isLoading.value = false
    }
  }
}

watch(
  () => [props.code, props.language] as const,
  renderCode,
  { immediate: true },
)

onBeforeUnmount(() => {
  renderVersion += 1
})
</script>

<template>
  <div class="code-block" :aria-busy="isLoading">
    <div
      v-if="highlightedCode"
      class="code-block__highlight"
      v-html="highlightedCode"
    ></div>
    <pre v-else class="code-block__fallback"><code>{{ code }}</code></pre>
  </div>
</template>

<style scoped>
.code-block {
  min-width: 0;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}

.code-block__highlight {
  height: 100%;
  min-height: 0;
}

.code-block :deep(.shiki),
.code-block__fallback {
  height: 100%;
  padding: 20px;
  margin: 0;
  overflow: auto;
  color: #d8d3cb;
  font-family: var(--font-mono);
  font-size: clamp(0.66rem, 0.9vw, 0.75rem);
  line-height: 1.65;
  white-space: normal;
  tab-size: 2;
  background: transparent !important;
  scrollbar-color: #3a3a3a transparent;
  scrollbar-gutter: stable;
  scrollbar-width: thin;
}

.code-block :deep(.line) {
  display: block;
  min-height: 0;
  white-space: pre;
}

.code-block__fallback {
  min-height: 100%;
  white-space: pre;
}

.code-block :deep(.shiki)::-webkit-scrollbar,
.code-block__fallback::-webkit-scrollbar {
  width: 7px;
  height: 7px;
}

.code-block :deep(.shiki)::-webkit-scrollbar-track,
.code-block__fallback::-webkit-scrollbar-track {
  background: transparent;
}

.code-block :deep(.shiki)::-webkit-scrollbar-thumb,
.code-block__fallback::-webkit-scrollbar-thumb {
  background: #343434;
  border: 2px solid transparent;
  border-radius: 999px;
  background-clip: padding-box;
}

.code-block :deep(.shiki)::-webkit-scrollbar-thumb:hover,
.code-block__fallback::-webkit-scrollbar-thumb:hover {
  background: #505050;
  border: 2px solid transparent;
  background-clip: padding-box;
}

@media (max-width: 560px) {
  .code-block :deep(.shiki),
  .code-block__fallback {
    padding: 16px 13px;
  }
}
</style>
