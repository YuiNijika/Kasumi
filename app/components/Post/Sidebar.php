<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;

// 域名映射函数
function getDomainName($url)
{
    $parsedUrl = parse_url($url);
    $host = isset($parsedUrl['host']) ? $parsedUrl['host'] : '';

    if (empty($host)) {
        return '未知链接';
    }

    // 域名映射表（使用正则表达式）
    $domainPatterns = array(
        '/github\.com/' => 'GitHub',
        '/github\.io/' => 'GitHub Pages',
        '/pan\.baidu\.com|yun\.baidu\.com/' => '百度网盘',
        '/share\.weiyun\.com/' => '微云',
        '/drive\.google\.com/' => 'Google Drive',
        '/onedrive\.live\.com/' => 'OneDrive',
        '/mega\.nz/' => 'Mega',
        '/mediafire\.com/' => 'MediaFire',
        '/aliyundrive\.com|aliyundrive\.net/' => '阿里云盘',
        '/cloud\.189\.cn/' => '天翼云盘',
        '/.*lanzou.*\.com/' => '蓝奏云',
        '/cowtransfer\.com/' => '奶牛快传',
        '/wetransfer\.com/' => 'WeTransfer',
        '/send\.firefox\.com|send\.vis\.ee|send\.ephemeral\.land/' => 'Send',
        '/pixeldrain\.com/' => 'PixelDrain',
        '/gofile\.io/' => 'GoFile',
        '/anonfiles\.com|bayfiles\.com/' => 'AnonFiles',
    );

    // 按照模式匹配域名
    foreach ($domainPatterns as $pattern => $name) {
        if (preg_match($pattern, $host)) {
            return $name;
        }
    }

    // 返回主域名作为默认值
    return ucfirst($host);
}

// 获取下载信息数组
$downloadInfo = Get::Fields('Download_Info', true);
// 确保是数组格式
$downloadInfo = is_array($downloadInfo) ? $downloadInfo : (is_string($downloadInfo) ? explode(',', $downloadInfo) : array());

// 定义信息映射
$infoMap = array(
    'php7.4' => 'PHP 7.4+',
    'php8.0' => 'PHP 8.0+',
    'typecho1.2' => 'Typecho 1.2+',
    'typecho1.3' => 'Typecho 1.3+',
    'custom' => '自定义信息'
);

// 所有可能的选项
$allOptions = array(
    'php7.4' => 'PHP 7.4+',
    'php8.0' => 'PHP 8.0+',
    'typecho1.2' => 'Typecho 1.2+',
    'typecho1.3' => 'Typecho 1.3+',
    'custom' => '自定义信息'
);

// 获取按钮颜色类
function getButtonClass($index)
{
    $classes = array(
        'btn-primary',
        'btn-secondary',
        'btn-accent',
        'btn-info',
        'btn-success',
        'btn-warning',
        'btn-error'
    );
    return $classes[$index % count($classes)];
}

// 使用正则表达式解析下载链接
function parseDownloadLinks($text)
{
    if (empty($text)) {
        return array();
    }

    // 使用正则表达式分割链接，支持多种分隔符并去除空格
    $links = preg_split('/[\|\n\r]+/', $text, -1, PREG_SPLIT_NO_EMPTY);

    // 过滤并清理链接
    $cleanLinks = array();
    foreach ($links as $link) {
        // 去除首尾空格
        $link = trim($link);
        // 验证链接格式（简单验证）
        if (!empty($link) && (strpos($link, 'http://') === 0 || strpos($link, 'https://') === 0)) {
            $cleanLinks[] = $link;
        } else if (!empty($link) && strpos($link, '://') === false) {
            // 如果没有协议，自动添加https://
            $cleanLinks[] = 'https://' . $link;
        } else if (!empty($link)) {
            $cleanLinks[] = $link;
        }
    }

    return $cleanLinks;
}
?>
<div class="sticky-sidebar">
    <?php
    $customInfo = Get::Fields('Download_Custom_Info', true);
    if (!empty($customInfo) && in_array('custom', $downloadInfo)):
    ?>
        <div role="alert" class="alert alert-warning alert-soft mb-2">
            <span><?php echo $customInfo; ?></span>
        </div>
    <?php endif; ?>

    <div class="card bg-base-100 shadow-sm">

        <div class="card-body">
            <span class="badge badge-xs badge-warning"><?php GetPost::Category(true); ?></span>
            <div class="flex justify-between">
                <h2 class="text-3xl font-bold"><?php GetPost::Date(); ?></h2>
            </div>

            <?php if (!empty($downloadInfo) || !empty(Get::Fields('Download_Custom_Info', true))): ?>
                <ul class="mt-6 flex flex-col gap-2 text-xs">
                    <?php
                    // 显示选中的项目
                    foreach ($downloadInfo as $info): ?>
                        <?php if (!empty($info)): ?>
                            <li>
                                <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-success" fill="none"
                                    viewBox="0 0 24 24" stroke="currentColor">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                                </svg>
                                <span>
                                    <?php
                                    // 显示对应的标签文本
                                    if (isset($infoMap[$info])) {
                                        echo $infoMap[$info];
                                    } else {
                                        echo $info; // 如果没有映射则直接显示
                                    }
                                    ?>
                                </span>
                            </li>
                        <?php endif; ?>
                    <?php endforeach; ?>

                    <?php
                    // 显示未选中的项目作为不兼容项
                    $uncheckedItems = array_diff(array_keys($allOptions), $downloadInfo);
                    foreach ($uncheckedItems as $unchecked): ?>
                        <li class="opacity-50">
                            <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-base-content/50"
                                fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                            </svg>
                            <span class="line-through"><?php echo $allOptions[$unchecked]; ?></span>
                        </li>
                    <?php endforeach; ?>

                    <?php
                    // 处理自定义信息
                    $customInfo = Get::Fields('Download_Custom_Info', true);
                    if (!in_array('custom', $downloadInfo) && !empty($customInfo)): ?>
                        <li class="opacity-50">
                            <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-base-content/50"
                                fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                            </svg>
                            <span class="line-through"><?php echo htmlspecialchars($customInfo); ?></span>
                        </li>
                    <?php endif; ?>
                </ul>
            <?php endif; ?>

            <div class="mt-6">
                <button class="btn btn-primary btn-block" onclick="downloadInfo.showModal()">立即下载</button>
            </div>
        </div>
    </div>
</div>

<dialog id="downloadInfo" class="modal">
    <div class="modal-box">
        <h3 class="text-lg font-bold">下载信息</h3>
        <?php
        // 获取下载链接
        $downloadText = Get::Fields('Download_Text', true);
        // 使用正则表达式解析下载链接
        $downloadLinks = parseDownloadLinks($downloadText);
        ?>

        <?php if (!empty($downloadLinks)): ?>
            <?php foreach ($downloadLinks as $index => $link): ?>
                
                    <?php if (!empty($link)): ?>
                        <?php
                        $domainName = getDomainName($link);
                        $buttonClass = getButtonClass($index);
                        ?>
                        <a href="<?php echo htmlspecialchars($link); ?>" class="mx-1 btn <?php echo $buttonClass; ?>" target="_blank">
                            <?php echo htmlspecialchars($domainName); ?>
                        </a>
                    <?php endif; ?>
                
            <?php endforeach; ?>
        <?php endif; ?>
        <div class="modal-action">
            <form method="dialog">
                <button class="btn">关闭</button>
            </form>
        </div>
    </div>
</dialog>