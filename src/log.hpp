#pragma once
#include <time.h>
#include <stdio.h>

// 定义日志等级
#define INF "INFORMATION" // 信息日志宏
#define DBG "DEBUG"       // 调试日志宏
#define ERR "ERRO"        // 错误日志宏

// 默认日志等级为信息类日志
#define DEFAULT_LOG_LEVEL INF

// 日志宏函数
// 需要输出            时间        文件名 第几行    日志信息
// 例如      [2024-10-27 21:50:43 log.hpp:34][DEBUG] mysql init error
#define LOG(LEVEL, format, ...)                                                                           \
    do                                                                                                    \
    {                                                                                                     \
        if (DEFAULT_LOG_LEVEL > LEVEL)                                                                    \
            break;                                                                                        \
        time_t tp = time(nullptr);                                                                        \
        struct tm *lt = localtime(&tp);                                                                   \
        char buffer[32];                                                                                  \
        size_t n = strftime(buffer, sizeof(buffer) - 1, "%Y-%m-%d %H:%M:%S", lt);                         \
        fprintf(stdout, "[%s %s:%d][%s] " format "\n", buffer, __FILE__, __LINE__, LEVEL, ##__VA_ARGS__); \
    } while (0)

#define ILOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)