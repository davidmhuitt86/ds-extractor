#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"

#include <cassert>

using namespace eke::dx::wire;

static EndpointCandidate endpoint(
    const char* id,
    EndpointKind kind,
    TerminalRole role,
    ConfidenceClass confidence) {

    EndpointCandidate e;
    e.id = id;
    e.kind = kind;
    e.terminal_role = role;
    e.confidence = confidence;
    return e;
}

int main() {
    const auto result = CircuitRoleEvidenceBuilder().build({
        endpoint(
            "G",
            EndpointKind::Ground,
            TerminalRole::GroundTerminal,
            ConfidenceClass::High),
        endpoint(
            "P",
            EndpointKind::ExternalConnection,
            TerminalRole::PowerSource,
            ConfidenceClass::High),
        endpoint(
            "S",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::High),
        endpoint(
            "U",
            EndpointKind::GeometricConductorEnd,
            TerminalRole::Unknown,
            ConfidenceClass::Low),
        endpoint(
            "R",
            EndpointKind::Ground,
            TerminalRole::GroundTerminal,
            ConfidenceClass::Unresolved)
    });

    assert(result.size() == 2);
    assert(result[0].endpoint_id == "G");
    assert(result[0].role == DistributionRole::Ground);
    assert(result[1].endpoint_id == "P");
    assert(result[1].role == DistributionRole::PowerFeed);
    assert(result[0].source == "endpoint-semantic");
    assert(result[1].source == "endpoint-semantic");

    {
        auto labelled = endpoint(
            "LABEL-G",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::High);
        labelled.function_label = "gnd";

        const auto labelled_result =
            CircuitRoleEvidenceBuilder().build({labelled});

        assert(labelled_result.size() == 1);
        assert(labelled_result.front().endpoint_id == "LABEL-G");
        assert(labelled_result.front().role == DistributionRole::Ground);
        assert(labelled_result.front().source == "endpoint-label");
    }

    {
        auto labelled = endpoint(
            "LABEL-P",
            EndpointKind::ConnectorTerminal,
            TerminalRole::ConnectorTerminal,
            ConfidenceClass::Medium);
        labelled.terminal_name = "B+";

        const auto labelled_result =
            CircuitRoleEvidenceBuilder().build({labelled});

        assert(labelled_result.size() == 1);
        assert(labelled_result.front().role == DistributionRole::PowerFeed);
        assert(labelled_result.front().confidence == ConfidenceClass::Medium);
    }

    {
        auto labelled = endpoint(
            "LABEL-S",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::High);
        labelled.function_label = "shared-function-feed";

        const auto labelled_result =
            CircuitRoleEvidenceBuilder().build({labelled});

        assert(labelled_result.size() == 1);
        assert(
            labelled_result.front().role ==
            DistributionRole::SharedFunctionFeed);
    }

    {
        auto conflicting = endpoint(
            "CONFLICT",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::High);
        conflicting.terminal_name = "GND";
        conflicting.function_label = "B+";

        const auto conflicting_result =
            CircuitRoleEvidenceBuilder().build({conflicting});

        // Conflicting explicit semantic annotations must not be resolved by
        // lexical or enum ordering.
        assert(conflicting_result.empty());
    }

    {
        auto unresolved = endpoint(
            "UNRESOLVED",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::Unresolved);
        unresolved.function_label = "GND";

        const auto unresolved_result =
            CircuitRoleEvidenceBuilder().build({unresolved});

        assert(unresolved_result.empty());
    }

    return 0;
}
