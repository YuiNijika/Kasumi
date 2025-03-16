<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>

<div id="PostContent">
    <article>
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
                <div class="mdui-card-actions mdui-card-primary-subtitle">
                    作者：<a :href="AuthorUrl">{{ Author }}</a>丨{{ PostDate }}丨共 {{ WordCount }} 字
                </div>
                <div class="mdui-divider"></div>
                <div class="mdui-card-content">
                    <div class="mdui-typo" id="PostContent">
                        <?php GetPost::Content(); ?>
                    </div>
                </div>
                <?php Kasumi::Components('PostContentButton'); ?>
            </div>
        </a-card>
    </article>
</div>