<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'

const { t } = useI18n()
const { success, error } = useToast()
const { confirm } = useConfirm()

// 状态
const config = ref({ server_addr: 'b1.xfrp.net', server_port: 7000, token: '', auto_start: 0, enabled: 0 })
const proxies = ref([])
const status = ref({ running: 0, pid: -1, proxy_count: 0 })
const logs = ref('')
const showDialog = ref(false)
const isEditing = ref(false)
const showGuide = ref(false)
const downloading = ref(false)
const downloadStatus = ref(0)  // 0=未开始 1=下载中 2=成功 3=失败
const downloadLog = ref('')
let statusTimer = null
let downloadTimer = null

// 表单
const form = ref({
  id: 0,
  name: '',
  type: 'tcp',
  local_ip: '127.0.0.1',
  local_port: '',
  remote_port: '',
  enabled: 1
})

// 常用端口预设
const presets = [
  { name: 'SSH', type: 'tcp', local_port: 22, remote_port: 10022 },
  { name: 'Web', type: 'tcp', local_port: 80, remote_port: 10080 },
  { name: 'WebHTTPS', type: 'tcp', local_port: 443, remote_port: 10443 },
  { name: 'RDP', type: 'tcp', local_port: 3389, remote_port: 13389 },
  { name: 'FTP', type: 'tcp', local_port: 21, remote_port: 10021 },
]

function applyPreset(preset) {
  form.value.name = preset.name
  form.value.type = preset.type
  form.value.local_port = preset.local_port
  form.value.remote_port = preset.remote_port
}

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
    const res = await fetch('/api/frpc/config', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      config.value = data.data
    }
  } catch (e) {
    console.error('获取配置失败:', e)
  }
}

// 获取隧道列表
async function fetchProxies() {
  try {
    const res = await fetch('/api/frpc/proxies', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      proxies.value = data.data
    }
  } catch (e) {
    console.error('获取隧道列表失败:', e)
  }
}

// 获取运行状态
async function fetchStatus() {
  try {
    const res = await fetch('/api/frpc/status', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      status.value = data.data
      if (data.data.binary_exists) {
        downloadStatus.value = 2  // 已安装
      }
    }
  } catch (e) {
    console.error('获取状态失败:', e)
  }
}

// 获取日志
async function fetchLogs() {
  try {
    const res = await fetch('/api/frpc/logs?lines=50', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      logs.value = data.data.logs || ''
    }
  } catch (e) {
    console.error('获取日志失败:', e)
  }
}

// 保存配置
async function saveConfig() {
  try {
    const res = await fetch('/api/frpc/config', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify(config.value)
    })
    if (!res.ok) throw new Error('保存失败')
    success(t('rathole.configSaved'))
  } catch (e) {
    error(e.message)
  }
}

// 切换自启动
async function toggleAutoStart() {
  config.value.auto_start = config.value.auto_start ? 0 : 1
  await saveConfig()
}

// 启动服务
async function startService() {
  await saveConfig()
  try {
    const res = await fetch('/api/frpc/start', {
      method: 'POST',
      headers: getHeaders()
    })
    const data = await res.json()
    if (data.status === 'ok') {
      success(t('rathole.startSuccess'))
      await fetchStatus()
    } else {
      error(data.message || t('rathole.startFailed'))
    }
  } catch (e) {
    error(e.message)
  }
}

// 停止服务
async function stopService() {
  const confirmed = await confirm({
    title: t('rathole.stopService'),
    message: t('rathole.confirmStop'),
    danger: true
  })
  if (!confirmed) return

  try {
    const res = await fetch('/api/frpc/stop', {
      method: 'POST',
      headers: getHeaders()
    })
    const data = await res.json()
    if (data.status === 'ok') {
      success(t('rathole.stopSuccess'))
      await fetchStatus()
    } else {
      error(data.message || t('rathole.stopFailed'))
    }
  } catch (e) {
    error(e.message)
  }
}

