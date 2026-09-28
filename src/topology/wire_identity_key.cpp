#include "eke_dx_wire/topology/wire_identity_key.hpp"

#include <algorithm>

namespace eke::dx::wire {

std::string canonical_wire_identity_key(const Wire& wire) {
    std::string a = wire.start_endpoint;
    std::string b = wire.end_endpoint;
    if (a > b) {
        std::swap(a, b);
    }

    std::vector<std::string> edges = wire.topology_edges;
    std::sort(edges.begin(), edges.end());

    std::vector<std::string> segments = wire.conductor_segments;
    std::sort(segments.begin(), segments.end());

    std::string key = a + "|" + b;
    for (const auto& edge : edges) {
        key += "|E:" + edge;
    }
    for (const auto& segment : segments) {
        key += "|S:" + segment;
    }
    return key;
}

} // namespace eke::dx::wire
