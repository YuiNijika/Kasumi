<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php while (GetPost::List()) { ?>
    <article class="card bg-base-100 image-full shadow-xl article-card h-42">
        <?php
        $Fields = Get::Fields('Thumbnail_Url') ?? null;
        $thumbnailUrl = $Fields ?: Kasumi::get_ArticleThumbnail($this);
        ?>
        <figure>
            <img src="<?php echo $thumbnailUrl ?>" alt="<?php GetPost::Title(true) ?>" class="w-full object-cover" />
        </figure>
        <div class="card-body">
            <?php GetPost::Category(true) ?>

            <h3 class="card-title py-3">
                <a href="<?php GetPost::Permalink(true) ?>">
                    <?php GetPost::Title(true) ?>
                </a>
            </h3>

            <div class="card-actions justify-between">
                <div class="flex items-center gap-2">
                    <div class="avatar">
                        <div class="w-8 rounded-full">
                            <img src="<?php GetUser::AvatarURL(512, true) ?>" />
                        </div>
                    </div>
                    <span class="text-sm opacity-70"><?php GetPost::Author(true) ?> · <?php GetPost::Date() ?></span>
                </div>
            </div>
        </div>
    </article>
<?php } ?>