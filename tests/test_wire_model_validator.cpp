#include "eke_dx_wire/topology/wire_model_validator.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

static EndpointCandidate endpoint(
    const char* id,
    const char* node) {
    EndpointCandidate e;
    e.id = id;
    e.node_id = node;
    e.kind = EndpointKind::GeometricConductorEnd;
    e.confidence = ConfidenceClass::Medium;
    return e;
}

static ConductorSegment segment(
    const char* id,
    bool heavy = false) {
    ConductorSegment s;
    s.id = id;
    s.heavy_cable = heavy;
    return s;
}

int main() {
    {
        WireModel model;
        model.endpoint_candidates = {
            endpoint("A", "n1"),
            endpoint("B", "n3")
        };
        model.nodes = {
            {"n1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"n2", {10, 0}, TopologyNodeType::Continuation, true},
            {"n3", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };
        model.edges = {
            {"e1", "n1", "n2", "s1"},
            {"e2", "n2", "n3", "s2"}
        };
        model.conductor_segments = {
            segment("s1"),
            segment("s2")
        };
        model.wires = {
            {"w1", "A", "B", {"e1", "e2"}, {"s1", "s2"},
             ConfidenceClass::Medium, false}
        };

        const auto report = WireModelValidator().validate(model);

        assert(report.valid);
        assert(report.wires_checked == 1);
        assert(report.valid_wires == 1);
        assert(report.electrical_nets_checked == 0);
        assert(report.issues.size() == 1);
        assert(report.issues.front().severity ==
               WireValidationSeverity::Warning);
        assert(report.issues.front().code ==
               "WIRE-GEOMETRIC-ENDPOINTS");
    }

    {
        WireModel model;
        model.endpoint_candidates = {
            endpoint("A", "n1"),
            endpoint("B", "n3")
        };
        model.nodes = {
            {"n1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"n2", {10, 0}, TopologyNodeType::Continuation, true},
            {"n3", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };
        model.edges = {
            {"e1", "n1", "n2", "s1"},
            {"e2", "n2", "n3", "s2"}
        };
        model.conductor_segments = {
            segment("s1"),
            segment("s2")
        };
        model.wires = {
            {"bad", "A", "B", {"e1"}, {"s1"},
             ConfidenceClass::Medium, false}
        };

        const auto report = WireModelValidator().validate(model);
        assert(!report.valid);
        assert(report.valid_wires == 0);

        bool found = false;
        for (const auto& item : report.issues) {
            if (item.code == "WIRE-ENDPOINT-DEGREE-MISMATCH") {
                found = true;
            }
        }
        assert(found);
    }

    {
        WireModel model;
        model.endpoint_candidates = {
            endpoint("A", "n1"),
            endpoint("B", "n2")
        };
        model.nodes = {
            {"n1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"n2", {10, 0}, TopologyNodeType::ConductorEnd, true}
        };
        model.edges = {
            {"e1", "n1", "n2", "missing-segment"}
        };
        model.wires = {
            {"bad", "A", "B", {"e1"}, {"missing-segment"},
             ConfidenceClass::Medium, false}
        };

        const auto report = WireModelValidator().validate(model);
        assert(!report.valid);
        assert(report.valid_wires == 0);
    }

    {
        WireModel model;
        model.endpoint_candidates = {
            endpoint("A", "n1"),
            endpoint("B", "n2")
        };
        model.nodes = {
            {"n1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"n2", {10, 0}, TopologyNodeType::ConductorEnd, true}
        };
        model.edges = {
            {"e1", "n1", "n2", "s1"}
        };
        model.conductor_segments = {segment("s1", true)};
        model.wires = {
            {"bad-heavy", "A", "B", {"e1"}, {"s1"},
             ConfidenceClass::Medium, false}
        };

        const auto report = WireModelValidator().validate(model);
        assert(!report.valid);
        bool found = false;
        for (const auto& item : report.issues) {
            if (item.code == "WIRE-HEAVY-CABLE-MISMATCH") {
                found = true;
            }
        }
        assert(found);
    }

    {
        WireModel model;
        model.endpoint_candidates = {
            endpoint("G", "n1"),
            endpoint("A", "n2")
        };
        model.nodes = {
            {"n1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s", {10, 0}, TopologyNodeType::Splice, true},
            {"n2", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };
        model.edges = {
            {"e1", "n1", "s", "s1"},
            {"e2", "s", "n2", "s2"}
        };
        model.electrical_nets = {
            {"net1", {"G", "A"}, {"s"}, {"e1", "e2"},
             DistributionRole::Unknown, ConfidenceClass::Unresolved, ""}
        };

        const auto report = WireModelValidator().validate(model);
        assert(report.valid);
        bool found = false;
        for (const auto& item : report.issues) {
            if (item.code == "NET-ROLE-UNRESOLVED") {
                found = true;
            }
        }
        assert(found);
    }

    std::cout << "wire model validator tests passed\n";
    return 0;
}
