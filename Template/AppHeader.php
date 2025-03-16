<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<!doctype html>
<html lang="<?php echo Get::Options('lang', true) ? Get::Options('lang', true) : 'zh-CN' ?>">

<head>
    <?php TTDF::HeadMeta() ?>
    <?php TTDF::HeadMetaOG() ?>
    <link href="<?php echo Get::Options('FaviconUrl', false) ? Get::Options('FaviconUrl', false) : Get::SiteUrl(false) . 'favicon.ico'; ?>" rel="icon" />
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/main.css?ver=<?php GetTheme::Ver(true); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/mdui/css/mdui.min.css?ver=<?php GetTheme::Ver(); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/arco/arco.min.css?ver=<?php GetTheme::Ver(); ?>">
    <script src="<?php GetTheme::AssetsUrl() ?>/vue.global.js?ver=<?php GetTheme::Ver(); ?>"></script>
</head>

<body class="mdui-drawer-body-left mdui-appbar-with-toolbar mdui-theme-primary-indigo mdui-theme-accent-pink">
    <main id="app">
        <header>
            <div id="HeaderAppbar"></div>
        </header>
        <nav>
            <?php Kasumi::Components('AppLeftSidebar'); ?>
        </nav>
        <div class="Kasumi">