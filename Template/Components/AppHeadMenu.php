<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="mdui-appbar mdui-appbar-fixed">
    <div class="mdui-toolbar mdui-color-theme">
        <a href="javascript:;" class="mdui-btn mdui-btn-icon" mdui-drawer="{target: '#drawer'}">
            <i class="mdui-icon material-icons">menu</i>
        </a>
        <a href="<?php Get::SiteUrl() ?>" class="mdui-typo-headline mdui-hidden-xs"><?php Get::SiteName(); ?></a>
        <a class="mdui-typo-title">
            <?php
            if (Get::Is("index")) {
                echo Get::Options("SubTitle") ? Get::Options("SubTitle") : '首页';
            }
            $archiveTitle = GetPost::ArchiveTitle(
                [
                    "category" => _t("%s"),
                    "search" => _t("%s"),
                    "tag" => _t("%s"),
                    "author" => _t("%s"),
                ],
                "",
            );
            ?>
        </a>
        <div class="mdui-toolbar-spacer"></div>
        <a href="javascript:;" class="mdui-btn mdui-btn-icon">
            <i class="mdui-icon material-icons">search</i>
        </a>
        <a href="javascript:;" class="mdui-btn mdui-btn-icon">
            <i class="mdui-icon material-icons">refresh</i>
        </a>
        <a href="javascript:;" class="mdui-btn mdui-btn-icon">
            <i class="mdui-icon material-icons">more_vert</i>
        </a>
    </div>
</div>