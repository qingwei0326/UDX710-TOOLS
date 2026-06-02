/**
 * @file netif.h
 * @brief 网络接口监控模块头文件
 */

#ifndef NETIF_H
#define NETIF_H

#include "mongoose.h"
#include <pthread.h>
#include <sys/types.h>
#include <time.h>

/* 最大接口数量 */
#define MAX_NET_INTERFACES 16

/* 网络接口信息结构体 */
typedef struct {
    char name[32];          /* 接口名称 */
    char hwaddr[32];        /* MAC地址 */
    char inet_addr[32];     /* IPv4地址 */
    char inet6_addr[64];    /* IPv6地址 */
    char mask[32];          /* 子网掩码 */
    int is_up;              /* 是否启用 */
    int monitoring;         /* 是否监听中 */
} NetInterface;

/* 实时流量统计结构体 */
typedef struct {
    int index;
    int seconds;
    struct {
        char ratestring[32];
        long long bytespersecond;
        long long packetspersecond;
        long long bytes;
        long long packets;
        long long totalbytes;
        long long totalpackets;
    } rx;
    struct {
        char ratestring[32];
        long long bytespersecond;
        long long packetspersecond;
        long long bytes;
        long long packets;
        long long totalbytes;
        long long totalpackets;
    } tx;
} NetifStats;

/* 网络接口监听器 */
typedef struct {
    char ifname[32];           /* 接口名称 */
    pid_t vnstat_pid;          /* vnstat -l 进程ID,0表示未运行 */
    pthread_t reader_thread;   /* 读取线程ID */
    int pipefd;                /* 管道文件描述符 */
    NetifStats latest_stats;   /* 最新统计数据缓存 */
    pthread_mutex_t lock;      /* 数据锁 */
    int running;               /* 运行状态: 1=运行中, 0=已停止 */
    time_t last_update;        /* 最后更新时间 */
} NetifMonitor;

/**
 * 初始化网络接口模块
 */
void init_netif(void);

/**
 * 获取所有网络接口列表
 * @param interfaces 接口数组
 * @param max_count 最大数量
 * @return 实际接口数量
 */
int netif_get_list(NetInterface *interfaces, int max_count);

/**
 * 获取指定接口的实时流量统计
 * @param ifname 接口名称
 * @param stats 统计数据输出
 * @return 0成功，-1失败
 */
int netif_get_stats(const char *ifname, NetifStats *stats);

/**
 * 获取接口监听状态
 * @param ifname 接口名称
 * @return 1监听中，0未监听
 */
int netif_get_monitor_status(const char *ifname);

/**
 * 设置接口监听状态
 * @param ifname 接口名称
 * @param enabled 是否启用
 * @return 0成功，-1失败
 */
int netif_set_monitor(const char *ifname, int enabled);

/**
 * 启动接口监听进程
 * @param ifname 接口名称
 * @return 0成功，-1失败
 */
int netif_start_monitor_process(const char *ifname);

/**
 * 停止接口监听进程
 * @param ifname 接口名称
 * @return 0成功，-1失败
 */
int netif_stop_monitor_process(const char *ifname);

/**
 * 清理所有监听进程
 */
void netif_cleanup_all_monitors(void);

/* HTTP API处理函数 */
void handle_netif_list(struct mg_connection *c, struct mg_http_message *hm);
void handle_netif_stats(struct mg_connection *c, struct mg_http_message *hm);
void handle_netif_monitor(struct mg_connection *c, struct mg_http_message *hm);

#endif /* NETIF_H */

