<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { techStack } from '@/data/site'
</script>

<template>
  <section
    class="section-block"
    data-section="stack"
    aria-labelledby="tech-stack-title"
  >
    <div class="section-shell">
      <div data-reveal>
        <SectionHeading
          eyebrow="技术栈"
          title="每层只处理自己的问题"
          intro="前端不接触真实平台对象，原生宿主不承载页面逻辑，协议把两端稳定地连接起来。"
          title-id="tech-stack-title"
          centered
        />
      </div>

      <div class="stack-grid">
        <article
          v-for="item in techStack"
          :key="item.name"
          class="stack-card"
          data-reveal
        >
          <div class="stack-card__head">
            <span class="stack-card__mark" translate="no">{{ item.mark }}</span>
            <span class="stack-card__role" translate="no">{{ item.role }}</span>
          </div>
          <h3 translate="no">{{ item.name }}</h3>
          <p>{{ item.description }}</p>
        </article>
      </div>
    </div>
  </section>
</template>

<style scoped>
.stack-grid {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 12px;
}

.stack-card {
  min-height: 235px;
  padding: 22px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
  transition:
    background-color 160ms ease,
    border-color 160ms ease;
}

.stack-card:hover {
  background: var(--surface-hover);
  border-color: var(--border-strong);
}

.stack-card__head {
  display: flex;
  gap: 12px;
  align-items: center;
  justify-content: space-between;
}

.stack-card__mark {
  display: grid;
  width: 38px;
  height: 38px;
  color: var(--accent-bright);
  font-family: var(--font-mono);
  font-size: 0.68rem;
  font-weight: 700;
  background: var(--accent-wash);
  border: 1px solid var(--accent-line);
  border-radius: var(--radius-xs);
  place-items: center;
}

.stack-card__role {
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.57rem;
  text-align: right;
}

h3 {
  margin: 40px 0 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: 1.2rem;
  font-weight: 650;
  letter-spacing: -0.02em;
}

p {
  margin: 9px 0 0;
  color: var(--muted);
  font-size: 0.77rem;
  line-height: 1.7;
}

@media (max-width: 980px) {
  .stack-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 560px) {
  .stack-grid {
    grid-template-columns: 1fr;
  }

  .stack-card {
    min-height: 205px;
  }
}
</style>
