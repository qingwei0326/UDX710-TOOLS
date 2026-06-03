/**
 * @file frpc.c
 * @brief Sakura Frp (frpc) 内网穿透控制模块实现
 *
 * 功能实现:
 * - SQLite 数据库配置存储
 * - INI 配置文件生成
 * - 进程启动/停止/状态检测
 * - 日志文件读取
 * - 开机自启动处理
 */

#include "frpc.h"
#include "database.h"
#include "exec_utils.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*============================================================================
 * 内部变量
 *============================================================================*/

static pthread_mutex_t g_frpc_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_frpc_initialized = 0;
static FrpcConfig g_current_config = {0};

/*============================================================================
 * 内部函数声明
 *============================================================================*/

static int create_frpc_tables(void);
static int load_frpc_config(void);
static int parse_proxy_row(const char *row, FrpcProxy *proxy);

/*============================================================================
 * 数据库表创建
 *============================================================================*/

static int create_frpc_tables(void) {
  const char *sql = "CREATE TABLE IF NOT EXISTS frpc_config ("
                    "id INTEGER PRIMARY KEY DEFAULT 1,"
                    "server_addr TEXT DEFAULT 'b1.xfrp.net',"
                    "server_port INTEGER DEFAULT 7000,"
                    "token TEXT DEFAULT '',"
                    "auto_start INTEGER DEFAULT 0,"
                    "enabled INTEGER DEFAULT 0"
                    ");"
                    "CREATE TABLE IF NOT EXISTS frpc_proxies ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name TEXT NOT NULL UNIQUE,"
                    "type TEXT NOT NULL DEFAULT 'tcp',"
                    "local_ip TEXT NOT NULL DEFAULT '127.0.0.1',"
                    "local_port INTEGER NOT NULL,"
                    "remote_port INTEGER NOT NULL,"
                    "enabled INTEGER DEFAULT 1,"
                    "created_at INTEGER NOT NULL"
                    ");";

  return db_execute(sql);
}

/*============================================================================
 * 配置加载
 *============================================================================*/

static int load_frpc_config(void) {
  char output[512];
  const char *sql = "SELECT server_addr || '|' || server_port || '|' || token "
                    "|| '|' || auto_start || '|' || enabled "
                    "FROM frpc_config WHERE id = 1;";

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_query_string(sql, output, sizeof(output));
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret == 0 && strlen(output) > 0) {
    char *fields[5] = {NULL};
    int field_count = 0;
    char *p = output;
    char *start = p;

    while (*p && field_count < 5) {
      if (*p == '|') {
        *p = '\0';
        fields[field_count++] = start;
        start = p + 1;
      }
      p++;
    }
    if (field_count < 5 && start) {
      fields[field_count++] = start;
    }

    if (field_count >= 5) {
      strncpy(g_current_config.server_addr, fields[0],
              sizeof(g_current_config.server_addr) - 1);
      g_current_config.server_port = atoi(fields[1]);
      strncpy(g_current_config.token, fields[2],
              sizeof(g_current_config.token) - 1);
      g_current_config.auto_start = atoi(fields[3]);
      g_current_config.enabled = atoi(fields[4]);
    }
  } else {
    /* 默认配置 - 樱花穿透默认服务器 */
    memset(&g_current_config, 0, sizeof(g_current_config));
    strncpy(g_current_config.server_addr, "b1.xfrp.net",
            sizeof(g_current_config.server_addr) - 1);
    g_current_config.server_port = 7000;
  }

  printf("[Frpc] 配置加载完成: 服务器=%s:%d, 自启动=%d, 启用=%d\n",
         g_current_config.server_addr, g_current_config.server_port,
         g_current_config.auto_start, g_current_config.enabled);
  return 0;
}

/*============================================================================
 * 初始化和清理
 *============================================================================*/

int frpc_init(const char *db_path) {
  if (g_frpc_initialized) {
    return 0;
  }

  printf("[Frpc] 初始化模块\n");

  /* 初始化数据库（如果未初始化） */
  if (db_path && strlen(db_path) > 0) {
    db_init(db_path);
  }

  /* 创建数据库表 */
  if (create_frpc_tables() != 0) {
    printf("[Frpc] 创建数据库表失败\n");
    return -1;
  }

  /* 加载配置 */
  load_frpc_config();

  /* 处理自启动 */
  if (g_current_config.enabled && g_current_config.auto_start) {
    printf("[Frpc] 检测到自启动配置，正在启动服务...\n");
    if (frpc_start() == 0) {
      printf("[Frpc] 自启动成功\n");
    } else {
      printf("[Frpc] 自启动失败\n");
    }
  }

  g_frpc_initialized = 1;
  printf("[Frpc] 模块初始化完成\n");
  return 0;
}