// 保存并重启
async function saveAndRestart() {
  await saveConfig()
  if (status.value.running) {
    await fetch('/api/frpc/stop', { method: 'POST', headers: getHeaders() })
    await new Promise(r => setTimeout(r, 500))
  }
  await startService()
}

// 打开新增对话框
function openNewDialog() {
  isEditing.value = false
  form.value = { id: 0, name: '', type: 'tcp', local_ip: '127.0.0.1', local_port: '', remote_port: '', enabled: 1 }
  showDialog.value = true
}

// 打开编辑对话框
function openEditDialog(proxy) {
  isEditing.value = true
  form.value = { ...proxy }
  showDialog.value = true
}

// 保存隧道
async function saveProxy() {
  if (!form.value.name || !form.value.local_port || !form.value.remote_port) {
    error(t('rathole.fillRequired'))
    return
  }

  try {
    const url = isEditing.value ? `/api/frpc/proxies/${form.value.id}` : '/api/frpc/proxies'
    const method = isEditing.value ? 'PUT' : 'POST'
    const res = await fetch(url, {
      method,
      headers: getHeaders(true),
      body: JSON.stringify(form.value)
    })
    if (!res.ok) throw new Error('保存失败')
    success(t('rathole.serviceSaved'))
    showDialog.value = false
    await fetchProxies()
  } catch (e) {
    error(e.message)
  }
}

