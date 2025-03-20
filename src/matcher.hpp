// 匹配队列模块
#pragma once

#include <list>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include "online.hpp"
#include "room.hpp"
#include "util.hpp"
#include "db.hpp"

// 匹配队列
template <class T>
class match_queue
{
private:
    std::list<T> _queue; // 匹配队列
    std::mutex _mutex;
    std::condition_variable _cond; // 条件变量
public:
    // 获取元素个数
    size_t size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _queue.size();
    }

    // 判断队列是否为空
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _queue.empty();
    }

    // 阻塞线程
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);
        return;
    }

    // 入队列
    void push(const T &data)
    {
        // 因为STL库不是线程安全的，所以操作STL库时，需要加锁
        std::unique_lock<std::mutex> lock(_mutex);
        _queue.push_back(data); // 插入数据后，需要唤醒线程队列
        _cond.notify_all();
        return;
    }

    // 出队列
    bool pop(T &data)
    {
        // DLOG("进入pop函数");
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.empty() == true)
        {
            return false;
        }
        data = _queue.front();
        _queue.pop_front();
        return true;
    }

    // 移除指定数据
    void remove(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _queue.remove(data);
        return;
    }
};

// 管理匹配队列
class matcher
{
private:
    match_queue<uint64_t> _queue_bronze; // 青铜匹配队列
    match_queue<uint64_t> _queue_sliver; // 白银匹配队列
    match_queue<uint64_t> _queue_gold;   // 黄金匹配队列
    std::thread _thread_bronze;          // 处理青铜队列的匹配的线程
    std::thread _thread_sliver;          // 处理白银队列的匹配的线程
    std::thread _thread_gold;            // 处理黄金队列的匹配的线程
    online_manager *_online_manager;
    room_manager *_room_manager;
    user_table *_user_table;

public:
    matcher(online_manager *online_manager, room_manager *room_manager, user_table *user_table)
        : _online_manager(online_manager), _room_manager(room_manager), _user_table(user_table),
          _thread_bronze(std::thread(&matcher::handler_bronze_match, this)),
          _thread_sliver(std::thread(&matcher::handler_sliver_match, this)),
          _thread_gold(std::thread(&matcher::handler_gold_match, this))
    {
        DLOG("游戏匹配模块处理完毕!!!");
    }

    // 处理匹配功能
    void handler_match(match_queue<uint64_t> &queue)
    {
        while (true)
        {
            // 如果队列中的人数 小于2 则阻塞匹配队列线程
            while (queue.size() < 2)
            {
                queue.wait();
            }
            // DLOG("人数大于2个，开始匹配");
            // DLOG("!!!!!!!!!!");
            // 走到这里说明匹配队列中的人数 大于2
            // 从匹配队列中取出前两个玩家进行创建房间对战
            uint64_t uid1, uid2;
            // DLOG("!!!!!");
            bool ret = queue.pop(uid1);
            // DLOG("pop1");
            if (ret == false) // 即使走到这里，匹配队列中的人数也有可能不足2人，因为如果有人进行匹配后，又取消了就有可能出现这种情况
            {
                // DLOG("1");
                continue;
            }
            ret = queue.pop(uid2);
            // DLOG("pop2");
            if (ret == false)
            {
                // DLOG("2");
                // 走到这里说明有一个人匹配了成功，可对方却取消匹配了，所以要将uid1重新加入匹配队列
                add(uid1);
                continue;
            }
            // 走到这里说明，两人匹配成功，但是匹配成功后，要判断两个人是否都还在游戏大厅
            // DLOG("conn1");
            server_t::connection_ptr conn1 = _online_manager->get_con_from_hall(uid1);
            if (conn1.get() == nullptr) // uid1不在游戏大厅
            {
                // DLOG("3");
                add(uid2);
                continue;
            }
            // DLOG("conn2");
            server_t::connection_ptr conn2 = _online_manager->get_con_from_hall(uid2);
            if (conn2.get() == nullptr) // uid2不在游戏大厅
            {
                // DLOG("4");
                add(uid1);
                continue;
            }
            // 走到这里说明，两个人终于匹配成功，为他们创建房间
            // DLOG("create_room");
            room_ptr rp = _room_manager->create_room(uid1, uid2);
            if (rp.get() == nullptr)
            {
                // DLOG("5");
                add(uid1);
                add(uid2);
                continue;
            }
            // DLOG("!");
            // 对两个玩家进行响应
            Json::Value response;
            response["optype"] = "match_success";
            response["result"] = true;
            std::string body;
            // DLOG("!!!!!!!!!!");
            json_util::serialize(response, body);
            // DLOG("handler_match : %s", body.c_str());
            conn1->send(body);
            conn2->send(body);
        }
        return;
    }

    // 处理青铜匹配队列
    void handler_bronze_match()
    {
        return handler_match(_queue_bronze);
    }

    // 处理白银匹配队列
    void handler_sliver_match()
    {
        return handler_match(_queue_sliver);
    }

    // 处理黄金匹配队列
    void handler_gold_match()
    {
        return handler_match(_queue_gold);
    }

    // 将玩家加入匹配队列
    bool add(const uint64_t &uid)
    {
        // 根据玩家的分数，把不同的玩家加入到不同的匹配队列
        Json::Value message; // 通过用户的id可以将用户的信息传到message中
        bool ret = _user_table->select_by_id(uid, message);
        if (ret == false)
        {
            DLOG("获取用户 %d 信息失败", uid);
            return false;
        }
        uint64_t score = message["score"].asUInt64();
        if (score < 2000)
        {
            _queue_bronze.push(uid);
        }
        else if (score < 3000)
        {
            _queue_sliver.push(uid);
        }
        else
        {
            _queue_gold.push(uid);
        }
        return true;
    }

    // 将玩家从匹配队列中移除，当玩家取消匹配后调用
    bool del(const uint64_t &uid)
    {
        Json::Value message;
        bool ret = _user_table->select_by_id(uid, message);
        if (ret == false)
        {
            DLOG("获取用户 %d 信息失败", uid);
            return false;
        }
        uint64_t score = message["score"].asUInt64();
        if (score < 2000)
        {
            _queue_bronze.remove(uid);
        }
        else if (score < 3000)
        {
            _queue_sliver.remove(uid);
        }
        else
        {
            _queue_gold.remove(uid);
        }
        return true;
    }
};