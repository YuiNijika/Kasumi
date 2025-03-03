<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
Get::Template('AppHeader');
?>
<a-card>
    <a-result status="404">
        <template #subtitle>
            返回首页去寻找闪闪亮亮令人心动的东西罢！
        </template>
        <template #extra>
            <a-space>
                <a href="<?php Get::SiteUrl() ?>">
                    <a-button type="primary">返回首页</a-button>
                </a>
            </a-space>
        </template>
    </a-result>
</a-card>
<?php
Get::Template('AppFooter');
?>