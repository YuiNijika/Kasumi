<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
// 定义所有设置项
return [
    '基础设置' => [
        'title' => '基础设置',
        'fields' => [
            [
                'type' => 'Text',
                'name' => 'SubTitle',
                'value' => '由 Kasumi 主题强力驱动',
                'label' => '副标题',
                'description' => '设置网站副标题，如果为空则不显示。'
            ],
            [
                'type' => 'AddList',
                'name' => 'navbar_link',
                'value' => '首页|home|' . Get::SiteUrl(false),
                'label' => '导航栏链接',
                'description' => '添加导航栏链接，格式为：名称|图标|链接'
            ],
            [
                'type' => 'DialogSelect',
                'name' => 'theme_style',
                'value' => 'theme1',
                'label' => '主题选择',
                'description' => '点击按钮打开对话框选择主题，支持单选模式。',
                'title' => '选择主题',
                'multiple' => false,
                'options' => [
                    'default' => '默认主题',
                    'blue' => '蓝色主题',
                    'pink' => '粉色主题',
                    'green' => '绿色主题',
                    'yellow' => '黄色主题',
                    'orange' => '橙色主题',
                    'red' => '红色主题',
                    'purple' => '紫色主题',
                    'white' => '白色主题',
                ]
            ],
        ]
    ],
    'index' => [
        'title' => '首页设置',
        'fields' => [
            [
                'type' => 'Radio',
                'name' => 'index_card_style',
                'value' => 'small',
                'label' => '文章列表',
                'layout' => 'horizontal',
                'description' => '设置首页文章列表卡片的样式。',
                'options' => [
                    'small' => '小卡片',
                    'big' => '大卡片',
                ]
            ],
        ]
    ],
];
