<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { comparisonRows } from '@/data/site'
</script>

<template>
  <section
    class="relative scroll-mt-[90px] border-t border-line py-24 max-md:py-[76px] max-sm:py-16"
    data-section="comparison"
    aria-labelledby="comparison-title"
  >
    <div
      class="mx-auto w-[calc(100%-48px)] max-w-site max-md:w-[calc(100%-32px)] max-sm:w-[calc(100%-24px)]"
    >
      <div data-reveal>
        <SectionHeading
          eyebrow="方案对比"
          title="先看运行时和边界"
          intro="三种方案都能承载 Web 界面。真正影响工程选择的是运行时来源、平台目标、原生边界和扩展方式。"
          title-id="comparison-title"
          centered
        />
      </div>

      <div
        class="overflow-x-auto overscroll-x-contain rounded-md border border-line bg-surface focus-visible:outline-offset-6"
        tabindex="0"
        role="region"
        aria-label="Kasumi、Tauri 2 与 Electron 能力对比表"
        data-reveal
      >
        <table class="w-full min-w-[760px] border-collapse">
          <caption class="sr-only">
            Kasumi、Tauri 2 与 Electron 的能力对比
          </caption>
          <thead>
            <tr>
              <th
                scope="col"
                class="bg-bg-subtle px-[21px] py-4 text-left font-mono text-[0.63rem] font-[650] text-muted uppercase max-sm:px-4 max-sm:py-3.5"
              >
                维度
              </th>
              <th
                scope="col"
                class="bg-bg-subtle px-[21px] py-4 text-left font-mono text-[0.63rem] font-[650] text-accent-bright uppercase max-sm:px-4 max-sm:py-3.5"
              >
                Kasumi
              </th>
              <th
                scope="col"
                class="bg-bg-subtle px-[21px] py-4 text-left font-mono text-[0.63rem] font-[650] text-muted uppercase max-sm:px-4 max-sm:py-3.5"
              >
                Tauri 2
              </th>
              <th
                scope="col"
                class="bg-bg-subtle px-[21px] py-4 text-left font-mono text-[0.63rem] font-[650] text-muted uppercase max-sm:px-4 max-sm:py-3.5"
              >
                Electron
              </th>
            </tr>
          </thead>
          <tbody>
            <tr
              v-for="row in comparisonRows"
              :key="row.criterion"
              class="border-b border-line last:border-b-0 hover:bg-surface-hover"
            >
              <th
                scope="row"
                class="w-[18%] px-[21px] py-4 text-left text-[0.77rem] font-[650] text-text-soft max-sm:px-4 max-sm:py-3.5"
              >
                {{ row.criterion }}
              </th>
              <td
                class="w-[27%] bg-accent-wash px-[21px] py-4 text-[0.77rem] font-semibold text-text max-sm:px-4 max-sm:py-3.5"
              >
                <span
                  class="mr-2 inline-block size-[5px] translate-y-0.5 rounded-full bg-accent"
                  aria-hidden="true"
                ></span>
                {{ row.kasumi }}
              </td>
              <td class="w-[27%] px-[21px] py-4 text-[0.77rem] text-muted max-sm:px-4 max-sm:py-3.5">
                {{ row.tauri }}
              </td>
              <td class="w-[27%] px-[21px] py-4 text-[0.77rem] text-muted max-sm:px-4 max-sm:py-3.5">
                {{ row.electron }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </section>
</template>
