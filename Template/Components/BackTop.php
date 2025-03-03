<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div class="wrapper">
    <ul id="basic-demo">
        <li v-for="(_, index) of Array(40)" :key="index">This is the content</li>
    </ul>
    <a-back-top target-container="#basic-demo" :style="{position:'absolute'}" />
</div>

<style scoped lang="less">
    .wrapper {
        position: relative;

        ul {
            height: 200px;
            overflow-y: auto;

            li {
                line-height: 30px;
            }
        }
    }
</style>