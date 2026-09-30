// AP-DIAG-018 regression coverage for the source-to-object reconciliation
// artifact. Diagnostic/reporting only; this test does not implement detector
// behavior or reproduce human visual classification.
//
// Invariants:
//   1. Every referenced object_id and related_object_id resolves to a stable
//      ID in the same-run extraction_audit.json.
//   2. Machine-defined reconciliation populations exactly match the
//      populations derivable from that same audit.
//   3. No duplicate reconciliation IDs; classification counts and summary
//      totals agree.
//   4. Actual artifact records are in stable object_id order.
//   5. Visual sample IDs are unique and resolve to known objects.
//   6. The 27 unresolved-Wire subset is structurally validated without
//      pretending the test can reproduce human visual judgment.
//
// The test is skipped when diagnostic artifacts are absent because they are
// checked-in AP outputs rather than build products.

#include <opencv2/core.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {
std::string slurp(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::set<std::string> collect_stable_ids(const cv::FileNode& objects) {
    std::set<std::string> ids;
    for (const auto& collection : objects) {
        if (!collection.isSeq()) continue;
        for (const auto& item : collection) {
            const cv::FileNode id_node = item["id"];
            if (!id_node.empty()) ids.insert((std::string)id_node);
        }
    }
    return ids;
}

const char* kGroups[] = {
    "endpoint_reconciliation",
    "topology_edge_reconciliation",
    "conductor_segment_reconciliation",
    "component_reconciliation",
    "net_endpoint_reconciliation",
    "wire_reconciliation",
    "connector_reconciliation",
    "geometric_endpoint_warning_reconciliation",
};

const char* kSummaryKeys[] = {
    "zero_wire_endpoints_total",
    "unowned_topology_edges_total",
    "topology_only_conductor_segments_total",
    "components_without_terminal_evidence_total",
    "endpoints_outside_nets_total",
    "fully_unresolved_wires_total",
    "connectors_total",
    "geometric_endpoint_warnings_total",
};
}

