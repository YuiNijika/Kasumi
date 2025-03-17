<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div id="Index">
    <?php 
        // 首页轮播图开关开启
        if (Get::Options('CarouselSwitch') === 'open') {
            if (Get::Is('index')) {
                Kasumi::Components('IndexCarousel');
            }
            // 分类标签轮播图开关开启
            elseif (Get::Is('category') && Get::Options('CarouselCategory') === 'open') {
                Kasumi::Components('IndexCarousel');
            } elseif (Get::Is('tag') && Get::Options('CarouselCategory') === 'open') {
                Kasumi::Components('IndexCarousel');
            } 
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