#include "CloudiskServer.h"
#include <iostream>
#include <signal.h>

WFFacilities::WaitGroup g_waitGroup{1};

void sig_handler(int)
{
    g_waitGroup.done();
}

int main()
{

    signal(SIGINT, sig_handler);

    CloudiskServer svr;

    // 注册路由
    svr.register_modules();

    if (svr.track().start(8888) == 0)
    {
        svr.list_routes();
        g_waitGroup.wait();
        svr.stop();
    }
    else
    {
        std::cerr << "Error: server start failed!\n";
    }
}

