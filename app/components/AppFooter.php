<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
</main>
<footer>
    <?php Get::Components('App/Footer'); ?>
</footer>
</div>
</div>
<?php
TTDF_Hook::do_action('load_foot');
if (!(TTDF_CONFIG['VITE'] ?? false)) {
?>
    <script src="<?php get_theme_file_url('assets/main.js?ver=' . get_theme_version(false)); ?>"></script>
    <script src="<?php get_theme_file_url('assets/browser.js?ver=' . get_theme_version(false)); ?>"></script>
    <script type="module" src="<?php get_theme_file_url('assets/dist/components.js?ver=' . get_theme_version(false)); ?>"></script>
<?php } ?>
<script>
    var pageData = {
        'title': '<?php TTDF_SEO_Title(); ?>',
        'keywords': '<?php TTDF_SEO_Keywords(); ?>',
        'description': '<?php TTDF_SEO_Description(); ?>',
        'url': '<?php Get::PageUrl(true) ?>',
        'cid': '<?php if (Get::Is('single')) {
                    GetPost::Cid(true);
                } else {
                    echo 'null';
                } ?>'
    }
</script>
</body>

</html>