void frpc_deinit(void) {
  if (!g_frpc_initialized) {
    return;
  }

  frpc_stop();
  g_frpc_initialized = 0;
  printf("[Frpc] 模块已清理\n");
}

/*============================================================================
 * 配置管理
 *============================================================================*/

int frpc_get_config(FrpcConfig *config) {
  if (!config) {
    return -1;
  }

  pthread_mutex_lock(&g_frpc_mutex);
  memcpy(config, &g_current_config, sizeof(FrpcConfig));
  pthread_mutex_unlock(&g_frpc_mutex);

  return 0;
}

int frpc_set_config(const char *server_addr, int server_port, const char *token,
                    int auto_start, int enabled) {
  char sql[1024];
  char escaped_addr[256];
  char escaped_token[256];

  if (!server_addr) server_addr = "";
  if (!token) token = "";

  db_escape_string(server_addr, escaped_addr, sizeof(escaped_addr));
  db_escape_string(token, escaped_token, sizeof(escaped_token));

  snprintf(sql, sizeof(sql),
           "INSERT OR REPLACE INTO frpc_config (id, server_addr, server_port, "
           "token, auto_start, enabled) "
           "VALUES (1, '%s', %d, '%s', %d, %d);",
           escaped_addr, server_port, escaped_token, auto_start ? 1 : 0,
           enabled ? 1 : 0);

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_execute(sql);
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret != 0) {
    printf("[Frpc] 保存配置失败\n");
    return -1;
  }

  strncpy(g_current_config.server_addr, server_addr,
          sizeof(g_current_config.server_addr) - 1);
  g_current_config.server_port = server_port;
  strncpy(g_current_config.token, token, sizeof(g_current_config.token) - 1);
  g_current_config.auto_start = auto_start;
  g_current_config.enabled = enabled;

  printf("[Frpc] 配置保存成功: 服务器=%s:%d\n", server_addr, server_port);
  return 0;
}

/*============================================================================
 * 隧道管理
 *============================================================================*/

static int parse_proxy_row(const char *row, FrpcProxy *proxy) {
  if (!row || !proxy || strlen(row) == 0) {
    return -1;
  }

  char buf[1024];
  strncpy(buf, row, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *fields[8] = {NULL};
  int field_count = 0;
  char *p = buf;
  char *start = p;

  while (*p && field_count < 8) {
    if (*p == '|') {
      *p = '\0';
      fields[field_count++] = start;
      start = p + 1;
    }
    p++;
  }
  if (field_count < 8 && start) {
    fields[field_count++] = start;
  }

  if (field_count < 8) {
    return -1;
  }

  memset(proxy, 0, sizeof(FrpcProxy));
  proxy->id = atoi(fields[0]);
  strncpy(proxy->name, fields[1], sizeof(proxy->name) - 1);
  strncpy(proxy->type, fields[2], sizeof(proxy->type) - 1);
  strncpy(proxy->local_ip, fields[3], sizeof(proxy->local_ip) - 1);
  proxy->local_port = atoi(fields[4]);
  proxy->remote_port = atoi(fields[5]);
  proxy->enabled = atoi(fields[6]);
  proxy->created_at = (time_t)atol(fields[7]);

  return 0;
}

int frpc_proxy_list(FrpcProxy *proxies, int max_count) {
  char *output = NULL;

  if (!proxies || max_count <= 0) {
    return -1;
  }

  output = (char *)malloc(32 * 1024);
  if (!output) {
    return -1;
  }

  const char *sql =
      "SELECT id || '|' || name || '|' || type || '|' || local_ip || '|' || "
      "local_port || '|' || remote_port || '|' || enabled || '|' || created_at "
      "FROM frpc_proxies ORDER BY id ASC;";

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_query_string(sql, output, 32 * 1024);
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret != 0 || strlen(output) == 0) {
    free(output);
    return 0;
  }

  int count = 0;
  char *line = output;
  char *next_line;

  while (line && *line && count < max_count) {
    next_line = strchr(line, '\n');
    if (next_line) {
      *next_line = '\0';
      next_line++;
    }

    if (strlen(line) == 0) {
      line = next_line;
      continue;
    }

    if (parse_proxy_row(line, &proxies[count]) == 0) {
      count++;
    }

    line = next_line;
  }

  free(output);
  printf("[Frpc] 获取到 %d 个隧道\n", count);
  return count;
}

