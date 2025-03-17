<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<?php
if (Get::Is('post')) {
    // 获取缩略图 URL
    $thumbnailStyle = Get::Fields('thumbnailStyle') ?? null;
    $thumbnailUrl = $thumbnailStyle ?: get_ArticleThumbnail($this);

    // 确保 $thumbnailUrl 不为空
    if (!$thumbnailUrl) {
        $thumbnailUrl = ''; // 如果缩略图 URL 为空，设置为空字符串
    }
?>
    <script>
        const PostApp = Vue.createApp({
            data() {
                return {
                    Title: '<?php echo htmlspecialchars(GetPost::Title(), ENT_QUOTES, 'UTF-8'); ?>',
                    Tag: '<?php echo htmlspecialchars(GetPost::Tags(), ENT_QUOTES, 'UTF-8'); ?>',
                    Category: '<?php echo htmlspecialchars(GetPost::Category(',', true), ENT_QUOTES, 'UTF-8'); ?>',
                    PostDate: '<?php echo htmlspecialchars(GetPost::Date(), ENT_QUOTES, 'UTF-8'); ?>',
                    PostUrl: '<?php echo htmlspecialchars(GetPost::Permalink(), ENT_QUOTES, 'UTF-8'); ?>',
                    Thumbnail: '<?php echo htmlspecialchars($thumbnailUrl); ?>',
                    WordCount: '<?php echo htmlspecialchars(GetPost::WordCount(), ENT_QUOTES, 'UTF-8'); ?>',
                    Author: '<?php echo htmlspecialchars(GetAuthor::Name(), ENT_QUOTES, 'UTF-8'); ?>',
                    AuthorUrl: '<?php echo htmlspecialchars(GetAuthor::Permalink(), ENT_QUOTES, 'UTF-8'); ?>',
                    PostBreadcrumb: `
                        <a-breadcrumb>
                            <a-breadcrumb-item><a href="<?php Get::SiteUrl(); ?>">首页</a></a-breadcrumb-item>
                            <a-breadcrumb-item><?php echo htmlspecialchars(GetPost::Category(' & ', true), ENT_QUOTES, 'UTF-8'); ?></a-breadcrumb-item>
                            <a-breadcrumb-item>文章详情</a-breadcrumb-item>
                        </a-breadcrumb>
                    `,
                    PostsCopyright: `
                        <div><span>©</span> 版权声明</div>
                        <div>分享是一种美德，转载请保留原链接</div>
                    `,
                };
            },
            computed: {
                SubTitle() {
                    return `
                        作者：<a :href="this.AuthorUrl">${this.Author}</a>丨${this.PostDate}丨共 ${this.WordCount} 字
                    `;
                },
                PostShare() {
                    return `
                        <div style="text-align: center;">
                            文章不错？分享下呗
                        </div>
                        <div style="text-align: center;">
                            <a-space>
                                <a href="http://connect.qq.com/widget/shareqq/index.html?url=${encodeURIComponent(this.PostUrl)}&sharesource=qzone&title=${encodeURIComponent(this.Title)}&pics=${encodeURIComponent(this.Thumbnail)}&summary=${encodeURIComponent(this.PostExcerpt)}&desc=${encodeURIComponent(this.PostExcerptShort)}" target="_blank" class="mdui-btn mdui-btn-icon" title="分享至QQ好友">
                                    <img class="mdui-icon material-icons" src="<?php GetTheme::AssetsUrl(); ?>/images/icon/qq.svg" />
                                </a>
                                <a href="https://sns.qzone.qq.com/cgi-bin/qzshare/cgi_qzshare_onekey?url=${encodeURIComponent(this.PostUrl)}&sharesource=qzone&title=${encodeURIComponent(this.Title)}&pics=${encodeURIComponent(this.Thumbnail)}&summary=${encodeURIComponent(this.PostExcerpt)}" target="_blank" class="mdui-btn mdui-btn-icon" title="分享至QQ好友">
                                    <img class="mdui-icon material-icons" src="<?php GetTheme::AssetsUrl(); ?>/images/icon/QQZone.svg" />
                                </a>
                                <span class="mdui-btn mdui-btn-icon" id="CopyPostUrl" title="复制文章链接">
                                    <i class="mdui-icon material-icons">insert_link</i>
                                </span>
                            </a-space>
                        </div>
                    `;
                },
            },
        });
        PostApp.mount('#PostContent');
    </script>
<?php } ?>

<?php if (Get::Options('AlertSwitch') === 'open') { ?>
<script>
    const Alert = Vue.createApp({
        data() {
            return {
                alertSwitch: "<?php echo htmlspecialchars(Get::Options('AlertSwitch', false), ENT_QUOTES, 'UTF-8'); ?>",
                alertMode: "<?php echo htmlspecialchars(Get::Options('AlertMode', false), ENT_QUOTES, 'UTF-8'); ?>",
                alertContent: "<?php echo htmlspecialchars(Get::Options('AlertContent', false), ENT_QUOTES, 'UTF-8'); ?>",
            };
        },
        template: `
            <a-alert v-if="alertSwitch === 'open'" :type="alertMode" banner closable style="margin-bottom: 10px;">{{ alertContent }}</a-alert>
        `,
    });
    Alert.mount('#Alert');
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
                    <form method="post" :action="siteUrl" role="search">
                        <a-input class="mdui-hidden-xs" :style="{width:'320px'}" placeholder="随便搜搜吧？" type="search" name="s" allow-clear />
                    </form>
                    <a href="javascript:;" class="mdui-btn mdui-btn-icon" mdui-dialog="{target: '#search-dialog'}">
                        <i class="mdui-icon material-icons">search</i>
                    </a>
                </div>
            </div>
            <div class="mdui-dialog" id="search-dialog" role="dialog" aria-labelledby="search-dialog-title">
                <div class="mdui-dialog-title" id="search-dialog-title">搜索文章 · <small>精彩近在咫尺！</small></div>
                <form method="post" :action="siteUrl" role="search">
                <div style="margin: -10px 20px 0px 20px;">
                    <div class="mdui-textfield">
                        <i class="mdui-icon material-icons" aria-hidden="true">search</i>
                        <input class="mdui-textfield-input" type="search" name="s" placeholder="输入关键词后按回车(Enter)..." aria-label="搜索文章" required>
                    </div>
                </div>
                <div class="mdui-dialog-actions">
                    <button class="mdui-btn mdui-ripple" mdui-dialog-cancel>关闭</button>
                    <button class="mdui-btn mdui-ripple" type="submit">搜索</button>
                </div>
                </form>
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