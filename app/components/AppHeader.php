<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<!DOCTYPE html>
<html lang="zh-CN">

<head>
    <?php
    TTDF_Hook::do_action('load_head');
    if (TTDF_CONFIG['VITE'] ?? false) {
    ?>
        <script type="module" src="http://localhost:3000/@vite/client"></script>
        <script type="module" src="http://localhost:3000/src/main.ts"></script>
    <?php } else { ?>
        <link rel="stylesheet" href="<?php get_theme_file_url('assets/main.css?ver=' . get_theme_version(false)); ?>">
        <link rel="stylesheet" href="<?php get_theme_file_url('assets/daisyui.css?ver=' . get_theme_version(false)); ?>">
        <link rel="stylesheet" href="<?php get_theme_file_url('assets/themes.css?ver=' . get_theme_version(false)); ?>">
        <link rel="stylesheet" href="<?php get_theme_file_url('assets/dist/components.css?ver=' . get_theme_version(false)); ?>">
    <?php } ?>
</head>

<body>
    <div class="container" id="app">
        <div class="card bg-base-100 shadow-xl">
        <header>
            <?php Get::Components('App/Header'); ?>
        </header>
        <main class="card-body">