int frpc_proxy_add(const char *name, const char *type, const char *local_ip,
                   int local_port, int remote_port) {
  char sql[1024];
  char escaped_name[128];
  char escaped_type[32];
  char escaped_ip[128];

  if (!name || !type || !local_ip || strlen(name) == 0 || local_port <= 0 ||
      remote_port <= 0) {
    printf("[Frpc] 隧道参数无效\n");
    return -1;
  }

  db_escape_string(name, escaped_name, sizeof(escaped_name));
  db_escape_string(type, escaped_type, sizeof(escaped_type));
  db_escape_string(local_ip, escaped_ip, sizeof(escaped_ip));

  time_t now = time(NULL);

  snprintf(sql, sizeof(sql),
           "INSERT INTO frpc_proxies (name, type, local_ip, local_port, "
           "remote_port, enabled, created_at) "
           "VALUES ('%s', '%s', '%s', %d, %d, 1, %ld);",
           escaped_name, escaped_type, escaped_ip, local_port, remote_port,
           (long)now);

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_execute(sql);
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret == 0) {
    printf("[Frpc] 隧道添加成功: %s -> %s:%d\n", name, local_ip, local_port);
  } else {
    printf("[Frpc] 隧道添加失败\n");
  }

  return ret;
}

int frpc_proxy_update(int id, const char *name, const char *type,
                      const char *local_ip, int local_port, int remote_port,
                      int enabled) {
  char sql[1024];
  char escaped_name[128];
  char escaped_type[32];
  char escaped_ip[128];

  if (id <= 0) {
    return -1;
  }

  if (!name || !type || !local_ip || strlen(name) == 0 || local_port <= 0 ||
      remote_port <= 0) {
    printf("[Frpc] 隧道参数无效\n");
    return -1;
  }

  db_escape_string(name, escaped_name, sizeof(escaped_name));
  db_escape_string(type, escaped_type, sizeof(escaped_type));
  db_escape_string(local_ip, escaped_ip, sizeof(escaped_ip));

  snprintf(sql, sizeof(sql),
           "UPDATE frpc_proxies SET name='%s', type='%s', local_ip='%s', "
           "local_port=%d, remote_port=%d, enabled=%d WHERE id=%d;",
           escaped_name, escaped_type, escaped_ip, local_port, remote_port,
           enabled ? 1 : 0, id);

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_execute(sql);
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret == 0) {
    printf("[Frpc] 隧道更新成功: ID=%d\n", id);
  } else {
    printf("[Frpc] 隧道更新失败\n");
  }

  return ret;
}

int frpc_proxy_delete(int id) {
  char sql[128];

  if (id <= 0) {
    return -1;
  }

  snprintf(sql, sizeof(sql), "DELETE FROM frpc_proxies WHERE id = %d;", id);

  pthread_mutex_lock(&g_frpc_mutex);
  int ret = db_execute(sql);
  pthread_mutex_unlock(&g_frpc_mutex);

  if (ret == 0) {
    printf("[Frpc] 隧道删除成功: ID=%d\n", id);
  } else {
    printf("[Frpc] 隧道删除失败\n");
  }

  return ret;
}

/*============================================================================
 * INI 配置生成
 *============================================================================*/

