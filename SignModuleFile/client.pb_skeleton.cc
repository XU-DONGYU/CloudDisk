#include "Sign.srpc.h"
#include "workflow/WFFacilities.h"

using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

static void signup_done(::Sign::SignupResp *response, srpc::RPCContext *context)
{
	if(context->success())
	{
		std::cout << "Signup successful!" << std::endl;
		std::cout << "Username: " << response->username() << std::endl;
		std::cout << "Status: " << response->status() << std::endl;
	}
	else
	{
		std::cerr << "Signup failed: " << std::endl;

	}
}

static void signin_done(::Sign::SigninResp *response, srpc::RPCContext *context)
{
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	const char *ip = "127.0.0.1";
	unsigned short port = 1412;

	::Sign::Sign::SRPCClient client(ip, port);

	// example for RPC method call
	::Sign::SignupReq signup_req;
	signup_req.set_username("testuser");
	signup_req.set_password("testpassword");

	
	client.Signup(&signup_req, signup_done);

	wait_group.wait();
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
