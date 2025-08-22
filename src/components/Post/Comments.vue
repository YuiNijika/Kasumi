<script setup>
import { ref, onMounted, computed } from 'vue'

// 从页面获取 cid 和 uid
const cid = window.pageData?.cid || '1'
const uid = window.pageData?.uid || '0'

// 评论列表数据
const comments = ref([])
const loading = ref(true)

// 排序方式
const sortOption = ref('newest') // newest or oldest

// 评论表单数据
const commentForm = ref({
    author: '',
    mail: '',
    url: '',
    text: ''
})

// 回复相关数据
const replyForm = ref({
    author: '',
    mail: '',
    url: '',
    text: ''
})
const replyTo = ref(null) // 当前回复的评论ID
const replyToAuthor = ref('') // 当前回复的作者名

// 提交评论相关状态
const submitting = ref(false)
const submitError = ref('')

// 切换排序方式
const changeSort = (option) => {
    sortOption.value = option
    // 对评论列表进行排序
    sortComments()
}

// 排序评论列表
const sortComments = () => {
    if (sortOption.value === 'newest') {
        comments.value.sort((a, b) => new Date(b.created) - new Date(a.created))
    } else {
        comments.value.sort((a, b) => new Date(a.created) - new Date(b.created))
    }
}

// 获取评论列表后添加排序
const fetchComments = async () => {
    try {
        const response = await fetch(`/ty-json/comments/cid/${cid}`)
        const result = await response.json()

        if (result.code === 200 && result.data) {
            const rawData = Array.isArray(result.data) ? result.data : [result.data]
            comments.value = rawData
            // 获取评论后立即排序
            sortComments()
        }
    } catch (error) {
        console.error('获取评论失败:', error)
    } finally {
        loading.value = false
    }
}

// 时间格式化函数
const formatDate = (dateString) => {
    const date = new Date(dateString)
    return date.toLocaleDateString('zh-CN', {
        year: 'numeric',
        month: 'short',
        day: 'numeric'
    })
}

// 处理评论内容，添加@用户名
const processCommentText = (text, parentAuthor = '') => {
    if (!parentAuthor) return text
    // 检查是否已经包含@用户名
    if (text.includes(`@${parentAuthor}`)) return text
    return `@${parentAuthor} ${text}`
}

// 提交评论
const submitComment = async (isReply = false, parentId = null, parentAuthor = '') => {
    const form = isReply ? replyForm : commentForm
    let text = form.value.text.trim()

    if (!text) {
        submitError.value = '评论内容不能为空'
        return
    }

    // 如果是回复评论，处理@用户名
    if (isReply && parentAuthor) {
        text = processCommentText(text, parentAuthor)
    }

    submitting.value = true
    submitError.value = ''

    try {
        const requestData = {
            cid: cid,
            author: form.value.author,
            mail: form.value.mail,
            url: form.value.url,
            text: text
        }

        // 如果用户已登录，添加 authorId 字段
        if (uid && uid !== '0') {
            requestData.authorId = uid
        }

        // 如果是回复评论，添加parent字段
        if (isReply && parentId) {
            requestData.parent = parentId
        }

        const response = await fetch('/ty-json/comments/', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(requestData)
        })

        const result = await response.json()

        if (result.code === 200) {
            // 提交成功，重新获取评论列表
            await fetchComments()

            // 清空表单
            if (isReply) {
                replyForm.value = {
                    author: '',
                    mail: '',
                    url: '',
                    text: ''
                }
                replyTo.value = null // 关闭回复表单
                replyToAuthor.value = ''
            } else {
                commentForm.value = {
                    author: '',
                    mail: '',
                    url: '',
                    text: ''
                }
            }
        } else {
            submitError.value = result.message || '提交失败'
        }
    } catch (error) {
        console.error('提交评论失败:', error)
        submitError.value = '提交评论时发生错误'
    } finally {
        submitting.value = false
    }
}

// 设置回复目标
const setReplyTo = (commentId, commentAuthor) => {
    replyTo.value = commentId
    replyToAuthor.value = commentAuthor
    // 自动填充回复表单的作者信息（如果用户已经填写过）
    if (commentForm.value.author) {
        replyForm.value.author = commentForm.value.author
        replyForm.value.mail = commentForm.value.mail
        replyForm.value.url = commentForm.value.url
    }
}

// 取消回复
const cancelReply = () => {
    replyTo.value = null
    replyToAuthor.value = ''
    replyForm.value = {
        author: '',
        mail: '',
        url: '',
        text: ''
    }
}

// 获取指定父评论的子评论
const getChildComments = (parentCoid) => {
    return comments.value.filter(comment => comment.parent === parentCoid)
}

