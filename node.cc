#include "node.h"

using namespace cppraft;

Node::Node(std::string address, int number, std::vector<Peer> &peers) : m_address(address),
                                                                        m_number(number),
                                                                        m_peers(std::move(peers))
{
    currentTerm = 0;
    votedFor = -1;
    commitIndex = 0;
    lastApplied = 0;
    nextIndex = std::vector<int>(m_peers.size(), 1);
    matchIndex = std::vector<int>(m_peers.size(), 0);
}

Node::~Node()
{
    Stop();
    if (m_t.joinable())
        m_t.join();
}

void Node::Start()
{
    m_t = std::thread(&Node::startRpc, this);
    spdlog::info("node start");
    m_status = FOLLOWER;
    LogEntry log;
    log.set_term(0);
    log.set_index(0);
    log.set_payload("");
    logs.push_back(log);
    for (auto &peer : m_peers)
    {
        auto ch = grpc::CreateChannel(peer.address, grpc::InsecureChannelCredentials());
        ch->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(60, GPR_TIMESPAN)));
        peer.stub = std::move(Raft::NewStub(ch));
        spdlog::info("connected to peer {} {}", peer.number, peer.address);
    }
    mainLoop();
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
    spdlog::debug("startRpc quit");
}

void Node::mainLoop()
{
    while (true)
    {
        mu.lock();
        auto s = m_status;
        mu.unlock();
        switch (s)
        {
        case FOLLOWER:
            asFollower();
            break;
        case CANDIDATE:
            asCandidate();
            break;
        case LEADER:
            asLeader();
            break;
        }
    }
}

void Node::asFollower()
{
    spdlog::debug("become FOLLOWER");
    mu.lock();
    m_heartbeat_promise = std::promise<void>();
    std::future<void> f = m_heartbeat_promise.get_future();
    mu.unlock();
    while (true)
    {
        auto fs = f.wait_for(std::chrono::milliseconds(LEADERTIMEOUT));
        if (fs == std::future_status::timeout)
        {
            mu.lock();
            m_status = CANDIDATE;
            mu.unlock();
            return;
        }
        else
        {
            mu.lock();
            m_heartbeat_promise = std::promise<void>();
            std::future<void> f = m_heartbeat_promise.get_future();
            mu.unlock();
        }
    }
}

void Node::asCandidate()
{
    spdlog::debug("become CANDIDATE");
    srand((unsigned)time(0));
    std::this_thread::sleep_for(std::chrono::milliseconds(CANDIDATERANDTIME + rand() % CANDIDATERANDTIME));
    mu.lock();
    m_candidate_count = 1;
    currentTerm++;
    votedFor = m_number;
    m_candidate_promise = std::promise<void>();
    std::future<void> f = m_candidate_promise.get_future();
    mu.unlock();
    for (auto &peer : m_peers)
    {
        std::thread t(&Node::sendRequestVote, this, peer.number, std::ref(peer.stub));
        t.detach();
    }
    auto fs = f.wait_for(std::chrono::milliseconds(CANDIDATETIMEOUT));
}

