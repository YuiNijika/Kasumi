<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
    </div>
    <?php Kasumi::Components('AppFooterCard'); ?>
    </main>
    <script src="<?php GetTheme::AssetsUrl() ?>/arco/arco-vue.min.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script src="<?php GetTheme::AssetsUrl() ?>/main.js?ver=<?php GetTheme::Ver(); ?>"></script>
    <script>
        console.log('加载时间<?php GetFunctions::TimerStop(); ?>')
    </script>
</body>
<!-- 加载时间<?php GetFunctions::TimerStop(); ?> -->

</html>