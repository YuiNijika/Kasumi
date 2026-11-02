<script setup lang="ts">
import FeatureIcon from '@/components/FeatureIcon.vue'
import SectionHeading from '@/components/SectionHeading.vue'
import { advantages, capabilityOverview } from '@/data/site'

const leadAdvantage = advantages[0]
const capabilityItems = advantages.slice(1)
</script>

<template>
  <section
    class="relative scroll-mt-[90px] border-t border-line py-24 max-md:py-[76px] max-sm:py-16"
    data-section="features"
    aria-labelledby="features-title"
  >
    <div class="mx-auto w-[calc(100%-48px)] max-w-site max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]">
      <div data-reveal>
        <SectionHeading
          eyebrow="核心能力"
          title="需要的能力，都有清楚边界"
          intro="把跨平台共用的部分收进统一接口，把真正依赖操作系统的工作留给原生插件。"
          title-id="features-title"
        />
      </div>

      <div class="grid grid-cols-[minmax(0,1.08fr)_minmax(380px,0.92fr)] items-stretch gap-[18px] max-lg:grid-cols-1">
        <article
          v-if="leadAdvantage"
          class="relative flex min-h-[520px] flex-col overflow-hidden rounded-md border border-line bg-surface p-[clamp(26px,4vw,40px)] max-lg:min-h-[480px] max-[620px]:min-h-0 max-[620px]:p-5"
          data-reveal
        >
          <div
            class="pointer-events-none absolute inset-0 [background-image:linear-gradient(to_right,var(--site-grid-line)_1px,transparent_1px),linear-gradient(to_bottom,var(--site-grid-line)_1px,transparent_1px)] [background-size:40px_40px] [mask-image:linear-gradient(to_bottom,black,transparent_78%)]"
            aria-hidden="true"
          ></div>

          <p class="relative z-[1] inline-flex w-fit items-center gap-2 font-mono text-[0.65rem] font-[650] text-accent-bright">
            <span class="h-px w-[18px] bg-current" aria-hidden="true"></span>
            {{ capabilityOverview.label }}
          </p>
          <h3 class="relative z-[1] mt-12 max-w-[11ch] text-balance font-display text-[clamp(2rem,3.6vw,3.2rem)] font-[650] leading-[1.05] tracking-[-0.045em] text-text max-[620px]:mt-9 max-[620px]:text-[clamp(1.85rem,9vw,2.55rem)]">
            {{ leadAdvantage.title }}
          </h3>
          <p class="relative z-[1] mt-[15px] max-w-[44ch] text-[0.84rem] leading-[1.7] text-muted">
            {{ leadAdvantage.description }}
          </p>

          <div
            class="relative z-[1] mt-auto flex items-center gap-2.5 pt-[42px] max-[620px]:flex-col max-[620px]:items-stretch max-[620px]:pt-8"
            aria-label="Web 界面通过 Kasumi API 连接 Windows 与 Android"
          >
            <div class="flex min-h-[88px] min-w-0 flex-col justify-center rounded-sm border border-line bg-bg/75 px-3.5 py-[13px]">
              <span class="mb-[7px] font-mono text-[0.54rem] text-accent">01</span>
              <strong class="whitespace-nowrap font-display text-[0.79rem] font-[650] text-text">
                {{ capabilityOverview.web.label }}
              </strong>
              <small class="mt-[3px] whitespace-nowrap font-mono text-[0.52rem] text-muted-strong">
                {{ capabilityOverview.web.detail }}
              </small>
            </div>

            <span
              class="relative h-px w-6 shrink-0 bg-line-strong after:absolute after:-top-[3px] after:right-0 after:size-1.5 after:rotate-45 after:border-r after:border-t after:border-muted-strong max-[620px]:ml-5 max-[620px]:h-5 max-[620px]:w-px max-[620px]:after:bottom-0 max-[620px]:after:right-[-3px] max-[620px]:after:top-auto max-[620px]:after:rotate-[135deg]"
              aria-hidden="true"
            ></span>

            <div class="flex min-h-[88px] min-w-0 flex-col justify-center rounded-sm border border-accent-line bg-bg/75 px-3.5 py-[13px]">
              <span class="mb-[7px] font-mono text-[0.54rem] text-accent">02</span>
              <strong class="whitespace-nowrap font-display text-[0.79rem] font-[650] text-text">
                {{ capabilityOverview.bridge.label }}
              </strong>
              <small class="mt-[3px] whitespace-nowrap font-mono text-[0.52rem] text-muted-strong">
                {{ capabilityOverview.bridge.detail }}
              </small>
            </div>

            <span
              class="relative h-px w-6 shrink-0 bg-line-strong after:absolute after:-top-[3px] after:right-0 after:size-1.5 after:rotate-45 after:border-r after:border-t after:border-muted-strong max-[620px]:ml-5 max-[620px]:h-5 max-[620px]:w-px max-[620px]:after:bottom-0 max-[620px]:after:right-[-3px] max-[620px]:after:top-auto max-[620px]:after:rotate-[135deg]"
              aria-hidden="true"
            ></span>

            <div class="grid min-w-0 flex-1 gap-2 max-[620px]:grid-cols-2 max-[420px]:grid-cols-1">
              <div
                v-for="platform in capabilityOverview.platforms"
                :key="platform.label"
                class="flex min-h-0 min-w-0 flex-col justify-center rounded-sm border border-line bg-bg/75 px-3.5 py-2.5"
              >
                <strong class="whitespace-nowrap font-display text-[0.79rem] font-[650] text-text">
                  {{ platform.label }}
                </strong>
                <small class="mt-[3px] whitespace-nowrap font-mono text-[0.52rem] text-muted-strong">
                  {{ platform.detail }}
                </small>
              </div>
            </div>
          </div>
        </article>

        <div class="border-t border-line">
          <article
            v-for="advantage in capabilityItems"
            :key="advantage.title"
            class="grid grid-cols-[34px_minmax(0,1fr)] gap-[15px] border-b border-line px-4 py-[19px] transition-colors hover:bg-surface max-sm:px-2"
            data-reveal
          >
            <span class="grid size-[34px] place-items-center rounded-xs border border-line text-accent-bright">
              <FeatureIcon :name="advantage.icon" />
            </span>
            <div>
              <h3 class="mt-px font-display text-base font-[650] text-text">
                {{ advantage.title }}
              </h3>
              <p class="mt-1.5 text-[0.76rem] leading-[1.62] text-muted">
                {{ advantage.description }}
              </p>
            </div>
          </article>
        </div>
      </div>
    </div>
  </section>
</template>