void Node::asLeader()
{
    mu.lock();
    spdlog::debug("become LEADER at term {}", currentTerm);
    mu.unlock();
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

void Node::sendRequestVote(int number, std::unique_ptr<Raft::Stub> &stub)
{
    mu.lock();
    if (m_status != CANDIDATE)
    {
        mu.unlock();
        return;
    }
    RequestVoteReq req;
    req.set_term(currentTerm);
    req.set_candidataid(m_number);
    req.set_lastlogindex(logs.back().index());
    req.set_lastlogterm(logs.back().term());
    spdlog::debug("send RequestVote to {} at term {}", number, currentTerm);
    mu.unlock();
    RequestVoteResp resp;
    grpc::ClientContext ctx;
    grpc::Status s = stub->RequestVote(&ctx, req, &resp);
    if (!s.ok())
    {
        spdlog::error("requestVote rpc failed with code {} and message {}", s.error_code(), s.error_message());
        return;
    }

    mu.lock();

    if (resp.term() > currentTerm)
    {
        currentTerm = resp.term();
        m_status = FOLLOWER;
        votedFor = -1;
    }
    else if (resp.votegranted())
    {
        spdlog::debug("receive vote from {}", number);
        m_candidate_count++;
        if (m_candidate_count == ((m_peers.size() + 1) / 2) + 1)
        {
            m_status = LEADER;
            try
            {
                m_candidate_promise.set_value();
            }
            catch (...)
            {
                spdlog::error("sendRequestVote set value failed");
            }
        }
    }
    mu.unlock();
}

void Node::sendAppendEntries(std::unique_ptr<Raft::Stub> &stub)
{
    AppendEntriesReq req;
    AppendEntriesResp resp;
    grpc::ClientContext ctx;
    grpc::Status s = stub->AppendEntries(&ctx, req, &resp);
    if (!s.ok())
    {
        spdlog::error("appendEntries rpc failed with code {} and message {}", s.error_code(), s.error_message());
        return;
    }
}

void Node::sendMessage(Message &msg, std::unique_ptr<Raft::Stub> &stub)
{
}

grpc::Status Node::AppendEntries(grpc::ServerContext *ctx, const AppendEntriesReq *req, AppendEntriesResp *resp)
{
    mu.lock();
    if (req->term() < currentTerm)
    {
        resp->set_success(false);
        resp->set_term(currentTerm);
        mu.unlock();
        return grpc::Status::OK;
    }

    if (req->term() > currentTerm)
    {
        currentTerm = req->term();
        votedFor = -1;
    }

    m_status = FOLLOWER;
    // heartbeat
    if (req->entries().size() == 0)
    {
        try
        {
            m_heartbeat_promise.set_value();
        }
        catch (...)
        {
            spdlog::error("AppendEntries set value failed");
        }
        mu.unlock();
        return grpc::Status::OK;
    }

    if (logs.size() < req->prevlogindex() || logs[req->prevlogindex()].term() != req->prevlogterm())
    {
        resp->set_success(false);
        resp->set_term(req->term());
        mu.unlock();
        return grpc::Status::OK;
    }

    resp->set_success(true);
    resp->set_term(req->term());
    int existed = 0;
    for (auto iter = req->entries().begin(); iter != req->entries().end(); ++iter)
    {
        if (logs.size() <= iter->index())
            break;
        if (logs.size() > iter->index() && logs[iter->index()].term() != iter->term())
        {
            while (logs.back().index() >= iter->index())
                logs.pop_back();
            break;
        }
        ++existed;
    }
    for (auto iter = req->entries().begin() + existed; iter < req->entries().end(); ++iter)
    {
        logs.push_back(*iter);
    }
    mu.unlock();
    return grpc::Status::OK;
}

grpc::Status Node::RequestVote(grpc::ServerContext *ctx, const RequestVoteReq *req, RequestVoteResp *resp)
{
    mu.lock();
    if (req->term() < currentTerm)
    {
        mu.unlock();
        resp->set_votegranted(false);
        resp->set_term(currentTerm);
        return grpc::Status::OK;
    }

    if (req->term() > currentTerm)
    {
        currentTerm = req->term();
        m_status = FOLLOWER;
        votedFor = -1;
    }

    resp->set_term(req->term());
    if (votedFor == -1)
    {
        if (logs.back().term() > req->lastlogterm() || (logs.back().term() == req->lastlogterm() && logs.back().index() >= req->lastlogindex()))
        {
            resp->set_votegranted(true);
            votedFor = req->candidataid();
            spdlog::debug("vote for {} at term {}", req->candidataid(), req->term());
        }
        else
            resp->set_votegranted(false);
    }
    mu.unlock();
    return grpc::Status::OK;
}
