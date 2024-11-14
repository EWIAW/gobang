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

    // 返回响应信息
    void http_response(server_t::connection_ptr &conn, websocketpp::http::status_code::value code, const bool &result, const std::string &message)
    {
        Json::Value http_response;
        http_response["result"] = result;
        http_response["resson"] = message;
        std::string body;
        json_util::serialize(http_response, body);
        conn->set_status(code);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        return;
    }

    // 进行登录
    void login(server_t::connection_ptr &conn)
    {
        std::string request_json = conn->get_request_body(); // 获取请求的body内容，即登录的账号和密码
        Json::Value login_information;
        bool ret = json_util::unserialize(request_json, login_information);
        // 1.将传回来的登录信息进行反序列化
        if (ret == false)
        {
            DLOG("反序列化登录信息失败");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "请求的正文格式错误");
        }
        // 2.检查登录的用户名和密码是否都有输入
        if (login_information["username"].isNull() || login_information["password"].isNull())
        {
            DLOG("用户名密码不完整");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "请输入用户名和密码");
        }
        // 3.判断用户存不存在数据库中
        bool ret = _user_table.login(login_information);
        if (ret == false)
        {
            DLOG("用户名密码错误");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "用户名密码错误");
        }
        // 4.为用户创建session信息
        uint64_t uid = login_information["id"].asInt64();
        session_ptr ssp = _session_manager.create_session(uid, LOGIN);
        if (ssp.get() == nullptr)
        {
            DLOG("创建会话失败");
            return http_response(conn, websocketpp::http::status_code::internal_server_error, false, "创建会话失败");
        }
        _session_manager.set_session_expire_time(ssp->get_sesssion_id(), SESSION_TIMEOUT);
        // 5.设置响应头部
        std::string cooked_ssid = "SSID=" + ssp->get_sesssion_id();
        conn->append_header("Set-Cookie", cooked_ssid);
        return http_response(conn, websocketpp::http::status_code::ok, true, "登录成功");
    }

    // 进行注册
    void reg(server_t::connection_ptr &conn)
    {
        std::string body = conn->get_request_body();
        Json::Value reg_response;

        // 1.获取注册的用户名和密码
        bool ret = json_util::unserialize(body, reg_response);
        if (ret == false)
        {
            DLOG("注册信息反序列化失败");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "注册信息反序列化失败");
        }

        // 2.检查用户名和密码是否为空
        if (reg_response["username"].isNull() || reg_response["password"].isNull())
        {
            DLOG("用户名密码不完整");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "请输入用户名和密码");
        }

        // 3.进行注册，将注册信息插入到数据库中
        ret = _user_table.insert(reg_response);
        if (ret == false)
        {
            DLOG("插入新用户失败");
            return http_response(conn, websocketpp::http::status_code::internal_server_error, false, "用户名已经存在");
        }
        return http_response(conn, websocketpp::http::status_code::ok, true, "注册成功");
    }

    // 通过cookie和session来检查用户的状态
    void information(server_t::connection_ptr &conn)
    {
        // 1.获取cookie信息
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty() == true)
        {
            DLOG("Cookie信息不存在");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "找不到cookie信息，请重新登录");
        }

        // 2.判断session是否存在

        // 3.通过session去获取会话的信息

        // 4.通过会话信息获取用户信息
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

    /////
    // open连接成功处理回调函数
    void handler_open(websocketpp::connection_hdl hdl)
    {
    }

    /////
    // close连接关闭处理回调函数
    void handler_close(websocketpp::connection_hdl hdl)
    {
    }

    /////
    // message消息处理回调函数
    void handler_message(websocketpp::connection_hdl hdl, server_t::message_ptr message)
    {
    }

private:
    // 从cookie信息中提取session的id
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, const std::string &val)
    {
        // Cookie:SSID=XXX;path=/
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, ";", cookie_arr);
        for(auto& tmp:cookie_arr)
        {
            

        }
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