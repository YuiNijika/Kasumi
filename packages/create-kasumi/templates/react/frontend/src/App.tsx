import { useEffect, useState } from 'react'
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
        message: '已连接 Kasumi 运行时，可以开始编辑。',
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

      <div className="app-shell">
        <header className="masthead">
          <div className="brand">
            <svg
              className="brand-mark"
              viewBox="0 0 42 42"
              width="30"
              height="30"
              aria-hidden="true"
            >
              <path
                d="m21 2.5 4.7 13.1 13.9.6-11 8.5 3.8 13.4L21 30.2 9.6 38.1l3.8-13.4-11-8.5 13.9-.6Z"
                fill="var(--brand-pink)"
                stroke="currentColor"
                strokeLinejoin="round"
                strokeWidth="2.2"
              />
              <circle
                cx="33"
                cy="8.5"
                r="3.4"
                fill="var(--brand-orange)"
                stroke="currentColor"
                strokeWidth="1.5"
              />
            </svg>
            <span className="brand-name" translate="no">
              Kasumi
            </span>
            <span className="brand-tag">app</span>
          </div>

          <div className="stack">
            <span className="stack-label">Built with</span>
            <span className="stack-logos" aria-hidden="true">
              <span className="tech-mark">
                <svg viewBox="-12 -12 24 24" width="21" height="21">
                  <circle r="2.2" fill="#61dafb" />
                  <g fill="none" stroke="#61dafb" strokeWidth="1">
                    <ellipse rx="10" ry="4.2" />
                    <ellipse rx="10" ry="4.2" transform="rotate(60)" />
                    <ellipse rx="10" ry="4.2" transform="rotate(120)" />
                  </g>
                </svg>
              </span>
              <span className="tech-mark">
                <svg viewBox="0 0 24 24" width="20" height="20">
                  <path d="M3 2.6 12 21.5 21 2.6 12 5.4Z" fill="#ffd62e" />
                  <path d="M12 5.4 21 2.6 12 21.5Z" fill="#8f63ff" />
                </svg>
              </span>
            </span>
            <span className="stack-name">React + Vite</span>
          </div>
        </header>

        <main id="main-content" className="content">
          <div className="intro">
            <section className="hero" aria-labelledby="app-title">
              <p className="eyebrow">
                <span>Web 界面</span>
                <span aria-hidden="true">+</span>
                <span>原生宿主</span>
              </p>
              <h1 id="app-title">__APP_TITLE__</h1>
              <p className="lede">
                页面已经运行在 Kasumi
                的原生容器中。修改前端文件即可继续开发，需要系统能力时通过
                KSM 桥接调用。
              </p>
              <div className="actions">
                <button
                  className="button"
                  type="button"
                  disabled={isChecking}
                  onClick={() => void connect()}
                >
                  {isChecking ? '检查中…' : '重新检查'}
                </button>
                <span className="command">
                  <span aria-hidden="true">$</span> pnpm ksm dev
                </span>
              </div>
            </section>

            <aside className="aside" aria-label="开发提示">
              <p className="aside-label">Start here</p>
              <p className="aside-copy">
                从 <code>frontend/</code> 开始编辑。保存后页面会立即更新，原生能力由{' '}
                <code>KSM</code> 统一提供。
              </p>
              <p className="aside-command">
                <span aria-hidden="true">→</span> KSM.app.getInfo()
              </p>
            </aside>
          </div>

          <section className="runtime" aria-labelledby="runtime-title">
            <div className="runtime-heading">
              <h2 id="runtime-title">Runtime</h2>
              <span className="status-badge" data-state={status.state}>
                <span className="status-dot" aria-hidden="true" />
                <span>{statusLabels[status.state]}</span>
              </span>
            </div>

            <dl className="metrics">
              <div>
                <dt>Platform</dt>
                <dd>{status.platform}</dd>
              </div>
              <div>
                <dt>App version</dt>
                <dd>{status.version}</dd>
              </div>
              <div>
                <dt>Capabilities</dt>
                <dd>{status.capabilities}</dd>
              </div>
            </dl>

            <p className="runtime-message" role="status" aria-live="polite">
              {status.message}
            </p>
          </section>
        </main>

        <footer className="footer">
          <span>Win32 / Android</span>
          <span>
            Kasumi <span className="footer-mark" aria-hidden="true">✦</span>
          </span>
        </footer>
      </div>
    </>
  )
}
