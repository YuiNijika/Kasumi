<script setup lang="ts">
import type { CodeLanguage } from '@/data/site'

defineProps<{
  language: CodeLanguage
}>()
</script>

<template>
  <span
    class="language-icon"
    :class="`language-icon--${language}`"
    aria-hidden="true"
    translate="no"
  >
    {{
      language === 'typescript'
        ? 'TS'
        : language === 'cpp'
          ? 'C++'
          : language === 'kotlin'
            ? 'K'
            : '{}'
    }}
  </span>
</template>

<style scoped>
.language-icon {
  display: grid;
  flex: none;
  width: 18px;
  height: 18px;
  color: #fff;
  font-family: var(--font-mono);
  font-size: 0.43rem;
  font-weight: 800;
  letter-spacing: -0.03em;
  line-height: 1;
  border-radius: 4px;
  place-items: center;
}

.language-icon--typescript {
  background: #3178c6;
}

.language-icon--cpp {
  background: #00599c;
}

.language-icon--kotlin {
  background: linear-gradient(135deg, #7f52ff 0 58%, #e44857 58% 78%, #ff8c00 78%);
}

.language-icon--json {
  background: #5e5b55;
}
</style>
