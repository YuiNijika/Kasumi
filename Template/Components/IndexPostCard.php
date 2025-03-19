<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
while (GetPost::List()) {
    // 获取缩略图 URL
    $thumbnailStyle = Get::Fields('thumbnailStyle') ?? null;
    $thumbnailUrl = $thumbnailStyle ?: get_ArticleThumbnail($this);

    // 确保 $thumbnailUrl 不为空
    if (!$thumbnailUrl) {
        $thumbnailUrl = ''; // 如果缩略图 URL 为空，设置为空字符串
    }
?>

    <a-col :xs="24" :sm="24" :md="12" :lg="12" :xl="6" :xxl="6">
        <article>
            <a href="<?php
                        if (Get::Fields('PostStyleGoUrl') === 'open') {
                            echo Get::Fields('goUrlStyle') ?? GetPost::Permalink();
                        } else {
                            echo GetPost::Permalink();
                        }
                        ?>">
                <a-card
                    role="article"
                    aria-label="文章卡片"
                    hoverable
                    :bordered="false"
                    class="KasumiIndexPostCard">
                    <template #cover>
                        <?php if ($thumbnailUrl) { ?>
                            <div class="thumbnail" style="
                        background-image: url(<?php echo htmlspecialchars($thumbnailUrl); ?>);
                    "></div>
                        <?php } ?>
                    </template>
                    <a-card-meta title="<?php GetPost::Title() ?>">
                        <template #description>
                            <?php GetPost::Category() ?> · <?php GetPost::Date() ?>
                        </template>
                    </a-card-meta>
                </a-card>
            </a>
        </article>
    </a-col>
<?php } ?>