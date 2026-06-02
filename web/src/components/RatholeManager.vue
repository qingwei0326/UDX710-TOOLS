<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useToast } from '../composables/useToast'
import { useConfirm } from '../composables/useConfirm'
import FrpcManager from './FrpcManager.vue'

const { t } = useI18n()
const { success, error } = useToast()
const { confirm } = useConfirm()

// 方案选择: 'rathole' | 'frpc'
const provider = ref(localStorage.getItem('tunnel_provider') || 'rathole')

function setProvider(p) {
  provider.value = p
  localStorage.setItem('tunnel_provider', p)
}

// 状态
const loading = ref(false)
const saving = ref(false)
const config = ref({ server_addr: '', auto_start: 0, enabled: 0 })
const services = ref([])
const status = ref({ running: 0, pid: -1, service_count: 0 })
const logs = ref('')
const showDialog = ref(false)
const isEditing = ref(false)
const showGuide = ref(false)
const showServerConfig = ref(false)
const serverConfig = ref('')
const configTab = ref('script')
let statusTimer = null

// 表单
const form = ref({
  id: 0,
  name: '',
  token: '',
  local_addr: '',
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
    const res = await fetch('/api/rathole/config', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      config.value = data.data
    }
  } catch (e) {
    console.error('获取配置失败:', e)
  }
}

// 获取服务列表
async function fetchServices() {
  try {
    const res = await fetch('/api/rathole/services', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      services.value = data.data
    }
  } catch (e) {
    console.error('获取服务列表失败:', e)
  }
}

// 获取运行状态
async function fetchStatus() {
  try {
    const res = await fetch('/api/rathole/status', { headers: getHeaders() })
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
    const res = await fetch('/api/rathole/logs?lines=50', { headers: getHeaders() })
    if (!res.ok) return
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      logs.value = data.data.logs || ''
    }
  } catch (e) {
    console.error('获取日志失败:', e)
  }
}

// 检测是否包含中文字符
function containsChinese(str) {
  return /[\u4e00-\u9fa5]/.test(str)
}

// 保存配置
async function saveConfig() {
  if (!config.value.server_addr.trim()) {
    error(t('rathole.serverAddrRequired'))
    return false
  }
  
  // 检测中文
  if (containsChinese(config.value.server_addr)) {
    error('服务器地址不能包含中文，请检查')
    return false
  }
  
  saving.value = true
  try {
    const res = await fetch('/api/rathole/config', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify(config.value)
    })
    if (!res.ok) throw new Error('保存失败')
    success(t('rathole.configSaved'))
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
    const res = await fetch('/api/rathole/autostart', {
      method: 'POST',
      headers: getHeaders(true),
      body: JSON.stringify({ auto_start: config.value.auto_start })
    })
    if (!res.ok) throw new Error('设置失败')
    const data = await res.json()
    if (data.status === 'ok') {
      success(config.value.auto_start ? t('rathole.autoStartEnabled') : t('rathole.autoStartDisabled'))
    }
  } catch (e) {
    error(e.message)
    // 恢复原值
    config.value.auto_start = config.value.auto_start ? 0 : 1
  }
}

