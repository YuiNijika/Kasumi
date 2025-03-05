<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-dropdown trigger="hover">
    <a-link>分享</a-link>
    <template #content>
        <?php
        // 获取缩略图URL
        $thumbnailUrl = get_ArticleThumbnail($this);
        if ($thumbnailUrl) {
            // 如果缩略图URL存在，则输出缩略图  
        ?>
            <a-doption>
                <a href="http://connect.qq.com/widget/shareqq/index.html?url=<?php GetPost::Permalink(); ?>&sharesource=qzone&title=<?php GetPost::Title() ?>&pics=<?php echo htmlspecialchars($thumbnailUrl); ?>&summary=<?php GetPost::Excerpt(150) ?>&desc=<?php GetPost::Excerpt(50) ?>" target="_blank">QQ好友</a>
            </a-doption>
            <a-doption>
                <a href="https://sns.qzone.qq.com/cgi-bin/qzshare/cgi_qzshare_onekey?url=<?php GetPost::Permalink(); ?>&sharesource=qzone&title=<?php GetPost::Title(); ?>&pics=<?php echo htmlspecialchars($thumbnailUrl) ?>&summary=<?php GetPost::Excerpt(100) ?>" target="_blank">QQ空间</a>
            </a-doption>
        <?php } ?>
    </template>
</a-dropdown>