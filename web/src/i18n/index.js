import { createI18n } from 'vue-i18n'
import zhCN from './locales/zh-CN'
import enUS from './locales/en-US'

// 从localStorage获取语言偏好，默认中文
const getLocale = () => {
  const saved = localStorage.getItem('locale')
  if (saved && ['zh-CN', 'en-US'].includes(saved)) {
    return saved
  }
  return 'zh-CN'
}

const i18n = createI18n({
  legacy: false, // 使用Composition API
  locale: getLocale(),
  fallbackLocale: 'zh-CN',
  messages: {
    'zh-CN': zhCN,
    'en-US': enUS
  },
  globalInjection: true // 注入$t到所有组件
})

// 切换语言并保存到localStorage
export function setLocale(locale) {
  if (['zh-CN', 'en-US'].includes(locale)) {
    i18n.global.locale.value = locale
    localStorage.setItem('locale', locale)
  }
}

// 获取当前语言
export function getLocaleValue() {
  return i18n.global.locale.value
}

export default i18n
