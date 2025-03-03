<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div>
    <a-card title="<?php GetPost::Title(); ?>">
        <template #extra>
            <a-link>分享</a-link>
        </template>
        <div class="Kasumi-typo">
            <?php GetPost::Content(); ?>
        </div>
    </a-card>
</div>