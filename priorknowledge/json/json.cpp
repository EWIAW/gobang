#include <iostream>
#include <sstream>
#include <string>
#include <jsoncpp/json/json.h>

std::string serialization()
{
    Json::Value root;
    root["姓名"] = "rjk";
    root["学号"] = "22034620136";
    root["年龄"] = 21;
    root["性别"] = "女";
    root["成绩"].append(60);
    root["成绩"].append(61);
    root["成绩"].append(62);

    Json::StreamWriterBuilder swb;
    Json::StreamWriter *sw = swb.newStreamWriter();

    std::stringstream ss;
    int ret = sw->write(root, &ss); // 如果成功返回0
    if (ret != 0)
    {
        std::cout << "序列化失败" << std::endl;
        return "";
    }
    delete sw;

    std::cout << ss.str() << std::endl;

    return ss.str();
}

void unserialization(const std::string str)
{
    Json::CharReaderBuilder crb;
    Json::CharReader *cr = crb.newCharReader();

    Json::Value root;
    std::string err;
    cr->parse(str.c_str(), str.c_str() + str.size(), &root, &err);
    std::cout << root["姓名"].asString() << std::endl;
    std::cout << root["学号"].asString() << std::endl;
    std::cout << root["年龄"].asInt() << std::endl;
    std::cout << root["性别"].asString() << std::endl;
    int sz = root["成绩"].size();
    for (int i = 0; i < sz; i++)
    {
        std::cout << root["成绩"][i].asInt() << std::endl;
    }

    delete cr;
}

int main()
{
    std::string ret = serialization();
    unserialization(ret);
    return 0;
}