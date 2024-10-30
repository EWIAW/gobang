#pragma once
#include <unordered_map>
#include <mutex>

#include "util.hpp"

// 通过哈希表对游戏大厅和游戏房间的用户进行管理

class online_manager
{
public:
private:
    std::mutex _mutex;                                               // 定义互斥锁，保护对哈希表进行操作的时候是原子性的
    std::unordered_map<uint64_t, server::connection_ptr> _hall_user; // 管理游戏大厅用户信息
};