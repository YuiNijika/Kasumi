<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
// 定义字段配置
return [
    [
        'type' => 'Radio',
        'name' => 'Thumbnail_Enabled',
        'value' => 'true', 
        'label' => '缩略图',
        'description' => '请选择是否展示缩略图<small>(仅在列表模式生效)</small>',
        'options' => [
            'true' => '启用',
            'false' => '关闭'
        ]
    ],
    [
        'type' => 'Text',
        'name' => 'Thumbnail_Url',
        'value' => null, 
        'label' => '缩略图链接',
        'description' => '请输入缩略图Url, 如果没有则自动获取',
    ],
    [
        'type' => 'Radio',
        'name' => 'Download_Enable',
        'value' => 'true', 
        'label' => '下载状态',
        'description' => '是否启用侧边下载',
        'options' => [
            'true' => '启用',
            'false' => '关闭'
        ]
    ],
    [
        'type' => 'Textarea',
        'value' => null,
        'name' => 'Download_Text',
        'label' => '下载链接',
        'description' => '请输入下载链接, 多个链接请以|隔开',
    ],
    [
        'type' => 'Checkbox',
        'value' => null,
        'name' => 'Download_Info',
        'label' => '下载信息',
        'description' => '请选择兼容性等展示内容',
        'options' => [
            'php7.4' => 'PHP 7.4+',
            'php8.0' => 'PHP 8.0+',
            'typecho1.2' => 'Typecho 1.2+',
            'typecho1.3' => 'Typecho 1.3+',
            'custom' => '自定义信息'
        ]
    ],
    [
        'type' => 'Text',
        'value' => null,
        'name' => 'Download_Custom_Info',
        'label' => '自定义信息',
        'description' => '请填写自定义信息',
    ]
];
