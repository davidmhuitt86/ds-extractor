#include "eke_dx_wire/ingest/extraction_scope_io.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace eke::dx::wire;

namespace {

bool parse_throws(const std::string& json) {
    try {
        [[maybe_unused]] const ExtractionScope scope =
            ExtractionScopeIO::parse(json);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

} // namespace

int main() {
    // Full round trip: every field populated, including annotation
    // regions and metadata.
    {
        ExtractionScope scope;
        scope.schema_version = 1;
        scope.source_path = "samples/trx300ODG.png";
        scope.source_page = 0;
        scope.include_regions.push_back({10, 20, 300, 400});
        scope.exclusion_regions.push_back({0, 0, 50, 50});
        scope.exclusion_regions.push_back({100, 100, 60, 30});

        AnnotationRegion annotation;
        annotation.id = "annotation-1";
        annotation.bounds = {5, 5, 15, 15};
        annotation.annotation_type = "switch_matrix";
        annotation.description = "ignition/lighting/dimmer/engine-stop/starter tables";
        scope.annotation_regions.push_back(annotation);

        scope.metadata.diagram_type = "wiring_diagram";
        scope.metadata.manufacturer = "Honda";
        scope.metadata.make = "Honda";
        scope.metadata.model = "TRX300";
        scope.metadata.year = "1988";
        scope.metadata.vehicle_type = "ATV";
        scope.metadata.system = "electrical";
        scope.metadata.harness_branch = "main";
        scope.metadata.diagram_section = "full";

        const std::string json = ExtractionScopeIO::serialize(scope);
        const ExtractionScope parsed = ExtractionScopeIO::parse(json);

        assert(parsed.schema_version == 1);
        assert(parsed.source_path == "samples/trx300ODG.png");
        assert(parsed.source_page == 0);

        assert(parsed.include_regions.size() == 1);
        assert(parsed.include_regions[0].x == 10);
        assert(parsed.include_regions[0].y == 20);
        assert(parsed.include_regions[0].width == 300);
        assert(parsed.include_regions[0].height == 400);

        assert(parsed.exclusion_regions.size() == 2);
        assert(parsed.exclusion_regions[1].x == 100);
        assert(parsed.exclusion_regions[1].height == 30);

        assert(parsed.annotation_regions.size() == 1);
        assert(parsed.annotation_regions[0].id == "annotation-1");
        assert(parsed.annotation_regions[0].bounds.width == 15);
        assert(parsed.annotation_regions[0].annotation_type == "switch_matrix");
        assert(parsed.annotation_regions[0].description ==
               "ignition/lighting/dimmer/engine-stop/starter tables");

        assert(parsed.metadata.manufacturer == "Honda");
        assert(parsed.metadata.model == "TRX300");
        assert(parsed.metadata.year == "1988");
        assert(parsed.metadata.vehicle_type == "ATV");
    }

    // Sec 5: an empty scope (no regions at all) round-trips to empty
    // vectors, not fabricated defaults - this is what preserves
    // "no scope means entire image eligible" end to end.
    {
        ExtractionScope scope;
        const std::string json = ExtractionScopeIO::serialize(scope);
        const ExtractionScope parsed = ExtractionScopeIO::parse(json);

        assert(parsed.include_regions.empty());
        assert(parsed.exclusion_regions.empty());
        assert(parsed.annotation_regions.empty());
        assert(parsed.metadata.manufacturer.empty());
    }

    // Determinism: serializing the same scope twice produces byte-identical
    // JSON text.
    {
        ExtractionScope scope;
        scope.include_regions.push_back({1, 2, 3, 4});
        scope.metadata.manufacturer = "Honda";

        const std::string first = ExtractionScopeIO::serialize(scope);
        const std::string second = ExtractionScopeIO::serialize(scope);
        assert(first == second);
    }

    // AP-INGEST-002 Sec 11: invalid scopes fail explicitly rather than
    // being silently accepted or guessed at.

    // Unsupported schema version.
    assert(parse_throws(R"({"schema_version": 2, "source": {"path": "x", "page": 0}})"));

    // A valid, currently-supported schema version does not throw.
    {
        const ExtractionScope scope = ExtractionScopeIO::parse(
            R"({"schema_version": 1, "source": {"path": "x", "page": 0}})");
        assert(scope.schema_version == 1);
    }

    // Negative dimensions on an include region.
    assert(parse_throws(
        R"({"schema_version": 1, "source": {"path": "x", "page": 0},
            "include_regions": [{"x": 0, "y": 0, "width": -10, "height": 20}]})"));

    // Zero-width region (zero-area) is rejected, not silently accepted as
    // a harmless no-op - almost certainly an authoring mistake.
    assert(parse_throws(
        R"({"schema_version": 1, "source": {"path": "x", "page": 0},
            "exclusion_regions": [{"x": 0, "y": 0, "width": 0, "height": 20}]})"));

    // A negative region origin is NOT rejected at this stage - it may be a
    // legitimate region that starts before the image's top-left corner and
    // still substantially overlaps it. Whether it is actually out of
    // bounds can only be decided once the source image's size is known
    // (SourceScoper::apply() performs that check - see test_source_scoper.cpp).
    {
        const ExtractionScope scope = ExtractionScopeIO::parse(
            R"({"schema_version": 1, "source": {"path": "x", "page": 0},
                "include_regions": [{"x": -5, "y": 0, "width": 10, "height": 20}]})");
        assert(scope.include_regions.size() == 1);
        assert(scope.include_regions[0].x == -5);
    }

    // Malformed geometry on an annotation region is caught too, even
    // though annotation regions never affect masking (Sec 21).
    assert(parse_throws(
        R"({"schema_version": 1, "source": {"path": "x", "page": 0},
            "annotation_regions": [{"id": "a", "x": 0, "y": 0, "width": 0, "height": 0,
                                     "annotation_type": "legend", "description": ""}]})"));

    // Negative source_page.
    assert(parse_throws(R"({"schema_version": 1, "source": {"path": "x", "page": -1}})"));

    // A well-formed scope with valid, positive-area regions on all three
    // region kinds does not throw.
    {
        const ExtractionScope scope = ExtractionScopeIO::parse(
            R"({"schema_version": 1, "source": {"path": "x", "page": 0},
                "include_regions": [{"x": 0, "y": 0, "width": 100, "height": 100}],
                "exclusion_regions": [{"x": 10, "y": 10, "width": 5, "height": 5}],
                "annotation_regions": [{"id": "a", "x": 1, "y": 1, "width": 1, "height": 1,
                                         "annotation_type": "legend", "description": ""}]})");
        assert(scope.include_regions.size() == 1);
        assert(scope.exclusion_regions.size() == 1);
        assert(scope.annotation_regions.size() == 1);
    }

    return 0;
}
