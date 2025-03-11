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
            丨
            <?php if (empty(Get::Options('IcpCode'))) {
                echo '<a-link href="https://github.com/ShuShuicu/Typecho-Kasumi-Theme" target="_blank">Kasumi</a-link>';
            } else { ?>
                <a-link href="https://beian.miit.gov.cn/" target="_blank"><?php Get::Options('IcpCode', true) ?></a-link>
            <?php } ?>
        </span>
        <a-dropdown trigger="hover">
            <a-link>翻译</a-link>
            <template #content>
                <?php
                $languages = [
                    'chinese_simplified' => '简体中文',
                    'chinese_traditional' => '繁体中文',
                    'english' => 'English',
                    'japanese' => '日本語',
                    'korean' => '한국어'
                ];
                foreach ($languages as $code => $name) { ?>
                    <a-doption>
                        <a href="javascript:translate.changeLanguage('<?php echo $code; ?>');"><?php echo $name; ?></a>
                    </a-doption>
                <?php }; ?>
            </template>
        </a-dropdown>
    </div>
</a-card>