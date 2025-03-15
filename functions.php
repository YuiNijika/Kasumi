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

/**
 * 获取随机缩略图URL
 *
 * @param string $base_url 基础URL路径
 * @param int $maxImages 最大图片数量
 * @return string 随机缩略图URL
 */
function get_RandomThumbnail($base_url, $maxImages = 15)
{
    // 生成一个1到$maxImages之间的随机数  
    $rand = mt_rand(1, $maxImages);
    // 构造随机缩略图的URL  
    return $base_url . $rand . '.webp';
}

/**
 * 获取文章缩略图URL
 *
 * @param Widget_Abstract_Contents $widget 文章对象
 * @return string 缩略图URL
 */
function get_ArticleThumbnail($widget)
{
    // 自定义缩略图逻辑  
    if ($customThumb = $widget->fields->ThumbnailUrl) {
        return $customThumb;
    }

    // 尝试从内容中提取图片URL  
    if ($contentThumb = extractImageFromContent($widget->content)) {
        return $contentThumb;
    }

    // 尝试从附件中获取图片URL  
    if ($attachmentThumb = getAttachmentImageUrl($widget)) {
        return $attachmentThumb;
    }

    // 获取默认缩略图路径
    $base_url = '/Assets/images/thumb/'; // 默认缩略图路径  

    // 如果设置了articleImgSpeed，则使用它作为图片的基本URL  
    if (!empty(Helper::options()->articleImgSpeed)) {
        $base_url = Helper::options()->articleImgSpeed;
        // 确保URL以斜杠结尾  
        if (substr($base_url, -1) !== '/') {
            $base_url .= '/';
        }
    } else {
        // 使用themeUrl和默认的图片路径  
        $base_url = $widget->widget('Widget_Options')->themeUrl . $base_url;
    }

    // 调用辅助函数获取随机缩略图  
    return get_RandomThumbnail($base_url);
}

/**
 * 从文章内容中提取图片URL
 *
 * @param string $content 文章内容
 * @return string|null 图片URL或null
 */
function extractImageFromContent($content)
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
function getAttachmentImageUrl($widget)
{
    $attach = $widget->attachments(1)->attachment;
    if ($attach && $attach->isImage) {
        return htmlspecialchars($attach->url);
    }
    return null;
}

/**
 * 短代码解析
 */
class ShortCodeParser {
    public function __construct() {
        // 为文章内容和摘要添加过滤器
        Typecho_Plugin::factory('Widget_Abstract_Contents')->contentEx = array($this, 'add_shortcode_support');
        Typecho_Plugin::factory('Widget_Abstract_Contents')->excerptEx = array($this, 'add_shortcode_support');
    }

    /**
     * 解码HTML实体并解析短代码
     *
     * @param string $content
     * @return string
     */
    public function add_shortcode_support($content) {
        if(!empty($content)){
        $content = htmlspecialchars_decode($content); // 先解码HTML实体
        $content = $this->parse_button_shortcode($content);
        }
        return $content;
    }

    /**
     * 解析按钮短代码
     *
     * @param string $content
     * @return string
     */
    private function parse_button_shortcode($content) {
        // 按钮
        $pattern = '/\[b\s*url="(.*?)"\](.*?)\[\/b\]/i';
        $callback = function ($matches) {
            $buttonLink = htmlspecialchars($matches[1], ENT_QUOTES, 'UTF-8');
            $buttonName = htmlspecialchars($matches[2], ENT_QUOTES, 'UTF-8');
            return '<a target="_blank" rel="external nofollow" href="' . $buttonLink . '">
                <button class="mdui-btn mdui-btn-raised mzei-ripple mdui-color-theme-accent"><b>' . $buttonName . '</b></button></a>';
        };
        $content = preg_replace_callback($pattern, $callback, $content);

        // 纸片短代码
        $pattern_c = '/\[c url="(.*?)" img="(.*?)"\](.*?)\[\/c\]/i';
        $callback_c = function ($matches) {
            $link = htmlspecialchars($matches[1], ENT_QUOTES, 'UTF-8');
            $image = htmlspecialchars($matches[2], ENT_QUOTES, 'UTF-8');
            $text = htmlspecialchars($matches[3], ENT_QUOTES, 'UTF-8');
            return '
                <div class="mdui-chip">
                    <img class="mdui-chip-icon" src="' . $image . '" alt="Chip Icon" />
                        <span class="mdui-chip-title">
                        <a target="_blank" rel="external nofollow" href="' . $link . '">' . $text . '</a>
                    </span>
                </div>
            ';
        };
        $content = preg_replace_callback($pattern_c, $callback_c, $content);

        // 提示短代码
        $pattern_t = '/\[t\](.*?)\[\/t\]/i';
        $callback_t = function ($matches) {
            $text = htmlspecialchars($matches[1], ENT_QUOTES, 'UTF-8');
            return '
            <div class="mdui-card mdui-color-indigo">
			<div class="mdui-card-content mdui-valign">
				<div class="mdui-m-r-2"><i class="mdui-icon material-icons">info</i></div>
				<div style="font-size: 1.3em;">' . $text . '</strong></div>
			</div>
		</div>
            ';
        };
        $content = preg_replace_callback($pattern_t, $callback_t, $content);

        return $content;

    }
}

// 初始化短代码解析器
$shortCodeParser = new ShortCodeParser();