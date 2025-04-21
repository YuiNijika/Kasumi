<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>      
        </div>
        <footer>
            <div id="FooterCard"></div>
        </footer>
    </main>
    <script src="<?php GetTheme::AssetsUrl() ?>/main.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/script.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/mdui/js/mdui.min.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/arco/arco-vue.min.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <?php TTDF_Hook::do_action('load_foot'); ?>
</body>

</html>