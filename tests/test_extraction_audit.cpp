#include "eke_dx_wire/core/extraction_audit.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    WireModel model;

    model.conductor_segments.resize(3);
    model.nodes = {
        {"n1", {}, TopologyNodeType::ConductorEnd, true},
        {"n2", {}, TopologyNodeType::Continuation, true},
        {"n3", {}, TopologyNodeType::Junction, true},
        {"n4", {}, TopologyNodeType::Splice, true},
        {"n5", {}, TopologyNodeType::Crossing, false},
        {"n6", {}, TopologyNodeType::ComponentBoundary, true},
        {"n7", {}, TopologyNodeType::Unresolved, true}
    };

    EndpointCandidate geometric;
    geometric.kind = EndpointKind::GeometricConductorEnd;

    EndpointCandidate component;
    component.kind = EndpointKind::ComponentTerminal;

    EndpointCandidate connector;
    connector.kind = EndpointKind::ConnectorTerminal;

    EndpointCandidate ground_endpoint;
    ground_endpoint.kind = EndpointKind::Ground;

    EndpointCandidate external;
    external.kind = EndpointKind::ExternalConnection;

    EndpointCandidate splice;
    splice.kind = EndpointKind::Splice;

    EndpointCandidate unresolved;
    unresolved.kind = EndpointKind::Unresolved;

    model.endpoint_candidates = {
        geometric, component, connector, ground_endpoint,
        external, splice, unresolved
    };

    ComponentCandidate enclosure;
    enclosure.kind = ComponentCandidateKind::Enclosure;

    ComponentCandidate circle;
    circle.kind = ComponentCandidateKind::CircularSymbol;

    ComponentCandidate chassis;
    chassis.kind = ComponentCandidateKind::ChassisGround;

    ComponentCandidate primitive;
    primitive.kind = ComponentCandidateKind::PrimitiveSymbol;

    ComponentCandidate unknown;
    unknown.kind = ComponentCandidateKind::Unknown;

    model.component_candidates = {
        enclosure, circle, chassis, primitive, unknown
    };

    Wire wire;
    wire.confidence = ConfidenceClass::Medium;
    wire.heavy_cable = true;

    Wire unresolved_wire;
    unresolved_wire.confidence = ConfidenceClass::Unresolved;

    model.wires = {wire, unresolved_wire};

    ElectricalNet ground;
    ground.role = DistributionRole::Ground;

    ElectricalNet power;
    power.role = DistributionRole::PowerFeed;

    ElectricalNet shared;
    shared.role = DistributionRole::SharedFunctionFeed;

    ElectricalNet unresolved_net;
    unresolved_net.role = DistributionRole::Unknown;

    model.electrical_nets = {
        ground, power, shared, unresolved_net
    };

    model.wire_validation.valid_wires = 1;
    model.wire_validation.issues = {
        {WireValidationSeverity::Error, "E", "x", "error"},
        {WireValidationSeverity::Warning, "W", "x", "warning"}
    };

    const ExtractionAudit audit = build_extraction_audit(model, 5);

    assert(audit.conductor_segments == 3);
    assert(audit.topology_nodes == 7);
    assert(audit.topology_edges == 0);
    assert(audit.conductor_end_nodes == 1);
    assert(audit.continuation_nodes == 1);
    assert(audit.junction_nodes == 1);
    assert(audit.splice_nodes == 1);
    assert(audit.crossing_nodes == 1);
    assert(audit.component_boundary_nodes == 1);
    assert(audit.unresolved_nodes == 1);

    assert(audit.endpoint_candidates == 7);
    assert(audit.geometric_endpoints == 1);
    assert(audit.component_terminals == 1);
    assert(audit.connector_terminals == 1);
    assert(audit.ground_endpoints == 1);
    assert(audit.external_connections == 1);
    assert(audit.splice_endpoints == 1);
    assert(audit.unresolved_endpoints == 1);

    assert(audit.shapes == 5);
    assert(audit.enclosure_shapes == 1);
    assert(audit.circular_shapes == 1);
    assert(audit.chassis_ground_shapes == 1);
    assert(audit.primitive_shapes == 1);
    assert(audit.unknown_shapes == 1);

    assert(audit.wires == 2);
    assert(audit.heavy_cable_wires == 1);
    assert(audit.unresolved_wires == 1);

    assert(audit.electrical_nets == 4);
    assert(audit.ground_nets == 1);
    assert(audit.power_feed_nets == 1);
    assert(audit.shared_function_feed_nets == 1);
    assert(audit.unresolved_nets == 1);

    assert(audit.valid_wires == 1);
    assert(audit.validation_errors == 1);
    assert(audit.validation_warnings == 1);
    assert(audit.gaps_bridged == 5);

    std::cout << "extraction audit tests passed\n";
    return 0;
}
