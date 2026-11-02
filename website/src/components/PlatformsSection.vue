<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { platforms } from '@/data/site'
</script>

<template>
  <section
    class="relative scroll-mt-[90px] border-t border-line bg-bg-subtle/75 py-24 max-md:py-[76px] max-sm:py-16"
    data-section="platforms"
    aria-labelledby="platforms-title"
  >
    <div class="mx-auto w-[calc(100%-48px)] max-w-site max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]">
      <div data-reveal>
        <SectionHeading
          eyebrow="平台支持"
          title="同一套页面，两种原生运行环境"
          intro="共享调用接口和协议契约，窗口、权限、生命周期与系统能力分别由对应平台实现。"
          title-id="platforms-title"
        />
      </div>

      <div class="grid grid-cols-2 gap-3.5 max-md:grid-cols-1">
        <article
          v-for="platform in platforms"
          :key="platform.id"
          class="flex min-h-[380px] flex-col rounded-md border border-line bg-surface p-7 transition-colors hover:border-line-strong hover:bg-surface-hover max-md:min-h-[340px] max-sm:p-[22px]"
          data-reveal
        >
          <div class="flex items-center justify-between">
            <span
              class="grid h-[30px] min-w-11 place-items-center rounded-xs border border-accent-line bg-accent-wash px-2 font-mono text-[0.65rem] font-bold text-accent-bright"
              translate="no"
            >
              {{ platform.shortName }}
            </span>
            <span class="font-mono text-[0.62rem] text-muted-strong" translate="no">
              {{ platform.runtime }}
            </span>
          </div>

          <h3 class="mt-[42px] font-display text-[clamp(2rem,3.4vw,3rem)] font-[650] leading-none tracking-[-0.045em] text-text">
            {{ platform.name }}
          </h3>
          <p class="mt-[15px] max-w-[46ch] text-[0.88rem] leading-[1.72] text-muted">
            {{ platform.description }}
          </p>

          <ul
            class="mt-auto grid grid-cols-2 gap-x-3.5 gap-y-[9px] border-t border-line pt-5 max-sm:grid-cols-1"
            :aria-label="`${platform.name} 能力`"
          >
            <li
              v-for="feature in platform.features"
              :key="feature"
              class="flex min-w-0 items-center gap-[7px] text-[0.77rem] text-text-soft"
            >
              <span class="text-[0.66rem] text-accent" aria-hidden="true">✓</span>
              {{ feature }}
            </li>
          </ul>

          <a
            class="mt-[22px] inline-flex items-center gap-1.5 self-start text-[0.75rem] font-bold text-accent-bright hover:text-text"
            href="https://blog.miomoe.cn/docs/kasumi/development"
            target="_blank"
            rel="noreferrer"
          >
            查看平台说明
            <span aria-hidden="true">↗</span>
          </a>
        </article>
      </div>
    </div>
  </section>
</template>
