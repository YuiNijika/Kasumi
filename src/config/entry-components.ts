import App_Footer from '../components/App/Footer.vue'
import App_Header from '../components/App/Header.vue'
import App_Navbar from '../components/App/Navbar.vue'
import App_Sidebar from '../components/App/Sidebar.vue'
import Index_ArticleCard from '../components/Index/ArticleCard.vue'
import Index_CardBody from '../components/Index/CardBody.vue'

// 挂载所有组件的函数
export function mountAllComponents() {
  const App_FooterElements = document.querySelectorAll('[data-component="App_Footer"]');
  App_FooterElements.forEach(el => {
    const App_FooterApp = createApp(App_Footer);
    App_FooterApp.mount(el);
  });
  const App_HeaderElements = document.querySelectorAll('[data-component="App_Header"]');
  App_HeaderElements.forEach(el => {
    const App_HeaderApp = createApp(App_Header);
    App_HeaderApp.mount(el);
  });
  const App_NavbarElements = document.querySelectorAll('[data-component="App_Navbar"]');
  App_NavbarElements.forEach(el => {
    const App_NavbarApp = createApp(App_Navbar);
    App_NavbarApp.mount(el);
  });
  const App_SidebarElements = document.querySelectorAll('[data-component="App_Sidebar"]');
  App_SidebarElements.forEach(el => {
    const App_SidebarApp = createApp(App_Sidebar);
    App_SidebarApp.mount(el);
  });
  const Index_ArticleCardElements = document.querySelectorAll('[data-component="Index_ArticleCard"]');
  Index_ArticleCardElements.forEach(el => {
    const Index_ArticleCardApp = createApp(Index_ArticleCard);
    Index_ArticleCardApp.mount(el);
  });
  const Index_CardBodyElements = document.querySelectorAll('[data-component="Index_CardBody"]');
  Index_CardBodyElements.forEach(el => {
    const Index_CardBodyApp = createApp(Index_CardBody);
    Index_CardBodyApp.mount(el);
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
