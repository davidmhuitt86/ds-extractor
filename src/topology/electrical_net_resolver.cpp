#include "eke_dx_wire/topology/electrical_net_resolver.hpp"

#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"

#include <algorithm>
#include <utility>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

void sort_unique(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

bool contains(
    const std::vector<std::string>& values,
    const std::string& value) {
    return std::binary_search(values.begin(), values.end(), value);
}

} // namespace

ElectricalNetResolver::ElectricalNetResolver(
    DistributionDecompositionConfig distribution_config)
    : distribution_config_(distribution_config) {}

ElectricalNetResolutionArtifacts ElectricalNetResolver::resolve(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<ConductorSegment>& conductors,
    const std::vector<CircuitRoleEvidence>& observation_evidence,
    const std::string& source_id,
    int page) const {

    DistributionDecompositionArtifacts decomposed =
        DistributionDecomposer(distribution_config_).decompose(
            nodes,
            edges,
            endpoints,
            conductors,
            source_id,
            page);

    CircuitRoleEvidenceBuilder role_evidence_builder;
    std::vector<CircuitRoleEvidence> role_evidence =
        role_evidence_builder.build(endpoints);
    role_evidence.insert(
        role_evidence.end(),
        observation_evidence.begin(),
        observation_evidence.end());

    const CircuitRoleResolutionArtifacts role_resolution =
        CircuitRoleResolver().resolve(
            decomposed.nets,
            endpoints,
            role_evidence);

    ElectricalNetResolutionArtifacts result;
    result.nets = role_resolution.nets;
    result.wires = std::move(decomposed.wires);
    result.unresolved_net_ids = role_resolution.unresolved_net_ids;

    std::unordered_set<std::string> endpoint_ids;
    endpoint_ids.reserve(endpoints.size());
    for (const auto& endpoint : endpoints) {
        endpoint_ids.insert(endpoint.id);
    }

    std::unordered_set<std::string> emitted_net_ids;
    std::vector<ElectricalNet> normalized_nets;
    normalized_nets.reserve(result.nets.size());

    for (auto net : result.nets) {
        sort_unique(net.endpoint_ids);
        sort_unique(net.splice_node_ids);
        sort_unique(net.topology_edges);

        if (!net.anchor_endpoint.empty() &&
            !contains(net.endpoint_ids, net.anchor_endpoint)) {
            net.anchor_endpoint.clear();
            net.role = DistributionRole::Unknown;
            net.confidence = ConfidenceClass::Unresolved;
            result.unresolved_net_ids.push_back(net.id);
        }

        bool references_known_endpoints = true;
        for (const auto& endpoint_id : net.endpoint_ids) {
            if (!endpoint_ids.contains(endpoint_id)) {
                references_known_endpoints = false;
                break;
            }
        }
        if (!references_known_endpoints) {
            net.role = DistributionRole::Unknown;
            net.confidence = ConfidenceClass::Unresolved;
            result.unresolved_net_ids.push_back(net.id);
        }

        if (!emitted_net_ids.insert(net.id).second) {
            continue;
        }

        normalized_nets.push_back(std::move(net));
    }

    std::sort(
        normalized_nets.begin(),
        normalized_nets.end(),
        [](const ElectricalNet& a, const ElectricalNet& b) {
            return a.id < b.id;
        });

    std::sort(
        result.unresolved_net_ids.begin(),
        result.unresolved_net_ids.end());
    result.unresolved_net_ids.erase(
        std::unique(
            result.unresolved_net_ids.begin(),
            result.unresolved_net_ids.end()),
        result.unresolved_net_ids.end());

    result.nets = std::move(normalized_nets);

    std::sort(
        result.wires.begin(),
        result.wires.end(),
        [](const Wire& a, const Wire& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire
