<script setup lang="ts">
import CodeShowcase from '@/components/CodeShowcase.vue'
import CopyCommand from '@/components/CopyCommand.vue'
import { hero, heroCodeExamples } from '@/data/site'
</script>

<template>
  <section class="hero" aria-labelledby="hero-title">
    <div class="hero__grid section-shell">
      <div class="hero__copy" data-reveal>
        <p class="eyebrow">{{ hero.eyebrow }}</p>
        <h1 id="hero-title">
          <span>{{ hero.titleLead }}</span>
          <span class="hero__accent">{{ hero.titleAccent }}</span>
        </h1>
        <p class="hero__description">{{ hero.description }}</p>

        <div class="hero__actions">
          <CopyCommand :command="hero.command" />
          <a
            class="hero__primary"
            :href="hero.primaryAction.href"
            target="_blank"
            rel="noreferrer"
          >
            {{ hero.primaryAction.label }}
            <span aria-hidden="true">↗</span>
          </a>
          <a
            class="hero__secondary"
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

        <dl class="hero__metrics">
          <div v-for="metric in hero.metrics" :key="metric.label">
            <dt>{{ metric.label }}</dt>
            <dd>{{ metric.value }}</dd>
          </div>
        </dl>
      </div>

      <div class="hero__visual" data-reveal>
        <CodeShowcase :examples="heroCodeExamples" />
        <p class="hero__visual-note">
          <span aria-hidden="true"></span>
          前端与两端宿主使用同一套能力边界
        </p>
      </div>
    </div>
  </section>
</template>

<style scoped>
.hero {
  position: relative;
  padding: 88px 0 96px;
  overflow: hidden;
}

.hero::before {
  position: absolute;
  inset: 0;
  pointer-events: none;
  content: "";
  background-image:
    linear-gradient(to right, var(--grid-line) 1px, transparent 1px),
    linear-gradient(to bottom, var(--grid-line) 1px, transparent 1px);
  background-size: 56px 56px;
  mask-image: linear-gradient(to bottom, black, transparent 82%);
}

.hero::after {
  position: absolute;
  top: -24rem;
  left: 50%;
  width: 82rem;
  height: 52rem;
  pointer-events: none;
  content: "";
  background: radial-gradient(ellipse, var(--page-glow), transparent 64%);
  transform: translateX(-50%);
}

.hero__grid {
  position: relative;
  z-index: 1;
  display: grid;
  grid-template-columns: minmax(0, 0.9fr) minmax(480px, 1.1fr);
  gap: clamp(42px, 7vw, 88px);
  align-items: center;
}

.hero__copy {
  min-width: 0;
  max-width: 565px;
}

h1 {
  max-width: 100%;
  margin: 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: clamp(2.7rem, 5.4vw, 3.8rem);
  font-weight: 680;
  letter-spacing: -0.045em;
  line-height: 1.02;
  text-wrap: balance;
}

h1 span {
  display: block;
}

.hero__accent {
  margin-top: 4px;
  color: var(--accent);
  font-size: inherit;
  font-weight: inherit;
  letter-spacing: inherit;
  line-height: inherit;
}

.hero__description {
  max-width: 50ch;
  margin: 24px 0 0;
  color: var(--muted);
  font-size: clamp(0.98rem, 1.35vw, 1.06rem);
  line-height: 1.8;
  text-wrap: pretty;
}

.hero__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: center;
  margin-top: 30px;
}

.hero__primary,
.hero__secondary {
  display: inline-flex;
  gap: 8px;
  align-items: center;
  min-height: 54px;
  padding: 0 17px;
  color: var(--text-soft);
  font-size: 0.79rem;
  font-weight: 700;
  text-decoration: none;
  border: 1px solid var(--border-strong);
  border-radius: var(--radius-sm);
  transition:
    color 150ms ease,
    background-color 150ms ease,
    border-color 150ms ease;
}

.hero__primary:hover,
.hero__secondary:hover {
  color: var(--text);
  background: var(--surface-hover);
  border-color: var(--accent-line);
}

.hero__secondary svg {
  flex: none;
}

.hero__metrics {
  display: flex;
  margin: 38px 0 0;
}

.hero__metrics div {
  min-width: 104px;
  padding-right: 24px;
}

.hero__metrics div + div {
  padding-left: 24px;
  border-left: 1px solid var(--border);
}

.hero__metrics dt {
  color: var(--muted-strong);
  font-size: 0.68rem;
  font-weight: 650;
}

.hero__metrics dd {
  margin: 3px 0 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: 1.45rem;
  font-weight: 650;
  font-variant-numeric: tabular-nums;
  line-height: 1;
}

.hero__visual {
  min-width: 0;
}

.hero__visual-note {
  display: flex;
  gap: 8px;
  align-items: center;
  justify-content: flex-end;
  margin: 13px 0 0;
  color: var(--muted-strong);
  font-size: 0.69rem;
}

.hero__visual-note span {
  width: 6px;
  height: 6px;
  background: var(--accent);
  border-radius: 50%;
}

@media (max-width: 980px) {
  .hero {
    padding-top: 72px;
  }

  .hero__grid {
    grid-template-columns: 1fr;
    gap: 48px;
  }

  .hero__copy {
    max-width: 720px;
  }

  .hero__visual {
    width: min(100%, 720px);
  }

  .hero__visual-note {
    justify-content: flex-start;
  }
}

@media (max-width: 560px) {
  .hero {
    padding: 56px 0 68px;
  }

  h1 {
    max-width: none;
    font-size: clamp(2.45rem, 12vw, 3.55rem);
    letter-spacing: -0.05em;
  }

  .hero__actions {
    align-items: stretch;
    flex-direction: column;
  }

  .hero__primary {
    justify-content: center;
  }

  .hero__secondary {
    justify-content: center;
  }

  .hero__metrics {
    justify-content: space-between;
  }

  .hero__metrics div {
    min-width: 0;
    padding-right: 13px;
  }

  .hero__metrics div + div {
    padding-left: 13px;
  }
}
</style>
