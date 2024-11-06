// 管理session模块
#pragma once

#include "util.hpp"

// session状态
typedef enum status
{
    UNLOGIN,
    LOGIN
} session_status;

class session
{
public:
    session(const uint64_t &ssid)
        : _ssid(ssid)
    {
        DLOG("SESSION %d 被创建", _ssid);
    }

    ~session()
    {
        DLOG("SESSION %d 被释放", _ssid);
    }

    // 获取session id
    uint64_t get_sesssion_id()
    {
        return _ssid;
    }

    // 获取用户id
    uint64_t get_user_id()
    {
        return _uid;
    }

    // 设置用户id
    void set_uid(const uint64_t &uid)
    {
        _uid = uid;
    }

    // 设置session状态
    void set_status(const session_status &status)
    {
        _status = status;
    }

    // 判断是否登录
    bool is_login()
    {
        if (_status == LOGIN)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    // 设置定时器
    void set_timer(const server_t::timer_ptr &tp)
    {
        _tp = tp;
    }

    // 获取定时器
    server_t::timer_ptr get_timer()
    {
        return _tp;
    }

private:
    uint64_t _ssid;          // session id
    uint64_t _uid;           // 用户id
    session_status _status;  // session状态
    server_t::timer_ptr _tp; // 定时器
};

// session管理类
class session_manager
{
public:
private:
};