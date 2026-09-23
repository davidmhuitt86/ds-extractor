#include "eke_dx_wire/image/diagram_furniture_classifier.hpp"

#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

ComponentCandidate make(
    const std::string& id, ComponentCandidateKind kind, int x, int y) {
    ComponentCandidate c;
    c.id = id;
    c.kind = kind;
    c.bounds = BoundingBox{x, y, 10, 10};
    return c;
}

std::size_t count_kind(
    const std::vector<ComponentCandidate>& candidates, ComponentCandidateKind kind) {
    std::size_t n = 0;
    for (const auto& c : candidates) {
        if (c.kind == kind) ++n;
    }
    return n;
}

} // namespace

int main() {
    // A dense grid well above minimum_cluster_size, like a
    // switch-continuity chart (4 rows x 5 columns = 20 shapes), is
    // re-tagged as furniture.
    {
        std::vector<ComponentCandidate> candidates;
        int id = 0;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 5; ++col) {
                const ComponentCandidateKind kind =
                    (col % 2 == 0) ? ComponentCandidateKind::CircularSymbol
                                   : ComponentCandidateKind::PrimitiveSymbol;
                candidates.push_back(make(
                    "c" + std::to_string(id++), kind,
                    col * 30, row * 30));
            }
        }

        DiagramFurnitureClassifier classifier;
        const auto result = classifier.classify(candidates);

        assert(count_kind(result, ComponentCandidateKind::DiagramFurniture) == 20);
    }

    // A handful of real, scattered circuit symbols must not be swept in.
    {
        std::vector<ComponentCandidate> candidates = {
            make("relay", ComponentCandidateKind::CircularSymbol, 10, 10),
            make("switch", ComponentCandidateKind::PrimitiveSymbol, 400, 250),
            make("diode", ComponentCandidateKind::PrimitiveSymbol, 120, 480),
            make("ground", ComponentCandidateKind::ChassisGround, 600, 90),
            make("enclosure", ComponentCandidateKind::Enclosure, 700, 700),
        };

        DiagramFurnitureClassifier classifier;
        const auto result = classifier.classify(candidates);

        assert(count_kind(result, ComponentCandidateKind::DiagramFurniture) == 0);
    }

    // A single row, even with many members (well above
    // minimum_cluster_size), is not tabular: real hardware - a relay's
    // contact strip, a wide connector - is routinely one row of many
    // pins, and must not be treated as a reference table.
    {
        std::vector<ComponentCandidate> candidates;
        for (int col = 0; col < 18; ++col) {
            candidates.push_back(make(
                "pin" + std::to_string(col),
                ComponentCandidateKind::PrimitiveSymbol, col * 15, 100));
        }

        DiagramFurnitureClassifier classifier;
        const auto result = classifier.classify(candidates);

        assert(count_kind(result, ComponentCandidateKind::DiagramFurniture) == 0);
    }

    // Regression: two unrelated single-row groups, far apart on the page,
    // that happen to reuse the same x-coordinates (common in schematics
    // laid out on an implicit grid) must NOT be merged into a fake table
    // just because row/column alignment matches globally. Each group is a
    // single row on its own (10 members, below minimum_cluster_size) and
    // must stay untagged; without the spatial-proximity gate this pair
    // combines into one 20-member, 2-row x 10-column "grid" that clears
    // every threshold and would be wrongly tagged as furniture.
    {
        std::vector<ComponentCandidate> candidates;
        for (int col = 0; col < 10; ++col) {
            candidates.push_back(make(
                "near-top-" + std::to_string(col),
                ComponentCandidateKind::PrimitiveSymbol, col * 20, 50));
        }
        for (int col = 0; col < 10; ++col) {
            candidates.push_back(make(
                "far-below-" + std::to_string(col),
                ComponentCandidateKind::PrimitiveSymbol, col * 20, 700));
        }

        DiagramFurnitureClassifier classifier;
        const auto result = classifier.classify(candidates);

        assert(count_kind(result, ComponentCandidateKind::DiagramFurniture) == 0);
    }

    // Enclosure/ChassisGround/Unknown candidates are never eligible, even
    // when positioned inside an otherwise-tabular grid.
    {
        std::vector<ComponentCandidate> candidates;
        int id = 0;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 5; ++col) {
                candidates.push_back(make(
                    "c" + std::to_string(id++),
                    ComponentCandidateKind::CircularSymbol,
                    col * 30, row * 30));
            }
        }
        candidates.push_back(make(
            "housing", ComponentCandidateKind::Enclosure, 15, 15));

        DiagramFurnitureClassifier classifier;
        const auto result = classifier.classify(candidates);

        assert(count_kind(result, ComponentCandidateKind::DiagramFurniture) == 20);
        assert(count_kind(result, ComponentCandidateKind::Enclosure) == 1);
    }

    return 0;
}
