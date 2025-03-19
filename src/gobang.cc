#include <iostream>
#include "log.hpp"
#include "util.hpp"
#include "db.hpp"
#include "online.hpp"
#include "room.hpp"
#include "server.hpp"

#define HOST "127.0.0.1"
#define USERNAME "root"
#define PASSWORD "Zrb20031017"
#define DBNAME "online_gobang"

// 测试日志系统
void test_log()
{
    ILOG("测试信息宏：%d,%s", 111, "aaa");
    DLOG("test buglog");
    ELOG("test elog");
}

// 测试数据库连接模块
void test_mysql()
{
    // 初始化和连接数据库，并设置编码集
    MYSQL *mysql = mysql_util::mysql_create("localhost", "root", "Zrb20031017", "test");

    // 执行MySQL语句
    bool ret = mysql_util::mysql_exec(mysql, "insert into student values('阮靖凯',36,'女')");
    if (ret == false)
    {
        return;
    }

    // 关闭连接数据库
    mysql_util::mysql_destroy(mysql);
}

// 测试json工具类
void test_json()
{
    Json::Value root;
    root["姓名"] = "阮靖凯";
    root["学号"] = 36;
    root["性别"] = "女";
    std::string str;
    json_util::serialize(root, str);

    Json::Value val;
    json_util::unserialize(str, val);
    std::cout << val["姓名"].asCString() << std::endl;
    std::cout << val["学号"].asInt() << std::endl;
    std::cout << val["性别"].asCString() << std::endl;
}

// 测试string_utile::split字符串分割工具类
void test_split()
{
    // std::string src = "123,456,,,789";
    std::string src = ",,123,456,,,789";
    std::string sep = ",";
    std::vector<std::string> res;
    size_t ret = string_util::split(src, sep, res);
    for (size_t i = 0; i < ret; i++)
    {
        std::cout << res[i] << std::endl;
    }
}

// 测试read_util读取文件工具类
void test_read()
{
    std::string filename = "./test.txt";
    std::string body;
    read_util::read(filename, body);
    std::cout << body << std::endl;
}

int main()
{
    // test_log();
    // test_mysql();
    // test_json();
    // test_split();
    // test_read();
    gobang_server _server(HOST, USERNAME, PASSWORD, DBNAME);
    _server.start(3489);

    return 0;
}