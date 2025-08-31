<?php

/**
 * 主题核心文件
 * Theme core file
 * @link https://github.com/YuiNijika/TTDF
 */
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

// 加载框架核心文件
if (file_exists(__DIR__ . '/core/Main.php')) {
    require_once __DIR__ . '/core/Main.php';
} else {
    throw new Exception('TTDF核心文件加载失败, 请检查文件是否存在: ' . __DIR__ . '/core/Main.php');
}

/**
 * 主题自定义代码
 * theme custom code
 */
class Kasumi
{
    public static function initPage()
    {
        if (Get::Is('index')) {
            Get::Components('Index/Body');
        }
    }
}