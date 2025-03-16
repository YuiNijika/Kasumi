<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-card role="article" aria-label="文章卡片">
    <?php
    // 判断是否显示缩略图
    if (Get::Fields('PostStyleThumbnail') == 'open') {
        $thumbnailStyle = Get::Fields('thumbnailStyle') ?? null;
        $thumbnailUrl = $thumbnailStyle ?: get_ArticleThumbnail($this);

        if ($thumbnailUrl) {
    ?>
            <div class="mdui-card-media">
                <div class="thumbnail-post" style="background-image: url(<?php echo htmlspecialchars($thumbnailUrl); ?>);"></div>
            </div>
    <?php
        }
    }
    ?>
    <div class="mdui-card-content">
        <div class="mdui-card-primary-title">
            <?php GetPost::Title(); ?>
        </div>
        <div class="mdui-divider"></div>
        <div class="mdui-card-content">
            <div class="mdui-typo" id="PostContentTypo">
                <?php GetPost::DB_Content_Html() ?>
            </div>
        </div>
        <div class="PostContentButton">
            <div class="PostContentButtonSeparator">
                THE END
            </div>
        </div>
    </div>
</a-card>