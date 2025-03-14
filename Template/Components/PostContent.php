<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-card title="<?php GetPost::Title(); ?>">
    <template #extra>
        <?php Kasumi::Components('PostShare') ?>
    </template>
    <div class="Kasumi-typo">
        <?php GetPost::Content(); ?>
    </div>
</a-card>