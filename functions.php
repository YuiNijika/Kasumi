<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
// 引入框架配置文件
require_once 'Core/TTDF.php';

/**
 * Kasumi 主题
 */
class Kasumi
{
    /**
     * 获取组件
     * @param string $file 文件名
     */
    public static function Components($file)
    {
        Get::Template('Components/' . $file);
    }
    public static function Pages($file)
    {
        Get::Template('Pages/' . $file);
    }
    /**
     * 获取模板
     * @param string $file 文件名
     */
    public static function Tomori()
    {
        if (Get::Is("index")) {
            Kasumi::Pages('Index');
        } elseif (Get::Is("post")) {
            Kasumi::Pages('Post');
        } elseif (Get::Is("page")) {
            Kasumi::Pages('Page');
        } else {
            Kasumi::Pages('Index');
        }
    }
}
