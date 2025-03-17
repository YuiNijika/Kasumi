<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

ob_start();
GetPost::Tags(',');
$tagsOutput = ob_get_clean();

ob_start();
GetPost::Category(',');
$categoryOutput = ob_get_clean();

// 定义颜色数组
$colors = [
    'red',
    'orangered',
    'orange',
    'gold',
    'lime',
    'green',
    'cyan',
    'blue',
    'arcoblue',
    'purple',
    'pinkpurple',
    'magenta',
    'gray'
];

// 使用正则表达式解析标签并转换为 <a-tag> 格式
$pattern = '/<a href="([^"]+)">([^<]+)<\/a>/';

// 替换函数 for Tags
$replacementTags = function ($matches) use ($colors) {
    $color = $colors[array_rand($colors)];
    return "<a-tag color=\"$color\"><a href=\"{$matches[1]}\">#{$matches[2]}</a></a-tag>";
};

// 替换函数 for Category
$replacementCategory = function ($matches) use ($colors) {
    $color = $colors[array_rand($colors)];
    return "<a-tag color=\"$color\"><a href=\"{$matches[1]}\">{$matches[2]}</a></a-tag>";
};

// 处理标签
$formattedTags = preg_replace_callback($pattern, $replacementTags, $tagsOutput);
$formattedTags = str_replace(',', '', $formattedTags);

if (trim($formattedTags) === '暂无标签') {
    $color = $colors[array_rand($colors)];
    $formattedTags = "<a-tag color=\"$color\">#暂无标签</a-tag>";
}

// 处理分类
$formattedCategory = preg_replace_callback($pattern, $replacementCategory, $categoryOutput);
$formattedCategory = str_replace(',', '', $formattedCategory);

if (trim($formattedCategory) === '暂无分类') {
    $color = $colors[array_rand($colors)];
    $formattedCategory = "<a-tag color=\"$color\">#暂无分类</a-tag>";
}
?>
<a-card role="article" aria-label="文章卡片">
    <?php
    // 判断是否显示缩略图
    if (Get::Fields('PostStyleThumbnail') == 'open') {
        $thumbnailStyle = Get::Fields('thumbnailStyle') ?? null;
        $thumbnailUrl = $thumbnailStyle ?: get_ArticleThumbnail($this);

        if ($thumbnailUrl) {
    ?>
            <div class="mdui-card-media">
                <div class="thumbnail-post" style="background-image: url(<?php echo htmlspecialchars($thumbnailUrl); ?>);"></div>
            </div>
    <?php
        }
    }
    ?>
    <div class="mdui-card-content">
        <div class="mdui-card-primary-title">
            <?php GetPost::Title(); ?>
        </div>
        <div class="mdui-divider"></div>
        <div class="mdui-card-actions mdui-card-primary-subtitle" v-html="SubTitle"></div>
        <div class="mdui-divider"></div>
        <div class="mdui-card-content">
            <div class="mdui-typo" id="PostContentTypo">
                <?php GetPost::DB_Content_Html() ?>
            </div>
        </div>
        <div class="PostContentButton">
            <div v-html="PostsCopyright"></div>
            <div class="PostContentButtonSeparator">
                THE END
            </div>
            <a-space style="margin-bottom: 5px;">
                <?php echo $formattedCategory; ?>
            </a-space>
            <br>
            <a-space>
                <?php echo $formattedTags; ?>
            </a-space>
            <div v-html="PostShare"></div>
        </div>
    </div>
</a-card>