#include "eke_dx_wire/topology/semantic_observation_resolver.hpp"

#include <cassert>

using namespace eke::dx::wire;

static TextSemanticEvidence semantic(
    const char* id,
    const char* region,
    TextSemanticKind kind,
    ConfidenceClass confidence) {

    TextSemanticEvidence value;
    value.id = id;
    value.text_region_id = region;
    value.kind = kind;
    value.confidence = confidence;
    value.source = "test-provider";
    return value;
}

static SemanticAssociation association(
    const char* id,
    const char* region,
    const char* endpoint,
    double distance,
    ConfidenceClass confidence) {

    SemanticAssociation value;
    value.id = id;
    value.text_region_id = region;
    value.target_id = endpoint;
    value.target_kind = SemanticAssociationTargetKind::Endpoint;
    value.relation = SemanticAssociationRelation::LabelToEndpoint;
    value.distance = distance;
    value.confidence = confidence;
    return value;
}

int main() {
    {
        const auto result = SemanticObservationResolver().resolve(
            {semantic(
                "s-gnd",
                "text-gnd",
                TextSemanticKind::GroundLabel,
                ConfidenceClass::High)},
            {association(
                "a-gnd",
                "text-gnd",
                "endpoint-1",
                5.0,
                ConfidenceClass::High)});

        assert(result.size() == 1);
        assert(result.front().endpoint_id == "endpoint-1");
        assert(result.front().role == DistributionRole::Ground);
        assert(result.front().confidence == ConfidenceClass::High);
    }

    {
        const auto result = SemanticObservationResolver().resolve(
            {semantic(
                "s-b",
                "text-b",
                TextSemanticKind::PowerFeedLabel,
                ConfidenceClass::Medium)},
            {association(
                "a-b",
                "text-b",
                "endpoint-2",
                10.0,
                ConfidenceClass::Low)});

        assert(result.size() == 1);
        assert(result.front().role == DistributionRole::PowerFeed);
        assert(result.front().confidence == ConfidenceClass::Low);
    }

    {
        // A label-to-COMPONENT association (not label-to-endpoint) must
        // never be treated as endpoint evidence. The shared association()
        // helper always builds an Endpoint/LabelToEndpoint association, so
        // this scenario needs its own SemanticAssociation with the actual
        // component-targeting fields.
        SemanticAssociation component_association;
        component_association.id = "a-component";
        component_association.text_region_id = "text-shared";
        component_association.target_id = "component-1";
        component_association.target_kind =
            SemanticAssociationTargetKind::Component;
        component_association.relation =
            SemanticAssociationRelation::LabelToComponent;
        component_association.distance = 1.0;
        component_association.confidence = ConfidenceClass::High;

        const auto result = SemanticObservationResolver().resolve(
            {semantic(
                "s-shared",
                "text-shared",
                TextSemanticKind::SharedFunctionFeedLabel,
                ConfidenceClass::High)},
            {component_association});

        assert(result.empty());
    }

    {
        const auto result = SemanticObservationResolver().resolve(
            {semantic(
                "s-near",
                "text-near",
                TextSemanticKind::GroundLabel,
                ConfidenceClass::High)},
            {
                association(
                    "a-1",
                    "text-near",
                    "endpoint-1",
                    8.0,
                    ConfidenceClass::High),
                association(
                    "a-2",
                    "text-near",
                    "endpoint-2",
                    8.0,
                    ConfidenceClass::High)
            });

        assert(result.empty());
    }

    {
        const auto result = SemanticObservationResolver().resolve(
            {
                semantic(
                    "s-gnd",
                    "text-conflict-g",
                    TextSemanticKind::GroundLabel,
                    ConfidenceClass::High),
                semantic(
                    "s-power",
                    "text-conflict-p",
                    TextSemanticKind::PowerFeedLabel,
                    ConfidenceClass::High)
            },
            {
                association(
                    "a-g",
                    "text-conflict-g",
                    "endpoint-conflict",
                    5.0,
                    ConfidenceClass::High),
                association(
                    "a-p",
                    "text-conflict-p",
                    "endpoint-conflict",
                    5.0,
                    ConfidenceClass::High)
            });

        assert(result.empty());
    }

    {
        const auto result = SemanticObservationResolver().resolve(
            {semantic(
                "s-fn",
                "text-fn",
                TextSemanticKind::FunctionLabel,
                ConfidenceClass::High)},
            {association(
                "a-fn",
                "text-fn",
                "endpoint-ignored",
                2.0,
                ConfidenceClass::High)});

        assert(result.empty());
    }

    return 0;
}
