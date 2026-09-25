#pragma once

#include "eke_dx_wire/ingest/extraction_scope.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace eke::dx::wire {

// AP-INGEST-001 Sec 10 / AP-INGEST-002 Sec 7: the deterministic, traceable
// record of how a scoped source relates back to its original. Deliberately
// excludes anything non-deterministic (timestamps, process/machine
// identity) - see Sec 11/Sec 8.
struct ScopeProvenance {
    // stable_id("scoped-source", ...) over source_id + page + scope_id.
    std::string id;

    std::string source_id;
    int source_page = 0;

    // AP-INGEST-002: explicit, unambiguous answer to "was this source
    // scoped?" - always true for any ScopeProvenance SourceScoper::apply()
    // produces (an unscoped run never constructs one at all), but recorded
    // as its own field rather than left implicit in "a provenance file
    // exists," so a consumer reading only the JSON's own content - not the
    // fact of its presence on disk - can answer the question directly.
    bool scoped = true;

    // stable_id("extraction-scope", ...) over the scope's own regions and
    // metadata content only - identical scopes always produce the same id,
    // regardless of where the scope file lives on disk.
    std::string scope_id;

    std::vector<BoundingBox> include_regions;
    std::vector<BoundingBox> exclusion_regions;

    // AP-INGEST-002: previously omitted from provenance even though
    // AnnotationRegions were part of the applied scope - closes that gap
    // (Sec 7's "What annotation regions were used?").
    std::vector<AnnotationRegion> annotation_regions;

    DiagramMetadata metadata;

    int scoped_width = 0;
    int scoped_height = 0;

    // "identity": Approach A (Sec 7) - the scoped image keeps the source
    // image's exact dimensions, so a point (x, y) in the scoped image is,
    // by construction, the same (x, y) in the original source image. No
    // other coordinate system is currently produced by this stage.
    std::string coordinate_system = "identity";
};

struct ScopedSourceArtifacts {
    cv::Mat scoped_image;
    ScopeProvenance provenance;
};

// AP-INGEST-001: applies an ExtractionScope to a loaded source image,
// producing a scoped image of identical dimensions with everything outside
// the scope's eligible region painted a flat background value. This stage
// never draws a shape, border, or line of any kind - it only ever replaces
// pixels with a single flat color - so it can never introduce geometry an
// extractor could mistake for a conductor (Sec 8). It performs no
// classification, recognition, or extraction of its own.
class SourceScoper {
public:
    [[nodiscard]] ScopedSourceArtifacts apply(
        const cv::Mat& source,
        const ExtractionScope& scope,
        const std::string& source_id) const;

    // Deterministic JSON rendering of a ScopeProvenance record, following
    // this project's existing hand-rolled JSON convention (Sec 10).
    [[nodiscard]] static std::string serialize_provenance(
        const ScopeProvenance& provenance);
};

} // namespace eke::dx::wire
