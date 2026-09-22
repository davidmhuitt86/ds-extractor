#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

int confidence_rank(ConfidenceClass confidence) {
    switch (confidence) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

bool intrinsic_role(
    const EndpointCandidate& endpoint,
    DistributionRole& role) {

    if (endpoint.kind == EndpointKind::Ground ||
        endpoint.terminal_role == TerminalRole::GroundTerminal) {
        role = DistributionRole::Ground;
        return true;
    }

    if (endpoint.terminal_role == TerminalRole::PowerSource) {
        role = DistributionRole::PowerFeed;
        return true;
    }

    return false;
}

} // namespace

CircuitRoleResolutionArtifacts CircuitRoleResolver::resolve(
    const std::vector<ElectricalNet>& nets,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<CircuitRoleEvidence>& evidence) const {

    CircuitRoleResolutionArtifacts result;
    result.nets = nets;

    std::unordered_map<std::string, const EndpointCandidate*> endpoint_by_id;
    for (const auto& endpoint : endpoints) {
        endpoint_by_id.emplace(endpoint.id, &endpoint);
    }

    // External semantic evidence is first-wins by endpoint, matching the
    // terminal semantic boundary. Conflicting evidence must be resolved by
    // the producer rather than silently overwritten here.
    std::unordered_map<std::string, const CircuitRoleEvidence*> evidence_by_endpoint;
    for (const auto& item : evidence) {
        if (item.endpoint_id.empty() ||
            item.role == DistributionRole::Unknown) {
            continue;
        }
        evidence_by_endpoint.emplace(item.endpoint_id, &item);
    }

    for (auto& net : result.nets) {
        struct Candidate {
            DistributionRole role;
            ConfidenceClass confidence;
            std::string endpoint_id;
        };

        std::vector<Candidate> candidates;

        for (const auto& endpoint_id : net.endpoint_ids) {
            const auto endpoint_it = endpoint_by_id.find(endpoint_id);
            if (endpoint_it != endpoint_by_id.end()) {
                DistributionRole role = DistributionRole::Unknown;
                if (intrinsic_role(*endpoint_it->second, role)) {
                    candidates.push_back({
                        role,
                        endpoint_it->second->confidence,
                        endpoint_id
                    });
                }
            }

            const auto evidence_it = evidence_by_endpoint.find(endpoint_id);
            if (evidence_it != evidence_by_endpoint.end()) {
                const auto& item = *evidence_it->second;
                candidates.push_back({
                    item.role,
                    item.confidence,
                    endpoint_id
                });
            }
        }

        if (candidates.empty()) {
            if (net.role == DistributionRole::Unknown) {
                net.confidence = ConfidenceClass::Unresolved;
                result.unresolved_net_ids.push_back(net.id);
            }
            continue;
        }

        int best_rank = -1;
        for (const auto& candidate : candidates) {
            best_rank = std::max(
                best_rank, confidence_rank(candidate.confidence));
        }

        std::vector<Candidate> best;
        for (const auto& candidate : candidates) {
            if (confidence_rank(candidate.confidence) == best_rank) {
                best.push_back(candidate);
            }
        }

        std::unordered_set<int> distinct_roles;
        for (const auto& candidate : best) {
            distinct_roles.insert(static_cast<int>(candidate.role));
        }

        if (distinct_roles.size() != 1) {
            // Conflicting same-strength semantic evidence is intentionally
            // unresolved. Circuit-role inference must never choose a winner
            // merely because of ordering.
            net.role = DistributionRole::Unknown;
            net.confidence = ConfidenceClass::Unresolved;
            result.unresolved_net_ids.push_back(net.id);
            continue;
        }

        net.role = best.front().role;
        net.confidence = best.front().confidence;
        if (net.anchor_endpoint.empty()) {
            net.anchor_endpoint = best.front().endpoint_id;
        }
    }

    std::sort(
        result.unresolved_net_ids.begin(),
        result.unresolved_net_ids.end());

    std::sort(
        result.nets.begin(),
        result.nets.end(),
        [](const ElectricalNet& a, const ElectricalNet& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire
