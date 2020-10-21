#include "common.h"

namespace cppraft
{

    void from_json(const json &j, peerConfig &p)
    {
        j.at("address").get_to(p.address);
        j.at("number").get_to(p.number);
    }

}; // namespace cppraft