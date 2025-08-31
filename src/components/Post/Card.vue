<template>
    <div class="flex flex-col lg:flex-row gap-8">
        <article class="w-full lg:w-2/3 space-y-6">
            <div class="space-y-4">
                <div class="flex flex-wrap gap-2">
                    <span class="badge bg-primary/10 text-primary hover:bg-primary/20">前端开发</span>
                    <span
                        class="badge bg-gray-100 text-gray-800 hover:bg-gray-200 dark:bg-gray-800 dark:text-gray-200 dark:hover:bg-gray-700">技术趋势</span>
                </div>

                <h1 class="text-3xl md:text-4xl font-bold text-balance">2023年现代前端开发趋势与实践指南</h1>

                <div class="flex items-center gap-4 text-sm text-gray-600 dark:text-gray-400">
                    <div class="flex items-center gap-2">
                        <img src="https://picsum.photos/id/1005/40/40" alt="作者头像"
                            class="w-8 h-8 rounded-full object-cover border border-gray-200">
                        <span>张开发</span>
                    </div>
                    <time datetime="2023-11-15"><span>📅</span> 2023-11-15</time>
                    <span><span>⏱️</span> 8分钟阅读</span>
                </div>
            </div>

            <figure class="w-full rounded-xl overflow-hidden shadow-lg">
                <img src="https://picsum.photos/id/0/1200/600" alt="前端开发趋势"
                    class="w-full h-auto object-cover transition-transform duration-500 hover:scale-105">
                <figcaption class="p-3 text-sm text-gray-600 dark:text-gray-400 text-center">现代前端开发技术栈概览
                </figcaption>
            </figure>

            <div class="prose prose-lg max-w-none dark:prose-invert">
                <p>随着Web技术的快速发展，前端开发领域也在不断演进。2023年，我们看到了许多令人兴奋的趋势和实践，这些都在重塑着我们构建Web应用的方式。本文将深入探讨这些趋势，并提供一些实用的指南，帮助开发者在这个快速变化的领域中保持竞争力。
                </p>

                <h2>1. 组件驱动开发的普及</h2>
                <p>组件驱动开发(CDD)已经成为现代前端开发的主流方法。通过将UI分解为独立、可重用的组件，开发团队能够更高效地协作，同时提高代码质量和可维护性。</p>
                <p>React、Vue和Angular等框架都大力支持组件化开发，而像Storybook这样的工具则进一步简化了组件的开发、测试和文档编写流程。</p>

                <blockquote class="border-l-4 border-primary pl-4 italic bg-gray-50 dark:bg-gray-800 p-4 rounded-r">
                    "组件化不仅是一种技术选择，更是一种思维方式，它改变了我们设计和构建用户界面的方式。"
                </blockquote>

                <h2>2. 无代码和低代码平台的兴起</h2>
                <p>无代码和低代码平台正在改变前端开发的格局。这些工具允许开发者和非技术人员通过可视化界面快速构建Web应用，而无需编写大量代码。</p>
                <p>虽然这些平台不能完全取代传统的前端开发，但它们确实为快速原型设计和简单应用开发提供了高效解决方案，使专业开发者能够专注于更复杂的问题。</p>

                <h2>3. 性能优化成为核心关注点</h2>
                <p>随着Web应用变得越来越复杂，性能优化已经从"锦上添花"变成了"必不可少"的环节。用户体验研究表明，页面加载时间每增加1秒，转化率可能下降多达20%。</p>
                <p>现代前端开发实践中，性能优化涉及多个方面：代码分割、懒加载、图片优化、缓存策略以及使用更高效的JavaScript特性等。</p>

                <img src="https://picsum.photos/id/180/800/400" alt="性能优化示意图"
                    class="w-full h-auto rounded-lg shadow-md my-6">

                <h2>4. Web Assembly扩展前端能力</h2>
                <p>Web Assembly(Wasm)为前端开发带来了新的可能性。作为一种低级二进制指令格式，Wasm允许高性能代码在Web浏览器中运行，其性能接近原生应用。</p>
                <p>虽然JavaScript仍然是Web开发的主要语言，但Wasm为计算密集型任务(如图像处理、数据分析和游戏开发)提供了更好的性能选择。</p>

                <h2>5. 跨平台开发的统一</h2>
                <p>随着用户使用的设备越来越多样化，跨平台开发变得越来越重要。现代框架如React Native、Flutter和Ionic允许开发者使用一套代码库构建在多个平台上运行的应用。</p>
                <p>这种趋势不仅提高了开发效率，还确保了不同平台上的用户体验一致性，同时降低了维护成本。</p>
            </div>

            <!-- 文章标签 -->
            <div class="pt-4 border-t border-gray-200 dark:border-gray-700">
                <h3 class="font-medium mb-3">相关标签</h3>
                <div class="flex flex-wrap gap-2">
                    <a href="#" class="chip hover:bg-gray-200 dark:hover:bg-gray-700">#JavaScript</a>
                    <a href="#" class="chip hover:bg-gray-200 dark:hover:bg-gray-700">#React</a>
                    <a href="#" class="chip hover:bg-gray-200 dark:hover:bg-gray-700">#Vue</a>
                    <a href="#" class="chip hover:bg-gray-200 dark:hover:bg-gray-700">#Web开发</a>
                    <a href="#" class="chip hover:bg-gray-200 dark:hover:bg-gray-700">#性能优化</a>
                </div>
            </div>

            <!-- 分享和点赞 -->
            <div
                class="flex flex-wrap justify-between items-center gap-4 pt-4 border-t border-gray-200 dark:border-gray-700">
                <div class="flex items-center gap-3">
                    <button class="btn btn-ghost gap-1 hover:bg-gray-100 dark:hover:bg-gray-800">
                        <span>❤️</span>
                        <span>喜欢 (128)</span>
                    </button>
                    <button class="btn btn-ghost gap-1 hover:bg-gray-100 dark:hover:bg-gray-800">
                        <span>🔖</span>
                        <span>收藏</span>
                    </button>
                </div>

                <div class="flex items-center gap-2">
                    <span class="text-sm">分享：</span>
                    <a href="#" class="btn btn-circle btn-ghost hover:bg-primary hover:text-white">
                        <span>🐦</span>
                    </a>
                    <a href="#" class="btn btn-circle btn-ghost hover:bg-primary hover:text-white">
                        <span>📘</span>
                    </a>
                    <a href="#" class="btn btn-circle btn-ghost hover:bg-primary hover:text-white">
                        <span>🔗</span>
                    </a>
                </div>
            </div>

            <div class="card bg-gray-50 dark:bg-gray-800/50 border border-gray-200 dark:border-gray-700">
                <div class="card-body p-6 flex flex-col sm:flex-row gap-4 items-center sm:items-start">
                    <img src="https://picsum.photos/id/1005/100/100" alt="作者头像"
                        class="w-16 h-16 rounded-full object-cover border-2 border-white dark:border-gray-700 shadow-sm">
                    <div class="text-center sm:text-left">
                        <h3 class="font-bold text-lg">张开发</h3>
                        <p class="text-gray-600 dark:text-gray-400 text-sm">资深前端工程师，拥有10年Web开发经验，热衷于分享前端技术和最佳实践。</p>
                        <div class="flex justify-center sm:justify-start gap-3 mt-3">
                            <a href="#" class="text-gray-600 dark:text-gray-400 hover:text-primary transition-colors">
                                <span>🐙</span>
                            </a>
                            <a href="#" class="text-gray-600 dark:text-gray-400 hover:text-primary transition-colors">
                                <span>🐦</span>
                            </a>
                            <a href="#" class="text-gray-600 dark:text-gray-400 hover:text-primary transition-colors">
                                <span>🌐</span>
                            </a>
                        </div>
                    </div>
                </div>
            </div>

            <div class="space-y-6 pt-6">
                <PostComment />
            </div>
        </article>
        <PostSidebar />
    </div>
</template>