<script setup lang="ts">
import { onMounted, reactive, ref } from 'vue'
import { KSM } from 'kasumi-bridge'

type ConnectionState = 'pending' | 'ready' | 'error'

interface RuntimeStatus {
  state: ConnectionState
  message: string
  platform: string
  version: string
  capabilities: string
}

const pendingStatus: RuntimeStatus = {
  state: 'pending',
  message: '正在连接宿主…',
  platform: '检测中',
  version: '检测中',
  capabilities: '检测中'
}

const status = reactive<RuntimeStatus>({ ...pendingStatus })
const isChecking = ref(false)

async function connect() {
  isChecking.value = true
  Object.assign(status, pendingStatus)
  try {
    const [pong, capabilities, appInfo] = await Promise.all([
      KSM.ping(),
      KSM.get(),
      KSM.app.getInfo()
    ])
    if (!pong.pong) {
      throw new Error('宿主返回了无效的连接检查结果。')
    }
    Object.assign(status, {
      state: 'ready',
      message: '已连接宿主，可以开始开发。',
      platform: capabilities.platform,
      version: appInfo.version,
      capabilities: String(capabilities.commands.length)
    })
  } catch (error) {
    Object.assign(status, {
      state: 'error',
      message: error instanceof Error ? error.message : '连接宿主失败。',
      platform: '不可用',
      version: '不可用',
      capabilities: '不可用'
    })
  } finally {
    isChecking.value = false
  }
}

onMounted(() => {
  if (!new URLSearchParams(window.location.search).has('smoke')) {
    void connect()
  }
})
</script>

<template>
  <a class="skip-link" href="#main-content">跳到主要内容</a>
  <main id="main-content" class="shell">
    <header class="hero">
      <p class="eyebrow">Kasumi Application</p>
      <h1>__APP_TITLE__</h1>
      <p class="lede">
        一个使用 Vue 构建的 Kasumi 应用。前端通过类型安全的桥接调用 Windows 与
        Android 原生能力。
      </p>
      <span class="framework">Vue</span>
    </header>

    <section class="status-card" aria-labelledby="status-title">
      <div class="status-heading">
        <div>
          <p class="eyebrow">Runtime</p>
          <h2 id="status-title">连接状态</h2>
        </div>
        <span
          class="status-dot"
          :data-state="status.state"
          aria-hidden="true"
        />
      </div>

      <dl class="facts">
        <div>
          <dt>平台</dt>
          <dd>{{ status.platform }}</dd>
        </div>
        <div>
          <dt>应用版本</dt>
          <dd>{{ status.version }}</dd>
        </div>
        <div>
          <dt>可用能力</dt>
          <dd>{{ status.capabilities }}</dd>
        </div>
      </dl>

      <p class="message" role="status" aria-live="polite">
        {{ status.message }}
      </p>

      <button
        class="button"
        type="button"
        :disabled="isChecking"
        @click="connect"
      >
        {{ isChecking ? '检查中…' : '重新检查' }}
      </button>
    </section>
  </main>
</template>
