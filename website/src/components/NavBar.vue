<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'

import { navigation, type NavigationTarget, siteMeta } from '@/data/site'
import { useTheme } from '@/useTheme'

const isOpen = ref(false)
const activeSection = ref<NavigationTarget>()
const { isDark, toggleTheme } = useTheme()
let sectionObserver: IntersectionObserver | undefined

function closeMenu() {
  isOpen.value = false
}

function prefersReducedMotion() {
  return window.matchMedia('(prefers-reduced-motion: reduce)').matches
}

function navigateTo(target: NavigationTarget) {
  const section = document.querySelector<HTMLElement>(
    `[data-section="${target}"]`,
  )

  if (!section) {
    return
  }

  closeMenu()
  activeSection.value = target
  section.scrollIntoView({
    behavior: prefersReducedMotion() ? 'auto' : 'smooth',
    block: 'start',
  })
}

function scrollToTop() {
  closeMenu()
  window.scrollTo({
    top: 0,
    behavior: prefersReducedMotion() ? 'auto' : 'smooth',
  })
}

function handleEscape(event: KeyboardEvent) {
  if (event.key === 'Escape') {
    closeMenu()
  }
}

watch(isOpen, (open) => {
  if (open) {
    window.addEventListener('keydown', handleEscape)
    return
  }

  window.removeEventListener('keydown', handleEscape)
})

onMounted(() => {
  const sections = navigation
    .map((item) =>
      document.querySelector<HTMLElement>(`[data-section="${item.target}"]`),
    )
    .filter((section): section is HTMLElement => section !== null)

  sectionObserver = new IntersectionObserver(
    (entries) => {
      const visibleEntry = entries
        .filter((entry) => entry.isIntersecting)
        .sort((left, right) => right.intersectionRatio - left.intersectionRatio)[0]

      if (!visibleEntry) {
        return
      }

      activeSection.value = (visibleEntry.target as HTMLElement).dataset.section as
        | NavigationTarget
        | undefined
    },
    { rootMargin: '-18% 0px -68% 0px', threshold: [0, 0.25, 0.5] },
  )

  sections.forEach((section) => sectionObserver?.observe(section))
})

onBeforeUnmount(() => {
  sectionObserver?.disconnect()
  window.removeEventListener('keydown', handleEscape)
})
</script>

