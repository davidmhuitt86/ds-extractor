#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>

namespace eke::dx::wire {
namespace {

std::string normalize_label(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    bool pending_space = false;
    for (const unsigned char ch : value) {
        if (std::isspace(ch) || ch == '-' || ch == '_') {
            pending_space = !result.empty();
            continue;
        }

        if (pending_space) {
            result.push_back(' ');
            pending_space = false;
        }

        result.push_back(
            static_cast<char>(std::toupper(ch)));
    }

    return result;
}

DistributionRole explicit_label_role(const std::string& value) {
    const auto label = normalize_label(value);

    if (label == "GROUND" ||
        label == "GND") {
        return DistributionRole::Ground;
    }

    if (label == "POWER FEED" ||
        label == "POWER SOURCE" ||
        label == "BATTERY+" ||
        label == "BATTERY POSITIVE" ||
        label == "B+") {
        return DistributionRole::PowerFeed;
    }

    if (label == "SHARED FUNCTION FEED") {
        return DistributionRole::SharedFunctionFeed;
    }

    return DistributionRole::Unknown;
}

void collect_label_role(
    const std::string& label,
    std::set<DistributionRole>& roles) {

    const auto role = explicit_label_role(label);
    if (role != DistributionRole::Unknown) {
        roles.insert(role);
    }
}

} // namespace

std::vector<CircuitRoleEvidence>
CircuitRoleEvidenceBuilder::build(
    const std::vector<EndpointCandidate>& endpoints) const {

    std::vector<CircuitRoleEvidence> result;

    for (const auto& endpoint : endpoints) {
        DistributionRole intrinsic = DistributionRole::Unknown;

        if (endpoint.kind == EndpointKind::Ground ||
            endpoint.terminal_role == TerminalRole::GroundTerminal) {
            intrinsic = DistributionRole::Ground;
        } else if (endpoint.terminal_role == TerminalRole::PowerSource) {
            intrinsic = DistributionRole::PowerFeed;
        }

        if (intrinsic != DistributionRole::Unknown &&
            endpoint.confidence != ConfidenceClass::Unresolved) {
            result.push_back({
                endpoint.id,
                intrinsic,
                endpoint.confidence,
                "endpoint-semantic"
            });
        }

        if (endpoint.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        // AP-WIRE-005: only explicit semantic labels may add a new role.
        // Do not interpret arbitrary text, wire colors, topology shape, or
        // component class as circuit-role evidence at this boundary.
        std::set<DistributionRole> label_roles;
        collect_label_role(endpoint.terminal_name, label_roles);
        collect_label_role(endpoint.function_label, label_roles);

        // Conflicting explicit labels are intentionally left unresolved.
        // The resolver must never choose a role merely because of ordering.
        if (label_roles.size() != 1) {
            continue;
        }

        const auto role = *label_roles.begin();

        // An already-established intrinsic role has priority over a weaker
        // label annotation. A conflicting label is not emitted as evidence.
        if (intrinsic != DistributionRole::Unknown &&
            intrinsic != role) {
            continue;
        }

        result.push_back({
            endpoint.id,
            role,
            endpoint.confidence,
            "endpoint-label"
        });
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const CircuitRoleEvidence& a, const CircuitRoleEvidence& b) {
            if (a.endpoint_id != b.endpoint_id) {
                return a.endpoint_id < b.endpoint_id;
            }
            if (a.role != b.role) {
                return static_cast<int>(a.role) < static_cast<int>(b.role);
            }
            return a.source < b.source;
        });

    return result;
}

} // namespace eke::dx::wire
