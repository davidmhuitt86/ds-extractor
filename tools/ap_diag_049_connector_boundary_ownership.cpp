#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/core/ids.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/component_candidate_classifier.hpp"
#include "eke_dx_wire/image/circle_context_classifier.hpp"
#include "eke_dx_wire/image/diagram_furniture_classifier.hpp"
#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"
#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"
#include "eke_dx_wire/image/connector_geometry_detector.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

struct Focus {
    double x = 320.0;
    double y = 335.0;
    double width = 70.0;
    double height = 65.0;
};

std::string esc(const std::string& value) {
    std::string out;
    for (const char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string shape_kind(ShapeKind value) {
    switch (value) {
    case ShapeKind::Rectangle: return "rectangle";
    case ShapeKind::Circle: return "circle";
    case ShapeKind::ChassisGround: return "chassis_ground";
    case ShapeKind::ConnectorBody: return "connector_body";
    }
    return "unknown";
}

std::string shape_role(ShapeRole value) {
    switch (value) {
    case ShapeRole::Primitive: return "primitive";
    case ShapeRole::Enclosure: return "enclosure";
    case ShapeRole::Exclusion: return "exclusion";
    }
    return "unknown";
}

std::string component_kind(ComponentCandidateKind value) {
    switch (value) {
    case ComponentCandidateKind::Enclosure: return "enclosure";
    case ComponentCandidateKind::CircularSymbol: return "circular_symbol";
    case ComponentCandidateKind::ChassisGround: return "chassis_ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "primitive_symbol";
    case ComponentCandidateKind::DiagramFurniture: return "diagram_furniture";
    case ComponentCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

std::string confidence_name(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unknown";
}

std::string rejection_class(RejectedGeometryClass value) {
    switch (value) {
    case RejectedGeometryClass::ComponentAssociated: return "component_associated";
    case RejectedGeometryClass::ConnectorAssociated: return "connector_associated";
    case RejectedGeometryClass::TextAssociated: return "text_associated";
    case RejectedGeometryClass::Unresolved: return "unresolved";
    }
    return "unknown";
}

std::string endpoint_kind(EndpointKind value) {
    switch (value) {
    case EndpointKind::GeometricConductorEnd: return "geometric_conductor_end";
    case EndpointKind::ComponentTerminal: return "component_terminal";
    case EndpointKind::ConnectorTerminal: return "connector_terminal";
    case EndpointKind::Splice: return "splice";
    case EndpointKind::Ground: return "ground";
    case EndpointKind::ExternalConnection: return "external_connection";
    case EndpointKind::Unresolved: return "unresolved";
    }
    return "unknown";
}

std::string node_type(TopologyNodeType value) {
    switch (value) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    case TopologyNodeType::Unresolved: return "unresolved";
    }
    return "unknown";
}

void write_box(std::ostream& out, const BoundingBox& b) {
    out << "{\"x\":" << b.x << ",\"y\":" << b.y
        << ",\"width\":" << b.width << ",\"height\":" << b.height << "}";
}

bool box_in_focus(const BoundingBox& b, const Focus& f) {
    return static_cast<double>(b.x) <= f.x + f.width &&
           static_cast<double>(b.x + b.width) >= f.x &&
           static_cast<double>(b.y) <= f.y + f.height &&
           static_cast<double>(b.y + b.height) >= f.y;
}

bool point_in_focus(const Point2D& p, const Focus& f) {
    return p.x >= f.x && p.x <= f.x + f.width &&
           p.y >= f.y && p.y <= f.y + f.height;
}

bool segment_in_focus(const Segment2D& s, const Focus& f) {
    const double left = (std::min)(s.a.x, s.b.x);
    const double right = (std::max)(s.a.x, s.b.x);
    const double top = (std::min)(s.a.y, s.b.y);
    const double bottom = (std::max)(s.a.y, s.b.y);
    return left <= f.x + f.width && right >= f.x &&
           top <= f.y + f.height && bottom >= f.y;
}

// Mirrors the narrow AP-DIAG-FIX-001 boundary test; diagnostic only.
bool traces_boundary(const Segment2D& s, const BoundingBox& b, double tol) {
    const double left = b.x;
    const double right = b.x + b.width;
    const double top = b.y;
    const double bottom = b.y + b.height;
    const bool horizontal = std::abs(s.a.y - s.b.y) <= tol;
    const bool vertical = std::abs(s.a.x - s.b.x) <= tol;

    if (horizontal == vertical)
        return horizontal && s.length() <= tol;

    if (horizontal) {
        const double y = (s.a.y + s.b.y) * 0.5;
        if (std::abs(y - top) > tol && std::abs(y - bottom) > tol)
            return false;
        return (std::min)(s.a.x, s.b.x) >= left - tol &&
               (std::max)(s.a.x, s.b.x) <= right + tol;
    }

    const double x = (s.a.x + s.b.x) * 0.5;
    if (std::abs(x - left) > tol && std::abs(x - right) > tol)
        return false;
    return (std::min)(s.a.y, s.b.y) >= top - tol &&
           (std::max)(s.a.y, s.b.y) <= bottom + tol;
}

bool has_segment(
    const std::vector<ConductorSegment>& items, const std::string& id) {
    return std::any_of(items.begin(), items.end(), [&](const auto& item) {
        return item.id == id;
    });
}

const RejectedGeometryEvidence* rejected(
    const std::vector<RejectedGeometryEvidence>& items,
    const std::string& id) {
    const auto it = std::find_if(items.begin(), items.end(), [&](const auto& item) {
        return item.id == id;
    });
    return it == items.end() ? nullptr : &*it;
}

void write_shapes(
    std::ostream& out, const std::vector<ShapeRegion>& shapes, const Focus& focus) {
    out << "[";
    bool first = true;
    for (const auto& s : shapes) {
        if (!box_in_focus(s.bounds, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(s.id)
            << "\",\"kind\":\"" << shape_kind(s.kind)
            << "\",\"role\":\"" << shape_role(s.role)
            << "\",\"confidence\":" << s.confidence << ",\"bounds\":";
        write_box(out, s.bounds);
        out << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_components(
    std::ostream& out, const std::vector<ComponentCandidate>& components,
    const Focus& focus) {
    out << "[";
    bool first = true;
    for (const auto& c : components) {
        if (!box_in_focus(c.bounds, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(c.id)
            << "\",\"kind\":\"" << component_kind(c.kind)
            << "\",\"confidence\":\"" << confidence_name(c.confidence)
            << "\",\"shape_ids\":[";
        for (std::size_t i = 0; i < c.shape_ids.size(); ++i) {
            if (i) out << ",";
            out << "\"" << esc(c.shape_ids[i]) << "\"";
        }
        out << "],\"bounds\":";
        write_box(out, c.bounds);
        out << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_text(
    std::ostream& out, const std::vector<TextRegion>& regions, const Focus& focus) {
    out << "[";
    bool first = true;
    for (const auto& t : regions) {
        if (!box_in_focus(t.bounds, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(t.id)
            << "\",\"kind_numeric\":" << static_cast<int>(t.kind)
            << ",\"confidence\":" << t.confidence << ",\"bounds\":";
        write_box(out, t.bounds);
        out << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_regions(
    std::ostream& out, const std::vector<ShapeRegion>& regions, const Focus& focus,
    bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& c : regions) {
        if (!include_all && !box_in_focus(c.bounds, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(c.id)
            << "\",\"bounds\":";
        write_box(out, c.bounds);
        out << ",\"confidence\":" << c.confidence << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_candidates(
    std::ostream& out, const std::vector<ConnectorCandidate>& connectors,
    const Focus& focus, bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& c : connectors) {
        if (!include_all && !box_in_focus(c.bounds, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(c.id)
            << "\",\"component_candidate_id\":\"" << esc(c.component_candidate_id)
            << "\",\"topology_numeric\":" << static_cast<int>(c.topology)
            << ",\"confidence\":\"" << confidence_name(c.confidence)
            << "\",\"bounds\":";
        write_box(out, c.bounds);
        out << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_boundary_resolutions(
    std::ostream& out,
    const std::vector<ConnectorBoundaryResolution>& boundaries,
    const Focus& focus, bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& boundary : boundaries) {
        if (!include_all &&
            !box_in_focus(boundary.effective_bounds, focus) &&
            !box_in_focus(boundary.detected_bounds, focus)) {
            continue;
        }
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(boundary.connector_region_id)
            << "\",\"reconstructed_from_rectangular_strokes\":"
            << (boundary.reconstructed_from_rectangular_strokes ? "true" : "false")
            << ",\"detected_bounds\":";
        write_box(out, boundary.detected_bounds);
        out << ",\"effective_bounds\":";
        write_box(out, boundary.effective_bounds);
        out << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_pins(
    std::ostream& out, const WireModel& model, const Focus& focus,
    bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& pin : model.connector_pins) {
        if (!include_all && !point_in_focus(pin.position, focus))
            continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(pin.id)
            << "\",\"connector_id\":\"" << esc(pin.connector_id)
            << "\",\"x\":" << pin.position.x
            << ",\"y\":" << pin.position.y
            << ",\"ordinal\":" << pin.ordinal
            << ",\"confidence\":\"" << confidence_name(pin.confidence)
            << "\",\"conductor_crossing_evidence_ids\":[";
        for (std::size_t i = 0; i < pin.conductor_crossing_evidence_ids.size(); ++i) {
            if (i) out << ",";
            out << "\"" << esc(pin.conductor_crossing_evidence_ids[i]) << "\"";
        }
        out << "]}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_crossings(
    std::ostream& out, const WireModel& model, const Focus& focus,
    bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& crossing : model.connector_conductor_crossing_evidence) {
        if (!include_all && !point_in_focus(crossing.crossing_point, focus))
            continue;
        if (!first) out << ",";
        first = false;
        const char* interaction =
            crossing.interaction == ConnectorConductorInteractionKind::Termination
                ? "termination"
                : crossing.interaction == ConnectorConductorInteractionKind::PassThrough
                    ? "pass_through" : "unknown";
        out << "\n    {\"id\":\"" << esc(crossing.id)
            << "\",\"connector_id\":\"" << esc(crossing.connector_id)
            << "\",\"pin_id\":\"" << esc(crossing.pin_id)
            << "\",\"conductor_segment_id\":\"" << esc(crossing.conductor_segment_id)
            << "\",\"interaction\":\"" << interaction
            << "\",\"x\":" << crossing.crossing_point.x
            << ",\"y\":" << crossing.crossing_point.y
            << ",\"confidence\":\"" << confidence_name(crossing.confidence)
            << "\",\"stage\":\"" << esc(crossing.provenance.stage) << "\"}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_connector_associations(
    std::ostream& out, const WireModel& model, const Focus& focus,
    bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& association : model.connector_terminal_associations) {
        const auto pin = std::find_if(
            model.connector_pins.begin(), model.connector_pins.end(),
            [&](const ConnectorPin& candidate) {
                return candidate.id == association.pin_id;
            });
        if (pin == model.connector_pins.end() ||
            (!include_all && !point_in_focus(pin->position, focus)))
            continue;
        if (!first) out << ",";
        first = false;
        const char* status =
            association.status == ConnectorTerminalAssociationStatus::Resolved
                ? "resolved"
                : association.status == ConnectorTerminalAssociationStatus::Conflicted
                    ? "conflicted" : "unresolved";
        out << "\n    {\"id\":\"" << esc(association.id)
            << "\",\"connector_id\":\"" << esc(association.connector_id)
            << "\",\"pin_id\":\"" << esc(association.pin_id)
            << "\",\"endpoint_id\":\"" << esc(association.endpoint_id)
            << "\",\"status\":\"" << status
            << "\",\"confidence\":\"" << confidence_name(association.confidence)
            << "\",\"evidence_ids\":[";
        for (std::size_t i = 0; i < association.evidence_ids.size(); ++i) {
            if (i) out << ",";
            out << "\"" << esc(association.evidence_ids[i]) << "\"";
        }
        out << "]}";
    }
    if (!first) out << "\n  ";
    out << "]";
}


const ConnectorCandidate* find_connector_candidate(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(model.connector_candidates.begin(),
        model.connector_candidates.end(),
        [&](const ConnectorCandidate& item) { return item.id == id; });
    return it == model.connector_candidates.end() ? nullptr : &*it;
}

const ConnectorPin* find_connector_pin(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(model.connector_pins.begin(),
        model.connector_pins.end(),
        [&](const ConnectorPin& item) { return item.id == id; });
    return it == model.connector_pins.end() ? nullptr : &*it;
}

const EndpointCandidate* find_endpoint_candidate(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(model.endpoint_candidates.begin(),
        model.endpoint_candidates.end(),
        [&](const EndpointCandidate& item) { return item.id == id; });
    return it == model.endpoint_candidates.end() ? nullptr : &*it;
}

const ConnectorTerminal* find_connector_terminal(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(model.connector_terminals.begin(),
        model.connector_terminals.end(),
        [&](const ConnectorTerminal& item) { return item.id == id; });
    return it == model.connector_terminals.end() ? nullptr : &*it;
}

std::string association_status_name(ConnectorTerminalAssociationStatus value) {
    switch (value) {
    case ConnectorTerminalAssociationStatus::Resolved: return "resolved";
    case ConnectorTerminalAssociationStatus::Unresolved: return "unresolved";
    case ConnectorTerminalAssociationStatus::Conflicted: return "conflicted";
    }
    return "unknown";
}

std::string terminal_role_name(TerminalRole value) {
    switch (value) {
    case TerminalRole::Unknown: return "unknown";
    case TerminalRole::ComponentTerminal: return "component_terminal";
    case TerminalRole::ConnectorTerminal: return "connector_terminal";
    case TerminalRole::GroundTerminal: return "ground_terminal";
    case TerminalRole::PowerSource: return "power_source";
    case TerminalRole::ExternalConnection: return "external_connection";
    }
    return "unknown";
}

const char* connector_interaction_name(ConnectorConductorInteractionKind value) {
    switch (value) {
    case ConnectorConductorInteractionKind::PassThrough: return "pass_through";
    case ConnectorConductorInteractionKind::Termination: return "termination";
    case ConnectorConductorInteractionKind::Unknown: return "unknown";
    }
    return "unknown";
}

bool contains_id(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

std::string terminal_materialization_outcome(
    const ConnectorTerminalAssociationEvidence& association,
    const WireModel& model,
    std::string& expected_terminal_id) {
    expected_terminal_id.clear();
    if (association.status == ConnectorTerminalAssociationStatus::Unresolved)
        return "association_status_unresolved";
    if (association.status == ConnectorTerminalAssociationStatus::Conflicted)
        return "association_status_conflicted";
    if (association.endpoint_id.empty())
        return "endpoint_id_empty";

    expected_terminal_id = stable_id(
        "connector-terminal", association.id + ":" + association.endpoint_id);

    // An actual object is authoritative. Endpoint semantic reconstruction
    // runs later and can alter the endpoint snapshot after materialization.
    if (find_connector_terminal(model, expected_terminal_id) != nullptr)
        return "materialized";
    if (find_connector_candidate(model, association.connector_id) == nullptr)
        return "connector_reference_missing";
    if (find_connector_pin(model, association.pin_id) == nullptr)
        return "pin_reference_missing";
    if (find_endpoint_candidate(model, association.endpoint_id) == nullptr)
        return "endpoint_reference_missing";

    // Do not infer the earlier builder-stage endpoint-kind failure from the
    // later endpoint snapshot. Preserve the discrepancy as an audit finding.
    return "resolved_association_terminal_missing";
}

struct ConnectorAssociationAuditCounts {
    std::size_t resolved = 0;
    std::size_t unresolved = 0;
    std::size_t conflicted = 0;
    std::size_t materialized = 0;
    std::size_t association_status_unresolved = 0;
    std::size_t association_status_conflicted = 0;
    std::size_t endpoint_id_empty = 0;
    std::size_t connector_reference_missing = 0;
    std::size_t pin_reference_missing = 0;
    std::size_t endpoint_reference_missing = 0;
    std::size_t resolved_association_terminal_missing = 0;
};

ConnectorAssociationAuditCounts count_connector_associations(const WireModel& model) {
    ConnectorAssociationAuditCounts counts;
    for (const auto& association : model.connector_terminal_associations) {
        switch (association.status) {
        case ConnectorTerminalAssociationStatus::Resolved: ++counts.resolved; break;
        case ConnectorTerminalAssociationStatus::Unresolved: ++counts.unresolved; break;
        case ConnectorTerminalAssociationStatus::Conflicted: ++counts.conflicted; break;
        }
        std::string expected_id;
        const std::string outcome =
            terminal_materialization_outcome(association, model, expected_id);
        if (outcome == "materialized") ++counts.materialized;
        else if (outcome == "association_status_unresolved")
            ++counts.association_status_unresolved;
        else if (outcome == "association_status_conflicted")
            ++counts.association_status_conflicted;
        else if (outcome == "endpoint_id_empty") ++counts.endpoint_id_empty;
        else if (outcome == "connector_reference_missing")
            ++counts.connector_reference_missing;
        else if (outcome == "pin_reference_missing") ++counts.pin_reference_missing;
        else if (outcome == "endpoint_reference_missing")
            ++counts.endpoint_reference_missing;
        else if (outcome == "resolved_association_terminal_missing")
            ++counts.resolved_association_terminal_missing;
    }
    return counts;
}

void write_connector_association_audit(
    std::ostream& out, const WireModel& model, const Focus& focus) {
    out << "[";
    bool first_association = true;
    for (const auto& association : model.connector_terminal_associations) {
        const ConnectorPin* pin = find_connector_pin(model, association.pin_id);
        const bool pin_in_focus =
            pin != nullptr && point_in_focus(pin->position, focus);
        if (!first_association) out << ",";
        first_association = false;

        std::string expected_terminal_id;
        const std::string outcome = terminal_materialization_outcome(
            association, model, expected_terminal_id);
        const ConnectorCandidate* connector =
            find_connector_candidate(model, association.connector_id);
        const EndpointCandidate* selected_endpoint = association.endpoint_id.empty()
            ? nullptr : find_endpoint_candidate(model, association.endpoint_id);
        const ConnectorTerminal* terminal = expected_terminal_id.empty()
            ? nullptr : find_connector_terminal(model, expected_terminal_id);

        out << "\n    {\"association_id\":\"" << esc(association.id)
            << "\",\"connector_id\":\"" << esc(association.connector_id)
            << "\",\"pin_id\":\"" << esc(association.pin_id)
            << "\",\"pin_in_focus\":" << (pin_in_focus ? "true" : "false")
            << ",\"association_status\":\"" 
            << association_status_name(association.status)
            << "\",\"association_confidence\":\""
            << confidence_name(association.confidence)
            << "\",\"materialization_outcome\":\"" << outcome
            << "\",\"materialization_reason\":\"" << outcome
            << "\",\"expected_terminal_id\":\"" << esc(expected_terminal_id)
            << "\",\"materialized_terminal_present\":" << (terminal ? "true" : "false")
            << ",\"connector_reference_present\":" << (connector ? "true" : "false")
            << ",\"pin_reference_present\":" << (pin ? "true" : "false")
            << ",\"endpoint_reference_present\":"
            << (selected_endpoint ? "true" : "false");
        if (pin != nullptr)
            out << ",\"pin_confidence\":\"" << confidence_name(pin->confidence) << "\"";
        else
            out << ",\"pin_confidence\":null";

        if (selected_endpoint != nullptr) {
            const double dx = selected_endpoint->position.x - pin->position.x;
            const double dy = selected_endpoint->position.y - pin->position.y;
            out << ",\"selected_endpoint_snapshot_after_later_stages\":{"
                << "\"id\":\"" << esc(selected_endpoint->id)
                << "\",\"x\":" << selected_endpoint->position.x
                << ",\"y\":" << selected_endpoint->position.y
                << ",\"kind\":\"" << endpoint_kind(selected_endpoint->kind)
                << "\",\"terminal_role\":\""
                << terminal_role_name(selected_endpoint->terminal_role)
                << "\",\"confidence\":\"" << confidence_name(selected_endpoint->confidence)
                << "\",\"distance_to_pin_px\":" << std::hypot(dx, dy)
                << ",\"incident_edge_count\":" << selected_endpoint->incident_edges.size()
                << "}";
        } else {
            out << ",\"selected_endpoint_snapshot_after_later_stages\":null";
        }

        std::vector<std::string> pin_segment_ids;
        out << ",\"crossing_evidence\":[";
        bool first_crossing = true;
        if (pin != nullptr) {
        for (const auto& crossing_id : pin->conductor_crossing_evidence_ids) {
            const auto crossing_it = std::find_if(
                model.connector_conductor_crossing_evidence.begin(),
                model.connector_conductor_crossing_evidence.end(),
                [&](const ConnectorConductorCrossingEvidence& item) {
                    return item.id == crossing_id;
                });
            if (crossing_it == model.connector_conductor_crossing_evidence.end())
                continue;
            const auto& crossing = *crossing_it;
            if (!first_crossing) out << ",";
            first_crossing = false;
            out << "{\"id\":\"" << esc(crossing.id)
                << "\",\"referenced_by_association\":"
                << (contains_id(association.evidence_ids, crossing.id) ? "true" : "false")
                << ",\"conductor_segment_id\":\"" << esc(crossing.conductor_segment_id)
                << "\",\"interaction\":\"" << connector_interaction_name(crossing.interaction)
                << "\",\"confidence\":\"" << confidence_name(crossing.confidence)
                << "\",\"x\":" << crossing.crossing_point.x
                << ",\"y\":" << crossing.crossing_point.y
                << ",\"provenance_stage\":\"" << esc(crossing.provenance.stage) << "\"}";
            pin_segment_ids.push_back(crossing.conductor_segment_id);
        }
        }
        out << "]";

        // Reconstruct the exact evidence gates used by the association stage:
        // endpoint within 6 px and an incident topology edge on one of the
        // conductor segments observed at this pin.
        out << ",\"nearby_endpoint_candidates_within_6px\":[";
        bool first_endpoint = true;
        if (pin != nullptr) {
        for (const auto& endpoint : model.endpoint_candidates) {
            const double dx = endpoint.position.x - pin->position.x;
            const double dy = endpoint.position.y - pin->position.y;
            const double d = std::hypot(dx, dy);
            if (d > 6.0) continue;

            std::vector<const TopologyEdge*> supporting_edges;
            for (const auto& edge_id : endpoint.incident_edges) {
                const auto edge_it = std::find_if(
                    model.edges.begin(), model.edges.end(),
                    [&](const TopologyEdge& edge) { return edge.id == edge_id; });
                if (edge_it != model.edges.end() &&
                    contains_id(pin_segment_ids, edge_it->conductor_segment)) {
                    supporting_edges.push_back(&*edge_it);
                }
            }

            if (!first_endpoint) out << ",";
            first_endpoint = false;
            out << "{\"id\":\"" << esc(endpoint.id)
                << "\",\"x\":" << endpoint.position.x
                << ",\"y\":" << endpoint.position.y
                << ",\"distance_to_pin_px\":" << d
                << ",\"kind_after_later_stages\":\"" << endpoint_kind(endpoint.kind)
                << "\",\"terminal_role_after_later_stages\":\""
                << terminal_role_name(endpoint.terminal_role)
                << "\",\"confidence_after_later_stages\":\""
                << confidence_name(endpoint.confidence)
                << "\",\"is_association_endpoint\":"
                << (endpoint.id == association.endpoint_id ? "true" : "false")
                << ",\"conductor_supported_by_pin_crossing_segments\":"
                << (supporting_edges.empty() ? "false" : "true")
                << ",\"supporting_incident_edges\":[";
            for (std::size_t i = 0; i < supporting_edges.size(); ++i) {
                if (i) out << ",";
                const auto& edge = *supporting_edges[i];
                const auto from_node = std::find_if(
                    model.nodes.begin(), model.nodes.end(),
                    [&](const TopologyNode& node) { return node.id == edge.from_node; });
                const auto to_node = std::find_if(
                    model.nodes.begin(), model.nodes.end(),
                    [&](const TopologyNode& node) { return node.id == edge.to_node; });
                out << "{\"edge_id\":\"" << esc(edge.id)
                    << "\",\"conductor_segment_id\":\"" << esc(edge.conductor_segment)
                    << "\",\"from_node_id\":\"" << esc(edge.from_node)
                    << "\",\"to_node_id\":\"" << esc(edge.to_node) << "\"";
                if (from_node != model.nodes.end())
                    out << ",\"from_node_position\":{\"x\":" << from_node->position.x
                        << ",\"y\":" << from_node->position.y
                        << "},\"from_type\":\"" << node_type(from_node->type) << "\"";
                if (to_node != model.nodes.end())
                    out << ",\"to_node_position\":{\"x\":" << to_node->position.x
                        << ",\"y\":" << to_node->position.y
                        << "},\"to_type\":\"" << node_type(to_node->type) << "\"";
                out << "}";
            }
            out << "]}";
        }
        }
        out << "]}";
    }
    if (!first_association) out << "\n  ";
    out << "]";
}


void write_connector_terminals(
    std::ostream& out, const WireModel& model, const Focus& focus,
    bool include_all = false) {
    out << "[";
    bool first = true;
    for (const auto& terminal : model.connector_terminals) {
        if (!include_all && !point_in_focus(terminal.position, focus))
            continue;
        if (!first) out << ",";
        first = false;
        const char* status =
            terminal.status == ConnectorTerminalStatus::Resolved
                ? "resolved"
                : terminal.status == ConnectorTerminalStatus::Conflicted
                    ? "conflicted" : "unresolved";
        out << "\n    {\"id\":\"" << esc(terminal.id)
            << "\",\"connector_id\":\"" << esc(terminal.connector_id)
            << "\",\"endpoint_id\":\"" << esc(terminal.endpoint_id)
            << "\",\"x\":" << terminal.position.x
            << ",\"y\":" << terminal.position.y
            << ",\"terminal_name\":\"" << esc(terminal.terminal_name)
            << "\",\"function_label\":\"" << esc(terminal.function_label)
            << "\",\"wire_color\":\"" << esc(terminal.wire_color)
            << "\",\"confidence\":\"" << confidence_name(terminal.confidence)
            << "\",\"status\":\"" << status << "\"}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_raw_segments(
    std::ostream& out,
    const std::vector<ConductorSegment>& raw,
    const GeometryOwnershipArtifacts& ownership_result,
    const ConductorEvidenceArtifacts& evidence,
    const std::vector<ConductorSegment>& normalized_segments,
    const std::vector<ComponentCandidate>& components,
    const std::vector<ShapeRegion>& connector_regions,
    const std::vector<ConnectorBoundaryResolution>& boundary_resolutions,
    const Focus& focus,
    double tolerance) {

    out << "[";
    bool first = true;
    for (const auto& s : raw) {
        if (!segment_in_focus(s.geometry, focus)) continue;
        if (!first) out << ",";
        first = false;

        const auto* owner_rejection = rejected(ownership_result.rejected, s.id);
        const auto* evidence_rejection = rejected(evidence.rejected, s.id);
        const bool ownership_retained = has_segment(ownership_result.conductor_candidates, s.id);
        const bool evidence_retained = has_segment(evidence.accepted, s.id);
        bool normalized_exact_match = false;
        for (const auto& n : normalized_segments) {
            const bool direct =
                std::abs(n.geometry.a.x - s.geometry.a.x) < 1e-6 &&
                std::abs(n.geometry.a.y - s.geometry.a.y) < 1e-6 &&
                std::abs(n.geometry.b.x - s.geometry.b.x) < 1e-6 &&
                std::abs(n.geometry.b.y - s.geometry.b.y) < 1e-6;
            const bool reverse =
                std::abs(n.geometry.a.x - s.geometry.b.x) < 1e-6 &&
                std::abs(n.geometry.a.y - s.geometry.b.y) < 1e-6 &&
                std::abs(n.geometry.b.x - s.geometry.a.x) < 1e-6 &&
                std::abs(n.geometry.b.y - s.geometry.a.y) < 1e-6;
            if (direct || reverse) {
                normalized_exact_match = true;
                break;
            }
        }

        out << "\n    {\"id\":\"" << esc(s.id)
            << "\",\"a\":{\"x\":" << s.geometry.a.x
            << ",\"y\":" << s.geometry.a.y
            << "},\"b\":{\"x\":" << s.geometry.b.x
            << ",\"y\":" << s.geometry.b.y
            << "},\"length_px\":" << s.geometry.length()
            << ",\"thickness_px\":" << s.thickness_px
            << ",\"provenance_stage\":\"" << esc(s.provenance.stage)
            << "\",\"ownership_outcome\":\""
            << (owner_rejection ? "rejected" : ownership_retained ? "retained" : "not_found")
            << "\",\"ownership_rejection_reason\":\""
            << esc(owner_rejection ? owner_rejection->reason : std::string{})
            << "\",\"ownership_rejection_class\":\""
            << (owner_rejection ? rejection_class(owner_rejection->classification) : "none")
            << "\",\"ownership_associated_object_id\":\""
            << esc(owner_rejection ? owner_rejection->associated_object_id : std::string{})
            << "\",\"component_boundary_trace_ids\":[";

        bool first_owner = true;
        for (const auto& c : components) {
            if (!traces_boundary(s.geometry, c.bounds, tolerance)) continue;
            if (!first_owner) out << ",";
            first_owner = false;
            out << "\"" << esc(c.id) << "\"";
        }

        out << "],\"connector_geometry_boundary_trace_ids\":[";
        bool first_connector = true;
        bool connector_box_overlap = false;
        for (const auto& c : connector_regions) {
            const auto resolved = std::find_if(
                boundary_resolutions.begin(), boundary_resolutions.end(),
                [&](const ConnectorBoundaryResolution& boundary) {
                    return boundary.connector_region_id == c.id;
                });
            const BoundingBox effective_bounds =
                resolved != boundary_resolutions.end()
                    ? resolved->effective_bounds
                    : c.bounds;
            if (traces_boundary(s.geometry, effective_bounds, tolerance)) {
                if (!first_connector) out << ",";
                first_connector = false;
                out << "\"" << esc(c.id) << "\"";
            }
            const double sx0 = (std::min)(s.geometry.a.x, s.geometry.b.x);
            const double sx1 = (std::max)(s.geometry.a.x, s.geometry.b.x);
            const double sy0 = (std::min)(s.geometry.a.y, s.geometry.b.y);
            const double sy1 = (std::max)(s.geometry.a.y, s.geometry.b.y);
            if (sx0 <= effective_bounds.x + effective_bounds.width &&
                sx1 >= effective_bounds.x &&
                sy0 <= effective_bounds.y + effective_bounds.height &&
                sy1 >= effective_bounds.y) {
                connector_box_overlap = true;
            }
        }

        out << "],\"connector_geometry_bounds_overlap\":"
            << (connector_box_overlap ? "true" : "false")
            << ",\"evidence_outcome\":\""
            << (evidence_rejection ? "rejected" : evidence_retained ? "accepted" : "not_found")
            << "\",\"evidence_rejection_reason\":\""
            << esc(evidence_rejection ? evidence_rejection->reason : std::string{})
            << "\",\"exact_geometry_in_normalized_segments\":"
            << (normalized_exact_match ? "true" : "false")
            << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_normalized_segments(
    std::ostream& out, const std::vector<ConductorSegment>& segments, const Focus& focus) {
    out << "[";
    bool first = true;
    for (const auto& s : segments) {
        if (!segment_in_focus(s.geometry, focus)) continue;
        if (!first) out << ",";
        first = false;
        out << "\n    {\"id\":\"" << esc(s.id)
            << "\",\"a\":{\"x\":" << s.geometry.a.x
            << ",\"y\":" << s.geometry.a.y
            << "},\"b\":{\"x\":" << s.geometry.b.x
            << ",\"y\":" << s.geometry.b.y
            << "},\"thickness_px\":" << s.thickness_px << "}";
    }
    if (!first) out << "\n  ";
    out << "]";
}

void write_production_graph(
    std::ostream& out, const WireModel& model, const Focus& focus) {
    out << "{\n    \"nodes\":[";
    bool first = true;
    for (const auto& n : model.nodes) {
        if (n.position.x < focus.x || n.position.x > focus.x + focus.width ||
            n.position.y < focus.y || n.position.y > focus.y + focus.height) continue;
        if (!first) out << ",";
        first = false;
        out << "\n      {\"id\":\"" << esc(n.id) << "\",\"x\":" << n.position.x
            << ",\"y\":" << n.position.y << ",\"type\":\"" << node_type(n.type)
            << "\",\"electrically_connective\":"
            << (n.electrically_connective ? "true" : "false") << "}";
    }
    if (!first) out << "\n    ";
    out << "],\n    \"edges\":[";
    first = true;
    for (const auto& e : model.edges) {
        const auto from = std::find_if(model.nodes.begin(), model.nodes.end(),
            [&](const auto& n) { return n.id == e.from_node; });
        const auto to = std::find_if(model.nodes.begin(), model.nodes.end(),
            [&](const auto& n) { return n.id == e.to_node; });
        const auto segment = std::find_if(model.conductor_segments.begin(), model.conductor_segments.end(),
            [&](const auto& s) { return s.id == e.conductor_segment; });
        if (from == model.nodes.end() || to == model.nodes.end()) continue;
        const bool point_near =
            (from->position.x >= focus.x && from->position.x <= focus.x + focus.width &&
             from->position.y >= focus.y && from->position.y <= focus.y + focus.height) ||
            (to->position.x >= focus.x && to->position.x <= focus.x + focus.width &&
             to->position.y >= focus.y && to->position.y <= focus.y + focus.height);
        const bool segment_near =
            segment != model.conductor_segments.end() && segment_in_focus(segment->geometry, focus);
        if (!point_near && !segment_near) continue;
        if (!first) out << ",";
        first = false;
        out << "\n      {\"id\":\"" << esc(e.id)
            << "\",\"from_position\":{\"x\":" << from->position.x
            << ",\"y\":" << from->position.y << "},\"from_type\":\"" << node_type(from->type)
            << "\",\"to_position\":{\"x\":" << to->position.x
            << ",\"y\":" << to->position.y << "},\"to_type\":\"" << node_type(to->type)
            << "\",\"conductor_segment\":\"" << esc(e.conductor_segment) << "\"";
        if (segment != model.conductor_segments.end()) {
            out << ",\"segment_a\":{\"x\":" << segment->geometry.a.x
                << ",\"y\":" << segment->geometry.a.y
                << "},\"segment_b\":{\"x\":" << segment->geometry.b.x
                << ",\"y\":" << segment->geometry.b.y << "}";
        }
        out << "}";
    }
    if (!first) out << "\n    ";
    out << "],\n    \"endpoints\":[";
    first = true;
    for (const auto& e : model.endpoint_candidates) {
        if (e.position.x < focus.x || e.position.x > focus.x + focus.width ||
            e.position.y < focus.y || e.position.y > focus.y + focus.height) continue;
        if (!first) out << ",";
        first = false;
        out << "\n      {\"id\":\"" << esc(e.id)
            << "\",\"node_id\":\"" << esc(e.node_id)
            << "\",\"x\":" << e.position.x << ",\"y\":" << e.position.y
            << ",\"kind\":\"" << endpoint_kind(e.kind)
            << "\",\"incident_edge_count\":" << e.incident_edges.size() << "}";
    }
    if (!first) out << "\n    ";
    out << "]\n  }";
}

bool diagnostic_segment_rect_overlap(
    const Segment2D& segment, const BoundingBox& bounds,
    double& t0, double& t1) {
    const double dx = segment.b.x - segment.a.x;
    const double dy = segment.b.y - segment.a.y;
    t0 = 0.0;
    t1 = 1.0;
    auto clip = [&](double p, double q) {
        if (std::abs(p) < 1e-9)
            return q >= 0.0;
        const double r = q / p;
        if (p < 0.0) {
            if (r > t1) return false;
            if (r > t0) t0 = r;
        } else {
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
        return true;
    };
    return clip(-dx, segment.a.x - bounds.x) &&
           clip(dx, bounds.x + bounds.width - segment.a.x) &&
           clip(-dy, segment.a.y - bounds.y) &&
           clip(dy, bounds.y + bounds.height - segment.a.y) &&
           t1 >= t0;
}

bool diagnostic_point_in_bounds(
    const Point2D& point, const BoundingBox& bounds, double margin) {
    return point.x >= bounds.x - margin &&
           point.x <= bounds.x + bounds.width + margin &&
           point.y >= bounds.y - margin &&
           point.y <= bounds.y + bounds.height + margin;
}

void write_connector_observation_gate_audit(
    std::ostream& out,
    const std::vector<ShapeRegion>& regions,
    const GeometryOwnershipArtifacts& ownership,
    const WireModel& model) {
    out << "[";
    bool first_region = true;
    constexpr double kMinimumOverlapPx = 3.0;
    constexpr double kBoundaryMarginPx = 2.0;
    constexpr std::size_t kMinimumInteractions = 2;

    for (const auto& region : regions) {
        const auto boundary = std::find_if(
            ownership.connector_boundaries.begin(),
            ownership.connector_boundaries.end(),
            [&](const ConnectorBoundaryResolution& item) {
                return item.connector_region_id == region.id;
            });
        const BoundingBox effective_bounds =
            boundary == ownership.connector_boundaries.end()
                ? region.bounds : boundary->effective_bounds;
        const bool boundary_reconstructed =
            boundary != ownership.connector_boundaries.end() &&
            boundary->reconstructed_from_rectangular_strokes;
        const std::string connector_id = stable_id("connector", region.id);
        const bool candidate_present = std::any_of(
            model.connector_candidates.begin(), model.connector_candidates.end(),
            [&](const ConnectorCandidate& candidate) {
                return candidate.id == connector_id;
            });

        std::size_t legacy_count = 0;
        std::size_t effective_strict_count = 0;
        std::size_t current_count = 0;
        std::size_t legacy_to_current_additions = 0;
        std::ostringstream newly_admitted;
        newly_admitted << "[";
        bool first_added = true;

        for (const auto& segment : model.conductor_segments) {
            double old_t0 = 0.0, old_t1 = 0.0;
            const bool intersects_detected = diagnostic_segment_rect_overlap(
                segment.geometry, region.bounds, old_t0, old_t1);
            const double old_overlap = intersects_detected
                ? segment.geometry.length() * (old_t1 - old_t0) : 0.0;
            const bool legacy_accepted =
                intersects_detected && old_overlap >= kMinimumOverlapPx;
            if (legacy_accepted) ++legacy_count;

            double new_t0 = 0.0, new_t1 = 0.0;
            const bool intersects_effective = diagnostic_segment_rect_overlap(
                segment.geometry, effective_bounds, new_t0, new_t1);
            const double new_overlap = intersects_effective
                ? segment.geometry.length() * (new_t1 - new_t0) : 0.0;
            const bool strict_effective_accepted =
                intersects_effective && new_overlap >= kMinimumOverlapPx;
            if (strict_effective_accepted) ++effective_strict_count;

            const bool a_near = diagnostic_point_in_bounds(
                segment.geometry.a, effective_bounds, kBoundaryMarginPx);
            const bool b_near = diagnostic_point_in_bounds(
                segment.geometry.b, effective_bounds, kBoundaryMarginPx);
            const bool termination_contact = a_near != b_near;
            const bool current_accepted = intersects_effective &&
                (new_overlap >= kMinimumOverlapPx || termination_contact);
            if (current_accepted) ++current_count;

            if (current_accepted && !legacy_accepted) {
                ++legacy_to_current_additions;
                if (!first_added) newly_admitted << ",";
                first_added = false;
                std::string reason;
                if (!intersects_detected || new_overlap >= kMinimumOverlapPx) {
                    reason = "effective_bounds_changed";
                }
                if (new_overlap < kMinimumOverlapPx && termination_contact) {
                    if (!reason.empty()) reason += "+";
                    reason += "termination_contact_exception";
                }
                newly_admitted
                    << "{\"segment_id\":\"" << esc(segment.id)
                    << "\",\"detected_bounds_intersects\":"
                    << (intersects_detected ? "true" : "false")
                    << ",\"detected_bounds_overlap_px\":" << old_overlap
                    << ",\"effective_bounds_intersects\":"
                    << (intersects_effective ? "true" : "false")
                    << ",\"effective_bounds_overlap_px\":" << new_overlap
                    << ",\"a_near_effective_bounds\":"
                    << (a_near ? "true" : "false")
                    << ",\"b_near_effective_bounds\":"
                    << (b_near ? "true" : "false")
                    << ",\"termination_contact\":"
                    << (termination_contact ? "true" : "false")
                    << ",\"admission_reason\":\"" << esc(reason) << "\"}";
            }
        }
        newly_admitted << "]";

        if (!first_region) out << ",";
        first_region = false;
        out << "\n  {\"connector_region_id\":\"" << esc(region.id)
            << "\",\"production_connector_id\":\"" << esc(connector_id)
            << "\",\"detected_bounds\":";
        write_box(out, region.bounds);
        out << ",\"effective_bounds\":";
        write_box(out, effective_bounds);
        out << ",\"boundary_reconstructed_from_rectangular_strokes\":"
            << (boundary_reconstructed ? "true" : "false")
            << ",\"production_candidate_present\":"
            << (candidate_present ? "true" : "false")
            << ",\"counterfactual_reuses_current_normalized_segments\":true"
            << ",\"minimum_overlap_px\":" << kMinimumOverlapPx
            << ",\"termination_endpoint_margin_px\":" << kBoundaryMarginPx
            << ",\"minimum_interactions\":" << kMinimumInteractions
            << ",\"legacy_gate_interaction_count\":" << legacy_count
            << ",\"effective_bounds_strict_gate_interaction_count\":"
            << effective_strict_count
            << ",\"current_gate_interaction_count\":" << current_count
            << ",\"legacy_gate_would_meet_minimum\":"
            << (legacy_count >= kMinimumInteractions ? "true" : "false")
            << ",\"effective_bounds_strict_gate_would_meet_minimum\":"
            << (effective_strict_count >= kMinimumInteractions ? "true" : "false")
            << ",\"current_gate_would_meet_minimum\":"
            << (current_count >= kMinimumInteractions ? "true" : "false")
            << ",\"newly_admitted_segment_count\":" << legacy_to_current_additions
            << ",\"newly_admitted_segments\":" << newly_admitted.str() << "}";
    }
    if (!first_region) out << "\n";
    out << "]";
}


} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 7) {
            std::cerr << "Usage: dx-audit-connector-boundary-ownership "
                      << "<source_image> <output_json> "
                      << "[focus_x focus_y focus_width focus_height]\n";
            return 2;
        }

        const fs::path source_path = argv[1];
        const fs::path output_path = argv[2];
        Focus focus;
        if (argc == 7) {
            focus.x = std::stod(argv[3]);
            focus.y = std::stod(argv[4]);
            focus.width = std::stod(argv[5]);
            focus.height = std::stod(argv[6]);
            if (focus.width <= 0.0 || focus.height <= 0.0)
                throw std::runtime_error("Focus width and height must be positive.");
        }

        const cv::Mat source = ImageLoader::load(source_path.string());
        if (source.empty())
            throw std::runtime_error("Unable to load source image: " + source_path.string());
        const cv::Mat normalized = ImageNormalizer::normalize(source);
        if (normalized.empty())
            throw std::runtime_error("ImageNormalizer returned an empty image.");

        ExtractionConfig config;
        ShapeDetector shape_detector(config.shapes);
        const ShapeDetectionArtifacts shapes =
            shape_detector.detect(normalized, source_path.string(), 0);
        TextRegionDetector text_detector(config.text);
        const TextDetectionArtifacts text =
            text_detector.detect(normalized, source_path.string(), 0);

        cv::Mat exclusion = shapes.exclusion_mask.clone();
        if (exclusion.empty())
            exclusion = cv::Mat::zeros(normalized.size(), CV_8UC1);
        if (!text.exclusion_mask.empty())
            cv::bitwise_or(exclusion, text.exclusion_mask, exclusion);

        MorphologyWireDetector morphology(config.morphology);
        const DetectionArtifacts detected =
            morphology.detect(normalized, source_path.string(), 0, exclusion);

        ComponentCandidateClassifier component_classifier;
        CircleContextClassifier circle_context_classifier;
        const auto context_candidates = circle_context_classifier.classify(
            component_classifier.classify(shapes), detected.conductor_segments);
        DiagramFurnitureClassifier furniture_classifier(config.diagram_furniture);
        const auto components = furniture_classifier.classify(context_candidates);

        // Match the canonical pipeline: connector geometry is available
        // before ownership, where line-supported housing-frame geometry is
        // resolved by the exact production classifier.
        ConnectorGeometryDetector connector_detector(config.connectors);
        const auto connector_regions =
            connector_detector.detect(normalized, source_path.string(), 0);
        GeometryOwnershipClassifier ownership_classifier(config.geometry_ownership);
        const auto ownership = ownership_classifier.classify(
            detected.conductor_segments, components, text.regions,
            connector_regions.regions);
        ConductorEvidenceEvaluator evaluator(config.conductor_evidence);
        const auto evidence = evaluator.evaluate(ownership.conductor_candidates, normalized);
        ConductorNormalizer normalizer(config.geometry);
        const auto normalized_segments = normalizer.normalize(evidence.accepted);

        ExtractionPipeline pipeline(config);
        const WireModel model = pipeline.run(source_path.string(), source_path.string());

        if (!output_path.parent_path().empty())
            fs::create_directories(output_path.parent_path());
        std::ofstream out(output_path);
        if (!out)
            throw std::runtime_error("Unable to open report: " + output_path.string());
        out << std::setprecision(12);
        out << "{\n"
            << "  \"ap\":\"AP-DIAG-049\",\n"
            << "  \"status\":\"diagnostic_only\",\n"
            << "  \"production_logic_modified\":false,\n"
            << "  \"production_logic_modified_by_ap_diag_fix_012_branch\":true,\n"
            << "  \"source\":{\"path\":\"" << esc(source_path.string())
            << "\",\"source_id\":\"" << esc(source_path.string())
            << "\",\"width\":" << normalized.cols << ",\"height\":" << normalized.rows << "},\n"
            << "  \"focus_region\":{\"x\":" << focus.x << ",\"y\":" << focus.y
            << ",\"width\":" << focus.width << ",\"height\":" << focus.height << "},\n"
            << "  \"stage_order\":{\"connector_geometry_precedes_ownership\":true,"
            << "\"ownership_precedes_topology\":true,"
            << "\"connector_geometry_detector_runs_after_topology_in_production\":false,"
            << "\"diagnostic_runs_connector_detector_early_for_comparison\":true,"
            << "\"ownership_boundary_tolerance_px\":"
            << config.geometry_ownership.component_boundary_tolerance_px << "},\n"
            << "  \"population\":{\"shape_regions_total\":" << shapes.regions.size()
            << ",\"component_candidates_total\":" << components.size()
            << ",\"text_regions_total\":" << text.regions.size()
            << ",\"raw_morphology_segments_total\":" << detected.conductor_segments.size()
            << ",\"ownership_retained_total\":" << ownership.conductor_candidates.size()
            << ",\"ownership_rejected_total\":" << ownership.rejected.size()
            << ",\"evidence_accepted_total\":" << evidence.accepted.size()
            << ",\"evidence_rejected_total\":" << evidence.rejected.size()
            << ",\"normalized_segments_total\":" << normalized_segments.size()
            << ",\"connector_geometry_regions_total\":" << connector_regions.regions.size()
            << ",\"production_wires\":" << model.wires.size()
            << ",\"production_endpoints\":" << model.endpoint_candidates.size()
            << ",\"production_nodes\":" << model.nodes.size()
            << ",\"production_edges\":" << model.edges.size()
            << ",\"production_connector_candidates\":" << model.connector_candidates.size()
            << ",\"production_connector_pins\":" << model.connector_pins.size()
            << ",\"production_connector_crossings\":" << model.connector_conductor_crossing_evidence.size()
            << ",\"production_connector_terminal_associations\":" << model.connector_terminal_associations.size()
            << ",\"production_connector_terminals\":" << model.connector_terminals.size();
        const ConnectorAssociationAuditCounts association_counts =
            count_connector_associations(model);
        out << ",\"production_connector_associations_resolved\":"
            << association_counts.resolved
            << ",\"production_connector_associations_unresolved\":"
            << association_counts.unresolved
            << ",\"production_connector_associations_conflicted\":"
            << association_counts.conflicted
            << ",\"production_connector_associations_materialized\":"
            << association_counts.materialized
            << ",\"production_connector_association_materialization_outcomes\":{"
            << "\"association_status_unresolved\":"
            << association_counts.association_status_unresolved
            << ",\"association_status_conflicted\":"
            << association_counts.association_status_conflicted
            << ",\"endpoint_id_empty\":" << association_counts.endpoint_id_empty
            << ",\"connector_reference_missing\":"
            << association_counts.connector_reference_missing
            << ",\"pin_reference_missing\":" << association_counts.pin_reference_missing
            << ",\"endpoint_reference_missing\":"
            << association_counts.endpoint_reference_missing
            << ",\"resolved_association_terminal_missing\":"
            << association_counts.resolved_association_terminal_missing
            << "}},\n"
            << "  \"shape_regions_near_focus\":";
        write_shapes(out, shapes.regions, focus);
        out << ",\n  \"component_candidates_near_focus\":";
        write_components(out, components, focus);
        out << ",\n  \"text_regions_near_focus\":";
        write_text(out, text.regions, focus);
        out << ",\n  \"connector_geometry_regions_near_focus\":";
        write_connector_regions(out, connector_regions.regions, focus);
        out << ",\n  \"connector_boundary_resolutions_near_focus\":";
        write_connector_boundary_resolutions(
            out, ownership.connector_boundaries, focus);
        out << ",\n  \"production_connector_candidates_near_focus\":";
        write_connector_candidates(out, model.connector_candidates, focus);
        out << ",\n  \"production_connector_pins_near_focus\":";
        write_connector_pins(out, model, focus);
        out << ",\n  \"production_connector_crossings_near_focus\":";
        write_connector_crossings(out, model, focus);
        out << ",\n  \"production_connector_terminal_associations_near_focus\":";
        write_connector_associations(out, model, focus);
        out << ",\n  \"production_connector_terminals_near_focus\":";
        write_connector_terminals(out, model, focus);
        out << ",\n  \"connector_geometry_regions_all\":";
        write_connector_regions(out, connector_regions.regions, focus, true);
        out << ",\n  \"connector_boundary_resolutions_all\":";
        write_connector_boundary_resolutions(
            out, ownership.connector_boundaries, focus, true);
        out << ",\n  \"production_connector_candidates_all\":";
        write_connector_candidates(out, model.connector_candidates, focus, true);
        out << ",\n  \"production_connector_pins_all\":";
        write_connector_pins(out, model, focus, true);
        out << ",\n  \"production_connector_crossings_all\":";
        write_connector_crossings(out, model, focus, true);
        out << ",\n  \"production_connector_terminal_associations_all\":";
        write_connector_associations(out, model, focus, true);
        out << ",\n  \"production_connector_terminals_all\":";
        write_connector_terminals(out, model, focus, true);
        out << ",\n  \"connector_observation_gate_audit_all\":";
        write_connector_observation_gate_audit(
            out, connector_regions.regions, ownership, model);
        out << ",\n  \"connector_terminal_materialization_audit\":";
        write_connector_association_audit(out, model, focus);
        out << ",\n  \"raw_morphology_segments_near_focus\":";
        write_raw_segments(
            out, detected.conductor_segments, ownership, evidence,
            normalized_segments, components, connector_regions.regions,
            ownership.connector_boundaries, focus,
            config.geometry_ownership.component_boundary_tolerance_px);
        out << ",\n  \"production_normalized_segments_near_focus\":";
        write_normalized_segments(out, model.conductor_segments, focus);
        out << ",\n  \"production_topology_near_focus\":";
        write_production_graph(out, model, focus);
        out << "\n}\n";
        out.close();
        if (!out)
            throw std::runtime_error("Failed while writing report.");

        std::size_t raw_count = 0, kept_count = 0, rejected_count = 0;
        for (const auto& s : detected.conductor_segments) {
            if (!segment_in_focus(s.geometry, focus)) continue;
            ++raw_count;
            kept_count += has_segment(ownership.conductor_candidates, s.id) ? 1U : 0U;
            rejected_count += rejected(ownership.rejected, s.id) != nullptr ? 1U : 0U;
        }
        std::cout << "[AP-DIAG-049] Source: " << source_path.string() << "\n"
                  << "[AP-DIAG-049] Dimensions: " << normalized.cols << "x" << normalized.rows << "\n"
                  << "[AP-DIAG-049] Focus region: (" << focus.x << "," << focus.y
                  << "," << focus.width << "," << focus.height << ")\n"
                  << "[AP-DIAG-049] Raw/retained/rejected segments near focus: "
                  << raw_count << "/" << kept_count << "/" << rejected_count << "\n"
                  << "[AP-DIAG-049] Shape/component/connector regions: "
                  << shapes.regions.size() << "/" << components.size() << "/"
                  << connector_regions.regions.size() << "\n"
                  << "[AP-DIAG-049] Report: " << output_path.string() << "\n"
                  << "[AP-DIAG-049] Diagnostic only; production source unchanged.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[AP-DIAG-049] ERROR: " << ex.what() << "\n";
        return 1;
    }
}
