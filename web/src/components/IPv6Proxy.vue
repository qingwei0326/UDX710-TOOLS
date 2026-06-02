<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { t } = useI18n()
const { success, error } = useToast()
const { confirm } = useConfirm()

// Webhook平台模板
const platformTemplates = {
  pushplus: { 
    url: 'http://www.pushplus.plus/send', 
    body: '{"token":"YOUR_TOKEN","title":"IPv6地址更新","content":"IPv6: #{ipv6}\\n访问链接: #{link}\\n时间: #{time}"}', 
    headers: 'Content-Type: application/json' 
  },
  serverchan: { 
    url: 'https://sctapi.ftqq.com/YOUR_KEY.send', 
    body: '{"title":"IPv6地址更新","desp":"IPv6: #{ipv6}\\n访问链接: #{link}\\n时间: #{time}"}', 
    headers: 'Content-Type: application/json' 
  },
  bark: { 
    url: 'https://api.day.app/YOUR_KEY/IPv6地址更新/#{ipv6}', 
    body: '', 
    headers: '' 
  },
  dingtalk: { 
    url: 'https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN', 
    body: '{"msgtype":"text","text":{"content":"IPv6地址更新\\nIPv6: #{ipv6}\\n访问链接: #{link}\\n时间: #{time}"}}', 
    headers: 'Content-Type: application/json' 
  },
  feishu: { 
    url: 'https://open.feishu.cn/open-apis/bot/v2/hook/YOUR_TOKEN', 
    body: '{"msg_type":"text","content":{"text":"IPv6地址更新\\nIPv6: #{ipv6}\\n访问链接: #{link}\\n时间: #{time}"}}', 
    headers: 'Content-Type: application/json' 
  },
  custom: { url: '', body: '{"ipv6":"#{ipv6}","link":"#{link}","time":"#{time}"}', headers: 'Content-Type: application/json' }
}

// 当前选择的平台
const selectedPlatform = ref('custom')

// 应用模板
function applyTemplate(platform) {
  const tpl = platformTemplates[platform]
  if (tpl) {
    config.value.webhook_url = tpl.url
    config.value.webhook_body = tpl.body
    config.value.webhook_headers = tpl.headers
  }
}

// 状态
const loading = ref(false)
const saving = ref(false)
const config = ref({
  enabled: 0,
  auto_start: 0,
  send_enabled: 0,
  send_interval: 60,
  webhook_url: '',
  webhook_body: '',
  webhook_headers: ''
})
const rules = ref([])
const status = ref({
  running: 0,
  rule_count: 0,
  active_count: 0,
  ipv6_addr: '',
  link: ''
})
const showDialog = ref(false)
const isEditing = ref(false)
const showGuide = ref(false)
let statusTimer = null

// 表单
const form = ref({
  id: 0,
  local_port: '',
  ipv6_port: '',
  enabled: 1
})

// API请求头
function getHeaders(json = false) {
  const token = localStorage.getItem('auth_token')
  const headers = {}
  if (token) headers['Authorization'] = `Bearer ${token}`
  if (json) headers['Content-Type'] = 'application/json'
  return headers
}

// 获取配置
async function fetchConfig() {
  try {
    const res = await fetch('/api/ipv6-proxy/config', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      config.value = data.data
    }
  } catch (e) {
    console.error('获取配置失败:', e)
  }
}

// 获取规则列表
async function fetchRules() {
  try {
    const res = await fetch('/api/ipv6-proxy/rules', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      rules.value = data.data
    }
  } catch (e) {
    console.error('获取规则列表失败:', e)
  }
}

// 获取运行状态
async function fetchStatus() {
  try {
    const res = await fetch('/api/ipv6-proxy/status', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      status.value = data.data
    }
  } catch (e) {
    console.error('获取状态失败:', e)
  }
}

// 保存配置
async function saveConfig() {
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/config', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify(config.value)
    })
    if (!res.ok) throw new Error('保存失败')
    success(t('ipv6Proxy.configSaved'))
    return true
  } catch (e) {
    error(e.message)
    return false
  } finally {
    saving.value = false
  }
}

// 切换开机自启动
async function toggleAutoStart() {
  try {
    const res = await fetch('/api/ipv6-proxy/config', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify(config.value)
    })
    if (!res.ok) throw new Error('设置失败')
    success(config.value.auto_start ? t('ipv6Proxy.autoStartEnabled') : t('ipv6Proxy.autoStartDisabled'))
  } catch (e) {
    error(e.message)
    config.value.auto_start = config.value.auto_start ? 0 : 1
  }
}

// 切换定时发送
async function toggleSendEnabled() {
  try {
    const res = await fetch('/api/ipv6-proxy/config', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify(config.value)
    })
    if (!res.ok) throw new Error('设置失败')
    success(config.value.send_enabled ? t('ipv6Proxy.sendEnabled') : t('ipv6Proxy.sendDisabled'))
  } catch (e) {
    error(e.message)
    config.value.send_enabled = config.value.send_enabled ? 0 : 1
  }
}

