#ifndef CLIENT_H
#define CLINET_H

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "message.grpc.pb.h"
#include "spdlog/spdlog.h"

class Client
{
private:
public:
    Client();
    ~Client();

    std::unique_ptr<Raft::Stub> Connect(std::string address);
    void Request(std::string payload);

private:
    std::vector<std::string> servers;
};

#endif