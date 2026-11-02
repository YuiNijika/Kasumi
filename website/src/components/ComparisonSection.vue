<script setup lang="ts">
import SectionHeading from '@/components/SectionHeading.vue'
import { comparisonRows } from '@/data/site'
</script>

<template>
  <section
    class="section-block"
    data-section="comparison"
    aria-labelledby="comparison-title"
  >
    <div class="section-shell">
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
        class="table-frame"
        tabindex="0"
        role="region"
        aria-label="Kasumi、Tauri 2 与 Electron 能力对比表"
        data-reveal
      >
        <table>
          <caption>
            Kasumi、Tauri 2 与 Electron 的能力对比
          </caption>
          <thead>
            <tr>
              <th scope="col">维度</th>
              <th scope="col">Kasumi</th>
              <th scope="col">Tauri 2</th>
              <th scope="col">Electron</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="row in comparisonRows" :key="row.criterion">
              <th scope="row">{{ row.criterion }}</th>
              <td>{{ row.kasumi }}</td>
              <td>{{ row.tauri }}</td>
              <td>{{ row.electron }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </section>
</template>

<style scoped>
.table-frame {
  overflow-x: auto;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
  overscroll-behavior-inline: contain;
}

.table-frame:focus-visible {
  outline-offset: 6px;
}

table {
  width: 100%;
  min-width: 760px;
  border-collapse: collapse;
}

caption {
  position: absolute;
  width: 1px;
  height: 1px;
  margin: -1px;
  overflow: hidden;
  white-space: nowrap;
  clip: rect(0 0 0 0);
  clip-path: inset(50%);
}

th,
td {
  padding: 16px 21px;
  text-align: left;
  border-bottom: 1px solid var(--border);
}

thead th {
  color: var(--muted);
  font-family: var(--font-mono);
  font-size: 0.63rem;
  font-weight: 650;
  background: var(--bg-subtle);
  text-transform: uppercase;
}

thead th:nth-child(2) {
  color: var(--accent-bright);
}

tbody tr:last-child th,
tbody tr:last-child td {
  border-bottom: 0;
}

tbody tr:hover {
  background: var(--surface-hover);
}

tbody th {
  width: 18%;
  color: var(--text-soft);
  font-size: 0.77rem;
  font-weight: 650;
}

td {
  width: 27%;
  color: var(--muted);
  font-size: 0.77rem;
}

td:nth-child(2) {
  color: var(--text);
  font-weight: 600;
  background: var(--accent-wash);
}

td:nth-child(2)::before {
  display: inline-block;
  width: 5px;
  height: 5px;
  margin-right: 8px;
  vertical-align: 1px;
  content: "";
  background: var(--accent);
  border-radius: 50%;
}

@media (max-width: 560px) {
  th,
  td {
    padding: 14px 16px;
  }
}
</style>