int frpc_generate_config(void) {
  FILE *fp;
  FrpcProxy proxies[FRPC_MAX_PROXIES];
  int count;

  /* 检查必要配置 */
  if (strlen(g_current_config.server_addr) == 0) {
    printf("[Frpc] 服务器地址未配置\n");
    return -1;
  }

  if (strlen(g_current_config.token) == 0) {
    printf("[Frpc] Token 未配置\n");
    return -1;
  }

  /* 获取隧道列表 */
  count = frpc_proxy_list(proxies, FRPC_MAX_PROXIES);
  if (count <= 0) {
    printf("[Frpc] 没有配置任何隧道\n");
    return -1;
  }

  /* 生成 INI 文件 */
  fp = fopen(FRPC_CONFIG_PATH, "w");
  if (!fp) {
    printf("[Frpc] 无法创建配置文件: %s\n", FRPC_CONFIG_PATH);
    return -1;
  }

  /* 写入 [common] 配置 */
  fprintf(fp, "# Sakura Frp Client Configuration\n");
  fprintf(fp, "# Auto-generated by Web Management\n\n");
  fprintf(fp, "[common]\n");
  fprintf(fp, "server_addr = %s\n", g_current_config.server_addr);
  fprintf(fp, "server_port = %d\n", g_current_config.server_port);
  fprintf(fp, "token = %s\n\n", g_current_config.token);

  /* 写入隧道配置 */
  for (int i = 0; i < count; i++) {
    if (!proxies[i].enabled) {
      continue;
    }

    fprintf(fp, "[%s]\n", proxies[i].name);
    fprintf(fp, "type = %s\n", proxies[i].type);
    fprintf(fp, "local_ip = %s\n", proxies[i].local_ip);
    fprintf(fp, "local_port = %d\n", proxies[i].local_port);
    fprintf(fp, "remote_port = %d\n\n", proxies[i].remote_port);
  }

  fclose(fp);
  printf("[Frpc] 配置文件已生成: %s\n", FRPC_CONFIG_PATH);
  return 0;
}

/*============================================================================
 * 进程控制
 *============================================================================*/

int frpc_start(void) {
  char cmd[512];
  char output[256];

  /* 先强制停止所有可能存在的 frpc 进程 */
  printf("[Frpc] 启动前先清理可能存在的进程...\n");
  snprintf(cmd, sizeof(cmd), "pkill -9 -f '%s' 2>/dev/null; sleep 0.5",
           FRPC_BIN_PATH);
  run_command(output, sizeof(output), "sh", "-c", cmd, NULL);

  /* 清理 PID 文件 */
  unlink(FRPC_PID_PATH);

  /* 再次检查是否已运行 */
  if (frpc_get_status(NULL) == 1) {
    printf("[Frpc] 清理后仍有进程运行，再次强制终止\n");
    snprintf(cmd, sizeof(cmd), "pkill -9 -f '%s'", FRPC_BIN_PATH);
    run_command(output, sizeof(output), "sh", "-c", cmd, NULL);
    usleep(500000);
  }

  /* 检查可执行文件 */
  if (access(FRPC_BIN_PATH, X_OK) != 0) {
    printf("[Frpc] 可执行文件不存在或无执行权限: %s\n", FRPC_BIN_PATH);
    return -1;
  }

  /* 生成配置文件 */
  if (frpc_generate_config() != 0) {
    printf("[Frpc] 生成配置文件失败\n");
    return -1;
  }

  /* 清空旧日志 */
  frpc_clear_logs();

  /* 启动进程 */
  snprintf(cmd, sizeof(cmd), "nohup %s -c %s > %s 2>&1 & echo $!",
           FRPC_BIN_PATH, FRPC_CONFIG_PATH, FRPC_LOG_PATH);

  if (run_command(output, sizeof(output), "sh", "-c", cmd, NULL) != 0) {
    printf("[Frpc] 启动命令执行失败\n");
    return -1;
  }

  /* 保存 PID */
  int pid = atoi(output);
  if (pid > 0) {
    FILE *fp = fopen(FRPC_PID_PATH, "w");
    if (fp) {
      fprintf(fp, "%d\n", pid);
      fclose(fp);
    }
  }

  /* 等待一下检查是否成功启动 */
  usleep(500000);

  if (frpc_get_status(NULL) == 1) {
    printf("[Frpc] 服务启动成功, PID=%d\n", pid);
    return 0;
  } else {
    printf("[Frpc] 服务启动后立即退出，请检查日志\n");
    return -1;
  }
}

