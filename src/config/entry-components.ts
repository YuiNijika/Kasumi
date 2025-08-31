import App_Navbar from '../components/App/Navbar.vue'
import ErrorBody from '../components/ErrorBody.vue'
import Index_Body from '../components/Index/Body.vue'
import Index_Card from '../components/Index/Card.vue'
import Post_Body from '../components/Post/Body.vue'
import Post_Card from '../components/Post/Card.vue'
import Post_Common from '../components/Post/Common.vue'

// 挂载所有组件的函数
export function mountAllComponents() {
  const App_NavbarElements = document.querySelectorAll('[data-component="App_Navbar"]');
  App_NavbarElements.forEach(el => {
    const App_NavbarApp = createApp(App_Navbar);
    App_NavbarApp.mount(el);
  });
  const ErrorBodyElements = document.querySelectorAll('[data-component="ErrorBody"]');
  ErrorBodyElements.forEach(el => {
    const ErrorBodyApp = createApp(ErrorBody);
    ErrorBodyApp.mount(el);
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
  const Post_BodyElements = document.querySelectorAll('[data-component="Post_Body"]');
  Post_BodyElements.forEach(el => {
    const Post_BodyApp = createApp(Post_Body);
    Post_BodyApp.mount(el);
  });
  const Post_CardElements = document.querySelectorAll('[data-component="Post_Card"]');
  Post_CardElements.forEach(el => {
    const Post_CardApp = createApp(Post_Card);
    Post_CardApp.mount(el);
  });
  const Post_CommonElements = document.querySelectorAll('[data-component="Post_Common"]');
  Post_CommonElements.forEach(el => {
    const Post_CommonApp = createApp(Post_Common);
    Post_CommonApp.mount(el);
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
