#include "log.hpp"

int main()
{
    DLOG("hello log:%d", 100);
    ILOG("information");
    ELOG("error");
    return 0;
}