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
private:
    server_t _server;                 // 服务器类
    std::string _wwwroot;             // web网页资源根目录
    user_table _user_table;           // 数据库用户管理类
    online_manager _online_manager;   // 在线用户管理类
    room_manager _room_manager;       // 房间管理类
    session_manager _session_manager; // session管理类
    matcher _matcher;                 // 匹配队列管理类
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
        _server.set_http_handler(std::bind(&gobang_server::handler_http, this, std::placeholders::_1));
        _server.set_open_handler(std::bind(&gobang_server::handler_open, this, std::placeholders::_1));
        _server.set_close_handler(std::bind(&gobang_server::handler_close, this, std::placeholders::_1));
        _server.set_message_handler(std::bind(&gobang_server::handler_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    // 启动服务器
    void start(uint16_t port)
    {
        _server.listen(port);
        _server.start_accept();
        _server.run();
        return;
    }

private:
    // 返回响应信息
    void http_response(server_t::connection_ptr &conn, websocketpp::http::status_code::value code, const bool &result, const std::string &message)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入http_response函数");
        Json::Value http_response;
        http_response["result"] = result;
        http_response["resson"] = message;
        std::string body;
        json_util::serialize(http_response, body);
        conn->set_status(code);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        // DLOG("退出http_response函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // http请求处理回调函数
    // 进行登录
    void login(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入login函数");
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
        ret = _user_table.login(login_information);
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
        std::string cooked_ssid = "SSID=" + std::to_string(ssp->get_sesssion_id());
        conn->append_header("Set-Cookie", cooked_ssid);
        // DLOG("退出login函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return http_response(conn, websocketpp::http::status_code::ok, true, "登录成功");
    }

    // 进行注册
    void reg(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入reg函数");
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
        // DLOG("退出reg函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return http_response(conn, websocketpp::http::status_code::ok, true, "注册成功");
    }

    // 通过cookie和session来获取用户的信息，例如用户的分数和对局数等等
    void information(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入information函数");
        // 1.获取cookie信息
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty() == true)
        {
            DLOG("Cookie信息不存在");
            return http_response(conn, websocketpp::http::status_code::bad_request, false, "找不到cookie信息，请重新登录");
        }

        // 2.判断session是否存在
        std::string val;
        if (get_cookie_val(cookie_str, "SSID", val) == false)
        {
            DLOG("session id信息不存在");
            http_response(conn, websocketpp::http::status_code::bad_request, false, "session id不存在，请重新登录");
        }

        // 3.通过session去获取会话的信息
        session_ptr ssp = _session_manager.get_session_by_id(std::stol(val));
        if (ssp.get() == nullptr)
        {
            DLOG("session信息不存在");
            http_response(conn, websocketpp::http::status_code::bad_request, false, "session信息不存在，请重新登录");
        }

        // 4.从数据库中通过会话信息获取用户信息
        uint64_t uid = ssp->get_user_id();
        Json::Value user_info;
        if (_user_table.select_by_id(uid, user_info) == false)
        {
            DLOG("数据库中不存在该用户信息");
            http_response(conn, websocketpp::http::status_code::bad_request, false, "找不到用户信息，请重新登录");
        }
        std::string body;
        json_util::serialize(user_info, body);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json");
        conn->set_status(websocketpp::http::status_code::ok);
        // 5.刷新session过期时间
        _session_manager.set_session_expire_time(ssp->get_sesssion_id(), SESSION_TIMEOUT);
        // DLOG("退出information函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    // 返回默认界面，即登录界面
    void default_page(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入default_page函数");
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
        // DLOG("退出default_page函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    void handler_http(websocketpp::connection_hdl hdl)
    {
        server_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request(); // 获取http请求
        std::string method = req.get_method();                        // 获取http请求的方法
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
            return information(conn); // 后去用户信息请求，例如用户的分数、id等
        }
        else
        {
            return default_page(conn); // 默认页面，即登录页面
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // open连接成功处理回调函数，即websocekt协议升级成功
    void open_game_hall(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入open_game_hall函数");
        Json::Value response;
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 判断是否重复登录
        if (_online_manager.is_in_game_hall(ssp->get_user_id()) || _online_manager.is_in_game_room(ssp->get_user_id()))
        {
            response["optype"] = "hall_ready";
            response["result"] = false;
            response["reason"] = "重复登录";
            return server_response(conn, response);
        }
        // 将登录用户添加进管理用户模块
        _online_manager.login_game_hall(ssp->get_user_id(), conn);
        response["optype"] = "hall_ready";
        response["result"] = true;
        server_response(conn, response);
        // 设置session为有限的时间
        _session_manager.set_session_expire_time(ssp->get_user_id(), SESSION_TIMEOUT);
        // DLOG("退出open_game_hall函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    void open_game_room(server_t::connection_ptr &conn)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入open_game_room函数");
        Json::Value response;
        // 1.获取session信息
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }

        // 2.判断当前用户是否重复登录
        if (_online_manager.is_in_game_hall(ssp->get_user_id()) || _online_manager.is_in_game_room(ssp->get_user_id()))
        {
            response["optype"] = "room_ready";
            response["result"] = false;
            response["reason"] = "重复登录";
            return server_response(conn, response);
        }

        // 3.判断有没有为该用户创建房间
        room_ptr rm = _room_manager.get_room_by_user_id(ssp->get_user_id());
        if (rm.get() == nullptr)
        {
            response["optype"] = "room_ready";
            response["result"] = false;
            response["reason"] = "没有找到房间信息";
            return server_response(conn, response);
        }

        // 4.将玩家添加进房间
        _online_manager.login_game_room(ssp->get_user_id(), conn);

        // 5.设置session时间为永久
        _session_manager.set_session_expire_time(ssp->get_user_id(), SESSION_FOREVER);
        response["optype"] = "room_ready";
        response["result"] = true;
        response["room_id"] = (Json::UInt64)rm->get_room_id();
        response["uid"] = (Json::UInt64)ssp->get_user_id();
        response["white_id"] = (Json::UInt64)rm->get_white_id();
        response["black_id"] = (Json::UInt64)rm->get_black_id();
        // DLOG("退出open_game_room函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return server_response(conn, response);
    }

    void handler_open(websocketpp::connection_hdl hdl)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入handler_open函数");
        server_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
        websocketpp::http::parser::request request = conn->get_request();
        std::string url = request.get_uri();
        if (url == "/hall")
        {
            return open_game_hall(conn);
        }
        else if (url == "/room")
        {
            return open_game_room(conn);
        }
        // DLOG("退出handler_open函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // close连接关闭处理回调函数
    void close_game_hall(server_t::connection_ptr &conn)
    {
        // 将用户从大厅中移除
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        _online_manager.exit_game_hall(ssp->get_user_id());
        // 设置session时间
        _session_manager.set_session_expire_time(ssp->get_user_id(), SESSION_TIMEOUT);
    }

    void close_game_room(server_t::connection_ptr &conn)
    {
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        _online_manager.exit_game_room(ssp->get_user_id());
        _session_manager.set_session_expire_time(ssp->get_user_id(), SESSION_TIMEOUT);
        _room_manager.remove_user(ssp->get_user_id());
    }

    void handler_close(websocketpp::connection_hdl hdl)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入handler_close函数");
        server_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
        websocketpp::http::parser::request request = conn->get_request();
        std::string url = request.get_uri();
        if (url == "/hall")
        {
            return close_game_hall(conn);
        }
        else if (url == "/room")
        {
            return close_game_room(conn);
        }
        // DLOG("退出handler_close函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // message消息处理回调函数
    void message_game_hall(server_t::connection_ptr &conn, server_t::message_ptr &message)
    {
        Json::Value response; // 需要返回的json数据
        std::string resquest_body;
        // 1.身份验证，判断当前客户端是那个玩家
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 2.获取请求信息
        resquest_body = message->get_payload();
        // std::cout << "!!!!!resquest_body : " << resquest_body << std::endl;
        bool ret = json_util::unserialize(resquest_body, response);
        if (ret == false)
        {
            response["result"] = false;
            response["reason"] = "反序列化失败";
            return server_response(conn, response);
        }
        if (!response["optype"].isNull() && response["optype"].asString() == "match_start")
        {
            // 开始匹配对战
            // DLOG("开始匹配对战");
            _matcher.add(ssp->get_user_id());
            response["optype"] = "match_start";
            response["result"] = true;
            return server_response(conn, response);
        }
        else if (!response["optype"].isNull() && response["optype"].asString() == "match_stop")
        {
            // 停止匹配对战
            _matcher.del(ssp->get_user_id());
            response["optype"] = "match_stop";
            response["result"] = true;
            return server_response(conn, response);
        }
        response["optype"] = "unknow";
        response["result"] = false;
        response["reason"] = "未知类型";
        return server_response(conn, response);
    }

    void message_game_room(server_t::connection_ptr &conn, server_t::message_ptr &message)
    {
        Json::Value response;

        // 1.检查身份信息
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            DLOG("房间-没有找到会话信息");
            return;
        }
        // 获取客户端房间信息
        room_ptr rp = _room_manager.get_room_by_user_id(ssp->get_user_id());
        if (rp.get() == nullptr)
        {
            response["optype"] = "unknow";
            response["result"] = false;
            response["reason"] = "没有找到房间信息";
            DLOG("没有找到玩家房间信息");
            return server_response(conn, response);
        }
        // 获取消息内容并进行反序列化
        std::string resquest_body;
        resquest_body = message->get_payload();
        bool ret = json_util::unserialize(resquest_body, response);
        if (ret == false)
        {
            response["optype"] = "unknow";
            response["result"] = false;
            response["reason"] = "反序列化信息失败";
            DLOG("房间反序列化信息失败");
            return server_response(conn, response);
        }
        // 处理房间动作
        DLOG("房间-动作");
        return rp->handle_request(response);
    }

    void handler_message(websocketpp::connection_hdl hdl, server_t::message_ptr message)
    {
        // DLOG("----------------------------------------------------------------------------------------------------");
        // DLOG("进入handler_message函数");
        server_t::connection_ptr conn = _server.get_con_from_hdl(hdl);
        websocketpp::http::parser::request request = conn->get_request();
        std::string url = request.get_uri();
        if (url == "/hall")
        {
            return message_game_hall(conn, message);
        }
        else if (url == "/room")
        {
            return message_game_room(conn, message);
        }
        // DLOG("退出handler_message函数");
        // DLOG("----------------------------------------------------------------------------------------------------");
        return;
    }

    // 从cookie信息中提取session的id
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val)
    {
        // Cookie:SSID=XXX;path=/;
        std::vector<std::string> cookie_arr;
        string_util::split(cookie_str, ";", cookie_arr);
        for (auto &tmp : cookie_arr)
        {
            std::vector<std::string> ssd_arr;
            size_t ret = string_util::split(tmp, "=", ssd_arr);
            if (ret != 2)
            {
                continue;
            }
            if (ssd_arr[0] == key)
            {
                val = ssd_arr[1];
                return true;
            }
        }
        return false;
    }

    // 将json结构化数据发送给客户端
    void server_response(server_t::connection_ptr &conn, const Json::Value &response)
    {
        std::string body;
        json_util::serialize(response, body);
        conn->send(body);
    }

    // 通过cookie找到session
    session_ptr get_session_by_cookie(server_t::connection_ptr &conn)
    {
        Json::Value err_response; // 用于返回错误信息
        // 1.查找cookie信息
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty() == true)
        {
            err_response["optype"] = "hall_ready";
            err_response["result"] = false;
            err_response["reason"] = "找不到cookie信息，请重新登录";
            server_response(conn, err_response);
            return session_ptr();
        }
        // 从cookie信息中找到session
        std::string ssid_str;
        if (get_cookie_val(cookie_str, "SSID", ssid_str) == false)
        {
            err_response["optype"] = "hall_ready";
            err_response["result"] = false;
            err_response["optype"] = "找不到session信息，请重新登录";
            server_response(conn, err_response);
            return session_ptr();
        }
        // 在session管理中查找对应的会话信息
        session_ptr ssp = _session_manager.get_session_by_id(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        {
            err_response["optype"] = "hall_ready";
            err_response["result"] = false;
            err_response["reason"] = "找不到session信息，请重新登录";
            server_response(conn, err_response);
            return session_ptr();
        }
        return ssp;
    }
};