#include "eke_dx_wire/topology/sidecar_text_recognition_provider.hpp"

#include <cassert>
#include <fstream>

using namespace eke::dx::wire;

int main() {
    const std::string path = "test-recognition.sidecar";
    {
        std::ofstream out(path);
        out << "# text-region-id\\tconfidence\\ttext\\n";
        out << "text-region-1\\thigh\\tGND\\n";
        out << "text-region-2\\tmedium\\tB+\\n";
        out << "text-region-3\\tunresolved\\tIGN\\n";
        out << "malformed\\n";
    }

    SidecarTextRecognitionProvider provider(path);
    const cv::Mat image;
    const std::vector<TextRegion> regions = {
        {"text-region-1", TextRegionKind::Label, {0, 0, 10, 5}, 0.85, true},
        {"text-region-2", TextRegionKind::Label, {20, 0, 10, 5}, 0.85, true}
    };

    const auto result = provider.recognize(image, regions, "source", 0);

    assert(provider.provider_id() == "sidecar");
    assert(result.size() == 2);
    assert(result[0].text_region_id == "text-region-1");
    assert(result[0].raw_text == "GND");
    assert(result[0].confidence == ConfidenceClass::High);
    assert(result[0].provider == "sidecar");
    assert(result[1].text_region_id == "text-region-2");
    assert(result[1].raw_text == "B+");
    assert(result[1].confidence == ConfidenceClass::Medium);

    std::remove(path.c_str());
    return 0;
}
