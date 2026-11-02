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
    class="app-window"
    aria-label="Kasumi 应用的方法示例、文件树与高亮代码"
  >
    <header class="app-window__bar">
      <span class="window-dots" aria-hidden="true">
        <i></i>
        <i></i>
        <i></i>
      </span>

      <div class="example-tabs" role="tablist" aria-label="方法示例">
        <button
          v-for="(example, index) in examples"
          :id="`example-tab-${example.id}`"
          :key="example.id"
          type="button"
          role="tab"
          class="example-tab"
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
      class="example-panel"
      role="tabpanel"
      :aria-labelledby="`example-tab-${example.id}`"
      tabindex="0"
    >
      <div
        class="file-tree"
        role="group"
        :aria-label="`${example.label}示例文件`"
      >
        <p class="file-tree__root">
          <span aria-hidden="true"></span>
          kirakira-app
        </p>

        <div
          v-for="group in fileTreeGroups(example)"
          :key="group.folder || 'root'"
          class="file-tree__group"
        >
          <p v-if="group.folder" class="file-tree__folder">
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
            class="file-tree__file"
            :class="{ 'file-tree__file--active': selectedFileIndex(example) === entry.index }"
            :aria-pressed="selectedFileIndex(example) === entry.index"
            @click="selectFile(example.id, entry.index)"
          >
            <CodeLanguageIcon :language="entry.file.language" />
            <span translate="no">{{ entry.name }}</span>
          </button>
        </div>
      </div>

      <div class="code-pane">
        <header v-if="selectedFile(example)" class="code-pane__bar">
          <span translate="no">{{ selectedFile(example)?.path }}</span>
          <span>{{ example.method }}</span>
        </header>

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

    <footer class="app-window__status">
      <span>
        <i aria-hidden="true"></i>
        {{ activeExample?.method }}
      </span>
      <span>Shiki · UTF-8</span>
    </footer>
  </figure>
</template>

<style scoped>
.app-window {
  display: flex;
  height: 500px;
  max-height: calc(100dvh - 96px);
  margin: 0;
  flex-direction: column;
  overflow: hidden;
  color: var(--code-text);
  background: var(--code-bg);
  border: 1px solid var(--code-border);
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-soft);
}

.app-window__bar {
  display: grid;
  flex: none;
  grid-template-columns: auto minmax(0, 1fr);
  gap: 12px;
  align-items: center;
  min-height: 49px;
  padding: 0 14px;
  background: var(--code-surface);
  border-bottom: 1px solid var(--code-border);
}

.window-dots {
  display: inline-flex;
  gap: 6px;
}

.window-dots i {
  width: 7px;
  height: 7px;
  background: #3b3b3b;
  border-radius: 50%;
}

.window-dots i:first-child {
  background: var(--accent);
}

.example-tabs {
  display: flex;
  min-width: 0;
  align-self: stretch;
  overflow-x: auto;
  scrollbar-width: none;
}

.example-tabs::-webkit-scrollbar {
  display: none;
}

.example-tab {
  display: inline-flex;
  flex: none;
  gap: 7px;
  align-items: center;
  padding: 0 11px;
  color: #77736c;
  font-size: 0.65rem;
  font-weight: 650;
  white-space: nowrap;
  border-bottom: 1px solid transparent;
  transition:
    color 150ms ease,
    background-color 150ms ease,
    border-color 150ms ease;
}

.example-tab :deep(svg) {
  width: 14px;
  height: 14px;
}

.example-tab:hover {
  color: #c4c0b8;
  background: #151515;
}

.example-tab[aria-selected="true"] {
  color: #efede8;
  background: var(--code-bg);
  border-bottom-color: var(--accent);
}

.example-panel {
  display: grid;
  flex: 1 1 auto;
  grid-template-columns: 156px minmax(0, 1fr);
  min-height: 0;
  overflow: hidden;
}

.file-tree {
  min-height: 0;
  padding: 10px 8px;
  overflow-y: auto;
  background: #0a0a0a;
  border-right: 1px solid var(--code-border);
  scrollbar-color: #3a3a3a transparent;
  scrollbar-width: thin;
}

.file-tree::-webkit-scrollbar {
  width: 7px;
}

.file-tree::-webkit-scrollbar-track {
  background: transparent;
}

.file-tree::-webkit-scrollbar-thumb {
  background: #343434;
  border: 2px solid transparent;
  border-radius: 999px;
  background-clip: padding-box;
}

.file-tree::-webkit-scrollbar-thumb:hover {
  background: #505050;
  border: 2px solid transparent;
  background-clip: padding-box;
}

.file-tree__root {
  display: flex;
  gap: 7px;
  align-items: center;
  padding: 5px 7px 10px;
  margin: 0;
  color: #a7a39c;
  font-family: var(--font-mono);
  font-size: 0.59rem;
}

.file-tree__root span {
  width: 6px;
  height: 6px;
  background: var(--accent);
  border-radius: 50%;
}

.file-tree__group + .file-tree__group {
  margin-top: 8px;
}

.file-tree__folder {
  display: flex;
  gap: 6px;
  align-items: center;
  padding: 5px 7px;
  margin: 0 0 2px;
  color: #77736d;
  font-family: var(--font-mono);
  font-size: 0.56rem;
}

.file-tree__file {
  display: flex;
  gap: 7px;
  align-items: center;
  width: 100%;
  min-width: 0;
  padding: 6px 7px 6px 18px;
  color: #77736d;
  font-family: var(--font-mono);
  font-size: 0.56rem;
  text-align: left;
  border-radius: 3px;
  transition:
    color 150ms ease,
    background-color 150ms ease;
}

.file-tree__file:hover {
  color: #d4d0c8;
  background: #151515;
}

.file-tree__file--active {
  color: #f0ede7;
  background: #1b1b1b;
}

.file-tree__file span {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.code-pane {
  display: flex;
  min-width: 0;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
}

.code-pane :deep(.code-block) {
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

.code-pane__bar {
  display: flex;
  gap: 16px;
  align-items: center;
  justify-content: space-between;
  min-height: 38px;
  padding: 0 14px;
  color: #6f6b64;
  font-family: var(--font-mono);
  font-size: 0.56rem;
  background: #101010;
  border-bottom: 1px solid var(--code-border);
}

.code-pane__bar span:first-child {
  min-width: 0;
  overflow: hidden;
  color: #b6b2ab;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.app-window__status {
  display: flex;
  flex: none;
  align-items: center;
  justify-content: space-between;
  min-height: 34px;
  padding: 0 14px;
  color: #65615b;
  font-family: var(--font-mono);
  font-size: 0.58rem;
  background: var(--code-surface);
  border-top: 1px solid var(--code-border);
}

.app-window__status span:first-child {
  display: inline-flex;
  gap: 7px;
  align-items: center;
}

.app-window__status i {
  width: 5px;
  height: 5px;
  background: var(--accent);
  border-radius: 50%;
}

@media (max-width: 560px) {
  .app-window {
    height: 470px;
    max-height: calc(100dvh - 72px);
  }

  .app-window__bar {
    grid-template-columns: minmax(0, 1fr);
    padding-left: 8px;
  }

  .window-dots {
    display: none;
  }

  .example-panel {
    grid-template-columns: 1fr;
    grid-template-rows: 142px minmax(0, 1fr);
  }

  .file-tree {
    height: 142px;
    border-right: 0;
    border-bottom: 1px solid var(--code-border);
  }

  .code-pane {
    min-height: 0;
  }
}
</style>
