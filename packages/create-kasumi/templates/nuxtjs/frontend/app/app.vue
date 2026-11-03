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

const statusLabels: Record<ConnectionState, string> = {
  pending: '连接中',
  ready: '已连接',
  error: '未连接'
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
      message: '已连接 Kasumi 运行时，可以开始编辑。',
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
  KSM.init()
  if (!new URLSearchParams(window.location.search).has('smoke')) {
    void connect()
    return
  }
  void KSM.ping().then((pong) => {
    if (!pong.pong) {
      throw new Error('宿主返回了无效的连接检查结果。')
    }
    return KSM.app.exit()
  })
})
</script>

<template>
  <a class="skip-link" href="#main-content">跳到主要内容</a>

  <div class="app-shell">
    <header class="masthead">
      <div class="brand">
        <svg
          class="brand-mark"
          viewBox="0 0 42 42"
          width="30"
          height="30"
          aria-hidden="true"
        >
          <path
            d="m21 2.5 4.7 13.1 13.9.6-11 8.5 3.8 13.4L21 30.2 9.6 38.1l3.8-13.4-11-8.5 13.9-.6Z"
            fill="var(--brand-pink)"
            stroke="currentColor"
            stroke-linejoin="round"
            stroke-width="2.2"
          />
          <circle
            cx="33"
            cy="8.5"
            r="3.4"
            fill="var(--brand-orange)"
            stroke="currentColor"
            stroke-width="1.5"
          />
        </svg>
        <span class="brand-name" translate="no">Kasumi</span>
        <span class="brand-tag">app</span>
      </div>

      <div class="stack">
        <span class="stack-label">Built with</span>
        <span class="stack-logos" aria-hidden="true">
          <span class="tech-mark">
            <svg viewBox="0 0 32 24" width="22" height="17">
              <path d="M2 21 13 2l5.5 9.4L22 5l8 16Z" fill="#00dc82" />
              <path d="m13 2 6 10.3-2.5 4.1L13 9.4Z" fill="#fff" opacity=".32" />
            </svg>
          </span>
          <span class="tech-mark">
            <svg viewBox="0 0 32 28" width="21" height="19">
              <path fill="#41b883" d="M0 0h8l8 13.8L24 0h8L16 28Z" />
              <path fill="#34495e" d="M8 0h5.5L16 5 18.5 0H24L16 13.8Z" />
            </svg>
          </span>
        </span>
        <span class="stack-name">Nuxt + Vue</span>
      </div>
    </header>

    <main id="main-content" class="content">
      <div class="intro">
        <section class="hero" aria-labelledby="app-title">
          <p class="eyebrow">
            <span>Web 界面</span>
            <span aria-hidden="true">+</span>
            <span>原生宿主</span>
          </p>
          <h1 id="app-title">__APP_TITLE__</h1>
          <p class="lede">
            页面已经运行在 Kasumi
            的原生容器中。修改前端文件即可继续开发，需要系统能力时通过 KSM
            桥接调用。
          </p>
          <div class="actions">
            <button
              class="button"
              type="button"
              :disabled="isChecking"
              @click="connect"
            >
              {{ isChecking ? '检查中…' : '重新检查' }}
            </button>
            <span class="command">
              <span aria-hidden="true">$</span> pnpm ksm dev
            </span>
          </div>
        </section>

        <aside class="aside" aria-label="开发提示">
          <p class="aside-label">Start here</p>
          <p class="aside-copy">
            从 <code>frontend/</code> 开始编辑。保存后页面会立即更新，原生能力由
            <code>KSM</code> 统一提供。
          </p>
          <p class="aside-command">
            <span aria-hidden="true">→</span> KSM.app.getInfo()
          </p>
        </aside>
      </div>

      <section class="runtime" aria-labelledby="runtime-title">
        <div class="runtime-heading">
          <h2 id="runtime-title">Runtime</h2>
          <span class="status-badge" :data-state="status.state">
            <span class="status-dot" aria-hidden="true" />
            <span>{{ statusLabels[status.state] }}</span>
          </span>
        </div>

        <dl class="metrics">
          <div>
            <dt>Platform</dt>
            <dd>{{ status.platform }}</dd>
          </div>
          <div>
            <dt>App version</dt>
            <dd>{{ status.version }}</dd>
          </div>
          <div>
            <dt>Capabilities</dt>
            <dd>{{ status.capabilities }}</dd>
          </div>
        </dl>

        <p class="runtime-message" role="status" aria-live="polite">
          {{ status.message }}
        </p>
      </section>
    </main>

    <footer class="footer">
      <span>Win32 / Android</span>
      <span>
        Kasumi <span class="footer-mark" aria-hidden="true">✦</span>
      </span>
    </footer>
  </div>
</template>
