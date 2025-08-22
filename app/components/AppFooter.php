<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
class Kasumi_Footer
{
    public static function cid()
    {
        if (Get::Is('single')) {
            GetPost::Cid(true);
        } else {
            echo '2';
        }
    }
    public static function uid()
    {
        if (GetUser::Login(false)) {
            GetUser::Uid(true);
        } else {
            echo '0';
        }
    }
}
?>
</div>
</main>
<footer class="footer footer-horizontal footer-center text-base-content rounded p-5">
    <?php Get::Components('App/Footer'); ?>
</footer>
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
        'cid': '<?php Kasumi_Footer::cid() ?>'
        'uid': '<?php Kasumi_Footer::uid() ?>'
    }
</script>
</body>

</html>