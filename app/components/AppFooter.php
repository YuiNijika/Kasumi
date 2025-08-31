<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
class Kasumi_Footer
{
    public static function cid()
    {
        if (Get::Is('single')) {
            GetPost::Cid(true);
        } else {
            echo '0';
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

    public static function getUserUid()
    {
        if (GetUser::Login(false)) {
            return GetUser::Uid(false);
        }
        return '0';
    }

    public static function SubTitle()
    {
        if (Get::Is('single') || Get::Is('category') || Get::Is('tag')) {
            GetPost::Title(true);
        } elseif (Get::Is('search')) {
            echo '搜索结果';
        } else {
            echo Get::Options('SubTitle', false) ?: '首页';
        }
    }
}
?>
</div>
</main>
</div>
<script src="<?php get_theme_file_url('assets/main.js?ver=' . get_theme_version(false)); ?>"></script>
<script src="<?php get_theme_file_url('assets/browser.js?ver=' . get_theme_version(false)); ?>"></script>
<script src="<?php get_theme_file_url('assets/motyf.js?ver=' . get_theme_version(false)); ?>"></script>
<script src="<?php get_theme_file_url('assets/nprogress.min.js?ver=' . get_theme_version(false)); ?>"></script>
<?php
TTDF_Hook::do_action('load_foot');
if (!(TTDF_CONFIG['VITE'] ?? false)) {
?>
<script type="module" src="<?php get_theme_file_url('assets/dist/components.js?ver=' . get_theme_version(false)); ?>"></script>
<?php } ?>
<script>
    window.Kasumi = {
        'site': {
            'title': '<?php Get::SiteName(true) ?>',
            'subTitle': '<?php Kasumi_Footer::SubTitle() ?>',
            'url': '<?php Get::SiteUrl(true) ?>',
        },
        'page': {
            'url': '<?php Get::PageUrl(true) ?>',
            'cid': '<?php Kasumi_Footer::cid() ?>',
        },
        'user': {
            'status': '<?php GetUser::Login(true) ?>'
        }
    }
</script>
</body>

</html>