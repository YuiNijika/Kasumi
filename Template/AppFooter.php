<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>      
        </div>
        <footer>
            <?php Kasumi::Components('AppFooterCard'); ?>
        </footer>
    </main>
    <script src="<?php GetTheme::AssetsUrl() ?>/main.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/mdui/js/mdui.min.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/arco/arco-vue.min.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script type="text/javascript">
        console.log('加载时间<?php GetFunctions::TimerStop(); ?>')
    </script>
</body>

</html>