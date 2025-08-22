<?php
if (!defined('__TYPECHO_ROOT_DIR__')) exit;
?>
<div data-component="Post_Comments">
    <div class="card card-dash bg-base-100">
        <div class="card-body">

            <div  class="card-actions justify-end">
                <div class="w-full">
                    <h3 class="text-lg font-bold mb-4">发表评论</h3>

                    <div  class="alert alert-error">
                        <span></span>
                    </div>

                    <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
                        <div>
                            <label class="label">
                                <span class="label-text">昵称 *</span>
                            </label>
                            <input  type="text" placeholder="请输入昵称"
                                class="input input-bordered w-full" required />
                        </div>

                        <div>
                            <label class="label">
                                <span class="label-text">邮箱</span>
                            </label>
                            <input  type="email" placeholder="请输入邮箱"
                                class="input input-bordered w-full" />
                        </div>

                        <div class="md:col-span-2">
                            <label class="label">
                                <span class="label-text">网站</span>
                            </label>
                            <input  type="url" placeholder="https://example.com"
                                class="input input-bordered w-full" />
                        </div>

                        <div class="md:col-span-2">
                            <label class="label">
                                <span class="label-text">评论内容 *</span>
                            </label>
                            <textarea  placeholder="请输入评论内容"
                                class="textarea textarea-bordered w-full" rows="4" required></textarea>
                        </div>
                    </div>

                    <div class="flex justify-end">
                        <button class="btn btn-primary"  :disabled="submitting">
                            <span  class="loading loading-spinner"></span>
                            
                        </button>
                    </div>
                </div>
            </div>

            <div class="flex justify-between items-center mb-4">
                <h2 class="card-title">评论列表</h2>

                <div class="flex items-center space-x-2">
                    <select  class="select select-bordered select-sm"
                        >
                        <option value="newest">最新评论</option>
                        <option value="oldest">最早评论</option>
                    </select>
                </div>
            </div>

            <div  class="flex justify-center py-8">
                <div class="loading loading-spinner loading-md"></div>
            </div>

            <div v-else>

                <div  class="text-center py-8 text-gray-500">
                    还没有评论，快来抢沙发吧！
                </div>

                <div v-else class="space-y-6">

                    <div  :key="comment.coid">

                        <div class="border-l-2 border-base-300 pl-4">
                            <div class="chat chat-start">
                                <div class="chat-image avatar">
                                    <div class="w-10 rounded-full">
                                        <img
                                            :src="`https://cn.cravatar.com/avatar/${comment.mail || ''}?s=640&d=retro`" />
                                    </div>
                                </div>
                                <div class="chat-header">
                                    <span class="font-bold"></span>
                                    <time class="text-xs opacity-50 ml-2"></time>
                                </div>
                                <div class="chat-bubble break-words whitespace-pre-wrap" ></div>
                            </div>

                            <div class="mt-2 ml-4">
                                <button  class="btn btn-xs btn-ghost">
                                    回复
                                </button>
                            </div>

                            <div  class="mt-4 ml-8">
                                <div class="card bg-base-200">
                                    <div class="card-body p-4">
                                        <h3 class="card-title text-sm">回复 </h3>

                                        <div class="grid grid-cols-1 md:grid-cols-2 gap-2 mb-2">
                                            <div>
                                                <input  type="text" placeholder="昵称 *"
                                                    class="input input-bordered input-sm w-full" required />
                                            </div>

                                            <div>
                                                <input  type="email" placeholder="邮箱"
                                                    class="input input-bordered input-sm w-full" />
                                            </div>

                                            <div class="md:col-span-2">
                                                <input  type="url" placeholder="网站"
                                                    class="input input-bordered input-sm w-full" />
                                            </div>

                                            <div class="md:col-span-2">
                                                <textarea  :placeholder="`回复 ${comment.author}`"
                                                    class="textarea textarea-bordered textarea-sm w-full" rows="2"
                                                    required></textarea>
                                            </div>
                                        </div>

                                        <div class="card-actions justify-end">
                                            <button  class="btn btn-sm btn-ghost">取消</button>
                                            <button 
                                                class="btn btn-sm btn-primary" :disabled="submitting">
                                                <span 
                                                    class="loading loading-spinner loading-xs"></span>
                                                
                                            </button>
                                        </div>
                                    </div>
                                </div>
                            </div>


                            <div class="mt-4 space-y-4">
                                <div  :key="child1.coid"
                                    class="ml-8 border-l-2 border-base-300 pl-4">
                                    <div class="chat chat-start">
                                        <div class="chat-image avatar">
                                            <div class="w-10 rounded-full">
                                                <img
                                                    :src="`https://cn.cravatar.com/avatar/${child1.mail || ''}?s=640&d=retro`" />
                                            </div>
                                        </div>
                                        <div class="chat-header">
                                            <span class="font-bold"></span>
                                            <time class="text-xs opacity-50 ml-2"></time>
                                        </div>
                                        <div class="chat-bubble break-words whitespace-pre-wrap" >
                                        </div>
                                    </div>

                                    <div class="mt-2 ml-4">
                                        <button 
                                            class="btn btn-xs btn-ghost" :disabled="isMaxNestedLevel(child1)">
                                            
                                        </button>
                                    </div>

                                    <div  class="mt-4 ml-8">
                                        <div class="card bg-base-200">
                                            <div class="card-body p-4">
                                                <h3 class="card-title text-sm">回复 </h3>

                                                <div class="grid grid-cols-1 md:grid-cols-2 gap-2 mb-2">
                                                    <div>
                                                        <input  type="text" placeholder="昵称 *"
                                                            class="input input-bordered input-sm w-full" required />
                                                    </div>

                                                    <div>
                                                        <input  type="email" placeholder="邮箱"
                                                            class="input input-bordered input-sm w-full" />
                                                    </div>

                                                    <div class="md:col-span-2">
                                                        <input  type="url" placeholder="网站"
                                                            class="input input-bordered input-sm w-full" />
                                                    </div>

                                                    <div class="md:col-span-2">
                                                        <textarea 
                                                            :placeholder="`回复 ${child1.author}`"
                                                            class="textarea textarea-bordered textarea-sm w-full"
                                                            rows="2" required></textarea>
                                                    </div>
                                                </div>

                                                <div class="card-actions justify-end">
                                                    <button 
                                                        class="btn btn-sm btn-ghost">取消</button>
                                                    <button 
                                                        class="btn btn-sm btn-primary" :disabled="submitting">
                                                        <span 
                                                            class="loading loading-spinner loading-xs"></span>
                                                        
                                                    </button>
                                                </div>
                                            </div>
                                        </div>
                                    </div>

                                    <div class="mt-4 space-y-4">
                                        <div  :key="child2.coid"
                                            class="ml-8 border-l-2 border-base-300 pl-4">
                                            <div class="chat chat-start">
                                                <div class="chat-image avatar">
                                                    <div class="w-10 rounded-full">
                                                        <img
                                                            :src="`https://cn.cravatar.com/avatar/${child2.mail || ''}?s=640&d=retro`" />
                                                    </div>
                                                </div>
                                                <div class="chat-header">
                                                    <span class="font-bold"></span>
                                                    <time class="text-xs opacity-50 ml-2"></time>
                                                </div>
                                                <div class="chat-bubble break-words whitespace-pre-wrap"
                                                    ></div>
                                            </div>

                                            <div class="mt-2 ml-4">
                                                <button class="btn btn-xs btn-ghost" disabled>
                                                    已达最大回复层级
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</div>
