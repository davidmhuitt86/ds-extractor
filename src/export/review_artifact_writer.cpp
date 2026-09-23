#include "eke_dx_wire/export/review_artifact_writer.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace eke::dx::wire {
namespace {

cv::Scalar kind_color(ComponentSymbolKind kind) {
    switch (kind) {
    case ComponentSymbolKind::ChassisGround:
        return cv::Scalar(60, 200, 60);
    case ComponentSymbolKind::CircularSymbol:
        return cv::Scalar(220, 140, 40);
    case ComponentSymbolKind::Enclosure:
        return cv::Scalar(40, 160, 220);
    case ComponentSymbolKind::PrimitiveSymbol:
        return cv::Scalar(200, 80, 180);
    case ComponentSymbolKind::Unknown:
        return cv::Scalar(80, 80, 220);
    }
    return cv::Scalar(80, 80, 220);
}

const char* symbol_name(ComponentSymbolKind kind) {
    switch (kind) {
    case ComponentSymbolKind::Enclosure: return "enclosure";
    case ComponentSymbolKind::CircularSymbol: return "circular";
    case ComponentSymbolKind::ChassisGround: return "ground";
    case ComponentSymbolKind::PrimitiveSymbol: return "primitive";
    case ComponentSymbolKind::Unknown: return "unknown";
    }
    return "unknown";
}

cv::Scalar endpoint_color(const EndpointCandidate& endpoint) {
    if (endpoint.kind == EndpointKind::Ground)
        return cv::Scalar(60, 210, 60);
    if (endpoint.terminal_role == TerminalRole::ConnectorTerminal)
        return cv::Scalar(40, 180, 255);
    if (endpoint.terminal_role == TerminalRole::ComponentTerminal)
        return cv::Scalar(255, 190, 40);
    if (endpoint.kind == EndpointKind::GeometricConductorEnd)
        return cv::Scalar(0, 210, 255);
    return cv::Scalar(180, 80, 220);
}

cv::Point point(Point2D p) {
    return {
        static_cast<int>(std::lround(p.x)),
        static_cast<int>(std::lround(p.y))
    };
}

cv::Rect bounds(const BoundingBox& b) {
    return {
        static_cast<int>(std::lround(b.x)),
        static_cast<int>(std::lround(b.y)),
        (std::max)(1, static_cast<int>(std::lround(b.width))),
        (std::max)(1, static_cast<int>(std::lround(b.height)))
    };
}

void line(cv::Mat& image, Point2D a, Point2D b, cv::Scalar color, int width = 2) {
    cv::line(image, point(a), point(b), color, width, cv::LINE_AA);
}

const EndpointCandidate* endpoint(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(
        model.endpoint_candidates.begin(),
        model.endpoint_candidates.end(),
        [&](const EndpointCandidate& value) { return value.id == id; });
    return it == model.endpoint_candidates.end() ? nullptr : &*it;
}

const ComponentCandidate* component(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(
        model.component_candidates.begin(),
        model.component_candidates.end(),
        [&](const ComponentCandidate& value) { return value.id == id; });
    return it == model.component_candidates.end() ? nullptr : &*it;
}

void render_wires(cv::Mat& image, const WireModel& model) {
    for (const auto& wire : model.wires) {
        for (const auto& segment_id : wire.conductor_segments) {
            const auto it = std::find_if(
                model.conductor_segments.begin(),
                model.conductor_segments.end(),
                [&](const auto& segment) { return segment.id == segment_id; });
            if (it != model.conductor_segments.end())
                line(image, it->geometry.a, it->geometry.b,
                     cv::Scalar(0, 0, 220), 3);
        }
    }
}

void render_wire_colors(cv::Mat& image, const WireModel& model) {
    for (const auto& wire : model.wires) {
        const auto* start = endpoint(model, wire.start_endpoint);
        const auto* finish = endpoint(model, wire.end_endpoint);
        if (!start || !finish)
            continue;

        const std::string& color =
            !start->wire_color.empty() ? start->wire_color : finish->wire_color;
        if (color.empty())
            continue;

        std::string normalized = color;
        std::transform(
            normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

        cv::Scalar value(0, 0, 220);
        if (normalized.find("black") != std::string::npos) value = {20, 20, 20};
        else if (normalized.find("white") != std::string::npos) value = {245, 245, 245};
        else if (normalized.find("red") != std::string::npos) value = {40, 40, 220};
        else if (normalized.find("orange") != std::string::npos) value = {20, 150, 240};
        else if (normalized.find("yellow") != std::string::npos) value = {40, 220, 240};
        else if (normalized.find("green") != std::string::npos) value = {50, 180, 60};
        else if (normalized.find("blue") != std::string::npos) value = {220, 80, 40};
        else if (normalized.find("brown") != std::string::npos) value = {50, 80, 130};
        else if (normalized.find("gray") != std::string::npos ||
                 normalized.find("grey") != std::string::npos) value = {130, 130, 130};

        for (const auto& segment_id : wire.conductor_segments) {
            const auto it = std::find_if(
                model.conductor_segments.begin(),
                model.conductor_segments.end(),
                [&](const auto& segment) { return segment.id == segment_id; });
            if (it != model.conductor_segments.end())
                line(image, it->geometry.a, it->geometry.b, value, 4);
        }
    }
}

void render_symbols(cv::Mat& image, const WireModel& model, bool labels) {
    for (const auto& recognition : model.component_symbol_recognitions) {
        const auto* item = component(model, recognition.component_id);
        if (!item)
            continue;

        const cv::Rect r = bounds(item->bounds);
        cv::rectangle(image, r, kind_color(recognition.symbol_kind), 2, cv::LINE_AA);

        if (labels) {
            cv::putText(
                image,
                symbol_name(recognition.symbol_kind),
                {r.x, (std::max)(12, r.y - 4)},
                cv::FONT_HERSHEY_SIMPLEX,
                0.42,
                kind_color(recognition.symbol_kind),
                1,
                cv::LINE_AA);
        }
    }
}

void render_terminals(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.endpoint_candidates) {
        if (item.terminal_role != TerminalRole::ComponentTerminal &&
            item.terminal_role != TerminalRole::ConnectorTerminal)
            continue;

        const cv::Point p = point(item.position);
        const cv::Scalar color = endpoint_color(item);

        if (item.terminal_role == TerminalRole::ConnectorTerminal) {
            std::vector<cv::Point> diamond{
                {p.x, p.y - 7}, {p.x + 7, p.y},
                {p.x, p.y + 7}, {p.x - 7, p.y}};
            cv::polylines(image, diamond, true, color, 2, cv::LINE_AA);
        } else {
            cv::circle(image, p, 5, color, 2, cv::LINE_AA);
        }
    }
}

void render_connectors(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.connector_candidates)
        cv::rectangle(image, bounds(item.bounds), cv::Scalar(255, 140, 40), 2);

