#include "eke_dx_wire/topology/conductor_boundary_resolver.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

EndpointCandidate make_endpoint(
    const std::string& id, EndpointKind kind = EndpointKind::Unresolved) {
    EndpointCandidate e;
    e.id = id;
    e.kind = kind;
    return e;
}

TerminalCandidate make_terminal_candidate(
    const std::string& id,
    const std::string& endpoint_id,
    const std::string& component_id,
    TerminalCandidateKind kind,
    ConfidenceClass confidence = ConfidenceClass::High) {
    TerminalCandidate tc;
    tc.id = id;
    tc.endpoint_id = endpoint_id;
    tc.component_candidate_id = component_id;
    tc.kind = kind;
    tc.confidence = confidence;
    return tc;
}

ConnectorTerminal make_connector_terminal(
    const std::string& id,
    const std::string& endpoint_id,
    const std::string& connector_id,
    const std::string& terminal_name,
    ConnectorTerminalStatus status) {
    ConnectorTerminal ct;
    ct.id = id;
    ct.endpoint_id = endpoint_id;
    ct.connector_id = connector_id;
    ct.terminal_name = terminal_name;
    ct.status = status;
    ct.confidence = ConfidenceClass::High;
    return ct;
}

const ConductorBoundaryResolution* find_resolution(
    const std::vector<ConductorBoundaryResolution>& resolutions,
    const std::string& endpoint_id) {
    for (const auto& r : resolutions) {
        if (r.endpoint_id == endpoint_id) return &r;
    }
    return nullptr;
}

} // namespace

