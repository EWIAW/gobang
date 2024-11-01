#pragma once
#include <unordered_map>
#include <mutex>

#include "util.hpp"

// 通过哈希表对游戏大厅和游戏房间的用户进行管理

class online_manager
{
public:
    // 插入用户到游戏大厅
    void login_game_hall(const uint64_t &id, server_t::connection_ptr &con)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.insert(std::make_pair(id, con));
    }

    // 插入用户到游戏房间
    void login_game_room(const uint64_t &id, server_t::connection_ptr &con)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _game_user.insert(std::make_pair(id, con));
    }

    // 将用户从游戏大厅删除
    void exit_game_hall(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _hall_user.erase(id);
    }

    // 将用户从游戏房间删除
    void exit_game_room(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _game_user.erase(id);
    }

    // 判断当前用户是否在游戏大厅
    bool is_in_game_hall(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(id);
        if (it == _hall_user.end())
        {
            return false;
        }
        return true;
    }

    // 判断当前用户是否在游戏房间
    bool is_in_game_room(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _game_user.find(id);
        if (it == _game_user.end())
        {
            return false;
        }
        return true;
    }

    // 获取用户在游戏大厅的信息指针
    server_t::connection_ptr get_con_from_hall(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _hall_user.find(id);
        if (it == _hall_user.end())
        {
            return server_t::connection_ptr();
        }
        return it->second;
    }

    // 获取用户在游戏房间的信息指针
    server_t::connection_ptr get_con_from_room(const uint64_t &id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _game_user.find(id);
        if (it == _game_user.end())
        {
            return server_t::connection_ptr();
        }
        return it->second;
    }

private:
    std::mutex _mutex;                                                 // 定义互斥锁，保护对哈希表进行操作的时候是原子性的
    std::unordered_map<uint64_t, server_t::connection_ptr> _hall_user; // 管理游戏大厅用户信息
    std::unordered_map<uint64_t, server_t::connection_ptr> _game_user; // 管理游戏房间用户信息
};