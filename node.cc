#include "node.h"

Node::Node()
{
    m_address = "localhost:50051";
}

Node::Node(std::string address, int number, std::vector<Peer> &peers) : m_address(address),
                                                                        m_number(number),
                                                                        m_peers(std::move(peers))
{
    // for (auto &peer : peers)
    // {
    //     Peer p;
    //     p.address = peer.address;
    //     p.number = peer.number;
    //     m_peers.push_back(p);
    // }
}

Node::~Node()
{
    Stop();
    if (m_t.joinable())
        m_t.join();
}

void Node::Start()
{
    for (auto &peer : m_peers)
    {
        peer.stub = std::move(Raft::NewStub(grpc::CreateChannel(peer.address, grpc::InsecureChannelCredentials())));
    }
    m_t = std::thread(&Node::startRpc, this);
    spdlog::info("node start");
}

void Node::Stop()
{
    if (m_server == nullptr)
        return;
    m_server->Shutdown();
    spdlog::info("node stop");
}

void Node::startRpc()
{
    spdlog::debug("startRpc start");
    std::string server_address(m_address);
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    m_server = std::move(builder.BuildAndStart());
    spdlog::info("rpc server start, listening on {}", server_address);
    m_server->Wait();
}

grpc::Status Node::AppendEntries(grpc::ServerContext *ctx, const AppendEntriesReq *req, AppendEntriesResp *resp)
{
    return grpc::Status::OK;
}

grpc::Status Node::RequestVote(grpc::ServerContext *ctx, const RequestVoteReq *req, RequestVoteResp *resp)
{
    return grpc::Status::OK;
}