import App_Footer from '../components/App/Footer.vue'
import App_Header from '../components/App/Header.vue'
import App_Navbar from '../components/App/Navbar.vue'
import error from '../components/error.vue'
import Index_ArticleCard from '../components/Index/ArticleCard.vue'
import Index_ArticleList from '../components/Index/ArticleList.vue'
import Index_Body from '../components/Index/Body.vue'
import Post_ArticleCard from '../components/Post/ArticleCard.vue'
import Post_Body from '../components/Post/Body.vue'
import Post_Comments from '../components/Post/Comments.vue'
import Post_Sidebar from '../components/Post/Sidebar.vue'

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
  const errorElements = document.querySelectorAll('[data-component="error"]');
  errorElements.forEach(el => {
    const errorApp = createApp(error);
    errorApp.mount(el);
  });
  const Index_ArticleCardElements = document.querySelectorAll('[data-component="Index_ArticleCard"]');
  Index_ArticleCardElements.forEach(el => {
    const Index_ArticleCardApp = createApp(Index_ArticleCard);
    Index_ArticleCardApp.mount(el);
  });
  const Index_ArticleListElements = document.querySelectorAll('[data-component="Index_ArticleList"]');
  Index_ArticleListElements.forEach(el => {
    const Index_ArticleListApp = createApp(Index_ArticleList);
    Index_ArticleListApp.mount(el);
  });
  const Index_BodyElements = document.querySelectorAll('[data-component="Index_Body"]');
  Index_BodyElements.forEach(el => {
    const Index_BodyApp = createApp(Index_Body);
    Index_BodyApp.mount(el);
  });
  const Post_ArticleCardElements = document.querySelectorAll('[data-component="Post_ArticleCard"]');
  Post_ArticleCardElements.forEach(el => {
    const Post_ArticleCardApp = createApp(Post_ArticleCard);
    Post_ArticleCardApp.mount(el);
  });
  const Post_BodyElements = document.querySelectorAll('[data-component="Post_Body"]');
  Post_BodyElements.forEach(el => {
    const Post_BodyApp = createApp(Post_Body);
    Post_BodyApp.mount(el);
  });
  const Post_CommentsElements = document.querySelectorAll('[data-component="Post_Comments"]');
  Post_CommentsElements.forEach(el => {
    const Post_CommentsApp = createApp(Post_Comments);
    Post_CommentsApp.mount(el);
  });
  const Post_SidebarElements = document.querySelectorAll('[data-component="Post_Sidebar"]');
  Post_SidebarElements.forEach(el => {
    const Post_SidebarApp = createApp(Post_Sidebar);
    Post_SidebarApp.mount(el);
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
