<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<!doctype html>
<html lang="zh-CN">

<head>
    <?php TTDF::HeadMeta(); ?>
    <?php TTDF::Functions('SEO') ?>
    <link href="<?php echo Get::Options('FaviconUrl', false) ? Get::Options('FaviconUrl', false) : Get::SiteUrl(false) . 'favicon.ico'; ?>" rel="icon" />
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/main.css?ver=<?php GetTheme::Ver(); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/arco/arco.min.css?ver=<?php GetTheme::Ver(); ?>">
    <script src="<?php GetTheme::AssetsUrl() ?>/vue.global.js?ver=<?php GetTheme::Ver(); ?>"></script>
</head>
<body>
    <main id="app">
    <?php Kasumi::Components('AppHeadMenu'); ?>
    <div class="Kasumi">
        