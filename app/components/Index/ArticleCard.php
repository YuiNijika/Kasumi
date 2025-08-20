<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php while (GetPost::List()) { ?>
    <article class="card bg-base-100 shadow-md article-card">
        <?php if (Get::Fields('Thumbnail_Enabled')) { 
            $Fields = Get::Fields('Thumbnail_Url') ?? null;
            $thumbnailUrl = $Fields ?: Kasumi::get_ArticleThumbnail($this);
        ?>
            <figure>
                <img src="<?php echo $thumbnailUrl ?>" alt="<?php GetPost::Title(true) ?>" class="w-full h-64 object-cover" />
            </figure>
        <?php } ?>
        <div class="card-body">
            <h2 class="card-title text-2xl hover:text-primary">
                <a href="<?php GetPost::Permalink(true) ?>"><?php GetPost::Title(true) ?></a>
            </h2>
            <div class="flex gap-2 flex-wrap">
                <div class="badge badge-primary">鬼灭之刃</div>
                <div class="badge badge-secondary">新番</div>
                <div class="badge badge-accent">UFO社</div>
            </div>
            <p><?php GetPost::Excerpt('150', true) ?>...</p>
            <div class="card-actions justify-between mt-4">
                <div class="flex items-center gap-2">
                    <div class="avatar">
                        <div class="w-8 rounded-full">
                            <img src="<?php GetUser::AvatarURL(512, true) ?>" />
                        </div>
                    </div>
                    <span class="text-sm opacity-70"><?php GetUser::Name(true) ?> · <?php GetPost::Date() ?></span>
                </div>
                <a href="<?php GetPost::Permalink(true) ?>" class="btn-anime">阅读更多</a>
            </div>
        </div>
    </article>
<?php } ?>