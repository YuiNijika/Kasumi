<script setup lang="ts">
import { onBeforeUnmount, ref } from 'vue'

const props = defineProps<{
  command: string
}>()

const state = ref<'idle' | 'copied' | 'failed'>('idle')
let resetTimer: ReturnType<typeof setTimeout> | undefined

async function copyText() {
  try {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(props.command)
    } else {
      const input = document.createElement('textarea')
      input.value = props.command
      input.setAttribute('readonly', '')
      input.style.position = 'fixed'
      input.style.opacity = '0'
      document.body.appendChild(input)
      input.select()
      const copied = document.execCommand('copy')
      input.remove()

      if (!copied) {
        throw new Error('Copy command failed.')
      }
    }

    state.value = 'copied'
  } catch {
    state.value = 'failed'
  }

  if (resetTimer) {
    clearTimeout(resetTimer)
  }

  resetTimer = setTimeout(() => {
    state.value = 'idle'
  }, 2200)
}

onBeforeUnmount(() => {
  if (resetTimer) {
    clearTimeout(resetTimer)
  }
})
</script>

<template>
  <div
    class="flex min-h-[54px] w-full max-w-[420px] items-center gap-2.5 rounded-sm border border-line-strong bg-surface px-3.5 py-1 font-mono text-text max-[420px]:gap-2 max-[420px]:px-3"
  >
    <span class="shrink-0 text-[0.84rem] font-extrabold text-accent" aria-hidden="true">$</span>
    <code
      class="min-w-0 flex-1 truncate text-[0.76rem] max-[420px]:text-[0.69rem]"
      translate="no"
    >
      {{ command }}
    </code>
    <button
      type="button"
      class="grid size-11 shrink-0 place-items-center rounded-xs bg-accent text-[#190c03] transition-[background-color,transform] duration-150 hover:bg-accent-bright active:scale-[0.97]"
      :aria-label="
        state === 'copied'
          ? '创建命令已复制'
          : state === 'failed'
            ? '复制失败，重试复制创建命令'
            : '复制创建命令'
      "
      :title="state === 'copied' ? '已复制' : state === 'failed' ? '复制失败，重试' : '复制'"
      @click="copyText"
    >
      <svg
        v-if="state === 'idle'"
        aria-hidden="true"
        viewBox="0 0 20 20"
        width="15"
        height="15"
      >
        <rect x="6.5" y="5.5" width="8" height="9" rx="1.4" fill="none" stroke="currentColor" />
        <path d="M4.5 12.5h-1v-8h8v1" fill="none" stroke="currentColor" stroke-linecap="round" />
      </svg>
      <svg
        v-else-if="state === 'copied'"
        aria-hidden="true"
        viewBox="0 0 20 20"
        width="16"
        height="16"
      >
        <path
          d="m4.5 10.5 3.4 3.3 7.6-8"
          fill="none"
          stroke="currentColor"
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-width="1.8"
        />
      </svg>
      <svg
        v-else
        aria-hidden="true"
        viewBox="0 0 20 20"
        width="16"
        height="16"
      >
        <path
          d="M15.2 7.2A5.8 5.8 0 1 0 16 11M15.2 3.8v3.4h-3.4"
          fill="none"
          stroke="currentColor"
          stroke-linecap="round"
          stroke-linejoin="round"
          stroke-width="1.6"
        />
      </svg>
    </button>
    <span class="sr-only" aria-live="polite">
      {{
        state === 'copied'
          ? '创建命令已复制到剪贴板'
          : state === 'failed'
            ? '复制失败，请手动选择命令'
            : ''
      }}
    </span>
  </div>
</template>
