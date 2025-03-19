<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div id="PostContent">
    <article>
        <div v-html="PostBreadcrumb" style="margin-bottom: 10px;"></div>
            <?php Kasumi::Components('PostContent') ?>
        <?php Kasumi::Components('PostButton'); ?>
    </article>
</div>