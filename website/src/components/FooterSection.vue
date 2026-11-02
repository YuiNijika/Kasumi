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
  <footer class="site-footer">
    <div class="footer-cta-wrap">
      <div class="section-shell footer-cta">
        <div class="footer-cta__copy">
          <p class="eyebrow">现在开始</p>
          <h2>用熟悉的 Web 界面，交付下一款应用。</h2>
        </div>
        <div class="footer-cta__actions">
          <CopyCommand :command="hero.command" />
          <a :href="siteMeta.documentation" target="_blank" rel="noreferrer">
            阅读开发文档
            <span aria-hidden="true">↗</span>
          </a>
        </div>
      </div>
    </div>

    <div class="section-shell footer-body">
      <div class="footer-main">
        <div class="footer-brand">
          <button type="button" aria-label="返回页面顶部" @click="scrollToTop">
            <span aria-hidden="true">K</span>
            <strong translate="no">{{ siteMeta.name }}</strong>
          </button>
          <p>{{ siteMeta.description }}</p>
        </div>

        <nav aria-label="页脚导航">
          <p>资源</p>
          <a
            v-for="link in footerLinks"
            :key="link.href"
            :href="link.href"
            target="_blank"
            rel="noreferrer"
          >
            {{ link.label }}
            <span aria-hidden="true">↗</span>
          </a>
        </nav>

        <div class="footer-meta">
          <p>平台</p>
          <span>Windows</span>
          <span>Android</span>
          <span>MIT License</span>
        </div>
      </div>

      <div class="footer-bottom">
        <p>© 2026 Kasumi</p>
        <a :href="siteMeta.repository" target="_blank" rel="noreferrer">
          GitHub
          <span aria-hidden="true">↗</span>
        </a>
      </div>
    </div>
  </footer>
</template>

<style scoped>
.site-footer {
  padding-bottom: calc(30px + env(safe-area-inset-bottom));
  border-top: 1px solid var(--border);
}

.footer-cta-wrap {
  background: linear-gradient(90deg, var(--accent-wash), transparent 64%);
  border-bottom: 1px solid var(--border);
}

.footer-cta {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  gap: 44px;
  align-items: center;
  padding-block: 62px;
}

.footer-cta .eyebrow {
  margin-bottom: 12px;
}

h2 {
  max-width: 22ch;
  margin: 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: clamp(2rem, 4vw, 3.2rem);
  font-weight: 650;
  letter-spacing: -0.045em;
  line-height: 1.06;
  text-wrap: balance;
}

.footer-cta__copy {
  min-width: 0;
}

.footer-cta__actions {
  display: flex;
  gap: 10px;
  align-items: flex-start;
  flex-direction: column;
}

.footer-cta__actions > a {
  display: inline-flex;
  gap: 6px;
  align-items: center;
  color: var(--accent-bright);
  font-size: 0.76rem;
  font-weight: 700;
  text-decoration: none;
}

.footer-cta__actions > a:hover {
  color: var(--text);
}

.footer-main {
  display: grid;
  grid-template-columns: 1.4fr 0.7fr 0.7fr;
  gap: 54px;
  padding: 42px 0;
}

.footer-body {
  padding-top: 0;
}

.footer-brand > button {
  display: inline-flex;
  gap: 9px;
  align-items: center;
  color: var(--text);
}

.footer-brand > button span {
  display: grid;
  width: 27px;
  height: 27px;
  color: #190c03;
  font-family: var(--font-display);
  font-size: 0.86rem;
  font-weight: 800;
  background: var(--accent);
  border-radius: 6px;
  place-items: center;
}

.footer-brand strong {
  font-family: var(--font-display);
  font-size: 1.1rem;
  font-weight: 650;
  letter-spacing: -0.025em;
}

.footer-brand p {
  max-width: 38ch;
  margin: 13px 0 0;
  color: var(--muted-strong);
  font-size: 0.76rem;
}

nav,
.footer-meta {
  display: flex;
  flex-direction: column;
}

.footer-main nav > p,
.footer-meta > p {
  margin: 2px 0 12px;
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.6rem;
  font-weight: 650;
}

nav a,
.footer-meta span {
  width: fit-content;
  margin-bottom: 8px;
  color: var(--muted);
  font-size: 0.75rem;
  text-decoration: none;
}

nav a {
  display: inline-flex;
  gap: 5px;
  align-items: center;
}

nav a:hover {
  color: var(--accent-bright);
}

.footer-bottom {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding-top: 20px;
  color: var(--muted-strong);
  font-size: 0.65rem;
  border-top: 1px solid var(--border);
}

.footer-bottom p {
  margin: 0;
}

.footer-bottom a {
  display: inline-flex;
  gap: 5px;
  align-items: center;
  color: var(--muted);
  text-decoration: none;
}

.footer-bottom a:hover {
  color: var(--accent-bright);
}

@media (max-width: 820px) {
  .footer-cta {
    grid-template-columns: 1fr;
    gap: 26px;
  }

  .footer-main {
    grid-template-columns: 1fr 1fr;
  }

  .footer-brand {
    grid-column: 1 / -1;
  }
}

@media (max-width: 560px) {
  .site-footer {
    padding-bottom: calc(24px + env(safe-area-inset-bottom));
  }

  .footer-main {
    grid-template-columns: 1fr;
    gap: 28px;
  }

  .footer-brand {
    grid-column: auto;
  }

  .footer-bottom {
    gap: 8px;
    align-items: flex-start;
    flex-direction: column;
  }
}
</style>
