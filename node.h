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
        grpc::Status PreVote(grpc::ServerContext *ctx, const PreVoteReq *req, PreVoteResp *resp) override;

        void Start();
        void Stop();
        void Tick();
        void Apply();
        std::vector<LogEntry> GetLogs();

    private:
        void resetTick();
        void startRpc(std::promise<void> &p);
        void asCandidate();
        void asLeader();
        void asFollower();
        void asPreCandidate();
        void sendAppendEntries(AppendEntriesReq req, int i, bool isHeartBeat);
        void sendRequestVote(RequestVoteReq req, int i);
        void sendPreVote(PreVoteReq, int i);
        void sendHeartBeat();
        void updateTerm(int term);
        bool logsMoreUpdate(int term, int index);
        void adjustCommitIndex();
        void waitApplied(int index);

        bool m_stop;
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
        // id
        int m_number;
        Status m_status;
        // for vote
        int m_candidate_count;
        // for prevote
        int m_precandidate_count;
        int m_elapsed;
        int m_election_timeout;

        // for wait to response to client
        std::unordered_map<int, std::promise<void>> m_applied;
    };
}; // namespace cppraft
#endif