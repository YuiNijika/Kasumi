<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>

<div id="con">
    <a href="#modal-one">
        <div id="TA-con" id="BPayTomori">
            <div id="text-con">
                <div id="linght"></div>
                <a href="#modal-one">
                    <div id="TA">为TA充电</div>
                </a>
            </div>
        </div>
    </a>

    <div id="tube-con">
        <svg viewBox="0 0 1028 385" fill="none" xmlns="http://www.w3.org/2000/svg">
            <path d="M1 77H234.226L307.006 24H790" stroke="#e5e9ef" stroke-width="20" />
            <path d="M0 140H233.035L329.72 71H1028" stroke="#e5e9ef" stroke-width="20" />
            <path d="M1 255H234.226L307.006 307H790" stroke="#e5e9ef" stroke-width="20" />
            <path d="M0 305H233.035L329.72 375H1028" stroke="#e5e9ef" stroke-width="20" />
            <rect y="186" width="236" height="24" fill="#e5e9ef" />
            <ellipse cx="790" cy="25.5" rx="25" ry="25.5" fill="#e5e9ef" />
            <circle r="14" transform="matrix(1 0 0 -1 790 25)" fill="white" />
            <ellipse cx="790" cy="307.5" rx="25" ry="25.5" fill="#e5e9ef" />
            <circle r="14" transform="matrix(1 0 0 -1 790 308)" fill="white" />
        </svg>
        <div id="mask">
            <svg viewBox="0 0 1028 385" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M1 77H234.226L307.006 24H790" stroke="#f25d8e" stroke-width="20" />
                <path d="M0 140H233.035L329.72 71H1028" stroke="#f25d8e" stroke-width="20" />
                <path d="M1 255H234.226L307.006 307H790" stroke="#f25d8e" stroke-width="20" />
                <path d="M0 305H233.035L329.72 375H1028" stroke="#f25d8e" stroke-width="20" />
                <rect y="186" width="236" height="24" fill="#f25d8e" />
                <ellipse cx="790" cy="25.5" rx="25" ry="25.5" fill="#f25d8e" />
                <circle r="14" transform="matrix(1 0 0 -1 790 25)" fill="white" />
                <ellipse cx="790" cy="307.5" rx="25" ry="25.5" fill="#f25d8e" />
                <circle r="14" transform="matrix(1 0 0 -1 790 308)" fill="white" />
            </svg>
        </div>
        <div id="orange-mask">
            <svg viewBox="0 0 1028 385" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M1 77H234.226L307.006 24H790" stroke="#ffd52b" stroke-width="20" />
                <path d="M0 140H233.035L329.72 71H1028" stroke="#ffd52b" stroke-width="20" />
                <path d="M1 255H234.226L307.006 307H790" stroke="#ffd52b" stroke-width="20" />
                <path d="M0 305H233.035L329.72 375H1028" stroke="#ffd52b" stroke-width="20" />
                <rect y="186" width="236" height="24" fill="#ffd52b" />
                <ellipse cx="790" cy="25.5" rx="25" ry="25.5" fill="#ffd52b" />
                <circle r="14" transform="matrix(1 0 0 -1 790 25)" fill="white" />
                <ellipse cx="790" cy="307.5" rx="25" ry="25.5" fill="#ffd52b" />
                <circle r="14" transform="matrix(1 0 0 -1 790 308)" fill="white" />
            </svg>
        </div>
        <div class="modal" id="modal-one">
            <div class="modal-dialog">
                <div class="modal-header">
                    <h2>为 <?php echo $this->user->screenName; ?> 的创作充电</h2>
                    <a href="#" class="modal-close">&times;</a>
                </div>
                <div class="modal-body" style="text-align: center;">
                    <?php if (!empty(Get::Options('PayQrcode'))){ ?>
                    <img src="<?php echo Get::Options('PayQrcode') ?>" />
                    <?php } else {
                        echo '<p>感谢支持！目前还不需要充电~</p>';
                    } ?>
                </div>
                <div class="modal-footer">
                    <a href="#">
                        <button class="cancel-btn">取消</button>
                    </a>
                </div>
            </div>
        </div>
        <p id="people"><b>感谢支持</b></p>
    </div>
</div>