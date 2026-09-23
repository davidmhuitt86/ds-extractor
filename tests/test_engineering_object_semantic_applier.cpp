#include "eke_dx_wire/topology/engineering_object_semantic_applier.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace eke::dx::wire;

int main() {
    EngineeringObjectSemanticApplier applier;

    ComponentCandidate component;
    component.id = "component-1";

    EndpointCandidate endpoint;
    endpoint.id = "endpoint-1";
    endpoint.terminal_name = "PIN 1";
    endpoint.function_label.clear();
    endpoint.wire_color.clear();

    EngineeringObjectSemanticResolution function{
        "resolution-1",
        "text-1",
        "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        TextSemanticKind::FunctionLabel,
        "IGNITION",
        "IGNITION",
        ConfidenceClass::High,
        3.0,
        "semantic-object-association:json-observation"
    };

    EngineeringObjectSemanticResolution color{
        "resolution-2",
        "text-2",
        "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        TextSemanticKind::WireColorLabel,
        "BL/R",
        "BL/R",
        ConfidenceClass::Medium,
        4.0,
        "semantic-object-association:json-observation"
    };

    std::vector<ComponentCandidate> components{component};
    std::vector<EndpointCandidate> endpoints{endpoint};

    applier.apply(
        components,
        endpoints,
        {function, color});

    endpoint = endpoints.front();

    // The applier enriches existing engineering objects without changing
    // established terminal identity.
    assert(endpoint.terminal_name == "PIN 1");
    assert(endpoint.function_label == "IGNITION");
    assert(endpoint.wire_color == "BL/R");

    EndpointCandidate existing = endpoint;

    EngineeringObjectSemanticResolution conflicting{
        "resolution-3",
        "text-3",
        "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        TextSemanticKind::FunctionLabel,
        "STARTER",
        "STARTER",
        ConfidenceClass::High,
        2.0,
        "semantic-object-association:json-observation"
    };

    std::vector<EndpointCandidate> conflicting_endpoints{existing};
    applier.apply(
        components,
        conflicting_endpoints,
        {conflicting});

    existing = conflicting_endpoints.front();

    // Existing semantic identity is never overwritten by a later conflicting
    // resolution.
    assert(existing.function_label == "IGNITION");

    EngineeringObjectSemanticResolution component_label{
        "resolution-4",
        "text-4",
        "component-1",
        SemanticAssociationTargetKind::Component,
        TextSemanticKind::ComponentLabel,
        "IGNITION COIL",
        "IGNITION COIL",
        ConfidenceClass::High,
        5.0,
        "semantic-object-association:json-observation"
    };

    applier.apply(
        components,
        conflicting_endpoints,
        {component_label});

    assert(components.front().semantic_labels.size() == 1);
    assert(components.front().semantic_labels.front() == "IGNITION COIL");

    return 0;
}
