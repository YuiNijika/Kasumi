import { computed, onMounted, ref } from 'vue'

type Theme = 'dark' | 'light'

const STORAGE_KEY = 'kasumi-theme'
const THEME_COLORS: Record<Theme, string> = {
  dark: '#080808',
  light: '#f7f5f0',
}

function readTheme(): Theme {
  return document.documentElement.dataset.theme === 'light' ? 'light' : 'dark'
}

function applyTheme(theme: Theme) {
  document.documentElement.dataset.theme = theme
  document.documentElement.style.colorScheme = theme
  document
    .getElementById('theme-color')
    ?.setAttribute('content', THEME_COLORS[theme])
}

export function useTheme() {
  const theme = ref<Theme>(readTheme())
  const isDark = computed(() => theme.value === 'dark')

  function toggleTheme() {
    theme.value = theme.value === 'dark' ? 'light' : 'dark'
    applyTheme(theme.value)

    try {
      localStorage.setItem(STORAGE_KEY, theme.value)
    } catch {
      // Private browsing can reject storage, while the active theme still applies
    }
  }

  onMounted(() => applyTheme(theme.value))

  return {
    isDark,
    toggleTheme,
  }
}