    for (const auto& item : model.connector_terminals)
        cv::circle(image, point(item.position), 4,
                   cv::Scalar(255, 220, 50), cv::FILLED, cv::LINE_AA);
}

void render_splices(cv::Mat& image, const WireModel& model) {
    for (const auto& node : model.nodes) {
        if (node.type != TopologyNodeType::Splice &&
            node.type != TopologyNodeType::Junction)
            continue;

        cv::circle(image, point(node.position), 6,
                   cv::Scalar(0, 210, 140), 2, cv::LINE_AA);
    }
}

void render_grounds(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.endpoint_candidates) {
        if (item.kind != EndpointKind::Ground)
            continue;

        const cv::Point p = point(item.position);
        line(image, {static_cast<double>(p.x), static_cast<double>(p.y)},
             {static_cast<double>(p.x), static_cast<double>(p.y + 10)},
             cv::Scalar(60, 210, 60), 2);
        line(image, {static_cast<double>(p.x - 8), static_cast<double>(p.y + 10)},
             {static_cast<double>(p.x + 8), static_cast<double>(p.y + 10)},
             cv::Scalar(60, 210, 60), 2);
        line(image, {static_cast<double>(p.x - 5), static_cast<double>(p.y + 14)},
             {static_cast<double>(p.x + 5), static_cast<double>(p.y + 14)},
             cv::Scalar(60, 210, 60), 2);
    }
}

