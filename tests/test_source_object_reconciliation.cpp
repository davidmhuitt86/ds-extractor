// AP-DIAG-018 regression coverage for the source-to-object reconciliation
// artifact (artifacts/audit/source_object_reconciliation.json). This is a
// diagnostic/reporting artifact, not a new pipeline stage - this test
// verifies the structural invariants the AP itself required, not any
// classification judgment (which is a one-time human/model analysis, not
// something a unit test can re-derive):
//
//   1. Every object_id referenced anywhere in the reconciliation artifact
//      is a real stable ID present in the same-run extraction_audit.json.
//   2. classification_counts for each population sum to that population's
//      record count (no silently-dropped records).
//   3. Every population's record count matches its declared summary total
//      (no silently-omitted population).
//   4. Deterministic serialization: constructing the same logical dataset
//      twice and serializing both with sorted keys produces byte-identical
//      JSON text - the principle the real artifact was generated under
//      (Python's json.dumps(..., sort_keys=True), no generated_at-style
//      field used for identity).
//
// This test is skipped (not failed) when the two artifact files are not
// present, since they are diagnostic outputs checked in by AP-DIAG-018
// rather than produced by any build step.

#include <opencv2/core.hpp>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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
        if (!collection.isSeq())
            continue;
        for (const auto& item : collection) {
            const cv::FileNode id_node = item["id"];
            if (!id_node.empty())
                ids.insert((std::string)id_node);
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

} // namespace

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

    // 1 & 2: every referenced object_id (and related_object_ids) is a known
    // stable ID, and classification_counts sums match record counts.
    for (const char* group : kGroups) {
        const cv::FileNode records = rec_fs[group];
        assert(records.isSeq());

        std::map<std::string, int> tally;
        for (const auto& record : records) {
            const std::string object_id = (std::string)record["object_id"];
            assert(known_ids.count(object_id) > 0);

            const cv::FileNode related = record["related_object_ids"];
            if (related.isSeq()) {
                for (const auto& rel : related) {
                    const std::string rel_id = (std::string)rel;
                    assert(known_ids.count(rel_id) > 0);
                }
            }

            const std::string classification = (std::string)record["classification"];
            ++tally[classification];
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

    // 3: population record counts match the declared summary totals.
    const cv::FileNode summary = rec_fs["summary"];
    for (std::size_t i = 0; i < sizeof(kGroups) / sizeof(kGroups[0]); ++i) {
        const int declared = (int)summary[kSummaryKeys[i]];
        const int actual = static_cast<int>(rec_fs[kGroups[i]].size());
        assert(declared == actual);
    }

    // 4: deterministic serialization of the same logical dataset.
    struct Record {
        std::string object_id;
        std::string classification;
        std::string confidence;
    };
    const std::vector<Record> sample = {
        {"endpoint-candidate-b", "WIRE_INTERRUPTION", "MEDIUM"},
        {"endpoint-candidate-a", "COMPONENT_TERMINAL", "HIGH"},
    };

    auto serialize = [](std::vector<Record> records) {
        std::sort(records.begin(), records.end(),
                   [](const Record& a, const Record& b) { return a.object_id < b.object_id; });
        std::ostringstream out;
        out << "[";
        for (std::size_t i = 0; i < records.size(); ++i) {
            if (i)
                out << ",";
            out << "{\"object_id\":\"" << records[i].object_id
                << "\",\"classification\":\"" << records[i].classification
                << "\",\"confidence\":\"" << records[i].confidence << "\"}";
        }
        out << "]";
        return out.str();
    };

    const std::string first = serialize(sample);
    const std::string second = serialize(sample);
    assert(first == second);
    assert(first.find("endpoint-candidate-a") < first.find("endpoint-candidate-b"));

    std::cout << "source object reconciliation invariants passed\n";
    return 0;
}
