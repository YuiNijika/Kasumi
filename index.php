<?php
/**
 * Nakano Kasumi Theme
 * 基于TTDF_v2开发的一款简约主题
 * @package Kasumi
 * @author 鼠子(Tomoriゞ)
 * @version 1.0.1
 * @link https://github.com/ShuShuicu/Typecho-Kasumi-Theme
 */
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
Get::Template('AppHeader');
Kasumi::Tomori();
Get::Template('AppFooter');
