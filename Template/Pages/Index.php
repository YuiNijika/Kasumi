<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div id="Index">
    <a-row>
        <?php
        while ($this->next()) {
            Kasumi::Components('IndexPostCard');
        };
        ?>
    </a-row>
</div>