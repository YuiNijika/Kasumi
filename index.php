<?php 
/**
 * Typecho Kasumi主题重构版
 * 如有任何不懂的问题欢迎联系作者<a href="https://space.bilibili.com/435502585"> · B站 · </a>提供帮助。
 * @package Kasumi
 * @author 鼠子(Tomoriゞ)
 * @version 1.0.0
 * @link https://github.com/YuiNijika/Typecho-Kasumi-Theme
 */
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
Get::Components('AppHeader');
Kasumi::initPage();
Get::Components('AppFooter');