<script setup lang="ts">
import { computed, ref } from 'vue'

import CodeBlock from '@/components/CodeBlock.vue'
import CodeLanguageIcon from '@/components/CodeLanguageIcon.vue'
import FeatureIcon from '@/components/FeatureIcon.vue'
import type {
  CodeExample,
  CodeExampleFile,
} from '@/data/site'

type FileTreeGroup = {
  folder: string
  files: Array<{
    file: CodeExampleFile
    index: number
    name: string
  }>
}

const props = defineProps<{
  examples: CodeExample[]
}>()

const activeExampleIndex = ref(0)
const selectedFileIndexes = ref<Record<string, number>>(
  Object.fromEntries(props.examples.map((example) => [example.id, 0])),
)

const activeExample = computed(() => props.examples[activeExampleIndex.value])

function selectedFileIndex(example: CodeExample) {
  return selectedFileIndexes.value[example.id] ?? 0
}

function selectedFile(example: CodeExample) {
  return example.files[selectedFileIndex(example)]
}

function selectExample(index: number) {
  if (index < 0 || index >= props.examples.length) {
    return
  }

  activeExampleIndex.value = index
}

function selectFile(exampleId: string, index: number) {
  selectedFileIndexes.value = {
    ...selectedFileIndexes.value,
    [exampleId]: index,
  }
}

function handleTabKeydown(event: KeyboardEvent, index: number) {
  const lastIndex = props.examples.length - 1
  let nextIndex = index

  if (event.key === 'ArrowRight') {
    nextIndex = index === lastIndex ? 0 : index + 1
  } else if (event.key === 'ArrowLeft') {
    nextIndex = index === 0 ? lastIndex : index - 1
  } else if (event.key === 'Home') {
    nextIndex = 0
  } else if (event.key === 'End') {
    nextIndex = lastIndex
  } else {
    return
  }

  event.preventDefault()
  selectExample(nextIndex)

  const tablist = (event.currentTarget as HTMLElement).closest(
    '[role="tablist"]',
  )
  const tabs = tablist?.querySelectorAll<HTMLButtonElement>('[role="tab"]')
  requestAnimationFrame(() => tabs?.[nextIndex]?.focus())
}

function fileTreeGroups(example: CodeExample): FileTreeGroup[] {
  const groups = new Map<string, FileTreeGroup>()

  example.files.forEach((file, index) => {
    const segments = file.path.split('/')
    const folder = segments.length > 1 ? segments[0] ?? '' : ''
    const name = segments[segments.length - 1] ?? file.path
    const group = groups.get(folder) ?? { folder, files: [] }

    group.files.push({ file, index, name })
    groups.set(folder, group)
  })

  return Array.from(groups.values())
}
</script>

