import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { KSM } from 'kasumi-bridge'
import './index.css'
import App from './App.tsx'

KSM.init()

if (new URLSearchParams(window.location.search).has('smoke')) {
  void KSM.ping().then((pong) => {
    if (!pong.pong) {
      throw new Error('宿主返回了无效的连接检查结果。')
    }
    return KSM.app.exit()
  })
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