int frpc_stop(void) {
  char cmd[256];
  char output[128];

  printf("[Frpc] 停止服务\n");

  /* 方式1: 通过 PID 文件终止 */
  FILE *fp = fopen(FRPC_PID_PATH, "r");
  if (fp) {
    int pid = 0;
    if (fscanf(fp, "%d", &pid) == 1 && pid > 0) {
      printf("[Frpc] 从PID文件读取到PID=%d，终止进程\n", pid);
      kill(pid, SIGTERM);
      usleep(300000);
      kill(pid, SIGKILL);
    }
    fclose(fp);
  }

  /* 方式2: 通过 pkill 终止 */
  snprintf(cmd, sizeof(cmd), "pkill -f '%s'", FRPC_BIN_PATH);
  run_command(output, sizeof(output), "sh", "-c", cmd, NULL);
  usleep(300000);

  /* 方式3: 强制终止 */
  snprintf(cmd, sizeof(cmd), "pkill -9 -f '%s'", FRPC_BIN_PATH);
  run_command(output, sizeof(output), "sh", "-c", cmd, NULL);
  usleep(300000);

  /* 清理 PID 文件 */
  unlink(FRPC_PID_PATH);

  if (frpc_get_status(NULL) == 0) {
    printf("[Frpc] 服务已停止\n");
    return 0;
  } else {
    printf("[Frpc] 警告：可能仍有进程未完全停止\n");
    return 0;
  }
}

int frpc_restart(void) {
  frpc_stop();
  usleep(500000);
  return frpc_start();
}

int frpc_get_status(FrpcStatus *status) {
  char output[256] = {0};
  int running = 0;
  int pid = -1;

  /* 方式1: 检查 PID 文件 */
  FILE *fp = fopen(FRPC_PID_PATH, "r");
  if (fp) {
    int saved_pid = 0;
    if (fscanf(fp, "%d", &saved_pid) == 1 && saved_pid > 0) {
      if (kill(saved_pid, 0) == 0) {
        running = 1;
        pid = saved_pid;
      }
    }
    fclose(fp);
  }

  /* 方式2: ps + grep */
  if (!running) {
    if (run_command(output, sizeof(output), "sh", "-c",
                    "ps | grep '[f]rpc' | grep -v grep", NULL) == 0 &&
        strlen(output) > 0) {
      running = 1;
      char *p = output;
      while (*p == ' ')
        p++;
      pid = atoi(p);
    }
  }

  if (status) {
    memset(status, 0, sizeof(FrpcStatus));
    status->running = running;
    status->pid = pid;

    const char *sql = "SELECT COUNT(*) FROM frpc_proxies WHERE enabled = 1;";
    status->proxy_count = db_query_int(sql, 0);
  }

  return running;
}

/*============================================================================
 * 日志管理
 *============================================================================*/

int frpc_get_logs(char *buf, size_t size, int max_lines) {
  char cmd[512];

  if (!buf || size == 0) {
    return -1;
  }

  buf[0] = '\0';

  if (access(FRPC_LOG_PATH, R_OK) != 0) {
    return 0;
  }

  if (max_lines > 0) {
    snprintf(cmd, sizeof(cmd), "tail -n %d '%s'", max_lines, FRPC_LOG_PATH);
  } else {
    snprintf(cmd, sizeof(cmd), "cat '%s'", FRPC_LOG_PATH);
  }

  if (run_command(buf, size, "sh", "-c", cmd, NULL) != 0) {
    buf[0] = '\0';
    return -1;
  }

  return (int)strlen(buf);
}

int frpc_clear_logs(void) {
  FILE *fp = fopen(FRPC_LOG_PATH, "w");
  if (fp) {
    fclose(fp);
    printf("[Frpc] 日志已清空\n");
    return 0;
  }
  return -1;
}

/*============================================================================
 * 客户端下载
 *============================================================================*/

#define FRPC_DOWNLOAD_STATUS_PATH "/tmp/frpc_download_status"
#define FRPC_DOWNLOAD_LOG_PATH    "/tmp/frpc_download.log"
#define FRPC_DOWNLOAD_URL         "https://nya.globalslb.net/natfrp/client/launcher-unix/3.1.8/natfrp-service_linux_arm64.tar.zst"

