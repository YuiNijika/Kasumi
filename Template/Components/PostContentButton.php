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

<a-space style="margin-bottom: 5px;">
    <?php echo $formattedCategory; ?>
</a-space>
<br>
<a-space>
    <?php echo $formattedTags; ?>
</a-space>