<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php
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

$formattedTags = preg_replace_callback($pattern, $replacementTags, $tagsOutput);
$formattedCategory = preg_replace_callback($pattern, $replacementCategory, $categoryOutput);

// 移除逗号
$formattedTags = str_replace(',', '', $formattedTags);
$formattedCategory = str_replace(',', '', $formattedCategory);
?>
<a-space>
    <?php echo $formattedCategory; ?>
</a-space>
<br>
<a-space>
    <?php echo $formattedTags; ?>
</a-space>