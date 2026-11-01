import { createApp } from 'vue'
import { KSM } from 'kasumi-bridge'
import './style.css'
import App from './App.vue'

KSM.init()

if (new URLSearchParams(window.location.search).has('smoke')) {
  void KSM.ping().then((pong) => {
    if (!pong.pong) {
      throw new Error('宿主返回了无效的连接检查结果。')
    }
    return KSM.app.exit()
  })
}

createApp(App).mount('#app')
