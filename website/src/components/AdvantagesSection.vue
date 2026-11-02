<script setup lang="ts">
import FeatureIcon from '@/components/FeatureIcon.vue'
import SectionHeading from '@/components/SectionHeading.vue'
import {
  advantages,
  capabilityOverview,
} from '@/data/site'

const leadAdvantage = advantages[0]
const capabilityItems = advantages.slice(1)
</script>

<template>
  <section
    class="section-block"
    data-section="features"
    aria-labelledby="features-title"
  >
    <div class="section-shell">
      <div data-reveal>
        <SectionHeading
          eyebrow="核心能力"
          title="需要的能力，都有清楚边界"
          intro="把跨平台共用的部分收进统一接口，把真正依赖操作系统的工作留给原生插件。"
          title-id="features-title"
        />
      </div>

      <div class="capability-layout">
        <article
          v-if="leadAdvantage"
          class="capability-lead"
          data-reveal
        >
          <p class="capability-lead__label">{{ capabilityOverview.label }}</p>
          <h3>{{ leadAdvantage.title }}</h3>
          <p class="capability-lead__copy">{{ leadAdvantage.description }}</p>

          <div
            class="capability-route"
            aria-label="Web 界面通过 Kasumi API 连接 Windows 与 Android"
          >
            <div class="capability-node">
              <span>01</span>
              <strong>{{ capabilityOverview.web.label }}</strong>
              <small>{{ capabilityOverview.web.detail }}</small>
            </div>

            <span class="capability-arrow" aria-hidden="true"></span>

            <div class="capability-node capability-node--bridge">
              <span>02</span>
              <strong>{{ capabilityOverview.bridge.label }}</strong>
              <small>{{ capabilityOverview.bridge.detail }}</small>
            </div>

            <span class="capability-arrow" aria-hidden="true"></span>

            <div class="platform-stack">
              <div
                v-for="platform in capabilityOverview.platforms"
                :key="platform.label"
                class="capability-node capability-node--platform"
              >
                <strong>{{ platform.label }}</strong>
                <small>{{ platform.detail }}</small>
              </div>
            </div>
          </div>
        </article>

        <div class="capability-list">
          <article
            v-for="advantage in capabilityItems"
            :key="advantage.title"
            class="capability-item"
            data-reveal
          >
            <span class="capability-item__icon">
              <FeatureIcon :name="advantage.icon" />
            </span>
            <div>
              <h3>{{ advantage.title }}</h3>
              <p>{{ advantage.description }}</p>
            </div>
          </article>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.capability-layout {
  display: grid;
  grid-template-columns: minmax(0, 1.08fr) minmax(380px, 0.92fr);
  gap: 18px;
  align-items: stretch;
}

.capability-lead {
  position: relative;
  display: flex;
  min-height: 520px;
  padding: clamp(26px, 4vw, 40px);
  overflow: hidden;
  flex-direction: column;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
}

.capability-lead::before {
  position: absolute;
  inset: 0;
  pointer-events: none;
  content: "";
  background-image:
    linear-gradient(to right, var(--grid-line) 1px, transparent 1px),
    linear-gradient(to bottom, var(--grid-line) 1px, transparent 1px);
  background-size: 40px 40px;
  mask-image: linear-gradient(to bottom, black, transparent 78%);
}

.capability-lead > * {
  position: relative;
  z-index: 1;
}

.capability-lead__label {
  display: inline-flex;
  gap: 8px;
  align-items: center;
  width: fit-content;
  margin: 0;
  color: var(--accent-bright);
  font-family: var(--font-mono);
  font-size: 0.65rem;
  font-weight: 650;
}

.capability-lead__label::before {
  width: 18px;
  height: 1px;
  content: "";
  background: currentColor;
}

.capability-lead > h3 {
  max-width: 11ch;
  margin: 48px 0 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: clamp(2rem, 3.6vw, 3.2rem);
  font-weight: 650;
  letter-spacing: -0.045em;
  line-height: 1.05;
  text-wrap: balance;
}

.capability-lead__copy {
  max-width: 44ch;
  margin: 15px 0 0;
  color: var(--muted);
  font-size: 0.84rem;
  line-height: 1.7;
}

.capability-route {
  display: flex;
  gap: 10px;
  align-items: center;
  margin-top: auto;
  padding-top: 42px;
}

.capability-node {
  display: flex;
  min-width: 0;
  min-height: 88px;
  padding: 13px 14px;
  flex-direction: column;
  justify-content: center;
  background: color-mix(in srgb, var(--bg) 76%, transparent);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
}

.capability-node > span {
  margin-bottom: 7px;
  color: var(--accent);
  font-family: var(--font-mono);
  font-size: 0.54rem;
}

.capability-node strong {
  color: var(--text);
  font-family: var(--font-display);
  font-size: 0.79rem;
  font-weight: 650;
  white-space: nowrap;
}

.capability-node small {
  margin-top: 3px;
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.52rem;
  white-space: nowrap;
}

.capability-node--bridge {
  border-color: var(--accent-line);
}

.capability-arrow {
  position: relative;
  flex: none;
  width: 24px;
  height: 1px;
  background: var(--border-strong);
}

.capability-arrow::after {
  position: absolute;
  top: -3px;
  right: 0;
  width: 6px;
  height: 6px;
  content: "";
  border-top: 1px solid var(--muted-strong);
  border-right: 1px solid var(--muted-strong);
  transform: rotate(45deg);
}

.platform-stack {
  display: grid;
  flex: 1;
  gap: 8px;
  min-width: 0;
}

.capability-node--platform {
  min-height: 0;
  flex: 1;
  padding-block: 10px;
}

.capability-list {
  border-top: 1px solid var(--border);
}

.capability-item {
  display: grid;
  grid-template-columns: 34px minmax(0, 1fr);
  gap: 15px;
  padding: 19px 16px;
  border-bottom: 1px solid var(--border);
  transition: background-color 150ms ease;
}

.capability-item:hover {
  background: var(--surface);
}

.capability-item__icon {
  display: grid;
  width: 34px;
  height: 34px;
  color: var(--accent-bright);
  border: 1px solid var(--border);
  border-radius: var(--radius-xs);
  place-items: center;
}

.capability-item h3 {
  margin: 1px 0 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: 1rem;
  font-weight: 650;
}

.capability-item p {
  margin: 6px 0 0;
  color: var(--muted);
  font-size: 0.76rem;
  line-height: 1.62;
}

@media (max-width: 980px) {
  .capability-layout {
    grid-template-columns: 1fr;
  }

  .capability-lead {
    min-height: 480px;
  }
}

@media (max-width: 620px) {
  .capability-lead {
    min-height: 0;
    padding: 24px 20px;
  }

  .capability-lead > h3 {
    margin-top: 36px;
    font-size: clamp(1.85rem, 9vw, 2.55rem);
  }

  .capability-route {
    align-items: stretch;
    flex-direction: column;
    padding-top: 32px;
  }

  .capability-arrow {
    width: 1px;
    height: 20px;
    margin-left: 20px;
  }

  .capability-arrow::after {
    top: auto;
    right: -3px;
    bottom: 0;
    transform: rotate(135deg);
  }

  .platform-stack {
    grid-template-columns: 1fr 1fr;
  }

  .capability-item {
    padding-inline: 8px;
  }
}

@media (max-width: 420px) {
  .platform-stack {
    grid-template-columns: 1fr;
  }
}
</style>