// 启动服务
async function startService() {
  // 先保存配置
  const saved = await saveConfig()
  if (!saved) return
  
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/start', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('启动失败')
    success(t('ipv6Proxy.startSuccess'))
    await fetchStatus()
  } catch (e) {
    error(t('ipv6Proxy.startFailed') + ': ' + e.message)
  } finally {
    saving.value = false
  }
}

// 停止服务
async function stopService() {
  const ok = await confirm({ 
    title: t('ipv6Proxy.stop'), 
    message: t('ipv6Proxy.confirmStop'),
    danger: true 
  })
  if (!ok) return
  
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/stop', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('停止失败')
    success(t('ipv6Proxy.stopSuccess'))
    await fetchStatus()
  } catch (e) {
    error(t('ipv6Proxy.stopFailed') + ': ' + e.message)
  } finally {
    saving.value = false
  }
}

// 重启服务
async function restartService() {
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/restart', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('重启失败')
    success(t('ipv6Proxy.restartSuccess'))
    await fetchStatus()
  } catch (e) {
    error(e.message)
  } finally {
    saving.value = false
  }
}

// 打开新建对话框
function openNewDialog() {
  isEditing.value = false
  form.value = { id: 0, local_port: '', ipv6_port: '', enabled: 1 }
  showDialog.value = true
}

// 打开编辑对话框
function openEditDialog(rule) {
  isEditing.value = true
  form.value = { ...rule, local_port: String(rule.local_port), ipv6_port: String(rule.ipv6_port) }
  showDialog.value = true
}

// 保存规则
async function saveRule() {
  const localPort = parseInt(form.value.local_port)
  const ipv6Port = parseInt(form.value.ipv6_port)
  
  if (!localPort || localPort < 1 || localPort > 65535) {
    error(t('ipv6Proxy.invalidLocalPort'))
    return
  }
  if (!ipv6Port || ipv6Port < 1 || ipv6Port > 65535) {
    error(t('ipv6Proxy.invalidIpv6Port'))
    return
  }
  
  saving.value = true
  try {
    const url = isEditing.value ? `/api/ipv6-proxy/rules/${form.value.id}` : '/api/ipv6-proxy/rules'
    const method = isEditing.value ? 'PUT' : 'POST'
    const res = await fetch(url, { 
      method, 
      headers: getHeaders(true), 
      body: JSON.stringify({
        local_port: localPort,
        ipv6_port: ipv6Port,
        enabled: form.value.enabled
      })
    })
    if (!res.ok) throw new Error('保存失败')
    success(isEditing.value ? t('ipv6Proxy.ruleUpdated') : t('ipv6Proxy.ruleAdded'))
    showDialog.value = false
    await fetchRules()
  } catch (e) {
    error(e.message)
  } finally {
    saving.value = false
  }
}

// 删除规则
async function deleteRule(rule) {
  const ok = await confirm({ 
    title: t('common.delete'), 
    message: t('ipv6Proxy.confirmDeleteRule'),
    danger: true 
  })
  if (!ok) return
  
  try {
    const res = await fetch(`/api/ipv6-proxy/rules/${rule.id}`, { 
      method: 'DELETE', 
      headers: getHeaders() 
    })
    if (!res.ok) throw new Error('删除失败')
    success(t('ipv6Proxy.ruleDeleted'))
    await fetchRules()
  } catch (e) {
    error(e.message)
  }
}

// 立即发送IPv6
async function sendNow() {
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/send', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('发送失败')
    success(t('ipv6Proxy.sendSuccess'))
  } catch (e) {
    error(t('ipv6Proxy.sendFailed'))
  } finally {
    saving.value = false
  }
}

// 测试发送
async function testSend() {
  saving.value = true
  try {
    const res = await fetch('/api/ipv6-proxy/test', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('测试失败')
    success(t('ipv6Proxy.testSuccess'))
  } catch (e) {
    error(t('ipv6Proxy.testFailed'))
  } finally {
    saving.value = false
  }
}

// 格式化访问链接为 [ipv6]:端口 格式（每个端口一行）
const formattedLinks = computed(() => {
  if (!status.value.ipv6_addr || !rules.value.length) {
    return status.value.ipv6_addr ? [`[${status.value.ipv6_addr}]:port`] : []
  }
  // 每个启用的规则生成一行
  return rules.value
    .filter(r => r.enabled)
    .map(r => `[${status.value.ipv6_addr}]:${r.ipv6_port}`)
})

// 保持向后兼容
const formattedLink = computed(() => formattedLinks.value.join('\n'))

// 复制文本
async function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text)
      success(t('common.copied'))
      return
    } catch (e) { }
  }
  try {
    const textarea = document.createElement('textarea')
    textarea.value = text
    textarea.style.position = 'fixed'
    textarea.style.left = '-9999px'
    document.body.appendChild(textarea)
    textarea.select()
    document.execCommand('copy')
    document.body.removeChild(textarea)
    success(t('common.copied'))
  } catch (e) {
    error(t('common.copyFailed') || '复制失败')
  }
}

