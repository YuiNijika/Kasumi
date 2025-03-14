<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="mdui-drawer mdui-card" id="drawer">
    <ul class="mdui-list">
        <?php Kasumi::Components('AppSidebarNav') ?>
        <div class="mdui-divider"></div>
        <div class="mdui-collapse" mdui-collapse>
            <div class="mdui-collapse-item">
                <div class="mdui-collapse-item-header mdui-list-item mdui-ripple" role="button" aria-expanded="false">
                    <i class="mdui-list-item-icon mdui-icon material-icons" aria-hidden="true">language</i>
                    <div class="mdui-list-item-content">页面翻译</div>
                    <i class="mdui-collapse-item-arrow mdui-icon material-icons" aria-hidden="true">keyboard_arrow_down</i>
                </div>
                <div class="ignore-translate mdui-collapse-item-body mdui-list" role="menu">
                    <?php Kasumi::Components('AppSidebarTranslate') ?>
                </div>
            </div>
        </div>
    </ul>
</div>