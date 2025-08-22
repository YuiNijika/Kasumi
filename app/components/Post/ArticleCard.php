<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<article class="card bg-base-100 shadow-xl">

    <div class="card-body">
        <h2 class="card-title"><?php GetPost::Title(); ?></h2>
        <div class="typo px-3 py-3">
            <?php GetPost::Content(); ?>
        </div>
    </div>

</article>