// 复制IPv6地址
function copyIpv6() {
  if (status.value.ipv6_addr) {
    copyText(status.value.ipv6_addr)
  }
}

// 复制访问链接
function copyLink() {
  if (formattedLink.value) {
    copyText(formattedLink.value)
  }
}

// 发送日志
const sendLogs = ref([])
const showLogsDialog = ref(false)

async function fetchSendLogs() {
  try {
    const res = await fetch('/api/ipv6-proxy/send-logs?lines=50', {
      headers: getHeaders()
    })
    if (res.ok) {
      const data = await res.json()
      sendLogs.value = data.data || []
    }
  } catch (e) {
    console.warn('获取发送日志失败:', e)
  }
}

function formatLogTime(timestamp) {
  if (!timestamp) return ''
  const date = new Date(timestamp * 1000)
  return date.toLocaleString()
}

// 初始化
onMounted(async () => {
  loading.value = true
  await Promise.all([fetchConfig(), fetchRules(), fetchStatus()])
  loading.value = false
  
  // 定时刷新状态
  statusTimer = setInterval(() => {
    fetchStatus()
  }, 10000)
})

onUnmounted(() => {
  if (statusTimer) {
    clearInterval(statusTimer)
  }
})
</script>

<template>
  <div class="space-y-6">
    <!-- IPv6地址显示 -->
    <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
      <div class="flex items-center justify-between mb-4">
        <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
          <i class="fas fa-globe-americas text-cyan-500 mr-2"></i>
          {{ t('ipv6Proxy.currentIpv6') }}
          <button @click="showGuide = true" class="ml-2 w-6 h-6 rounded-full bg-cyan-100 dark:bg-cyan-500/20 hover:bg-cyan-200 dark:hover:bg-cyan-500/30 flex items-center justify-center transition-colors" title="功能说明">
            <i class="fas fa-question text-cyan-500 text-xs"></i>
          </button>
        </h3>
        <span :class="[
          'px-3 py-1.5 rounded-lg text-xs font-medium flex items-center',
          status.running
            ? 'bg-green-100 dark:bg-green-500/20 text-green-600 dark:text-green-400'
            : 'bg-slate-100 dark:bg-white/10 text-slate-500 dark:text-white/50'
        ]">
          <span :class="['w-2 h-2 rounded-full mr-1.5', status.running ? 'bg-green-500 animate-pulse' : 'bg-slate-400']"></span>
          {{ status.running ? t('ipv6Proxy.running') : t('ipv6Proxy.stopped') }}
        </span>
      </div>

      <div class="space-y-3">
        <!-- 访问链接列表 -->
        <div class="bg-slate-50 dark:bg-white/5 rounded-xl p-4 border border-slate-200 dark:border-white/10">
          <div class="flex items-center justify-between mb-2">
            <p class="text-slate-500 dark:text-white/50 text-xs">{{ t('ipv6Proxy.accessLink') }}</p>
            <button
              v-if="formattedLinks.length > 0"
              @click="copyLink"
              class="px-3 py-1.5 bg-cyan-500 hover:bg-cyan-600 text-white text-xs rounded-lg transition-colors flex items-center"
            >
              <i class="fas fa-copy mr-1.5"></i>
              {{ t('common.copy') }}
            </button>
          </div>
          <div v-if="formattedLinks.length > 0" class="space-y-1">
            <p 
              v-for="(link, idx) in formattedLinks" 
              :key="idx" 
              class="font-mono text-sm text-slate-900 dark:text-white break-all"
            >{{ link }}</p>
          </div>
          <p v-else class="font-mono text-sm text-slate-400 dark:text-white/50">{{ t('ipv6Proxy.noIpv6') }}</p>
          <!-- 发送日志按钮移至底部 -->
          <div class="mt-3 pt-3 border-t border-slate-200 dark:border-white/10">
            <button
              @click="showLogsDialog = true; fetchSendLogs()"
              class="w-full sm:w-auto px-4 py-2 bg-slate-200 dark:bg-white/10 hover:bg-slate-300 dark:hover:bg-white/20 text-slate-600 dark:text-white/70 text-sm rounded-lg transition-colors flex items-center justify-center"
            >
              <i class="fas fa-history mr-2"></i>
              {{ t('ipv6Proxy.sendLogs') || '查看发送日志' }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      <!-- 转发规则 -->
      <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
        <div class="flex items-center justify-between mb-4">
          <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
            <i class="fas fa-random text-violet-500 mr-2"></i>
            {{ t('ipv6Proxy.forwardRules') }}
          </h3>
          <button @click="openNewDialog" class="px-3 py-1.5 bg-violet-500 text-white text-sm rounded-lg hover:bg-violet-600 transition-colors flex items-center">
            <i class="fas fa-plus mr-1.5"></i>
            {{ t('ipv6Proxy.addRule') }}
          </button>
        </div>
        
        <div class="space-y-2 max-h-64 overflow-y-auto">
          <template v-if="rules.length > 0">
            <div
              v-for="rule in rules"
              :key="rule.id"
              class="p-4 rounded-xl bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10"
            >
              <div class="flex items-center justify-between">
                <div class="flex items-center space-x-3">
                  <div :class="['w-9 h-9 rounded-lg flex items-center justify-center', rule.enabled ? 'bg-violet-500' : 'bg-slate-200 dark:bg-white/10']">
                    <i :class="['fas fa-arrows-alt-h', rule.enabled ? 'text-white' : 'text-slate-400']"></i>
                  </div>
                  <div>
                    <p class="text-slate-900 dark:text-white font-medium font-mono">
                      :{{ rule.ipv6_port }} → 127.0.0.1:{{ rule.local_port }}
                    </p>
                    <p class="text-slate-500 dark:text-white/50 text-xs">
                      {{ t('ipv6Proxy.ipv6Port') }}: {{ rule.ipv6_port }} → {{ t('ipv6Proxy.localPort') }}: {{ rule.local_port }}
                    </p>
                  </div>
                </div>
                <div class="flex items-center space-x-2">
                  <button @click="openEditDialog(rule)" class="w-8 h-8 rounded-lg bg-slate-200 dark:bg-white/10 hover:bg-blue-100 dark:hover:bg-blue-500/20 flex items-center justify-center transition-colors">
                    <i class="fas fa-pen text-blue-500 text-xs"></i>
                  </button>
                  <button @click="deleteRule(rule)" class="w-8 h-8 rounded-lg bg-slate-200 dark:bg-white/10 hover:bg-red-100 dark:hover:bg-red-500/20 flex items-center justify-center transition-colors">
                    <i class="fas fa-trash-alt text-red-500 text-xs"></i>
                  </button>
                </div>
              </div>
            </div>
          </template>
          <div v-else class="w-full flex flex-col items-center justify-center py-8">
            <i class="fas fa-inbox text-slate-300 dark:text-white/20 text-3xl mb-3"></i>
            <p class="text-slate-500 dark:text-white/50 text-sm">{{ t('ipv6Proxy.noRules') }}</p>
          </div>
        </div>
      </div>

      <!-- 控制面板 -->
      <div class="space-y-6">
        <!-- 运行状态 -->
        <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
          <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
            <i class="fas fa-info-circle text-cyan-500 mr-2"></i>
            {{ t('ipv6Proxy.status') }}
          </h3>
          
          <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-4">
            <div class="flex items-center space-x-3">
              <div :class="['w-12 h-12 rounded-xl flex items-center justify-center', status.running ? 'bg-green-500' : 'bg-slate-200 dark:bg-white/10']">
                <i :class="['fas fa-power-off text-xl', status.running ? 'text-white' : 'text-slate-400']"></i>
              </div>
              <div>
                <p class="text-slate-900 dark:text-white font-semibold">
                  {{ status.running ? t('ipv6Proxy.running') : t('ipv6Proxy.stopped') }}
                </p>
                <p class="text-slate-500 dark:text-white/50 text-xs">
                  {{ status.rule_count }} {{ t('ipv6Proxy.rulesCount') }}
                </p>
              </div>
            </div>
            <span :class="[
              'px-3 py-1.5 rounded-lg text-sm font-medium flex items-center',
              status.running 
                ? 'bg-green-100 dark:bg-green-500/20 text-green-600 dark:text-green-400' 
                : 'bg-slate-100 dark:bg-white/10 text-slate-500 dark:text-white/50'
            ]">
              <span :class="['w-2 h-2 rounded-full mr-2', status.running ? 'bg-green-500 animate-pulse' : 'bg-slate-400']"></span>
              {{ status.active_count }} {{ t('ipv6Proxy.activeRules') }}
            </span>
          </div>
          
          <!-- 控制按钮 -->
          <div class="grid grid-cols-3 gap-3">
            <button
              @click="startService"
              :disabled="saving || status.running"
              class="py-3 bg-green-500 text-white rounded-xl hover:bg-green-600 disabled:opacity-50 disabled:cursor-not-allowed transition-all flex items-center justify-center space-x-2"
            >
              <i class="fas fa-play"></i>
              <span>{{ t('ipv6Proxy.start') }}</span>
            </button>
            <button
              @click="stopService"
              :disabled="saving || !status.running"
              class="py-3 bg-red-500 text-white rounded-xl hover:bg-red-600 disabled:opacity-50 disabled:cursor-not-allowed transition-all flex items-center justify-center space-x-2"
            >
              <i class="fas fa-stop"></i>
              <span>{{ t('ipv6Proxy.stop') }}</span>
            </button>
            <button
              @click="restartService"
              :disabled="saving"
              class="py-3 bg-indigo-500 text-white rounded-xl hover:bg-indigo-600 disabled:opacity-50 disabled:cursor-not-allowed transition-all flex items-center justify-center space-x-2"
            >
              <i :class="saving ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'"></i>
              <span>{{ t('ipv6Proxy.restart') }}</span>
            </button>
          </div>
        </div>

        <!-- 自启动设置 -->
        <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
          <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl">
            <div class="flex items-center space-x-3">
              <div class="w-10 h-10 rounded-lg bg-green-100 dark:bg-green-500/20 flex items-center justify-center">
                <i class="fas fa-bolt text-green-500"></i>
              </div>
              <div>
                <p class="text-slate-900 dark:text-white font-medium text-sm">{{ t('ipv6Proxy.autoStart') }}</p>
                <p class="text-slate-500 dark:text-white/50 text-xs">{{ t('ipv6Proxy.autoStartDesc') }}</p>
              </div>
            </div>
            <label class="relative cursor-pointer">
              <input type="checkbox" v-model="config.auto_start" :true-value="1" :false-value="0" @change="toggleAutoStart" class="sr-only peer">
              <div class="w-12 h-6 bg-slate-300 dark:bg-white/20 rounded-full peer peer-checked:bg-green-500 transition-all"></div>
              <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-6"></div>
            </label>
          </div>
        </div>
      </div>
    </div>

    <!-- Webhook配置 -->
    <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-bell text-amber-500 mr-2"></i>
        {{ t('ipv6Proxy.webhookConfig') }}
      </h3>
      
      <div class="space-y-4">
        <!-- 启用开关 -->
        <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl">
          <div class="flex items-center space-x-3">
            <div class="w-10 h-10 rounded-lg bg-amber-100 dark:bg-amber-500/20 flex items-center justify-center">
              <i class="fas fa-paper-plane text-amber-500"></i>
            </div>
            <div>
              <p class="text-slate-900 dark:text-white font-medium text-sm">{{ t('ipv6Proxy.sendEnabled') }}</p>
              <p class="text-slate-500 dark:text-white/50 text-xs">{{ t('ipv6Proxy.sendEnabledDesc') }}</p>
            </div>
          </div>
          <label class="relative cursor-pointer">
            <input type="checkbox" v-model="config.send_enabled" :true-value="1" :false-value="0" @change="toggleSendEnabled" class="sr-only peer">
            <div class="w-12 h-6 bg-slate-300 dark:bg-white/20 rounded-full peer peer-checked:bg-amber-500 transition-all"></div>
            <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-6"></div>
          </label>
        </div>
        
        <!-- 发送间隔 -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('ipv6Proxy.sendInterval') }}
          </label>
          <div class="flex items-center space-x-2">
            <input 
              v-model.number="config.send_interval" 
              type="number" 
              min="1"
              max="1440"
              class="flex-1 px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white focus:border-amber-500 outline-none transition-all"
            >
            <span class="text-slate-500 dark:text-white/50">{{ t('common.minutes') }}</span>
          </div>
        </div>
        
        <!-- 平台选择 -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('ipv6Proxy.selectPlatform') }}
          </label>
          <select 
            v-model="selectedPlatform" 
            @change="applyTemplate(selectedPlatform)"
            class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white focus:border-amber-500 outline-none transition-all [&>option]:bg-white [&>option]:dark:bg-slate-800 [&>option]:text-slate-900 [&>option]:dark:text-white"
          >
            <option value="pushplus">PushPlus</option>
            <option value="serverchan">Server酱</option>
            <option value="bark">Bark</option>
            <option value="dingtalk">钉钉机器人</option>
            <option value="feishu">飞书机器人</option>
            <option value="custom">自定义</option>
          </select>
        </div>
        
        <!-- Webhook URL -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('ipv6Proxy.webhookUrl') }}
          </label>
          <input 
            v-model="config.webhook_url" 
            type="text" 
            :placeholder="t('ipv6Proxy.webhookUrlPlaceholder')"
            class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono text-sm focus:border-amber-500 outline-none transition-all"
          >
        </div>
        
        <!-- 请求体 -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('ipv6Proxy.webhookBody') }}
            <span class="text-slate-500 dark:text-white/50 font-normal ml-2">{{ t('ipv6Proxy.webhookBodyTip') }}</span>
          </label>
          <textarea 
            v-model="config.webhook_body" 
            rows="4"
            :placeholder="t('ipv6Proxy.webhookBodyPlaceholder')"
            class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono text-sm focus:border-amber-500 outline-none transition-all resize-none"
          ></textarea>
        </div>
        
        <!-- 请求头 -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('ipv6Proxy.webhookHeaders') }}
          </label>
          <input 
            v-model="config.webhook_headers" 
            type="text" 
            :placeholder="t('ipv6Proxy.webhookHeadersPlaceholder')"
            class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono text-sm focus:border-amber-500 outline-none transition-all"
          >
        </div>
        
        <!-- 操作按钮 -->
        <div class="flex space-x-3">
          <button 
            @click="saveConfig"
            :disabled="saving"
            class="flex-1 py-3 bg-amber-500 text-white rounded-xl hover:bg-amber-600 disabled:opacity-50 transition-all flex items-center justify-center space-x-2"
          >
            <i :class="saving ? 'fas fa-spinner animate-spin' : 'fas fa-save'"></i>
            <span>{{ t('common.save') }}</span>
          </button>
          <button 
            @click="testSend"
            :disabled="saving"
            class="px-6 py-3 bg-violet-500 text-white rounded-xl hover:bg-violet-600 disabled:opacity-50 transition-all flex items-center justify-center space-x-2"
          >
            <i class="fas fa-flask"></i>
            <span>{{ t('ipv6Proxy.test') }}</span>
          </button>
          <button 
            @click="sendNow"
            :disabled="saving"
            class="px-6 py-3 bg-cyan-500 text-white rounded-xl hover:bg-cyan-600 disabled:opacity-50 transition-all flex items-center justify-center space-x-2"
          >
            <i class="fas fa-paper-plane"></i>
            <span>{{ t('ipv6Proxy.sendNow') }}</span>
          </button>
        </div>
      </div>
    </div>

    <!-- 使用说明 -->
    <div class="bg-cyan-50 dark:bg-cyan-500/10 rounded-2xl border border-cyan-200 dark:border-cyan-500/20 p-5">
      <div class="flex items-start space-x-3">
        <div class="w-10 h-10 rounded-lg bg-cyan-100 dark:bg-cyan-500/20 flex items-center justify-center flex-shrink-0">
          <i class="fas fa-lightbulb text-cyan-500"></i>
        </div>
        <div class="space-y-2">
          <p class="text-cyan-800 dark:text-cyan-200 font-medium">{{ t('ipv6Proxy.tips') }}</p>
          <ul class="text-cyan-700 dark:text-cyan-300 text-sm space-y-1">
            <li>• {{ t('ipv6Proxy.tip1') }}</li>
            <li>• {{ t('ipv6Proxy.tip2') }}</li>
            <li>• {{ t('ipv6Proxy.tip3') }}</li>
          </ul>
        </div>
      </div>
    </div>

    <!-- 规则编辑弹窗 -->
    <Teleport to="body">
      <div v-if="showDialog" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showDialog = false"></div>
        <div class="relative w-full max-w-md bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden">
          <!-- 头部 -->
          <div class="px-6 py-4 bg-violet-500 text-white flex items-center justify-between">
            <h3 class="font-semibold">{{ isEditing ? t('ipv6Proxy.editRule') : t('ipv6Proxy.addRule') }}</h3>
            <button @click="showDialog = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>

          <!-- 表单 -->
          <div class="p-6 space-y-4">
            <div>
              <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
                {{ t('ipv6Proxy.localPort') }} <span class="text-red-500">*</span>
              </label>
              <input v-model="form.local_port" type="number" min="1" max="65535" :placeholder="t('ipv6Proxy.localPortPlaceholder')"
                class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white focus:border-violet-500 outline-none transition-all">
            </div>
            
            <div>
              <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
                {{ t('ipv6Proxy.ipv6Port') }} <span class="text-red-500">*</span>
              </label>
              <input v-model="form.ipv6_port" type="number" min="1" max="65535" :placeholder="t('ipv6Proxy.ipv6PortPlaceholder')"
                class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white focus:border-violet-500 outline-none transition-all">
            </div>
            
            <!-- 启用开关 -->
            <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl">
              <span class="text-slate-700 dark:text-white/80 font-medium">{{ t('ipv6Proxy.enabled') }}</span>
              <label class="relative cursor-pointer">
                <input type="checkbox" v-model="form.enabled" :true-value="1" :false-value="0" class="sr-only peer">
                <div class="w-12 h-6 bg-slate-300 dark:bg-white/20 rounded-full peer peer-checked:bg-violet-500 transition-all"></div>
                <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-6"></div>
              </label>
            </div>
          </div>
          
          <!-- 底部按钮 -->
          <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 flex justify-end space-x-3">
            <button @click="showDialog = false" class="px-5 py-2.5 rounded-xl border border-slate-200 dark:border-white/10 text-slate-700 dark:text-white hover:bg-slate-100 dark:hover:bg-white/10 transition-colors">
              {{ t('common.cancel') }}
            </button>
            <button @click="saveRule" :disabled="saving" class="px-5 py-2.5 rounded-xl bg-violet-500 text-white hover:bg-violet-600 disabled:opacity-50 transition-colors flex items-center">
              <i v-if="saving" class="fas fa-spinner animate-spin mr-2"></i>
              {{ t('common.save') }}
            </button>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 功能说明弹窗 -->
    <Teleport to="body">
      <div v-if="showGuide" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showGuide = false"></div>
        <div class="relative w-full max-w-2xl max-h-[85vh] bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden flex flex-col">
          <!-- 头部 -->
          <div class="px-6 py-4 bg-gradient-to-r from-cyan-500 to-blue-500 text-white flex items-center justify-between flex-shrink-0">
            <h3 class="font-semibold flex items-center">
              <i class="fas fa-book-open mr-2"></i>
              {{ t('ipv6Proxy.guideTitle') }}
            </h3>
            <button @click="showGuide = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>

          <!-- 内容区域 -->
          <div class="flex-1 overflow-y-auto p-6 space-y-6">
            <!-- 功能介绍 -->
            <div class="space-y-3">
              <h4 class="text-slate-900 dark:text-white font-semibold text-lg">{{ t('ipv6Proxy.guideIntroTitle') }}</h4>
              <p class="text-slate-600 dark:text-white/70 leading-relaxed">{{ t('ipv6Proxy.guideIntroDesc') }}</p>
            </div>

            <!-- 适用场景 -->
            <div class="space-y-3">
              <h4 class="text-slate-900 dark:text-white font-semibold text-lg">{{ t('ipv6Proxy.guideUseCaseTitle') }}</h4>
              <div class="grid grid-cols-1 sm:grid-cols-2 gap-2">
                <div class="flex items-center space-x-2 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <i class="fas fa-desktop text-cyan-500"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideUseCase1') }}</span>
                </div>
                <div class="flex items-center space-x-2 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <i class="fas fa-globe text-cyan-500"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideUseCase2') }}</span>
                </div>
                <div class="flex items-center space-x-2 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <i class="fas fa-server text-cyan-500"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideUseCase3') }}</span>
                </div>
                <div class="flex items-center space-x-2 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <i class="fas fa-video text-cyan-500"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideUseCase4') }}</span>
                </div>
              </div>
            </div>

            <!-- 使用前提 -->
            <div class="space-y-3">
              <h4 class="text-slate-900 dark:text-white font-semibold text-lg">{{ t('ipv6Proxy.guideRequireTitle') }}</h4>
              <p class="text-slate-600 dark:text-white/70 text-sm">{{ t('ipv6Proxy.guideRequireDesc') }}</p>
              <ul class="space-y-2">
                <li class="flex items-start space-x-2">
                  <i class="fas fa-check-circle text-green-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideRequire1') }}</span>
                </li>
                <li class="flex items-start space-x-2">
                  <i class="fas fa-check-circle text-green-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideRequire2') }}</span>
                </li>
                <li class="flex items-start space-x-2">
                  <i class="fas fa-check-circle text-green-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideRequire3') }}</span>
                </li>
              </ul>
              <div class="p-3 bg-amber-50 dark:bg-amber-500/10 rounded-xl border border-amber-200 dark:border-amber-500/20">
                <p class="text-amber-700 dark:text-amber-300 text-sm">{{ t('ipv6Proxy.guideRequireTip') }}</p>
              </div>
            </div>

            <!-- 使用方法 -->
            <div class="space-y-3">
              <h4 class="text-slate-900 dark:text-white font-semibold text-lg">{{ t('ipv6Proxy.guideHowToTitle') }}</h4>
              <div class="space-y-2">
                <div class="flex items-start space-x-3 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <span class="w-6 h-6 rounded-full bg-cyan-500 text-white text-xs flex items-center justify-center flex-shrink-0">1</span>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideHowTo1') }}</span>
                </div>
                <div class="flex items-start space-x-3 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <span class="w-6 h-6 rounded-full bg-cyan-500 text-white text-xs flex items-center justify-center flex-shrink-0">2</span>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideHowTo2') }}</span>
                </div>
                <div class="flex items-start space-x-3 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <span class="w-6 h-6 rounded-full bg-cyan-500 text-white text-xs flex items-center justify-center flex-shrink-0">3</span>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideHowTo3') }}</span>
                </div>
                <div class="flex items-start space-x-3 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
                  <span class="w-6 h-6 rounded-full bg-cyan-500 text-white text-xs flex items-center justify-center flex-shrink-0">4</span>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideHowTo4') }}</span>
                </div>
              </div>
            </div>

            <!-- 安全提示 -->
            <div class="space-y-3">
              <h4 class="text-slate-900 dark:text-white font-semibold text-lg">{{ t('ipv6Proxy.guideSecurityTitle') }}</h4>
              <p class="text-slate-600 dark:text-white/70 text-sm">{{ t('ipv6Proxy.guideSecurityDesc') }}</p>
              <ul class="space-y-2">
                <li class="flex items-start space-x-2">
                  <i class="fas fa-shield-alt text-red-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideSecurity1') }}</span>
                </li>
                <li class="flex items-start space-x-2">
                  <i class="fas fa-shield-alt text-red-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideSecurity2') }}</span>
                </li>
                <li class="flex items-start space-x-2">
                  <i class="fas fa-shield-alt text-red-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideSecurity3') }}</span>
                </li>
                <li class="flex items-start space-x-2">
                  <i class="fas fa-shield-alt text-red-500 mt-0.5"></i>
                  <span class="text-slate-700 dark:text-white/80 text-sm">{{ t('ipv6Proxy.guideSecurity4') }}</span>
                </li>
              </ul>
              <div class="p-3 bg-red-50 dark:bg-red-500/10 rounded-xl border border-red-200 dark:border-red-500/20">
                <p class="text-red-700 dark:text-red-300 text-sm font-medium">{{ t('ipv6Proxy.guideSecurityWarn') }}</p>
              </div>
            </div>
          </div>

          <!-- 底部按钮 -->
          <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 flex justify-end flex-shrink-0">
            <button @click="showGuide = false" class="px-6 py-2.5 rounded-xl bg-cyan-500 text-white hover:bg-cyan-600 transition-colors flex items-center">
              <i class="fas fa-check mr-2"></i>
              {{ t('ipv6Proxy.guideGotIt') }}
            </button>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 发送日志对话框 -->
    <Teleport to="body">
      <div v-if="showLogsDialog" class="fixed inset-0 z-50 flex items-center justify-center" @click.self="showLogsDialog = false">
        <div class="absolute inset-0 bg-black/60 backdrop-blur-sm"></div>
        <div class="relative max-w-2xl w-full mx-4 bg-white dark:bg-slate-800 rounded-2xl shadow-2xl max-h-[80vh] flex flex-col">
          <!-- 标题 -->
          <div class="px-6 py-5 border-b border-slate-200 dark:border-white/10 flex items-center justify-between flex-shrink-0">
            <h2 class="text-xl font-bold text-slate-900 dark:text-white flex items-center">
              <i class="fas fa-history text-cyan-500 mr-3"></i>
              {{ t('ipv6Proxy.sendLogsTitle') || 'IPv6 发送日志' }}
            </h2>
            <button @click="showLogsDialog = false" class="w-8 h-8 rounded-lg bg-slate-100 dark:bg-white/10 hover:bg-slate-200 dark:hover:bg-white/20 flex items-center justify-center transition-colors">
              <i class="fas fa-times text-slate-500 dark:text-white/50"></i>
            </button>
          </div>

          <!-- 内容 -->
          <div class="flex-1 overflow-y-auto p-4 sm:p-6 space-y-3">
            <div v-if="sendLogs.length === 0" class="text-center py-8">
              <i class="fas fa-inbox text-slate-300 dark:text-white/20 text-4xl mb-4"></i>
              <p class="text-slate-500 dark:text-white/50">{{ t('ipv6Proxy.noSendLogs') || '暂无发送记录' }}</p>
            </div>
            <div 
              v-for="log in sendLogs" 
              :key="log.id" 
              class="p-3 sm:p-4 rounded-xl bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10"
            >
              <!-- 头部: IPv6和状态 -->
              <div class="flex flex-wrap items-center justify-between gap-2 mb-2">
                <span class="font-mono text-xs sm:text-sm text-slate-900 dark:text-white break-all">{{ log.ipv6 }}</span>
                <span :class="['px-2 py-0.5 rounded text-xs flex-shrink-0', log.result ? 'bg-green-100 dark:bg-green-500/20 text-green-600 dark:text-green-400' : 'bg-red-100 dark:bg-red-500/20 text-red-600 dark:text-red-400']">
                  {{ log.result ? (t('ipv6Proxy.sendSuccess') || '已发送') : (t('ipv6Proxy.sendFailed') || '失败') }}
                </span>
              </div>
              <!-- 发送内容: 可展开 -->
              <details class="mb-2">
                <summary class="text-xs text-slate-500 dark:text-white/50 cursor-pointer hover:text-slate-700 dark:hover:text-white/70">
                  <span class="ml-1">{{ t('ipv6Proxy.sendContent') || '发送内容' }}</span>
                </summary>
                <pre class="mt-2 p-2 text-xs bg-slate-100 dark:bg-white/5 rounded-lg overflow-x-auto whitespace-pre-wrap break-all text-slate-700 dark:text-white/70">{{ log.content }}</pre>
              </details>
              <!-- 返回结果: 可展开 -->
              <details v-if="log.response" class="mb-2">
                <summary class="text-xs text-slate-500 dark:text-white/50 cursor-pointer hover:text-slate-700 dark:hover:text-white/70">
                  <span class="ml-1">{{ t('ipv6Proxy.sendResponse') || '返回结果' }}</span>
                </summary>
                <pre class="mt-2 p-2 text-xs bg-slate-100 dark:bg-white/5 rounded-lg overflow-x-auto whitespace-pre-wrap break-all text-slate-700 dark:text-white/70">{{ log.response }}</pre>
              </details>
              <!-- 时间 -->
              <p class="text-xs text-slate-400 dark:text-white/30">{{ formatLogTime(log.created_at) }}</p>
            </div>
          </div>

          <!-- 底部按钮 -->
          <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 flex justify-between items-center flex-shrink-0">
            <button @click="fetchSendLogs" class="px-4 py-2 rounded-lg bg-slate-200 dark:bg-white/10 text-slate-600 dark:text-white/70 hover:bg-slate-300 dark:hover:bg-white/20 transition-colors flex items-center text-sm">
              <i class="fas fa-sync-alt mr-2"></i>
              {{ t('common.refresh') || '刷新' }}
            </button>
            <button @click="showLogsDialog = false" class="px-6 py-2.5 rounded-xl bg-cyan-500 text-white hover:bg-cyan-600 transition-colors flex items-center">
              <i class="fas fa-check mr-2"></i>
              {{ t('common.close') || '关闭' }}
            </button>
          </div>
        </div>
      </div>
    </Teleport>
  </div>
</template>
