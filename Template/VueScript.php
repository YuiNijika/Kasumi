<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php if (Get::Is('post')) { ?>
    <script>
        const PostApp = Vue.createApp({
            data() {
                return {
                    Title: '<?php echo htmlspecialchars(GetPost::Title(), ENT_QUOTES, 'UTF-8'); ?>',
                    Tag: '<?php echo htmlspecialchars(GetPost::Tags(), ENT_QUOTES, 'UTF-8'); ?>',
                    Category: '<?php echo htmlspecialchars(GetPost::Category(',', true), ENT_QUOTES, 'UTF-8'); ?>',
                    PostDate: '<?php echo htmlspecialchars(GetPost::Date(), ENT_QUOTES, 'UTF-8'); ?>',
                    WordCount: '<?php echo htmlspecialchars(GetPost::WordCount(), ENT_QUOTES, 'UTF-8'); ?>',
                    Author: '<?php echo htmlspecialchars(GetAuthor::Name(), ENT_QUOTES, 'UTF-8'); ?>',
                    AuthorUrl: '<?php echo htmlspecialchars(GetAuthor::Permalink(), ENT_QUOTES, 'UTF-8'); ?>',
                };
            }
        });
        PostApp.mount('#PostContent');
    </script>
<?php } ?>
<script>
    const HeaderAppbar = Vue.createApp({
        data() {
            return {
                siteUrl: "<?php echo htmlspecialchars(Get::SiteUrl(false), ENT_QUOTES, 'UTF-8'); ?>",
                siteName: "<?php echo htmlspecialchars(Get::SiteName(false), ENT_QUOTES, 'UTF-8'); ?>",
                subTitle: "<?php echo htmlspecialchars(Get::Options("SubTitle", false) ? Get::Options("SubTitle") : '首页', ENT_QUOTES, 'UTF-8'); ?>",
                archiveTitle: "<?php echo htmlspecialchars(GetPost::ArchiveTitle([
                                    "category" => _t("%s"),
                                    "search" => _t("%s"),
                                    "tag" => _t("%s"),
                                    "author" => _t("%s"),
                                ], "", false), ENT_QUOTES, 'UTF-8'); ?>",
            };
        },
        template: `
            <div class="mdui-appbar mdui-appbar-fixed">
                <div class="mdui-toolbar mdui-color-theme">
                    <a href="javascript:;" class="mdui-btn mdui-btn-icon" mdui-drawer="{target: '#drawer'}">
                        <i class="mdui-icon material-icons">menu</i>
                    </a>
                    <a :href="siteUrl" class="ignore-translate mdui-typo-headline mdui-hidden-xs">{{ siteName }}</a>
                    <a class="mdui-typo-title">
                        {{ isIndexPage ? subTitle : archiveTitle }}
                    </a>
                    <div class="mdui-toolbar-spacer"></div>
                    <a href="javascript:;" class="mdui-btn mdui-btn-icon" mdui-dialog="{target: '#search-dialog'}">
                        <i class="mdui-icon material-icons">search</i>
                    </a>
                </div>
            </div>
            <div class="mdui-dialog" id="search-dialog" style="border-radius: 5px;" role="dialog" aria-labelledby="search-dialog-title">
                <div class="mdui-dialog-title" id="search-dialog-title">搜索文章 · <small>精彩近在咫尺！</small></div>
                <div class="mdui-dialog-content">
                    <form method="post" :action="siteUrl" role="search">
                        <div class="mdui-textfield">
                            <i class="mdui-icon material-icons" aria-hidden="true">search</i>
                            <input class="mdui-textfield-input" type="search" name="s" placeholder="输入关键词后按回车(Enter)..." aria-label="搜索文章" required>
                        </div>
                    </form>
                </div>
                <div class="mdui-dialog-actions">
                    <button class="mdui-btn mdui-ripple" mdui-dialog-cancel>关闭</button>
                </div>
            </div>
        `,
        computed: {
            isIndexPage() {
                return window.location.pathname === "/";
            }
        }
    });

    HeaderAppbar.mount('#HeaderAppbar');
</script>
<script>
    const FooterCard = Vue.createApp({
        data() {
            return {
                year: new Date().getFullYear(),
                siteTitle: "<?php echo htmlspecialchars(Get::Options('title', false), ENT_QUOTES, 'UTF-8'); ?>",
                siteUrl: "<?php echo htmlspecialchars(Get::SiteUrl(false), ENT_QUOTES, 'UTF-8'); ?>",
                icpCode: "<?php echo htmlspecialchars(Get::Options('IcpCode', false), ENT_QUOTES, 'UTF-8'); ?>",
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
    });
    FooterCard.mount('#FooterCard');
</script>