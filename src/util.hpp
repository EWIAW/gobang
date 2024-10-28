#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>

#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>

#include "log.hpp"

// 封装数据库工具类
class mysql_util
{
public:
    // 数据库初始化和连接
    static MYSQL *mysql_create(const std::string &host,
                               const std::string &user,
                               const std::string &password,
                               const std::string &database,
                               const uint16_t &port = 3306)
    {
        // 1.初始化MYSQL结构体
        MYSQL *mysql = mysql_init(nullptr);
        if (mysql == nullptr)
        {
            ELOG("mysql init failed");
            return nullptr;
        }
        DLOG("mysql init success");

        // 2.连接数据库
        if (mysql_real_connect(mysql,
                               host.c_str(),
                               user.c_str(),
                               password.c_str(),
                               database.c_str(),
                               port, nullptr, 0) == nullptr)
        {
            ELOG("mysql connect failed : %s", mysql_errno(mysql));
            mysql_close(mysql);
            return nullptr;
        }
        DLOG("mysql connect success");

        // 3.设置编码集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            ELOG("mysql set character failed : %s", mysql_errno(mysql));
            mysql_close(mysql);
            return nullptr;
        }
        DLOG("mysql set character success");

        return mysql;
    }

    // 执行数据库语句
    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        if (mysql_query(mysql, sql.c_str()) != 0)
        {
            ELOG("%s", sql.c_str());
            ELOG("mysql query failed : %d", mysql_errno(mysql));
            return false;
        }
        DLOG("mysql query success : %s", sql.c_str());

        return true;
    }

    // 关闭连接数据库
    static void mysql_destroy(MYSQL *mysql)
    {
        if (mysql != nullptr)
        {
            mysql_close(mysql);
            DLOG("mysql close success");
        }
        return;
    }
};

// 封装jsoncpp工具类
class json_util
{
public:
    // 序列化   将结构化数据转换为字符串
    static bool serialize(const Json::Value &root, std::string &str)
    {
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter()); // 使用智能指针来管理
        std::stringstream ss;
        int ret = sw->write(root, &ss);
        if (ret != 0)
        {
            ELOG("json serialize failed");
            return false;
        }
        str = ss.str();
        DLOG("json serialize success");
        return true;
    }

    // 反序列化    将字符串转换为结构化数据
    static bool unserialize(const std::string &str, Json::Value &root)
    {
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader()); // 使用智能指针来管理
        std::string err;
        bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
        if (ret == false)
        {
            ELOG("json unserialize failed : %s", err.c_str());
            return false;
        }
        DLOG("json unserialize success");
        return true;
    }
};

// 封装字符串处理功能类
class string_util
{
public:
    // 进行字符串分割处理
    static size_t split(const std::string &src, const std::string &sep, std::vector<std::string> &res)
    {
        //"123,456,,,789"
        size_t left = 0, right = 0;
        while (right < src.size())
        {
            right = src.find(sep, right);
            if (right == std::string::npos)
            {
                res.push_back(src.substr(left, right));
                break;
            }
            //",123,456"    //处理特殊情况
            if (left == right)
            {
                right += sep.size();
                left = right;
                continue;
            }
            res.push_back(src.substr(left, right - left));
            right += sep.size();
            left = right;
        }
        return res.size();
    }
};