// 检查评论是否达到最大嵌套级别
const isMaxNestedLevel = (comment) => {
    // 如果评论没有parent，说明是一级评论
    if (!comment.parent || comment.parent === 0) return false

    // 查找父评论
    const parentComment = comments.value.find(c => c.coid === comment.parent)
    if (!parentComment) return false

    // 如果父评论有parent，说明当前评论已经是三级评论
    return parentComment.parent && parentComment.parent !== 0
}

// 挂载时获取评论
onMounted(() => {
    fetchComments()
})
</script>

<template>
    <div class="card card-dash bg-base-100">
        <div class="card-body">

            <div v-if="!replyTo" class="card-actions justify-end">
                <div class="w-full">
                    <h3 class="text-lg font-bold mb-4">发表评论</h3>

                    <div v-if="submitError" class="alert alert-error">
                        <span>{{ submitError }}</span>
                    </div>

                    <div class="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
                        <div>
                            <label class="label">
                                <span class="label-text">昵称 *</span>
                            </label>
                            <input v-model="commentForm.author" type="text" placeholder="请输入昵称"
                                class="input input-bordered w-full" required />
                        </div>

                        <div>
                            <label class="label">
                                <span class="label-text">邮箱</span>
                            </label>
                            <input v-model="commentForm.mail" type="email" placeholder="请输入邮箱"
                                class="input input-bordered w-full" />
                        </div>

                        <div class="md:col-span-2">
                            <label class="label">
                                <span class="label-text">网站</span>
                            </label>
                            <input v-model="commentForm.url" type="url" placeholder="https://example.com"
                                class="input input-bordered w-full" />
                        </div>

                        <div class="md:col-span-2">
                            <label class="label">
                                <span class="label-text">评论内容 *</span>
                            </label>
                            <textarea v-model="commentForm.text" placeholder="请输入评论内容"
                                class="textarea textarea-bordered w-full" rows="4" required></textarea>
                        </div>
                    </div>

                    <div class="flex justify-end">
                        <button class="btn btn-primary" @click="submitComment(false)" :disabled="submitting">
                            <span v-if="submitting" class="loading loading-spinner"></span>
                            {{ submitting ? '提交中...' : '提交评论' }}
                        </button>
                    </div>
                </div>
            </div>

            <div class="flex justify-between items-center mb-4">
                <h2 class="card-title">评论列表</h2>

                <div class="flex items-center space-x-2">
                    <select v-model="sortOption" class="select select-bordered select-sm"
                        @change="changeSort(sortOption)">
                        <option value="newest">最新评论</option>
                        <option value="oldest">最早评论</option>
                    </select>
                </div>
            </div>

            <div v-if="loading" class="flex justify-center py-8">
                <div class="loading loading-spinner loading-md"></div>
            </div>

            <div v-else>

                <div v-if="comments.length === 0" class="text-center py-8 text-gray-500">
                    还没有评论，快来抢沙发吧！
                </div>

                <div v-else class="space-y-6">

                    <div v-for="comment in comments.filter(c => c.parent === 0)" :key="comment.coid">

                        <div class="border-l-2 border-base-300 pl-4">
                            <div class="chat chat-start">
                                <div class="chat-image avatar">
                                    <div class="w-10 rounded-full">
                                        <img
                                            :src="`https://cn.cravatar.com/avatar/${comment.mail || ''}?s=640&d=retro`" />
                                    </div>
                                </div>
                                <div class="chat-header">
                                    <span class="font-bold">{{ comment.author }}</span>
                                    <time class="text-xs opacity-50 ml-2">{{ formatDate(comment.created) }}</time>
                                </div>
                                <div class="chat-bubble break-words whitespace-pre-wrap" v-html="comment.text"></div>
                            </div>

                            <div class="mt-2 ml-4">
                                <button @click="setReplyTo(comment.coid, comment.author)" class="btn btn-xs btn-ghost">
                                    回复
                                </button>
                            </div>

                            <div v-if="replyTo === comment.coid" class="mt-4 ml-8">
                                <div class="card bg-base-200">
                                    <div class="card-body p-4">
                                        <h3 class="card-title text-sm">回复 {{ comment.author }}</h3>

                                        <div class="grid grid-cols-1 md:grid-cols-2 gap-2 mb-2">
                                            <div>
                                                <input v-model="replyForm.author" type="text" placeholder="昵称 *"
                                                    class="input input-bordered input-sm w-full" required />
                                            </div>

                                            <div>
                                                <input v-model="replyForm.mail" type="email" placeholder="邮箱"
                                                    class="input input-bordered input-sm w-full" />
                                            </div>

                                            <div class="md:col-span-2">
                                                <input v-model="replyForm.url" type="url" placeholder="网站"
                                                    class="input input-bordered input-sm w-full" />
                                            </div>

                                            <div class="md:col-span-2">
                                                <textarea v-model="replyForm.text" :placeholder="`回复 ${comment.author}`"
                                                    class="textarea textarea-bordered textarea-sm w-full" rows="2"
                                                    required></textarea>
                                            </div>
                                        </div>

                                        <div class="card-actions justify-end">
                                            <button @click="cancelReply" class="btn btn-sm btn-ghost">取消</button>
                                            <button @click="submitComment(true, comment.coid, comment.author)"
                                                class="btn btn-sm btn-primary" :disabled="submitting">
                                                <span v-if="submitting"
                                                    class="loading loading-spinner loading-xs"></span>
                                                {{ submitting ? '提交中...' : '提交回复' }}
                                            </button>
                                        </div>
                                    </div>
                                </div>
                            </div>


                            <div class="mt-4 space-y-4">
                                <div v-for="child1 in getChildComments(comment.coid)" :key="child1.coid"
                                    class="ml-8 border-l-2 border-base-300 pl-4">
                                    <div class="chat chat-start">
                                        <div class="chat-image avatar">
                                            <div class="w-10 rounded-full">
                                                <img
                                                    :src="`https://cn.cravatar.com/avatar/${child1.mail || ''}?s=640&d=retro`" />
                                            </div>
                                        </div>
                                        <div class="chat-header">
                                            <span class="font-bold">{{ child1.author }}</span>
                                            <time class="text-xs opacity-50 ml-2">{{ formatDate(child1.created)
                                                }}</time>
                                        </div>
                                        <div class="chat-bubble break-words whitespace-pre-wrap" v-html="child1.text"></div>
                                    </div>

                                    <div class="mt-2 ml-4">
                                        <button @click="setReplyTo(child1.coid, child1.author)"
                                            class="btn btn-xs btn-ghost" :disabled="isMaxNestedLevel(child1)">
                                            {{ isMaxNestedLevel(child1) ? '已达最大回复层级' : '回复' }}
                                        </button>
                                    </div>

                                    <div v-if="replyTo === child1.coid && !isMaxNestedLevel(child1)" class="mt-4 ml-8">
                                        <div class="card bg-base-200">
                                            <div class="card-body p-4">
                                                <h3 class="card-title text-sm">回复 {{ child1.author }}</h3>

                                                <div class="grid grid-cols-1 md:grid-cols-2 gap-2 mb-2">
                                                    <div>
                                                        <input v-model="replyForm.author" type="text" placeholder="昵称 *"
                                                            class="input input-bordered input-sm w-full" required />
                                                    </div>

                                                    <div>
                                                        <input v-model="replyForm.mail" type="email" placeholder="邮箱"
                                                            class="input input-bordered input-sm w-full" />
                                                    </div>

                                                    <div class="md:col-span-2">
                                                        <input v-model="replyForm.url" type="url" placeholder="网站"
                                                            class="input input-bordered input-sm w-full" />
                                                    </div>

                                                    <div class="md:col-span-2">
                                                        <textarea v-model="replyForm.text"
                                                            :placeholder="`回复 ${child1.author}`"
                                                            class="textarea textarea-bordered textarea-sm w-full"
                                                            rows="2" required></textarea>
                                                    </div>
                                                </div>

                                                <div class="card-actions justify-end">
                                                    <button @click="cancelReply"
                                                        class="btn btn-sm btn-ghost">取消</button>
                                                    <button @click="submitComment(true, child1.coid, child1.author)"
                                                        class="btn btn-sm btn-primary" :disabled="submitting">
                                                        <span v-if="submitting"
                                                            class="loading loading-spinner loading-xs"></span>
                                                        {{ submitting ? '提交中...' : '提交回复' }}
                                                    </button>
                                                </div>
                                            </div>
                                        </div>
                                    </div>

                                    <div class="mt-4 space-y-4">
                                        <div v-for="child2 in getChildComments(child1.coid)" :key="child2.coid"
                                            class="ml-8 border-l-2 border-base-300 pl-4">
                                            <div class="chat chat-start">
                                                <div class="chat-image avatar">
                                                    <div class="w-10 rounded-full">
                                                        <img
                                                            :src="`https://cn.cravatar.com/avatar/${child2.mail || ''}?s=640&d=retro`" />
                                                    </div>
                                                </div>
                                                <div class="chat-header">
                                                    <span class="font-bold">{{ child2.author }}</span>
                                                    <time class="text-xs opacity-50 ml-2">{{ formatDate(child2.created)
                                                        }}</time>
                                                </div>
                                                <div class="chat-bubble break-words whitespace-pre-wrap" v-html="child2.text"></div>
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
</template>