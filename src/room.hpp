#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "db.hpp"
#include "online.hpp"
#include "log.hpp"
#include "util.hpp"

#define BOARD_ROW 15  // 棋盘行数
#define BOARD_COL 15  // 棋盘列数
#define WHITE_CHESS 1 // 白方的棋子
#define BLACK_CHESS 2 // 黑方的棋子

// 定义房间状态
enum room_status
{
    GAME_START = 1,
    GAME_OVER
};

// 房间类
class room
{
public:
    room(const uint64_t &room_id, user_table *user_table, online_manager *online_user)
        : _play_count(0), _room_id(room_id),
          _room_status(GAME_START), _user_table(user_table), _online_user(online_user),
          _board(BOARD_ROW, std::vector<int>(BOARD_COL, 0))
    {
        DLOG("房间创建成功");
    }

    ~room()
    {
        DLOG("房间销毁成功");
    }

    // 获取房间id
    uint64_t get_room_id()
    {
        return _room_id;
    }

    // 获取房间状态
    room_status get_room_status()
    {
        return _room_status;
    }

    // 获取玩家数量
    int get_player_count()
    {
        return _play_count;
    }

    // 将白方添加进房间
    void add_white_user(const uint64_t &id)
    {
        _white_id = id;
        _play_count++;
    }

    // 将黑方添加进房间
    void add_black_user(const uint64_t &id)
    {
        _black_id = id;
        _play_count++;
    }

    // 获取白方用户id
    uint64_t get_white_id()
    {
        return _white_id;
    }

    // 获取黑方用户id
    uint64_t get_black_id()
    {
        return _black_id;
    }

    // 处理下棋动作
    Json::Value handle_chess(const Json::Value &request)
    {
        Json::Value response;                    // 响应处理
        int chess_row = request["row"].asUInt(); // 下棋的位置
        int chess_col = request["col"].asUInt();
        uint64_t play_chess_id = request["uid"].asUInt64(); // 下棋用户的id
        // 1.在更新下棋位置的信息时，先判断对方有没有掉线，如果掉线了，直接获胜
        if (_online_user->is_in_game_room(_white_id) == false)
        {
            response["result"] = true;
            response["reason"] = "对方已掉线，恭喜你获胜了！！！";
            response["winner"] = (Json::UInt64)_black_id;
            return response;
        }
        if (_online_user->is_in_game_room(_black_id) == false)
        {
            response["result"] = true;
            response["reason"] = "对方已掉线，恭喜你获胜了！！！";
            response["winner"] = (Json::UInt64)_white_id;
            return response;
        }
        // 2.进行下棋
        if (_board[chess_row][chess_col] != 0) // 说明下棋的位置已经有棋子
        {
            response["result"] = false;
            response["reason"] = "该位置已经有棋子，请重新选择位置下棋！！！";
            return response;
        }
        // 走到这里，所以下棋的位置是合理的，更新棋盘信息
        int chess_color = 0; // 记录下棋棋子的颜色
        if (play_chess_id == _white_id)
        {
            chess_color = WHITE_CHESS;
        }
        else
        {
            chess_color = BLACK_CHESS;
        }
        _board[chess_row][chess_col] = chess_color;
        // 3.下棋完成后判断是否获胜
        uint64_t winner_id = check_win(chess_row, chess_col, chess_color);
        if (winner_id != 0)
        {
            response["reason"] = "五星连珠，恭喜你获胜了！！！";
        }
        response["result"] = true;
        response["winner"] = (Json::UInt64)winner_id;
        return response;
    }

    // 处理聊天动作
    Json::Value handle_chat(const Json::Value &resquest)
    {
        Json::Value response;
        std::string msg = resquest["message"].asCString();
        // 判断信息中是否存在敏感词
        if (msg.find("垃圾") != std::string::npos)
        {
            response["result"] = false;
            response["reason"] = "该信息中包含敏感词汇，无法发送";
            return response;
        }
        // 广播消息
        response["result"] = true;
        return response;
    }

