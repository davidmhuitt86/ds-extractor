#include "eke_dx_wire/topology/wire_semantic_resolver.hpp"
#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

const EndpointCandidate* find_endpoint(
    const std::unordered_map<std::string, const EndpointCandidate*>& by_id,
    const std::string& endpoint_id) {
    const auto it = by_id.find(endpoint_id);
    return it == by_id.end() ? nullptr : it->second;
}

const EndpointSemanticReconstruction* find_reconstruction(
    const std::unordered_map<std::string, const EndpointSemanticReconstruction*>& by_id,
    const std::string& endpoint_id) {
    const auto it = by_id.find(endpoint_id);
    return it == by_id.end() ? nullptr : it->second;
}

WireSemanticStatus component_status_for(
    const EndpointSemanticReconstruction* reconstruction) {
    if (!reconstruction) {
        return WireSemanticStatus::Unresolved;
    }
    switch (reconstruction->status) {
    case EndpointSemanticReconstructionStatus::Resolved:
        return reconstruction->component_id.empty()
            ? WireSemanticStatus::Unresolved
            : WireSemanticStatus::Resolved;
    case EndpointSemanticReconstructionStatus::Conflicted:
        return WireSemanticStatus::Conflicted;
    case EndpointSemanticReconstructionStatus::Unresolved:
        return WireSemanticStatus::Unresolved;
    }
    return WireSemanticStatus::Unresolved;
}

// Deterministic pick of a Resolved ConnectorTerminal for one endpoint: at
// most one connector terminal is expected per endpoint, but if more than
// one exists, the lowest terminal id is chosen (never insertion order).
const ConnectorTerminal* find_resolved_connector_terminal(
    const std::multimap<std::string, const ConnectorTerminal*>& by_endpoint,
    const std::string& endpoint_id) {

    const ConnectorTerminal* best = nullptr;
    auto range = by_endpoint.equal_range(endpoint_id);
    for (auto it = range.first; it != range.second; ++it) {
        const ConnectorTerminal* terminal = it->second;
        if (terminal->status != ConnectorTerminalStatus::Resolved) {
            continue;
        }
        if (!best || terminal->id < best->id) {
            best = terminal;
        }
    }
    return best;
}

// Reinforcement/conflict rule shared by wire_color and function_label:
// two independent per-endpoint text values, at most one authoritative
// result. Equal non-empty values reinforce (High). A single non-empty
// value is weaker (Medium) since only one end of the wire carries
// evidence. Disagreeing non-empty values are Conflicted, never averaged
// or arbitrarily picked.
void resolve_paired_text_field(
    const std::string& start_value,
    const std::string& end_value,
    std::string& out_value,
    WireSemanticStatus& out_status,
    ConfidenceClass& out_confidence) {

    const bool has_start = !start_value.empty();
    const bool has_end = !end_value.empty();

    if (has_start && has_end) {
        if (start_value == end_value) {
            out_value = start_value;
            out_status = WireSemanticStatus::Resolved;
            out_confidence = ConfidenceClass::High;
        } else {
            out_value.clear();
            out_status = WireSemanticStatus::Conflicted;
            out_confidence = ConfidenceClass::Unresolved;
        }
        return;
    }

    if (has_start || has_end) {
        out_value = has_start ? start_value : end_value;
        out_status = WireSemanticStatus::Resolved;
        out_confidence = ConfidenceClass::Medium;
        return;
    }

    out_value.clear();
    out_status = WireSemanticStatus::Unresolved;
    out_confidence = ConfidenceClass::Unresolved;
}

} // namespace

