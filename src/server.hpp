// 整合服务器类
#pragma once

#include <string>

#include "util.hpp"
#include "log.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include "session.hpp"
#include "matcher.hpp"

#define WWWROOT "./wwwroot/"

class gobang_server
{
public:
    gobang_server(const std::string &host,
                  const std::string &username,
                  const std::string &password,
                  const std::string &dbname,
                  const uint16_t &port = 3306,
                  const std::string &wwwroot = WWWROOT)
        : _wwwroot(wwwroot),
          _user_table(host, username, password, dbname, port),
          _room_manager(&_user_table, &_online_manager),
          _session_manager(&_server),
          _matcher(&_online_manager, &_room_manager, &_user_table)
    {
        // 对websocket的server类进行初始化
        _server.set_access_channels(websocketpp::log::alevel::none); // 关闭日志
        _server.init_asio();
        _server.set_reuse_addr(true); // 确保服务器重启时，能快速占用端口号
        // 初始化四个处理回调函数
        _server.set_http_handler(std::bind(handler_http, this, std::placeholders::_1));
        _server.set_open_handler(std::bind(handler_open, this, std::placeholders::_1));
        _server.set_close_handler(std::bind(handler_close, this, std::placeholders::_1));
        _server.set_message_handler(std::bind(handler_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    void login(server_t::connection_ptr &conn)
    {
    }

    void reg(server_t::connection_ptr &conn)
    {
    }

    void information(server_t::connection_ptr &conn)
    {
    }

    // 返回默认界面，即登录界面
    void default_page(server_t::connection_ptr &conn)
    {
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string url = req.get_uri();

        std::string path = _wwwroot + url; // 组合请求路径
        if (path.back() == '/')            // 如果这里的if为真，说明请求的路径为:./wwwroot/
        {
            path += "login.html";
        }

        std::string body; // 将请求的内容进行返回
        bool ret = read_util::read(path, body);
        if (ret == false) // 如果获取login.html资源失败，则返回404
        {
            std::string not_found = _wwwroot + "404.html";
            read_util::read(not_found, body);
        }
        conn->set_body(body);
        conn->set_status(websocketpp::http::status_code::ok);
    }

    // http请求处理回调函数
    void handler_http(websocketpp::connection_hdl hdl)
    {
        server_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string method = req.get_method();                        // 后去http请求的方法
        std::string url = req.get_uri();                              // 获取http请求的url
        if (method == "POST" && url == "/login")
        {
            return login(conn); // 进行登录请求
        }
        else if (method == "POST" && url == "/reg")
        {
            return reg(conn); // 进行注册请求
        }
        else if (method == "GET" && url == "/information")
        {
            return information(conn);
        }
        else
        {
            return default_page(conn); // 默认页面，即登录页面
        }
    }

    // open连接成功处理回调函数
    void handler_open(websocketpp::connection_hdl hdl)
    {
    }

    // close连接关闭处理回调函数
    void handler_close(websocketpp::connection_hdl hdl)
    {
    }

    // message消息处理回调函数
    void handler_message(websocketpp::connection_hdl hdl, server_t::message_ptr message)
    {
    }

private:
    server_t _server;                 // 服务器类
    std::string _wwwroot;             // web网页资源根目录
    user_table _user_table;           // 数据库用户管理类
    online_manager _online_manager;   // 在线用户管理类
    room_manager _room_manager;       // 房间管理类
    session_manager _session_manager; // session管理类
    matcher _matcher;                 // 匹配队列管理类
};