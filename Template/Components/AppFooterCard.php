<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-card>
    <div
        :style="{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
            }">
        <span
            :style="{ display: 'flex', alignItems: 'center', }">
            <a-typography-text>© <?php echo date("Y"); ?> <a-link href="<?php Get::SiteUrl() ?>"><?php echo Get::Options("title"); ?></a-link> 版权所有</a-typography-text>
        </span>
        <?php if (empty(Get::Options('IcpCode', true))) {
            echo '<a-link href="https://github.com/ShuShuicu/Typecho-Kasumi-Theme" target="_blank">Kasumi</a-link>';
        } else { ?>
            <a-link href="https://beian.miit.gov.cn/" target="_blank"><?php Get::Options('IcpCode', true) ?></a-link>
        <?php } ?>
    </div>
</a-card>