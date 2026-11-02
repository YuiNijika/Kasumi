<script setup lang="ts">
import CodeShowcase from '@/components/CodeShowcase.vue'
import CopyCommand from '@/components/CopyCommand.vue'
import { hero, heroCodeExamples } from '@/data/site'
</script>

<template>
  <section
    class="relative overflow-hidden pt-[88px] pb-24 max-lg:pt-[72px] max-sm:pt-14 max-sm:pb-[68px]"
    aria-labelledby="hero-title"
  >
    <div
      class="pointer-events-none absolute inset-0 [background-image:linear-gradient(to_right,var(--site-grid-line)_1px,transparent_1px),linear-gradient(to_bottom,var(--site-grid-line)_1px,transparent_1px)] [background-size:56px_56px] [mask-image:linear-gradient(to_bottom,black,transparent_82%)]"
      aria-hidden="true"
    ></div>
    <div
      class="pointer-events-none absolute top-[-24rem] left-1/2 h-[52rem] w-[82rem] -translate-x-1/2 [background:radial-gradient(ellipse,var(--site-page-glow),transparent_64%)]"
      aria-hidden="true"
    ></div>

    <div
      class="relative z-[1] mx-auto grid w-[calc(100%-48px)] max-w-site grid-cols-[minmax(0,0.9fr)_minmax(480px,1.1fr)] items-center gap-[clamp(42px,7vw,88px)] max-lg:grid-cols-1 max-lg:gap-12 max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]"
    >
      <div class="min-w-0 max-w-[565px] max-lg:max-w-[720px]" data-reveal>
        <p class="mb-4 font-mono text-[0.7rem] font-bold tracking-[0.06em] text-accent-bright">
          {{ hero.eyebrow }}
        </p>
        <h1
          id="hero-title"
          class="m-0 max-w-full text-balance font-display text-[clamp(2.7rem,5.4vw,3.8rem)] font-[680] leading-[1.02] tracking-[-0.045em] text-text max-sm:text-[clamp(2.45rem,12vw,3.55rem)] max-sm:tracking-[-0.05em]"
        >
          <span class="block">{{ hero.titleLead }}</span>
          <span class="mt-1 block text-accent">{{ hero.titleAccent }}</span>
        </h1>
        <p class="mt-6 max-w-[50ch] text-pretty text-[clamp(0.98rem,1.35vw,1.06rem)] leading-[1.8] text-muted">
          {{ hero.description }}
        </p>

        <div class="mt-[30px] flex flex-wrap items-center gap-2.5 max-sm:flex-col max-sm:items-stretch">
          <CopyCommand :command="hero.command" />
          <a
            class="inline-flex min-h-[54px] items-center justify-center gap-2 rounded-sm border border-line-strong px-[17px] text-[0.79rem] font-bold text-text-soft transition-colors hover:border-accent-line hover:bg-surface-hover hover:text-text"
            :href="hero.primaryAction.href"
            target="_blank"
            rel="noreferrer"
          >
            {{ hero.primaryAction.label }}
            <span aria-hidden="true">↗</span>
          </a>
          <a
            class="inline-flex min-h-[54px] items-center justify-center gap-2 rounded-sm border border-line-strong px-[17px] text-[0.79rem] font-bold text-text-soft transition-colors hover:border-accent-line hover:bg-surface-hover hover:text-text"
            :href="hero.secondaryAction.href"
            target="_blank"
            rel="noreferrer"
          >
            <svg aria-hidden="true" viewBox="0 0 24 24" width="16" height="16">
              <path
                d="M12 3.2a9 9 0 0 0-2.85 17.54c.45.08.62-.2.62-.44v-1.72c-2.53.55-3.06-1.07-3.06-1.07-.42-1.05-1.02-1.33-1.02-1.33-.83-.57.06-.56.06-.56.92.07 1.4.95 1.4.95.82 1.4 2.14 1 2.66.76.08-.59.32-1 .58-1.23-2.02-.23-4.14-1.01-4.14-4.5 0-1 .35-1.8.94-2.44-.1-.23-.41-1.16.09-2.4 0 0 .76-.25 2.5.93A8.7 8.7 0 0 1 12 7.52c.78 0 1.55.1 2.28.3 1.73-1.18 2.49-.93 2.49-.93.5 1.24.19 2.17.09 2.4.58.64.94 1.45.94 2.44 0 3.5-2.13 4.26-4.16 4.49.33.28.62.84.62 1.69v2.5c0 .25.17.53.63.44A9 9 0 0 0 12 3.2Z"
                fill="currentColor"
              />
            </svg>
            {{ hero.secondaryAction.label }}
          </a>
        </div>

        <dl class="mt-[38px] flex max-sm:justify-between">
          <div
            v-for="metric in hero.metrics"
            :key="metric.label"
            class="min-w-[104px] pr-6 [&+&]:border-l [&+&]:border-line [&+&]:pl-6 max-sm:min-w-0 max-sm:pr-[13px] max-sm:[&+&]:pl-[13px]"
          >
            <dt class="text-[0.68rem] font-[650] text-muted-strong">{{ metric.label }}</dt>
            <dd class="mt-[3px] font-display text-[1.45rem] font-[650] leading-none text-text [font-variant-numeric:tabular-nums]">
              {{ metric.value }}
            </dd>
          </div>
        </dl>
      </div>

      <div class="min-w-0 max-lg:w-[min(100%,720px)]" data-reveal>
        <CodeShowcase :examples="heroCodeExamples" />
        <p class="mt-[13px] flex items-center justify-end gap-2 text-[0.69rem] text-muted-strong max-lg:justify-start">
          <span class="size-1.5 rounded-full bg-accent" aria-hidden="true"></span>
          前端与两端宿主使用同一套能力边界
        </p>
      </div>
    </div>
  </section>
</template>
