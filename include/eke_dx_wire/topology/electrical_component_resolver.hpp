#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct ElectricalComponentResolutionArtifacts {
    std::vector<ElectricalComponent> electrical_components;
};

/**
 * AP-DIAG-FIX-008: resolves the semantic boundary between an
 * extraction-level ComponentCandidate and an engineering-level
 * ElectricalComponent/Module. Pure, deterministic, read-only with
 * respect to every input - it never mutates a ComponentCandidate,
 * SymbolFamilyResolution, EndpointCandidate, or ConnectorCandidate, and
 * never touches topology/wires/electrical nets.
 *
 * Governing rule (see docs/AP-DIAG-FIX-008_Electrical_Component_Semantic_
 * Boundary.md for the full definitions): a ComponentCandidate is
 * promoted to `ElectricalComponentResolutionStatus::Resolved` only when
 * BOTH an attributable electrical terminal (a ComponentTerminal
 * EndpointCandidate owned by it) AND independent electrical-function
 * evidence (a Resolved SymbolFamilyResolution whose family is neither
 * Ground nor Unknown) exist. Geometric resemblance to a component shape
 * is never, by itself, sufficient - this mirrors AP-WIRE-029's
 * never-guess rule for physical Wire identity, applied here to component
 * identity instead.
 *
 * Every non-furniture ComponentCandidate receives exactly one
 * ElectricalComponent record, so the two collections' sizes are directly
 * comparable and every candidate's disposition is explicit:
 *   - DiagramFurniture candidates are Rejected (DiagramFurniture reason).
 *   - ChassisGround candidates are Rejected (ChassisGroundReference
 *     reason) - a chassis ground is an electrical reference/termination
 *     object, never an ordinary component/module (Definition F).
 *   - Candidates owned by a ConnectorCandidate are Rejected
 *     (ConnectorInterface reason) - a connector is an electrical
 *     interface, kept distinct from component/module semantics even
 *     though it may have many terminals (Definition E).
 *   - Everything else is Resolved when the promotion evidence above
 *     exists, otherwise Unresolved (never guessed).
 */
class ElectricalComponentResolver {
public:
    [[nodiscard]] ElectricalComponentResolutionArtifacts resolve(
        const std::vector<ComponentCandidate>& components,
        const std::vector<SymbolFamilyResolution>& symbol_family_resolutions,
        const std::vector<ConnectorCandidate>& connectors,
        const std::vector<EndpointCandidate>& endpoints) const;
};

} // namespace eke::dx::wire
