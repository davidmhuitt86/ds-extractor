#include "eke_dx_wire/topology/json_text_recognition_provider.hpp"

#include <opencv2/core.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

int main() {
    const fs::path path =
        fs::temp_directory_path() / "eke-dx-wire-recognition-observations.json";

    {
        std::ofstream out(path);
        out << "{\n"
            << "  \"observations\": [\n"
            << "    {\"text_region_id\": \"r1\", "
               "\"raw_text\": \"GND\", \"confidence\": \"high\"},\n"
            << "    {\"text_region_id\": \"r2\", "
               "\"raw_text\": \"B+\", \"confidence\": \"medium\"},\n"
            << "    {\"text_region_id\": \"r3\", "
               "\"raw_text\": \"IGN\", \"confidence\": \"unresolved\"},\n"
            << "    {\"text_region_id\": \"r4\", "
               "\"raw_text\": \"bad\", \"confidence\": \"unknown\"},\n"
            << "    {\"text_region_id\": \"\", "
               "\"raw_text\": \"ignored\", \"confidence\": \"high\"}\n"
            << "  ]\n}\n";
    }

    JsonTextRecognitionProvider provider(path.string());
    const cv::Mat image = cv::Mat::zeros(10, 10, CV_8UC1);
    const std::vector<TextRegion> regions;

    const auto evidence =
        provider.recognize(image, regions, "fixture", 0);

    assert(evidence.size() == 2);
    assert(evidence[0].text_region_id == "r1");
    assert(evidence[0].raw_text == "GND");
    assert(evidence[0].confidence == ConfidenceClass::High);
    assert(evidence[0].provider == "json-observation");
    assert(evidence[1].text_region_id == "r2");
    assert(evidence[1].confidence == ConfidenceClass::Medium);

    fs::remove(path);
    return 0;
}