WireSemanticResolutionArtifacts WireSemanticResolver::resolve(
    const std::vector<Wire>& wires,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<EndpointSemanticReconstruction>& reconstructions,
    const std::vector<ConnectorTerminal>& connector_terminals,
    const std::vector<ElectricalNet>& electrical_nets) const {

    WireSemanticResolutionArtifacts result;

    std::unordered_map<std::string, const EndpointCandidate*> endpoint_by_id;
    endpoint_by_id.reserve(endpoints.size());
    for (const auto& endpoint : endpoints) {
        endpoint_by_id.emplace(endpoint.id, &endpoint);
    }

    std::unordered_map<std::string, const EndpointSemanticReconstruction*> reconstruction_by_endpoint;
    reconstruction_by_endpoint.reserve(reconstructions.size());
    for (const auto& reconstruction : reconstructions) {
        reconstruction_by_endpoint.emplace(reconstruction.endpoint_id, &reconstruction);
    }

    std::multimap<std::string, const ConnectorTerminal*> connector_terminal_by_endpoint;
    for (const auto& terminal : connector_terminals) {
        if (!terminal.endpoint_id.empty()) {
            connector_terminal_by_endpoint.emplace(terminal.endpoint_id, &terminal);
        }
    }

    // endpoint_id -> sorted, de-duplicated net ids that endpoint belongs
    // to. Deterministic regardless of electrical_nets iteration order.
    std::map<std::string, std::vector<std::string>> nets_by_endpoint;
    for (const auto& net : electrical_nets) {
        for (const auto& endpoint_id : net.endpoint_ids) {
            nets_by_endpoint[endpoint_id].push_back(net.id);
        }
    }
    for (auto& [endpoint_id, net_ids] : nets_by_endpoint) {
        std::sort(net_ids.begin(), net_ids.end());
        net_ids.erase(std::unique(net_ids.begin(), net_ids.end()), net_ids.end());
    }

    result.resolutions.reserve(wires.size());

    for (const auto& wire : wires) {
        WireSemanticResolution resolution;
        resolution.id = stable_id("wire-semantic-resolution", wire.id);
        resolution.wire_id = wire.id;

        const EndpointCandidate* start = find_endpoint(endpoint_by_id, wire.start_endpoint);
        const EndpointCandidate* end = find_endpoint(endpoint_by_id, wire.end_endpoint);

        // Wire color / function: explicit per-endpoint text evidence only.
        resolve_paired_text_field(
            start ? start->wire_color : std::string{},
            end ? end->wire_color : std::string{},
            resolution.wire_color,
            resolution.wire_color_status,
            resolution.wire_color_confidence);

        resolve_paired_text_field(
            start ? start->function_label : std::string{},
            end ? end->function_label : std::string{},
            resolution.function_label,
            resolution.function_status,
            resolution.function_confidence);

        // Component/terminal association: AP-WIRE-024 conflict constraint
        // - a Conflicted EndpointSemanticReconstruction is reported
        // Conflicted here, never treated as authoritative.
        const auto* start_reconstruction =
            find_reconstruction(reconstruction_by_endpoint, wire.start_endpoint);
        const auto* end_reconstruction =
            find_reconstruction(reconstruction_by_endpoint, wire.end_endpoint);

        resolution.start_component_status = component_status_for(start_reconstruction);
        if (resolution.start_component_status == WireSemanticStatus::Resolved) {
            resolution.start_component_id = start_reconstruction->component_id;
            resolution.start_terminal_name = start ? start->terminal_name : std::string{};
        }

        resolution.end_component_status = component_status_for(end_reconstruction);
        if (resolution.end_component_status == WireSemanticStatus::Resolved) {
            resolution.end_component_id = end_reconstruction->component_id;
            resolution.end_terminal_name = end ? end->terminal_name : std::string{};
        }

        // Connector-terminal association: only a Resolved ConnectorTerminal
        // is authoritative.
        const auto* start_connector = find_resolved_connector_terminal(
            connector_terminal_by_endpoint, wire.start_endpoint);
        if (start_connector) {
            resolution.start_connector_id = start_connector->connector_id;
            resolution.start_connector_terminal_name = start_connector->terminal_name;
            resolution.start_connector_status = WireSemanticStatus::Resolved;
        }

        const auto* end_connector = find_resolved_connector_terminal(
            connector_terminal_by_endpoint, wire.end_endpoint);
        if (end_connector) {
            resolution.end_connector_id = end_connector->connector_id;
            resolution.end_connector_terminal_name = end_connector->terminal_name;
            resolution.end_connector_status = WireSemanticStatus::Resolved;
        }

        // Electrical-net association: both endpoints must agree when both
        // are net-resolved; disagreement is a genuine cross-stage
        // inconsistency and is surfaced as Conflicted, not hidden.
        const auto start_nets_it = nets_by_endpoint.find(wire.start_endpoint);
        const auto end_nets_it = nets_by_endpoint.find(wire.end_endpoint);
        const bool start_has_net =
            start_nets_it != nets_by_endpoint.end() && start_nets_it->second.size() == 1;
        const bool end_has_net =
            end_nets_it != nets_by_endpoint.end() && end_nets_it->second.size() == 1;

        if (start_has_net && end_has_net) {
            if (start_nets_it->second.front() == end_nets_it->second.front()) {
                resolution.electrical_net_id = start_nets_it->second.front();
                resolution.electrical_net_status = WireSemanticStatus::Resolved;
                resolution.electrical_net_confidence = ConfidenceClass::High;
            } else {
                resolution.electrical_net_status = WireSemanticStatus::Conflicted;
                resolution.electrical_net_confidence = ConfidenceClass::Unresolved;
            }
        } else if (start_has_net || end_has_net) {
            resolution.electrical_net_id = start_has_net
                ? start_nets_it->second.front()
                : end_nets_it->second.front();
            resolution.electrical_net_status = WireSemanticStatus::Resolved;
            resolution.electrical_net_confidence = ConfidenceClass::Medium;
        } else {
            resolution.electrical_net_status = WireSemanticStatus::Unresolved;
            resolution.electrical_net_confidence = ConfidenceClass::Unresolved;
        }

        result.resolutions.push_back(std::move(resolution));
    }

    std::sort(
        result.resolutions.begin(),
        result.resolutions.end(),
        [](const WireSemanticResolution& a, const WireSemanticResolution& b) {
            return a.wire_id < b.wire_id;
        });

    result.coverage = build_wire_semantic_coverage(result.resolutions);

    return result;
}

