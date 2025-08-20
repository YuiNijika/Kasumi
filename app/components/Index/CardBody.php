<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div>
    <div class="grid grid-cols-1 lg:grid-cols-4 gap-6">

        <div class="lg:col-span-3 space-y-6">
            <?php Get::Components('Index/ArticleCard'); ?>
            <div class="join grid grid-cols-2 max-w-xs mx-auto">
                <?php Get::PageLink('<button class="join-item btn btn-outline">上一页</button>'); ?>
                <?php Get::PageLink('<button class="join-item btn btn-outline">下一页</button>', 'next'); ?>
            </div>
        </div>

        <div class="sidebar-container">
            <div class="sticky-sidebar space-y-6">
                <?php Get::Components('Index/Sidebar'); ?>
            </div>
        </div>
    </div>
</div>
