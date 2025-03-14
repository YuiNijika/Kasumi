<?php

/**
 * Options Functions
 */
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

/**
 * THEME_URL
 * 用于后台设置获取主题目录
 */
define("THEME_URL", GetTheme::Url(false));
define("THEME_NAME", GetTheme::Name(false));

/**
 * 辅助函数：创建表单元素
 */
function TTDF_FormElement($type, $name, $value, $label, $description, $options = [])
{
    // 确保 _t() 的参数不为 null
    $label = $label ?? '';
    $description = $description ?? '';

    $class = '\\Typecho\\Widget\\Helper\\Form\\Element\\' . $type;
    if ($type === 'Radio' || $type === 'Select' || $type === 'Checkbox') {
        // Radio、Select、Checkbox 类型需要额外的 options 参数
        return new $class($name, $options, $value, _t($label), _t($description));
    } else {
        return new $class($name, null, $value, _t($label), _t($description));
    }
}

function themeConfig($form)
{
?>
    <style>
        body {
            background: url(<?php GetTheme::Url() ?>/screenshot.webp) no-repeat 0 0;
            background-size: cover;
            background-attachment: fixed;
        }

        .main {
            background-color: #ffffffde;
            padding: 10px
        }

        .typecho-foot {
            padding: 1em 0 3em;
            background-color: #ffffffde;
        }

        .typecho-head-nav .operate a {
            background-color: #202328;
        }

        .typecho-option-tabs li {
            float: left;
            background-color: #fffbcc;
        }
    </style>
<?php
    // 定义表单元素配置
    $formElements = [
        [
            // Favicon
            'type' => 'Text',
            'name' => 'FaviconUrl',
            'value' => GetTheme::Url(false) . '/Assets/images/Kasumi.svg',
            'label' => 'Favicon',
            'description' => '在这里填入一个图片 URL 地址, 以在网站标题前加上一个图标'
        ],
        [
            // 副标题
            'type' => 'Text',
            'name' => 'SubTitle',
            'value' => null,
            'label' => '副标题',
            'description' => '在这里填入一个文字, 以在网站标题后加上一个副标题'
        ],
        [
            // ICP备案号
            'type' => 'Text',
            'name' => 'IcpCode',
            'value' => null,
            'label' => '备案号',
            'description' => '在这里填入网站ICP备案号, 将显示在底部右侧位置'
        ],
        [
            // 导航菜单
            'type' => 'Textarea',
            'name' => 'SidebarNav',
            'value' => '首页|home|' . Get::SiteUrl(false),
            'label' => '侧边导航',
            'description' => '侧边自定义导航栏，格式为 名称|图标|链接，多个导航请换行。'
        ],
        [
            // 轮播图开关
            'type' => 'Select',
            'name' => 'CarouselSwitch',
            'value' => 'close', 
            'label' => '轮播图',
            'description' => '是否启用轮播图',
            'options' => [
                'close' => '关闭',
                'open' => '启用',
            ]
        ],
        [
            // 缩略图高度
            'type' => 'Text',
            'name' => 'CarouselHeight',
            'value' => '340px',
            'label' => '缩略图高度',
            'description' => '设置轮播图缩略图的高度，单位为px，默认为340px。'
        ],
        [
            // 缩略图内容
            'type' => 'Textarea',
            'name' => 'CarouselContent',
            'value' => null,
            'label' => '缩略图图片',
            'description' => '请以 跳转链接|图片链接 的格式输入，每行一个。'
        ],
    ];
    // 循环添加表单元素
    foreach ($formElements as $TTDF) {
        $form->addInput(TTDF_FormElement(
            $TTDF['type'],
            $TTDF['name'],
            $TTDF['value'] ?? null,
            $TTDF['label'] ?? '',
            $TTDF['description'] ?? '',
            $TTDF['options'] ?? []
        ));
    }
}
