<template>
    <aside class="w-full lg:w-1/3 space-y-6">
        <div class="sticky top-24 space-y-6">

            <!-- 关于博客 -->
            <div class="card border border-gray-200 dark:border-gray-700">
                <div class="card-body p-5">
                    <h3 class="text-lg font-bold mb-3">关于TechBlog</h3>
                    <p class="text-gray-600 dark:text-gray-400 text-sm">
                        TechBlog是一个专注于前端开发技术的博客，分享最新的技术趋势、实用教程和最佳实践，帮助开发者不断提升技能。</p>
                    <a href="#" class="inline-block mt-4 text-primary hover:underline text-sm font-medium">了解更多</a>
                </div>
            </div>

            <!-- 热门文章 -->
            <div class="space-y-4">
                <h3 class="text-lg font-bold">热门文章</h3>
                <div class="space-y-4">
                    <!-- 热门文章1 -->
                    <a href="#" class="flex gap-3 group">
                        <img src="https://picsum.photos/id/160/100/100" alt="文章缩略图"
                            class="w-20 h-20 rounded-lg object-cover flex-shrink-0">
                        <div>
                            <h4 class="font-medium text-sm group-hover:text-primary transition-colors line-clamp-2">
                                TypeScript高级类型技巧：提升代码质量的5个实用方法</h4>
                            <p class="text-gray-500 dark:text-gray-400 text-xs mt-1">2周前 · 6分钟阅读</p>
                        </div>
                    </a>

                    <!-- 热门文章2 -->
                    <a href="#" class="flex gap-3 group">
                        <img src="https://picsum.photos/id/201/100/100" alt="文章缩略图"
                            class="w-20 h-20 rounded-lg object-cover flex-shrink-0">
                        <div>
                            <h4 class="font-medium text-sm group-hover:text-primary transition-colors line-clamp-2">
                                Next.js 13新特性详解：App Router带来的变革</h4>
                            <p class="text-gray-500 dark:text-gray-400 text-xs mt-1">3周前 · 10分钟阅读</p>
                        </div>
                    </a>

                    <!-- 热门文章3 -->
                    <a href="#" class="flex gap-3 group">
                        <img src="https://picsum.photos/id/119/100/100" alt="文章缩略图"
                            class="w-20 h-20 rounded-lg object-cover flex-shrink-0">
                        <div>
                            <h4 class="font-medium text-sm group-hover:text-primary transition-colors line-clamp-2">
                                CSS Grid布局完全指南：从基础到高级技巧</h4>
                            <p class="text-gray-500 dark:text-gray-400 text-xs mt-1">1个月前 · 12分钟阅读</p>
                        </div>
                    </a>
                </div>
            </div>

            <!-- 分类 -->
            <div class="space-y-4">
                <h3 class="text-lg font-bold">文章分类</h3>
                <div class="grid grid-cols-2 gap-2">
                    <a href="#"
                        class="flex items-center justify-between px-3 py-2 rounded-lg border border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-750 transition-colors">
                        <span>前端开发</span>
                        <span class="text-xs px-2 py-0.5 bg-gray-100 dark:bg-gray-700 rounded-full">128</span>
                    </a>
                    <a href="#"
                        class="flex items-center justify-between px-3 py-2 rounded-lg border border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-750 transition-colors">
                        <span>JavaScript</span>
                        <span class="text-xs px-2 py-0.5 bg-gray-100 dark:bg-gray-700 rounded-full">95</span>
                    </a>
                    <a href="#"
                        class="flex items-center justify-between px-3 py-2 rounded-lg border border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-750 transition-colors">
                        <span>CSS/Tailwind</span>
                        <span class="text-xs px-2 py-0.5 bg-gray-100 dark:bg-gray-700 rounded-full">72</span>
                    </a>
                    <a href="#"
                        class="flex items-center justify-between px-3 py-2 rounded-lg border border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-750 transition-colors">
                        <span>性能优化</span>
                        <span class="text-xs px-2 py-0.5 bg-gray-100 dark:bg-gray-700 rounded-full">43</span>
                    </a>
                </div>
            </div>
        </div>
    </aside>
</template>