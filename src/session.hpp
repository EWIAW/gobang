// 管理session模块
#pragma once

#include <mutex>
#include <unordered_map>
#include <memory>
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

#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1

typedef std::shared_ptr<session> session_ptr;

// session管理类
class session_manager
{
public:
    session_manager(server_t *server)
        : _next_ssid(1), _server(server)
    {
        DLOG("session管理模块初始化完成");
    }

    ~session_manager()
    {
        DLOG("session管理模块销毁完毕");
    }

    // 创建session
    session_ptr create_session(const uint64_t &uid, const session_status &status)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr sp(new session(_next_ssid));
        sp->set_uid(uid);
        sp->set_status(status);
        _session.insert(std::make_pair(_next_ssid, sp));
        _next_ssid++;
        return sp;
    }

    // 重新添加session
    void append_session(const session_ptr &sp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(sp->get_sesssion_id(), sp));
        return;
    }

    // 通过session id获取session
    session_ptr get_session_by_id(const uint64_t &sid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(sid);
        if (it == _session.end())
        {
            return session_ptr();
        }
        return it->second;
    }

    // 销毁session
    void remove_session(const uint64_t &sid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(sid);
    }

    // 设置session生命周期
    void set_session_expire_time(const uint64_t &sid, const int ms)
    {
        // 先查找要设置的session是否存在
        session_ptr sp = get_session_by_id(sid);
        if (sp == nullptr)
        {
            return;
        }
        // 获取定时器
        server_t::timer_ptr tp = sp->get_timer();
        // 1.没有定时器，且session的生命周期为永久
        // 2.没有定时器，且session的生命周期存在
        // 3.有定时器，且session的生命周期为永久，重置定时器为永久
        // 4.有定时器，且session的生命周期存在，重置定时器时间
        if (tp.get() == nullptr && ms == SESSION_FOREVER)
        {
            return;
        }
        else if (tp.get() == nullptr && ms != SESSION_FOREVER)
        {
            server_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(remove_session, this, sid));
            sp->set_timer(tmp_tp);
        }
        else if (tp.get() != nullptr && ms == SESSION_FOREVER)
        {
            tp->cancel(); // 取消定时器，但是取消定时器后，所定时的任务就会被执行
            sp->set_timer(server_t::timer_ptr());
            _server->set_timer(0, std::bind(append_session, this, sp));
        }
        else if (tp.get() != nullptr && ms != SESSION_FOREVER)
        {
            tp->cancel();
            sp->set_timer(server_t::timer_ptr());
            _server->set_timer(0, std::bind(append_session, this, sp));
            // 重新设置时间
            server_t::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(remove_session, this, sid));
            sp->set_timer(tmp_tp);
        }
        return;
    }

private:
    uint64_t _next_ssid; // 给session分配的id
    std::mutex _mutex;
    std::unordered_map<uint64_t, session_ptr> _session; // 存储session id和session对象的映射关系
    server_t *_server;
};