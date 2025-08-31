<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
while (GetPost::List()) {
    IndexCard();
}
function IndexCard() { 
?>
<article class="card bg-base-100 shadow-xl article-card mb-4">
    <figure>
        <img src="https://i0.wp.com/i2.hdslb.com/bfs/archive/218fd054f9d6514c671c48fb81621d72fc5a3ae9.jpg@672w_378h_1c_!web-home-common-cover.webp"
            alt="" class="w-full object-cover" />
    </figure>
    <div class="card-body">
        <?php GetPost::Category()?>
            <h3 class="card-title py-1">
                <a href="<?php GetPost::Permalink() ?>" class="truncate"><?php GetPost::Title(); ?></a>
            </h3>

        <div class="card-actions justify-between">
            <div class="flex items-center gap-2">
                <div class="avatar">
                    <div class="w-8 rounded-full">
                        <img src="<?php GetUser::AvatarURL(512) ?>" />
                    </div>
                </div>
                <span class="text-sm opacity-70"><?php GetPost::Author() ?> · <?php GetPost::Date() ?></span>
            </div>
        </div>
    </div>
</article>
<?php } ?>