<template>
  <figure
    class="m-0 flex h-[500px] max-h-[calc(100dvh-96px)] flex-col overflow-hidden rounded-md border border-code-border bg-code-bg text-code-text shadow-soft max-sm:h-[470px] max-sm:max-h-[calc(100dvh-72px)]"
    aria-label="Kasumi 应用的方法示例、文件树与高亮代码"
  >
    <header
      class="grid min-h-[49px] flex-none grid-cols-[auto_minmax(0,1fr)] items-center gap-3 border-b border-code-border bg-code-surface px-3.5 max-sm:grid-cols-1 max-sm:pl-2"
    >
      <span class="inline-flex gap-1.5 max-sm:hidden" aria-hidden="true">
        <i class="size-[7px] rounded-full bg-accent"></i>
        <i class="size-[7px] rounded-full bg-[#3b3b3b]"></i>
        <i class="size-[7px] rounded-full bg-[#3b3b3b]"></i>
      </span>

      <div
        class="flex min-w-0 self-stretch overflow-x-auto [scrollbar-width:none] [&::-webkit-scrollbar]:hidden"
        role="tablist"
        aria-label="方法示例"
      >
        <button
          v-for="(example, index) in examples"
          :id="`example-tab-${example.id}`"
          :key="example.id"
          type="button"
          role="tab"
          class="inline-flex flex-none items-center gap-[7px] border-b border-transparent px-[11px] text-[0.65rem] font-[650] whitespace-nowrap text-[#77736c] transition-[color,background-color,border-color] hover:bg-[#151515] hover:text-[#c4c0b8] aria-selected:border-accent aria-selected:bg-code-bg aria-selected:text-[#efede8] [&_svg]:size-3.5"
          :aria-controls="`example-panel-${example.id}`"
          :aria-selected="activeExampleIndex === index"
          :tabindex="activeExampleIndex === index ? 0 : -1"
          @click="selectExample(index)"
          @keydown="handleTabKeydown($event, index)"
        >
          <FeatureIcon :name="example.icon" />
          <span>{{ example.label }}</span>
        </button>
      </div>
    </header>

    <div
      v-for="(example, exampleIndex) in examples"
      v-show="activeExampleIndex === exampleIndex"
      :id="`example-panel-${example.id}`"
      :key="example.id"
      class="grid min-h-0 flex-1 auto-rows-fr grid-cols-[156px_minmax(0,1fr)] overflow-hidden max-sm:grid-rows-[142px_minmax(0,1fr)] max-sm:grid-cols-1"
      role="tabpanel"
      :aria-labelledby="`example-tab-${example.id}`"
      tabindex="0"
    >
      <div
        class="min-h-0 overflow-y-auto border-r border-code-border bg-[#0a0a0a] p-2.5 [scrollbar-color:#3a3a3a_transparent] [scrollbar-width:thin] [&::-webkit-scrollbar-thumb:hover]:bg-[#505050] [&::-webkit-scrollbar-thumb]:rounded-full [&::-webkit-scrollbar-thumb]:border-2 [&::-webkit-scrollbar-thumb]:border-transparent [&::-webkit-scrollbar-thumb]:bg-[#343434] [&::-webkit-scrollbar-thumb]:bg-clip-padding [&::-webkit-scrollbar-track]:bg-transparent [&::-webkit-scrollbar]:w-[7px] max-sm:h-[142px] max-sm:border-r-0 max-sm:border-b max-sm:border-code-border"
        role="group"
        :aria-label="`${example.label}示例文件`"
      >
        <p
          class="m-0 flex items-center gap-[7px] px-[7px] pt-[5px] pb-2.5 font-mono text-[0.59rem] text-[#a7a39c]"
        >
          <span class="size-1.5 rounded-full bg-accent" aria-hidden="true"></span>
          kirakira-app
        </p>

        <div
          v-for="group in fileTreeGroups(example)"
          :key="group.folder || 'root'"
          class="[&+&]:mt-2"
        >
          <p
            v-if="group.folder"
            class="m-0 mb-0.5 flex items-center gap-1.5 px-[7px] py-[5px] font-mono text-[0.56rem] text-[#77736d]"
          >
            <svg
              aria-hidden="true"
              viewBox="0 0 20 20"
              width="14"
              height="14"
            >
              <path
                d="M2.8 5.5h5l1.7 1.8h7.7v7.2a1.7 1.7 0 0 1-1.7 1.7H4.5a1.7 1.7 0 0 1-1.7-1.7V5.5Z"
                fill="none"
                stroke="currentColor"
                stroke-linejoin="round"
                stroke-width="1.3"
              />
            </svg>
            {{ group.folder }}
          </p>

          <button
            v-for="entry in group.files"
            :key="entry.file.path"
            type="button"
            class="flex w-full min-w-0 items-center gap-[7px] rounded-[3px] py-1.5 pr-[7px] pl-[18px] text-left font-mono text-[0.56rem] text-[#77736d] transition-colors hover:bg-[#151515] hover:text-[#d4d0c8]"
            :class="{
              'bg-[#1b1b1b] text-[#f0ede7]':
                selectedFileIndex(example) === entry.index,
            }"
            :aria-pressed="selectedFileIndex(example) === entry.index"
            @click="selectFile(example.id, entry.index)"
          >
            <CodeLanguageIcon :language="entry.file.language" />
            <span class="min-w-0 overflow-hidden text-ellipsis whitespace-nowrap" translate="no">
              {{ entry.name }}
            </span>
          </button>
        </div>
      </div>

      <div class="flex min-h-0 min-w-0 flex-col overflow-hidden">
        <header
          v-if="selectedFile(example)"
          class="flex min-h-[38px] items-center justify-between gap-4 border-b border-code-border bg-[#101010] px-3.5 font-mono text-[0.56rem] text-[#6f6b64]"
        >
          <span
            class="min-w-0 overflow-hidden text-ellipsis whitespace-nowrap text-[#b6b2ab]"
            translate="no"
          >
            {{ selectedFile(example)?.path }}
          </span>
          <span>{{ example.method }}</span>
        </header>

        <div class="min-h-0 flex-1 overflow-hidden">
          <CodeBlock
            v-if="
              activeExampleIndex === exampleIndex &&
              selectedFile(example)
            "
            :code="selectedFile(example)?.code ?? ''"
            :language="selectedFile(example)?.language ?? 'typescript'"
          />
        </div>
      </div>
    </div>

    <footer
      class="flex min-h-[34px] flex-none items-center justify-between border-t border-code-border bg-code-surface px-3.5 font-mono text-[0.58rem] text-[#65615b]"
    >
      <span class="inline-flex items-center gap-[7px]">
        <i class="size-[5px] rounded-full bg-accent" aria-hidden="true"></i>
        {{ activeExample?.method }}
      </span>
      <span>Shiki · UTF-8</span>
    </footer>
  </figure>
</template>
