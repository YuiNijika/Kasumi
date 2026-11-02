<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { techStack } from '@/data/site'
</script>

<template>
  <section
    class="relative scroll-mt-[90px] border-t border-line py-24 max-md:py-[76px] max-sm:py-16"
    data-section="stack"
    aria-labelledby="tech-stack-title"
  >
    <div class="mx-auto w-[calc(100%-48px)] max-w-site max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]">
      <div data-reveal>
        <SectionHeading
          eyebrow="技术栈"
          title="每层只处理自己的问题"
          intro="前端不接触真实平台对象，原生宿主不承载页面逻辑，协议把两端稳定地连接起来。"
          title-id="tech-stack-title"
          centered
        />
      </div>

      <div class="grid grid-cols-4 gap-3 max-lg:grid-cols-2 max-sm:grid-cols-1">
        <article
          v-for="item in techStack"
          :key="item.name"
          class="min-h-[235px] rounded-md border border-line bg-surface p-[22px] transition-colors hover:border-line-strong hover:bg-surface-hover max-sm:min-h-[205px]"
          data-reveal
        >
          <div class="flex items-center justify-between gap-3">
            <span
              class="grid size-[38px] place-items-center rounded-xs border border-accent-line bg-accent-wash font-mono text-[0.68rem] font-bold text-accent-bright"
              translate="no"
            >
              {{ item.mark }}
            </span>
            <span class="text-right font-mono text-[0.57rem] text-muted-strong" translate="no">
              {{ item.role }}
            </span>
          </div>
          <h3 class="mt-10 font-display text-xl font-[650] tracking-[-0.02em] text-text" translate="no">
            {{ item.name }}
          </h3>
          <p class="mt-2.5 text-[0.77rem] leading-[1.7] text-muted">
            {{ item.description }}
          </p>
        </article>
      </div>
    </div>
  </section>
</template>
