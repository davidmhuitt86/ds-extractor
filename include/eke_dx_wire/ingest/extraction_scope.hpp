#pragma once

#include "eke_dx_wire/core/geometry.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

// AP-INGEST-001: contextual engineering metadata about the diagram a scope
// bounds. This is human-supplied context, never inferred from image
// content - any field left blank stays unknown rather than guessed.
struct DiagramMetadata {
    std::string diagram_type;
    std::string manufacturer;
    std::string make;
    std::string model;
    std::string year;
    std::string vehicle_type;
    std::string system;
    std::string harness_branch;
    std::string diagram_section;
};

// A labeled region of interest that does not affect extraction (AP-INGEST-001
// Sec 21). Purely descriptive context for a future consumer - e.g. marking
// where a switch-continuity matrix or connector table sits on the page.
struct AnnotationRegion {
    std::string id;
    BoundingBox bounds {};
    std::string annotation_type;
    std::string description;
};

// AP-INGEST-001: the ingestion-layer description of which part of a source
// document/page is actually engineering diagram content. This is the only
// input SourceScoper consumes; it never inspects pixel content to decide
// scope, only the regions and page identity supplied here.
struct ExtractionScope {
    int schema_version = 1;
    std::string source_path;
    int source_page = 0;

    // Empty means "the entire source image is eligible" (Sec 5) -
    // preserving pre-scoping behavior exactly when no scope is authored.
    std::vector<BoundingBox> include_regions;

    // Regions explicitly excluded from engineering extraction (Sec 6),
    // applied after include_regions narrow eligibility.
    std::vector<BoundingBox> exclusion_regions;

    // Sec 21: contextual only, never consumed by SourceScoper's masking.
    std::vector<AnnotationRegion> annotation_regions;

    DiagramMetadata metadata;
};

} // namespace eke::dx::wire
