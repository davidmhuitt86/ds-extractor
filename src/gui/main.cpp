#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

using namespace eke::dx::wire;

namespace {
void gui_log(const std::string& message) {
    std::ofstream out("dx-extractor-gui.log", std::ios::app);
    out << message << '\\n';
    out.flush();
}

constexpr char kWindow[] = "DS Extractor - Calibration Workbench";
constexpr int kToolbarHeight = 48;
constexpr int kPanelWidth = 330;
constexpr int kStatusHeight = 54;

#ifdef _WIN32
std::string open_image_dialog() {
    gui_log("DIALOG: enter");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool initialized = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        gui_log("DIALOG: CoInitializeEx failed hr=" + std::to_string(static_cast<long>(hr)));
        return {};
    }

    IFileOpenDialog* dialog = nullptr;
    hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) {
        gui_log("DIALOG: CoCreateInstance failed hr=" + std::to_string(static_cast<long>(hr)));
        if (initialized) CoUninitialize();
        return {};
    }

    COMDLG_FILTERSPEC filters[] = {
        {L"Image Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff"},
        {L"All Files", L"*.*"}
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);

    gui_log("DIALOG: showing IFileOpenDialog");
    hr = dialog->Show(nullptr);
    gui_log("DIALOG: Show returned hr=" + std::to_string(static_cast<long>(hr)));

    std::string result;
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        hr = dialog->GetResult(&item);
        gui_log("DIALOG: GetResult hr=" + std::to_string(static_cast<long>(hr)));

        if (SUCCEEDED(hr) && item != nullptr) {
            PWSTR path = nullptr;
            hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
            gui_log("DIALOG: GetDisplayName hr=" + std::to_string(static_cast<long>(hr)));

            if (SUCCEEDED(hr) && path != nullptr) {
                const int length = WideCharToMultiByte(
                    CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                if (length > 0) {
                    result.resize(static_cast<std::size_t>(length - 1));
                    if (WideCharToMultiByte(
                            CP_UTF8, 0, path, -1, result.data(), length,
                            nullptr, nullptr) <= 0) {
                        result.clear();
                    }
                }
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();
    if (initialized) CoUninitialize();
    gui_log("DIALOG: exit path length=" + std::to_string(result.size()));
    return result;
}
#else
std::string open_image_dialog() {
    std::cerr << "Windows file picker is not available on this platform.\n";
    return {};
}
#endif

enum class ViewMode {
    Source,
    Binary,
    HorizontalMask,
    VerticalMask,
    Conductors,
    Topology
};

struct GuiState {
    std::string image_path;
    cv::Mat source;
    cv::Mat normalized;
    DetectionArtifacts detection;
    TopologyArtifacts topology;

    MorphologyConfig config {};
    bool extracted = false;
    ViewMode view = ViewMode::Conductors;

    int active_slider = -1;
    bool dragging = false;
    int canvas_width = 1400;
    bool request_open = false;
};

struct Slider {
    const char* label;
    int min;
    int max;
    int* value;
};

void load_image(GuiState& state, const std::string& path) {
    gui_log("LOAD: enter");
    if (path.empty()) {
        gui_log("LOAD: empty path");
        return;
    }
    gui_log("LOAD: path=" + path);

    gui_log("LOAD: calling ImageLoader::load");
    cv::Mat image = ImageLoader::load(path);
    gui_log("LOAD: ImageLoader returned rows=" + std::to_string(image.rows) +
            " cols=" + std::to_string(image.cols) +
            " channels=" + std::to_string(image.channels()));
    state.source = std::move(image);
    gui_log("LOAD: state.source assigned");
    state.normalized.release();
    state.detection = {};
    state.topology = {};
    state.image_path = path;
    state.extracted = false;
    gui_log("LOAD: state updated; exit");
}

void extract(GuiState& state) {
    if (state.image_path.empty()) return;

    state.normalized = ImageNormalizer::normalize(state.source);

    MorphologyWireDetector detector(state.config);
    state.detection = detector.detect(
        state.normalized, state.image_path, 0);

    TopologyReconstructor topology;
    state.topology = topology.reconstruct(
        state.detection.conductor_segments, state.image_path, 0);

    state.extracted = true;
}

void reset_parameters(GuiState& state) {
    state.config = MorphologyConfig {};
    state.extracted = false;
}

std::vector<Slider> sliders(GuiState& state) {
    return {
        {"H Kernel", 3, 101, &state.config.horizontal_kernel_length},
        {"V Kernel", 3, 101, &state.config.vertical_kernel_length},
        {"Adaptive Block", 3, 101, &state.config.adaptive_block_size},
        {"Adaptive C", -20, 30, &state.config.adaptive_c},
        {"Min Length", 1, 100, &state.config.minimum_segment_length},
        {"Min Area", 1, 100, &state.config.minimum_component_area}
    };
}

int slider_y(int index) {
    return 170 + index * 64;
}

void set_slider_from_mouse(GuiState& state, int index, int mouse_x) {
    auto s = sliders(state);
    if (index < 0 || index >= static_cast<int>(s.size())) return;

    const int x0 = 155;
    const int x1 = kPanelWidth - 25;
    const int clamped = (std::max)(x0, (std::min)(x1, mouse_x));

    const double t = static_cast<double>(clamped - x0) /
                     static_cast<double>(x1 - x0);

    int value = static_cast<int>(
        s[index].min + t * (s[index].max - s[index].min) + 0.5);

    // OpenCV adaptiveThreshold requires an odd block size > 1.
    if (index == 2) {
        value = (std::max)(3, value);
        if ((value & 1) == 0) ++value;
        value = (std::min)(101, value);
    }

    *s[index].value = value;
}

void draw_slider(cv::Mat& panel, int index, const Slider& slider) {
    const int y = slider_y(index);
    cv::putText(panel, slider.label, {18, y - 12},
                cv::FONT_HERSHEY_SIMPLEX, 0.55,
                cv::Scalar(225, 225, 225), 1, cv::LINE_AA);

    cv::line(panel, {155, y - 8}, {kPanelWidth - 25, y - 8},
             cv::Scalar(90, 90, 90), 4, cv::LINE_AA);

    const double t = static_cast<double>(*slider.value - slider.min) /
                     static_cast<double>(slider.max - slider.min);
    const int knob = 155 + static_cast<int>(
        t * (kPanelWidth - 180));

    cv::line(panel, {155, y - 8}, {knob, y - 8},
             cv::Scalar(150, 150, 150), 4, cv::LINE_AA);

    cv::circle(panel, {knob, y - 8}, 7,
               cv::Scalar(235, 235, 235), cv::FILLED, cv::LINE_AA);

    cv::putText(panel, std::to_string(*slider.value),
                {kPanelWidth - 70, y - 12},
                cv::FONT_HERSHEY_SIMPLEX, 0.52,
                cv::Scalar(240, 240, 240), 1, cv::LINE_AA);
}

void draw_button(cv::Mat& panel, cv::Rect rect, const std::string& label,
                 bool active = false) {
    const cv::Scalar fill = active
        ? cv::Scalar(85, 105, 85)
        : cv::Scalar(60, 60, 60);

    cv::rectangle(panel, rect, fill, cv::FILLED);
    cv::rectangle(panel, rect, cv::Scalar(110, 110, 110), 1);

    cv::putText(panel, label,
                {rect.x + 10, rect.y + rect.height - 9},
                cv::FONT_HERSHEY_SIMPLEX, 0.48,
                cv::Scalar(235, 235, 235), 1, cv::LINE_AA);
}

cv::Mat make_view(const GuiState& state) {
    if (!state.extracted) return state.source;

    switch (state.view) {
    case ViewMode::Binary:
        return state.detection.binary;
    case ViewMode::HorizontalMask:
        return state.detection.horizontal_mask;
    case ViewMode::VerticalMask:
        return state.detection.vertical_mask;
    default:
        return state.source;
    }
}

void overlay_conductors(
    cv::Mat& display,
    const GuiState& state,
    double scale) {

    for (const auto& segment : state.detection.conductor_segments) {
        cv::Point a(
            static_cast<int>(segment.geometry.a.x * scale),
            static_cast<int>(segment.geometry.a.y * scale));
        cv::Point b(
            static_cast<int>(segment.geometry.b.x * scale),
            static_cast<int>(segment.geometry.b.y * scale));

        cv::line(display, a, b, cv::Scalar(0, 0, 220), 1, cv::LINE_AA);
    }
}

void overlay_topology(
    cv::Mat& display,
    const GuiState& state,
    double scale) {

    for (const auto& node : state.topology.nodes) {
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

cv::Mat render(const GuiState& state, cv::Size canvas_size) {
    canvas_size.width = (std::max)(canvas_size.width, 1100);
    canvas_size.height = (std::max)(canvas_size.height, 760);

    cv::Mat canvas(
        canvas_size, CV_8UC3, cv::Scalar(238, 238, 238));

    cv::rectangle(
        canvas, {0, 0}, {canvas.cols, kToolbarHeight},
        cv::Scalar(35, 35, 35), cv::FILLED);

    auto toolbar_button = [&](int x, int width, const std::string& label) {
        cv::rectangle(
            canvas, {x, 5}, {x + width, 43},
            cv::Scalar(65, 65, 65), cv::FILLED);
        cv::putText(
            canvas, label, {x + 10, 30},
            cv::FONT_HERSHEY_SIMPLEX, 0.55,
            cv::Scalar(240, 240, 240), 1, cv::LINE_AA);
    };

    toolbar_button(10, 90, "OPEN");
    toolbar_button(110, 95, "EXTRACT");
    toolbar_button(215, 90, "RESET");
    toolbar_button(315, 105, "CONDUCTORS");
    toolbar_button(430, 95, "TOPOLOGY");

    const int image_width = canvas.cols - kPanelWidth;
    const int image_height =
        canvas.rows - kToolbarHeight - kStatusHeight;

    cv::Mat view = make_view(state);

    if (!view.empty()) {
        cv::Mat color_view;

        if (view.channels() == 1)
            cv::cvtColor(view, color_view, cv::COLOR_GRAY2BGR);
        else
            color_view = view.clone();

        const double scale = (std::min)(
            static_cast<double>(image_width - 24) / color_view.cols,
            static_cast<double>(image_height - 24) / color_view.rows);

        const cv::Size display_size(
            (std::max)(1, static_cast<int>(color_view.cols * scale)),
            (std::max)(1, static_cast<int>(color_view.rows * scale)));

        cv::resize(
            color_view, color_view, display_size,
            0, 0, cv::INTER_AREA);

        if (state.view == ViewMode::Conductors)
            overlay_conductors(color_view, state, scale);
        else if (state.view == ViewMode::Topology) {
            overlay_conductors(color_view, state, scale);
            overlay_topology(color_view, state, scale);
        }

        const int ox = (image_width - color_view.cols) / 2;
        const int oy =
            kToolbarHeight +
            (image_height - color_view.rows) / 2;

        color_view.copyTo(canvas(
            cv::Rect(ox, oy, color_view.cols, color_view.rows)));
    } else {
        cv::putText(
            canvas, "Open a wiring diagram to begin.",
            {35, 100}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
            cv::Scalar(70, 70, 70), 1, cv::LINE_AA);
    }

    // Calibration panel.
    const int panel_left = canvas.cols - kPanelWidth;
    cv::rectangle(
        canvas,
        {panel_left, kToolbarHeight},
        {canvas.cols, canvas.rows - kStatusHeight},
        cv::Scalar(42, 42, 42), cv::FILLED);

    cv::Mat panel = canvas(cv::Rect(
        panel_left, kToolbarHeight,
        kPanelWidth, canvas.rows - kToolbarHeight - kStatusHeight));

    cv::putText(
        panel, "CALIBRATION", {18, 34},
        cv::FONT_HERSHEY_SIMPLEX, 0.72,
        cv::Scalar(245, 245, 245), 1, cv::LINE_AA);

    cv::putText(
        panel, "Morphology detector", {18, 57},
        cv::FONT_HERSHEY_SIMPLEX, 0.48,
        cv::Scalar(160, 160, 160), 1, cv::LINE_AA);

    const auto parameter_sliders = sliders(
        const_cast<GuiState&>(state));

    for (int i = 0;
         i < static_cast<int>(parameter_sliders.size()); ++i)
        draw_slider(panel, i, parameter_sliders[i]);

    draw_button(panel, {18, 507, 135, 34}, "RUN EXTRACT");
    draw_button(panel, {165, 507, 135, 34}, "RESET");

    const int button_y = 562;
    draw_button(panel, {18, button_y, 88, 30},
                "SOURCE", state.view == ViewMode::Source);
    draw_button(panel, {114, button_y, 88, 30},
                "BINARY", state.view == ViewMode::Binary);
    draw_button(panel, {210, button_y, 88, 30},
                "H-MASK", state.view == ViewMode::HorizontalMask);

    draw_button(panel, {18, button_y + 38, 88, 30},
                "V-MASK", state.view == ViewMode::VerticalMask);
    draw_button(panel, {114, button_y + 38, 88, 30},
                "WIRES", state.view == ViewMode::Conductors);
    draw_button(panel, {210, button_y + 38, 88, 30},
                "TOPOLOGY", state.view == ViewMode::Topology);

    cv::rectangle(
        canvas,
        {0, canvas.rows - kStatusHeight},
        {canvas.cols, canvas.rows},
        cv::Scalar(35, 35, 35), cv::FILLED);

    std::string status;

    if (!state.extracted) {
        status = state.image_path.empty()
            ? "No source loaded"
            : "Ready - adjust parameters, then RUN EXTRACT";
    } else {
        const std::size_t segments =
            state.detection.conductor_segments.size();
        const std::size_t nodes = state.topology.nodes.size();
        const std::size_t edges = state.topology.edges.size();

        status =
            "Segments: " + std::to_string(segments) +
            "    Nodes: " + std::to_string(nodes) +
            "    Edges: " + std::to_string(edges);
    }

    cv::putText(
        canvas, status,
        {12, canvas.rows - 30},
        cv::FONT_HERSHEY_SIMPLEX, 0.56,
        cv::Scalar(235, 235, 235), 1, cv::LINE_AA);

    if (state.extracted) {
        std::string detail =
            "H: " +
            std::to_string(std::count_if(
                state.detection.conductor_segments.begin(),
                state.detection.conductor_segments.end(),
                [](const ConductorSegment& s) {
                    return s.geometry.a.y == s.geometry.b.y;
                })) +
            "   V: " +
            std::to_string(std::count_if(
                state.detection.conductor_segments.begin(),
                state.detection.conductor_segments.end(),
                [](const ConductorSegment& s) {
                    return s.geometry.a.x == s.geometry.b.x;
                }));

        cv::putText(
            canvas, detail,
            {12, canvas.rows - 9},
            cv::FONT_HERSHEY_SIMPLEX, 0.43,
            cv::Scalar(170, 170, 170), 1, cv::LINE_AA);
    }

    return canvas;
}

void handle_mouse(
    GuiState& state,
    int event,
    int x,
    int y) {

    if (event == cv::EVENT_LBUTTONDOWN) {
        if (y < kToolbarHeight) {
            if (x >= 10 && x < 100)
                state.request_open = true;
            else if (x >= 110 && x < 205)
                extract(state);
            else if (x >= 215 && x < 305)
                reset_parameters(state);
            else if (x >= 315 && x < 420)
                state.view = ViewMode::Conductors;
            else if (x >= 430 && x < 525)
                state.view = ViewMode::Topology;
            return;
        }

        const int panel_left = state.canvas_width - kPanelWidth;
        const int panel_x = x - panel_left;

        if (x >= panel_left &&
            y >= kToolbarHeight + 135 && y < kToolbarHeight + 535) {

            const int index = (y - (kToolbarHeight + 135)) / 64;
            if (index >= 0 && index < 6) {
                state.active_slider = index;
                state.dragging = true;
                set_slider_from_mouse(state, index, panel_x);
            }
            return;
        }

        if (x >= panel_left &&
            y >= kToolbarHeight + 507 && y < kToolbarHeight + 547) {
            if (panel_x >= 18 && panel_x < 153)
                extract(state);
            else if (panel_x >= 165 && panel_x < 300)
                reset_parameters(state);
            return;
        }

        if (x >= panel_left &&
            y >= kToolbarHeight + 562 &&
            y < kToolbarHeight + 630) {
            const int bx = panel_x;

            if (y < kToolbarHeight + 592) {
                if (bx >= 18 && bx < 106) state.view = ViewMode::Source;
                else if (bx >= 114 && bx < 202) state.view = ViewMode::Binary;
                else if (bx >= 210 && bx < 298)
                    state.view = ViewMode::HorizontalMask;
            } else {
                if (bx >= 18 && bx < 106) state.view = ViewMode::VerticalMask;
                else if (bx >= 114 && bx < 202)
                    state.view = ViewMode::Conductors;
                else if (bx >= 210 && bx < 298)
                    state.view = ViewMode::Topology;
            }
        }
    }

    if (event == cv::EVENT_MOUSEMOVE && state.dragging) {
        const int panel_left = state.canvas_width - kPanelWidth;
        const int panel_x = x - panel_left;
        set_slider_from_mouse(state, state.active_slider, panel_x);
    }

    if (event == cv::EVENT_LBUTTONUP) {
        state.dragging = false;
        state.active_slider = -1;
    }
}

void on_mouse(int event, int x, int y, int flags, void* userdata) {
    (void)flags;
    auto* state = static_cast<GuiState*>(userdata);
    try {
        handle_mouse(*state, event, x, y);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        GuiState state;

        if (argc > 1)
            load_image(state, argv[1]);

        cv::namedWindow(kWindow, cv::WINDOW_NORMAL);
        cv::resizeWindow(kWindow, 1400, 850);
        cv::setMouseCallback(kWindow, on_mouse, &state);

        while (true) {
            const cv::Size size =
                cv::getWindowImageRect(kWindow).size();

            state.canvas_width = (std::max)(size.width, 1100);
            gui_log("LOOP: render");
            cv::imshow(kWindow, render(state, size));

            const int key = cv::waitKey(30);

            // Do not invoke the native Windows file dialog from the OpenCV
            // mouse callback.  The callback runs inside HighGUI's event
            // dispatch, and re-entering a native modal dialog there can cause
            // instability when control returns to HighGUI.  Queue the request
            // in the callback and service it from the main loop instead.
            if (state.request_open) {
                state.request_open = false;
                try {
                    gui_log("OPEN: calling file dialog");
                    const std::string path = open_image_dialog();
                    gui_log("OPEN: dialog returned; path length=" + std::to_string(path.size()));
                    if (!path.empty())
                        load_image(state, path);
                } catch (const std::exception& e) {
                    std::cerr << "open error: " << e.what() << '\\n';
                }
            }

            if (key == 27 || key == 'q' || key == 'Q')
                break;

            if (key == 'e' || key == 'E')
                extract(state);

            if (key == 'o' || key == 'O')
                state.request_open = true;

            if (key == '1') state.view = ViewMode::Source;
            if (key == '2') state.view = ViewMode::Binary;
            if (key == '3') state.view = ViewMode::HorizontalMask;
            if (key == '4') state.view = ViewMode::VerticalMask;
            if (key == '5') state.view = ViewMode::Conductors;
            if (key == '6') state.view = ViewMode::Topology;
        }

        cv::destroyAllWindows();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
