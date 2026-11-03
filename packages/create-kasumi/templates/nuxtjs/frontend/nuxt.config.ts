export default defineNuxtConfig({
  ssr: false,
  compatibilityDate: '2026-11-01',
  telemetry: false,
  devtools: {
    enabled: false
  },
  css: ['~/assets/css/main.css'],
  app: {
    head: {
      htmlAttrs: {
        lang: 'zh-CN'
      },
      title: '__APP_TITLE__',
      meta: [
        {
          name: 'viewport',
          content: 'width=device-width, initial-scale=1, viewport-fit=cover'
        },
        {
          name: 'theme-color',
          content: '#f2ece3'
        }
      ]
    }
  }
})
