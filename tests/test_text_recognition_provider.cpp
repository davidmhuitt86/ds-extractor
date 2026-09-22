#include "eke_dx_wire/topology/text_recognition_provider.hpp"

#include <cassert>

using namespace eke::dx::wire;

namespace {

class MockProvider final : public TextRecognitionProvider {
public:
    [[nodiscard]] std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat&,
        const std::vector<TextRegion>& regions,
        const std::string&,
        int) const override {
        assert(regions.size() == 1);
        return {
            {regions.front().id, "GND", ConfidenceClass::High, provider_id()}
        };
    }

    [[nodiscard]] std::string provider_id() const override {
        return "test-provider";
    }
};

} // namespace

int main() {
    NullTextRecognitionProvider null_provider;
    const cv::Mat image;

    const std::vector<TextRegion> regions = {
        {"text-region-1", TextRegionKind::Label, {10, 20, 30, 8}, 0.85, true}
    };

    assert(null_provider.provider_id() == "none");
    assert(null_provider.recognize(image, regions, "source", 0).empty());

    MockProvider mock;
    const auto evidence = mock.recognize(
        image,
        regions,
        "source",
        0);

    assert(evidence.size() == 1);
    assert(evidence[0].text_region_id == "text-region-1");
    assert(evidence[0].raw_text == "GND");
    assert(evidence[0].confidence == ConfidenceClass::High);
    assert(evidence[0].provider == "test-provider");

    return 0;
}
