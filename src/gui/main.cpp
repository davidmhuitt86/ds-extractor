#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/image/image_loader.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include <algorithm>
#include <iostream>
#include <string>

using namespace eke::dx::wire;

namespace {
constexpr char kWindow[] = "DS Extractor";
constexpr int kToolbarHeight = 54;

#ifdef _WIN32
std::string open_image_dialog() {
    wchar_t buffer[32768] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
    dialog.lpstrFilter =
        L"Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0"
        L"All Files\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) return {};

    const int length = WideCharToMultiByte(
        CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};

    std::string utf8(static_cast<std::size_t>(length - 1), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, buffer, -1, utf8.data(), length, nullptr, nullptr);
    return utf8;
}
#else
std::string open_image_dialog() {
    std::cerr << "Windows file picker is not available on this platform.\n";
    return {};
}
#endif

struct GuiState {
    std::string image_path;
    cv::Mat source;
    WireModel model;
    bool extracted = false;
    bool show_source = true;
    bool show_conductors = true;
    bool show_topology = false;
};

void load_image(GuiState& state, const std::string& path) {
    if (path.empty()) return;
    state.source = ImageLoader::load(path);
    state.image_path = path;
    state.model = {};
    state.extracted = false;
}

void extract(GuiState& state) {
    if (state.image_path.empty()) return;
    ExtractionPipeline pipeline;
    state.model = pipeline.run(state.image_path, state.image_path);
    state.extracted = true;
}

cv::Mat render(const GuiState& state, cv::Size canvas_size) {
    canvas_size.width = std::max(canvas_size.width, 800);
    canvas_size.height = std::max(canvas_size.height, 600);

    cv::Mat canvas(canvas_size, CV_8UC3, cv::Scalar(245, 245, 245));
    cv::rectangle(canvas, {0, 0}, {canvas.cols, kToolbarHeight},
                  cv::Scalar(35, 35, 35), cv::FILLED);

    auto button = [&](int x, int width, const std::string& label) {
        cv::rectangle(canvas, {x, 7}, {x + width, 47},
                      cv::Scalar(65, 65, 65), cv::FILLED);
        cv::putText(canvas, label, {x + 12, 34},
                    cv::FONT_HERSHEY_SIMPLEX, 0.62,
                    cv::Scalar(240, 240, 240), 1, cv::LINE_AA);
    };

    button(10, 100, "OPEN");
    button(120, 110, "EXTRACT");
    button(240, 100, "SOURCE");
    button(350, 130, "CONDUCTORS");
    button(490, 110, "TOPOLOGY");

    if (state.source.empty()) {
        cv::putText(canvas, "Open a wiring diagram to begin.",
                    {30, kToolbarHeight + 50}, cv::FONT_HERSHEY_SIMPLEX,
                    0.8, cv::Scalar(70, 70, 70), 1, cv::LINE_AA);
        return canvas;
    }

    const int available_h = canvas.rows - kToolbarHeight - 32;
    const double scale = std::min(
        static_cast<double>(canvas.cols - 32) / state.source.cols,
        static_cast<double>(available_h) / state.source.rows);

    const cv::Size display_size(
        std::max(1, static_cast<int>(state.source.cols * scale)),
        std::max(1, static_cast<int>(state.source.rows * scale)));

    const cv::Point offset(
        (canvas.cols - display_size.width) / 2,
        kToolbarHeight + 10 + (available_h - display_size.height) / 2);

    cv::Mat display;
    cv::resize(state.source, display, display_size, 0, 0, cv::INTER_AREA);
    if (!state.show_source) display.setTo(cv::Scalar(255, 255, 255));

    if (state.extracted && state.show_conductors) {
        for (const auto& segment : state.model.conductor_segments) {
            cv::Point a(
                static_cast<int>(segment.geometry.a.x * scale),
                static_cast<int>(segment.geometry.a.y * scale));
            cv::Point b(
                static_cast<int>(segment.geometry.b.x * scale),
                static_cast<int>(segment.geometry.b.y * scale));
            cv::line(display, a, b, cv::Scalar(0, 0, 220), 1, cv::LINE_AA);
        }
    }

    if (state.extracted && state.show_topology) {
        for (const auto& node : state.model.nodes) {
            cv::Point p(
                static_cast<int>(node.position.x * scale),
                static_cast<int>(node.position.y * scale));
            const cv::Scalar color =
                node.type == TopologyNodeType::Crossing
                    ? cv::Scalar(220, 120, 0)
                    : cv::Scalar(0, 180, 0);
            cv::circle(display, p, 3, color, cv::FILLED, cv::LINE_AA);
        }
    }

    display.copyTo(canvas(cv::Rect(offset.x, offset.y,
                                   display.cols, display.rows)));

    std::string status = state.extracted
        ? "Segments: " + std::to_string(state.model.conductor_segments.size()) +
          "  Nodes: " + std::to_string(state.model.nodes.size()) +
          "  Edges: " + std::to_string(state.model.edges.size())
        : "Loaded: " + state.image_path;

    cv::rectangle(canvas, {0, canvas.rows - 32},
                  {canvas.cols, canvas.rows},
                  cv::Scalar(35, 35, 35), cv::FILLED);
    cv::putText(canvas, status, {12, canvas.rows - 10},
                cv::FONT_HERSHEY_SIMPLEX, 0.52,
                cv::Scalar(235, 235, 235), 1, cv::LINE_AA);
    return canvas;
}

void on_mouse(int event, int x, int y, int, void* userdata) {
    auto* state = static_cast<GuiState*>(userdata);
    if (event != cv::EVENT_LBUTTONUP || y < 0 || y > kToolbarHeight) return;

    try {
        if (x >= 10 && x < 110) load_image(*state, open_image_dialog());
        else if (x >= 120 && x < 230) extract(*state);
        else if (x >= 240 && x < 340) state->show_source = !state->show_source;
        else if (x >= 350 && x < 480) state->show_conductors = !state->show_conductors;
        else if (x >= 490 && x < 600) state->show_topology = !state->show_topology;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
    }
}
}

int main(int argc, char** argv) {
    try {
        GuiState state;
        if (argc > 1) load_image(state, argv[1]);

        cv::namedWindow(kWindow, cv::WINDOW_NORMAL);
        cv::resizeWindow(kWindow, 1200, 800);
        cv::setMouseCallback(kWindow, on_mouse, &state);

        while (true) {
            cv::Size size = cv::getWindowImageRect(kWindow).size();
            cv::imshow(kWindow, render(state, size));
            const int key = cv::waitKey(30);
            if (key == 27 || key == 'q' || key == 'Q') break;
            if (key == 'e' || key == 'E') extract(state);
            if (key == 'o' || key == 'O') load_image(state, open_image_dialog());
        }

        cv::destroyAllWindows();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
