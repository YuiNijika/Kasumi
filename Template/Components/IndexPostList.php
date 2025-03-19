<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
while (GetPost::List()) {
?>
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
                title="<?php GetPost::Title() ?>"
                hoverable
                :bordered="false"
                class="KasumiIndexPostCard">
                <template #extra>
                    <a-link><?php GetPost::Date() ?></a-link>
                </template>
                <a-card-meta>
                    <template #description>
                        <?php GetPost::Excerpt('150') ?>
                    </template>
                </a-card-meta>
            </a-card>
        </a>
    </article>
<?php } ?>