#include <memory>
#include <fstream>

#include <chrono>

#include "cmdline.h"
#include "node.h"

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    cmdline::parser p;
    p.add<std::string>("config", 'c', "config file", true, "");
    p.parse_check(argc, argv);

    json j;
    std::ifstream f(p.get<std::string>("config"));
    f >> j;
    std::string address = j.at("address");
    int number = j.at("number");
    spdlog::debug("I am NO.{}", number);
    int size = j.at("peers").size();
    std::vector<cppraft::Peer> peers;
    for (int i = 0; i < size; ++i)
    {
        cppraft::peerConfig pc = j["peers"][i];
        peers.push_back(cppraft::Peer(pc.address, pc.number));
        spdlog::debug("Peer Info {} : {}", pc.number, pc.address);
    }

    std::unique_ptr<cppraft::Node> node(new cppraft::Node(address, number, peers));
    std::thread t([&]() { node->Start(); });
    t.detach();

    // for test
    std::this_thread::sleep_for(std::chrono::seconds(30));
    node->Stop();
    auto logs = node->GetLogs();
    for (auto &log : logs)
    {
        spdlog::debug("log {} : {}", log.index(), log.payload().payload());
    }
    return 0;
}