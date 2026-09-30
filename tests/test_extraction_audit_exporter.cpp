#include "eke_dx_wire/export/extraction_audit_exporter.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
using namespace eke::dx::wire;

int main() {
    const std::filesystem::path a = "extraction_audit_test_a.json";
    const std::filesystem::path b = "extraction_audit_test_b.json";
    WireModel model;
    model.source_id = "fixture.png";
    model.page = 0;
    model.image_width = 100;
    model.image_height = 80;
    ComponentCandidate component;
    component.id = "component_2";
    component.kind = ComponentCandidateKind::CircularSymbol;
    model.component_candidates.push_back(component);
    EndpointCandidate endpoint;
    endpoint.id = "endpoint_2";
    endpoint.node_id = "node_2";
    endpoint.kind = EndpointKind::ComponentTerminal;
    endpoint.component_id = component.id;
    model.endpoint_candidates.push_back(endpoint);
    Wire wire;
    wire.id = "wire_2";
    wire.start_endpoint = endpoint.id;
    wire.end_endpoint = endpoint.id;
    model.wires.push_back(wire);
    ElectricalNet net;
    net.id = "net_2";
    net.endpoint_ids = {endpoint.id};
    model.electrical_nets.push_back(net);

    const auto write = [&](const std::filesystem::path& path) {
        ExtractionAuditExporter::export_json(model, path);
        std::ifstream in(path);
        return std::string((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    };
    const std::string first = write(a);
    const std::string second = write(b);
    assert(first == second);
    assert(first.find("\"schema_version\": 2") != std::string::npos);
    assert(first.find("\"components\"") != std::string::npos);
    assert(first.find("\"endpoint_2\"") != std::string::npos);
    assert(first.find("\"wire_2\"") != std::string::npos);
    assert(first.find("\"net_2\"") != std::string::npos);
    assert(first.find("\"coverage\"") != std::string::npos);
    std::filesystem::remove(a);
    std::filesystem::remove(b);
    return 0;
}
