<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>

<div id="Index">
    <?php 
        if (Get::Options('CarouselSwitch') === 'open') {
            Kasumi::Components('IndexCarousel');
        }
    ?>
    <a-row>
        <?php
        while (Get::Next()) {
            Kasumi::Components('IndexPostCard');
        };
        ?>
    </a-row>
    <?php Kasumi::Components('IndexPageLink'); ?>
</div>