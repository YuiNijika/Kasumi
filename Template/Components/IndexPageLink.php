<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div style="text-align: center;margin-top: 10px;">
    <a-space>
        <?php $this->pageLink('<a-button status="warning">上一页</a-button>'); ?>
        <?php $this->pageLink('<a-button status="danger">下一页</a-button>', 'next'); ?>
    </a-space>
</div>