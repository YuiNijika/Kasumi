<script setup lang="ts">
import CopyCommand from '@/components/CopyCommand.vue'
import SectionHeading from '@/components/SectionHeading.vue'
import { templates, workflowSteps } from '@/data/site'
</script>

<template>
  <section
    class="relative scroll-mt-[90px] border-t border-line bg-bg-subtle/70 py-24 max-md:py-[76px] max-sm:py-16"
    data-section="workflow"
    aria-labelledby="workflow-title"
  >
    <div
      class="mx-auto grid w-[calc(100%-48px)] max-w-site grid-cols-[minmax(0,0.82fr)_minmax(500px,1.18fr)] items-start gap-[clamp(42px,7vw,90px)] max-lg:grid-cols-1 max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]"
    >
      <div
        class="sticky top-[98px] max-lg:static [&>header]:mb-[26px]"
        data-reveal
      >
        <SectionHeading
          eyebrow="开发方式"
          title="从创建到发布，只记一组命令"
          intro="不用先理解两个原生工程。选择熟悉的模板，开发和构建继续使用同一个包管理器。需要处理平台差异时，再进入对应的 Windows 或 Android 工程。"
          title-id="workflow-title"
        />

        <CopyCommand command="pnpm create kasumi" />

        <div class="mt-[26px] flex items-start gap-4 max-sm:flex-col" aria-label="可用模板">
          <span class="flex-none pt-1 font-mono text-[0.62rem] text-muted-strong">
            支持模板
          </span>
          <ul class="m-0 flex list-none flex-wrap gap-1.5 p-0">
            <li
              v-for="name in templates"
              :key="name"
              class="rounded-xs border border-line bg-surface px-2 py-[3px] font-mono text-[0.63rem] text-text-soft"
              translate="no"
            >
              {{ name }}
            </li>
          </ul>
        </div>
      </div>

      <div class="overflow-hidden rounded-md border border-line bg-surface" data-reveal>
        <div
          class="flex min-h-[42px] items-center gap-3 border-b border-line px-3.5 font-mono text-[0.61rem] text-muted-strong"
        >
          <span class="inline-flex gap-[5px]" aria-hidden="true">
            <i class="size-1.5 rounded-full bg-accent"></i>
            <i class="size-1.5 rounded-full bg-line-strong"></i>
            <i class="size-1.5 rounded-full bg-line-strong"></i>
          </span>
          <span>项目终端</span>
        </div>

        <ol class="m-0 list-none p-0">
          <li
            v-for="step in workflowSteps"
            :key="step.step"
            class="border-b border-line p-6 last:border-b-0 max-sm:p-5"
          >
            <div class="flex items-start gap-[15px]">
              <span class="flex-none pt-0.5 font-mono text-[0.63rem] text-accent">
                {{ step.step }}
              </span>
              <div>
                <h3 class="m-0 font-display text-base font-[650] text-text">
                  {{ step.title }}
                </h3>
                <p class="mt-[5px] text-[0.77rem] leading-[1.65] text-muted">
                  {{ step.description }}
                </p>
              </div>
            </div>
            <code
              class="mt-3.5 ml-[27px] block overflow-x-auto rounded-xs border border-line bg-bg-subtle px-3 py-2.5 font-mono text-[0.72rem] whitespace-nowrap text-text-soft max-sm:ml-0"
              translate="no"
            >
              <span class="mr-[7px] text-accent" aria-hidden="true">$</span>
              {{ step.command }}
            </code>
          </li>
        </ol>
      </div>
    </div>
  </section>
</template>
