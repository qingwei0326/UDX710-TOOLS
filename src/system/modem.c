/**
 * @file modem.c
 * @brief Modem control (Go: system/modem.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "modem.h"
#include "sysinfo.h"
#include "ofono.h"

/* 有效的网络模式 */
static const char *valid_modes[] = {"lte_only", "nr_5g_only", "nr_5g_lte_auto", "nsa_only", NULL};
static const char *valid_slots[] = {"slot1", "slot2", NULL};

int is_valid_network_mode(const char *mode) {
    if (!mode) return 0;
    for (int i = 0; valid_modes[i]; i++) {
        if (strcmp(mode, valid_modes[i]) == 0) return 1;
    }
    return 0;
}

int is_valid_slot(const char *slot) {
    if (!slot) return 0;
    for (int i = 0; valid_slots[i]; i++) {
        if (strcmp(slot, valid_slots[i]) == 0) return 1;
    }
    return 0;
}

int get_network_mode_code(const char *mode) {
    if (!mode) return -1;
    if (strcmp(mode, "lte_only") == 0) return MODE_LTE_ONLY;
    if (strcmp(mode, "nr_5g_only") == 0) return MODE_NR_5G_ONLY;
    if (strcmp(mode, "nr_5g_lte_auto") == 0) return MODE_NR_5G_LTE_AUTO;
    if (strcmp(mode, "nsa_only") == 0) return MODE_NSA_ONLY;
    return -1;
}

int set_network_mode(const char *mode) {
    return set_network_mode_for_slot(mode, NULL);
}

int set_network_mode_for_slot(const char *mode, const char *slot) {
    char ril_path[32];
    int mode_code;

    if (!is_valid_network_mode(mode)) {
        return -1;
    }

    mode_code = get_network_mode_code(mode);
    if (mode_code < 0) return -1;

    if (!slot || strlen(slot) == 0) {
        char slot_name[16];
        if (get_current_slot(slot_name, ril_path) != 0 || 
            strcmp(ril_path, "unknown") == 0 || strlen(ril_path) == 0) {
            return -1;
        }
    } else {
        /* slot1=外部SIM=ril_1, slot2=eSIM=ril_0 */
        if (strcmp(slot, "slot1") == 0) {
            strcpy(ril_path, "/ril_1");
        } else if (strcmp(slot, "slot2") == 0) {
            strcpy(ril_path, "/ril_0");
        } else {
            return -1;
        }
    }

    if (ofono_network_set_mode_sync(ril_path, mode_code, OFONO_TIMEOUT_MS) != 0) {
        return -1;
    }

    return 0;
}

extern int ofono_is_initialized(void);

/* SIM 卡槽切换的可靠方式：写配置文件 + reboot
 * 这台破解机上 oFono SetDataCard 不可靠（原厂固件用 stoneoim-service + GPIO 控制），
 * 只有改 sim_config + reboot 才能真正生效。
 * 路径: /var/stoneoim/sim_config.conf
 * 格式: [global]\nsim_slot=0  (0=物理SIM, 1=eSIM)           */
#define SIM_CONFIG_PATH "/etc/ril/system.ini"

static int switch_slot_via_config(const char *target_ril) {
    /*
     * 可靠的 SIM 卡槽切换：修改 /etc/ril/system.ini 的 persist.radio.card.setting
     * slot1 (外部SIM) = ril_1 = card.setting=1
     * slot2 (eSIM)    = ril_0 = card.setting=0
     *
     * 读取原文件，替换 card.setting 行，写回，然后 reboot。
     */
    int card_setting;
    if (strcmp(target_ril, "/ril_1") == 0) {
        card_setting = 1;  /* 外部 SIM */
    } else {
        card_setting = 0;  /* eSIM */
    }

    /* remount root 为可写 */
    system("mount -o remount,rw /");

    /* 用 busybox sed 替换 card.setting 行（避免 \r\r\n 问题） */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "/home/root/busybox-aarch64 sed -i "
        "'s/^persist\.radio\.card\.setting=.*/persist.radio.card.setting=%d/' "
        "%s",
        card_setting, SIM_CONFIG_PATH);
    int rc = system(cmd);
    if (rc != 0) {
        /* sed 失败，可能文件里没有这行，追加 */
        snprintf(cmd, sizeof(cmd),
            "echo 'persist.radio.card.setting=%d' >> %s",
            card_setting, SIM_CONFIG_PATH);
        system(cmd);
    }

    printf("[Modem] 已设置 card.setting=%d (target=%s)\n", card_setting, target_ril);

    /* remount 回只读 */
    system("mount -o remount,ro /");

    /* fork 子进程执行 reboot */
    pid_t pid = fork();
    if (pid == 0) {
        usleep(500 * 1000);
        sync();
        usleep(200 * 1000);
        execl("/sbin/reboot", "reboot", NULL);
        _exit(127);
    }

    return 0;
}

int switch_slot(const char *slot) {
    char target_ril[16], other_ril[16];
    char new_slot[16], new_ril[32];

    if (!ofono_is_initialized() || !is_valid_slot(slot)) {
        return -1;
    }

    /* slot1=外部SIM=ril_1, slot2=eSIM=ril_0 */
    if (strcmp(slot, "slot1") == 0) {
        strcpy(target_ril, "/ril_1");
        strcpy(other_ril, "/ril_0");
    } else {
        strcpy(target_ril, "/ril_0");
        strcpy(other_ril, "/ril_1");
    }

    /* 步骤1: 把当前卡槽设置为 LTE only (mode=5) */
    ofono_network_set_mode_sync(other_ril, MODE_LTE_ONLY, OFONO_TIMEOUT_MS);
    usleep(500 * 1000);  /* 500ms */

    /* 步骤2: 设置当前 ril 在线状态为 0 (关闭) */
    ofono_modem_set_online(other_ril, 0, OFONO_TIMEOUT_MS);

    /* 步骤3: 设置目标 ril 在线状态为 1 (开启) */
    ofono_modem_set_online(target_ril, 1, OFONO_TIMEOUT_MS);

    /* 步骤4: 设置数据卡为目标 ril (ofono_set_datacard 返回1=成功,0=失败) */
    if (ofono_set_datacard(target_ril) == 0) {
        printf("[Modem] SetDataCard 失败，回退到 sim_config + reboot\n");
        return switch_slot_via_config(target_ril);
    }

    /* 等待系统状态更新 */
    sleep(1);

    /* 步骤5: 把目标卡槽设置为 auto 模式 (mode=9) */
    ofono_network_set_mode_sync(target_ril, MODE_NR_5G_LTE_AUTO, OFONO_TIMEOUT_MS);

    /* 验证切换结果 */
    if (get_current_slot(new_slot, new_ril) != 0) {
        printf("[Modem] 切换验证失败，回退到 sim_config + reboot\n");
        return switch_slot_via_config(target_ril);
    }

    /* 步骤6: 重启数据连接监听（切换到新卡槽） */
    if (ofono_is_data_monitor_running()) {
        printf("[Modem] 切卡完成，重启数据连接监听...\n");
        ofono_stop_data_monitor();
        ofono_start_data_monitor();
    }

    return 0;
}
