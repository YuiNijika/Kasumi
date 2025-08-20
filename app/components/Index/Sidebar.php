<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div>
    <div class="card bg-base-100 shadow-md">
        <div class="card-body">
            <h2 class="card-title">关于本站</h2>
            <div class="avatar flex justify-center">
                <div class="w-24 rounded-full">
                    <img src="https://i.imgur.com/7Qq7QyK.png" />
                </div>
            </div>
            <p class="text-center">这里是动漫爱好者的聚集地，分享最新动漫资讯、深度解析和观后感。</p>
            <div class="card-actions justify-center">
                <a href="#" class="btn-anime">了解更多</a>
            </div>
        </div>
    </div>

    <div class="card bg-base-100 shadow-md">
        <div class="card-body">
            <h2 class="card-title">本周新番</h2>
            <div class="space-y-3">
            <?php 
                $this->widget('Widget_Metas_Category_List')->parse('
                    <a href="{permalink}" class="flex justify-between items-center hover:text-primary">
                        <span>{name}</span>
                        <span class="badge badge-primary">{count}</span>
                    </a>
                '); 
            ?>
            </div>
        </div>
    </div>
</div>
