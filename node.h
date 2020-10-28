#ifndef NODE_H
#define NODE_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <future>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>

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
        grpc::Status ClientCommandRequest(grpc::ServerContext *ctx, const ClientCommandRequestReq *req, ClientCommandRequestResp *resp) override;

        void Start();
        void Stop();
        void Tick();
        void Apply();
        std::vector<LogEntry> GetLogs();

    private:
        void resetTick();
        void startRpc(std::promise<void> &p);
        // void mainLoop();
        void asCandidate();
        void asLeader();
        void asFollower();
        void startCampaign();
        void sendAppendEntries(AppendEntriesReq req, int i, bool isHeartBeat);
        void sendRequestVote(RequestVoteReq req, int i);
        void sendHeartBeat();
        void adjustCommitIndex();
        void waitApplied(int index);

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

        std::unordered_map<int, std::promise<void>> m_applied;
    };
}; // namespace cppraft
#endif