int main() {
    ConductorBoundaryResolver resolver;

    // A. Geometric conductor end remains geometric without evidence.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-a")};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-a");
        assert(r != nullptr);
        assert(r->boundary_kind == EndpointKind::GeometricConductorEnd);
        assert(r->boundary_status == ConductorBoundaryStatus::Unresolved);
        assert(r->component_status == ConductorBoundaryStatus::Unresolved);
        assert(r->terminal_status == ConductorBoundaryStatus::Unresolved);
    }

    // B. Explicit component terminal resolves.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-b")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-b", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-b");
        assert(r->boundary_kind == EndpointKind::ComponentTerminal);
        assert(r->boundary_status == ConductorBoundaryStatus::Resolved);
        assert(r->component_status == ConductorBoundaryStatus::Resolved);
        assert(r->component_id == "comp-1");
        assert(!r->evidence_ids.empty());
    }

    // C. Component association can resolve while terminal identity
    // remains unresolved (no terminal_name evidence was ever supplied).
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-c")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-c", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-c");
        assert(r->component_status == ConductorBoundaryStatus::Resolved);
        assert(r->terminal_status == ConductorBoundaryStatus::Unresolved);
        assert(r->terminal_identifier.empty());
    }

    // D. Explicit connector terminal resolves.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-d")};
        std::vector<ConnectorTerminal> cts = {
            make_connector_terminal(
                "ct-1", "ep-d", "conn-1", "A1",
                ConnectorTerminalStatus::Resolved)};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, cts);
        const auto* r = find_resolution(artifacts.resolutions, "ep-d");
        assert(r->boundary_kind == EndpointKind::ConnectorTerminal);
        assert(r->boundary_status == ConductorBoundaryStatus::Resolved);
        assert(r->connector_status == ConductorBoundaryStatus::Resolved);
        assert(r->connector_id == "conn-1");
        assert(r->connector_terminal_status == ConductorBoundaryStatus::Resolved);
        assert(r->connector_terminal_identifier == "A1");
    }

    // E. Connector identity can resolve while pin identity remains
    // unresolved.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-e")};
        std::vector<ConnectorTerminal> cts = {
            make_connector_terminal(
                "ct-1", "ep-e", "conn-1", "",
                ConnectorTerminalStatus::Unresolved)};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, cts);
        const auto* r = find_resolution(artifacts.resolutions, "ep-e");
        assert(r->connector_status == ConductorBoundaryStatus::Resolved);
        assert(r->connector_id == "conn-1");
        assert(r->connector_terminal_status == ConductorBoundaryStatus::Unresolved);
        assert(r->connector_terminal_identifier.empty());
    }

    // F. Explicit ground evidence resolves GroundTerminal.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-f")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-f", "comp-ground",
                TerminalCandidateKind::GroundConnection)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-f");
        assert(r->boundary_kind == EndpointKind::Ground);
        assert(r->boundary_status == ConductorBoundaryStatus::Resolved);
        assert(r->ground_status == ConductorBoundaryStatus::Resolved);
    }

    // G. A chassis-ground symbol/component elsewhere in the model must
    // not leak into an unrelated endpoint's ground status - this
    // resolver only ever consults evidence keyed to the specific
    // endpoint_id being resolved, never component-kind alone.
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-g1"), make_endpoint("ep-g2")};
        std::vector<TerminalCandidate> tcs = {
            // ep-g1 has real ground evidence.
            make_terminal_candidate(
                "tc-1", "ep-g1", "comp-ground",
                TerminalCandidateKind::GroundConnection),
            // ep-g2 has unrelated component evidence only - no ground
            // evidence at all, even though a chassis-ground component
            // exists elsewhere in the same model.
            make_terminal_candidate(
                "tc-2", "ep-g2", "comp-other",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r1 = find_resolution(artifacts.resolutions, "ep-g1");
        const auto* r2 = find_resolution(artifacts.resolutions, "ep-g2");
        assert(r1->ground_status == ConductorBoundaryStatus::Resolved);
        assert(r2->ground_status == ConductorBoundaryStatus::Unresolved);
        assert(r2->boundary_kind == EndpointKind::ComponentTerminal);
    }

    // H. Explicit external connection resolves ExternalConnection.
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-h", EndpointKind::ExternalConnection)};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-h");
        assert(r->boundary_kind == EndpointKind::ExternalConnection);
        assert(r->boundary_status == ConductorBoundaryStatus::Resolved);
        assert(r->external_status == ConductorBoundaryStatus::Resolved);
    }

    // I. Splice is never produced as a boundary result, even if the
    // input EndpointCandidate itself carries EndpointKind::Splice (the
    // standing rule from AP-WIRE-029: Splice != Conductor Boundary).
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-i", EndpointKind::Splice)};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-i");
        assert(r->boundary_kind != EndpointKind::Splice);
        assert(r->boundary_kind == EndpointKind::GeometricConductorEnd);
        assert(r->boundary_status == ConductorBoundaryStatus::Unresolved);
    }

    // J. Crossing is never a boundary - structurally guaranteed, not
    // merely by convention: ConductorBoundaryResolver::resolve() takes no
    // TopologyNode/TopologyEdge parameter at all (see the header), so
    // crossing-node topology information is never available to this
    // resolver to consult in the first place. There is also no
    // EndpointKind::Crossing value for an endpoint to carry, unlike
    // Splice. This test exercises the resolver in the total absence of
    // any topology input and confirms it still terminates correctly and
    // produces no boundary from nothing.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-j")};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-j");
        assert(r->boundary_status == ConductorBoundaryStatus::Unresolved);
    }

    // K. Continuation is never a boundary - same structural guarantee as
    // J: no topology-node type, including Continuation, is ever passed
    // to or consulted by this resolver.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-k")};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-k");
        assert(r->boundary_status == ConductorBoundaryStatus::Unresolved);
    }

    // L. A TerminalCandidate whose kind is Unknown (proximity found
    // something, but it wasn't classifiable) contributes no boundary
    // evidence and leaves the endpoint unresolved.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-l")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-l", "comp-1", TerminalCandidateKind::Unknown)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-l");
        assert(r->boundary_kind == EndpointKind::GeometricConductorEnd);
        assert(r->boundary_status == ConductorBoundaryStatus::Unresolved);
        assert(r->component_status == ConductorBoundaryStatus::Unresolved);
    }

    // M. Equal competing component candidates remain conflicted - the
    // resolver never selects the higher-confidence, closer, or
    // first-seen candidate as a winner.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-m")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-m", "comp-a",
                TerminalCandidateKind::ComponentBoundary, ConfidenceClass::High),
            make_terminal_candidate(
                "tc-2", "ep-m", "comp-b",
                TerminalCandidateKind::ComponentBoundary, ConfidenceClass::Low)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-m");
        assert(r->component_status == ConductorBoundaryStatus::Conflicted);
        assert(r->boundary_status == ConductorBoundaryStatus::Conflicted);
        assert(r->component_id.empty());
        assert(r->conflicting_component_ids.size() == 2);
    }

    // N. Equal competing terminal (pin) candidates remain conflicted -
    // tested via two resolved ConnectorTerminals on the same connector
    // disagreeing on the pin name, since that is the only path in the
    // current model that carries an independent specific-terminal
    // identifier at all.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-n")};
        std::vector<ConnectorTerminal> cts = {
            make_connector_terminal(
                "ct-1", "ep-n", "conn-1", "A1",
                ConnectorTerminalStatus::Resolved),
            make_connector_terminal(
                "ct-2", "ep-n", "conn-1", "A2",
                ConnectorTerminalStatus::Resolved)};
        const auto artifacts = resolver.resolve(endpoints, {}, {}, cts);
        const auto* r = find_resolution(artifacts.resolutions, "ep-n");
        assert(r->connector_status == ConductorBoundaryStatus::Resolved);
        assert(r->connector_terminal_status == ConductorBoundaryStatus::Conflicted);
        assert(r->connector_terminal_identifier.empty());
        assert(r->conflicting_terminal_identifiers.size() == 2);
    }

    // P. Electrical-net membership cannot create boundary identity -
    // structurally guaranteed: resolve() takes no ElectricalNet
    // parameter at all, so net membership is never available to
    // consult. Confirmed by compiling and running with exactly the
    // resolver's declared four evidence parameters and nothing else.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-p")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-p", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        assert(find_resolution(artifacts.resolutions, "ep-p") != nullptr);
    }

    // R. Deterministic output across repeated runs on identical input.
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-r1"), make_endpoint("ep-r2")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-r1", "comp-1",
                TerminalCandidateKind::ComponentBoundary),
            make_terminal_candidate(
                "tc-2", "ep-r2", "comp-a",
                TerminalCandidateKind::ComponentBoundary),
            make_terminal_candidate(
                "tc-3", "ep-r2", "comp-b",
                TerminalCandidateKind::ComponentBoundary)};
        const auto first = resolver.resolve(endpoints, tcs, {}, {});
        const auto second = resolver.resolve(endpoints, tcs, {}, {});
        assert(first.resolutions.size() == second.resolutions.size());
        for (std::size_t i = 0; i < first.resolutions.size(); ++i) {
            assert(first.resolutions[i].id == second.resolutions[i].id);
            assert(first.resolutions[i].boundary_kind == second.resolutions[i].boundary_kind);
            assert(first.resolutions[i].boundary_status == second.resolutions[i].boundary_status);
            assert(first.resolutions[i].component_status == second.resolutions[i].component_status);
        }
        assert(first.evidence.size() == second.evidence.size());
        for (std::size_t i = 0; i < first.evidence.size(); ++i) {
            assert(first.evidence[i].id == second.evidence[i].id);
        }
    }

    // S. Provenance remains traceable: every resolved boundary's
    // evidence_ids resolve to real entries in the returned evidence
    // vector.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-s")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-s", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-s");
        assert(!r->evidence_ids.empty());
        for (const auto& evidence_id : r->evidence_ids) {
            const bool found = std::any_of(
                artifacts.evidence.begin(), artifacts.evidence.end(),
                [&](const ConductorBoundaryEvidence& e) {
                    return e.id == evidence_id;
                });
            assert(found);
        }
    }

    // Cross-category conflict: component evidence and ground evidence
    // both present for the same endpoint, pointing at different
    // components - decision matrix case 9 ("conflicting ground/component
    // evidence -> Conflicted").
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-x")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-x", "comp-1",
                TerminalCandidateKind::ComponentBoundary),
            make_terminal_candidate(
                "tc-2", "ep-x", "comp-ground",
                TerminalCandidateKind::GroundConnection)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-x");
        assert(r->boundary_status == ConductorBoundaryStatus::Conflicted);
        assert(r->component_status == ConductorBoundaryStatus::Resolved);
        assert(r->ground_status == ConductorBoundaryStatus::Resolved);
    }

    // EndpointSemanticReconstruction is consulted as corroborating
    // provenance only - it never overrides the per-category facts
    // computed from raw TerminalCandidate evidence.
    {
        std::vector<EndpointCandidate> endpoints = {make_endpoint("ep-y")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-y", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        EndpointSemanticReconstruction reconstruction;
        reconstruction.id = "esr-1";
        reconstruction.endpoint_id = "ep-y";
        reconstruction.component_id = "comp-1";
        reconstruction.endpoint_kind = EndpointKind::ComponentTerminal;
        reconstruction.status = EndpointSemanticReconstructionStatus::Resolved;
        const auto artifacts =
            resolver.resolve(endpoints, tcs, {reconstruction}, {});
        const auto* r = find_resolution(artifacts.resolutions, "ep-y");
        assert(r->component_status == ConductorBoundaryStatus::Resolved);
        assert(r->component_id == "comp-1");
        // The EndpointSemanticReconstruction contributed a traceable
        // evidence entry, not a silent second decision path.
        assert(r->evidence_ids.size() >= 2);
    }

    // Multiple endpoints resolve independently in one call (used by the
    // real pipeline against the full endpoint list).
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-multi-1"), make_endpoint("ep-multi-2")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-multi-1", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        assert(artifacts.resolutions.size() == 2);
        const auto* r1 = find_resolution(artifacts.resolutions, "ep-multi-1");
        const auto* r2 = find_resolution(artifacts.resolutions, "ep-multi-2");
        assert(r1->boundary_status == ConductorBoundaryStatus::Resolved);
        assert(r2->boundary_status == ConductorBoundaryStatus::Unresolved);
    }

    // Coverage tallying matches the resolutions produced.
    {
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-cov-1"), make_endpoint("ep-cov-2")};
        std::vector<TerminalCandidate> tcs = {
            make_terminal_candidate(
                "tc-1", "ep-cov-1", "comp-1",
                TerminalCandidateKind::ComponentBoundary)};
        const auto artifacts = resolver.resolve(endpoints, tcs, {}, {});
        assert(artifacts.coverage.total == 2);
        assert(artifacts.coverage.boundary_resolved == 1);
        assert(artifacts.coverage.boundary_unresolved == 1);
        const auto recomputed =
            build_conductor_boundary_coverage(artifacts.resolutions);
        assert(recomputed.total == artifacts.coverage.total);
        assert(recomputed.boundary_resolved == artifacts.coverage.boundary_resolved);
    }

    return 0;
}