int frpc_download_binary(void) {
  /* 检查是否已在下载（status=1 表示进行中） */
  FILE *fp = fopen(FRPC_DOWNLOAD_STATUS_PATH, "r");
  if (fp) {
    char status[16] = {0};
    if (fgets(status, sizeof(status), fp)) {
      int s = atoi(status);
      fclose(fp);
      if (s == 1) {
        /* 检查下载进程是否还活着 */
        char cmd[256];
        char output[64] = {0};
        snprintf(cmd, sizeof(cmd), "ps | grep 'natfrp.tar.zst' | grep -v grep");
        if (run_command(output, sizeof(output), "sh", "-c", cmd, NULL) != 0
            || strlen(output) == 0) {
          /* 进程不在了，清理残留状态 */
          printf("[Frpc] 清理残留下载状态\n");
          unlink(FRPC_DOWNLOAD_STATUS_PATH);
        } else {
          printf("[Frpc] 下载正在进行中\n");
          return -1;
        }
      }
    } else {
      fclose(fp);
    }
  }

  /* 检查 curl 是否可用 */
  char curl_check[64] = {0};
  if (run_command(curl_check, sizeof(curl_check), "sh", "-c", "which curl", NULL) != 0
      || strlen(curl_check) == 0) {
    printf("[Frpc] curl 未安装\n");
    return -2;
  }

  /* 检查目标路径是否可写 */
  FILE *test = fopen(FRPC_BIN_PATH, "w");
  if (!test) {
    printf("[Frpc] 目标路径不可写: %s\n", FRPC_BIN_PATH);
    return -3;
  }
  fclose(test);
  unlink(FRPC_BIN_PATH);  /* 删除测试文件 */

  /* 设置状态为下载中 */
  fp = fopen(FRPC_DOWNLOAD_STATUS_PATH, "w");
  if (fp) {
    fprintf(fp, "1\n");
    fclose(fp);
  }

  printf("[Frpc] 开始下载客户端...\n");

  /* fork 子进程执行下载 */
  pid_t pid = fork();
  if (pid == 0) {
    /* 子进程 */
    char cmd[1024];

    /* 清空日志 */
    FILE *logfp = fopen(FRPC_DOWNLOAD_LOG_PATH, "w");
    if (logfp) fclose(logfp);

    /* 下载 */
    snprintf(cmd, sizeof(cmd),
             "curl -sL '%s' -o /tmp/natfrp.tar.zst "
             "&& echo '下载完成' >> '%s' "
             "&& zstd -d /tmp/natfrp.tar.zst -o /tmp/natfrp.tar -f "
             "&& echo '解压zstd完成' >> '%s' "
             "&& tar xf /tmp/natfrp.tar -C /tmp/ "
             "&& echo '解压tar完成' >> '%s' "
             "&& cp /tmp/natfrp-service '%s' "
             "&& chmod 755 '%s' "
             "&& rm -f /tmp/natfrp.tar.zst /tmp/natfrp.tar /tmp/natfrp-service "
             "&& echo '2' > '%s' "
             "&& echo '安装完成' >> '%s' "
             "|| (echo '3' > '%s' && echo '下载失败' >> '%s')",
             FRPC_DOWNLOAD_URL,
             FRPC_DOWNLOAD_LOG_PATH,
             FRPC_DOWNLOAD_LOG_PATH,
             FRPC_DOWNLOAD_LOG_PATH,
             FRPC_BIN_PATH, FRPC_BIN_PATH,
             FRPC_DOWNLOAD_STATUS_PATH,
             FRPC_DOWNLOAD_LOG_PATH,
             FRPC_DOWNLOAD_STATUS_PATH,
             FRPC_DOWNLOAD_LOG_PATH);

    execl("/bin/sh", "sh", "-c", cmd, NULL);
    _exit(1);
  }

  if (pid < 0) {
    printf("[Frpc] fork 失败\n");
    fp = fopen(FRPC_DOWNLOAD_STATUS_PATH, "w");
    if (fp) {
      fprintf(fp, "3\n");
      fclose(fp);
    }
    return -1;
  }

  printf("[Frpc] 下载子进程已启动, PID=%d\n", pid);
  return 0;
}

int frpc_get_download_status(void) {
  FILE *fp = fopen(FRPC_DOWNLOAD_STATUS_PATH, "r");
  if (!fp) {
    return 0;  /* 未开始 */
  }
  char status[16] = {0};
  if (fgets(status, sizeof(status), fp)) {
    fclose(fp);
    return atoi(status);
  }
  fclose(fp);
  return 0;
}

int frpc_get_download_log(char *buf, size_t size) {
  if (!buf || size == 0) return -1;
  buf[0] = '\0';

  FILE *fp = fopen(FRPC_DOWNLOAD_LOG_PATH, "r");
  if (!fp) return 0;
  size_t n = fread(buf, 1, size - 1, fp);
  buf[n] = '\0';
  fclose(fp);
  return (int)n;
}
