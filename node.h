#ifndef NODE_H
#define NODE_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <future>
#include <chrono>
#include <ctime>
#include <cstdlib>

#include "common.h"

namespace cppraft
{
    class Node final : public Raft::Service
    {
    public:
        Node() = delete;
        Node(std::string address, int number, std::vector<Peer> &peers);
        ~Node();

        grpc::Status AppendEntries(grpc::ServerContext *ctx, const AppendEntriesReq *req, AppendEntriesResp *resp) override;
        grpc::Status RequestVote(grpc::ServerContext *ctx, const RequestVoteReq *req, RequestVoteResp *resp) override;

        void Start();
        void Stop();
        void Tick();

    private:
        void resetTick();
        void startRpc();
        // void mainLoop();
        void asCandidate();
        void asLeader();
        void asFollower();
        void startCampaign();
        void sendAppendEntries(const AppendEntriesReq &req, Peer &peer);
        void sendRequestVote(const RequestVoteReq &req, Peer &peer);
        void sendHeartBeat();
        void sendMessage(Message &msg, std::unique_ptr<Raft::Stub> &stub);

        std::mutex m_mu;
        int m_currentTerm;
        int m_votedFor;
        std::vector<LogEntry> m_logs;
        int m_commitIndex;
        int m_lastApplied;

        // only for leader
        std::vector<int> m_nextIndex;
        std::vector<int> m_matchIndex;

        std::unique_ptr<grpc::Server> m_server;
        std::vector<Peer> m_peers;
        std::string m_address;
        int m_number;
        Status m_status;
        int m_candidate_count;
        int m_elapsed;
        int m_election_timeout;
        // std::promise<void> m_candidate_promise;
        // std::promise<void> m_heartbeat_promise;
    };
}; // namespace cppraft
#endif