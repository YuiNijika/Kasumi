<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'

import AdvantagesSection from '@/components/AdvantagesSection.vue'
import ComparisonSection from '@/components/ComparisonSection.vue'
import FooterSection from '@/components/FooterSection.vue'
import HeroSection from '@/components/HeroSection.vue'
import NavBar from '@/components/NavBar.vue'
import PlatformsSection from '@/components/PlatformsSection.vue'
import TechStackSection from '@/components/TechStackSection.vue'
import WorkflowSection from '@/components/WorkflowSection.vue'

let observer: IntersectionObserver | undefined
const mainContent = ref<HTMLElement>()

function focusMainContent() {
  mainContent.value?.focus()
}

onMounted(() => {
  const elements = Array.from(
    document.querySelectorAll<HTMLElement>('[data-reveal]'),
  )

  if (
    window.matchMedia('(prefers-reduced-motion: reduce)').matches ||
    !('IntersectionObserver' in window)
  ) {
    elements.forEach((element) => element.classList.add('is-visible'))
    return
  }

  observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) {
          return
        }

        entry.target.classList.add('is-visible')
        observer?.unobserve(entry.target)
      })
    },
    { rootMargin: '0px 0px -10% 0px', threshold: 0.1 },
  )

  elements.forEach((element) => observer?.observe(element))
})

onBeforeUnmount(() => observer?.disconnect())
</script>

<template>
  <button
    class="fixed left-3 top-3 z-[100] -translate-y-[160%] rounded-xs bg-accent px-4 py-2.5 text-[0.82rem] font-extrabold text-[#170b03] transition-transform duration-150 focus:translate-y-0"
    type="button"
    @click="focusMainContent"
  >
    跳到主要内容
  </button>

  <NavBar />
  <main
    ref="mainContent"
    class="focus-visible:outline-2 focus-visible:outline-offset-[-4px] focus-visible:outline-accent-bright"
    tabindex="-1"
  >
    <HeroSection />
    <AdvantagesSection />
    <PlatformsSection />
    <TechStackSection />
    <WorkflowSection />
    <ComparisonSection />
  </main>
  <FooterSection />
</template>
