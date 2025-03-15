<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php if (Get::Is('post')) { ?>
<script>
    const PostApp = {
        data() {
            return {
                Title : '<?php GetPost::Title(); ?>',
                Tag: '<?php GetPost::Tags(); ?>',
                Category: '<?php GetPost::Category(',', true); ?>',
                PostDate: '<?php GetPost::Date(); ?>',
                WordCount: '<?php GetPost::WordCount(); ?>',
                Author: '<?php GetAuthor::Name(); ?>',
                AuthorUrl: '<?php GetAuthor::Permalink(); ?>',
            }
        }
    }
    Vue.createApp(PostApp).mount('#PostPage');
</script>
<?php } ?>
<script>
    const FooterApp = {
        data() {
            return {
                year: new Date().getFullYear(),
                siteTitle: "<?php echo Get::Options('title'); ?>",
                siteUrl: "<?php echo Get::SiteUrl(); ?>",
                icpCode: "<?php echo Get::Options('IcpCode'); ?>",
            };
        },
        template: `
            <a-card>
                <div :style="{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }">
                    <span :style="{ display: 'flex', alignItems: 'center' }">
                        <a-typography-text>© {{ year }} <a-link :href="siteUrl">{{ siteTitle }}</a-link> 版权所有</a-typography-text>
                    </span>
                    <template v-if="!icpCode">
                        <a-tooltip content="由Kasumi主题强力驱动">
                            <a-link href="https://github.com/ShuShuicu/Typecho-Kasumi-Theme" target="_blank">Kasumi</a-link>
                        </a-tooltip>
                    </template>
                    <template v-else>
                        <a-link href="https://beian.miit.gov.cn/" target="_blank">{{ icpCode }}</a-link>
                    </template>
                </div>
            </a-card>
        `,
    };

    Vue.createApp(FooterApp).mount('#footer');
</script>