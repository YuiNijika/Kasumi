<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
// 定义字段配置
return [
    [
        // Radio
        'type' => 'Radio',
        'name' => 'Thumbnail_Enabled',
        'value' => null, // 默认值为 null
        'label' => '缩略图',
        'description' => '请选择是否展示缩略图<small>(仅在列表模式生效)</small>',
        'options' => [
            true => '启用',
            false => '关闭'
        ]
    ],
    [
        // Text
        'type' => 'Text',
        'name' => 'Thumbnail_Url',
        'value' => null, 
        'label' => '缩略图链接',
        'description' => '请输入缩略图Url, 如果没有则自动获取',
    ],
];
