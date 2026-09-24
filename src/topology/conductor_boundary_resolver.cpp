#include "eke_dx_wire/topology/conductor_boundary_resolver.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace eke::dx::wire {
namespace {

EndpointKind boundary_for(TerminalCandidateKind kind) {
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

int confidence_rank(ConfidenceClass confidence) {
    switch (confidence) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

std::vector<std::string> distinct_sorted(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

} // namespace

ConductorBoundaryResolutionArtifacts ConductorBoundaryResolver::resolve(
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<TerminalCandidate>& terminal_candidates,
    const std::vector<EndpointSemanticReconstruction>&
        endpoint_semantic_reconstructions,
    const std::vector<ConnectorTerminal>& connector_terminals) const {

    ConductorBoundaryResolutionArtifacts result;

    std::map<std::string, std::vector<const TerminalCandidate*>>
        terminal_candidates_by_endpoint;
    for (const auto& candidate : terminal_candidates) {
        if (!candidate.endpoint_id.empty())
            terminal_candidates_by_endpoint[candidate.endpoint_id]
                .push_back(&candidate);
    }

    std::map<std::string, const EndpointSemanticReconstruction*>
        reconstruction_by_endpoint;
    for (const auto& reconstruction : endpoint_semantic_reconstructions) {
        if (!reconstruction.endpoint_id.empty())
            reconstruction_by_endpoint.emplace(
                reconstruction.endpoint_id, &reconstruction);
    }

    std::map<std::string, std::vector<const ConnectorTerminal*>>
        connector_terminals_by_endpoint;
    for (const auto& terminal : connector_terminals) {
        if (!terminal.endpoint_id.empty())
            connector_terminals_by_endpoint[terminal.endpoint_id]
                .push_back(&terminal);
    }

    for (const auto& endpoint : endpoints) {
        ConductorBoundaryResolution resolution;
        resolution.id = stable_id(
            "conductor-boundary-resolution",
            endpoint.id + ":conductor-boundary-resolution");
        resolution.endpoint_id = endpoint.id;

        const auto tc_it = terminal_candidates_by_endpoint.find(endpoint.id);
        const std::vector<const TerminalCandidate*> empty_tcs;
        const auto& tcs =
            tc_it != terminal_candidates_by_endpoint.end() ? tc_it->second
                                                             : empty_tcs;

        std::vector<std::string> component_boundary_ids;
        std::vector<std::string> ground_component_ids;
        std::vector<std::string> connector_boundary_ids;
        ConfidenceClass best_confidence = ConfidenceClass::Unresolved;

        for (const auto* candidate : tcs) {
            const EndpointKind suggested = boundary_for(candidate->kind);
            if (suggested == EndpointKind::Unresolved)
                continue; // TerminalCandidateKind::Unknown - no boundary evidence

            ConductorBoundaryEvidence evidence;
            evidence.id = stable_id(
                "conductor-boundary-evidence",
                candidate->id + ":from-terminal-candidate");
            evidence.endpoint_id = endpoint.id;
            evidence.kind =
                ConductorBoundaryEvidenceKind::TerminalCandidateEvidence;
            evidence.source_object_id = candidate->id;
            evidence.suggested_boundary = suggested;
            evidence.component_id = candidate->component_candidate_id;
            evidence.confidence = candidate->confidence;
            result.evidence.push_back(evidence);
            resolution.evidence_ids.push_back(evidence.id);

            if (confidence_rank(candidate->confidence) >
                confidence_rank(best_confidence)) {
                best_confidence = candidate->confidence;
            }

            switch (candidate->kind) {
            case TerminalCandidateKind::ComponentBoundary:
                if (!candidate->component_candidate_id.empty())
                    component_boundary_ids.push_back(
                        candidate->component_candidate_id);
                break;
            case TerminalCandidateKind::GroundConnection:
                if (!candidate->component_candidate_id.empty())
                    ground_component_ids.push_back(
                        candidate->component_candidate_id);
                break;
            case TerminalCandidateKind::ConnectorBoundary:
                if (!candidate->component_candidate_id.empty())
                    connector_boundary_ids.push_back(
                        candidate->component_candidate_id);
                break;
            case TerminalCandidateKind::Unknown:
                break;
            }
        }

        // ---- component association (independent of terminal identity) --
        const auto distinct_components = distinct_sorted(component_boundary_ids);
        ConductorBoundaryStatus component_status =
            ConductorBoundaryStatus::Unresolved;
        if (distinct_components.size() == 1) {
            component_status = ConductorBoundaryStatus::Resolved;
            resolution.component_id = distinct_components.front();
        } else if (distinct_components.size() > 1) {
            component_status = ConductorBoundaryStatus::Conflicted;
            resolution.conflicting_component_ids = distinct_components;
        }
        resolution.component_status = component_status;

        // ---- ground boundary candidate --------------------------------
        const auto distinct_ground = distinct_sorted(ground_component_ids);
        ConductorBoundaryStatus ground_candidate_status =
            ConductorBoundaryStatus::Unresolved;
        if (distinct_ground.size() == 1) {
            ground_candidate_status = ConductorBoundaryStatus::Resolved;
        } else if (distinct_ground.size() > 1) {
            ground_candidate_status = ConductorBoundaryStatus::Conflicted;
        }

        // ---- connector association / connector-terminal identity ------
        // A resolved ConnectorTerminal (AP-WIRE-020) is authoritative
        // evidence and is adopted directly - never re-derived here.
        const auto ct_it = connector_terminals_by_endpoint.find(endpoint.id);
        ConductorBoundaryStatus connector_status =
            ConductorBoundaryStatus::Unresolved;
        if (ct_it != connector_terminals_by_endpoint.end() &&
            !ct_it->second.empty()) {
            std::vector<std::string> connector_ids;
            for (const auto* terminal : ct_it->second) {
                ConductorBoundaryEvidence evidence;
                evidence.id = stable_id(
                    "conductor-boundary-evidence",
                    terminal->id + ":from-connector-terminal");
                evidence.endpoint_id = endpoint.id;
                evidence.kind =
                    ConductorBoundaryEvidenceKind::ConnectorTerminalEvidence;
                evidence.source_object_id = terminal->id;
                evidence.suggested_boundary = EndpointKind::ConnectorTerminal;
                evidence.connector_id = terminal->connector_id;
                evidence.terminal_identifier =
                    terminal->status == ConnectorTerminalStatus::Resolved
                        ? terminal->terminal_name
                        : std::string {};
                evidence.confidence = terminal->confidence;
                result.evidence.push_back(evidence);
                resolution.evidence_ids.push_back(evidence.id);

                if (!terminal->connector_id.empty())
                    connector_ids.push_back(terminal->connector_id);
            }

            const auto distinct_connector_ids = distinct_sorted(connector_ids);
            if (distinct_connector_ids.size() == 1) {
                resolution.connector_id = distinct_connector_ids.front();
                connector_status = ConductorBoundaryStatus::Resolved;

                std::vector<std::string> resolved_names;
                bool any_conflicted = false;
                for (const auto* terminal : ct_it->second) {
                    if (terminal->status == ConnectorTerminalStatus::Conflicted)
                        any_conflicted = true;
                    if (terminal->status == ConnectorTerminalStatus::Resolved &&
                        !terminal->terminal_name.empty()) {
                        resolved_names.push_back(terminal->terminal_name);
                    }
                }
                const auto distinct_names = distinct_sorted(resolved_names);
                if (any_conflicted || distinct_names.size() > 1) {
                    resolution.connector_terminal_status =
                        ConductorBoundaryStatus::Conflicted;
                    resolution.conflicting_terminal_identifiers = distinct_names;
                } else if (distinct_names.size() == 1) {
                    resolution.connector_terminal_identifier =
                        distinct_names.front();
                    resolution.connector_terminal_status =
                        ConductorBoundaryStatus::Resolved;
                } else {
                    resolution.connector_terminal_status =
                        ConductorBoundaryStatus::Unresolved;
                }
            } else {
                // Multiple distinct connectors claim the same endpoint -
                // a genuine conflict; never pick one.
                connector_status = ConductorBoundaryStatus::Conflicted;
            }
        } else if (!connector_boundary_ids.empty()) {
            // Connector-shaped geometric evidence exists, but no
            // ConnectorTerminal was materialized for it - the connector
            // boundary remains unresolved; nothing is invented.
            connector_status = ConductorBoundaryStatus::Unresolved;
        }
        resolution.connector_status = connector_status;

        // ---- external connection (only ever set by an upstream stage
        // directly on the endpoint today; no current producer) ----------
        ConductorBoundaryStatus external_status =
            ConductorBoundaryStatus::Unresolved;
        if (endpoint.kind == EndpointKind::ExternalConnection) {
            ConductorBoundaryEvidence evidence;
            evidence.id = stable_id(
                "conductor-boundary-evidence",
                endpoint.id + ":from-endpoint-external-connection");
            evidence.endpoint_id = endpoint.id;
            evidence.kind =
                ConductorBoundaryEvidenceKind::ExternalConnectionEvidence;
            evidence.source_object_id = endpoint.id;
            evidence.suggested_boundary = EndpointKind::ExternalConnection;
            evidence.confidence = endpoint.confidence;
            result.evidence.push_back(evidence);
            resolution.evidence_ids.push_back(evidence.id);
            external_status = ConductorBoundaryStatus::Resolved;
        }
        resolution.external_status = external_status;

        // ---- EndpointSemanticReconstruction: corroborating evidence
        // only, never authoritative over the per-category facts above --
        const auto reconstruction_it = reconstruction_by_endpoint.find(endpoint.id);
        if (reconstruction_it != reconstruction_by_endpoint.end()) {
            const auto* reconstruction = reconstruction_it->second;
            ConductorBoundaryEvidence evidence;
            evidence.id = stable_id(
                "conductor-boundary-evidence",
                reconstruction->id + ":from-endpoint-semantic-reconstruction");
            evidence.endpoint_id = endpoint.id;
            evidence.kind = ConductorBoundaryEvidenceKind::
                EndpointSemanticReconstructionEvidence;
            evidence.source_object_id = reconstruction->id;
            evidence.suggested_boundary = reconstruction->endpoint_kind;
            evidence.component_id = reconstruction->component_id;
            evidence.confidence = reconstruction->confidence;
            result.evidence.push_back(evidence);
            resolution.evidence_ids.push_back(evidence.id);
        }

        // ---- overall boundary classification (Sec 9's six categories) -
        std::vector<EndpointKind> contributing_categories;
        if (component_status != ConductorBoundaryStatus::Unresolved)
            contributing_categories.push_back(EndpointKind::ComponentTerminal);
        if (connector_status != ConductorBoundaryStatus::Unresolved)
            contributing_categories.push_back(EndpointKind::ConnectorTerminal);
        if (ground_candidate_status != ConductorBoundaryStatus::Unresolved)
            contributing_categories.push_back(EndpointKind::Ground);
        if (external_status != ConductorBoundaryStatus::Unresolved)
            contributing_categories.push_back(EndpointKind::ExternalConnection);

        if (contributing_categories.empty()) {
            resolution.boundary_kind = EndpointKind::GeometricConductorEnd;
            resolution.boundary_status = ConductorBoundaryStatus::Unresolved;
            resolution.ground_status = ground_candidate_status;
        } else if (contributing_categories.size() == 1) {
            resolution.boundary_kind = contributing_categories.front();
            switch (resolution.boundary_kind) {
            case EndpointKind::ComponentTerminal:
                resolution.boundary_status = component_status;
                break;
            case EndpointKind::ConnectorTerminal:
                resolution.boundary_status = connector_status;
                break;
            case EndpointKind::Ground:
                resolution.boundary_status = ground_candidate_status;
                break;
            case EndpointKind::ExternalConnection:
                resolution.boundary_status = external_status;
                break;
            default:
                resolution.boundary_status = ConductorBoundaryStatus::Unresolved;
                break;
            }
            resolution.ground_status = ground_candidate_status;
            if (resolution.boundary_status == ConductorBoundaryStatus::Resolved)
                resolution.boundary_confidence = best_confidence;
        } else {
            // Multiple distinct boundary categories both have evidence for
            // the same endpoint (e.g. component vs. ground) - a genuine
            // conflict per AP-WIRE-030's decision matrix; never pick one.
            resolution.boundary_kind = EndpointKind::GeometricConductorEnd;
            resolution.boundary_status = ConductorBoundaryStatus::Conflicted;
            resolution.ground_status = ground_candidate_status;
        }

        // ---- terminal identity: only from evidence that independently
        // carries a specific identifier - never invented from a resolved
        // component/boundary alone (Sec 10, 15, 17) -----------------------
        if (resolution.boundary_kind == EndpointKind::ComponentTerminal &&
            resolution.component_status == ConductorBoundaryStatus::Resolved &&
            !endpoint.terminal_name.empty()) {
            resolution.terminal_identifier = endpoint.terminal_name;
            resolution.terminal_status = ConductorBoundaryStatus::Resolved;
        } else {
            resolution.terminal_status = ConductorBoundaryStatus::Unresolved;
        }

        result.resolutions.push_back(std::move(resolution));
    }

    std::sort(
        result.evidence.begin(),
        result.evidence.end(),
        [](const ConductorBoundaryEvidence& a,
           const ConductorBoundaryEvidence& b) { return a.id < b.id; });
    result.evidence.erase(
        std::unique(
            result.evidence.begin(),
            result.evidence.end(),
            [](const ConductorBoundaryEvidence& a,
               const ConductorBoundaryEvidence& b) { return a.id == b.id; }),
        result.evidence.end());

    std::sort(
        result.resolutions.begin(),
        result.resolutions.end(),
        [](const ConductorBoundaryResolution& a,
           const ConductorBoundaryResolution& b) { return a.id < b.id; });

    result.coverage = build_conductor_boundary_coverage(result.resolutions);
    return result;
}

ConductorBoundaryCoverage build_conductor_boundary_coverage(
    const std::vector<ConductorBoundaryResolution>& resolutions) {

    ConductorBoundaryCoverage coverage;
    coverage.total = resolutions.size();

    for (const auto& resolution : resolutions) {
        switch (resolution.boundary_status) {
        case ConductorBoundaryStatus::Resolved: ++coverage.boundary_resolved; break;
        case ConductorBoundaryStatus::Unresolved: ++coverage.boundary_unresolved; break;
        case ConductorBoundaryStatus::Conflicted: ++coverage.boundary_conflicted; break;
        }
        switch (resolution.component_status) {
        case ConductorBoundaryStatus::Resolved: ++coverage.component_resolved; break;
        case ConductorBoundaryStatus::Unresolved: ++coverage.component_unresolved; break;
        case ConductorBoundaryStatus::Conflicted: ++coverage.component_conflicted; break;
        }
        switch (resolution.terminal_status) {
        case ConductorBoundaryStatus::Resolved: ++coverage.terminal_resolved; break;
        case ConductorBoundaryStatus::Unresolved: ++coverage.terminal_unresolved; break;
        case ConductorBoundaryStatus::Conflicted: ++coverage.terminal_conflicted; break;
        }
        switch (resolution.connector_status) {
        case ConductorBoundaryStatus::Resolved: ++coverage.connector_resolved; break;
        case ConductorBoundaryStatus::Unresolved: ++coverage.connector_unresolved; break;
        case ConductorBoundaryStatus::Conflicted: ++coverage.connector_conflicted; break;
        }
        switch (resolution.connector_terminal_status) {
        case ConductorBoundaryStatus::Resolved: ++coverage.connector_terminal_resolved; break;
        case ConductorBoundaryStatus::Unresolved: ++coverage.connector_terminal_unresolved; break;
        case ConductorBoundaryStatus::Conflicted: ++coverage.connector_terminal_conflicted; break;
        }
        if (resolution.ground_status == ConductorBoundaryStatus::Resolved)
            ++coverage.ground_resolved;
        if (resolution.external_status == ConductorBoundaryStatus::Resolved)
            ++coverage.external_resolved;
    }

    return coverage;
}

} // namespace eke::dx::wire
