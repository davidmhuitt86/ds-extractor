#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <opencv2/core.hpp>

#include <filesystem>
#include <string>

namespace eke::dx::wire {

class ReviewArtifactWriter {
public:
    // Regenerates the visual extraction review set beneath output_root.
    // The directory is replaced on every extraction so the review reflects
    // only the current model.
    static void write(
        const WireModel& model,
        const cv::Mat& normalized,
        const std::filesystem::path& output_root);
};

} // namespace eke::dx::wire
