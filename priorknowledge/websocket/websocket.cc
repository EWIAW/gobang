#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

// websocket握手成功回调函数
void on_open(server *svr, websocketpp::connection_hdl hdl)
{
    std::cout << "建立连接成功" << std::endl;
}

// websocket连接关闭回调函数
void on_close(server *svr, websocketpp::connection_hdl hdl)
{
    std::cout << "关闭连接成功" << std::endl;
}

// http请求回调函数
void on_http(server *svr, websocketpp::connection_hdl hdl)
{
    std::cout << "收到http请求" << std::endl;
    server::connection_ptr con = svr->get_con_from_hdl(hdl);
    std::stringstream ss;
    ss << "<!doctype html><html><head>"
       << "<title>hello websocket</title><body>"
       << "<h1>hello websocketpp</h1>"
       << "</body></head></html>";
    con->set_body(ss.str());
    con->set_status(websocketpp::http::status_code::ok);
}

// websocket消息回调处理函数
void on_message(server *svr, websocketpp::connection_hdl hdl, server::message_ptr msg)
{
    std::cout << "服务端收到客户端的消息" << msg->get_payload() << std::endl;
    // 将收到的消息发送给客户端
    svr->send(hdl, msg->get_payload(), websocketpp::frame::opcode::text);
}

int main()
{
    // 1.创建服务器类
    server svr;

    // 2.设置日志
    svr.set_access_channels(websocketpp::log::alevel::all);

    // 3.初始化asio调度器
    svr.init_asio();

    // 4.设置各种回调方法
    svr.set_open_handler(std::bind(on_open, &svr, std::placeholders::_1));
    svr.set_close_handler(std::bind(on_close, &svr, std::placeholders::_1));
    svr.set_http_handler(std::bind(on_http, &svr, std::placeholders::_1));
    svr.set_message_handler(std::bind(on_message, &svr, std::placeholders::_1, std::placeholders::_2));

    // 5.listen端口号
    svr.listen(8111);

    // 6.开始获取新连接
    svr.start_accept();

    // 7.服务器启动
    svr.run();
    return 0;
}