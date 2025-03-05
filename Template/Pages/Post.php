<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<a-row>
    <a-col :xs="24" :sm="24" :md="24" :lg="18" :xl="18" :xxl="18" id="Post" style="margin-right: 10px;margin-bottom: 10px;">
        <?php Kasumi::Components('PostContent'); ?>
    </a-col>
    <a-col :xs="24" :sm="24" :md="24" :lg="5" :xl="5" :xxl="5">
        <?php Kasumi::Components('AppSidebar'); ?>
    </a-col>
</a-row>