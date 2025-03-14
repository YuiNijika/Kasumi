<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<article>
    <a-card
        role="article"
        aria-label="文章卡片"
        title="<?php GetPost::Title(); ?>">
        <template #extra>
            <?php Kasumi::Components('PostShare') ?>
        </template>
        <div class="Kasumi-typo">
            <div class="mdui-typo">
                <?php GetPost::Content(); ?>
            </div>
            <div class="mdui-divider"></div>
            <div style="padding: 10px 0px;">
                <?php Kasumi::Components('PostContentButton') ?>
            </div>
        </div>
    </a-card>
</article>