    // 处理玩家退出房间动作                                     !!!
    void handle_exit(const uint64_t &id)
    {
        Json::Value response;
        if (_room_status == GAME_START)
        {
            uint64_t winner_id = 0;
            uint64_t loser_id = 0;
            if (id == _white_id)
            {
                winner_id = _black_id;
                loser_id = _white_id;
            }
            else
            {
                winner_id = _white_id;
                loser_id = _black_id;
            }
            response["optype"] = "put_chess";
            response["result"] = true;
            response["reason"] = "对方已掉线，恭喜你获胜了！！！";
            response["room_id"] = (Json::UInt64)_room_id;
            response["uid"] = (Json::UInt64)id;
            response["row"] = -1;
            response["col"] = -1;
            response["winner"] = (Json::UInt64)winner_id;
            _user_table->win(winner_id);
            _user_table->lose(loser_id);
            _room_status = GAME_OVER;
            broadcast(response); // 广播信息
        }
        _play_count--;
        return;
    }

    // 总的请求处理函数，处理各种请求                                       !!!
    void handle_request(const Json::Value &request)
    {
        Json::Value response;
        // 1.判断房间号是否匹配
        if (request["room_id"] != (Json::UInt64)_room_id)
        {
            response["optype"] = request["optype"].asCString();
            response["result"] = false;
            response["reason"] = "房间号不匹配！！！";
            broadcast(response);
            return;
        }
        // 2.开始处理各种请求
        if (request["optype"].asCString() == "put_chess")
        {
            response = handle_chess(request);
            if (response["winner"].asInt64() != 0) // 说明有人获胜了
            {
                uint64_t winner_id = response["winner"].asUInt64();
                uint64_t loser_id = 0;
                if (winner_id == _white_id)
                {
                    loser_id = _black_id;
                }
                else
                {
                    loser_id = _white_id;
                }
                _user_table->win(winner_id);
                _user_table->lose(loser_id);
                _room_status = GAME_OVER;
            }
        }
        else if (request["optype"].asCString() == "chat")
        {
            response = handle_chat(request);
        }
        else
        {
            response["optype"] = request["optype"].asCString();
            response["result"] = false;
            response["reason"] = "未知类型";
        }
        std::string body;
        json_util::serialize(response, body);
        DLOG("房间-广播动作：%s", body.c_str());
        broadcast(response);
        return;
    }

    // 广播消息给房间所有用户
    void broadcast(const Json::Value &response)
    {
        // 1.将需要广播的消息从json序列化成字符串
        std::string body;
        json_util::serialize(response, body);
        // 2.获取客户端的连接
        server_t::connection_ptr white_conn = _online_user->get_con_from_room(_white_id);
        if (white_conn != nullptr)
        {
            white_conn->send(body);
        }
        else
        {
            DLOG("房间-白棋玩家获取连接失败");
        }
        server_t::connection_ptr black_conn = _online_user->get_con_from_room(_black_id);
        if (black_conn != nullptr)
        {
            black_conn->send(body);
        }
        else
        {
            DLOG("房间-黑棋玩家获取连接失败");
        }
        return;
    }

private:
    // 判断某一方向上是否五星连珠
    // row和col为下棋的位置，offset为偏移量，依次判断横竖斜四个方向是否五星连珠
    bool five(const int row, const int col, const int offset_row, const int offset_col, const int chess_color)
    {
        int count = 1;
        int tmp_row = row;
        int tmp_col = col;

        tmp_row += offset_row;
        tmp_col += offset_col;
        while (tmp_row >= 0 && tmp_row < BOARD_ROW && tmp_col >= 0 && tmp_col < BOARD_COL)
        {
            if (_board[tmp_row][tmp_col] == chess_color)
            {
                count++;
                tmp_row += offset_row;
                tmp_col += offset_col;
            }
            else
            {
                break;
            }
        }

        tmp_row = row;
        tmp_col = col;
        tmp_row -= offset_row;
        tmp_col -= offset_col;
        while (tmp_row >= 0 && tmp_row < BOARD_ROW && tmp_col >= 0 && tmp_col < BOARD_COL)
        {
            if (_board[tmp_row][tmp_col] == chess_color)
            {
                count++;
            }
            else
            {
                break;
            }
        }

        if (count == 5)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    // 判断是否获胜，如果获胜了，返回获胜者的id
    uint64_t check_win(const int row, const int col, const int chess_color)
    {
        if (five(row, col, 1, 0, chess_color) ||
            five(row, col, 0, 1, chess_color) ||
            five(row, col, 1, 1, chess_color) ||
            five(row, col, -1, 1, chess_color))
        {
            if (chess_color == WHITE_CHESS)
            {
                return _white_id;
            }
            else
            {
                return _black_id;
            }
        }
        return 0;
    }

private:
    int _play_count;                      // 玩家数量
    uint64_t _room_id;                    // 房间id
    uint64_t _white_id;                   // 白方id
    uint64_t _black_id;                   // 黑方id
    room_status _room_status;             // 房间状态
    user_table *_user_table;              // user表管理类
    online_manager *_online_user;         // 用户在线信息类
    std::vector<std::vector<int>> _board; // 棋盘
};

typedef std::shared_ptr<room> room_ptr;
// using room_ptr = std::shared_ptr<room>;

// 房间管理类
class room_manager
{
public:
    room_manager(user_table *user_tb, online_manager *_online_user)
        : _next_room_id(1), _user_tb(user_tb), _online_user(_online_user)
    {
        DLOG("房间管理模块创建完毕！！！");
    }

