#include <memory>

#include <chrono>

#include "node.h"

int main()
{
    spdlog::set_level(spdlog::level::debug);
    std::vector<Peer> peers;
    std::string address("localhost:50051");
    std::unique_ptr<Node> node(new Node(address, 0, peers));
    node->Start();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}