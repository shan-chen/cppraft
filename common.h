#include <string>
#include <vector>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "spdlog/spdlog.h"
#include "message.grpc.pb.h"

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