WireSemanticCoverage build_wire_semantic_coverage(
    const std::vector<WireSemanticResolution>& resolutions) {

    WireSemanticCoverage coverage;
    coverage.total = resolutions.size();

    for (const auto& resolution : resolutions) {
        switch (resolution.wire_color_status) {
        case WireSemanticStatus::Resolved: ++coverage.wire_color_resolved; break;
        case WireSemanticStatus::Conflicted: ++coverage.wire_color_conflicted; break;
        case WireSemanticStatus::Unresolved: ++coverage.wire_color_unresolved; break;
        }

        switch (resolution.function_status) {
        case WireSemanticStatus::Resolved: ++coverage.function_resolved; break;
        case WireSemanticStatus::Conflicted: ++coverage.function_conflicted; break;
        case WireSemanticStatus::Unresolved: ++coverage.function_unresolved; break;
        }

        const bool component_resolved =
            resolution.start_component_status == WireSemanticStatus::Resolved ||
            resolution.end_component_status == WireSemanticStatus::Resolved;
        const bool component_conflicted =
            resolution.start_component_status == WireSemanticStatus::Conflicted ||
            resolution.end_component_status == WireSemanticStatus::Conflicted;
        if (component_resolved) {
            ++coverage.component_association_resolved;
        } else if (component_conflicted) {
            ++coverage.component_association_conflicted;
        } else {
            ++coverage.component_association_unresolved;
        }

        if (resolution.start_connector_status == WireSemanticStatus::Resolved ||
            resolution.end_connector_status == WireSemanticStatus::Resolved) {
            ++coverage.connector_association_resolved;
        }

        switch (resolution.electrical_net_status) {
        case WireSemanticStatus::Resolved: ++coverage.electrical_net_resolved; break;
        case WireSemanticStatus::Conflicted: ++coverage.electrical_net_conflicted; break;
        case WireSemanticStatus::Unresolved: ++coverage.electrical_net_unresolved; break;
        }

        const bool any_resolved =
            resolution.wire_color_status == WireSemanticStatus::Resolved ||
            resolution.function_status == WireSemanticStatus::Resolved ||
            component_resolved ||
            resolution.start_connector_status == WireSemanticStatus::Resolved ||
            resolution.end_connector_status == WireSemanticStatus::Resolved ||
            resolution.electrical_net_status == WireSemanticStatus::Resolved;
        if (!any_resolved) {
            ++coverage.fully_unresolved;
        }
    }

    return coverage;
}

} // namespace eke::dx::wire
