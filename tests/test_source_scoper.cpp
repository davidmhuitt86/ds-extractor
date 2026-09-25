#include "eke_dx_wire/ingest/source_scoper.hpp"

#include <opencv2/imgproc.hpp>

#include <cassert>

using namespace eke::dx::wire;

namespace {

cv::Mat white_page(int size = 100) {
    return cv::Mat(size, size, CV_8UC1, cv::Scalar(255));
}

bool is_white(const cv::Mat& image, int x, int y) {
    return image.at<std::uint8_t>(y, x) == 255;
}

bool is_black(const cv::Mat& image, int x, int y) {
    return image.at<std::uint8_t>(y, x) == 0;
}

} // namespace

int main() {
    SourceScoper scoper;

    // TEST 1: no scope (no include, no exclusion) preserves the source
    // exactly - this is what backward compatibility depends on.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 50}, {90, 50}, cv::Scalar(0), 2);

        ExtractionScope scope;
        const auto result = scoper.apply(image, scope, "src");

        assert(result.scoped_image.size() == image.size());
        cv::Mat diff;
        cv::absdiff(result.scoped_image, image, diff);
        assert(cv::countNonZero(diff) == 0);
    }

    // TEST 2: a single include rectangle - only geometry inside it survives.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 30}, {90, 30}, cv::Scalar(0), 2); // inside
        cv::line(image, {10, 80}, {90, 80}, cv::Scalar(0), 2); // outside

        ExtractionScope scope;
        scope.include_regions.push_back({0, 0, 100, 50});
        const auto result = scoper.apply(image, scope, "src");

        assert(is_black(result.scoped_image, 50, 30));
        assert(is_white(result.scoped_image, 50, 80));
    }

    // TEST 3: a single exclusion rectangle - excluded pixels never reach
    // extraction (become background), everything else is untouched.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 30}, {90, 30}, cv::Scalar(0), 2);

        ExtractionScope scope;
        scope.exclusion_regions.push_back({0, 0, 100, 50});
        const auto result = scoper.apply(image, scope, "src");

        assert(is_white(result.scoped_image, 50, 30));
    }

    // TEST 4: include + exclusion - deterministic intersection semantics.
    // The exclusion carves a hole out of the include region; nothing
    // outside the include region is ever visible regardless of exclusion.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 20}, {90, 20}, cv::Scalar(0), 2); // in include, not excluded
        cv::line(image, {10, 40}, {90, 40}, cv::Scalar(0), 2); // in include, excluded
        cv::line(image, {10, 80}, {90, 80}, cv::Scalar(0), 2); // outside include entirely

        ExtractionScope scope;
        scope.include_regions.push_back({0, 0, 100, 60});
        scope.exclusion_regions.push_back({0, 30, 100, 20});
        const auto result = scoper.apply(image, scope, "src");

        assert(is_black(result.scoped_image, 50, 20));
        assert(is_white(result.scoped_image, 50, 40));
        assert(is_white(result.scoped_image, 50, 80));
    }

    // TEST 5: an exclusion boundary never introduces conductor-like ink.
    // The excluded region and its immediate surroundings, on an otherwise
    // blank page, must remain entirely background - no border was drawn.
    {
        cv::Mat image = white_page();
        ExtractionScope scope;
        scope.exclusion_regions.push_back({20, 20, 40, 40});
        const auto result = scoper.apply(image, scope, "src");

        cv::Mat diff;
        cv::absdiff(result.scoped_image, white_page(), diff);
        assert(cv::countNonZero(diff) == 0);
    }

    // TEST 6: a real conductor immediately adjacent to (but outside) an
    // exclusion region remains fully intact, pixel for pixel.
    {
        cv::Mat image = white_page();
        cv::line(image, {41, 0}, {41, 99}, cv::Scalar(0), 1); // just outside excl right edge

        ExtractionScope scope;
        scope.exclusion_regions.push_back({0, 0, 40, 100});
        const auto result = scoper.apply(image, scope, "src");

        cv::Mat original_line_region = image(cv::Rect(41, 0, 1, 100));
        cv::Mat scoped_line_region = result.scoped_image(cv::Rect(41, 0, 1, 100));
        cv::Mat diff;
        cv::absdiff(original_line_region, scoped_line_region, diff);
        assert(cv::countNonZero(diff) == 0);
    }

    // TEST 7: two separate include regions both survive.
    {
        cv::Mat image = white_page();
        cv::line(image, {5, 10}, {15, 10}, cv::Scalar(0), 1);
        cv::line(image, {5, 90}, {15, 90}, cv::Scalar(0), 1);
        cv::line(image, {5, 50}, {15, 50}, cv::Scalar(0), 1); // not in either region

        ExtractionScope scope;
        scope.include_regions.push_back({0, 0, 20, 20});
        scope.include_regions.push_back({0, 80, 20, 20});
        const auto result = scoper.apply(image, scope, "src");

        assert(is_black(result.scoped_image, 10, 10));
        assert(is_black(result.scoped_image, 10, 90));
        assert(is_white(result.scoped_image, 10, 50));
    }

    // TEST 8: repeated application on identical input is byte-identical,
    // including the deterministic provenance identity.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 50}, {90, 50}, cv::Scalar(0), 2);

        ExtractionScope scope;
        scope.exclusion_regions.push_back({60, 0, 40, 100});

        const auto first = scoper.apply(image, scope, "src");
        const auto second = scoper.apply(image, scope, "src");

        cv::Mat diff;
        cv::absdiff(first.scoped_image, second.scoped_image, diff);
        assert(cv::countNonZero(diff) == 0);
        assert(first.provenance.id == second.provenance.id);
        assert(first.provenance.scope_id == second.provenance.scope_id);
    }

    // TEST 9: source-coordinate mapping. Approach A (Sec 7) keeps the
    // scoped image at the source's exact dimensions, so a point's
    // coordinates never change between source and scoped space - this is
    // the deterministic mapping itself, not merely an assumption about it.
    {
        cv::Mat image = white_page();
        ExtractionScope scope;
        scope.exclusion_regions.push_back({0, 0, 30, 30});
        const auto result = scoper.apply(image, scope, "src");

        assert(result.provenance.coordinate_system == "identity");
        assert(result.provenance.scoped_width == image.cols);
        assert(result.provenance.scoped_height == image.rows);
        assert(result.scoped_image.cols == image.cols);
        assert(result.scoped_image.rows == image.rows);
    }

    // TEST 10: metadata never alters scoped pixels - it is semantic
    // context recorded in provenance, not raster manipulation input.
    {
        cv::Mat image = white_page();
        cv::line(image, {10, 50}, {90, 50}, cv::Scalar(0), 2);

        ExtractionScope scope_a;
        scope_a.metadata.manufacturer = "Honda";
        scope_a.metadata.model = "TRX300";

        ExtractionScope scope_b;
        scope_b.metadata.manufacturer = "Yamaha";
        scope_b.metadata.model = "Different";

        const auto result_a = scoper.apply(image, scope_a, "src");
        const auto result_b = scoper.apply(image, scope_b, "src");

        cv::Mat diff;
        cv::absdiff(result_a.scoped_image, result_b.scoped_image, diff);
        assert(cv::countNonZero(diff) == 0);
        // Provenance itself does distinguish the two scopes.
        assert(result_a.provenance.scope_id != result_b.provenance.scope_id);
    }

    // Provenance content sanity: regions and metadata are echoed verbatim,
    // never fabricated.
    {
        cv::Mat image = white_page();
        ExtractionScope scope;
        scope.source_page = 2;
        scope.include_regions.push_back({1, 2, 3, 4});
        scope.exclusion_regions.push_back({5, 6, 7, 8});
        scope.metadata.manufacturer = "Honda";

        const auto result = scoper.apply(image, scope, "src-id");

        assert(result.provenance.source_id == "src-id");
        assert(result.provenance.source_page == 2);
        assert(result.provenance.include_regions.size() == 1);
        assert(result.provenance.include_regions[0].x == 1);
        assert(result.provenance.exclusion_regions.size() == 1);
        assert(result.provenance.exclusion_regions[0].x == 5);
        assert(result.provenance.metadata.manufacturer == "Honda");
        assert(!result.provenance.id.empty());
        assert(!result.provenance.scope_id.empty());
    }

    return 0;
}