void render_labels(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.component_candidates) {
        if (item.semantic_labels.empty())
            continue;
        cv::putText(
            image,
            item.semantic_labels.front(),
            cv::Point(static_cast<int>(item.bounds.x), static_cast<int>(item.bounds.y - 5)),
            cv::FONT_HERSHEY_SIMPLEX,
            0.45,
            cv::Scalar(40, 40, 220),
            1,
            cv::LINE_AA);
    }

    for (const auto& item : model.endpoint_candidates) {
        std::string label = item.terminal_name;
        if (label.empty()) label = item.function_label;
        if (label.empty()) label = item.wire_color;
        if (label.empty()) continue;

        const cv::Point p = point(item.position);
        cv::putText(image, label, {p.x + 6, p.y - 6},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    cv::Scalar(40, 40, 220), 1, cv::LINE_AA);
    }
}

void render_direction(cv::Mat& image, const WireModel& model) {
    for (const auto& wire : model.wires) {
        const auto* start = endpoint(model, wire.start_endpoint);
        const auto* finish = endpoint(model, wire.end_endpoint);
        if (!start || !finish) continue;

        const cv::Point a = point(start->position);
        const cv::Point b = point(finish->position);
        const cv::Point mid((a.x + b.x) / 2, (a.y + b.y) / 2);
        cv::arrowedLine(image, mid, b, cv::Scalar(80, 220, 255),
                        2, cv::LINE_AA, 0, 0.25);
    }
}

void render_topology(cv::Mat& image, const WireModel& model) {
    for (const auto& edge : model.edges) {
        const auto from = std::find_if(
            model.nodes.begin(), model.nodes.end(),
            [&](const auto& n) { return n.id == edge.from_node; });
        const auto to = std::find_if(
            model.nodes.begin(), model.nodes.end(),
            [&](const auto& n) { return n.id == edge.to_node; });
        if (from == model.nodes.end() || to == model.nodes.end())
            continue;

        line(image, from->position, to->position,
             cv::Scalar(220, 80, 180), 1);
    }
}

void render_bounds(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.component_candidates)
        cv::rectangle(image, bounds(item.bounds),
                      cv::Scalar(180, 100, 220), 2);
}

void render_endpoints(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.endpoint_candidates) {
        const cv::Point p = point(item.position);
        cv::circle(image, p, 7, endpoint_color(item), 2, cv::LINE_AA);
    }
}

void render_recognition(cv::Mat& image, const WireModel& model) {
    for (const auto& item : model.component_symbol_recognitions) {
        const auto* component_item = component(model, item.component_id);
        if (!component_item) continue;

        const cv::Rect r = bounds(component_item->bounds);
        cv::rectangle(image, r, kind_color(item.symbol_kind), 3, cv::LINE_AA);

        const char* status_label = "unresolved";
        switch (item.status) {
        case ComponentSymbolRecognitionStatus::Recognized:
            status_label = "recognized";
            break;
        case ComponentSymbolRecognitionStatus::GeometricallyClassified:
            status_label = "geometry-bucketed";
            break;
        case ComponentSymbolRecognitionStatus::Unresolved:
            status_label = "unresolved";
            break;
        case ComponentSymbolRecognitionStatus::Conflicted:
            status_label = "conflicted";
            break;
        }
        const std::string label =
            std::string(symbol_name(item.symbol_kind)) + " [" + status_label + "]";
        cv::putText(image, label, {r.x, r.y + r.height + 15},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40,
                    kind_color(item.symbol_kind), 1, cv::LINE_AA);
    }
}

void write_image(
    const cv::Mat& normalized,
    const WireModel& model,
    const fs::path& path,
    void (*renderer)(cv::Mat&, const WireModel&)) {

    cv::Mat image;
    if (normalized.channels() == 1)
        cv::cvtColor(normalized, image, cv::COLOR_GRAY2BGR);
    else
        normalized.copyTo(image);

    renderer(image, model);

    if (!cv::imwrite(path.string(), image))
        throw std::runtime_error("Unable to write review image: " + path.string());
}

void write_combined(
    const cv::Mat& normalized,
    const WireModel& model,
    const fs::path& path) {
    cv::Mat image;
    if (normalized.channels() == 1)
        cv::cvtColor(normalized, image, cv::COLOR_GRAY2BGR);
    else
        normalized.copyTo(image);

    render_wires(image, model);
    render_symbols(image, model, false);
    render_terminals(image, model);
    render_connectors(image, model);
    render_splices(image, model);
    render_grounds(image, model);
    render_labels(image, model);
    render_direction(image, model);
    render_topology(image, model);
    render_bounds(image, model);
    render_endpoints(image, model);
    render_recognition(image, model);

    if (!cv::imwrite(path.string(), image))
        throw std::runtime_error("Unable to write review image: " + path.string());
}

