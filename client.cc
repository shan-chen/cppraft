#include "client.h"

Client::Client()
{
    servers = {"localhost:5001", "localhost:5002", "localhost:5003"};
}

Client::~Client()
{
}

std::unique_ptr<Raft::Stub> Client::Connect(std::string address)
{
    auto ch = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    ch->WaitForConnected(gpr_time_add(
        gpr_now(GPR_CLOCK_REALTIME),
        gpr_time_from_seconds(10, GPR_TIMESPAN)));
    return Raft::NewStub(ch);
}

void Client::Request(std::string payload)
{
    ClientCommandRequestReq req;
    ClientCommandRequestResp resp;
    req.set_payload(payload);
    req.set_type(Normal);
    for (auto &addr : servers)
    {
        grpc::ClientContext ctx;
        auto stub = std::move(Connect(addr));
        stub->ClientCommandRequest(&ctx, req, &resp);
        if (resp.success())
        {
            spdlog::info("request to {} success", addr);
            break;
        }
        spdlog::debug("{} is not leader", addr);
    }
}

int main()
{
    spdlog::set_level(spdlog::level::debug);

    Client client;
    std::vector<std::string> dict{"aaa", "bbb", "ccc", "ddd", "eee"};

    for (auto &p : dict)
    {
        std::thread t(&Client::Request, &client, p);
        t.detach();
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}