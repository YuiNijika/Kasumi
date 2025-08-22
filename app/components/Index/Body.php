<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="grid grid-cols-1 lg:grid-cols-4 md:grid-cols-2 gap-8">
    <?php Get::Components('Index/ArticleCard'); ?>
</div>
<div class="flex justify-center gap-2 pt-4">
    <?php Get::PageLink('<button class="btn btn-soft btn-accent">上一页</button>'); ?>
    <?php Get::PageLink('<button class="btn btn-soft btn-info">下一页</button>', 'next'); ?>
</div>
