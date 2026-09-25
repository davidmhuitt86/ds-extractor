#include "eke_dx_wire/ingest/extraction_scope_io.hpp"

#include <cassert>

using namespace eke::dx::wire;

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

    return 0;
}
