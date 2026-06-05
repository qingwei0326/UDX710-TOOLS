/**
 * @file airplane.c
 * @brief Airplane mode and SIM info (Go: system/airplane.go)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include "airplane.h"
#include "sysinfo.h"
#include "ofono.h"

int send_at(const char *cmd, char **result) {
    GDBusConnection *conn = NULL;
    GVariant *ret = NULL;
    GError *error = NULL;
    int rc = -1;
    char slot[16], ril_path[32];

    if (!cmd || !result) return -1;
    *result = NULL;

    /* 获取当前 RIL 路径 */
    if (get_current_slot(slot, ril_path) != 0 || strcmp(ril_path, "unknown") == 0) {
        strcpy(ril_path, "/ril_0");  /* 默认使用 ril_0 */
    }

    /* 连接系统 D-Bus */
    conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!conn) {
        if (error) g_error_free(error);
        return -1;
    }

    /* 调用 SendAtcmd */
    ret = g_dbus_connection_call_sync(
        conn,
        "org.ofono",
        ril_path,
        "org.ofono.Modem",
        "SendAtcmd",
        g_variant_new("(s)", cmd),
        G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE,
        8000,  /* 8秒超时 */
        NULL,
        &error
    );

    if (ret) {
        const gchar *res_str = NULL;
        g_variant_get(ret, "(s)", &res_str);
        if (res_str) {
            *result = g_strdup(res_str);
            rc = 0;
        }
        g_variant_unref(ret);
    }

    if (error) g_error_free(error);
    g_object_unref(conn);
    return rc;
}


int get_airplane_mode(void) {
    char *result = NULL;
    int mode = -1;

    if (send_at("AT+CFUN?", &result) == 0 && result) {
        if (strstr(result, "+CFUN: 0")) {
            mode = 1;  /* 飞行模式开启 */
        } else {
            mode = 0;  /* 正常模式 */
        }
        g_free(result);
    }
    return mode;
}

int set_airplane_mode(int enabled) {
    char slot[16], ril_path[32];
    int online = enabled ? 0 : 1;  /* 飞行模式开启=离线, 关闭=在线 */
    int rc;

    /* 获取当前 RIL 路径 */
    if (get_current_slot(slot, ril_path) != 0 || strcmp(ril_path, "unknown") == 0) {
        strcpy(ril_path, "/ril_0");  /* 默认使用 ril_0 */
    }

    if (enabled) {
        /* 开启飞行模式：先抑制 data monitor 自动恢复，再离线 */
        ofono_set_data_restore_suppressed(1);
        rc = ofono_modem_set_online(ril_path, online, OFONO_TIMEOUT_MS);
        if (rc != 0) {
            printf("[Airplane] D-Bus set_online 失败(rc=%d), 尝试 AT 命令\n", rc);
            char *at_result = NULL;
            if (send_at("AT+CFUN=4", &at_result) == 0) {
                rc = 0;  /* AT 命令成功 */
                printf("[Airplane] AT+CFUN=4 成功\n");
            }
            if (at_result) g_free(at_result);
            if (rc != 0) {
                ofono_set_data_restore_suppressed(0);
            }
        }
    } else {
        /* 关闭飞行模式：先在线，再解除抑制让 monitor 恢复数据连接 */
        rc = ofono_modem_set_online(ril_path, online, OFONO_TIMEOUT_MS);
        if (rc != 0) {
            printf("[Airplane] D-Bus set_online 失败(rc=%d), 尝试 AT 命令\n", rc);
            char *at_result = NULL;
            if (send_at("AT+CFUN=1", &at_result) == 0) {
                rc = 0;
                printf("[Airplane] AT+CFUN=1 成功\n");
            }
            if (at_result) g_free(at_result);
        }
        ofono_set_data_restore_suppressed(0);
    }

    return rc;
}

