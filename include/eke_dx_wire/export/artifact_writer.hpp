#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <opencv2/core.hpp>

#include <filesystem>
#include <string>

namespace eke::dx::wire {

class ExtractionArtifactWriter {
public:
    // Write the canonical extraction artifact set beneath output_root.
    //
    // Both CLI and GUI callers use this boundary so the project layout,
    // audit, topology, and SVG artifacts are generated identically.
    static void write(
        const WireModel& model,
        const cv::Mat& normalized,
        const std::string& image_path,
        const std::filesystem::path& output_root);
};

} // namespace eke::dx::wire
