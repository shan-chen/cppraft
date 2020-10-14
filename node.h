#ifndef NODE_H
#define NODE_H

#include <mutex>
#include <thread>

#include "common.h"

class Node final : public Raft::Service
{
public:
    Node();
    Node(std::string address, int number, std::vector<Peer> &peers);
    ~Node();

    grpc::Status AppendEntries(grpc::ServerContext *ctx, const AppendEntriesReq *req, AppendEntriesResp *resp) override;
    grpc::Status RequestVote(grpc::ServerContext *ctx, const RequestVoteReq *req, RequestVoteResp *resp) override;

    void Start();
    void Stop();

private:
    void startRpc();

    int currentTerm;
    int votedFor;
    std::vector<LogEntry> logs;
    int commitIndex;
    int lastApplied;
    std::vector<int> nextIndex;
    std::vector<int> matchIndex;

    //std::unique_ptr<Transporter> transporter;
    std::unique_ptr<grpc::Server> m_server;
    std::thread m_t;
    std::vector<Peer> m_peers;
    std::string m_address;
    int m_number;
};

#endif