int get_iccid(char *iccid, size_t size) {
    char *result = NULL;
    int rc = -1;

    if (send_at("AT+CCID", &result) != 0 || !result) return -1;

    /* 解析 +CCID: "xxx" 或纯数字行 */
    char *lines = result;
    char *line = strtok(lines, "\n");
    while (line) {
        /* 去除首尾空白 */
        while (*line == ' ' || *line == '\t') line++;
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\r')) line[--len] = '\0';

        /* 检查 +CCID: 格式 */
        if (strncmp(line, "+CCID:", 6) == 0) {
            char *p = line + 6;
            while (*p == ' ') p++;
            /* 去除引号 */
            if (*p == '"') p++;
            char *end = strchr(p, '"');
            if (end) *end = '\0';
            if (strlen(p) >= 19) {
                strncpy(iccid, p, size - 1);
                iccid[size - 1] = '\0';
                rc = 0;
                break;
            }
        }
        /* 检查纯数字行 (19-22位) */
        else if (len >= 19 && len <= 22) {
            int is_hex = 1;
            for (size_t i = 0; i < len; i++) {
                char c = line[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                    is_hex = 0;
                    break;
                }
            }
            if (is_hex) {
                strncpy(iccid, line, size - 1);
                iccid[size - 1] = '\0';
                rc = 0;
                break;
            }
        }
        line = strtok(NULL, "\n");
    }

    g_free(result);
    return rc;
}


int get_imei(char *imei, size_t size) {
    char *result = NULL;
    int rc = -1;

    if (send_at("AT+SPIMEI?", &result) != 0 || !result) return -1;

    /* 解析响应，提取 15 位数字 */
    char *lines = result;
    char *line = strtok(lines, "\n");
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\r')) line[--len] = '\0';

        /* 检查 +SPIMEI: 格式 */
        if (strncmp(line, "+SPIMEI:", 8) == 0) {
            char *p = line + 8;
            while (*p == ' ') p++;
            if (strlen(p) == 15) {
                strncpy(imei, p, size - 1);
                imei[size - 1] = '\0';
                rc = 0;
                break;
            }
        }
        /* 检查 15 位纯数字 */
        else if (len == 15) {
            int is_digit = 1;
            for (size_t i = 0; i < len; i++) {
                if (line[i] < '0' || line[i] > '9') {
                    is_digit = 0;
                    break;
                }
            }
            if (is_digit) {
                strncpy(imei, line, size - 1);
                imei[size - 1] = '\0';
                rc = 0;
                break;
            }
        }
        line = strtok(NULL, "\n");
    }

    g_free(result);
    return rc;
}

int get_imsi(char *imsi, size_t size) {
    char *result = NULL;
    int rc = -1;

    if (send_at("AT+CIMI", &result) != 0 || !result) return -1;

    /* 解析响应，提取 15 位数字 */
    char *lines = result;
    char *line = strtok(lines, "\n");
    while (line) {
        while (*line == ' ' || *line == '\t') line++;
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\r')) line[--len] = '\0';

        /* 检查 15 位纯数字 */
        if (len == 15) {
            int is_digit = 1;
            for (size_t i = 0; i < len; i++) {
                if (line[i] < '0' || line[i] > '9') {
                    is_digit = 0;
                    break;
                }
            }
            if (is_digit) {
                strncpy(imsi, line, size - 1);
                imsi[size - 1] = '\0';
                rc = 0;
                break;
            }
        }
        line = strtok(NULL, "\n");
    }

    g_free(result);
    return rc;
}

const char *get_carrier_from_imsi(const char *imsi) {
    if (!imsi || strlen(imsi) < 5) return "未知";

    /* MCC = 前3位, MNC = 第4-5位 */
    char mcc[4] = {imsi[0], imsi[1], imsi[2], '\0'};
    char mnc[3] = {imsi[3], imsi[4], '\0'};

    /* 中国运营商 (MCC = 460) */
    if (strcmp(mcc, "460") == 0) {
        if (strcmp(mnc, "00") == 0 || strcmp(mnc, "02") == 0 ||
            strcmp(mnc, "04") == 0 || strcmp(mnc, "07") == 0 || strcmp(mnc, "08") == 0) {
            return "中国移动";
        }
        if (strcmp(mnc, "01") == 0 || strcmp(mnc, "06") == 0 || strcmp(mnc, "09") == 0) {
            return "中国联通";
        }
        if (strcmp(mnc, "03") == 0 || strcmp(mnc, "05") == 0 || strcmp(mnc, "11") == 0) {
            return "中国电信";
        }
        if (strcmp(mnc, "15") == 0) {
            return "中国广电";
        }
    }

    return "未知运营商";
}
