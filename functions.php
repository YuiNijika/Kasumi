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
    /**
     * 获取随机缩略图URL
     *
     * @return string 随机缩略图URL
     */
    public static function get_RandomThumbnail()
    {
        // 从后台设置中获取自定义缩略图列表
        $thumbnails = Helper::options()->Kasumi_Thumbnail_Custom;

        if (!empty($thumbnails)) {
            // 支持按换行符或空格分割缩略图列表
            $separators = ["\n", "\r\n", " "];
            $thumbnailList = [];

            // 尝试使用不同的分隔符分割
            foreach ($separators as $separator) {
                $list = array_filter(array_map('trim', explode($separator, $thumbnails)));
                if (count($list) > count($thumbnailList)) {
                    $thumbnailList = $list;
                }
            }

            // 如果还是没有有效的缩略图列表，则按空格分割整个字符串
            if (empty($thumbnailList) || (count($thumbnailList) == 1 && strpos($thumbnailList[0], ' ') !== false)) {
                $thumbnailList = array_filter(array_map('trim', explode(' ', $thumbnails)));
            }

            if (!empty($thumbnailList)) {
                // 随机选择一个缩略图
                $randIndex = array_rand($thumbnailList);
                return trim($thumbnailList[$randIndex]);
            }
        }

        // 默认缩略图路径
        return Helper::options()->siteUrl . Helper::options()->themeUrl . '/assets/images/thumb/thumbnail.svg';
    }

    /**
     * 获取文章缩略图URL
     *
     * @param Widget_Abstract_Contents $widget 文章对象
     * @return string 缩略图URL
     */
    public static function get_ArticleThumbnail($widget)
    {
        // 自定义缩略图逻辑  
        if ($customThumb = $widget->fields->ThumbnailUrl) {
            return $customThumb;
        }

        // 尝试从内容中提取图片URL  
        if ($contentThumb = self::extractImageFromContent($widget->content)) {
            return $contentThumb;
        }

        // 尝试从附件中获取图片URL  
        if ($attachmentThumb = self::getAttachmentImageUrl($widget)) {
            return $attachmentThumb;
        }

        // 调用辅助函数获取随机缩略图  
        return self::get_RandomThumbnail();
    }

    /**
     * 从文章内容中提取图片URL
     *
     * @param string $content 文章内容
     * @return string|null 图片URL或null
     */
    public static function extractImageFromContent($content)
    {
        $pattern = '/<img.*?src="(.*?)"[^>]*>/i';
        if (preg_match($pattern, $content, $matches) && strlen($matches[1]) > 7) {
            return htmlspecialchars($matches[1]);
        }
        return null;
    }

    /**
     * 从附件中获取图片URL
     *
     * @param Widget_Abstract_Contents $widget 文章对象
     * @return string|null 图片URL或null
     */
    public static function getAttachmentImageUrl($widget)
    {
        $attach = $widget->attachments(1)->attachment;
        if ($attach && $attach->isImage) {
            return htmlspecialchars($attach->url);
        }
        return null;
    }
}
