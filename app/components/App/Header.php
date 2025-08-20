<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div>
    <div class="navbar bg-base-100 rounded-t-box">
        <div class="navbar-start">
            <a class="btn btn-ghost text-xl font-bold" href="<?php Get::SiteUrl(true) ?>">
                <?php Get::SiteName(true) ?>
            </a>
        </div>
        <div class="navbar-center hidden lg:flex">
            <ul class="menu menu-horizontal px-1 gap-1">
                <?php Get::Components('App/Navbar') ?>
            </ul>
        </div>
        <div class="navbar-end gap-1">
            <div class="dropdown dropdown-end">
                <button class="btn btn-ghost btn-circle" >
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                        stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                            d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
                    </svg>
                </button>
            </div>
            <button class="btn btn-ghost btn-circle" id="theme-toggle">
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                    stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                        d="M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M16 12a4 4 0 11-8 0 4 4 0 018 0z" />
                </svg>
            </button>
            <div class="dropdown dropdown-end lg:hidden">
                <div tabindex="0" role="button" class="btn btn-ghost btn-circle">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5" fill="none" viewBox="0 0 24 24"
                        stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                            d="M4 6h16M4 12h16M4 18h16" />
                    </svg>
                </div>
                <ul tabindex="0"
                    class="menu menu-sm dropdown-content mt-2 z-[1] p-2 shadow bg-base-100 rounded-box w-52">
                    <?php Get::Components('App/Navbar') ?>
                </ul>
            </div>
        </div>
    </div>
    <dialog id="my_modal_3" class="modal">
        <div class="modal-box">
            <h3 class="font-bold text-lg">搜索内容</h3>
            <div class="py-4">
                <input type="text" placeholder="输入关键词..." class="input input-bordered w-full" />
            </div>
            <div class="modal-action">
                <form method="dialog">
                    <button class="btn btn-sm btn-circle btn-ghost absolute right-2 top-2">✕</button>
                    <button class="btn">关闭</button>
                    <button class="btn btn-primary ml-2">搜索</button>
                </form>
            </div>
        </div>
    </dialog>
</div>
