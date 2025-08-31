import App_Navbar from '../components/App/Navbar.vue'
import Index_Body from '../components/Index/Body.vue'
import Index_Card from '../components/Index/Card.vue'

// 挂载所有组件的函数
export function mountAllComponents() {
  const App_NavbarElements = document.querySelectorAll('[data-component="App_Navbar"]');
  App_NavbarElements.forEach(el => {
    const App_NavbarApp = createApp(App_Navbar);
    App_NavbarApp.mount(el);
  });
  const Index_BodyElements = document.querySelectorAll('[data-component="Index_Body"]');
  Index_BodyElements.forEach(el => {
    const Index_BodyApp = createApp(Index_Body);
    Index_BodyApp.mount(el);
  });
  const Index_CardElements = document.querySelectorAll('[data-component="Index_Card"]');
  Index_CardElements.forEach(el => {
    const Index_CardApp = createApp(Index_Card);
    Index_CardApp.mount(el);
  });
}

import { createApp } from 'vue';

// 如果在浏览器环境中，立即挂载所有组件
if (typeof window !== 'undefined') {
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', mountAllComponents);
  } else {
    mountAllComponents();
  }
}
