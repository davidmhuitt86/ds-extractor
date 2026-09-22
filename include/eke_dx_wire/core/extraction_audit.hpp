#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <cstddef>

namespace eke::dx::wire {

struct ExtractionAudit {
    // Geometry
    std::size_t conductor_segments = 0;

    // Topology
    std::size_t topology_nodes = 0;
    std::size_t topology_edges = 0;
    std::size_t conductor_end_nodes = 0;
    std::size_t continuation_nodes = 0;
    std::size_t junction_nodes = 0;
    std::size_t splice_nodes = 0;
    std::size_t crossing_nodes = 0;
    std::size_t component_boundary_nodes = 0;
    std::size_t unresolved_nodes = 0;

    // Endpoint candidates
    std::size_t endpoint_candidates = 0;
    std::size_t geometric_endpoints = 0;
    std::size_t component_terminals = 0;
    std::size_t connector_terminals = 0;
    std::size_t ground_endpoints = 0;
    std::size_t external_connections = 0;
    std::size_t splice_endpoints = 0;
    std::size_t unresolved_endpoints = 0;

    // Components / shapes
    std::size_t shapes = 0;
    std::size_t enclosure_shapes = 0;
    std::size_t circular_shapes = 0;
    std::size_t chassis_ground_shapes = 0;
    std::size_t primitive_shapes = 0;
    std::size_t unknown_shapes = 0;

    // Wires
    std::size_t wires = 0;
    std::size_t heavy_cable_wires = 0;
    std::size_t unresolved_wires = 0;

    // Electrical nets
    std::size_t electrical_nets = 0;
    std::size_t ground_nets = 0;
    std::size_t power_feed_nets = 0;
    std::size_t shared_function_feed_nets = 0;
    std::size_t unresolved_nets = 0;

    // Validation
    std::size_t validation_errors = 0;
    std::size_t validation_warnings = 0;
    std::size_t valid_wires = 0;

    // Pipeline evidence
    std::size_t gaps_bridged = 0;
};

[[nodiscard]] ExtractionAudit build_extraction_audit(
    const WireModel& model,
    std::size_t gaps_bridged = 0);

} // namespace eke::dx::wire
