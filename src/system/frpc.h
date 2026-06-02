/**
 * @file frpc.h
 * @brief Sakura Frp (frpc) 内网穿透控制模块 - 配置管理、进程控制、日志查看
 *
 * 功能特性:
 * - 多隧道转发配置管理
 * - INI 配置文件生成
 * - frpc 客户端进程管理
 * - 实时日志读取
 * - 开机自启动支持
 */

#ifndef FRPC_H
#define FRPC_H

#include <time.h>

/*============================================================================
 * 常量定义
 *============================================================================*/

#define FRPC_MAX_PROXIES          16       /* 最大隧道数量 */
#define FRPC_NAME_SIZE            64       /* 隧道名称最大长度 */
#define FRPC_TOKEN_SIZE           128      /* Token 最大长度 */
#define FRPC_ADDR_SIZE            64       /* 地址最大长度 */
#define FRPC_TYPE_SIZE            16       /* 类型最大长度 (tcp/udp/http) */
#define FRPC_LOG_MAX_LINES        100      /* 日志最大行数 */

/* 路径配置 */
#define FRPC_BIN_PATH             "/home/root/6677/frpc"
#define FRPC_CONFIG_PATH          "/home/root/6677/frpc.ini"
#define FRPC_LOG_PATH             "/tmp/frpc.log"
#define FRPC_PID_PATH             "/tmp/frpc.pid"

/*============================================================================
 * 数据结构
 *============================================================================*/

/**
 * frpc 隧道配置
 */
typedef struct {
    int id;                              /* 数据库 ID */
    char name[FRPC_NAME_SIZE];           /* 隧道名称 (ssh/web) */
    char type[FRPC_TYPE_SIZE];           /* 协议类型 tcp/udp/http */
    char local_ip[FRPC_ADDR_SIZE];       /* 本地地址 127.0.0.1 */
    int local_port;                      /* 本地端口 */
    int remote_port;                     /* 远端端口 */
    int enabled;                         /* 是否启用 */
    time_t created_at;                   /* 创建时间 */
} FrpcProxy;

/**
 * frpc 全局配置
 */
typedef struct {
    char server_addr[FRPC_ADDR_SIZE];    /* 樱花服务器地址 */
    int server_port;                     /* 樱花服务器端口 */
    char token[FRPC_TOKEN_SIZE];         /* 访问密钥 Token */
    int auto_start;                      /* 自启动开关 0=关闭 1=开启 */
    int enabled;                         /* 总开关 0=禁用 1=启用 */
} FrpcConfig;

/**
 * frpc 运行状态
 */
typedef struct {
    int running;                         /* 是否运行中 0=停止 1=运行 */
    int pid;                             /* 进程 PID (-1 表示无进程) */
    int proxy_count;                     /* 配置的隧道数量 */
    char last_error[256];                /* 最后错误信息 */
} FrpcStatus;

/*============================================================================
 * 初始化和清理
 *============================================================================*/

/**
 * 初始化 frpc 模块
 * @param db_path 数据库路径 (可空，使用默认路径)
 * @return 0=成功 -1=失败
 */
int frpc_init(const char *db_path);

/**
 * 清理 frpc 模块资源
 */
void frpc_deinit(void);

/*============================================================================
 * 配置管理
 *============================================================================*/

/**
 * 获取 frpc 全局配置
 * @param config 输出配置结构体
 * @return 0=成功 -1=失败
 */
int frpc_get_config(FrpcConfig *config);

/**
 * 设置 frpc 全局配置
 * @param server_addr 服务器地址
 * @param server_port 服务器端口
 * @param token 访问密钥
 * @param auto_start 自启动开关
 * @param enabled 总使能开关
 * @return 0=成功 -1=失败
 */
int frpc_set_config(const char *server_addr, int server_port, const char *token,
                    int auto_start, int enabled);

/*============================================================================
 * 隧道管理
 *============================================================================*/

/**
 * 获取隧道列表
 * @param proxies 输出隧道数组
 * @param max_count 数组最大容量
 * @return 隧道数量 (<0 表示错误)
 */
int frpc_proxy_list(FrpcProxy *proxies, int max_count);

/**
 * 添加隧道
 * @param name 隧道名称 (必须唯一)
 * @param type 协议类型 tcp/udp/http
 * @param local_ip 本地地址
 * @param local_port 本地端口
 * @param remote_port 远端端口
 * @return 0=成功 -1=失败
 */
int frpc_proxy_add(const char *name, const char *type, const char *local_ip,
                   int local_port, int remote_port);

/**
 * 更新隧道
 * @param id 隧道 ID
 * @param name 隧道名称
 * @param type 协议类型
 * @param local_ip 本地地址
 * @param local_port 本地端口
 * @param remote_port 远端端口
 * @param enabled 是否启用
 * @return 0=成功 -1=失败
 */
int frpc_proxy_update(int id, const char *name, const char *type,
                      const char *local_ip, int local_port, int remote_port,
                      int enabled);

/**
 * 删除隧道
 * @param id 隧道 ID
 * @return 0=成功 -1=失败
 */
int frpc_proxy_delete(int id);

/*============================================================================
 * 进程控制
 *============================================================================*/

/**
 * 启动 frpc 客户端
 * @return 0=成功 -1=失败
 */
int frpc_start(void);

/**
 * 停止 frpc 客户端
 * @return 0=成功 -1=失败
 */
int frpc_stop(void);

/**
 * 重启 frpc 客户端
 * @return 0=成功 -1=失败
 */
int frpc_restart(void);

/**
 * 获取 frpc 运行状态
 * @param status 输出状态结构体 (可空)
 * @return 1=运行中 0=已停止
 */
int frpc_get_status(FrpcStatus *status);

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
int frpc_get_logs(char *buf, size_t size, int max_lines);

/**
 * 清空日志文件
 * @return 0=成功 -1=失败
 */
int frpc_clear_logs(void);

/*============================================================================
 * INI 配置生成
 *============================================================================*/

/**
 * 生成 INI 配置文件
 * @return 0=成功 -1=失败
 */
int frpc_generate_config(void);

#endif /* FRPC_H */
