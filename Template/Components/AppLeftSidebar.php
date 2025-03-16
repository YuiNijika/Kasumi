<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="mdui-drawer mdui-card" id="drawer">
    <ul class="mdui-list" mdui-collapse="{accordion: true}">
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
        <div class="mdui-divider"></div>
        <div class="mdui-collapse" mdui-collapse>
            <div class="mdui-collapse-item">
                <div class="mdui-collapse-item-header mdui-list-item mdui-ripple" role="button" aria-expanded="false">
                    <i class="mdui-list-item-icon mdui-icon material-icons" aria-hidden="true">language</i>
                    <div class="mdui-list-item-content">页面翻译</div>
                    <i class="mdui-collapse-item-arrow mdui-icon material-icons" aria-hidden="true">keyboard_arrow_down</i>
                </div>
                <div class="ignore-translate mdui-collapse-item-body mdui-list" role="menu">
                    <?php Kasumi::Components('TranslateMenu') ?>
                </div>
            </div>
        </div>
    </ul>
</div>