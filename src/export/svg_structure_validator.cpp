#include "eke_dx_wire/export/svg_structure_validator.hpp"

#include <regex>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

using IdSet = std::unordered_set<std::string>;

void add(SvgValidationReport& report, std::string code, std::string detail) {
    report.issues.push_back(SvgValidationIssue{std::move(code), std::move(detail)});
}

// Maps a data-*-id attribute name to the id set it must resolve against.
// A name absent from this table is not a reference this validator checks
// (e.g. data-object-type, data-status carry no id to resolve).
const std::unordered_map<std::string, const IdSet*>& reference_table(
    const IdSet& component_ids,
    const IdSet& connector_ids,
    const IdSet& wire_ids,
    const IdSet& node_ids,
    const IdSet& terminal_ids,
    const IdSet& endpoint_ids,
    const IdSet& text_region_ids,
    const IdSet& net_ids,
    const IdSet& segment_ids) {
    static std::unordered_map<std::string, const IdSet*> table;
    table.clear();
    table["data-component-id"] = &component_ids;
    table["data-connector-id"] = &connector_ids;
    table["data-wire-id"] = &wire_ids;
    table["data-node-id"] = &node_ids;
    table["data-terminal-id"] = &terminal_ids;
    table["data-endpoint-id"] = &endpoint_ids;
    table["data-start-endpoint"] = &endpoint_ids;
    table["data-end-endpoint"] = &endpoint_ids;
    table["data-text-region-id"] = &text_region_ids;
    table["data-net-id"] = &net_ids;
    table["data-electrical-net-id"] = &net_ids;
    table["data-conductor-segment-id"] = &segment_ids;
    return table;
}

} // namespace

SvgValidationReport SvgStructureValidator::validate(
    const std::string& svg,
    const EngineeringDiagram& diagram,
    const WireModel& model) {

    SvgValidationReport report;

    // ---- root / no-raster-fallback checks ------------------------------
    if (svg.find("<svg") == std::string::npos) {
        add(report, "SVG-ROOT-MISSING", "no <svg root element found");
    }
    if (svg.find("</svg>") == std::string::npos) {
        add(report, "SVG-ROOT-UNCLOSED", "no closing </svg> found");
    }
    if (svg.find("<image") != std::string::npos) {
        add(report, "SVG-RASTER-FALLBACK", "a raster <image> element was found; output must be structured");
    }

    // ---- known-id sets, built from the diagram/model (never guessed) --
    IdSet component_ids;
    for (const auto& c : diagram.components) component_ids.insert(c.component_id);
    IdSet connector_ids;
    for (const auto& c : diagram.connectors) connector_ids.insert(c.connector_id);
    IdSet wire_ids;
    for (const auto& w : diagram.wires) wire_ids.insert(w.wire_id);
    IdSet node_ids;
    for (const auto& s : diagram.splices) node_ids.insert(s.node_id);
    for (const auto& id : diagram.crossing_node_ids) node_ids.insert(id);
    IdSet terminal_ids;
    for (const auto& t : model.terminal_candidates) terminal_ids.insert(t.id);
    for (const auto& t : model.connector_terminals) terminal_ids.insert(t.id);
    IdSet endpoint_ids;
    for (const auto& e : model.endpoint_candidates) endpoint_ids.insert(e.id);
    IdSet text_region_ids;
    for (const auto& l : diagram.labels) text_region_ids.insert(l.text_region_id);
    IdSet net_ids;
    for (const auto& n : diagram.electrical_nets) net_ids.insert(n.net_id);
    IdSet segment_ids;
    for (const auto& s : model.conductor_segments) segment_ids.insert(s.id);

    const auto& refs = reference_table(
        component_ids, connector_ids, wire_ids, node_ids, terminal_ids,
        endpoint_ids, text_region_ids, net_ids, segment_ids);

    // ---- element id uniqueness ------------------------------------------
    // Only a genuine element id=".." attribute, never a data-*-id
    // attribute that merely happens to end in "id=".
    static const std::regex element_id_pattern(R"re((?:^|[^\w-])id="([^"]*)")re");
    std::unordered_map<std::string, int> id_counts;
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), element_id_pattern);
         it != std::sregex_iterator(); ++it) {
        const std::string id = (*it)[1].str();
        ++id_counts[id];
        ++report.element_id_count;
    }
    for (const auto& [id, count] : id_counts) {
        if (count > 1) {
            add(report, "SVG-DUPLICATE-ELEMENT-ID", "element id rendered more than once: " + id);
        }
    }

    // ---- data-*-id reference resolution ----------------------------------
    static const std::regex data_ref_pattern(R"re((data-[a-z-]*-?id|data-start-endpoint|data-end-endpoint)="([^"]*)")re");
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), data_ref_pattern);
         it != std::sregex_iterator(); ++it) {
        const std::string attr = (*it)[1].str();
        const std::string value = (*it)[2].str();
        if (value.empty()) continue;
        const auto table_it = refs.find(attr);
        if (table_it == refs.end()) continue; // not a reference this validator resolves
        if (table_it->second->find(value) == table_it->second->end()) {
            add(report, "SVG-DANGLING-REFERENCE", attr + "=\"" + value + "\" does not resolve to a known object");
        }
    }

    // ---- no guessed symbol-family glyph for unresolved/conflicted -------
    static const std::regex symbol_block_pattern(
        R"re(<g class="symbol"[^>]*data-symbol-family="([^"]*)"[^>]*data-symbol-family-status="([^"]*)")re");
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), symbol_block_pattern);
         it != std::sregex_iterator(); ++it) {
        const std::string status = (*it)[2].str();
        if (status != "resolved") {
            add(report, "SVG-UNGUESSED-SYMBOL-FAMILY-VIOLATION",
                "a data-symbol-family value was rendered for status=" + status);
        }
    }

    return report;
}

} // namespace eke::dx::wire