std::string generation_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

void write_manifest(const WireModel& model, const fs::path& path) {
    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Unable to create review manifest: " + path.string());

    out << "{" << '\n'
        << "  \"generated_at\": \"" << generation_timestamp() << "\"," << '\n'
        << "  \"source_id\": \"" << model.source_id << "\"," << '\n'
        << "  \"page\": " << model.page << "," << '\n'
        << "  \"image_width\": " << model.image_width << "," << '\n'
        << "  \"image_height\": " << model.image_height << "," << '\n'
        << "  \"layers\": {" << '\n'
        << "    \"wires\": " << model.wires.size() << "," << '\n'
        << "    \"wire_colors\": " << model.conductor_segments.size() << "," << '\n'
        << "    \"symbols\": " << model.component_candidates.size() << "," << '\n'
        << "    \"terminals\": " << model.terminal_candidates.size() << "," << '\n'
        << "    \"connectors\": " << model.connector_candidates.size() << "," << '\n'
        << "    \"connector_terminals\": " << model.connector_terminals.size() << "," << '\n'
        << "    \"splices_and_junctions\": " << model.nodes.size() << "," << '\n'
        << "    \"grounds\": " << model.audit.ground_endpoints << "," << '\n'
        << "    \"labels\": " << model.semantic_associations.size() << "," << '\n'
        << "    \"topology_edges\": " << model.edges.size() << "," << '\n'
        << "    \"component_bounds\": " << model.component_candidates.size() << "," << '\n'
        << "    \"endpoint_debug\": " << model.endpoint_candidates.size() << "," << '\n'
        << "    \"recognition\": " << model.component_symbol_recognitions.size() << '\n'
        << "  }," << '\n'
        << "  \"electrical_nets\": " << model.electrical_nets.size() << "," << '\n'
        << "  \"validation_errors\": " << model.audit.validation_errors << "," << '\n'
        << "  \"validation_warnings\": " << model.audit.validation_warnings << "," << '\n'
        << "  \"validation_warning_codes\": {" << '\n';

    for (std::size_t i = 0; i < model.audit.validation_warning_summaries.size(); ++i) {
        const auto& summary = model.audit.validation_warning_summaries[i];
        out << "    \"" << summary.code << "\": " << summary.count
            << (i + 1 == model.audit.validation_warning_summaries.size() ? "\n" : ",\n");
    }

    out << "  }" << '\n'
        << "}" << '\n';
}

} // namespace

void ReviewArtifactWriter::write(
    const WireModel& model,
    const cv::Mat& normalized,
    const fs::path& output_root) {

    const fs::path review = output_root / "artifacts" / "extraction_review";
    std::error_code error;
    fs::remove_all(review, error);
    if (error)
        throw std::runtime_error("Unable to clear extraction review directory: " +
                                 review.string());

    fs::create_directories(review);

    if (normalized.empty())
        throw std::runtime_error("Cannot create extraction review from empty image");

    cv::Mat source;
    if (normalized.channels() == 1)
        cv::cvtColor(normalized, source, cv::COLOR_GRAY2BGR);
    else
        normalized.copyTo(source);

    if (!cv::imwrite((review / "00_source.png").string(), source))
        throw std::runtime_error("Unable to write source review image");

    write_image(normalized, model, review / "01_wires.png", render_wires);
    write_image(normalized, model, review / "02_wire_colors.png", render_wire_colors);
    write_image(normalized, model, review / "03_symbols.png",
                [](cv::Mat& image, const WireModel& value) {
                    render_symbols(image, value, true);
                });
    write_image(normalized, model, review / "04_terminals.png", render_terminals);
    write_image(normalized, model, review / "05_connectors.png", render_connectors);
    write_image(normalized, model, review / "06_splices.png", render_splices);
    write_image(normalized, model, review / "07_grounds.png", render_grounds);
    write_image(normalized, model, review / "08_labels.png", render_labels);
    write_image(normalized, model, review / "09_topology.png", render_topology);
    write_image(normalized, model, review / "10_component_bounds.png", render_bounds);
    write_image(normalized, model, review / "11_endpoint_debug.png", render_endpoints);
    write_image(normalized, model, review / "12_recognition.png", render_recognition);
    write_combined(normalized, model, review / "13_combined.png");
    write_manifest(model, review / "review_manifest.json");
}

} // namespace eke::dx::wire