    ~room_manager()
    {
        DLOG("房间管理模块销毁完毕！！！");
    }

    // 创建房间，当两个用户匹配成功时，为他们创建房间
    room_ptr create_room(const uint64_t &id1, const uint64_t &id2)
    {
        // 在创建房间之前，先判断两个用户是否都还在大厅
        if (_online_user->is_in_game_hall(id1) == false)
        {
            DLOG("用户：%lu 不在大厅中，创建房间失败", id1);
            return room_ptr();
        }
        if (_online_user->is_in_game_hall(id2) == false)
        {
            DLOG("用户：%lu 不在大厅中，创建房间失败", id2);
            return room_ptr();
        }

        // 说明两个用户都在大厅中，为他们创建房间
        std::unique_lock<std::mutex> lock(_mutex);
        room_ptr rp(new room(_next_room_id, _user_tb, _online_user));
        rp->add_white_user(id1);
        rp->add_black_user(id2);
        _rooms.insert(std::make_pair(_next_room_id, rp));
        _users.insert(std::make_pair(id1, _next_room_id));
        _users.insert(std::make_pair(id2, _next_room_id));
        _next_room_id++;
        return rp;
    }

    // 通过房间id获取房间信息
    room_ptr get_room_by_room_id(const uint64_t &room_id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _rooms.find(room_id);
        if (it == _rooms.end())
        {
            return room_ptr();
        }
        return it->second;
    }

    // 通过用户id获取房间信息
    room_ptr get_room_by_user_id(const uint64_t &user_id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        // 先通过用户id找到房间id
        auto uit = _users.find(user_id);
        if (uit == _users.end())
        {
            return room_ptr();
        }
        auto rit = _rooms.find(uit->second);
        if (rit == _rooms.end())
        {
            return room_ptr();
        }
        return rit->second;
    }

    // 通过id销毁房间
    void remove_room(const uint64_t &room_id)
    {
        // 先查找该房间，如果找到了才进行销毁
        room_ptr rp = get_room_by_room_id(room_id);
        if (rp == nullptr)
        {
            return;
        }

        std::unique_lock<std::mutex> lock(_mutex);
        uint64_t uid1 = rp->get_white_id();
        uint64_t uid2 = rp->get_black_id();
        _users.erase(uid1);
        _users.erase(uid2);
        _rooms.erase(room_id);
    }

    // 删除房间中指定用户，用于用户断开连接时调用
    void remove_user(const uint64_t &user_id)
    {
        // 先通过用户id查找房间在存不存在
        room_ptr rp = get_room_by_user_id(user_id);
        if (rp == nullptr)
        {
            return;
        }
        rp->handle_exit(user_id);
        if (rp->get_player_count() == 0)
        {
            remove_room(rp->get_room_id());
        }
        return;
    }

private:
    uint64_t _next_room_id; // 给房间id进行编排序号
    std::mutex _mutex;      // 互斥锁，用来保证哈希表的线程安全
    user_table *_user_tb;
    online_manager *_online_user;
    std::unordered_map<uint64_t, room_ptr> _rooms; // 用来管理通过房间号来找到房间对象
    std::unordered_map<uint64_t, uint64_t> _users; // 用来管理通过用户id找到房间id
};