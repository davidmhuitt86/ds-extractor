#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace eke::dx::wire {

// AP-WIRE-022A: coverage diagnostics OBSERVE the already-built WireModel.
// They must never mutate geometry, topology, endpoint identity, wire
// identity, or electrical-net membership. A diagnostic that requires
// changing engineering objects to "look correct" belongs in a correction
// AP, not here.

enum class ConductorOwnershipClass {
    Unreferenced,   // zero topology edges, zero wires
    TopologyOnly,   // referenced by topology but not by any Wire
    WireOnly,       // referenced by a Wire but not by topology
    Normal,         // referenced by topology and exactly one Wire
    Shared          // referenced by topology and more than one Wire
};

enum class EndpointWireCoverageClass {
    ZeroWireEndpoint,
    SingleWireEndpoint,
    MultipleWireEndpoint
};

// Severity is intentionally coarser than WireValidationSeverity: most
// coverage classifications describe expected ambiguity, not defects.
// "Notice" means "this is legitimate but worth surfacing" (e.g. a shared
// conductor traversed by two endpoint-to-endpoint wires). "Warning" means
// the diagnostic could not find engineering justification for the state.
enum class CoverageSeverity { Notice, Warning };

struct CoverageDiagnosticRecord {
    std::string category;   // e.g. "conductor", "endpoint", "wire",
                             // "topology_edge", "topology_node",
                             // "component", "connector", "electrical_net"
    std::string code;
    std::string object_id;
    std::vector<std::string> related_object_ids;
    CoverageSeverity severity = CoverageSeverity::Notice;
    std::string detail;
};

struct ConductorOwnershipCoverage {
    std::size_t total = 0;
    std::size_t unreferenced = 0;
    std::size_t topology_only = 0;
    std::size_t wire_only = 0;
    std::size_t normal = 0;
    std::size_t shared = 0;
};

struct EndpointWireCoverage {
    std::size_t total = 0;
    std::size_t zero_wire = 0;
    std::size_t single_wire = 0;
    std::size_t multiple_wire = 0;
};

struct WireIntegrityCoverage {
    std::size_t total = 0;
    std::size_t valid = 0;
    std::size_t invalid = 0;
};

struct TopologyEdgeCoverage {
    std::size_t total = 0;
    std::size_t missing_conductor = 0;
    std::size_t missing_from_node = 0;
    std::size_t missing_to_node = 0;
    std::size_t unowned_by_any_wire = 0;
};

struct TopologyNodeCoverage {
    std::size_t total = 0;
    std::size_t zero_degree = 0;
    std::size_t low_degree_splice = 0;
    std::size_t conductor_end_without_endpoint = 0;
};

struct ComponentTerminalCoverage {
    std::size_t total = 0;
    std::size_t diagram_furniture = 0;
    std::size_t real_candidates = 0;
    std::size_t real_with_terminal_evidence = 0;
    std::size_t real_without_terminal_evidence = 0;
    std::size_t furniture_without_terminal_evidence = 0;
};

struct ConnectorCoverage {
    std::size_t total = 0;
    std::size_t terminals_total = 0;
    std::size_t genuine_looking = 0;
    std::size_t furniture_derived = 0;
    std::size_t unresolved = 0;
};

struct ElectricalNetCoverage {
    std::size_t total = 0;
    std::size_t endpoints_in_nets_total = 0;
    std::size_t endpoints_not_in_any_net = 0;
    std::size_t endpoints_in_multiple_nets = 0;
};

struct CoverageReport {
    ConductorOwnershipCoverage conductors;
    EndpointWireCoverage endpoints;
    WireIntegrityCoverage wires;
    TopologyEdgeCoverage topology_edges;
    TopologyNodeCoverage topology_nodes;
    ComponentTerminalCoverage components;
    ConnectorCoverage connectors;
    ElectricalNetCoverage electrical_nets;

    // Deterministically ordered (category, code, object_id). Only
    // classifications the diagnostic could not justify as expected
    // ambiguity are severity=Warning; everything else is Notice.
    std::vector<CoverageDiagnosticRecord> findings;
};

[[nodiscard]] ConductorOwnershipClass classify_conductor_ownership(
    std::size_t topology_edge_references,
    std::size_t wire_references);

[[nodiscard]] EndpointWireCoverageClass classify_endpoint_wire_coverage(
    std::size_t wire_references);

[[nodiscard]] CoverageReport build_coverage_report(const WireModel& model);

} // namespace eke::dx::wire
