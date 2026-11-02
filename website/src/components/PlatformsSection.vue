<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { platforms } from '@/data/site'
</script>

<template>
  <section
    class="section-block section-block--muted"
    data-section="platforms"
    aria-labelledby="platforms-title"
  >
    <div class="section-shell">
      <div data-reveal>
        <SectionHeading
          eyebrow="平台支持"
          title="同一套页面，两种原生运行环境"
          intro="共享调用接口和协议契约，窗口、权限、生命周期与系统能力分别由对应平台实现。"
          title-id="platforms-title"
        />
      </div>

      <div class="platform-grid">
        <article
          v-for="platform in platforms"
          :key="platform.id"
          class="platform-card"
          data-reveal
        >
          <div class="platform-card__top">
            <span class="platform-card__mark" translate="no">
              {{ platform.shortName }}
            </span>
            <span class="platform-card__runtime" translate="no">
              {{ platform.runtime }}
            </span>
          </div>

          <h3>{{ platform.name }}</h3>
          <p>{{ platform.description }}</p>

          <ul :aria-label="`${platform.name} 能力`">
            <li v-for="feature in platform.features" :key="feature">
              <span aria-hidden="true">✓</span>
              {{ feature }}
            </li>
          </ul>

          <a
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

<style scoped>
.platform-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 14px;
}

.platform-card {
  display: flex;
  min-height: 380px;
  padding: 28px;
  flex-direction: column;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
  transition:
    background-color 160ms ease,
    border-color 160ms ease;
}

.platform-card:hover {
  background: var(--surface-hover);
  border-color: var(--border-strong);
}

.platform-card__top {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.platform-card__mark {
  display: grid;
  min-width: 44px;
  height: 30px;
  padding-inline: 8px;
  color: var(--accent-bright);
  font-family: var(--font-mono);
  font-size: 0.65rem;
  font-weight: 700;
  background: var(--accent-wash);
  border: 1px solid var(--accent-line);
  border-radius: var(--radius-xs);
  place-items: center;
}

.platform-card__runtime {
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.62rem;
}

h3 {
  margin: 42px 0 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: clamp(2rem, 3.4vw, 3rem);
  font-weight: 650;
  letter-spacing: -0.045em;
  line-height: 1;
}

p {
  max-width: 46ch;
  margin: 15px 0 0;
  color: var(--muted);
  font-size: 0.88rem;
  line-height: 1.72;
}

ul {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 9px 14px;
  padding: 20px 0 0;
  margin: auto 0 0;
  list-style: none;
  border-top: 1px solid var(--border);
}

li {
  display: flex;
  gap: 7px;
  align-items: center;
  min-width: 0;
  color: var(--text-soft);
  font-size: 0.77rem;
}

li span {
  color: var(--accent);
  font-size: 0.66rem;
}

.platform-card > a {
  display: inline-flex;
  gap: 6px;
  align-items: center;
  align-self: flex-start;
  margin-top: 22px;
  color: var(--accent-bright);
  font-size: 0.75rem;
  font-weight: 700;
  text-decoration: none;
}

.platform-card > a:hover {
  color: var(--text);
}

@media (max-width: 820px) {
  .platform-grid {
    grid-template-columns: 1fr;
  }

  .platform-card {
    min-height: 340px;
  }
}

@media (max-width: 560px) {
  .platform-card {
    padding: 22px;
  }

  ul {
    grid-template-columns: 1fr;
  }
}
</style>
