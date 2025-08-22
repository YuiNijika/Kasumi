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
                'value' => null,
                'label' => '副标题',
                'description' => '设置网站副标题，如果为空则不显示。'
            ],
            [
                'type' => 'AddList',
                'name' => 'Header_Navbar',
                'value' => '首页|' . Get::SiteUrl(false),
                'label' => '顶部导航',
                'description' => '请以 text|url 的格式填写。',
            ],
            [
                'type' => 'Text',
                'name' => 'Footer_Info',
                'value' => null,
                'label' => '页脚信息',
                'description' => '自定义页脚信息, 用于替换底部版权站点标题。'
            ],
        ]
    ],
    '文章功能' => [
        'title' => '文章功能',
        'fields' => [
            [
                'type' => 'AddList',
                'name' => 'Thumbnail_Custom',
                'value' => get_theme_file_url('assets/images/thumb/1.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/2.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/3.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/4.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/5.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/6.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/7.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/8.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/9.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/10.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/11.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/12.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/13.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/14.webp', false) . ',' .
                    get_theme_file_url('assets/images/thumb/15.webp', false),
                'label' => '自定义缩略图',
                'description' => '自定义缩略图, 请输入图片链接, 每行一个, 默认使用随机图片。'
            ]
        ]
    ]
];
