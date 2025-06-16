#include "Sign.srpc.h"
#include "workflow/WFFacilities.h"
#include <iostream>

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
        std::cout << "Received Signup request" << std::endl;
        std::cout << "Username: " << request->username() << std::endl;
        std::cout << "Password: " << request->password() << std::endl;      
        response->set_status(1); // Assuming 1 means success
        response->set_username(request->username());
    }

    void Signin(::Sign::SigninReq *request, ::Sign::SigninResp *response, srpc::RPCContext *ctx) override
    {
        // TODO: fill server logic here
    }
};

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    unsigned short port = 1412;
    SRPCServer server;

    SignServiceImpl sign_impl;
    server.add_service(&sign_impl);

    if (server.start(port)==0)
    {
        wait_group.wait();
        server.stop();
    }
    else
    {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return -1;
    }

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
