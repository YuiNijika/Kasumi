<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<!doctype html>
<html lang="zh-CN">

<head>
    <?php TTDF::HeadMeta() ?>
    <link href="<?php echo Get::Options('FaviconUrl', false) ? Get::Options('FaviconUrl', false) : Get::SiteUrl(false) . 'favicon.ico'; ?>" rel="icon" />
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/main.css?ver=<?php GetTheme::Ver(true); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/mdui/css/mdui.min.css?ver=<?php GetTheme::Ver(); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/arco/arco.min.css?ver=<?php GetTheme::Ver(); ?>">
    <script src="<?php GetTheme::AssetsUrl() ?>/vue.global.js?ver=<?php GetTheme::Ver(); ?>"></script>
</head>

<body class="mdui-drawer-body-left mdui-appbar-with-toolbar mdui-theme-primary-indigo mdui-theme-accent-pink">
    <main id="app">
        <nav>
            <?php Kasumi::Components('AppHeaderNav'); ?>
            <?php Kasumi::Components('AppLeftSidebar'); ?>
        </nav>
        <div class="Kasumi">