<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

if (Get::Fields('PostStyleButton') === 'open') {
    // 获取文章的 buttonStyle 字段内容
    $buttons = Get::Fields('buttonStyle');

    // 如果字段不为空
    if (!empty($buttons)) {
        // 分行解析
        $buttonLines = explode("\n", $buttons);

        // 定义域名图标映射
        $domainIcons = [
            // 互联网
            'bt\.cn' => 'Bt.svg',
            'qq\.com' => 'qq.svg',
            'baidu\.com' => 'Baidu.svg',
            'weibo\.com' => 'Weibo.svg',
            'gitee\.com' => 'Gitee.svg',
            'github\.com' => 'Github.svg',
            'aliyun\.com' => 'AliCloud.svg',
            'qcloud\.com' => 'QCloud.svg',
            'tencent\.com' => 'QCloud.svg',
            // 云盘类
            'uc\.CN' => 'uc.svg',
            'quark\.CN' => 'Quark.svg',
            '123pan\.com' => '123Pan.svg',
            '123684\.com' => '123Pan.svg',
            'alipan\.com' => 'AliPan.svg',
            'aliyundrive\.com' => 'AliPan.svg',
        ];

        // 解析按钮数据
        foreach ($buttonLines as $button) {
            $button = trim($button);
            if (preg_match('/\[(.*?)\]\((.*?)\)/', $button, $matches)) {
                // 格式：[按钮名称](按钮链接)
                $buttonText = $matches[1];
                $buttonLink = $matches[2];
                ?>
                <a target="_blank" href="<?php echo htmlspecialchars($buttonLink); ?>">
                    <a-button type="primary" style="margin: 10px 5px 0px 5px;"><?php echo htmlspecialchars($buttonText); ?></a-button>
                </a>
                <?php
            } elseif (preg_match('/^(.*?)\|(.*?)\|(.*?)\|(.*?)$/', $button, $matches)) {
                // 格式：名称|介绍|图片链接|超链
                $buttonText = $matches[1];
                $buttonIntro = $matches[2];
                $buttonImg = $matches[3];
                $buttonLink = $matches[4];

                // 如果图片链接为空，自动识别图标
                if (empty($buttonImg)) {
                    foreach ($domainIcons as $domain => $icon) {
                        if (preg_match('/^https?:\/\/[^\/]+\.' . $domain . '\/?$/', $buttonLink)) {
                            $buttonImg = GetTheme::AssetsUrl(false) . '/images/icon/' . $icon;
                            break;
                        }
                    }
                    // 如果没有匹配到任何预定义的域名，则通过 favicon.cccyun.cc 获取
                    if (empty($buttonImg) && preg_match('/^https?:\/\/([^\/]+)/', $buttonLink, $matches)) {
                        $domain = $matches[1];
                        $buttonImg = 'https://favicon.cccyun.cc/' . $domain;
                    }
                }
                ?>
                <a-card class="mdui-hoverable" style="margin-top: 10px;">
                    <a href="<?php echo htmlspecialchars($buttonLink); ?>" target="_blank" rel="external nofollow">
                        <div class="mdui-card-header">
                            <img class="mdui-card-header-avatar" src="<?php echo htmlspecialchars($buttonImg); ?>" alt="<?php echo htmlspecialchars($buttonLink); ?>">
                            <div class="mdui-card-header-title"><?php echo htmlspecialchars($buttonText); ?></div>
                            <div class="mdui-card-header-subtitle"><?php echo htmlspecialchars($buttonIntro); ?></div>
                        </div>
                    </a>
                </a-card>
                <?php
            }
        }
    }
}
?>