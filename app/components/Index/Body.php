<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-8">
    <?php Get::Components('Index/Card') ?>
</div>
<div class="join grid grid-cols-2 max-w-xs mx-auto pt-4">
    <button class="join-item btn btn-soft btn-accent">上一页</button>
    <button class="join-item btn btn-soft btn-info">下一页</button>
</div>