// 启动服务
async function startService() {
  // 先保存配置
  const saved = await saveConfig()
  if (!saved) return
  
  saving.value = true
  try {
    const res = await fetch('/api/rathole/start', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('启动失败')
    success(t('rathole.startSuccess'))
    await fetchStatus()
    await fetchLogs()
  } catch (e) {
    error(t('rathole.startFailed') + ': ' + e.message)
  } finally {
    saving.value = false
  }
}

// 停止服务
async function stopService() {
  const ok = await confirm({ 
    title: t('rathole.stop'), 
    message: t('rathole.confirmStop'),
    danger: true 
  })
  if (!ok) return
  
  saving.value = true
  try {
    const res = await fetch('/api/rathole/stop', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('停止失败')
    success(t('rathole.stopSuccess'))
    await fetchStatus()
  } catch (e) {
    error(t('rathole.stopFailed') + ': ' + e.message)
  } finally {
    saving.value = false
  }
}

// 保存并重启
async function saveAndRestart() {
  // 先保存配置
  const saved = await saveConfig()
  if (!saved) return
  
  saving.value = true
  try {
    // 如果正在运行，先停止
    if (status.value.running) {
      await fetch('/api/rathole/stop', { method: 'POST', headers: getHeaders() })
      await new Promise(r => setTimeout(r, 500))
    }
    
    // 启动
    const res = await fetch('/api/rathole/start', {
      method: 'POST',
      headers: getHeaders()
    })
    if (!res.ok) throw new Error('启动失败')
    
    success(t('rathole.startSuccess'))
    await fetchStatus()
    await fetchLogs()
  } catch (e) {
    error(e.message)
  } finally {
    saving.value = false
  }
}

// 打开新建对话框
function openNewDialog() {
  isEditing.value = false
  form.value = { id: 0, name: '', token: '', local_addr: '', enabled: 1 }
  showDialog.value = true
}

// 打开编辑对话框
function openEditDialog(svc) {
  isEditing.value = true
  form.value = { ...svc }
  showDialog.value = true
}

// 保存服务
async function saveService() {
  if (!form.value.name.trim() || !form.value.token.trim() || !form.value.local_addr.trim()) {
    error(t('rathole.nameRequired'))
    return
  }
  
  // 检测中文
  if (containsChinese(form.value.name)) {
    error('服务名称不能包含中文')
    return
  }
  if (containsChinese(form.value.token)) {
    error('Token 不能包含中文')
    return
  }
  if (containsChinese(form.value.local_addr)) {
    error('本地地址不能包含中文')
    return
  }
  
  saving.value = true
  try {
    const url = isEditing.value ? `/api/rathole/services/${form.value.id}` : '/api/rathole/services'
    const method = isEditing.value ? 'PUT' : 'POST'
    const res = await fetch(url, { 
      method, 
      headers: getHeaders(true), 
      body: JSON.stringify(form.value) 
    })
    if (!res.ok) throw new Error('保存失败')
    success(isEditing.value ? t('rathole.serviceUpdated') : t('rathole.serviceAdded'))
    showDialog.value = false
    await fetchServices()
  } catch (e) {
    error(e.message)
  } finally {
    saving.value = false
  }
}

// 删除服务
async function deleteService(svc) {
  const ok = await confirm({ 
    title: t('common.delete'), 
    message: t('rathole.confirmDelete'),
    danger: true 
  })
  if (!ok) return
  
  try {
    const res = await fetch(`/api/rathole/services/${svc.id}`, { 
      method: 'DELETE', 
      headers: getHeaders() 
    })
    if (!res.ok) throw new Error('删除失败')
    success(t('rathole.serviceDeleted'))
    await fetchServices()
  } catch (e) {
    error(e.message)
  }
}

// 获取服务端配置
async function fetchServerConfig() {
  try {
    const res = await fetch('/api/rathole/server-config', { headers: getHeaders() })
    if (!res.ok) throw new Error('获取失败')
    const data = await res.json()
    if (data.status === 'ok' && data.data) {
      serverConfig.value = data.data.config || ''
      showServerConfig.value = true
    }
  } catch (e) {
    error(e.message)
  }
}

// 生成一键安装脚本
const installScript = computed(() => {
  const config = serverConfig.value || ''
  return `#!/bin/bash
# Rathole Server Auto Setup Script
# Created by 5G MiFi Dashboard

# 检查 root 权限
if [ "$EUID" -ne 0 ]; then
  echo "❌ 错误: 请使用 root 权限运行此脚本"
  echo "尝试: sudo bash $0"
  exit 1
fi

echo "🚀 开始安装 Rathole 服务端..."

# 1. 创建工作目录
echo "📂 创建目录 /root/rathole..."
mkdir -p /root/rathole
cd /root/rathole

# 2. 写入配置文件
echo "CONFIG 写入 server.toml..."
cat > server.toml <<EOF
${config}
EOF

# 3. 创建 systemd 服务
echo "⚙️ 创建 systemd 服务..."
cat > /etc/systemd/system/rathole.service <<EOF
[Unit]
Description=Rathole NAT Traversal Server
After=network.target

[Service]
Type=simple
ExecStart=/root/rathole/rathole /root/rathole/server.toml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# 4. 下载 Rathole (如果不存在)
if [ ! -f "rathole" ]; then
  echo "⬇️ 下载 rathole-x86_64-unknown-linux-gnu..."
  
  # 检测网络连接
  if ! ping -c 1 github.com >/dev/null 2>&1; then
    echo "⚠️ 警告: 无法连接 GitHub，尝试使用 curl 下载..."
  fi

  # 尝试使用 wget 或 curl
  if command -v wget >/dev/null 2>&1; then
    wget -q --show-progress https://github.com/rapiz1/rathole/releases/download/v0.5.0/rathole-x86_64-unknown-linux-gnu.zip -O rathole.zip
  else
    curl -L https://github.com/rapiz1/rathole/releases/download/v0.5.0/rathole-x86_64-unknown-linux-gnu.zip -o rathole.zip
  fi
  
  if [ -f "rathole.zip" ]; then
    echo "📦 解压..."
    if command -v unzip >/dev/null 2>&1; then
      unzip -o rathole.zip
      chmod +x rathole
      rm rathole.zip
    else
      echo "⚠️ 警告: 未找到 unzip 命令，尝试安装..."
      if command -v apt-get >/dev/null 2>&1; then
        apt-get update && apt-get install -y unzip
        unzip -o rathole.zip
        chmod +x rathole
        rm rathole.zip
      elif command -v yum >/dev/null 2>&1; then
        yum install -y unzip
        unzip -o rathole.zip
        chmod +x rathole
        rm rathole.zip
      else
        echo "❌ 错误: 无法解压文件，请手动安装 unzip"
        exit 1
      fi
    fi
  else
    echo "❌ 下载失败"
    exit 1
  fi
fi

# 5. 启动服务
echo "🔄 启动服务..."
systemctl daemon-reload
systemctl enable rathole
systemctl restart rathole

# 检查状态
sleep 2
if systemctl is-active --quiet rathole; then
  echo "✅ Rathole 服务启动成功！"
  echo "📝 查看日志: journalctl -u rathole -f"
else
  echo "❌ 启动失败，请检查日志"
  systemctl status rathole
fi

# 6. 防火墙设置
echo "🛡️ 配置防火墙..."
if command -v ufw >/dev/null 2>&1; then
  echo "检测到 UFW..."
  ufw allow 2333/tcp
  ufw allow 9000:9100/tcp comment 'Rathole Services'
  echo "✅ 已放行 TCP 2333 和 9000-9100"
elif command -v firewall-cmd >/dev/null 2>&1; then
  echo "检测到 Firewalld..."
  firewall-cmd --permanent --add-port=2333/tcp
  firewall-cmd --permanent --add-port=9000-9100/tcp
  firewall-cmd --reload
  echo "✅ 已放行 TCP 2333 和 9000-9100"
else
  echo "⚠️ 未检测到常用防火墙，请手动放行端口 2333 和服务端口"
fi

# 7. 创建管理脚本
echo "🛠️ 创建管理脚本 manage.sh..."
cat > manage.sh <<EOF
#!/bin/bash
# Rathole 管理脚本

SERVICE="rathole"

case "\$1" in
    start)
        systemctl start \$SERVICE
        echo "✅ 服务已启动"
        ;;
    stop)
        systemctl stop \$SERVICE
        echo "🛑 服务已停止"
        ;;
    restart)
        systemctl restart \$SERVICE
        echo "🔄 服务已重启"
        ;;
    status)
        systemctl status \$SERVICE
        ;;
    logs)
        journalctl -u \$SERVICE -f
        ;;
    *)
        echo "用法: \$0 {start|stop|restart|status|logs}"
        exit 1
        ;;
esac
EOF
chmod +x manage.sh

echo "🎉 安装完成！"
echo "👉 管理命令: ./manage.sh {start|stop|restart|status|logs}"
`
})

// 复制文本通用函数
async function copyText(text) {
  // 优先使用现代 API
  if (navigator.clipboard && window.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text)
      success(t('rathole.configCopied') || '复制成功')
      return
    } catch (e) {
      // 降级到传统方法
    }
  }
  
  // 降级方案：创建临时 textarea
  try {
    const textarea = document.createElement('textarea')
    textarea.value = text
    textarea.style.position = 'fixed'
    textarea.style.left = '-9999px'
    textarea.style.top = '-9999px'
    document.body.appendChild(textarea)
    textarea.focus()
    textarea.select()
    document.execCommand('copy')
    document.body.removeChild(textarea)
    success(t('rathole.configCopied') || '复制成功')
  } catch (e) {
    error(t('rathole.copyFailed') || '复制失败')
  }
}

