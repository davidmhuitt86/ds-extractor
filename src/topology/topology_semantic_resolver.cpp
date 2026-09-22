#include "eke_dx_wire/topology/topology_semantic_resolver.hpp"

#include <algorithm>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

int confidence_rank(ConfidenceClass c) {
    switch (c) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

} // namespace

std::vector<TopologyNode> TopologySemanticResolver::resolve(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyConnectionEvidence>& evidence) const {

    std::unordered_map<std::string, const TopologyConnectionEvidence*> by_node;
    for (const auto& item : evidence) {
        if (item.node_id.empty() ||
            item.state == TopologyConnectionState::Unknown) {
            continue;
        }

        const auto it = by_node.find(item.node_id);
        if (it == by_node.end()) {
            by_node[item.node_id] = &item;
            continue;
        }

        const int incoming = confidence_rank(item.confidence);
        const int existing = confidence_rank(it->second->confidence);

        if (incoming > existing) {
            by_node[item.node_id] = &item;
        } else if (
            incoming == existing &&
            item.state != it->second->state) {
            // Equal-strength conflicting evidence must remain unresolved.
            by_node.erase(it);
        }
    }

    std::vector<TopologyNode> result = nodes;

    for (auto& node : result) {
        const auto it = by_node.find(node.id);
        if (it == by_node.end()) {
            continue;
        }

        const auto& item = *it->second;
        if (item.state == TopologyConnectionState::Connected) {
            node.electrically_connective = true;
            if (node.type == TopologyNodeType::Crossing ||
                node.type == TopologyNodeType::Junction) {
                node.type = TopologyNodeType::Splice;
            }
        } else if (item.state == TopologyConnectionState::NotConnected) {
            node.electrically_connective = false;
            node.type = TopologyNodeType::Crossing;
        }
    }

    std::sort(
        result.begin(), result.end(),
        [](const TopologyNode& a, const TopologyNode& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire
