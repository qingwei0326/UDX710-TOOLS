/**
 * @file rathole.h
 * @brief Rathole 内网穿透控制模块 - 配置管理、进程控制、日志查看
 * 
 * 功能特性:
 * - 多服务转发配置管理
 * - TOML 配置文件生成
 * - Rathole 客户端进程管理
 * - 实时日志读取
 * - 开机自启动支持
 */

#ifndef RATHOLE_H
#define RATHOLE_H

#include <time.h>

/*============================================================================
 * 常量定义
 *============================================================================*/

#define RATHOLE_MAX_SERVICES        16       /* 最大服务数量 */
#define RATHOLE_NAME_SIZE           64       /* 服务名称最大长度 */
#define RATHOLE_TOKEN_SIZE          128      /* Token 最大长度 */
#define RATHOLE_ADDR_SIZE           64       /* 地址最大长度 */
#define RATHOLE_LOG_MAX_LINES       100      /* 日志最大行数 */

/* 路径配置 */
#define RATHOLE_BIN_PATH            "/home/root/6677/rathole"
#define RATHOLE_CONFIG_PATH         "/home/root/6677/client.toml"
#define RATHOLE_LOG_PATH            "/tmp/rathole.log"
#define RATHOLE_PID_PATH            "/tmp/rathole.pid"

/*============================================================================
 * 数据结构
 *============================================================================*/

/**
 * Rathole 转发服务配置
 */
typedef struct {
    int id;                              /* 数据库 ID */
    char name[RATHOLE_NAME_SIZE];        /* 服务名称 (web/ssh/api) */
    char token[RATHOLE_TOKEN_SIZE];      /* 认证 Token */
    char local_addr[RATHOLE_ADDR_SIZE];  /* 本地地址 127.0.0.1:80 */
    int enabled;                         /* 是否启用 */
    time_t created_at;                   /* 创建时间 */
} RatholeService;

/**
 * Rathole 全局配置
 */
typedef struct {
    char server_addr[RATHOLE_ADDR_SIZE]; /* 服务器地址 IP:PORT */
    int auto_start;                      /* 自启动开关 0=关闭 1=开启 */
    int enabled;                         /* 总开关 0=禁用 1=启用 */
} RatholeConfig;

/**
 * Rathole 运行状态
 */
typedef struct {
    int running;                         /* 是否运行中 0=停止 1=运行 */
    int pid;                             /* 进程 PID (-1 表示无进程) */
    int service_count;                   /* 配置的服务数量 */
    char last_error[256];                /* 最后错误信息 */
} RatholeStatus;

/*============================================================================
 * 初始化和清理
 *============================================================================*/

/**
 * 初始化 Rathole 模块
 * @param db_path 数据库路径 (可空，使用默认路径)
 * @return 0=成功 -1=失败
 */
int rathole_init(const char *db_path);

/**
 * 清理 Rathole 模块资源
 */
void rathole_deinit(void);

/*============================================================================
 * 配置管理
 *============================================================================*/

/**
 * 获取 Rathole 全局配置
 * @param config 输出配置结构体
 * @return 0=成功 -1=失败
 */
int rathole_get_config(RatholeConfig *config);

/**
 * 设置 Rathole 全局配置
 * @param server_addr 服务器地址 (IP:PORT 格式)
 * @param auto_start 自启动开关
 * @param enabled 总使能开关
 * @return 0=成功 -1=失败
 */
int rathole_set_config(const char *server_addr, int auto_start, int enabled);

/*============================================================================
 * 服务管理
 *============================================================================*/

/**
 * 获取服务列表
 * @param services 输出服务数组
 * @param max_count 数组最大容量
 * @return 服务数量 (<0 表示错误)
 */
int rathole_service_list(RatholeService *services, int max_count);

/**
 * 添加转发服务
 * @param name 服务名称 (必须唯一)
 * @param token 认证 Token
 * @param local_addr 本地地址
 * @return 0=成功 -1=失败
 */
int rathole_service_add(const char *name, const char *token, const char *local_addr);

/**
 * 更新转发服务
 * @param id 服务 ID
 * @param name 服务名称
 * @param token 认证 Token
 * @param local_addr 本地地址
 * @param enabled 是否启用
 * @return 0=成功 -1=失败
 */
int rathole_service_update(int id, const char *name, const char *token, 
                           const char *local_addr, int enabled);

/**
 * 删除转发服务
 * @param id 服务 ID
 * @return 0=成功 -1=失败
 */
int rathole_service_delete(int id);

/*============================================================================
 * 进程控制
 *============================================================================*/

/**
 * 启动 Rathole 客户端
 * 会自动生成 TOML 配置文件并启动进程
 * @return 0=成功 -1=失败
 */
int rathole_start(void);

/**
 * 停止 Rathole 客户端
 * @return 0=成功 -1=失败
 */
int rathole_stop(void);

/**
 * 重启 Rathole 客户端
 * @return 0=成功 -1=失败
 */
int rathole_restart(void);

/**
 * 获取 Rathole 运行状态
 * @param status 输出状态结构体 (可空)
 * @return 1=运行中 0=已停止
 */
int rathole_get_status(RatholeStatus *status);

/*============================================================================
 * 日志管理
 *============================================================================*/

/**
 * 获取运行日志
 * @param buf 输出缓冲区
 * @param size 缓冲区大小
 * @param max_lines 最大行数 (0=全部)
 * @return 实际读取的字节数 (<0 表示错误)
 */
int rathole_get_logs(char *buf, size_t size, int max_lines);

/**
 * 清空日志文件
 * @return 0=成功 -1=失败
 */
int rathole_clear_logs(void);

/*============================================================================
 * TOML 配置生成
 *============================================================================*/

/**
 * 生成 TOML 配置文件
 * @return 0=成功 -1=失败
 */
int rathole_generate_config(void);

#endif /* RATHOLE_H */
