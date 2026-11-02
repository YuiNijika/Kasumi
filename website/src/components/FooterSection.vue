<script setup lang="ts">
import CopyCommand from '@/components/CopyCommand.vue'
import { footerLinks, hero, siteMeta } from '@/data/site'

function scrollToTop() {
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches

  window.scrollTo({
    top: 0,
    behavior: reducedMotion ? 'auto' : 'smooth',
  })
}
</script>

<template>
  <footer class="border-t border-line pb-[calc(30px+env(safe-area-inset-bottom))] max-sm:pb-[calc(24px+env(safe-area-inset-bottom))]">
    <div class="border-b border-line bg-[linear-gradient(90deg,var(--site-accent-wash),transparent_64%)]">
      <div
        class="mx-auto grid w-[calc(100%-48px)] max-w-site grid-cols-[minmax(0,1fr)_auto] items-center gap-11 py-[62px] max-md:w-[calc(100%-32px)] max-md:grid-cols-1 max-md:gap-[26px] max-sm:w-[calc(100%-24px)]"
      >
        <div class="min-w-0">
          <p class="mb-3 font-mono text-[0.7rem] font-bold tracking-[0.06em] text-accent-bright">
            现在开始
          </p>
          <h2
            class="m-0 max-w-[22ch] text-balance font-display text-[clamp(2rem,4vw,3.2rem)] font-[650] leading-[1.06] tracking-[-0.045em] text-text"
          >
            用熟悉的 Web 界面，交付下一款应用。
          </h2>
        </div>
        <div class="flex flex-col items-start gap-2.5">
          <CopyCommand :command="hero.command" />
          <a
            class="inline-flex items-center gap-1.5 text-[0.76rem] font-bold text-accent-bright no-underline hover:text-text"
            :href="siteMeta.documentation"
            target="_blank"
            rel="noreferrer"
          >
            阅读开发文档
            <span aria-hidden="true">↗</span>
          </a>
        </div>
      </div>
    </div>

    <div class="mx-auto w-[calc(100%-48px)] max-w-site max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]">
      <div
        class="grid grid-cols-[1.4fr_0.7fr_0.7fr] gap-[54px] py-[42px] max-md:grid-cols-2 max-sm:grid-cols-1 max-sm:gap-7"
      >
        <div class="max-md:col-span-2 max-sm:col-span-1">
          <button
            class="inline-flex items-center gap-[9px] text-text"
            type="button"
            aria-label="返回页面顶部"
            @click="scrollToTop"
          >
            <span
              class="grid size-[27px] place-items-center rounded-xs bg-accent font-display text-[0.86rem] font-extrabold text-[#190c03]"
              aria-hidden="true"
            >
              K
            </span>
            <strong
              class="font-display text-[1.1rem] font-[650] tracking-[-0.025em]"
              translate="no"
            >
              {{ siteMeta.name }}
            </strong>
          </button>
          <p class="mt-[13px] max-w-[38ch] text-[0.76rem] text-muted-strong">
            {{ siteMeta.description }}
          </p>
        </div>

        <nav class="flex flex-col" aria-label="页脚导航">
          <p class="mt-0.5 mb-3 font-mono text-[0.6rem] font-[650] text-muted-strong">
            资源
          </p>
          <a
            v-for="link in footerLinks"
            :key="link.href"
            class="mb-2 inline-flex w-fit items-center gap-[5px] text-[0.75rem] text-muted no-underline hover:text-accent-bright"
            :href="link.href"
            target="_blank"
            rel="noreferrer"
          >
            {{ link.label }}
            <span aria-hidden="true">↗</span>
          </a>
        </nav>

        <div class="flex flex-col">
          <p class="mt-0.5 mb-3 font-mono text-[0.6rem] font-[650] text-muted-strong">
            平台
          </p>
          <span class="mb-2 w-fit text-[0.75rem] text-muted">Windows</span>
          <span class="mb-2 w-fit text-[0.75rem] text-muted">Android</span>
          <span class="mb-2 w-fit text-[0.75rem] text-muted">MIT License</span>
        </div>
      </div>

      <div
        class="flex items-center justify-between border-t border-line pt-5 text-[0.65rem] text-muted-strong max-sm:flex-col max-sm:items-start max-sm:gap-2"
      >
        <p class="m-0">© 2026 Kasumi</p>
        <a
          class="inline-flex items-center gap-[5px] text-muted no-underline hover:text-accent-bright"
          :href="siteMeta.repository"
          target="_blank"
          rel="noreferrer"
        >
          GitHub
          <span aria-hidden="true">↗</span>
        </a>
      </div>
    </div>
  </footer>
</template>
