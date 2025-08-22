<script setup>
const cid = window.pageData?.cid || '1'
</script>

<template>
    <div class="card bg-base-100 shadow-sm sticky-sidebar">
        <div class="card-body">
            <span class="badge badge-xs badge-warning"></span>
            <div class="flex justify-between">
                <h2 class="text-3xl font-bold"></h2>
                <span class="text-xl"></span>
            </div>
            <ul class="mt-6 flex flex-col gap-2 text-xs">
                <li>
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-success" fill="none"
                        viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span>High-resolution image generation</span>
                </li>
                <li>
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-success" fill="none"
                        viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span>Customizable style templates</span>
                </li>
                <li>
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-success" fill="none"
                        viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span>Batch processing capabilities</span>
                </li>
                <li>
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-success" fill="none"
                        viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span>AI-driven image enhancements</span>
                </li>
                <li class="opacity-50">
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-base-content/50"
                        fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span class="line-through">Seamless cloud integration</span>
                </li>
                <li class="opacity-50">
                    <svg xmlns="http://www.w3.org/2000/svg" class="size-4 me-2 inline-block text-base-content/50"
                        fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7" />
                    </svg>
                    <span class="line-through">Real-time collaboration tools</span>
                </li>
            </ul>
            <div class="mt-6">
                <button class="btn btn-primary btn-block" onclick="downloadInfo.showModal()">立即下载</button>
            </div>
        </div>
    </div>
    <dialog id="downloadInfo" class="modal">
        <div class="modal-box">
            <h3 class="text-lg font-bold">Hello!</h3>
            <p class="py-4">Press ESC key or click the button below to close</p>
            <div class="modal-action">
                <form method="dialog">
                    <button class="btn">Close</button>
                </form>
            </div>
        </div>
    </dialog>
</template>