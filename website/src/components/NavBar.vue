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
  <header class="nav-wrap">
    <nav class="nav-shell" aria-label="主要导航">
      <button
        class="brand"
        type="button"
        aria-label="返回 Kasumi 首页"
        @click="scrollToTop"
      >
        <span class="brand__name" translate="no">{{ siteMeta.name }}</span>
      </button>

      <div class="nav-links">
        <button
          v-for="item in navigation"
          :key="item.target"
          type="button"
          class="nav-link"
          :class="{ 'nav-link--active': activeSection === item.target }"
          :aria-current="activeSection === item.target ? 'location' : undefined"
          @click="navigateTo(item.target)"
        >
          {{ item.label }}
        </button>
      </div>

      <div class="nav-actions">
        <button
          class="icon-button"
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
          class="icon-button github-link"
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
          class="nav-cta"
          :href="siteMeta.documentation"
          target="_blank"
          rel="noreferrer"
        >
          文档
          <span aria-hidden="true">↗</span>
        </a>

        <button
          class="icon-button menu-toggle"
          type="button"
          :aria-expanded="isOpen"
          aria-controls="mobile-navigation"
          :aria-label="isOpen ? '关闭导航菜单' : '打开导航菜单'"
          @click="isOpen = !isOpen"
        >
          <span :class="{ 'menu-toggle__line--top': isOpen }" aria-hidden="true"></span>
          <span :class="{ 'menu-toggle__line--bottom': isOpen }" aria-hidden="true"></span>
        </button>
      </div>

      <div
        v-if="isOpen"
        id="mobile-navigation"
        class="mobile-menu"
      >
        <button
          v-for="item in navigation"
          :key="item.target"
          type="button"
          :class="{ 'mobile-menu__item--active': activeSection === item.target }"
          @click="navigateTo(item.target)"
        >
          {{ item.label }}
          <span aria-hidden="true">→</span>
        </button>
        <a
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

<style scoped>
.nav-wrap {
  position: sticky;
  top: 0;
  z-index: 40;
  padding-top: env(safe-area-inset-top);
  background: var(--nav-bg);
  border-bottom: 1px solid var(--border);
  backdrop-filter: blur(16px) saturate(130%);
}

.nav-shell {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: min(calc(100% - 48px), 1240px);
  min-height: var(--nav-height);
  margin-inline: auto;
}

.brand {
  display: inline-flex;
  gap: 9px;
  align-items: center;
  color: var(--text);
}

.brand__mark {
  display: grid;
  width: 27px;
  height: 27px;
  color: #1a0c02;
  font-family: var(--font-display);
  font-size: 0.86rem;
  font-weight: 800;
  background: var(--accent);
  border-radius: 6px;
  place-items: center;
}

.brand__name {
  font-family: var(--font-display);
  font-size: 1.04rem;
  font-weight: 650;
  letter-spacing: -0.025em;
}

.nav-links {
  display: flex;
  gap: 2px;
  align-items: center;
}

.nav-link {
  position: relative;
  padding: 8px 11px;
  color: var(--muted);
  font-size: 0.78rem;
  font-weight: 600;
  transition: color 150ms ease;
}

.nav-link::after {
  position: absolute;
  right: 11px;
  bottom: 3px;
  left: 11px;
  height: 1px;
  content: "";
  background: var(--accent);
  opacity: 0;
  transform: scaleX(0.6);
  transition:
    opacity 150ms ease,
    transform 150ms ease;
}

.nav-link:hover,
.nav-link--active {
  color: var(--text);
}

.nav-link--active::after {
  opacity: 1;
  transform: scaleX(1);
}

.nav-actions {
  display: flex;
  gap: 4px;
  align-items: center;
}

.icon-button {
  display: grid;
  width: 34px;
  height: 34px;
  color: var(--muted);
  border: 1px solid transparent;
  border-radius: var(--radius-xs);
  place-items: center;
  transition:
    color 150ms ease,
    background-color 150ms ease,
    border-color 150ms ease;
}

.icon-button:hover {
  color: var(--text);
  background: var(--surface-hover);
  border-color: var(--border);
}

.nav-cta {
  display: inline-flex;
  gap: 7px;
  align-items: center;
  min-height: 34px;
  padding: 0 13px;
  margin-left: 4px;
  color: #190c03;
  font-size: 0.76rem;
  font-weight: 750;
  text-decoration: none;
  background: var(--accent);
  border-radius: var(--radius-xs);
  transition:
    background-color 150ms ease,
    transform 120ms ease;
}

.nav-cta:hover {
  background: var(--accent-bright);
}

.nav-cta:active {
  transform: scale(0.97);
}

.menu-toggle {
  display: none;
}

.menu-toggle span {
  display: block;
  width: 15px;
  height: 1.5px;
  margin: 3.5px auto;
  background: currentColor;
  transition:
    opacity 150ms ease,
    transform 150ms ease;
}

.menu-toggle__line--top {
  transform: translateY(2.5px) rotate(45deg);
}

.menu-toggle__line--bottom {
  transform: translateY(-2.5px) rotate(-45deg);
}

.mobile-menu {
  position: absolute;
  top: calc(100% + 1px);
  right: 0;
  left: 0;
  display: grid;
  padding: 8px;
  background: var(--nav-bg);
  border: 1px solid var(--border);
  border-top: 0;
  border-radius: 0 0 var(--radius-md) var(--radius-md);
  box-shadow: var(--shadow-soft);
  backdrop-filter: blur(18px);
}

.mobile-menu button,
.mobile-menu a {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 10px;
  color: var(--text-soft);
  font-size: 0.86rem;
  font-weight: 650;
  text-decoration: none;
  border-radius: var(--radius-xs);
}

.mobile-menu button:hover,
.mobile-menu a:hover,
.mobile-menu__item--active {
  color: var(--text);
  background: var(--surface-hover);
}

.mobile-menu a {
  color: var(--accent-bright);
}

@media (max-width: 900px) {
  .nav-links {
    display: none;
  }

  .menu-toggle {
    display: grid;
  }
}

@media (max-width: 820px) {
  .nav-shell {
    width: min(calc(100% - 32px), 1240px);
  }

  .nav-cta {
    display: none;
  }
}

@media (max-width: 420px) {
  .github-link {
    display: none;
  }
}
</style>
