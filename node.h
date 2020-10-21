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

    private:
        void startRpc();
        void mainLoop();
        void asCandidate();
        void asLeader();
        void asFollower();
        void startCampaign();
        void sendAppendEntries(std::unique_ptr<Raft::Stub> &stub);
        void sendRequestVote(int number, std::unique_ptr<Raft::Stub> &stub);
        void sendMessage(Message &msg, std::unique_ptr<Raft::Stub> &stub);

        std::mutex mu;
        int currentTerm;
        int votedFor;
        std::vector<LogEntry> logs;
        int commitIndex;
        int lastApplied;

        // only for leader
        std::vector<int> nextIndex;
        std::vector<int> matchIndex;

        std::unique_ptr<grpc::Server> m_server;
        std::thread m_t;
        std::vector<Peer> m_peers;
        std::string m_address;
        int m_number;
        Status m_status;
        int m_candidate_count;
        std::promise<void> m_candidate_promise;
        std::promise<void> m_heartbeat_promise;
    };
}; // namespace cppraft
#endif