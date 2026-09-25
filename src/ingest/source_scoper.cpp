#include "eke_dx_wire/ingest/source_scoper.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace eke::dx::wire {
namespace {

// White matches the blank-page background every source diagram in this
// project is drawn against (confirmed for samples/trx300ODG.png and used
// throughout the test suite's own white_page() fixtures). Filling with the
// page's own background color - never a drawn rectangle stroke - is what
// keeps a mask boundary invisible to conductor extraction (Sec 8): there is
// no color discontinuity introduced anywhere except where real content
// already ended, and that discontinuity is a plain truncation, not a new
// edge running along the mask's own border.
constexpr int kBackgroundValue = 255;

cv::Rect clipped(const BoundingBox& box, const cv::Size& bounds) {
    const cv::Rect rect(box.x, box.y, box.width, box.height);
    return rect & cv::Rect(0, 0, bounds.width, bounds.height);
}

// mask pixel == 255 means "eligible for extraction".
cv::Mat build_eligibility_mask(
    const ExtractionScope& scope, const cv::Size& size) {

    cv::Mat mask;
    if (scope.include_regions.empty()) {
        // Sec 5: no include region means the entire source is eligible -
        // this is what preserves unscoped-equivalent behavior.
        mask = cv::Mat(size, CV_8UC1, cv::Scalar(255));
    } else {
        mask = cv::Mat::zeros(size, CV_8UC1);
        for (const auto& region : scope.include_regions) {
            const cv::Rect rect = clipped(region, size);
            if (rect.area() > 0) {
                mask(rect).setTo(255);
            }
        }
    }

    for (const auto& region : scope.exclusion_regions) {
        const cv::Rect rect = clipped(region, size);
        if (rect.area() > 0) {
            mask(rect).setTo(0);
        }
    }

    return mask;
}

std::string canonical_region_text(const std::vector<BoundingBox>& regions) {
    std::ostringstream out;
    for (const auto& region : regions) {
        out << region.x << ',' << region.y << ',' << region.width << ','
            << region.height << ';';
    }
    return out.str();
}

std::string canonical_metadata_text(const DiagramMetadata& metadata) {
    std::ostringstream out;
    out << metadata.diagram_type << '|' << metadata.manufacturer << '|'
        << metadata.make << '|' << metadata.model << '|' << metadata.year
        << '|' << metadata.vehicle_type << '|' << metadata.system << '|'
        << metadata.harness_branch << '|' << metadata.diagram_section;
    return out.str();
}

// Sec 10/11: a deterministic identity for the scope's own content, entirely
// independent of the scope file's path on disk or when it was authored.
// Metadata deliberately does not participate in this hash the same way
// regions do - see Sec 11's own separation - but is still included here so
// two scopes that differ only in metadata remain distinguishable, which
// does not violate "metadata never alters pixel extraction" (TEST 10): this
// id never feeds back into masking, only into provenance identity.
std::string compute_scope_id(const ExtractionScope& scope) {
    std::ostringstream canonical;
    canonical << scope.schema_version << '#' << scope.source_page << '#'
              << canonical_region_text(scope.include_regions) << '#'
              << canonical_region_text(scope.exclusion_regions) << '#'
              << canonical_metadata_text(scope.metadata);
    return stable_id("extraction-scope", canonical.str());
}

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

std::string serialize_region_list(const std::vector<BoundingBox>& regions) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const auto& box = regions[i];
        out << "\n    {\"x\": " << box.x << ", \"y\": " << box.y
            << ", \"width\": " << box.width << ", \"height\": " << box.height
            << "}";
        if (i + 1 < regions.size()) out << ",";
    }
    if (!regions.empty()) out << "\n  ";
    out << "]";
    return out.str();
}

} // namespace

ScopedSourceArtifacts SourceScoper::apply(
    const cv::Mat& source,
    const ExtractionScope& scope,
    const std::string& source_id) const {

    if (source.empty()) {
        throw std::runtime_error("SourceScoper: source image is empty");
    }

    const cv::Mat mask = build_eligibility_mask(scope, source.size());

    cv::Mat scoped = source.clone();
    const cv::Mat background(
        source.size(), source.type(), cv::Scalar::all(kBackgroundValue));
    cv::Mat ineligible;
    cv::bitwise_not(mask, ineligible);
    background.copyTo(scoped, ineligible);

    ScopedSourceArtifacts result;
    result.scoped_image = std::move(scoped);

    result.provenance.source_id = source_id;
    result.provenance.source_page = scope.source_page;
    result.provenance.scope_id = compute_scope_id(scope);
    result.provenance.include_regions = scope.include_regions;
    result.provenance.exclusion_regions = scope.exclusion_regions;
    result.provenance.metadata = scope.metadata;
    result.provenance.scoped_width = source.cols;
    result.provenance.scoped_height = source.rows;
    result.provenance.coordinate_system = "identity";
    result.provenance.id = stable_id(
        "scoped-source",
        source_id + ":" + std::to_string(scope.source_page) + ":" +
            result.provenance.scope_id);

    return result;
}

std::string SourceScoper::serialize_provenance(
    const ScopeProvenance& provenance) {

    std::ostringstream out;
    out << "{\n"
        << "  \"format\": \"eke-dx-wire-scope-provenance\",\n"
        << "  \"version\": \"1.0\",\n"
        << "  \"id\": \"" << json_escape(provenance.id) << "\",\n"
        << "  \"source_id\": \"" << json_escape(provenance.source_id) << "\",\n"
        << "  \"source_page\": " << provenance.source_page << ",\n"
        << "  \"scope_id\": \"" << json_escape(provenance.scope_id) << "\",\n"
        << "  \"include_regions\": "
        << serialize_region_list(provenance.include_regions) << ",\n"
        << "  \"exclusion_regions\": "
        << serialize_region_list(provenance.exclusion_regions) << ",\n"
        << "  \"metadata\": {\n"
        << "    \"diagram_type\": \""
        << json_escape(provenance.metadata.diagram_type) << "\",\n"
        << "    \"manufacturer\": \""
        << json_escape(provenance.metadata.manufacturer) << "\",\n"
        << "    \"make\": \"" << json_escape(provenance.metadata.make) << "\",\n"
        << "    \"model\": \"" << json_escape(provenance.metadata.model) << "\",\n"
        << "    \"year\": \"" << json_escape(provenance.metadata.year) << "\",\n"
        << "    \"vehicle_type\": \""
        << json_escape(provenance.metadata.vehicle_type) << "\",\n"
        << "    \"system\": \"" << json_escape(provenance.metadata.system) << "\",\n"
        << "    \"harness_branch\": \""
        << json_escape(provenance.metadata.harness_branch) << "\",\n"
        << "    \"diagram_section\": \""
        << json_escape(provenance.metadata.diagram_section) << "\"\n"
        << "  },\n"
        << "  \"scoped_width\": " << provenance.scoped_width << ",\n"
        << "  \"scoped_height\": " << provenance.scoped_height << ",\n"
        << "  \"coordinate_system\": \""
        << json_escape(provenance.coordinate_system) << "\"\n"
        << "}\n";
    return out.str();
}

} // namespace eke::dx::wire
