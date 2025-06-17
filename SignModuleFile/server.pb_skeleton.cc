#include "../CryptoUtil.h"//hash_password, generate_salt, verify_token
#include "Sign.srpc.h"//srpc service definition
#include "workflow/WFFacilities.h"//wait_group
#include "workflow/MySQLMessage.h"
#include "workflow/MySQLResult.h"
#include "workflow/Workflow.h"
#include "workflow/WFTaskFactory.h"//timer task factory
#include "wfrest/HttpServer.h"
#include "ppconsul/agent.h"//agent for service registration and health check

#include <iostream>

using namespace srpc;
using namespace ppconsul::agent;
using ppconsul::Consul;

static void timer_callback(WFTimerTask *task)
{
    SeriesWork *series_work = series_of(task);
    Agent *agent = static_cast<Agent *>(series_work->get_context());
    agent->servicePass("SignService1","SignService1 keep alive");
    agent->servicePass("SignService2","SignService2 keep alive");
    agent->servicePass("SignService3","SignService3 keep alive");
    WFTimerTask *new_task = WFTaskFactory::create_timer_task(7,0,timer_callback);
    series_work->push_back(new_task);
}

static WFFacilities::WaitGroup wait_group(1);
void sig_handler(int signo)
{
    wait_group.done();
}

class SignServiceImpl : public ::Sign::Sign::Service
{
  public:
    void Signup(::Sign::SignupReq *request, ::Sign::SignupResp *response, srpc::RPCContext *ctx) override
    {
        std::string username = request->username();
        std::string password = request->password();

        std::string salt = CryptoUtil::generate_salt();
        std::string hashed_password = CryptoUtil::hash_password(password, salt);

        response->set_username(username);

        // 写入MySQL
        std::string mysql_url = "mysql://root:123@localhost:3306/webnet";
        std::string sql = "INSERT INTO tbl_user (username, password, salt) VALUES ('" + username + "', '" +
                          hashed_password + "', '" + salt + "')";

        WFMySQLTask *mysql_task = WFTaskFactory::create_mysql_task(mysql_url, 3, [response](WFMySQLTask *task) {
            if(task->get_state()!= WFT_STATE_SUCCESS||
               task->get_resp()->get_packet_type() != MYSQL_PACKET_OK)
            {
                response->set_status(false);
                return;
            }
            else{
                response->set_status(true);
            }
        });

        mysql_task->get_req()->set_query(sql);
        SeriesWork* series_work=ctx->get_series();
        series_work->push_back(mysql_task);
    }

    void Signin(::Sign::SigninReq *request, ::Sign::SigninResp *response, srpc::RPCContext *ctx) override
    {
        // TODO: fill server logic here
    }
};

int main()
{
    signal(SIGINT,sig_handler);

    unsigned short port = 1412;
    SRPCServer server;
    SignServiceImpl sign_impl;
    server.add_service(&sign_impl);

    if (server.start(port) == 0)
    {
        // 指定注册中心 Consul 的ip地址，端口和数据中心
        Consul consul { "http://127.0.0.1:8500", ppconsul::kw::dc="dc1" };
        // 创建代理
        Agent agent { consul };
        // 注册服务 
        agent.registerService(
            kw::id = "SignService1",
            kw::name = "SignService",
            kw::address = "127.0.0.1",
            kw::port = 1412,
            kw::check = TtlCheck(std::chrono::seconds{ 10 })
        );
        agent.registerService(
            kw::id = "SignService2",
            kw::name = "SignService",
            kw::address = "127.0.0.1",
            kw::port = 1413,
            kw::check = TtlCheck(std::chrono::seconds{ 10 })
        );
        agent.registerService(
            kw::id = "SignService3",
            kw::name = "SignService",
            kw::address = "127.0.0.1",
            kw::port = 1414,
            kw::check = TtlCheck(std::chrono::seconds{ 10 })
        );

        
        // 定时发送心跳包
        WFTimerTask* timerTask = WFTaskFactory::create_timer_task(7, 0, timer_callback);

        SeriesWork* series = Workflow::create_series_work(timerTask, nullptr);
        series->set_context(&agent);    // 设置序列的上下文
        series->start();
        wait_group.wait();
        server.stop();
    }
    else
    {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return -1;
    }

    return 0;
}
