<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

// 先定义 SEO 信息
const useSeo = [
    'title' => '出错啦',
    'description' => '您访问的页面不存在',
    'keywords' => '404, error, 错误'
];

// 确保 Archive 部件已初始化
$archive = Typecho_Widget::widget('Widget_Archive', array('type' => 'error'));

Get::Components('AppHeader');
Get::Components('error');
Get::Components('AppFooter');