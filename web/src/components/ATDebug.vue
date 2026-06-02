<script setup>
import { ref, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { executeAT } from '../composables/useApi'
import { useToast } from '../composables/useToast'

const { t } = useI18n()
const { success, error: showError } = useToast()

const command = ref('')
const result = ref('')
const loading = ref(false)
const history = ref([])

// IMEI 修改弹窗状态
const showImeiModal = ref(false)
const imeiSlot = ref(0) // 0=卡1, 1=卡2
const imeiInput = ref('')
const imeiSubmitting = ref(false)

// IMEI 输入位数
const imeiLength = computed(() => imeiInput.value.length)

// IMEI 验证
const imeiValid = computed(() => {
  return /^\d{15}$/.test(imeiInput.value)
})

// 打开 IMEI 修改弹窗
function openImeiModal(slot) {
  imeiSlot.value = slot
  imeiInput.value = ''
  showImeiModal.value = true
}

// 关闭弹窗
function closeImeiModal() {
  showImeiModal.value = false
  imeiInput.value = ''
}

// 提交 IMEI 修改
async function submitImei() {
  if (!imeiValid.value) {
    showError(t('at.imeiMustBe15'))
    return
  }
  
  imeiSubmitting.value = true
  const cmd = `AT+SPIMEI=${imeiSlot.value},"${imeiInput.value}"`
  
  try {
    const res = await executeAT(cmd)
    if (res.Code === 0) {
      success(t('at.slot') + (imeiSlot.value + 1) + ' ' + t('at.modifySuccess'))
      result.value = res.Data || 'OK'
      history.value.unshift({ cmd, result: result.value, time: new Date().toLocaleTimeString() })
      if (history.value.length > 20) history.value.pop()
      closeImeiModal()
    } else {
      showError(t('at.modifyFailed') + ': ' + res.Error)
      result.value = t('common.error') + ': ' + res.Error + '\n' + (res.Data || '')
    }
  } catch (err) {
    showError(t('at.requestFailed') + ': ' + err.message)
    result.value = t('at.requestFailed') + ': ' + err.message
  } finally {
    imeiSubmitting.value = false
  }
}

// 常用AT命令（点击直接发送）
const quickCommands = [
  { name: 'IMEI', cmd: 'AT+CGSN' },
  { name: 'ICCID', cmd: 'AT+CCID' },
  { name: 'signedRate', cmd: 'AT+CGEQOSRDP' },
]

async function sendCommand() {
  if (!command.value.trim() || loading.value) return
  loading.value = true
  result.value = ''
  const cmd = command.value.trim()
  
  try {
    const res = await executeAT(cmd)
    if (res.Code === 0) {
      result.value = res.Data || 'OK'
    } else {
      result.value = t('common.error') + ': ' + res.Error + '\n' + (res.Data || '')
    }
    history.value.unshift({ cmd, result: result.value, time: new Date().toLocaleTimeString() })
    if (history.value.length > 20) history.value.pop()
  } catch (error) {
    result.value = t('at.requestFailed') + ': ' + error.message
  } finally {
    loading.value = false
  }
}

function useQuickCommand(cmd) {
  command.value = cmd
  sendCommand()
}

function clearHistory() {
  history.value = []
  result.value = ''
}
</script>

<template>
  <div class="space-y-6">
    <!-- 输入区域 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-terminal text-cyan-600 dark:text-cyan-400 mr-2"></i>
        {{ $t('at.title') }}
      </h3>
      <div class="flex space-x-3 mb-4">
        <input v-model="command" @keyup.enter="sendCommand" type="text" :placeholder="$t('at.inputPlaceholder')"
          class="flex-1 px-4 py-3 bg-slate-100 dark:bg-black/30 border border-slate-200 dark:border-white/20 rounded-xl text-slate-900 dark:text-white placeholder-slate-400 dark:placeholder-white/30 focus:outline-none focus:border-cyan-500 font-mono">
        <button @click="sendCommand" :disabled="loading || !command.trim()"
          class="px-6 py-3 bg-gradient-to-r from-cyan-500 to-blue-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-cyan-500/30 transition-all disabled:opacity-50">
          <i :class="loading ? 'fas fa-spinner animate-spin' : 'fas fa-paper-plane'" class="mr-2"></i>{{ $t('at.send') }}
        </button>
      </div>
      <!-- 快捷命令（点击直接发送） -->
      <div class="mb-4">
        <p class="text-slate-500 dark:text-white/50 text-xs mb-2">{{ $t('at.quickCommands') }}</p>
        <div class="flex flex-wrap gap-2">
          <button v-for="qc in quickCommands" :key="qc.cmd" @click="useQuickCommand(qc.cmd)"
            class="px-3 py-1.5 text-xs bg-slate-100 dark:bg-white/10 hover:bg-slate-200 dark:hover:bg-white/20 text-slate-600 dark:text-white/70 hover:text-slate-900 dark:hover:text-white rounded-lg transition-colors">
            {{ qc.name === 'signedRate' ? $t('monitor.signedRate') : qc.name }}
          </button>
        </div>
      </div>
      
      <!-- 模板命令（点击弹窗修改IMEI） -->
      <div class="flex flex-wrap gap-2">
        <button @click="openImeiModal(0)"
          class="px-3 py-1.5 text-xs bg-amber-100 dark:bg-amber-500/20 hover:bg-amber-200 dark:hover:bg-amber-500/30 text-amber-700 dark:text-amber-400 hover:text-amber-900 dark:hover:text-amber-300 rounded-lg transition-colors border border-amber-200 dark:border-amber-500/30">
          <i class="fas fa-edit mr-1"></i>{{ $t('at.modifySlot1Imei') }}
        </button>
        <button @click="openImeiModal(1)"
          class="px-3 py-1.5 text-xs bg-amber-100 dark:bg-amber-500/20 hover:bg-amber-200 dark:hover:bg-amber-500/30 text-amber-700 dark:text-amber-400 hover:text-amber-900 dark:hover:text-amber-300 rounded-lg transition-colors border border-amber-200 dark:border-amber-500/30">
          <i class="fas fa-edit mr-1"></i>{{ $t('at.modifySlot2Imei') }}
        </button>
      </div>
    </div>

    <!-- IMEI 修改弹窗 -->
    <Teleport to="body">
      <div v-if="showImeiModal" class="fixed inset-0 z-50 flex items-center justify-center p-4">
        <div class="absolute inset-0 bg-black/50 backdrop-blur-sm" @click="closeImeiModal"></div>
        <div class="relative bg-white dark:bg-slate-800 rounded-2xl shadow-2xl w-full max-w-md p-6 border border-slate-200 dark:border-white/10">
          <h3 class="text-lg font-semibold text-slate-900 dark:text-white mb-4 flex items-center">
            <i class="fas fa-fingerprint text-amber-500 mr-2"></i>
            {{ $t('at.modifySlotImei', { slot: imeiSlot + 1 }) }}
          </h3>
          
          <div class="space-y-4">
            <div>
              <label class="block text-sm text-slate-600 dark:text-white/60 mb-2">
                {{ $t('at.enterNewImei') }}
              </label>
              <input 
                v-model="imeiInput" 
                type="text" 
                maxlength="15"
                :placeholder="$t('at.imeiPlaceholder')"
                class="w-full px-4 py-3 bg-slate-100 dark:bg-black/30 border rounded-xl text-slate-900 dark:text-white font-mono text-lg tracking-wider placeholder-slate-400 dark:placeholder-white/30 focus:outline-none transition-colors"
                :class="imeiInput.length > 0 ? (imeiValid ? 'border-green-500' : 'border-amber-500') : 'border-slate-200 dark:border-white/20'"
              >
            </div>
            
            <!-- 位数显示 -->
            <div class="flex items-center justify-between text-sm">
              <span class="text-slate-500 dark:text-white/50">
                {{ $t('at.inputCount') }} <span class="font-mono font-bold" :class="imeiValid ? 'text-green-500' : 'text-amber-500'">{{ imeiLength }}</span> / 15 {{ $t('at.digits') }}
              </span>
              <span v-if="imeiInput.length > 0 && !/^\d*$/.test(imeiInput)" class="text-red-500">
                <i class="fas fa-exclamation-circle mr-1"></i>{{ $t('at.onlyNumbers') }}
              </span>
              <span v-else-if="imeiValid" class="text-green-500">
                <i class="fas fa-check-circle mr-1"></i>{{ $t('at.formatCorrect') }}
              </span>
            </div>
            
            <!-- 按钮 -->
            <div class="flex space-x-3 pt-2">
              <button @click="closeImeiModal"
                class="flex-1 px-4 py-2.5 bg-slate-100 dark:bg-white/10 text-slate-700 dark:text-white/70 rounded-xl hover:bg-slate-200 dark:hover:bg-white/20 transition-colors">
                {{ $t('common.cancel') }}
              </button>
              <button @click="submitImei" :disabled="!imeiValid || imeiSubmitting"
                class="flex-1 px-4 py-2.5 bg-gradient-to-r from-amber-500 to-orange-500 text-white font-medium rounded-xl hover:shadow-lg hover:shadow-amber-500/30 transition-all disabled:opacity-50 disabled:cursor-not-allowed">
                <i :class="imeiSubmitting ? 'fas fa-spinner animate-spin' : 'fas fa-check'" class="mr-2"></i>
                {{ imeiSubmitting ? $t('at.submitting') : $t('at.confirmModify') }}
              </button>
            </div>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 结果显示 -->
    <div class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <div class="flex items-center justify-between mb-4">
        <h3 class="text-slate-900 dark:text-white font-semibold flex items-center">
          <i class="fas fa-code text-green-600 dark:text-green-400 mr-2"></i>{{ $t('at.result') }}
        </h3>
        <button v-if="history.length > 0" @click="clearHistory" class="text-xs text-slate-500 dark:text-white/50 hover:text-slate-900 dark:hover:text-white transition-colors">
          <i class="fas fa-trash mr-1"></i>{{ $t('at.clearHistory') }}
        </button>
      </div>
      <div class="bg-slate-900 dark:bg-black/50 rounded-xl p-4 font-mono text-sm min-h-[120px] max-h-[300px] overflow-auto">
        <pre v-if="result" class="text-green-400 whitespace-pre-wrap">{{ result }}</pre>
        <p v-else class="text-slate-500 dark:text-white/30">{{ $t('at.waitingExecution') }}</p>
      </div>
    </div>

    <!-- 历史记录 -->
    <div v-if="history.length > 0" class="rounded-2xl bg-white/95 dark:bg-white/5 backdrop-blur border border-slate-200/60 dark:border-white/10 p-6 shadow-lg shadow-slate-200/40 dark:shadow-black/20 hover:shadow-xl hover:shadow-slate-300/50 dark:hover:shadow-black/30 transition-all duration-300">
      <h3 class="text-slate-900 dark:text-white font-semibold mb-4 flex items-center">
        <i class="fas fa-history text-amber-500 dark:text-amber-400 mr-2"></i>{{ $t('at.history') }}
      </h3>
      <div class="space-y-3 max-h-[400px] overflow-auto">
        <div v-for="(item, index) in history" :key="index" class="p-3 bg-slate-900 dark:bg-black/30 rounded-xl">
          <div class="flex items-center justify-between mb-2">
            <code class="text-cyan-400 text-sm">{{ item.cmd }}</code>
            <span class="text-slate-500 dark:text-white/30 text-xs">{{ item.time }}</span>
          </div>
          <pre class="text-green-400/70 text-xs whitespace-pre-wrap max-h-[100px] overflow-auto">{{ item.result }}</pre>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
pre { font-family: 'Consolas', 'Monaco', monospace; }
</style>
