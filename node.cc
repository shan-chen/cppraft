#include "node.h"

using namespace cppraft;

Node::Node(std::string address, int number, std::vector<Peer> &peers) : m_address(address),
                                                                        m_number(number),
                                                                        m_peers(std::move(peers))
{
    m_currentTerm = 0;
    m_votedFor = -1;
    m_commitIndex = 0;
    m_lastApplied = 0;
    m_nextIndex = std::vector<int>(m_peers.size(), 1);
    m_matchIndex = std::vector<int>(m_peers.size(), 0);
}

Node::~Node()
{
    Stop();
}

void Node::Start()
{
    std::thread rpc(&Node::startRpc, this);
    rpc.detach();
    spdlog::info("node start");
    m_status = FOLLOWER;
    LogEntry log;
    log.set_term(0);
    log.set_index(0);
    log.set_payload("");
    m_logs.push_back(log);
    for (auto &peer : m_peers)
    {
        auto ch = grpc::CreateChannel(peer.address, grpc::InsecureChannelCredentials());
        ch->WaitForConnected(gpr_time_add(
            gpr_now(GPR_CLOCK_REALTIME),
            gpr_time_from_seconds(10, GPR_TIMESPAN)));
        peer.stub = std::move(Raft::NewStub(ch));
        spdlog::info("connected to peer {} {}", peer.number, peer.address);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    resetTick();
    std::thread tick(&Node::Tick, this);
    // mainLoop();
    asFollower();
    tick.join();
}

void Node::Stop()
{
    if (m_server == nullptr)
        return;
    m_server->Shutdown();
    spdlog::info("node stop");
}

void Node::Tick()
{
    while (true)
    {
        m_elapsed++;
        switch (m_status)
        {
        case FOLLOWER:
        case CANDIDATE:
            if (m_elapsed >= m_election_timeout)
            {
                m_status = CANDIDATE;
                resetTick();
                asCandidate();
            }
            break;

        case LEADER:
            if (m_elapsed >= HEARTBEATTIMEOUT)
            {
                sendHeartBeat();
            }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(TICKINTERVAL));
    }
}

void Node::resetTick()
{
    m_elapsed = 0;
    srand((unsigned)time(0));
    m_election_timeout = rand() % LEADERTIMEOUT + LEADERTIMEOUT;
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

// void Node::mainLoop()
// {
//     while (true)
//     {
//         switch (m_status)
//         {
//         case FOLLOWER:
//             asFollower();
//             break;
//         case CANDIDATE:
//             asCandidate();
//             break;
//         case LEADER:
//             asLeader();
//             break;
//         }
//     }
// }

void Node::asFollower()
{
    spdlog::debug("become FOLLOWER");
}

void Node::asCandidate()
{
    spdlog::debug("become CANDIDATE");
    m_mu.lock();
    m_candidate_count = 1;
    m_currentTerm++;
    m_votedFor = m_number;
    RequestVoteReq req;
    req.set_term(m_currentTerm);
    req.set_candidataid(m_number);
    req.set_lastlogindex(m_logs.back().index());
    req.set_lastlogterm(m_logs.back().term());
    spdlog::debug("send RequestVote at term {}", m_currentTerm);
    m_mu.unlock();
    for (auto &peer : m_peers)
    {
        std::thread t(&Node::sendRequestVote, this, req, std::ref(peer));
        t.detach();
    }
}

void Node::asLeader()
{
    spdlog::debug("become LEADER at term {}", m_currentTerm);
}

void Node::sendHeartBeat()
{
    m_elapsed = 0;
    m_mu.lock();
    AppendEntriesReq req;
    req.set_term(m_currentTerm);
    req.set_leaderid(m_number);
    req.set_leadercommit(m_commitIndex);
    req.set_prevlogindex(m_logs.back().index());
    req.set_prevlogterm(m_logs.back().term());
    spdlog::debug("send heartbeat at term {}", m_currentTerm);
    m_mu.unlock();
    for (auto &p : m_peers)
    {
        std::thread t(&Node::sendAppendEntries, this, req, std::ref(p));
        t.detach();
    }
}

void Node::sendRequestVote(const RequestVoteReq &req, Peer &peer)
{
    RequestVoteResp resp;
    grpc::ClientContext ctx;
    grpc::Status s = peer.stub->RequestVote(&ctx, req, &resp);
    if (!s.ok())
    {
        spdlog::error("requestVote rpc failed with code {} and message {}", s.error_code(), s.error_message());
        return;
    }

    m_mu.lock();

    if (resp.term() > m_currentTerm)
    {
        m_currentTerm = resp.term();
        m_status = FOLLOWER;
        m_votedFor = -1;
    }
    else if (resp.votegranted())
    {
        spdlog::debug("receive vote from {}", peer.number);
        m_candidate_count++;
        if (m_candidate_count == ((m_peers.size() + 1) / 2) + 1)
        {
            m_status = LEADER;
            asLeader();
        }
    }
    m_mu.unlock();
}

void Node::sendAppendEntries(const AppendEntriesReq &req, Peer &peer)
{
    AppendEntriesResp resp;
    grpc::ClientContext ctx;
    grpc::Status s = peer.stub->AppendEntries(&ctx, req, &resp);
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
    m_mu.lock();
    if (req->term() < m_currentTerm)
    {
        resp->set_success(false);
        resp->set_term(m_currentTerm);
        m_mu.unlock();
        return grpc::Status::OK;
    }

    if (req->term() > m_currentTerm)
    {
        m_currentTerm = req->term();
        m_votedFor = -1;
    }

    m_status = FOLLOWER;
    resetTick();

    if (m_logs.size() < req->prevlogindex() || m_logs[req->prevlogindex()].term() != req->prevlogterm())
    {
        resp->set_success(false);
        resp->set_term(req->term());
        m_mu.unlock();
        return grpc::Status::OK;
    }

    resp->set_success(true);
    resp->set_term(req->term());
    int existed = 0;
    for (auto iter = req->entries().begin(); iter != req->entries().end(); ++iter)
    {
        if (m_logs.size() <= iter->index())
            break;
        if (m_logs.size() > iter->index() && m_logs[iter->index()].term() != iter->term())
        {
            while (m_logs.back().index() >= iter->index())
                m_logs.pop_back();
            break;
        }
        ++existed;
    }
    for (auto iter = req->entries().begin() + existed; iter < req->entries().end(); ++iter)
    {
        m_logs.push_back(*iter);
    }
    m_mu.unlock();
    return grpc::Status::OK;
}

grpc::Status Node::RequestVote(grpc::ServerContext *ctx, const RequestVoteReq *req, RequestVoteResp *resp)
{
    m_mu.lock();
    if (req->term() < m_currentTerm)
    {
        m_mu.unlock();
        resp->set_votegranted(false);
        resp->set_term(m_currentTerm);
        return grpc::Status::OK;
    }

    if (req->term() > m_currentTerm)
    {
        m_currentTerm = req->term();
        m_status = FOLLOWER;
        m_votedFor = -1;
    }

    resp->set_term(req->term());
    if (m_votedFor == -1)
    {
        if (m_logs.back().term() > req->lastlogterm() || (m_logs.back().term() == req->lastlogterm() && m_logs.back().index() >= req->lastlogindex()))
        {
            resp->set_votegranted(true);
            m_votedFor = req->candidataid();
            resetTick();
            spdlog::debug("vote for {} at term {}", req->candidataid(), req->term());
        }
        else
            resp->set_votegranted(false);
    }
    m_mu.unlock();
    return grpc::Status::OK;
}
