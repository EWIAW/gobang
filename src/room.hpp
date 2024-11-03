#pragma once

#include <iostream>
#include <vector>
#include <string>

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
        uint64_t play_chess_id = request["uid"].asUInt64(); // 用户下棋的id
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
            response["reason"] = "该位置已经有棋子，请重新选择位置下棋";
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
        uint64_t winner_id = check_win();
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

    // 处理玩家退出房间动作
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

    void broadcast(const Json::Value &response)
    {
    }

private:
    // 判断是否获胜，如果获胜了，返回获胜者的id
    uint64_t check_win()
    {
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