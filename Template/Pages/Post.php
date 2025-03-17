<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div id="Post">
    <div id="PostContent">
        <article>
            <div v-html="PostBreadcrumb" style="margin-bottom: 10px;"></div>
            <a-watermark content="<?php Get::SiteName() ?>">
                <?php Kasumi::Components('PostContent') ?>
            </a-watermark>
            <?php Kasumi::Components('PostButton'); ?>
        </article>
    </div>
</div>