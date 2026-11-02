<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from 'vue'

import type { CodeLanguage } from '@/data/site'
import type { HighlighterCore, LanguageRegistration } from 'shiki/core'

const props = defineProps<{
  code: string
  language: CodeLanguage
}>()

const highlightedCode = ref('')
const isLoading = ref(true)
let highlighterPromise: Promise<HighlighterCore> | undefined
let renderVersion = 0

type LanguageModule = {
  default: LanguageRegistration[]
}

const languageLoaders: Record<CodeLanguage, () => Promise<LanguageModule>> = {
  typescript: () => import('shiki/langs/typescript.mjs'),
  cpp: () => import('shiki/langs/cpp.mjs'),
  kotlin: () => import('shiki/langs/kotlin.mjs'),
  json: () => import('shiki/langs/json.mjs'),
}

function getHighlighter() {
  highlighterPromise ??= Promise.all([
    import('shiki/core'),
    import('shiki/engine/javascript'),
    import('shiki/themes/vesper.mjs'),
  ]).then(([core, engine, theme]) =>
    core.createHighlighterCore({
      themes: [theme.default],
      engine: engine.createJavaScriptRegexEngine(),
    }),
  )

  return highlighterPromise
}

async function renderCode() {
  const version = ++renderVersion
  isLoading.value = true

  try {
    const [highlighter, language] = await Promise.all([
      getHighlighter(),
      languageLoaders[props.language](),
    ])

    if (version !== renderVersion) {
      return
    }

    if (!highlighter.getLoadedLanguages().includes(props.language)) {
      await highlighter.loadLanguage(language.default)

      if (version !== renderVersion) {
        return
      }
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
  <div
    class="code-block h-full min-h-0 min-w-0 overflow-hidden [&_.line]:block [&_.line]:min-h-0 [&_.line]:whitespace-pre [&_pre::-webkit-scrollbar-thumb:hover]:bg-[#505050] [&_pre::-webkit-scrollbar-thumb]:rounded-full [&_pre::-webkit-scrollbar-thumb]:border-2 [&_pre::-webkit-scrollbar-thumb]:border-transparent [&_pre::-webkit-scrollbar-thumb]:bg-[#343434] [&_pre::-webkit-scrollbar-thumb]:bg-clip-padding [&_pre::-webkit-scrollbar-track]:bg-transparent [&_pre::-webkit-scrollbar]:h-[7px] [&_pre::-webkit-scrollbar]:w-[7px] [&_pre]:m-0 [&_pre]:h-full [&_pre]:overflow-auto [&_pre]:bg-transparent! [&_pre]:p-5 [&_pre]:font-mono [&_pre]:text-[length:clamp(0.66rem,0.9vw,0.75rem)] [&_pre]:leading-[1.65] [&_pre]:whitespace-pre [&_pre]:text-[#d8d3cb] [&_pre]:[scrollbar-color:#3a3a3a_transparent] [&_pre]:[scrollbar-gutter:stable] [&_pre]:[scrollbar-width:thin] [&_pre]:[tab-size:2] max-sm:[&_pre]:px-[13px] max-sm:[&_pre]:py-4"
    :aria-busy="isLoading"
  >
    <div
      v-if="highlightedCode"
      class="h-full min-h-0"
      v-html="highlightedCode"
    ></div>
    <pre v-else class="min-h-full"><code>{{ code }}</code></pre>
  </div>
</template>
