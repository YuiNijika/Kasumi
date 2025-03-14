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
            <?php GetPost::Content(); ?>
        </div>
    </a-card>
</article>