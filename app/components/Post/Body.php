<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
    <div class="lg:col-span-3 space-y-6">
        <?php Get::Components('Post/ArticleCard') ?>
    </div>
    <div class="sidebar-container">
        <?php Get::Components('Post/Sidebar') ?>
    </div>
</div>