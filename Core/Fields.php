<?php
/**
 * Fields Functions
 */
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

function themeFields($layout)
{
    // 定义字段配置
    $fieldElements = [
        [
            // 是否缩略图
            'type' => 'Radio',
            'name' => 'PostStyleThumbnail',
            'value' => 'close', // 默认值为 'close'
            'label' => '顶图',
            'description' => '文章顶部将显示图片',
            'options' => [
                'close' => '关闭',
                'open' => '打开'
            ]
        ],
        [
            // 缩略图字段
            'type' => 'Text',
            'name' => 'thumbnailStyle',
            'value' => null, // 默认值为 null
            'label' => '缩略图',
            'description' => '请填入图片链接用于自定义文章缩略图 / 顶图，不填写取文章第一张图。',
            'attributes' => [
                'style' => 'width: 100%;'
            ]
        ],
        [
            // 是否文章跳转
            'type' => 'Radio',
            'name' => 'PostStyleGoUrl',
            'value' => 'close', // 默认值为 'close'
            'label' => '跳转',
            'description' => '在文章列表点击文章标题将跳转到指定链接。',
            'options' => [
                'close' => '关闭',
                'open' => '打开'
            ]
        ],
        [
            // 跳转链接字段
            'type' => 'Text',
            'name' => 'goUrlStyle',
            'value' => null, // 默认值为 null
            'label' => '跳转链接',
            'description' => '请输入文章跳转链接。',
            'attributes' => [
                'style' => 'width: 100%;'
            ]
        ],
        [
            // 是否按钮
            'type' => 'Radio',
            'name' => 'PostStyleButton',
            'value' => 'close', // 默认值为 'close'
            'label' => '按钮',
            'description' => '文章顶部将显示按钮',
            'options' => [
                'close' => '关闭',
                'open' => '打开'
            ]
        ],
        [
            // 按钮字段
            'type' => 'Textarea',
            'name' => 'buttonStyle',
            'value' => null, // 默认值为 null
            'label' => '按钮内容',
            'description' => '请填入按钮链接用于自定义文章底部按钮<hr>格式：<br>短按钮：[按钮名称](按钮链接)<br>长按钮：名称|介绍|图片|超链<br>如果图片链接为空则自动识别<hr>',
            'attributes' => [
                'style' => 'width: 100%;height: 100px;'
            ]
        ]
    ];

    // 循环添加字段
    foreach ($fieldElements as $field) {
        $element = TTDF_FormElement(
            $field['type'],
            $field['name'],
            $field['value'] ?? null,
            $field['label'] ?? '',
            $field['description'] ?? '',
            $field['options'] ?? []
        );

        // 设置字段属性（如 style）
        if (isset($field['attributes'])) {
            foreach ($field['attributes'] as $attr => $value) {
                $element->input->setAttribute($attr, $value);
            }
        }

        $layout->addItem($element);
    }
}