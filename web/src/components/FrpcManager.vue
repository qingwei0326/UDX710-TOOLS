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
let statusTimer = null

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
})
</script>

<template>
  <div class="space-y-4 sm:space-y-6">
    <!-- 标题 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
      <div class="flex items-center space-x-3 mb-6">
        <div class="w-10 h-10 sm:w-12 sm:h-12 rounded-xl bg-gradient-to-br from-pink-500 to-rose-500 flex items-center justify-center shadow-lg shadow-pink-500/30">
          <i class="fas fa-wifi text-white text-lg sm:text-xl"></i>
        </div>
        <div>
          <h3 class="text-slate-900 dark:text-white font-semibold text-sm sm:text-base">Sakura Frp</h3>
          <p class="text-slate-500 dark:text-white/50 text-xs sm:text-sm">{{ t('rathole.frpcSubtitle') }}</p>
        </div>
      </div>

      <!-- 服务器配置 -->
      <div class="grid grid-cols-1 sm:grid-cols-3 gap-4 mb-4">
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.serverAddr') }}</label>
          <input v-model="config.server_addr" type="text"
            class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white text-sm focus:border-pink-400 focus:ring-2 focus:ring-pink-400/20 transition-all"
            placeholder="b1.xfrp.net">
        </div>
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">{{ t('rathole.serverPort') || '服务器端口' }}</label>
          <input v-model.number="config.server_port" type="number"
            class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white text-sm focus:border-pink-400 focus:ring-2 focus:ring-pink-400/20 transition-all"
            placeholder="7000">
        </div>
        <div>
          <label class="block text-slate-600 dark:text-white/60 text-sm mb-1">Token</label>
          <input v-model="config.token" type="password"
            class="w-full px-3 py-2 bg-slate-50 dark:bg-white/10 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white text-sm focus:border-pink-400 focus:ring-2 focus:ring-pink-400/20 transition-all"
            placeholder="Sakura Frp Access Token">
        </div>
      </div>

      <div class="flex items-center justify-between">
        <div class="flex items-center space-x-2">
          <label class="relative cursor-pointer">
            <input type="checkbox" :checked="config.auto_start" @click="toggleAutoStart" class="sr-only peer">
            <div class="w-11 h-6 bg-slate-200 dark:bg-white/20 rounded-full peer peer-checked:bg-pink-500 transition-colors"></div>
            <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-5"></div>
          </label>
          <span class="text-slate-600 dark:text-white/60 text-sm">{{ t('rathole.autoStart') }}</span>
        </div>
        <a href="https://www.xfrp.net/" target="_blank" class="text-pink-500 hover:text-pink-600 text-sm">
          <i class="fas fa-external-link-alt mr-1"></i>{{ t('rathole.registerAccount') || '注册樱花账号' }}
        </a>
      </div>
    </div>

    <!-- 隧道列表 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
      <div class="flex items-center justify-between mb-4">
        <h4 class="text-slate-900 dark:text-white font-semibold text-sm">{{ t('rathole.serviceList') }}</h4>
        <button @click="openNewDialog"
          class="px-3 py-1.5 bg-pink-500/10 hover:bg-pink-500/20 text-pink-600 dark:text-pink-400 rounded-lg text-xs font-medium transition-all">
          <i class="fas fa-plus mr-1"></i>{{ t('rathole.addService') }}
        </button>
      </div>

      <!-- 快捷预设 -->
      <div v-if="proxies.length === 0" class="mb-4 p-3 bg-slate-50 dark:bg-white/5 rounded-xl">
        <p class="text-slate-500 dark:text-white/50 text-xs mb-2">{{ t('rathole.quickPresets') || '快捷预设:' }}</p>
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

    <!-- 状态与控制 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
      <div class="flex items-center justify-between mb-4">
        <div class="flex items-center space-x-2">
          <div class="w-3 h-3 rounded-full" :class="status.running ? 'bg-green-500 animate-pulse' : 'bg-slate-300 dark:bg-white/20'"></div>
          <span class="text-slate-900 dark:text-white font-medium text-sm">
            {{ status.running ? t('rathole.running') : t('rathole.stopped') }}
          </span>
          <span v-if="status.pid > 0" class="text-slate-400 dark:text-white/30 text-xs">PID: {{ status.pid }}</span>
        </div>
        <span class="text-slate-400 dark:text-white/30 text-xs">
          {{ status.proxy_count }} {{ t('rathole.activeServices') }}
        </span>
      </div>

      <div class="flex space-x-2">
        <button v-if="!status.running" @click="startService"
          class="flex-1 py-2 bg-green-500 hover:bg-green-600 text-white rounded-xl text-sm font-medium transition-all">
          <i class="fas fa-play mr-1"></i>{{ t('rathole.start') }}
        </button>
        <button v-if="status.running" @click="stopService"
          class="flex-1 py-2 bg-red-500 hover:bg-red-600 text-white rounded-xl text-sm font-medium transition-all">
          <i class="fas fa-stop mr-1"></i>{{ t('rathole.stop') }}
        </button>
        <button @click="saveAndRestart"
          class="flex-1 py-2 bg-amber-500 hover:bg-amber-600 text-white rounded-xl text-sm font-medium transition-all">
          <i class="fas fa-sync-alt mr-1"></i>{{ t('rathole.restart') }}
        </button>
      </div>
    </div>

    <!-- 日志 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-4 sm:p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20">
      <div class="flex items-center justify-between mb-3">
        <h4 class="text-slate-900 dark:text-white font-semibold text-sm">{{ t('rathole.logs') }}</h4>
        <button @click="fetchLogs" class="text-slate-400 dark:text-white/40 hover:text-slate-600 dark:hover:text-white/60 transition-all">
          <i class="fas fa-sync-alt text-sm"></i>
        </button>
      </div>
      <div class="bg-slate-900 dark:bg-black/50 rounded-xl p-3 max-h-48 overflow-y-auto font-mono text-xs text-green-400 whitespace-pre-wrap">{{ logs || 'No logs...' }}</div>
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
  </div>
</template>

<style scoped>
.modal-enter-active, .modal-leave-active { transition: all 0.3s ease; }
.modal-enter-from, .modal-leave-to { opacity: 0; }
</style>
