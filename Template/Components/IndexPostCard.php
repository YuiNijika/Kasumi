<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-col :xs="24" :sm="24" :md="12" :lg="12" :xl="6" :xxl="6">
    <a href="<?php GetPost::Permalink(); ?>">
        <a-card
            hoverable
            :bordered="false"
            class="KasumiIndexPostCard"
        >
            <template #cover>
                <?php
                // 获取缩略图URL
                $thumbnailUrl = get_ArticleThumbnail($this);
                if ($thumbnailUrl) {
                    // 如果缩略图URL存在，则输出缩略图  
                ?>
                    <div class="thumbnail" style="
                        background-image: url(<?php echo htmlspecialchars($thumbnailUrl); ?>);
                        border-radius: 4px 4px 0px 0px;
                    "></div>
                <?php } ?>
            </template>
            <a-card-meta title="<?php $this->title(); ?>">
                <template #description>
                <?php GetPost::Category() ?> · <?php GetPost::Date() ?>
                </template>
            </a-card-meta>
        </a-card>
    </a>
</a-col>