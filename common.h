#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "spdlog/spdlog.h"
#include "message.grpc.pb.h"
#include "json.hpp"

using json = nlohmann::json;

namespace cppraft
{
    struct Peer
    {
        std::string address;
        int number;
        std::unique_ptr<Raft::Stub> stub;
        Peer(){};
        Peer(std::string a, int n) : address(a), number(n){};
        Peer(Peer &&other)
        {
            address = other.address;
            number = other.number;
            stub = std::move(other.stub);
        };
    };

    enum Status
    {
        LEADER = 1,
        CANDIDATE = 2,
        FOLLOWER = 3,
    };

    enum MessageType
    {
        REQUESTVOTE = 1,
        APPENDENTRIES = 2,
    };

    struct Message
    {
        MessageType type;
    };

    const int TICKINTERVAL = 100;
    const int HEARTBEATTIMEOUT = 3;
    const int LEADERTIMEOUT = 5;

    struct peerConfig
    {
        std::string address;
        int number;
    };

    void from_json(const json &j, peerConfig &p);
} // namespace cppraft

#endif