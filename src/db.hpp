#pragma once
#include <assert.h>
#include <stdio.h>

#include <string>
#include <mutex>
#include <memory>

#include "util.hpp"
#include "log.hpp"

// 管理数据库user表类，通过这个类的实例化来管理我们的user表
class user_table
{
public:
    // 构造函数，连接数据库
    user_table(const std::string &host,
               const std::string &username,
               const std::string &password,
               const std::string &dbname,
               const uint16_t &port = 3306)
    {
        _mysql = mysql_util::mysql_create(host, username, password, dbname, port);
        assert(_mysql != nullptr);
    }

    // 释放连接数据库
    ~user_table()
    {
        mysql_util::mysql_destroy(_mysql);
        _mysql = nullptr;
    }

    // 注册用户时，插入数据
    bool insert(const Json::Value &user)
    {
        // 判断用户名和密码是否有空
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("username or password is null");
            return false;
        }

// 如果用户名和密码不为空，则进行插入数据
#define INSERT_USER "insert into user values(null,'%s',password('%s'),1000,0,0);"
        char sql[4096] = {0}; // 要执行的sql语句
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("insert user failed : %s", sql);
            return false;
        }
        DLOG("insert user success : %s", sql);
        return true;
    }

    // 登录验证，并返回用户具体信息
    bool login(Json::Value &user)
    {
        // 判断用户名和密码是否有空
        if (user["username"].isNull() || user["password"].isNull())
        {
            DLOG("username or password is null");
            return false;
        }

// 执行select搜索语句
#define LOGIN_USER "select id,score,total_count,win_count from user where username='%s' and password=password('%s');"
        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());

        // 执行select语句后，需要获取搜索出来的信息，称结果集
        MYSQL_RES *res = nullptr;
        {
            // 进行select查询的时候，要进行互斥，因为我们要通过res获取查询出来的信息，如果不进行互斥有可能res获取的信息是不符合的
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("login failed");
                return false;
            }
            res = mysql_store_result(_mysql); // 获取结果集
            if (res == nullptr)
            {
                DLOG("The user information does not exist");
                return false;
            }
        }
        // 判断结果集数据是否为1
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("This user information is not unique");
            return false;
        }

        // 将用户信息写回json user
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res); // 是否结果集
        return true;
    }

    // 通过用户名查找用户数据
    bool select_by_name(const std::string &username, Json::Value &user)
    {
#define SELECT_BY_NAME "select id,score,total_count,win_count from user where username='%s';"
        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_NAME, username);

        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by name failed");
                return false;
            }
            // 获取结果集
            res = mysql_store_result(_mysql);
            if (res == nullptr)
            {
                DLOG("the user %s is not exist", username);
                return true;
            }
        }

        // 判断数据是否只有一条
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information is not unique");
            return false;
        }

        // 将信息写回json中
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["username"] = username;
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        mysql_free_result(res);
        return true;
    }

    // 通过用户id查找用户数据
    bool select_by_id(const uint64_t &id, Json::Value &user)
    {
#define SELECT_BY_ID "select username,score,total_count,win_count from user where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, SELECT_BY_ID, id);

        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            bool ret = mysql_util::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                DLOG("get user by id failed");
                return false;
            }
            // 获取结果集
            res = mysql_store_result(_mysql);
            if (res == nullptr)
            {
                DLOG("the user %d is not exist", id);
                return true;
            }
        }

        // 判断数据是否只有一条
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            DLOG("the user information is not unique");
            return false;
        }

        // 将信息写回json中
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        mysql_free_result(res);
        return true;
    }

    // 胜利时，分数+30，胜场+1，总场数+1
    bool win(const uint64_t &id)
    {
#define USER_WIN "update user set score=score+30,total_count=total_count+1,win_count=win_count+1 where id = %d;"
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update win user information failed");
            return false;
        }
        DLOG("update win user information success");
        return true;
    }

    // 失败时，分数-30，总场数+1
    bool lose(const uint64_t &id)
    {
#define USER_LOSE "update user set score=score-30,total_count=total_count+1 where id = %d;"
        char sql[4096] = {0};
        sprintf(sql, USER_LOSE, id);

        bool ret = mysql_util::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            DLOG("update lose user information failed");
            return false;
        }
        DLOG("update lose user information success");
        return true;
    }

private:
    MYSQL *_mysql;     // mysql句柄
    std::mutex _mutex; // 互斥锁，保证访问数据库安全
};