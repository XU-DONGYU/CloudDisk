#include "../CryptoUtil.h"
#include "Sign.srpc.h"
#include "workflow/WFFacilities.h"
#include <iostream>
#include <workflow/MySQLMessage.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>

using namespace srpc;

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