// 删除隧道
async function deleteProxy(proxy) {
  const confirmed = await confirm({
    title: t('rathole.deleteService'),
    message: t('rathole.confirmDelete', { name: proxy.name }),
    danger: true
  })
  if (!confirmed) return

  try {
    const res = await fetch(`/api/frpc/proxies/${proxy.id}`, {
      method: 'DELETE',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('删除失败')
    success(t('rathole.serviceDeleted'))
    await fetchProxies()
  } catch (e) {
    error(e.message)
  }
}

// 下载 frpc 客户端
async function downloadFrpc() {
  try {
    const res = await fetch('/api/frpc/download', {
      method: 'POST',
      headers: getHeaders()
    })
    const data = await res.json()
    if (data.status === 'ok') {
      downloading.value = true
      downloadStatus.value = 1
      downloadLog.value = '开始下载...\n'
      pollDownloadStatus()
    } else {
      error(data.message || '下载启动失败')
    }
  } catch (e) {
    error(e.message)
  }
}

async function pollDownloadStatus() {
  if (downloadTimer) clearInterval(downloadTimer)
  downloadTimer = setInterval(async () => {
    try {
      const res = await fetch('/api/frpc/download/status', { headers: getHeaders() })
      const data = await res.json()
      downloadStatus.value = data.status
      downloadLog.value = data.log || ''
      if (data.status === 2) {
        downloading.value = false
        success('frpc 客户端安装成功！')
        clearInterval(downloadTimer)
        await fetchStatus()
      } else if (data.status === 3) {
        downloading.value = false
        error('下载失败，请检查网络连接')
        clearInterval(downloadTimer)
      }
    } catch (e) {
      console.error('轮询下载状态失败:', e)
    }
  }, 2000)
}

// 复制文本
async function copyText(text) {
  try {
    await navigator.clipboard.writeText(text)
    success(t('rathole.copied'))
  } catch {
    const ta = document.createElement('textarea')
    ta.value = text
    document.body.appendChild(ta)
    ta.select()
    document.execCommand('copy')
    document.body.removeChild(ta)
    success(t('rathole.copied'))
  }
}

onMounted(async () => {
  await Promise.all([fetchConfig(), fetchProxies(), fetchStatus()])
  await fetchLogs()
  statusTimer = setInterval(fetchStatus, 10000)
})

onUnmounted(() => {
  if (statusTimer) clearInterval(statusTimer)
  if (downloadTimer) clearInterval(downloadTimer)
})
</script>

<template>
  <div class="space-y-4 sm:space-y-6">
    <!-- 服务器配置 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-server text-pink-500 mr-2"></i>
        {{ t('rathole.serverConfig') }}
      </h3>

      <div class="space-y-4">
        <div class="grid grid-cols-1 sm:grid-cols-3 gap-4">
          <div>
            <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">{{ t('rathole.serverAddr') }}</label>
            <input v-model="config.server_addr" type="text"
              class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-pink-400 outline-none transition-all text-sm"
              placeholder="b1.xfrp.net">
          </div>
          <div>
            <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">{{ t('rathole.serverPort') }}</label>
            <input v-model.number="config.server_port" type="number"
              class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-pink-400 outline-none transition-all text-sm"
              placeholder="7000">
          </div>
          <div>
            <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">Token</label>
            <input v-model="config.token" type="password"
              class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-pink-400 outline-none transition-all text-sm"
              placeholder="Access Token">
          </div>
        </div>

        <div class="grid grid-cols-1 gap-4">
          <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl">
            <div class="flex items-center space-x-3">
              <div class="w-10 h-10 rounded-lg bg-green-100 dark:bg-green-500/20 flex items-center justify-center">
                <i class="fas fa-bolt text-green-500"></i>
              </div>
              <div>
                <p class="text-slate-900 dark:text-white font-medium text-sm">{{ t('rathole.autoStart') }}</p>
                <p class="text-slate-500 dark:text-white/50 text-xs">{{ t('rathole.autoStartDesc') }}</p>
              </div>
            </div>
            <label class="relative cursor-pointer">
              <input type="checkbox" :checked="config.auto_start" @click="toggleAutoStart" class="sr-only peer">
              <div class="w-14 h-7 bg-slate-200 dark:bg-white/20 rounded-full peer peer-checked:bg-pink-500 transition-colors"></div>
              <div class="absolute top-0.5 left-0.5 w-6 h-6 bg-white rounded-full shadow transition-transform peer-checked:translate-x-7"></div>
            </label>
          </div>
        </div>
      </div>
    </div>

    <!-- 转发服务 + 运行状态 两栏布局 -->
    <div class="grid grid-cols-1 lg:grid-cols-3 gap-4 sm:gap-6">
      <!-- 转发服务 (左栏) -->
      <div class="lg:col-span-2 rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
        <div class="flex items-center justify-between mb-4">
          <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
            <i class="fas fa-list text-pink-500 mr-2"></i>
            {{ t('rathole.serviceList') }}
          </h3>
          <button @click="openNewDialog"
            class="px-3 py-1.5 bg-pink-500/10 hover:bg-pink-500/20 text-pink-600 dark:text-pink-400 rounded-lg text-xs font-medium transition-all">
            <i class="fas fa-plus mr-1"></i>{{ t('rathole.addService') }}
          </button>
        </div>

        <!-- 快捷预设 -->
        <div v-if="proxies.length === 0" class="mb-4 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
          <p class="text-slate-500 dark:text-white/50 text-xs mb-2">{{ t('rathole.quickPresets') }}</p>
          <div class="flex flex-wrap gap-2">
            <button v-for="p in presets" :key="p.name" @click="openNewDialog(); applyPreset(p)"
              class="px-2 py-1 bg-white dark:bg-white/10 border border-slate-200 dark:border-white/10 rounded-lg text-xs text-slate-600 dark:text-white/60 hover:border-pink-400 hover:text-pink-500 transition-all">
              {{ p.name }} ({{ p.local_port }}→{{ p.remote_port }})
            </button>
          </div>
        </div>

        <div v-if="proxies.length === 0" class="text-center py-8 text-slate-400 dark:text-white/30">
          <i class="fas fa-network-wired text-3xl mb-3"></i>
          <p class="text-sm">{{ t('rathole.noServices') }}</p>
        </div>

        <div v-else class="space-y-2">
          <div v-for="proxy in proxies" :key="proxy.id"
            class="flex items-center justify-between p-3 bg-slate-50 dark:bg-white/5 rounded-xl hover:bg-slate-100 dark:hover:bg-white/10 transition-all">
            <div class="flex items-center space-x-3 min-w-0">
              <div class="w-2 h-2 rounded-full flex-shrink-0"
                :class="proxy.enabled ? 'bg-green-500' : 'bg-slate-300 dark:bg-white/20'"></div>
              <div class="min-w-0">
                <p class="text-slate-900 dark:text-white font-medium text-sm truncate">{{ proxy.name }}</p>
                <p class="text-slate-500 dark:text-white/50 text-xs truncate">
                  {{ proxy.type.toUpperCase() }} {{ proxy.local_ip }}:{{ proxy.local_port }} → :{{ proxy.remote_port }}
                </p>
              </div>
            </div>
            <div class="flex items-center space-x-1 flex-shrink-0">
              <button @click="openEditDialog(proxy)" class="w-8 h-8 rounded-lg hover:bg-slate-200 dark:hover:bg-white/10 text-slate-400 dark:text-white/40 flex items-center justify-center transition-all">
                <i class="fas fa-edit text-xs"></i>
              </button>
              <button @click="deleteProxy(proxy)" class="w-8 h-8 rounded-lg hover:bg-red-100 dark:hover:bg-red-500/10 text-slate-400 dark:text-white/40 hover:text-red-500 flex items-center justify-center transition-all">
                <i class="fas fa-trash text-xs"></i>
              </button>
            </div>
          </div>
        </div>
      </div>

      <!-- 运行状态 (右栏) -->
      <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
        <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
          <i class="fas fa-signal text-pink-500 mr-2"></i>
          {{ t('rathole.status') }}
        </h3>

        <div class="flex items-center justify-between mb-4 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
          <div class="flex items-center space-x-2">
            <div class="w-3 h-3 rounded-full" :class="status.running ? 'bg-green-500 animate-pulse' : 'bg-slate-300 dark:bg-white/20'"></div>
            <span class="text-slate-900 dark:text-white font-medium text-sm">
              {{ status.running ? t('rathole.running') : t('rathole.stopped') }}
            </span>
          </div>
          <span class="text-slate-400 dark:text-white/30 text-xs">
            {{ status.proxy_count }} {{ t('rathole.activeServices') }}
          </span>
        </div>

        <div class="space-y-2">
          <button v-if="!status.running" @click="startService"
            class="w-full py-2.5 bg-green-500 hover:bg-green-600 text-white rounded-xl text-sm font-medium transition-all">
            <i class="fas fa-play mr-1"></i>{{ t('rathole.start') }}
          </button>
          <button v-if="status.running" @click="stopService"
            class="w-full py-2.5 bg-red-500 hover:bg-red-600 text-white rounded-xl text-sm font-medium transition-all">
            <i class="fas fa-stop mr-1"></i>{{ t('rathole.stop') }}
          </button>
          <button @click="saveAndRestart"
            class="w-full py-2.5 bg-amber-500 hover:bg-amber-600 text-white rounded-xl text-sm font-medium transition-all">
            <i class="fas fa-sync-alt mr-1"></i>{{ t('rathole.restart') }}
          </button>
          <button v-if="!downloading && downloadStatus !== 2" @click="downloadFrpc"
            class="w-full py-2.5 bg-pink-500 hover:bg-pink-600 text-white rounded-xl text-sm font-medium transition-all">
            <i class="fas fa-download mr-1"></i>{{ t('rathole.downloadFrpc') }}
          </button>
          <div v-if="downloading" class="w-full py-2.5 bg-pink-500/20 text-pink-400 rounded-xl text-sm font-medium text-center">
            <i class="fas fa-spinner animate-spin mr-1"></i>{{ t('rathole.downloading') }}
          </div>
          <div v-if="downloadStatus === 2 && !downloading" class="w-full py-2.5 bg-green-500/20 text-green-400 rounded-xl text-sm font-medium text-center">
            <i class="fas fa-check mr-1"></i>{{ t('rathole.installed') }}
          </div>
          <div v-if="downloadStatus === 3 && !downloading" class="w-full py-2.5 bg-red-500/20 text-red-400 rounded-xl text-sm font-medium text-center">
            <i class="fas fa-times mr-1"></i>{{ t('rathole.downloadFailed') }}
          </div>
        </div>

        <!-- 日志 -->
        <div class="mt-4">
          <div class="flex items-center justify-between mb-2">
            <span class="text-slate-900 dark:text-white font-medium text-sm">{{ t('rathole.logs') }}</span>
            <div class="flex items-center space-x-1">
              <button @click="showGuide = true" class="w-7 h-7 rounded-lg hover:bg-slate-200 dark:hover:bg-white/10 text-slate-400 dark:text-white/40 flex items-center justify-center transition-all" :title="t('rathole.guideTitle')">
                <i class="fas fa-question-circle text-xs"></i>
              </button>
              <button @click="fetchLogs" class="w-7 h-7 rounded-lg hover:bg-slate-200 dark:hover:bg-white/10 text-slate-400 dark:text-white/40 flex items-center justify-center transition-all">
                <i class="fas fa-sync-alt text-xs"></i>
              </button>
            </div>
          </div>
          <div class="bg-slate-900 dark:bg-black/50 rounded-xl p-3 max-h-40 overflow-y-auto font-mono text-xs text-green-400 whitespace-pre-wrap">{{ logs || 'No logs...' }}</div>
        </div>
      </div>
    </div>

    <!-- 新增/编辑对话框 -->
    <Teleport to="body">
      <Transition name="modal">
        <div v-if="showDialog" class="fixed inset-0 z-50 flex items-center justify-center p-4">
          <div class="absolute inset-0 bg-black/60 backdrop-blur-sm" @click="showDialog = false"></div>
          <div class="relative w-full max-w-md bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden">
            <div class="px-6 py-4 bg-pink-500 text-white">
              <h3 class="font-bold text-lg">{{ isEditing ? t('rathole.editService') : t('rathole.addService') }}</h3>
            </div>
            <div class="p-6 space-y-4">
              <div>
                <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.serviceName') }}</label>
                <input v-model="form.name" type="text" class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-sm dark:text-white" placeholder="SSH / Web / RDP">
              </div>
              <div class="grid grid-cols-2 gap-3">
                <div>
                  <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.serviceType') || '类型' }}</label>
                  <select v-model="form.type" class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-sm dark:text-white">
                    <option value="tcp">TCP</option>
                    <option value="udp">UDP</option>
                  </select>
                </div>
                <div>
                  <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.localIp') || '本地IP' }}</label>
                  <input v-model="form.local_ip" type="text" class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-sm dark:text-white" placeholder="127.0.0.1">
                </div>
              </div>
              <div class="grid grid-cols-2 gap-3">
                <div>
                  <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.localPort') }}</label>
                  <input v-model.number="form.local_port" type="number" class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-sm dark:text-white" placeholder="22">
                </div>
                <div>
                  <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.remotePort') }}</label>
                  <input v-model.number="form.remote_port" type="number" class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-sm dark:text-white" placeholder="10022">
                </div>
              </div>
            </div>
            <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 border-t border-slate-200 dark:border-white/10 flex justify-end space-x-3">
              <button @click="showDialog = false" class="px-4 py-2 rounded-xl border border-slate-200 dark:border-white/10 text-slate-600 dark:text-white/60 text-sm hover:bg-slate-100 dark:hover:bg-white/10 transition-all">
                {{ t('common.cancel') }}
              </button>
              <button @click="saveProxy" class="px-4 py-2 rounded-xl bg-pink-500 text-white text-sm hover:bg-pink-600 transition-all">
                {{ t('common.confirm') }}
              </button>
            </div>
          </div>
        </div>
      </Transition>
    </Teleport>
    <!-- 使用指南弹窗 -->
    <Teleport to="body">
      <div v-show="showGuide" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showGuide = false"></div>
        <div class="relative w-full max-w-2xl max-h-[80vh] bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden flex flex-col">
          <div class="px-6 py-4 bg-pink-500 text-white flex items-center justify-between">
            <h3 class="font-semibold">Sakura Frp {{ t('rathole.guideTitle') }}</h3>
            <button @click="showGuide = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>
          <div class="p-6 overflow-y-auto space-y-4 text-slate-700 dark:text-white/80 text-sm">

            <!-- 简介 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white">{{ t('rathole.frpcGuideIntro') }}</h4>
            <p>{{ t('rathole.frpcGuideIntroDesc') }}</p>
            <div class="bg-pink-50 dark:bg-pink-500/10 border border-pink-200 dark:border-pink-500/20 rounded-xl p-4">
              <p class="text-pink-700 dark:text-pink-300">💡 {{ t('rathole.frpcGuideAdvantage') }}</p>
            </div>

            <!-- 步骤1: 注册 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">Step 1: {{ t('rathole.frpcGuideStep1') }}</h4>
            <ol class="text-sm space-y-1 pl-4 list-decimal">
              <li>{{ t('rathole.frpcGuideStep1a') }}</li>
              <li>{{ t('rathole.frpcGuideStep1b') }}</li>
              <li>{{ t('rathole.frpcGuideStep1c') }}</li>
            </ol>
            <a href="https://www.xfrp.net/" target="_blank" class="inline-flex items-center px-4 py-2 bg-pink-100 dark:bg-pink-500/20 rounded-xl text-pink-600 dark:text-pink-400 text-sm hover:bg-pink-200 dark:hover:bg-pink-500/30">
              <i class="fas fa-external-link-alt mr-2"></i>{{ t('rathole.registerAccount') }}
            </a>

            <!-- 步骤2: 获取Token -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">Step 2: {{ t('rathole.frpcGuideStep2') }}</h4>
            <ol class="text-sm space-y-1 pl-4 list-decimal">
              <li>{{ t('rathole.frpcGuideStep2a') }}</li>
              <li>{{ t('rathole.frpcGuideStep2b') }}</li>
              <li>{{ t('rathole.frpcGuideStep2c') }}</li>
            </ol>
            <div class="bg-slate-100 dark:bg-white/5 rounded-xl p-4 text-xs">
              <p class="font-medium mb-1">{{ t('rathole.frpcGuideStep2Note') }}</p>
              <code class="bg-slate-200 dark:bg-white/10 px-2 py-1 rounded">https://api.xfrp.net/api/v2/passport/token</code>
            </div>

            <!-- 步骤3: 配置 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">Step 3: {{ t('rathole.frpcGuideStep3') }}</h4>
            <ol class="text-sm space-y-1 pl-4 list-decimal">
              <li>{{ t('rathole.frpcGuideStep3a') }}</li>
              <li>{{ t('rathole.frpcGuideStep3b') }}</li>
              <li>{{ t('rathole.frpcGuideStep3c') }}</li>
              <li>{{ t('rathole.frpcGuideStep3d') }}</li>
            </ol>

            <!-- 步骤4: 连接 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">Step 4: {{ t('rathole.frpcGuideStep4') }}</h4>
            <p>{{ t('rathole.frpcGuideStep4Desc') }}</p>

            <!-- 注意事项 -->
            <div class="bg-amber-50 dark:bg-amber-500/10 border border-amber-200 dark:border-amber-500/20 rounded-xl p-4 space-y-2 text-sm">
              <p class="font-medium text-amber-700 dark:text-amber-300">{{ t('rathole.frpcGuideWarning') }}</p>
              <ul class="space-y-1 pl-4 text-amber-600 dark:text-amber-400">
                <li>• {{ t('rathole.frpcGuideWarning1') }}</li>
                <li>• {{ t('rathole.frpcGuideWarning2') }}</li>
                <li>• {{ t('rathole.frpcGuideWarning3') }}</li>
              </ul>
            </div>
          </div>
        </div>
      </div>
    </Teleport>
  </div>
</template>

<style scoped>
.modal-enter-active, .modal-leave-active { transition: all 0.3s ease; }
.modal-enter-from, .modal-leave-to { opacity: 0; }
</style>
