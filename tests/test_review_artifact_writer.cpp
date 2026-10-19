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

    // Deliberately include both a physical-boundary candidate and a
    // ground-connection candidate. The manifest must not count the latter
    // as a physical terminal.
    TerminalCandidate physical_terminal;
    physical_terminal.id = "terminal_component_1";
    physical_terminal.kind = TerminalCandidateKind::ComponentBoundary;
    model.terminal_candidates.push_back(physical_terminal);
    TerminalCandidate ground_connection;
    ground_connection.id = "terminal_ground_1";
    ground_connection.kind = TerminalCandidateKind::GroundConnection;
    model.terminal_candidates.push_back(ground_connection);

    model.conductor_segments.push_back(ConductorSegment{});
    WireSemanticResolution wire_color;
    wire_color.id = "wire_semantic_1";
    wire_color.wire_color_status = WireSemanticStatus::Resolved;
    wire_color.wire_color = "red";
    model.wire_semantics.push_back(wire_color);

    TextRegion text_region;
    text_region.id = "text_region_1";
    text_region.kind = TextRegionKind::WireLabel;
    model.text_regions.push_back(text_region);
    TextRecognitionEvidence ocr;
    ocr.text_region_id = text_region.id;
    ocr.raw_text = "R";
    ocr.confidence = ConfidenceClass::High;
    model.text_recognition_evidence.push_back(ocr);
    SemanticAssociation association;
    association.id = "association_1";
    association.text_region_id = text_region.id;
    association.target_id = endpoint.id;
    model.semantic_associations.push_back(association);

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

        // AP-GT-002A: the schema exposes counts whose names match their
        // actual model populations. Legacy fields remain but are explicitly
        // documented so existing integrity checks keep their old contract.
        assert(manifest.find("\"schema_version\": 2") != std::string::npos);
        assert(manifest.find("\"conductor_segments\": 1") != std::string::npos);
        assert(manifest.find("\"wire_color_resolved_wires\": 1") != std::string::npos);
        assert(manifest.find("\"terminal_candidates_total\": 2") != std::string::npos);
        assert(manifest.find("\"physical_terminal_candidates\": 1") != std::string::npos);
        assert(manifest.find("\"component_terminal_candidates\": 1") != std::string::npos);
        assert(manifest.find("\"ground_connection_candidates\": 1") != std::string::npos);
        assert(manifest.find("\"wire_colors\": 1") != std::string::npos);
        assert(manifest.find("\"terminals\": 2") != std::string::npos);
        assert(manifest.find("\"labels\": 1") != std::string::npos);
        assert(manifest.find("\"semantic_associations\": 1") != std::string::npos);
        assert(manifest.find("\"text_regions\": 1") != std::string::npos);
        assert(manifest.find("\"ocr_observations\": 1") != std::string::npos);
        assert(manifest.find("Deprecated alias for conductor_segments") != std::string::npos);
        assert(manifest.find("includes ground-connection candidates") != std::string::npos);
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
