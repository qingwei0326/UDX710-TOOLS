/**
 * @file phone_case.h
 * @brief 手机壳模式模块头文件
 */

#ifndef PHONE_CASE_H
#define PHONE_CASE_H

/**
 * 初始化手机壳模式模块
 * 检查开机自启动配置，如启用则自动启动监控
 */
void phone_case_init(void);

/**
 * 清理手机壳模式模块
 */
void phone_case_deinit(void);

/**
 * 启动监控
 * @return 0=成功, -1=失败
 */
int phone_case_start(void);

/**
 * 停止监控
 * @return 0=成功
 */
int phone_case_stop(void);

/**
 * 获取当前状态
 * @return 1=运行中, 0=已停止
 */
int phone_case_get_status(void);

/**
 * 设置开关状态
 * @param enabled 1=开启, 0=关闭
 * @return 0=成功, -1=失败
 */
int phone_case_set_enabled(int enabled);

/* HTTP API 处理函数 */
struct mg_connection;
struct mg_http_message;
void handle_phone_case(struct mg_connection *c, struct mg_http_message *hm);

#endif /* PHONE_CASE_H */
