import { useEffect, useState } from 'react'
import { KSM } from 'kasumi-bridge'

import './App.css'

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

export default function App() {
  const [status, setStatus] = useState(pendingStatus)
  const [isChecking, setIsChecking] = useState(false)

  async function connect() {
    setIsChecking(true)
    setStatus(pendingStatus)
    try {
      const [pong, capabilities, appInfo] = await Promise.all([
        KSM.ping(),
        KSM.get(),
        KSM.app.getInfo()
      ])
      if (!pong.pong) {
        throw new Error('宿主返回了无效的连接检查结果。')
      }
      setStatus({
        state: 'ready',
        message: '已连接宿主，可以开始开发。',
        platform: capabilities.platform,
        version: appInfo.version,
        capabilities: String(capabilities.commands.length)
      })
    } catch (error) {
      setStatus({
        state: 'error',
        message: error instanceof Error ? error.message : '连接宿主失败。',
        platform: '不可用',
        version: '不可用',
        capabilities: '不可用'
      })
    } finally {
      setIsChecking(false)
    }
  }

  useEffect(() => {
    if (!new URLSearchParams(window.location.search).has('smoke')) {
      void connect()
    }
  }, [])

  return (
    <>
      <a className="skip-link" href="#main-content">
        跳到主要内容
      </a>
      <main id="main-content" className="shell">
        <header className="hero">
          <p className="eyebrow">Kasumi Application</p>
          <h1>__APP_TITLE__</h1>
          <p className="lede">
            一个使用 React 构建的 Kasumi 应用。前端通过类型安全的桥接调用 Windows 与
            Android 原生能力。
          </p>
          <span className="framework">React</span>
        </header>

        <section className="status-card" aria-labelledby="status-title">
          <div className="status-heading">
            <div>
              <p className="eyebrow">Runtime</p>
              <h2 id="status-title">连接状态</h2>
            </div>
            <span
              className="status-dot"
              data-state={status.state}
              aria-hidden="true"
            />
          </div>

          <dl className="facts">
            <div>
              <dt>平台</dt>
              <dd>{status.platform}</dd>
            </div>
            <div>
              <dt>应用版本</dt>
              <dd>{status.version}</dd>
            </div>
            <div>
              <dt>可用能力</dt>
              <dd>{status.capabilities}</dd>
            </div>
          </dl>

          <p className="message" role="status" aria-live="polite">
            {status.message}
          </p>

          <button
            className="button"
            type="button"
            disabled={isChecking}
            onClick={() => void connect()}
          >
            {isChecking ? '检查中…' : '重新检查'}
          </button>
        </section>
      </main>
    </>
  )
}
