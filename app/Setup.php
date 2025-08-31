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
                'type' => 'Html',
                'content' => '<h2>设置导航栏链接</h2>',
            ],
            [
                'type' => 'AddList',
                'name' => 'header_navbar_link',
                'value' => '首页|' . Get::SiteUrl(false),
                'label' => '顶部',
                'description' => '添加导航栏链接，格式为：名称|链接'
            ],
            [
                'type' => 'AddList',
                'name' => 'drawer_navbar_link',
                'value' => '首页|' . Get::SiteUrl(false) . ',同款主题|https://github.com/YuiNijika/Kasumi',
                'label' => '侧边',
                'description' => '添加侧边抽屉导航栏链接，格式为：名称|链接'
            ]
        ]
    ],
    'post' => [
        'title' => '文章设置',
        'fields' => [
            [
                'type' => 'Checkbox',
                'name' => 'post_sidebar',
                'value' => 'small',
                'label' => '文章侧边栏',
                'layout' => 'horizontal',
                'description' => '设置文字侧边栏启用功能',
                'options' => [
                    'small' => '小卡片',
                    'big' => '大卡片',
                ]
            ],
        ]
    ],
];
