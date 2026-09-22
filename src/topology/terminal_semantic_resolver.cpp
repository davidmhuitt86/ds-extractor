#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <algorithm>
#include <unordered_map>

namespace eke::dx::wire {

std::vector<EndpointCandidate> TerminalSemanticResolver::resolve(
    const std::vector<EndpointCandidate>& candidates,
    const std::vector<TerminalSemanticEvidence>& evidence) const {

    std::unordered_map<std::string, const TerminalSemanticEvidence*> by_endpoint;
    for (const auto& item : evidence) {
        if (item.endpoint_id.empty()) {
            continue;
        }

        // Deterministic first-wins policy. Duplicate semantic evidence is
        // itself a conflict and must be resolved upstream rather than
        // silently replacing a previous interpretation.
        by_endpoint.emplace(item.endpoint_id, &item);
    }

    std::vector<EndpointCandidate> result = candidates;

    for (auto& candidate : result) {
        const auto it = by_endpoint.find(candidate.id);
        if (it == by_endpoint.end()) {
            continue;
        }

        const auto& semantic = *it->second;

        // Never downgrade an existing higher-confidence semantic identity
        // with weaker evidence.
        if (candidate.confidence != ConfidenceClass::High ||
            semantic.confidence == ConfidenceClass::High) {
            if (semantic.endpoint_kind != EndpointKind::Unresolved) {
                candidate.kind = semantic.endpoint_kind;
            }
            if (semantic.role != TerminalRole::Unknown) {
                candidate.terminal_role = semantic.role;
            }
            if (!semantic.component_id.empty()) {
                candidate.component_id = semantic.component_id;
            }
            if (!semantic.terminal_name.empty()) {
                candidate.terminal_name = semantic.terminal_name;
            }
            if (!semantic.function_label.empty()) {
                candidate.function_label = semantic.function_label;
            }
            if (!semantic.wire_color.empty()) {
                candidate.wire_color = semantic.wire_color;
            }

            if (semantic.confidence != ConfidenceClass::Unresolved) {
                candidate.confidence = semantic.confidence;
            }
        }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const EndpointCandidate& a, const EndpointCandidate& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire
