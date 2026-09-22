#include "eke_dx_wire/topology/terminal_semantic_evidence_builder.hpp"
#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <algorithm>
#include <utility>

namespace eke::dx::wire {

namespace {

EndpointKind endpoint_kind(TerminalCandidateKind kind) {
    switch (kind) {
    case TerminalCandidateKind::ComponentBoundary:
        return EndpointKind::ComponentTerminal;
    case TerminalCandidateKind::ConnectorBoundary:
        return EndpointKind::ConnectorTerminal;
    case TerminalCandidateKind::GroundConnection:
        return EndpointKind::Ground;
    case TerminalCandidateKind::Unknown:
        return EndpointKind::Unresolved;
    }
    return EndpointKind::Unresolved;
}

TerminalRole terminal_role(TerminalCandidateKind kind) {
    switch (kind) {
    case TerminalCandidateKind::ComponentBoundary:
        return TerminalRole::ComponentTerminal;
    case TerminalCandidateKind::ConnectorBoundary:
        return TerminalRole::ConnectorTerminal;
    case TerminalCandidateKind::GroundConnection:
        return TerminalRole::GroundTerminal;
    case TerminalCandidateKind::Unknown:
        return TerminalRole::Unknown;
    }
    return TerminalRole::Unknown;
}

} // namespace

std::vector<TerminalSemanticEvidence>
TerminalSemanticEvidenceBuilder::build(
    const std::vector<TerminalCandidate>& terminal_candidates) const {

    std::vector<TerminalSemanticEvidence> result;
    result.reserve(terminal_candidates.size());

    for (const auto& candidate : terminal_candidates) {
        if (candidate.endpoint_id.empty() ||
            candidate.component_candidate_id.empty()) {
            continue;
        }

        TerminalSemanticEvidence evidence;
        evidence.endpoint_id = candidate.endpoint_id;
        evidence.endpoint_kind = endpoint_kind(candidate.kind);
        evidence.role = terminal_role(candidate.kind);
        evidence.confidence = candidate.confidence;
        evidence.component_id = candidate.component_candidate_id;
        result.push_back(std::move(evidence));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const TerminalSemanticEvidence& a,
           const TerminalSemanticEvidence& b) {
            if (a.endpoint_id != b.endpoint_id) {
                return a.endpoint_id < b.endpoint_id;
            }
            return a.component_id < b.component_id;
        });

    return result;
}

} // namespace eke::dx::wire
