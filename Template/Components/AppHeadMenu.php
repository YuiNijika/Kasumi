<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-menu mode="horizontal" theme="dark" :default-selected-keys="['1']">
    <?php
    $LogoUrl = Get::Options('LogoUrl');
    if (!empty($LogoUrl)) {
    ?>
        <a-menu-item key="0" :style="{ padding: 0, height: '30px', marginRight: '15px', }" disabled>
            <img src="<?php echo htmlspecialchars($LogoUrl); ?>" style="height: 30px; cursor: none;" />
        </a-menu-item>
    <?php
    }
    ?>
    <?php
    $HeadMenu = Get::Options('HeadMenu');
    if (empty($HeadMenu)) {
        $HeadMenu = '首页|' . Get::SiteUrl(false);
    }
    $MenuItems = explode("\n", $HeadMenu);

    foreach ($MenuItems as $item) {
        $parts = explode('|', trim($item));
        if (count($parts) == 2) {
            list($name, $link) = $parts;
    ?>
            <a href="<?php echo htmlspecialchars($link); ?>">
                <a-menu-item key="<?php echo htmlspecialchars($name); ?>"><?php echo htmlspecialchars($name); ?></a-menu-item>
            </a>
    <?php
        }
    }
    ?>
</a-menu>