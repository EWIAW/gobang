#pragma once
#include <string>
#include <mysql/mysql.h>
#include "log.hpp"

// 封装数据库工具类
class mysql_util
{
public:
    // 数据库初始化和连接
    static MYSQL *mysql_create(const std::string host,
                               const std::string user,
                               const std::string password,
                               const std::string database,
                               const uint16_t port = 3306)
    {
        // 1.初始化MYSQL结构体
        MYSQL *mysql = mysql_init(nullptr);
        if (mysql == nullptr)
        {
            ELOG("mysql init error");
            return nullptr;
        }
        DLOG("mysql init success");

        // 2.连接数据库
        if (mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0) == nullptr)
        {
            ELOG("mysql connect error");
            mysql_close(mysql);
            return nullptr;
        }
        DLOG("mysql connect success");

        // 3.设置编码集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ELOG("mysql set character error");
            mysql_close(mysql);
            return nullptr;
        }
        DLOG("mysql set character success");

        return mysql;
    }

    // 执行数据库语句
    static bool mysql_exec()

    // 关闭连接数据库
};