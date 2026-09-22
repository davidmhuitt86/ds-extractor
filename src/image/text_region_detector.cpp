#include "eke_dx_wire/image/text_region_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <map>

namespace eke::dx::wire {

namespace {

struct Component {
    cv::Rect bounds;
};

bool vertically_compatible(const cv::Rect& a, const cv::Rect& b, int tolerance) {
    const int a_center = a.y + a.height / 2;
    const int b_center = b.y + b.height / 2;
    return std::abs(a_center - b_center) <= tolerance;
}

bool horizontally_close(const cv::Rect& a, const cv::Rect& b, int gap) {
    const int a_right = a.x + a.width;
    const int b_right = b.x + b.width;
    const int distance = b.x >= a_right ? b.x - a_right : a.x - b_right;
    return distance <= gap;
}

BoundingBox to_box(const cv::Rect& rect) {
    return BoundingBox{rect.x, rect.y, rect.width, rect.height};
}

} // namespace

TextRegionDetector::TextRegionDetector(TextDetectorConfig config)
    : config_(config) {}

TextDetectionArtifacts TextRegionDetector::detect(
    const cv::Mat& normalized,
    const std::string&,
    int) const {

    TextDetectionArtifacts artifacts;
    if (normalized.empty()) {
        return artifacts;
    }

    cv::Mat gray;
    if (normalized.channels() == 1) {
        gray = normalized;
    } else if (normalized.channels() == 4) {
        cv::cvtColor(normalized, gray, cv::COLOR_BGRA2GRAY);
    } else {
        cv::cvtColor(normalized, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat binary;
    cv::threshold(
        gray,
        binary,
        config_.threshold,
        255,
        cv::THRESH_BINARY_INV);

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int count = cv::connectedComponentsWithStats(
        binary,
        labels,
        stats,
        centroids,
        8,
        CV_32S);

    std::vector<Component> components;
    for (int i = 1; i < count; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        const int width = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int height = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        if (area < config_.minimum_component_area ||
            area > config_.maximum_component_area ||
            width > config_.maximum_component_width ||
            height > config_.maximum_component_height) {
            continue;
        }

        components.push_back(Component{
            cv::Rect(
                stats.at<int>(i, cv::CC_STAT_LEFT),
                stats.at<int>(i, cv::CC_STAT_TOP),
                width,
                height)});
    }

    std::sort(
        components.begin(),
        components.end(),
        [](const Component& a, const Component& b) {
            if (a.bounds.y != b.bounds.y) {
                return a.bounds.y < b.bounds.y;
            }
            return a.bounds.x < b.bounds.x;
        });

    std::vector<cv::Rect> groups;
    for (const auto& component : components) {
        bool attached = false;

        for (auto& group : groups) {
            if (!vertically_compatible(
                    group,
                    component.bounds,
                    config_.grouping_vertical_tolerance)) {
                continue;
            }

            if (!horizontally_close(
                    group,
                    component.bounds,
                    config_.grouping_gap)) {
                continue;
            }

            const cv::Rect merged = group | component.bounds;
            if (merged.height <= config_.maximum_group_height) {
                group = merged;
                attached = true;
                break;
            }
        }

        if (!attached) {
            groups.push_back(component.bounds);
        }
    }

    int index = 0;
    for (const auto& group : groups) {
        if (group.width < config_.minimum_group_width ||
            group.height < config_.minimum_group_height) {
            continue;
        }

        const double aspect =
            static_cast<double>(group.width) /
            static_cast<double>(std::max(1, group.height));

        // Text labels tend to be horizontally organized. Extremely compact
        // or nearly square groups are retained as candidates but receive
        // lower confidence rather than being treated as certain text.
        double confidence = 0.55;
        if (aspect >= 2.0 && aspect <= 30.0) {
            confidence = 0.85;
        } else if (aspect >= 1.25 && aspect <= 40.0) {
            confidence = 0.70;
        }

        if (confidence < config_.minimum_confidence) {
            continue;
        }

        TextRegion region;
        region.id = "text-region-" + std::to_string(index++);
        region.kind = TextRegionKind::Label;
        region.bounds = to_box(group);
        region.confidence = confidence;
        region.excluded_from_wire_detection = confidence >= 0.70;
        artifacts.regions.push_back(region);
    }

    std::sort(
        artifacts.regions.begin(),
        artifacts.regions.end(),
        [](const TextRegion& a, const TextRegion& b) {
            if (a.bounds.y != b.bounds.y) {
                return a.bounds.y < b.bounds.y;
            }
            if (a.bounds.x != b.bounds.x) {
                return a.bounds.x < b.bounds.x;
            }
            return a.id < b.id;
        });

    for (auto& region : artifacts.regions) {
        if (!region.excluded_from_wire_detection) {
            continue;
        }

        const cv::Rect rect(
            region.bounds.x,
            region.bounds.y,
            region.bounds.width,
            region.bounds.height);
        cv::rectangle(
            artifacts.exclusion_mask,
            rect,
            cv::Scalar(255),
            cv::FILLED);
    }

    return artifacts;
}

} // namespace eke::dx::wire