// 复制配置到剪贴板
async function copyServerConfig() {
  await copyText(serverConfig.value)
}

// 初始化
onMounted(async () => {
  loading.value = true
  await Promise.all([fetchConfig(), fetchServices(), fetchStatus()])
  await fetchLogs()
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
    <!-- 方案选择 Tab -->
    <div class="flex p-1 bg-slate-100 dark:bg-white/10 rounded-xl">
      <button @click="setProvider('rathole')"
        class="flex-1 py-2.5 px-4 rounded-lg text-sm font-medium transition-all flex items-center justify-center space-x-2"
        :class="provider === 'rathole' ? 'bg-white dark:bg-white/20 text-indigo-600 dark:text-indigo-400 shadow-sm' : 'text-slate-500 dark:text-white/50 hover:text-slate-700 dark:hover:text-white/70'">
        <i class="fas fa-shield-alt"></i>
        <span>Rathole</span>
        <span class="text-xs opacity-60 hidden sm:inline">{{ t('rathole.selfHosted') || '(自建)' }}</span>
      </button>
      <button @click="setProvider('frpc')"
        class="flex-1 py-2.5 px-4 rounded-lg text-sm font-medium transition-all flex items-center justify-center space-x-2"
        :class="provider === 'frpc' ? 'bg-white dark:bg-white/20 text-pink-600 dark:text-pink-400 shadow-sm' : 'text-slate-500 dark:text-white/50 hover:text-slate-700 dark:hover:text-white/70'">
        <i class="fas fa-wifi"></i>
        <span>Sakura Frp</span>
        <span class="text-xs opacity-60 hidden sm:inline">{{ t('rathole.freeService') || '(免费穿透)' }}</span>
      </button>
    </div>

    <!-- Rathole 内容 -->
    <template v-if="provider === 'rathole'">
    <!-- 服务器配置 -->
    <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-server text-indigo-500 mr-2"></i>
        {{ t('rathole.serverConfig') }}
      </h3>
      
      <div class="space-y-4">
        <!-- 服务器地址 -->
        <div>
          <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
            {{ t('rathole.serverAddr') }} <span class="text-red-500">*</span>
          </label>
          <input 
            v-model="config.server_addr" 
            type="text" 
            :placeholder="t('rathole.serverAddrPlaceholder')"
            class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-indigo-500 outline-none transition-all"
          >
        </div>
        
        <!-- 开关选项 -->
        <div class="grid grid-cols-1 gap-4">
          <!-- 自启动 -->
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
              <input type="checkbox" v-model="config.auto_start" :true-value="1" :false-value="0" @change="toggleAutoStart" class="sr-only peer">
              <div class="w-12 h-6 bg-slate-300 dark:bg-white/20 rounded-full peer peer-checked:bg-green-500 transition-all"></div>
              <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-6"></div>
            </label>
          </div>
          

        </div>
      </div>
    </div>

    <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      <!-- 服务列表 -->
      <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
        <div class="flex items-center justify-between mb-4">
          <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
            <i class="fas fa-list text-violet-500 mr-2"></i>
            {{ t('rathole.serviceList') }}
          </h3>
          <button @click="openNewDialog" class="px-3 py-1.5 bg-violet-500 text-white text-sm rounded-lg hover:bg-violet-600 transition-colors flex items-center">
            <i class="fas fa-plus mr-1.5"></i>
            {{ t('rathole.addService') }}
          </button>
        </div>
        
        <div class="space-y-2 max-h-64 overflow-y-auto">
          <template v-if="services.length > 0">
            <div
              v-for="svc in services"
              :key="svc.id"
              class="p-4 rounded-xl bg-slate-50 dark:bg-white/5 border border-slate-200 dark:border-white/10"
            >
              <div class="flex items-center justify-between">
                <div class="flex items-center space-x-3">
                  <div :class="['w-9 h-9 rounded-lg flex items-center justify-center', svc.enabled ? 'bg-violet-500' : 'bg-slate-200 dark:bg-white/10']">
                    <i :class="['fas fa-exchange-alt', svc.enabled ? 'text-white' : 'text-slate-400']"></i>
                  </div>
                  <div>
                    <p class="text-slate-900 dark:text-white font-medium">{{ svc.name }}</p>
                    <p class="text-slate-500 dark:text-white/50 text-xs font-mono">{{ svc.local_addr }}</p>
                  </div>
                </div>
                <div class="flex items-center space-x-2">
                  <button @click="openEditDialog(svc)" class="w-8 h-8 rounded-lg bg-slate-200 dark:bg-white/10 hover:bg-blue-100 dark:hover:bg-blue-500/20 flex items-center justify-center transition-colors">
                    <i class="fas fa-pen text-blue-500 text-xs"></i>
                  </button>
                  <button @click="deleteService(svc)" class="w-8 h-8 rounded-lg bg-slate-200 dark:bg-white/10 hover:bg-red-100 dark:hover:bg-red-500/20 flex items-center justify-center transition-colors">
                    <i class="fas fa-trash-alt text-red-500 text-xs"></i>
                  </button>
                </div>
              </div>
            </div>
          </template>
          <div v-else class="w-full flex flex-col items-center justify-center py-8">
            <i class="fas fa-inbox text-slate-300 dark:text-white/20 text-3xl mb-3"></i>
            <p class="text-slate-500 dark:text-white/50 text-sm">{{ t('rathole.noData') }}</p>
          </div>
        </div>
      </div>

      <!-- 控制面板 -->
      <div class="space-y-6">
        <!-- 运行状态 -->
        <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
          <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
            <i class="fas fa-info-circle text-cyan-500 mr-2"></i>
            {{ t('rathole.status') }}
          </h3>
          
          <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl mb-4">
            <div class="flex items-center space-x-3">
              <div :class="['w-12 h-12 rounded-xl flex items-center justify-center', status.running ? 'bg-green-500' : 'bg-slate-200 dark:bg-white/10']">
                <i :class="['fas fa-power-off text-xl', status.running ? 'text-white' : 'text-slate-400']"></i>
              </div>
              <div>
                <p class="text-slate-900 dark:text-white font-semibold">
                  {{ status.running ? t('rathole.running') : t('rathole.stopped') }}
                </p>
                <p v-if="status.running && status.pid > 0" class="text-slate-500 dark:text-white/50 text-xs">
                  PID: {{ status.pid }}
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
              {{ status.service_count }} {{ t('rathole.serviceCount') || '个服务' }}
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
              <span>{{ t('rathole.start') }}</span>
            </button>
            <button
              @click="stopService"
              :disabled="saving || !status.running"
              class="py-3 bg-red-500 text-white rounded-xl hover:bg-red-600 disabled:opacity-50 disabled:cursor-not-allowed transition-all flex items-center justify-center space-x-2"
            >
              <i class="fas fa-stop"></i>
              <span>{{ t('rathole.stop') }}</span>
            </button>
            <button
              @click="saveAndRestart"
              :disabled="saving"
              class="py-3 bg-indigo-500 text-white rounded-xl hover:bg-indigo-600 disabled:opacity-50 disabled:cursor-not-allowed transition-all flex items-center justify-center space-x-2"
            >
              <i :class="saving ? 'fas fa-spinner animate-spin' : 'fas fa-sync-alt'"></i>
              <span>{{ t('rathole.restart') }}</span>
            </button>
          </div>
        </div>

        <!-- 快捷操作 -->
        <div class="bg-indigo-50 dark:bg-indigo-500/10 rounded-2xl border border-indigo-200 dark:border-indigo-500/20 p-5">
          <div class="flex items-center justify-between mb-3">
            <div class="flex items-center space-x-3">
              <div class="w-10 h-10 rounded-lg bg-indigo-100 dark:bg-indigo-500/20 flex items-center justify-center">
                <i class="fas fa-lightbulb text-indigo-500"></i>
              </div>
              <p class="text-indigo-800 dark:text-indigo-200 font-medium">{{ t('rathole.tips') }}</p>
            </div>
          </div>
          <div class="grid grid-cols-2 gap-3">
            <button @click="showGuide = true" class="py-2.5 bg-indigo-500 text-white rounded-xl hover:bg-indigo-600 transition-colors flex items-center justify-center space-x-2">
              <i class="fas fa-book"></i>
              <span>{{ t('rathole.guide') || '使用指南' }}</span>
            </button>
            <button @click="fetchServerConfig" class="py-2.5 bg-violet-500 text-white rounded-xl hover:bg-violet-600 transition-colors flex items-center justify-center space-x-2">
              <i class="fas fa-download"></i>
              <span>{{ t('rathole.exportConfig') || '导出服务端配置' }}</span>
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 日志查看 -->
    <div class="bg-white/95 dark:bg-white/5 rounded-2xl border border-slate-200/60 dark:border-white/10 p-6 shadow-lg">
      <div class="flex items-center justify-between mb-4">
        <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
          <i class="fas fa-terminal text-amber-500 mr-2"></i>
          {{ t('rathole.logs') }}
        </h3>
        <button @click="fetchLogs" class="px-3 py-1.5 bg-amber-500 text-white text-sm rounded-lg hover:bg-amber-600 transition-colors flex items-center">
          <i class="fas fa-sync-alt mr-1.5"></i>
          {{ t('rathole.refreshLogs') }}
        </button>
      </div>
      
      <div class="bg-slate-900 rounded-xl p-4 h-48 overflow-auto font-mono text-sm">
        <pre v-if="logs" class="text-green-400 whitespace-pre-wrap">{{ logs }}</pre>
        <p v-else class="text-slate-500 text-center py-8">{{ t('rathole.noLogs') }}</p>
      </div>
    </div>



    <!-- 服务编辑弹窗 -->
    <Teleport to="body">
      <div v-show="showDialog" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showDialog = false"></div>
        <div class="relative w-full max-w-lg bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden">
          <!-- 头部 -->
          <div class="px-6 py-4 bg-violet-500 text-white flex items-center justify-between">
            <h3 class="font-semibold">{{ isEditing ? t('rathole.editService') : t('rathole.addService') }}</h3>
            <button @click="showDialog = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>

          <!-- 表单 -->
          <div class="p-6 space-y-4">
            <div>
              <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
                {{ t('rathole.serviceName') }} <span class="text-red-500">*</span>
              </label>
              <input v-model="form.name" type="text" :placeholder="t('rathole.serviceNamePlaceholder')"
                class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white focus:border-violet-500 outline-none transition-all">
            </div>
            
            <div>
              <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
                {{ t('rathole.token') }} <span class="text-red-500">*</span>
              </label>
              <input v-model="form.token" type="text" :placeholder="t('rathole.tokenPlaceholder')"
                class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-violet-500 outline-none transition-all">
            </div>
            
            <div>
              <label class="block text-slate-700 dark:text-white/80 text-sm font-medium mb-1.5">
                {{ t('rathole.localAddr') }} <span class="text-red-500">*</span>
              </label>
              <input v-model="form.local_addr" type="text" :placeholder="t('rathole.localAddrPlaceholder')"
                class="w-full px-4 py-3 rounded-xl border border-slate-200 dark:border-white/10 bg-slate-50 dark:bg-white/5 text-slate-900 dark:text-white font-mono focus:border-violet-500 outline-none transition-all">
            </div>
            
            <!-- 启用开关 -->
            <div class="flex items-center justify-between p-4 bg-slate-50 dark:bg-white/5 rounded-xl">
              <span class="text-slate-700 dark:text-white/80 font-medium">{{ t('rathole.enabled') }}</span>
              <label class="relative cursor-pointer">
                <input type="checkbox" v-model="form.enabled" :true-value="1" :false-value="0" class="sr-only peer">
                <div class="w-12 h-6 bg-slate-300 dark:bg-white/20 rounded-full peer peer-checked:bg-violet-500 transition-all"></div>
                <div class="absolute top-0.5 left-0.5 w-5 h-5 bg-white rounded-full shadow transition-transform peer-checked:translate-x-6"></div>
              </label>
            </div>
          </div>
          
          <!-- 底部按钮 -->
          <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 border-t border-slate-200 dark:border-white/10 flex justify-end space-x-3">
            <button @click="showDialog = false" class="px-5 py-2.5 rounded-xl border border-slate-200 dark:border-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-100 dark:hover:bg-white/10">
              {{ t('common.cancel') }}
            </button>
            <button @click="saveService" :disabled="saving" class="px-5 py-2.5 rounded-xl bg-violet-500 text-white hover:bg-violet-600 disabled:opacity-50 flex items-center space-x-2">
              <i :class="saving ? 'fas fa-spinner animate-spin' : 'fas fa-check'"></i>
              <span>{{ t('common.save') }}</span>
            </button>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 使用指南弹窗 -->
    <Teleport to="body">
      <div v-show="showGuide" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showGuide = false"></div>
        <div class="relative w-full max-w-2xl max-h-[80vh] bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden flex flex-col">
          <div class="px-6 py-4 bg-indigo-500 text-white flex items-center justify-between">
            <h3 class="font-semibold">{{ t('rathole.guideTitle') }}</h3>
            <button @click="showGuide = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>
          <div class="p-6 overflow-y-auto space-y-4 text-slate-700 dark:text-white/80">
            <!-- 功能介绍 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white">{{ t('rathole.guideIntroTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideIntroDesc') }}</p>
            <div class="bg-blue-50 dark:bg-blue-500/10 border border-blue-200 dark:border-blue-500/20 rounded-xl p-4">
              <p class="text-blue-700 dark:text-blue-300 text-sm">💡 {{ t('rathole.guideIntroExample') }}</p>
            </div>

            <!-- 一键安装推荐 -->
            <div class="bg-gradient-to-r from-violet-500/10 to-purple-500/10 border border-violet-200 dark:border-violet-500/20 rounded-xl p-4 mt-4">
              <h4 class="text-base font-bold text-violet-700 dark:text-violet-300 mb-2">{{ t('rathole.guideOneClick') }}</h4>
              <p class="text-sm text-violet-600 dark:text-violet-400 mb-3">{{ t('rathole.guideOneClickTip') }}</p>
              
              <div class="bg-white/50 dark:bg-black/10 rounded-lg p-3 mb-3 text-xs text-violet-700 dark:text-violet-300 whitespace-pre-line leading-relaxed border border-violet-100 dark:border-violet-500/10">
                {{ t('rathole.guideOneClickSteps') }}
              </div>

              <p class="text-xs text-orange-600 dark:text-orange-400 font-medium mb-2">{{ t('rathole.exportWarning') }}</p>

              <div class="text-xs text-violet-500 dark:text-violet-400/80">
                {{ t('rathole.guideOneClickNote') }}
              </div>
            </div>
            
            <!-- 下载 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideDownloadTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideDownloadDesc') }}</p>
            <a href="https://github.com/rapiz1/rathole/releases/tag/v0.5.0" target="_blank" class="inline-flex items-center px-4 py-3 bg-slate-100 dark:bg-white/10 rounded-xl text-indigo-600 dark:text-indigo-400 text-sm hover:bg-indigo-100 dark:hover:bg-indigo-500/20">
              <i class="fas fa-external-link-alt mr-2"></i>{{ t('rathole.guideDownloadLink') }}
            </a>
            <ul class="text-sm space-y-1 pl-4">
              <li>• {{ t('rathole.guideDownloadX86') }}: <code class="bg-slate-200 dark:bg-white/10 px-1.5 py-0.5 rounded">rathole-x86_64-unknown-linux-gnu.zip</code></li>
              <li>• {{ t('rathole.guideDownloadARM') }}: <code class="bg-slate-200 dark:bg-white/10 px-1.5 py-0.5 rounded">rathole-aarch64-unknown-linux-musl.zip</code></li>
            </ul>
            <p class="text-sm text-green-600 dark:text-green-400">💡 {{ t('rathole.guideDownloadTip') }}</p>
            
            <!-- 服务端配置 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideServerTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideServerDesc') }}</p>
            
            <!-- 启动服务端 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideStartTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideStartDesc') }}</p>
            <pre class="bg-slate-900 text-green-400 p-4 rounded-xl text-sm overflow-x-auto">nohup ./rathole server.toml &gt; rathole.log 2&gt;&amp;1 &amp;</pre>
            
            <!-- 端口放行 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideFirewallTitle') }}</h4>
            <p class="text-sm text-red-600 dark:text-red-400 font-medium">{{ t('rathole.guideFirewallDesc') }}</p>
            
            <!-- 云控制台放行 -->
            <div class="bg-slate-100 dark:bg-white/5 rounded-xl p-4 space-y-2 text-sm">
              <p class="font-medium text-indigo-600 dark:text-indigo-400">{{ t('rathole.guideFirewallCloud') }}</p>
              <ul class="space-y-1 pl-4">
                <li>• {{ t('rathole.guideFirewallTip1') }}</li>
                <li>• {{ t('rathole.guideFirewallTip2') }}</li>
                <li>• {{ t('rathole.guideFirewallTip3') }}</li>
              </ul>
            </div>
            
            <!-- 宝塔放行 -->
            <div class="bg-orange-50 dark:bg-orange-500/10 border border-orange-200 dark:border-orange-500/20 rounded-xl p-4 space-y-2 text-sm">
              <p class="font-medium text-orange-600 dark:text-orange-400">{{ t('rathole.guideFirewallBt') }}</p>
              <p class="text-orange-700 dark:text-orange-300">{{ t('rathole.guideFirewallBtTip') }}</p>
            </div>
            
            <!-- Shell 命令放行 -->
            <p class="text-sm font-medium pt-2">{{ t('rathole.guideFirewallShell') }}</p>
            <pre class="bg-slate-900 text-green-400 p-3 rounded-xl text-xs overflow-x-auto"># CentOS/RHEL
firewall-cmd --permanent --add-port=2333/tcp
firewall-cmd --permanent --add-port=9000/tcp
firewall-cmd --reload

# Ubuntu/Debian
ufw allow 2333/tcp
ufw allow 9000/tcp</pre>
            
            <!-- 自启动配置 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideAutoStartTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideAutoStartDesc') }}</p>
            
            <div class="bg-slate-100 dark:bg-white/5 rounded-xl p-4 space-y-3 text-sm">
              <p class="font-medium">{{ t('rathole.guideAutoStartStep1') }}:</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">sudo nano /etc/systemd/system/rathole.service</pre>
              
              <p class="font-medium pt-2">{{ t('rathole.guideAutoStartService') }}</p>
              <pre class="bg-slate-900 text-green-400 p-3 rounded text-xs overflow-x-auto">[Unit]
Description=Rathole NAT Traversal Server
After=network.target

[Service]
Type=simple
ExecStart=/root/rathole/rathole /root/rathole/server.toml
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target</pre>
              
              <p class="font-medium pt-2">{{ t('rathole.guideAutoStartStep2') }}:</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">sudo systemctl daemon-reload
sudo systemctl enable rathole
sudo systemctl start rathole</pre>
              
              <p class="font-medium pt-2">{{ t('rathole.guideAutoStartStep3') }}:</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">sudo systemctl status rathole</pre>
            </div>
            
            <!-- 客户端配置 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideClientTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideClientDesc') }}</p>
            
            <!-- 更新配置说明 -->
            <h4 class="text-lg font-semibold text-slate-900 dark:text-white pt-2">{{ t('rathole.guideUpdateTitle') }}</h4>
            <p class="text-sm">{{ t('rathole.guideUpdateDesc') }}</p>
            <div class="bg-cyan-50 dark:bg-cyan-500/10 border border-cyan-200 dark:border-cyan-500/20 rounded-xl p-4 space-y-2 text-sm">
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep1') }}</p>
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep2') }}</p>
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep3') }}</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">nano /root/rathole/server.toml</pre>
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep4') }}</p>
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep5') }}</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">sudo systemctl restart rathole</pre>
              <p class="font-medium text-cyan-700 dark:text-cyan-300">{{ t('rathole.guideUpdateStep6') }}</p>
              <pre class="bg-slate-900 text-green-400 p-2 rounded text-xs overflow-x-auto">sudo systemctl status rathole</pre>
            </div>
            <p class="text-sm text-cyan-600 dark:text-cyan-400">{{ t('rathole.guideUpdateTip') }}</p>
            
            <!-- 重要提示 -->
            <div class="bg-amber-50 dark:bg-amber-500/10 border border-amber-200 dark:border-amber-500/20 rounded-xl p-4">
              <p class="text-amber-800 dark:text-amber-200 font-medium mb-1">{{ t('rathole.guideWarningTitle') }}</p>
              <ul class="text-amber-700 dark:text-amber-300 text-sm space-y-1">
                <li>• {{ t('rathole.guideWarningToken') }}</li>
                <li>• {{ t('rathole.guideWarningFirewall') }}</li>
                <li>• {{ t('rathole.guideWarningSystemd') }}</li>
                <li>• {{ t('rathole.guideWarningNoChinese') }}</li>
              </ul>
            </div>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 服务端配置弹窗 -->
    <Teleport to="body">
      <div v-show="showServerConfig" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="showServerConfig = false"></div>
        <div class="relative w-full max-w-2xl max-h-[80vh] bg-white dark:bg-slate-800 rounded-2xl shadow-2xl overflow-hidden flex flex-col">
          <div class="px-6 py-4 bg-violet-500 text-white flex items-center justify-between">
            <h3 class="font-semibold">{{ t('rathole.serverConfigTitle') }}</h3>
            <div class="flex space-x-2 bg-violet-600/50 p-1 rounded-lg">
              <button 
                @click="configTab = 'script'"
                :class="['px-3 py-1 text-xs rounded-md transition-colors', configTab === 'script' ? 'bg-white text-violet-600 shadow-sm' : 'text-white/80 hover:bg-white/10']"
              >
                {{ t('rathole.autoSetupScript') || '一键安装脚本' }}
              </button>
              <button 
                @click="configTab = 'toml'"
                :class="['px-3 py-1 text-xs rounded-md transition-colors', configTab === 'toml' ? 'bg-white text-violet-600 shadow-sm' : 'text-white/80 hover:bg-white/10']"
              >
                TOML
              </button>
            </div>
            <button @click="showServerConfig = false" class="w-8 h-8 rounded-lg hover:bg-white/20 flex items-center justify-center">
              <i class="fas fa-times"></i>
            </button>
          </div>
          <div class="p-6 overflow-y-auto">
            <!-- 警告提示 -->
            <div class="mb-4 bg-orange-50 dark:bg-orange-500/10 p-3 rounded-xl border border-orange-200 dark:border-orange-500/20 flex items-start space-x-2">
               <i class="fas fa-exclamation-triangle text-orange-500 mt-0.5 text-xs"></i>
               <p class="text-xs text-orange-700 dark:text-orange-300 font-medium leading-relaxed">{{ t('rathole.exportWarning') }}</p>
            </div>

            <!-- 脚本模式 -->
            <div v-if="configTab === 'script'" class="space-y-4">
              <div class="bg-blue-50 dark:bg-blue-500/10 p-4 rounded-xl border border-blue-100 dark:border-blue-500/20">
                <p class="text-sm text-blue-800 dark:text-blue-200 font-medium mb-1">🚀 {{ t('rathole.scriptUsageTitle') || '使用方法' }}</p>
                <p class="text-xs text-blue-600 dark:text-blue-400 mb-2">{{ t('rathole.scriptUsageDesc') || '在服务器上创建一个 .sh 文件，粘贴下方内容并运行，即可完成所有配置。' }}</p>
                <div class="bg-white dark:bg-black/20 rounded-lg p-2 border border-blue-100 dark:border-blue-500/10 flex items-center justify-between">
                  <code class="text-xs text-slate-600 dark:text-slate-400 font-mono">nano install_rathole.sh && bash install_rathole.sh</code>
                  <button @click="copyText('nano install_rathole.sh && bash install_rathole.sh')" class="text-blue-500 hover:text-blue-600">
                    <i class="far fa-copy"></i>
                  </button>
                </div>
              </div>
              <pre class="bg-slate-900 text-green-400 p-4 rounded-xl text-xs overflow-auto max-h-[400px] font-mono whitespace-pre">{{ installScript }}</pre>
            </div>

            <!-- TOML 模式 -->
            <div v-else>
              <p class="text-slate-600 dark:text-white/60 text-sm mb-3">{{ t('rathole.serverConfigDesc') }}</p>
              <pre class="bg-slate-900 text-green-400 p-4 rounded-xl text-sm overflow-auto max-h-64 font-mono">{{ serverConfig }}</pre>
            </div>
          </div>
          <div class="px-6 py-4 bg-slate-50 dark:bg-white/5 border-t border-slate-200 dark:border-white/10 flex justify-end space-x-3">
            <button @click="showServerConfig = false" class="px-5 py-2.5 rounded-xl border border-slate-200 dark:border-white/10 text-slate-600 dark:text-white/60 hover:bg-slate-100 dark:hover:bg-white/10">
              {{ t('common.close') }}
            </button>
            <button @click="configTab === 'script' ? copyText(installScript) : copyServerConfig()" class="px-5 py-2.5 rounded-xl bg-violet-500 text-white hover:bg-violet-600 flex items-center space-x-2">
              <i class="fas fa-copy"></i>
              <span>{{ t('common.copy') }}</span>
            </button>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 加载遮罩 -->
    <Teleport to="body">
      <div v-show="loading" class="fixed inset-0 z-40 bg-black/30 backdrop-blur-sm flex items-center justify-center">
        <div class="bg-white dark:bg-slate-800 rounded-2xl p-6 shadow-2xl flex items-center space-x-4">
          <i class="fas fa-spinner animate-spin text-indigo-500 text-2xl"></i>
          <span class="text-slate-700 dark:text-white">{{ t('common.loading') }}</span>
        </div>
      </div>
    </Teleport>
    </template>
    <!-- END Rathole -->

    <!-- Sakura Frp 内容 -->
    <FrpcManager v-if="provider === 'frpc'" />
  </div>
</template>
