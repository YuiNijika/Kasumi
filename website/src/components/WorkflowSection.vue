<script setup lang="ts">
import CopyCommand from '@/components/CopyCommand.vue'
import SectionHeading from '@/components/SectionHeading.vue'
import { templates, workflowSteps } from '@/data/site'
</script>

<template>
  <section
    class="section-block workflow-section"
    data-section="workflow"
    aria-labelledby="workflow-title"
  >
    <div class="section-shell workflow-layout">
      <div class="workflow-intro" data-reveal>
        <SectionHeading
          eyebrow="开发方式"
          title="从创建到发布，只记一组命令"
          intro="不用先理解两个原生工程。选择熟悉的模板，开发和构建继续使用同一个包管理器。需要处理平台差异时，再进入对应的 Windows 或 Android 工程。"
          title-id="workflow-title"
        />

        <CopyCommand command="pnpm create kasumi" />

        <div class="template-list" aria-label="可用模板">
          <span>支持模板</span>
          <ul>
            <li v-for="name in templates" :key="name" translate="no">
              {{ name }}
            </li>
          </ul>
        </div>
      </div>

      <div class="workflow-panel" data-reveal>
        <div class="workflow-panel__bar">
          <span class="window-dots" aria-hidden="true">
            <i></i>
            <i></i>
            <i></i>
          </span>
          <span>项目终端</span>
        </div>

        <ol>
          <li v-for="step in workflowSteps" :key="step.step">
            <div class="workflow-step__copy">
              <span class="workflow-step__number">{{ step.step }}</span>
              <div>
                <h3>{{ step.title }}</h3>
                <p>{{ step.description }}</p>
              </div>
            </div>
            <code translate="no">
              <span aria-hidden="true">$</span>
              {{ step.command }}
            </code>
          </li>
        </ol>
      </div>
    </div>
  </section>
</template>

<style scoped>
.workflow-section {
  background: color-mix(in srgb, var(--bg-subtle) 68%, transparent);
}

.workflow-layout {
  display: grid;
  grid-template-columns: minmax(0, 0.82fr) minmax(500px, 1.18fr);
  gap: clamp(42px, 7vw, 90px);
  align-items: start;
}

.workflow-intro {
  position: sticky;
  top: calc(var(--nav-height) + 36px);
}

.workflow-intro :deep(.section-heading) {
  margin-bottom: 26px;
}

.template-list {
  display: flex;
  gap: 16px;
  align-items: flex-start;
  margin-top: 26px;
}

.template-list > span {
  flex: none;
  padding-top: 4px;
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.62rem;
}

.template-list ul {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  padding: 0;
  margin: 0;
  list-style: none;
}

.template-list li {
  padding: 3px 8px;
  color: var(--text-soft);
  font-family: var(--font-mono);
  font-size: 0.63rem;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-xs);
}

.workflow-panel {
  overflow: hidden;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
}

.workflow-panel__bar {
  display: flex;
  gap: 12px;
  align-items: center;
  min-height: 42px;
  padding: 0 14px;
  color: var(--muted-strong);
  font-family: var(--font-mono);
  font-size: 0.61rem;
  border-bottom: 1px solid var(--border);
}

.window-dots {
  display: inline-flex;
  gap: 5px;
}

.window-dots i {
  width: 6px;
  height: 6px;
  background: var(--border-strong);
  border-radius: 50%;
}

.window-dots i:first-child {
  background: var(--accent);
}

.workflow-panel ol {
  padding: 0;
  margin: 0;
  list-style: none;
}

.workflow-panel li {
  padding: 24px;
}

.workflow-panel li + li {
  border-top: 1px solid var(--border);
}

.workflow-step__copy {
  display: flex;
  gap: 15px;
  align-items: flex-start;
}

.workflow-step__number {
  flex: none;
  padding-top: 2px;
  color: var(--accent);
  font-family: var(--font-mono);
  font-size: 0.63rem;
}

h3 {
  margin: 0;
  color: var(--text);
  font-family: var(--font-display);
  font-size: 1rem;
  font-weight: 650;
}

p {
  margin: 5px 0 0;
  color: var(--muted);
  font-size: 0.77rem;
  line-height: 1.65;
}

code {
  display: block;
  padding: 10px 12px;
  margin: 14px 0 0 27px;
  overflow-x: auto;
  color: var(--text-soft);
  font-family: var(--font-mono);
  font-size: 0.72rem;
  white-space: nowrap;
  background: var(--bg-subtle);
  border: 1px solid var(--border);
  border-radius: var(--radius-xs);
}

code span {
  margin-right: 7px;
  color: var(--accent);
}

@media (max-width: 980px) {
  .workflow-layout {
    grid-template-columns: 1fr;
  }

  .workflow-intro {
    position: static;
  }
}

@media (max-width: 560px) {
  .template-list {
    flex-direction: column;
  }

  .workflow-panel li {
    padding: 20px;
  }

  code {
    margin-left: 0;
  }
}
</style>
