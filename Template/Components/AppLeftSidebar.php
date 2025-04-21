<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="mdui-drawer" id="drawer">

    <a-menu
        :style="{ height: '100%' }"
        mode="pop"
        showCollapseButton>
        <ul class="mdui-list">
            <?php
            $SidebarNav = Get::Options('SidebarNav');
            if (empty($SidebarNav)) {
                $SidebarNav = '首页|home|' . Get::SiteUrl(false);
            }
            $MenuItems = explode("\n", $SidebarNav);

            foreach ($MenuItems as $item) {
                $parts = explode('|', trim($item));
                if (count($parts) == 3) {
                    list($name, $icon, $link) = $parts;
            ?>
                    <a href="<?php echo htmlspecialchars($link); ?>">
                        <li class="mdui-list-item mdui-ripple">
                            <i class="mdui-list-item-icon mdui-icon material-icons"><?php echo htmlspecialchars($icon); ?></i>
                            <div class="mdui-list-item-content"><?php echo htmlspecialchars($name); ?></div>
                        </li>
                    </a>
            <?php
                }
            }
            ?>
            <li class="mdui-subheader">共有 <?php GetPost::PostsNum() ?> 篇文章</li>
        </ul>
        <?php kASUMI::Components('TranslateMenu') ?>
    </a-menu>

</div>