<template>
  <header
    class="nav-surface sticky top-0 z-40 border-b border-line bg-nav pt-[env(safe-area-inset-top)] backdrop-blur-xl backdrop-saturate-[1.3]"
  >
    <nav
      class="relative mx-auto flex min-h-[62px] w-[min(calc(100%-48px),1240px)] items-center justify-between max-md:w-[min(calc(100%-32px),1240px)]"
      aria-label="主要导航"
    >
      <button
        class="inline-flex min-h-11 items-center gap-[9px] text-text"
        type="button"
        aria-label="返回 Kasumi 首页"
        @click="scrollToTop"
      >
        <span
          class="font-display text-[1.04rem] font-[650] tracking-[-0.025em]"
          translate="no"
        >
          {{ siteMeta.name }}
        </span>
      </button>

      <div class="flex items-center gap-0.5 max-[900px]:hidden">
        <button
          v-for="item in navigation"
          :key="item.target"
          type="button"
          class="relative inline-flex min-h-11 items-center px-[11px] text-[0.78rem] font-semibold text-muted transition-colors after:absolute after:inset-x-[11px] after:bottom-[3px] after:h-px after:origin-center after:scale-x-[0.6] after:bg-accent after:opacity-0 after:transition-[opacity,transform] hover:text-text"
          :class="{
            'text-text after:scale-x-100 after:opacity-100':
              activeSection === item.target,
          }"
          :aria-current="activeSection === item.target ? 'location' : undefined"
          @click="navigateTo(item.target)"
        >
          {{ item.label }}
        </button>
      </div>

      <div class="flex items-center gap-1">
        <button
          class="grid size-11 place-items-center rounded-xs border border-transparent text-muted transition-colors hover:border-line hover:bg-surface-hover hover:text-text"
          type="button"
          :aria-label="isDark ? '切换到日间模式' : '切换到夜间模式'"
          @click="toggleTheme"
        >
          <svg
            v-if="isDark"
            aria-hidden="true"
            viewBox="0 0 24 24"
            width="18"
            height="18"
          >
            <circle cx="12" cy="12" r="3.5" fill="none" stroke="currentColor" stroke-width="1.6" />
            <path
              d="M12 2.8v2.1M12 19.1v2.1M2.8 12h2.1M19.1 12h2.1M5.5 5.5l1.5 1.5M17 17l1.5 1.5M18.5 5.5 17 7M7 17l-1.5 1.5"
              fill="none"
              stroke="currentColor"
              stroke-linecap="round"
              stroke-width="1.6"
            />
          </svg>
          <svg
            v-else
            aria-hidden="true"
            viewBox="0 0 24 24"
            width="18"
            height="18"
          >
            <path
              d="M19.3 15.6A7.8 7.8 0 0 1 8.4 4.7 8 8 0 1 0 19.3 15.6Z"
              fill="none"
              stroke="currentColor"
              stroke-linejoin="round"
              stroke-width="1.6"
            />
          </svg>
        </button>

        <a
          class="grid size-11 place-items-center rounded-xs border border-transparent text-muted transition-colors hover:border-line hover:bg-surface-hover hover:text-text max-[420px]:hidden"
          :href="siteMeta.repository"
          target="_blank"
          rel="noreferrer"
          aria-label="在 GitHub 查看 Kasumi 源码"
        >
          <svg aria-hidden="true" viewBox="0 0 24 24" width="18" height="18">
            <path
              d="M12 3.2a9 9 0 0 0-2.85 17.54c.45.08.62-.2.62-.44v-1.72c-2.53.55-3.06-1.07-3.06-1.07-.42-1.05-1.02-1.33-1.02-1.33-.83-.57.06-.56.06-.56.92.07 1.4.95 1.4.95.82 1.4 2.14 1 2.66.76.08-.59.32-1 .58-1.23-2.02-.23-4.14-1.01-4.14-4.5 0-1 .35-1.8.94-2.44-.1-.23-.41-1.16.09-2.4 0 0 .76-.25 2.5.93A8.7 8.7 0 0 1 12 7.52c.78 0 1.55.1 2.28.3 1.73-1.18 2.49-.93 2.49-.93.5 1.24.19 2.17.09 2.4.58.64.94 1.45.94 2.44 0 3.5-2.13 4.26-4.16 4.49.33.28.62.84.62 1.69v2.5c0 .25.17.53.63.44A9 9 0 0 0 12 3.2Z"
              fill="currentColor"
            />
          </svg>
        </a>

        <a
          class="ml-1 inline-flex min-h-11 items-center gap-[7px] rounded-xs bg-accent px-[13px] text-[0.76rem] font-[750] text-[#190c03] no-underline transition-[background-color,transform] hover:bg-accent-bright active:scale-[0.97] max-md:hidden"
          :href="siteMeta.documentation"
          target="_blank"
          rel="noreferrer"
        >
          文档
          <span aria-hidden="true">↗</span>
        </a>

        <button
          class="hidden size-11 place-items-center rounded-xs border border-transparent text-muted transition-colors hover:border-line hover:bg-surface-hover hover:text-text max-[900px]:grid"
          type="button"
          :aria-expanded="isOpen"
          aria-controls="mobile-navigation"
          :aria-label="isOpen ? '关闭导航菜单' : '打开导航菜单'"
          @click="isOpen = !isOpen"
        >
          <span
            class="mx-auto my-[3.5px] block h-[1.5px] w-[15px] bg-current transition-[opacity,transform]"
            :class="{ 'translate-y-[2.5px] rotate-45': isOpen }"
            aria-hidden="true"
          ></span>
          <span
            class="mx-auto my-[3.5px] block h-[1.5px] w-[15px] bg-current transition-[opacity,transform]"
            :class="{ '-translate-y-[2.5px] -rotate-45': isOpen }"
            aria-hidden="true"
          ></span>
        </button>
      </div>

      <div
        v-if="isOpen"
        id="mobile-navigation"
        class="absolute top-[calc(100%+1px)] right-0 left-0 grid gap-0 rounded-b-md border border-t-0 border-line bg-nav p-2 shadow-soft backdrop-blur-lg"
      >
        <button
          v-for="item in navigation"
          :key="item.target"
          type="button"
          class="flex min-h-11 items-center justify-between rounded-xs px-2.5 py-3 text-[0.86rem] font-[650] text-text-soft hover:bg-surface-hover hover:text-text"
          :class="{ 'bg-surface-hover text-text': activeSection === item.target }"
          @click="navigateTo(item.target)"
        >
          {{ item.label }}
          <span aria-hidden="true">→</span>
        </button>
        <a
          class="flex items-center justify-between rounded-xs px-2.5 py-3 text-[0.86rem] font-[650] text-accent-bright no-underline hover:bg-surface-hover hover:text-text"
          :href="siteMeta.documentation"
          target="_blank"
          rel="noreferrer"
          @click="closeMenu"
        >
          阅读文档
          <span aria-hidden="true">↗</span>
        </a>
      </div>
    </nav>
  </header>
</template>
