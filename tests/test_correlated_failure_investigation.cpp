// AP-DIAG-019 regression coverage for the correlated-failure root-cause
// investigation artifact (artifacts/audit/AP-DIAG-019_correlated_failure_
// investigation.json). Diagnostic/reporting only; this test verifies
// machine-checkable structural invariants, not the root-cause judgments
// themselves (which required direct code execution and source-image
// inspection, not something a unit test can re-derive).
//
// Invariants:
//   1. Every object_id and related_object_id resolves to a stable ID in
//      the same-run extraction_audit.json.
//   2. No duplicate object_id within any population.
//   3. Each population is in strict ascending object_id order.
//   4. classification_counts and root_cause_counts per population sum to
//      that population's record count.
//   5. population_counts match each population's actual record count.
//   6. connector_reconciliation and circular_symbol_reconciliation cover
//      exactly the full, real connector/circular-symbol populations
//      derived independently from extraction_audit.json (exact set
//      membership, not merely a subset).
//
// Skipped (not failed) when the diagnostic artifacts are absent, since
// they are checked-in AP outputs rather than build products.

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
    "connector_reconciliation",
    "circular_symbol_reconciliation",
    "net_resolution_gap_reconciliation",
};

const char* kPopulationCountKeys[] = {
    "connector_candidates",
    "circular_symbol_candidates",
    "net_resolution_gap_endpoints",
};

} // namespace

int main() {
    const fs::path root = fs::path(DX_WIRE_SOURCE_DIR);
    const fs::path audit_path = root / "artifacts" / "audit" / "extraction_audit.json";
    const fs::path investigation_path =
        root / "artifacts" / "audit" / "AP-DIAG-019_correlated_failure_investigation.json";

    if (!fs::exists(audit_path) || !fs::exists(investigation_path)) {
        std::cout << "AP-DIAG-019 artifacts not present in this build tree; skipping.\n";
        return 0;
    }

    cv::FileStorage audit_fs(
        slurp(audit_path), cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);
    assert(audit_fs.isOpened());
    const std::set<std::string> known_ids = collect_stable_ids(audit_fs["objects"]);
    assert(!known_ids.empty());

    cv::FileStorage inv_fs(
        slurp(investigation_path), cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);
    assert(inv_fs.isOpened());

    // Independently-derived expected exact membership for the two
    // populations extraction_audit.json can fully determine on its own.
    std::set<std::string> expected_connectors;
    for (const auto& item : audit_fs["objects"]["connectors"])
        expected_connectors.insert((std::string)item["id"]);

    std::set<std::string> expected_circular_symbols;
    for (const auto& item : audit_fs["objects"]["components"])
        if ((std::string)item["kind"] == "circular_symbol")
            expected_circular_symbols.insert((std::string)item["id"]);

    std::map<std::string, std::set<std::string>> expected = {
        {"connector_reconciliation", expected_connectors},
        {"circular_symbol_reconciliation", expected_circular_symbols},
    };

    for (const char* group : kGroups) {
        const cv::FileNode records = inv_fs[group];
        assert(records.isSeq());
        std::set<std::string> actual_ids;
        std::map<std::string, int> classification_tally;
        std::map<std::string, int> root_cause_tally;
        std::string previous_id;

        for (const auto& record : records) {
            const std::string object_id = (std::string)record["object_id"];
            assert(known_ids.count(object_id) > 0);
            assert(actual_ids.insert(object_id).second);
            if (!previous_id.empty())
                assert(previous_id < object_id);
            previous_id = object_id;

            for (const auto& rel : record["related_object_ids"])
                assert(known_ids.count((std::string)rel) > 0);

            ++classification_tally[(std::string)record["classification"]];
            ++root_cause_tally[(std::string)record["root_cause_category"]];
        }

        if (expected.count(group)) {
            assert(actual_ids == expected[group]);
        }

        const cv::FileNode class_counts = inv_fs["classification_counts"][group];
        int declared_class_total = 0;
        for (const auto& kv : class_counts) {
            assert(classification_tally.count(kv.name()) > 0 &&
                   classification_tally[kv.name()] == (int)kv);
            declared_class_total += (int)kv;
        }
        assert(declared_class_total == static_cast<int>(records.size()));

        const cv::FileNode cause_counts = inv_fs["root_cause_counts"][group];
        int declared_cause_total = 0;
        for (const auto& kv : cause_counts) {
            assert(root_cause_tally.count(kv.name()) > 0 &&
                   root_cause_tally[kv.name()] == (int)kv);
            declared_cause_total += (int)kv;
        }
        assert(declared_cause_total == static_cast<int>(records.size()));
    }

    const cv::FileNode population = inv_fs["population_counts"];
    for (std::size_t i = 0; i < sizeof(kGroups) / sizeof(kGroups[0]); ++i)
        assert((int)population[kPopulationCountKeys[i]] ==
               static_cast<int>(inv_fs[kGroups[i]].size()));

    std::cout << "correlated failure investigation invariants passed\n";
    return 0;
}
