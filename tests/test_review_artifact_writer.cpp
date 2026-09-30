// AP-DIAG-017A regression coverage for ReviewArtifactWriter's two
// publication-integrity fixes:
//   1. source_id is now JSON-escaped in review_manifest.json (a Windows
//      absolute path containing backslashes previously produced invalid
//      JSON).
//   2. write() now writes to a temporary sibling directory and only
//      atomically replaces the live extraction_review directory once every
//      artifact has been written successfully, so a failure partway
//      through can never leave the live directory cleared-but-incomplete.

#include "eke_dx_wire/export/review_artifact_writer.hpp"

#include <opencv2/core.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

WireModel make_model(const std::string& source_id) {
    WireModel model;
    model.source_id = source_id;
    model.page = 0;
    model.image_width = 20;
    model.image_height = 10;

    ComponentCandidate component;
    component.id = "component_1";
    component.kind = ComponentCandidateKind::CircularSymbol;
    model.component_candidates.push_back(component);

    EndpointCandidate endpoint;
    endpoint.id = "endpoint_1";
    endpoint.node_id = "node_1";
    endpoint.kind = EndpointKind::ComponentTerminal;
    endpoint.component_id = component.id;
    model.endpoint_candidates.push_back(endpoint);

    Wire wire;
    wire.id = "wire_1";
    wire.start_endpoint = endpoint.id;
    wire.end_endpoint = endpoint.id;
    model.wires.push_back(wire);

    return model;
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    const fs::path root = "review_artifact_writer_test_root";
    std::error_code error;
    fs::remove_all(root, error);

    // --- 1. Normal write: escaped source_id produces valid JSON --------
    {
        cv::Mat normalized(10, 20, CV_8UC1, cv::Scalar(255));
        const WireModel model = make_model("C:\\dev\\ds-extractor\\samples\\trx300ODG.png");

        ReviewArtifactWriter::write(model, normalized, root);

        const fs::path review = root / "artifacts" / "extraction_review";
        assert(fs::exists(review / "00_source.png"));
        assert(fs::exists(review / "13_combined.png"));
        assert(fs::exists(review / "review_manifest.json"));

        const std::string manifest = read_file(review / "review_manifest.json");
        // A raw backslash in a JSON string is invalid; every backslash in
        // the escaped source_id must be doubled in the written file.
        assert(manifest.find("C:\\\\dev\\\\ds-extractor") != std::string::npos);
    }

    // --- 2. Atomicity: a failed write never touches the live directory -
    {
        const fs::path review = root / "artifacts" / "extraction_review";
        const fs::path marker = review / "from_previous_successful_run.marker";
        {
            std::ofstream out(marker);
            out << "previous run content";
        }
        assert(fs::exists(marker));

        cv::Mat empty_image;
        const WireModel model = make_model("fixture.png");

        bool threw = false;
        try {
            ReviewArtifactWriter::write(model, empty_image, root);
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);

        // The live directory must be exactly what it was before the
        // failed call - not cleared, not partially overwritten.
        assert(fs::exists(marker));
        assert(read_file(marker) == "previous run content");
    }

    fs::remove_all(root, error);
    return 0;
}
