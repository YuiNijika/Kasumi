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
  <div class="copy-command">
    <span class="copy-command__prompt" aria-hidden="true">$</span>
    <code translate="no">{{ command }}</code>
    <button
      type="button"
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
    <span class="visually-hidden" aria-live="polite">
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

<style scoped>
.copy-command {
  display: flex;
  gap: 11px;
  align-items: center;
  width: min(100%, 420px);
  min-height: 54px;
  padding: 5px 5px 5px 15px;
  color: var(--text);
  font-family: var(--font-mono);
  background: var(--surface);
  border: 1px solid var(--border-strong);
  border-radius: var(--radius-sm);
}

.copy-command__prompt {
  color: var(--accent);
  font-size: 0.84rem;
  font-weight: 800;
}

.copy-command code {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  font-size: 0.8rem;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.copy-command button {
  display: grid;
  flex: none;
  align-items: center;
  justify-content: center;
  width: 42px;
  min-height: 42px;
  padding: 0;
  color: #190c03;
  background: var(--accent);
  border-radius: var(--radius-xs);
  place-items: center;
  transition:
    background-color 150ms ease,
    transform 120ms ease;
}

.copy-command button:hover {
  background: var(--accent-bright);
}

.copy-command button:active {
  transform: scale(0.97);
}

@media (max-width: 560px) {
  .copy-command {
    width: 100%;
    min-width: 0;
  }

  .copy-command code {
    font-size: 0.75rem;
  }
}

@media (max-width: 420px) {
  .copy-command {
    gap: 8px;
    padding-left: 12px;
  }

  .copy-command code {
    font-size: 0.69rem;
  }

  .copy-command button {
    width: 40px;
  }
}
</style>
