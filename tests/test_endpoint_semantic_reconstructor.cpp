#include "eke_dx_wire/topology/endpoint_semantic_reconstructor.hpp"

#include <cassert>

using namespace eke::dx::wire;

static EndpointCandidate endpoint(const char* id) {
    EndpointCandidate result;
    result.id = id;
    result.node_id = std::string(id) + "-node";
    result.kind = EndpointKind::GeometricConductorEnd;
    result.confidence = ConfidenceClass::Low;
    return result;
}

static TerminalSemanticEvidence evidence(
    const char* endpoint_id,
    const char* component_id,
    EndpointKind kind,
    TerminalRole role,
    ConfidenceClass confidence) {

    TerminalSemanticEvidence result;
    result.endpoint_id = endpoint_id;
    result.component_id = component_id;
    result.endpoint_kind = kind;
    result.role = role;
    result.confidence = confidence;
    return result;
}

int main() {
    {
        const std::vector<EndpointCandidate> endpoints{endpoint("e1")};
        const std::vector<TerminalSemanticEvidence> evidence_set{
            evidence("e1", "component-1",
                     EndpointKind::ComponentTerminal,
                     TerminalRole::ComponentTerminal,
                     ConfidenceClass::High)
        };

        const auto result =
            EndpointSemanticReconstructor().reconstruct(
                endpoints, evidence_set);

        assert(result.reconstructions.front().status ==
               EndpointSemanticReconstructionStatus::Resolved);
        assert(result.endpoints.front().kind ==
               EndpointKind::ComponentTerminal);
        assert(result.endpoints.front().terminal_role ==
               TerminalRole::ComponentTerminal);
        assert(result.endpoints.front().component_id == "component-1");
        assert(result.endpoints.front().confidence ==
               ConfidenceClass::High);
    }

    {
        const std::vector<EndpointCandidate> endpoints{endpoint("e1")};
        const std::vector<TerminalSemanticEvidence> evidence_set{
            evidence("e1", "component-1",
                     EndpointKind::ComponentTerminal,
                     TerminalRole::ComponentTerminal,
                     ConfidenceClass::High),
            evidence("e1", "component-2",
                     EndpointKind::ComponentTerminal,
                     TerminalRole::ComponentTerminal,
                     ConfidenceClass::High)
        };

        const auto result =
            EndpointSemanticReconstructor().reconstruct(
                endpoints, evidence_set);

        assert(result.reconstructions.front().status ==
               EndpointSemanticReconstructionStatus::Conflicted);
        assert(result.endpoints.front().kind ==
               EndpointKind::GeometricConductorEnd);
        assert(result.endpoints.front().terminal_role ==
               TerminalRole::Unknown);
        assert(result.endpoints.front().component_id.empty());
        assert(result.endpoints.front().confidence ==
               ConfidenceClass::Unresolved);
    }

    {
        const std::vector<EndpointCandidate> endpoints{endpoint("e1")};
        const std::vector<TerminalSemanticEvidence> evidence_set{
            evidence("e1", "component-1",
                     EndpointKind::ComponentTerminal,
                     TerminalRole::ComponentTerminal,
                     ConfidenceClass::Medium),
            evidence("e1", "component-1",
                     EndpointKind::ComponentTerminal,
                     TerminalRole::ComponentTerminal,
                     ConfidenceClass::High)
        };

        const auto result =
            EndpointSemanticReconstructor().reconstruct(
                endpoints, evidence_set);

        assert(result.reconstructions.front().status ==
               EndpointSemanticReconstructionStatus::Resolved);
        assert(result.endpoints.front().component_id == "component-1");
        assert(result.endpoints.front().confidence ==
               ConfidenceClass::High);
    }

    // AP-DIAG-FIX-002 / TEST 3: a conductor terminating at a recognized
    // connector terminal resolves to EndpointKind::ConnectorTerminal when
    // the upstream evidence (TerminalCandidateKind::ConnectorBoundary, via
    // TerminalSemanticEvidenceBuilder) supports it - this is the one link
    // in the existing connector-representation chain no prior test
    // exercised directly at this stage.
    {
        const std::vector<EndpointCandidate> endpoints{endpoint("e1")};
        const std::vector<TerminalSemanticEvidence> evidence_set{
            evidence("e1", "component-connector",
                     EndpointKind::ConnectorTerminal,
                     TerminalRole::ConnectorTerminal,
                     ConfidenceClass::High)
        };

        const auto result =
            EndpointSemanticReconstructor().reconstruct(
                endpoints, evidence_set);

        assert(result.reconstructions.front().status ==
               EndpointSemanticReconstructionStatus::Resolved);
        assert(result.endpoints.front().kind ==
               EndpointKind::ConnectorTerminal);
        assert(result.endpoints.front().terminal_role ==
               TerminalRole::ConnectorTerminal);
        assert(result.endpoints.front().component_id ==
               "component-connector");
        assert(result.endpoints.front().confidence ==
               ConfidenceClass::High);
    }

    return 0;
}
