<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div id="Index">
    <?php
    // 轮播图
    if (Get::Options('CarouselSwitch') === 'open') {
        Kasumi::Components('IndexCarousel');
    }
    ?>
    <?php if (Get::Options('IndexStyle') === 'Card') { ?>
    <a-row id="IndexPostCard">
        <?php
        Kasumi::Components('IndexPostCard');
        ?>
    </a-row>
    <?php } elseif (Get::Options('IndexStyle') === 'List') { ?>
    <a-row id="IndexPostList">
        <a-col :xs="24" :sm="24" :md="24" :lg="16" :xl="18" :xxl="18">
            <?php
            Kasumi::Components('IndexPostList');
            ?>
        </a-col>
        <a-col :xs="24" :sm="24" :md="24" :lg="8" :xl="6" :xxl="6" style="padding: 5px 5px 0px 5px;">
        <div>
        <a-affix offset-top="80">
            <div id="AppRightSidebar">
            </div>
        </div>    
        </a-col>
    </a-row>
    <?php } ?>
    <?php Kasumi::Components('IndexPageLink'); ?>
</div>