int main() {
    const fs::path root = fs::path(DX_WIRE_SOURCE_DIR);
    const fs::path audit_path = root / "artifacts" / "audit" / "extraction_audit.json";
    const fs::path reconciliation_path = root / "artifacts" / "audit" / "source_object_reconciliation.json";

    if (!fs::exists(audit_path) || !fs::exists(reconciliation_path)) {
        std::cout << "AP-DIAG-018 artifacts not present in this build tree; skipping.\n";
        return 0;
    }

    cv::FileStorage audit_fs(
        slurp(audit_path), cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);
    assert(audit_fs.isOpened());
    const std::set<std::string> known_ids = collect_stable_ids(audit_fs["objects"]);
    assert(!known_ids.empty());

    cv::FileStorage rec_fs(
        slurp(reconciliation_path), cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);
    assert(rec_fs.isOpened());

    std::map<std::string, std::set<std::string>> expected;
    const cv::FileNode objects = audit_fs["objects"];
    std::set<std::string> all_endpoint_ids, wire_endpoint_ids, owned_edge_ids, owned_segment_ids;
    std::set<std::string> components_with_terminal_evidence, net_endpoint_ids;
    std::set<std::string> geometric_warning_wire_ids, all_wire_ids;
    std::map<std::string, std::string> endpoint_kind;

    for (const auto& item : objects["endpoint_candidates"]) {
        const std::string id = (std::string)item["id"];
        all_endpoint_ids.insert(id);
        endpoint_kind[id] = (std::string)item["kind"];
    }

    for (const auto& item : objects["wires"]) {
        const std::string id = (std::string)item["id"];
        all_wire_ids.insert(id);
        const std::string a = (std::string)item["start_endpoint"];
        const std::string b = (std::string)item["end_endpoint"];
        wire_endpoint_ids.insert(a);
        wire_endpoint_ids.insert(b);
        if (endpoint_kind[a] == "geometric" && endpoint_kind[b] == "geometric")
            geometric_warning_wire_ids.insert(id);
        for (const auto& x : item["topology_edges"])
            owned_edge_ids.insert((std::string)x);
        for (const auto& x : item["conductor_segments"])
            owned_segment_ids.insert((std::string)x);
    }

    for (const auto& id : all_endpoint_ids)
        if (!wire_endpoint_ids.count(id)) expected["endpoint_reconciliation"].insert(id);

    for (const auto& item : objects["topology_edges"]) {
        const std::string id = (std::string)item["id"];
        if (!owned_edge_ids.count(id))
            expected["topology_edge_reconciliation"].insert(id);
    }

    for (const auto& item : objects["conductor_segments"]) {
        const std::string id = (std::string)item["id"];
        if (!owned_segment_ids.count(id))
            expected["conductor_segment_reconciliation"].insert(id);
    }

    std::set<std::string> component_ids;
    for (const auto& item : objects["components"])
        component_ids.insert((std::string)item["id"]);
    for (const auto& item : objects["terminal_candidates"])
        components_with_terminal_evidence.insert((std::string)item["component_candidate_id"]);
    for (const auto& id : component_ids)
        if (!components_with_terminal_evidence.count(id))
            expected["component_reconciliation"].insert(id);

    for (const auto& net : objects["electrical_nets"])
        for (const auto& x : net["endpoint_ids"])
            net_endpoint_ids.insert((std::string)x);
    for (const auto& id : all_endpoint_ids)
        if (!net_endpoint_ids.count(id))
            expected["net_endpoint_reconciliation"].insert(id);

    for (const auto& item : objects["connectors"])
        expected["connector_reconciliation"].insert((std::string)item["id"]);
    expected["geometric_endpoint_warning_reconciliation"] = geometric_warning_wire_ids;

    for (const char* group : kGroups) {
        const cv::FileNode records = rec_fs[group];
        assert(records.isSeq());
        std::set<std::string> actual_ids;
        std::map<std::string, int> tally;
        std::string previous_id;

        for (const auto& record : records) {
            const std::string object_id = (std::string)record["object_id"];
            assert(known_ids.count(object_id) > 0);
            assert(actual_ids.insert(object_id).second);
            if (!previous_id.empty()) assert(previous_id < object_id);
            previous_id = object_id;
            for (const auto& rel : record["related_object_ids"])
                assert(known_ids.count((std::string)rel) > 0);
            ++tally[(std::string)record["classification"]];
        }

        // wire_reconciliation (the 27 fully-unresolved wires) cannot be
        // exactly re-derived from extraction_audit.json alone: "fully
        // unresolved" depends on per-wire WireSemanticResolution status
        // (wire_color/function/component/connector/electrical_net), which
        // this artifact does not expose - only the wires' own start/end
        // endpoints and geometry. Exact membership for every other group
        // above IS fully derivable from this same file, so only this one
        // group is exempted; its population is instead checked below by
        // its actual structural signature (both endpoints geometric) and
        // its fixed size, which is what the source data can support
        // without pretending to reproduce a judgment this file can't hold.
        if (std::string(group) != "wire_reconciliation") {
            assert(actual_ids == expected[group]);
        }

        const cv::FileNode counts = rec_fs["classification_counts"][group];
        assert(counts.isMap());
        int declared_total = 0;
        for (const auto& kv : counts) {
            const std::string name = kv.name();
            const int count = (int)kv;
            assert(tally.count(name) > 0 && tally[name] == count);
            declared_total += count;
        }
        assert(declared_total == static_cast<int>(records.size()));
    }

    const cv::FileNode unresolved = rec_fs["wire_reconciliation"];
    assert(unresolved.size() == 27);
    for (const auto& record : unresolved) {
        const std::string id = (std::string)record["object_id"];
        assert(all_wire_ids.count(id) > 0);
        for (const auto& w : objects["wires"]) {
            if ((std::string)w["id"] == id) {
                assert(endpoint_kind[(std::string)w["start_endpoint"]] == "geometric");
                assert(endpoint_kind[(std::string)w["end_endpoint"]] == "geometric");
                break;
            }
        }
    }

    const cv::FileNode summary = rec_fs["summary"];
    for (std::size_t i = 0; i < sizeof(kGroups) / sizeof(kGroups[0]); ++i)
        assert((int)summary[kSummaryKeys[i]] == static_cast<int>(rec_fs[kGroups[i]].size()));

    const cv::FileNode samples = rec_fs["evidence"]["source_visual_sample_object_ids"];
    std::set<std::string> sample_ids;
    for (const auto& item : samples) {
        const std::string id = (std::string)item;
        assert(sample_ids.insert(id).second);
        assert(known_ids.count(id) > 0);
    }
    assert(sample_ids.size() == 19);

    std::cout << "source object reconciliation invariants passed\n";
    return 0;
}
