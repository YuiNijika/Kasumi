<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<!doctype html>
<html lang="zh-CN">

<head>
    <meta charset="<?php Get::Options('charset', true) ?>">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, shrink-to-fit=no" />
    <meta name="renderer" content="webkit" />
    <link href="<?php echo Get::Options('FaviconUrl') ? Get::Options('FaviconUrl',) : GetTheme::AssetsUrl() . "/images/Kasumi.svg"; ?>" rel="icon" />
    <?php TTDF::Functions('SEO') ?>
    <?php Get::Header(); ?>
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/main.css?ver=<?php GetTheme::Ver(); ?>">
    <link rel="stylesheet" href="<?php GetTheme::AssetsUrl() ?>/arco/arco.min.css?ver=<?php GetTheme::Ver(); ?>">
    <script src="<?php GetTheme::AssetsUrl() ?>/vue.global.js?ver=<?php GetTheme::Ver(); ?>"></script>
</head>
<body>
    <main id="app">
    <?php Kasumi::Components('AppHeadMenu'); ?>
    <div class="Kasumi">
        