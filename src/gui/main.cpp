#include "eke_dx_wire/export/artifact_writer.hpp"
#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/ingest/extraction_scope.hpp"
#include "eke_dx_wire/ingest/source_scoper.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
constexpr int kStatusHeight = 72;

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
        {L"Diagram Files", L"*.pdf;*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff"},
        {L"PDF Files", L"*.pdf"},
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
                    std::vector<char> utf8_path(static_cast<std::size_t>(length));
                    if (WideCharToMultiByte(
                            CP_UTF8, 0, path, -1, utf8_path.data(), length,
                            nullptr, nullptr) > 0) {
                        result.assign(
                            utf8_path.data(),
                            static_cast<std::size_t>(length - 1));
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

#ifdef _WIN32
std::filesystem::path find_release_script() {
    wchar_t buffer[32768] {};
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer, static_cast<DWORD>(std::size(buffer)));

    if (length == 0 || length >= std::size(buffer))
        return {};

    std::filesystem::path directory(std::wstring(buffer, length));
    directory = directory.parent_path();

    for (;;) {
        const auto candidate = directory / "tools" / "dx-release.ps1";
        if (std::filesystem::exists(candidate))
            return candidate;

        const auto parent = directory.parent_path();
        if (parent == directory)
            break;

        directory = parent;
    }

    return {};
}

bool launch_release_pipeline(bool build_only = false) {
    const std::filesystem::path script = find_release_script();

    if (script.empty()) {
        MessageBoxW(
            nullptr,
            L"Could not locate tools\\dx-release.ps1.\n"
            L"Launch the GUI from a ds-extractor working tree.",
            L"DX-Extractor Release",
            MB_OK | MB_ICONERROR);
        return false;
    }

    const std::wstring script_path = script.wstring();
    std::wstring command =
        L"powershell.exe -NoProfile -NoExit -ExecutionPolicy Bypass -File \"" +
        script_path + L"\"" +
        (build_only ? L" -BuildOnly" : L"");

    std::vector<wchar_t> command_line(command.begin(), command.end());
    command_line.push_back(L'\0');

    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);

    PROCESS_INFORMATION process {};

    const std::wstring working_directory =
        script.parent_path().parent_path().wstring();

    const BOOL created = CreateProcessW(
        nullptr,
        command_line.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        working_directory.c_str(),
        &startup,
        &process);

    if (!created) {
        MessageBoxW(
            nullptr,
            L"Failed to start the release pipeline.",
            L"DX-Extractor Release",
            MB_OK | MB_ICONERROR);
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
#endif

enum class ViewMode {
    Source,
    Binary,
    HorizontalMask,
    VerticalMask,
    Conductors,
    Topology,
    Endpoints,
    Shapes
};

// AP-GUI-002: source scoping is a first-class step of opening an image, not
// an afterthought bolted onto extraction. Normal is the ordinary calibration
// workbench (unchanged). ImportChoice is the mandatory fork shown the moment
// a file is picked: import the whole diagram as-is, or mask off regions
// first. RegionEditor is where those regions are actually drawn.
enum class GuiMode {
    Normal,
    ImportChoice,
    RegionEditor
};

// A scope region as drawn in the editor, always in SOURCE image pixel
// coordinates (never screen/display coordinates) so it survives zoom/pan
// and round-trips exactly into eke::dx::wire::BoundingBox for
// ExtractionScope - the same rectangle-only model the CLI's --scope already
// uses (AP-INGEST-001/002). No new region shape is invented here.
struct EditorRegion {
    cv::Rect rect;
    bool exclude = false;
};

struct DiagramLayerState {
    bool base_diagram = true;
    bool wires = true;
    bool wire_colors = false;
    bool symbols = true;
    bool terminals = true;
    bool connectors = true;
    bool splices = true;
    bool grounds = true;
    bool labels = true;
    bool wire_direction = false;
    bool topology = false;
    bool component_bounds = false;
    bool endpoint_debug = false;
    bool recognition_evidence = false;
};

struct GuiState {
    std::string image_path;
    // AP-GUI-002: the path ExtractionPipeline actually loads pixels from.
    // Equal to image_path when no scope was applied; otherwise the on-disk
    // scoped_source.png, while image_path itself keeps being the original
    // file and remains the source_id passed to ExtractionPipeline::run -
    // exactly the effective_image_path / image_path split the CLI's
    // extract() already uses for --scope (src/app/main.cpp).
    std::string pipeline_image_path;
    std::filesystem::path artifact_root;
    cv::Mat source;
    cv::Mat normalized;
    DetectionArtifacts detection;
    std::vector<ConductorSegment> normalized_conductors;
    TopologyArtifacts topology;
    GapInterpretationArtifacts gap_interpretation;
    EndpointArtifacts endpoints;
    WireModel model;
    DiagramLayerState layers {};

    MorphologyConfig config {};
    bool extracted = false;
    ViewMode view = ViewMode::Conductors;

    // AP-GUI-002: import-choice / region-editor state.
    GuiMode mode = GuiMode::Normal;
    cv::Mat pending_source;
    std::string pending_path;
    std::vector<EditorRegion> editor_regions;
    bool editor_exclude_mode = false;
    bool editor_dragging = false;
    cv::Point editor_drag_start_screen {};
    cv::Point editor_drag_now_screen {};
    // Where the pending image is currently drawn on screen, and at what
    // scale - set each time the editor renders, read back by the mouse
    // handler to convert screen coordinates to source coordinates. Regions
    // are only ever stored in source coordinates (see EditorRegion).
    cv::Rect editor_image_screen_rect {};
    double editor_scale = 1.0;

    int active_slider = -1;
    bool dragging = false;
    int canvas_width = 1400;
    int canvas_height = 850;
    bool request_open = false;
    bool request_release = false;
    bool request_build_test = false;
    bool render_trace_pending = false;

    // Diagram viewport controls.
    double zoom = 1.0;
    cv::Point2d pan {0.0, 0.0};
    bool panning = false;
    cv::Point pan_start {};
    cv::Point2d pan_origin {0.0, 0.0};

    // Bottom status-bar output directory hyperlink hit area.
    cv::Rect output_link_rect {};
};

#ifdef _WIN32
void publish_review_artifacts(const GuiState& state);
#else
void publish_review_artifacts(const GuiState& state);
#endif

struct Slider {
    const char* label;
    int min;
    int max;
    int* value;
};

std::filesystem::path find_project_root(const char* argv0) {
    std::filesystem::path directory;
    try {
        directory = std::filesystem::absolute(std::filesystem::path(argv0)).parent_path();
    } catch (...) {
        directory = std::filesystem::current_path();
    }

    for (;;) {
        if (std::filesystem::exists(directory / "CMakeLists.txt"))
            return directory;

        const auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }

    return std::filesystem::current_path();
}

// AP-GUI-002: `preloaded`/`pipeline_path` let the import-choice and
// region-editor flows hand over an already-decoded (and possibly already
// scoped) image without re-reading the file a second time. `path` is
// always the ORIGINAL file the user opened - it stays the source_id and
// the on-screen label regardless of scoping. `pipeline_path` is what
// ExtractionPipeline::run() actually loads; it defaults to `path` (no
// scoping) unless the caller supplies the on-disk scoped_source.png.
void load_image(
    GuiState& state,
    const std::string& path,
    cv::Mat preloaded = {},
    const std::string& pipeline_path = {}) {
    gui_log("LOAD: enter");
    if (path.empty()) {
        gui_log("LOAD: empty path");
        return;
    }
    gui_log("LOAD: path=" + path);

    cv::Mat image;
    if (!preloaded.empty()) {
        image = std::move(preloaded);
    } else {
        gui_log("LOAD: calling ImageLoader::load");
        image = ImageLoader::load(path);
    }
    gui_log("LOAD: image rows=" + std::to_string(image.rows) +
            " cols=" + std::to_string(image.cols) +
            " channels=" + std::to_string(image.channels()));
    state.source = std::move(image);
    gui_log("LOAD: state.source assigned");
    state.normalized.release();
    state.detection = {};
    state.normalized_conductors.clear();
    state.topology = {};
    state.gap_interpretation = {};
    state.endpoints = {};
    state.model = {};
    state.image_path = path;
    state.pipeline_image_path = pipeline_path.empty() ? path : pipeline_path;
    state.extracted = false;
    state.render_trace_pending = true;
    gui_log("LOAD: state updated; exit");
}

// AP-GUI-003: single entry point for "the user just picked/handed us a
// file to open" - used by both the OPEN toolbar button/O shortcut and a
// file passed on the command line (which is how the Windows Explorer
// "Extract with DS-Extractor" context-menu entry launches the GUI). Both
// paths get the same import-choice screen; there is no silent/implicit
// import anywhere in the GUI.
void begin_open_image(GuiState& state, const std::string& path) {
    if (path.empty()) return;
    state.pending_source = ImageLoader::load(path);
    state.pending_path = path;
    state.editor_regions.clear();
    state.editor_exclude_mode = false;
    state.mode = GuiMode::ImportChoice;
}

// AP-GUI-002: builds an ExtractionScope from the editor's drawn regions and
// runs it through the exact same SourceScoper the CLI's --scope uses
// (src/app/main.cpp), writing the same artifacts/scoping/scoped_source.png
// + scope_provenance.json pair so GUI and CLI scoped runs are
// indistinguishable on disk. Then loads the scoped image as the current
// diagram: state.source becomes the scoped pixels (so the calibration
// preview reflects masking too), while image_path/source_id stay the
// original file.
void apply_mask_and_load(GuiState& state) {
    if (state.pending_source.empty() || state.pending_path.empty()) return;

    ExtractionScope scope;
    scope.schema_version = 1;
    scope.source_path = state.pending_path;
    scope.source_page = 0;
    for (const auto& region : state.editor_regions) {
        const BoundingBox box {region.rect.x, region.rect.y,
                                region.rect.width, region.rect.height};
        if (region.exclude) scope.exclusion_regions.push_back(box);
        else scope.include_regions.push_back(box);
    }

    SourceScoper scoper;
    const ScopedSourceArtifacts scoped =
        scoper.apply(state.pending_source, scope, state.pending_path);

    const std::filesystem::path scoping_dir =
        state.artifact_root / "artifacts" / "scoping";
    std::filesystem::create_directories(scoping_dir);

    const std::filesystem::path scoped_image_path =
        scoping_dir / "scoped_source.png";
    if (!cv::imwrite(scoped_image_path.string(), scoped.scoped_image)) {
        throw std::runtime_error(
            "Unable to write scoped source image: " +
            scoped_image_path.string());
    }

    std::ofstream provenance_out(scoping_dir / "scope_provenance.json");
    if (!provenance_out) {
        throw std::runtime_error("Unable to create scope provenance JSON");
    }
    provenance_out << SourceScoper::serialize_provenance(scoped.provenance);

    load_image(
        state, state.pending_path, scoped.scoped_image,
        scoped_image_path.string());

    state.pending_source.release();
    state.pending_path.clear();
    state.editor_regions.clear();
    state.mode = GuiMode::Normal;
}

void extract(GuiState& state) {
    if (state.image_path.empty()) return;

    state.normalized = ImageNormalizer::normalize(state.source);

    ShapeDetector shape_detector;
    const ShapeDetectionArtifacts shapes =
        shape_detector.detect(state.normalized, state.image_path, 0);

    MorphologyWireDetector detector(state.config);
    state.detection = detector.detect(
        state.normalized, state.image_path, 0, shapes.exclusion_mask);
    state.detection.shapes = shapes;

    ConductorNormalizer normalizer;
    state.normalized_conductors = normalizer.normalize(
        state.detection.conductor_segments);

    TopologyReconstructor topology;
    state.topology = topology.reconstruct(
        state.normalized_conductors, state.image_path, 0);

    GapInterpreter gap_interpreter;
    state.gap_interpretation = gap_interpreter.interpret(
        state.topology.nodes, state.topology.edges,
        state.normalized, state.image_path, 0);

    state.topology.edges.insert(
        state.topology.edges.end(),
        state.gap_interpretation.inferred_edges.begin(),
        state.gap_interpretation.inferred_edges.end());

    EndpointReconstructor endpoint_reconstructor;
    state.endpoints = endpoint_reconstructor.reconstruct(
        state.topology.nodes, state.topology.edges,
        state.normalized, state.image_path, 0);

    // The GUI keeps its diagnostic calibration view, but artifact generation
    // always uses the complete ExtractionPipeline. This prevents GUI output
    // from becoming a second, partial extraction format.
    ExtractionConfig artifact_config;
    artifact_config.morphology = state.config;
    ExtractionPipeline artifact_pipeline(artifact_config);
    const std::string pipeline_path = state.pipeline_image_path.empty()
        ? state.image_path
        : state.pipeline_image_path;
    const WireModel artifact_model =
        artifact_pipeline.run(pipeline_path, state.image_path);

    state.model = artifact_model;

    ExtractionArtifactWriter::write(
        artifact_model,
        state.normalized,
        state.image_path,
        state.artifact_root);

    gui_log("EXTRACT: canonical artifacts written to " +
            (state.artifact_root / "output" / "wires.svg").string());

    state.extracted = true;
    publish_review_artifacts(state);
}

#ifdef _WIN32
void publish_review_artifacts(const GuiState& state) {
    const std::filesystem::path script =
        state.artifact_root / "tools" / "dx-publish-review.ps1";

    if (!std::filesystem::exists(script)) {
        gui_log("REVIEW: publisher script not found: " + script.string());
        return;
    }

    const std::wstring arguments =
        L"-NoProfile -ExecutionPolicy Bypass -File \"" +
        script.wstring() + L"\"";

    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        L"powershell.exe",
        arguments.c_str(),
        state.artifact_root.wstring().c_str(),
        SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        gui_log("REVIEW: unable to launch publisher.");
        return;
    }

    gui_log("REVIEW: publisher launched.");
}
#else
void publish_review_artifacts(const GuiState&) {
    gui_log("REVIEW: automatic Git publishing is currently supported on Windows.");
}
#endif

void reset_view(GuiState& state) {
    state.zoom = 1.0;
    state.pan = {0.0, 0.0};
}

void reset_parameters(GuiState& state) {
    state.config = MorphologyConfig {};
    state.extracted = false;
    state.endpoints = {};
    reset_view(state);
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

    const double max_scale = 0.50;
    const double min_scale = 0.28;
    const int horizontal_padding = 12;
    double scale = max_scale;

    int baseline = 0;
    cv::Size text_size = cv::getTextSize(
        label, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &baseline);

    while (text_size.width > rect.width - horizontal_padding &&
           scale > min_scale) {
        scale -= 0.02;
        text_size = cv::getTextSize(
            label, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &baseline);
    }

    const int text_x = rect.x + (rect.width - text_size.width) / 2;
    const int text_y =
        rect.y + (rect.height + text_size.height) / 2;

    cv::putText(panel, label, {text_x, text_y},
                cv::FONT_HERSHEY_SIMPLEX, scale,
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
    case ViewMode::Shapes:
        return state.detection.shapes.exclusion_mask;
    default:
        return state.source;
    }
}

void overlay_conductors(
    cv::Mat& display,
    const GuiState& state,
    double scale) {

    for (const auto& segment : state.normalized_conductors) {
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

void overlay_endpoints(
    cv::Mat& display,
    const GuiState& state,
    double scale) {

    for (const auto& endpoint : state.endpoints.candidates) {
        cv::Point p(
            static_cast<int>(endpoint.position.x * scale),
            static_cast<int>(endpoint.position.y * scale));

        cv::circle(display, p, 5, cv::Scalar(0, 215, 255), 1, cv::LINE_AA);
        cv::circle(display, p, 2, cv::Scalar(0, 215, 255), cv::FILLED, cv::LINE_AA);
    }
}


std::string normalized_color_name(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](unsigned char ch) {
                                   return std::isspace(ch) || ch == '-' || ch == '_';
                               }),
                value.end());
    return value;
}

cv::Scalar wire_color_scalar(const std::string& value) {
    const std::string color = normalized_color_name(value);
    if (color == "black") return cv::Scalar(20, 20, 20);
    if (color == "white") return cv::Scalar(245, 245, 245);
    if (color == "red") return cv::Scalar(40, 40, 220);
    if (color == "orange") return cv::Scalar(20, 150, 240);
    if (color == "yellow") return cv::Scalar(40, 220, 240);
    if (color == "green") return cv::Scalar(50, 180, 60);
    if (color == "blue") return cv::Scalar(220, 80, 40);
    if (color == "purple" || color == "violet") return cv::Scalar(180, 70, 170);
    if (color == "pink") return cv::Scalar(190, 100, 220);
    if (color == "brown") return cv::Scalar(50, 80, 130);
    if (color == "gray" || color == "grey") return cv::Scalar(130, 130, 130);
    return cv::Scalar(0, 0, 220);
}

const EndpointCandidate* find_endpoint(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(
        model.endpoint_candidates.begin(),
        model.endpoint_candidates.end(),
        [&](const EndpointCandidate& endpoint) {
            return endpoint.id == id;
        });
    return it == model.endpoint_candidates.end() ? nullptr : &*it;
}

const ComponentCandidate* find_component(
    const WireModel& model, const std::string& id) {
    const auto it = std::find_if(
        model.component_candidates.begin(),
        model.component_candidates.end(),
        [&](const ComponentCandidate& component) {
            return component.id == id;
        });
    return it == model.component_candidates.end() ? nullptr : &*it;
}

void overlay_wire_colors(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    std::unordered_map<std::string, std::string> segment_colors;
    std::unordered_set<std::string> ambiguous_segments;

    for (const auto& wire : state.model.wires) {
        const EndpointCandidate* start =
            find_endpoint(state.model, wire.start_endpoint);
        const EndpointCandidate* end =
            find_endpoint(state.model, wire.end_endpoint);
        const std::string start_color =
            start ? normalized_color_name(start->wire_color) : std::string {};
        const std::string end_color =
            end ? normalized_color_name(end->wire_color) : std::string {};

        std::string color;
        if (!start_color.empty() && !end_color.empty() &&
            start_color == end_color) {
            color = start_color;
        } else if (!start_color.empty() && end_color.empty()) {
            color = start_color;
        } else if (start_color.empty() && !end_color.empty()) {
            color = end_color;
        }

        if (color.empty())
            continue;

        for (const auto& segment_id : wire.conductor_segments) {
            const auto existing = segment_colors.find(segment_id);
            if (existing != segment_colors.end() && existing->second != color) {
                ambiguous_segments.insert(segment_id);
                segment_colors.erase(existing);
            } else if (!ambiguous_segments.contains(segment_id)) {
                segment_colors[segment_id] = color;
            }
        }
    }

    for (const auto& segment : state.model.conductor_segments) {
        const auto it = segment_colors.find(segment.id);
        if (it == segment_colors.end())
            continue;

        cv::Point a(
            static_cast<int>(segment.geometry.a.x * scale),
            static_cast<int>(segment.geometry.a.y * scale));
        cv::Point b(
            static_cast<int>(segment.geometry.b.x * scale),
            static_cast<int>(segment.geometry.b.y * scale));

        cv::line(display, a, b, wire_color_scalar(it->second), 3, cv::LINE_AA);
    }
}

void overlay_symbols(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& recognition : state.model.component_symbol_recognitions) {
        const ComponentCandidate* component =
            find_component(state.model, recognition.component_id);
        if (!component)
            continue;

        cv::Rect r(
            static_cast<int>(component->bounds.x * scale),
            static_cast<int>(component->bounds.y * scale),
            (std::max)(1, static_cast<int>(component->bounds.width * scale)),
            (std::max)(1, static_cast<int>(component->bounds.height * scale)));

        cv::Scalar color(220, 180, 40);
        if (recognition.symbol_kind == ComponentSymbolKind::ChassisGround)
            color = cv::Scalar(80, 220, 80);
        else if (recognition.symbol_kind == ComponentSymbolKind::Enclosure)
            color = cv::Scalar(220, 120, 40);

        cv::rectangle(display, r, color, 2, cv::LINE_AA);
    }
}

void overlay_terminals(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& endpoint : state.model.endpoint_candidates) {
        cv::Point p(
            static_cast<int>(endpoint.position.x * scale),
            static_cast<int>(endpoint.position.y * scale));

        if (endpoint.terminal_role == TerminalRole::ConnectorTerminal) {
            cv::polylines(display,
                          std::vector<std::vector<cv::Point>>{
                              {{p.x, p.y - 6}, {p.x + 6, p.y}, {p.x, p.y + 6},
                               {p.x - 6, p.y}}},
                          true, cv::Scalar(255, 180, 40), 2, cv::LINE_AA);
        } else if (endpoint.terminal_role == TerminalRole::ComponentTerminal) {
            cv::circle(display, p, 5, cv::Scalar(40, 220, 220), 2, cv::LINE_AA);
        }
    }
}

void overlay_connectors(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& connector : state.model.connector_candidates) {
        cv::Rect r(
            static_cast<int>(connector.bounds.x * scale),
            static_cast<int>(connector.bounds.y * scale),
            (std::max)(1, static_cast<int>(connector.bounds.width * scale)),
            (std::max)(1, static_cast<int>(connector.bounds.height * scale)));
        cv::rectangle(display, r, cv::Scalar(255, 150, 40), 2, cv::LINE_AA);
    }

    for (const auto& terminal : state.model.connector_terminals) {
        cv::Point p(
            static_cast<int>(terminal.position.x * scale),
            static_cast<int>(terminal.position.y * scale));
        cv::circle(display, p, 3, cv::Scalar(255, 220, 60), cv::FILLED, cv::LINE_AA);
    }
}

void overlay_splices(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& node : state.model.nodes) {
        if (node.type != TopologyNodeType::Splice &&
            node.type != TopologyNodeType::Junction)
            continue;

        cv::Point p(
            static_cast<int>(node.position.x * scale),
            static_cast<int>(node.position.y * scale));
        cv::circle(display, p, 5, cv::Scalar(0, 220, 120), 2, cv::LINE_AA);
    }
}

void overlay_grounds(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& endpoint : state.model.endpoint_candidates) {
        if (endpoint.kind != EndpointKind::Ground)
            continue;

        cv::Point p(
            static_cast<int>(endpoint.position.x * scale),
            static_cast<int>(endpoint.position.y * scale));
        cv::line(display, {p.x, p.y}, {p.x, p.y + 9},
                  cv::Scalar(80, 220, 80), 2, cv::LINE_AA);
        cv::line(display, {p.x - 7, p.y + 9}, {p.x + 7, p.y + 9},
                  cv::Scalar(80, 220, 80), 2, cv::LINE_AA);
        cv::line(display, {p.x - 4, p.y + 13}, {p.x + 4, p.y + 13},
                  cv::Scalar(80, 220, 80), 2, cv::LINE_AA);
    }
}

void overlay_labels(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& endpoint : state.model.endpoint_candidates) {
        std::string label = endpoint.terminal_name;
        if (label.empty())
            label = endpoint.function_label;
        if (label.empty())
            label = endpoint.wire_color;
        if (label.empty())
            continue;

        cv::Point p(
            static_cast<int>(endpoint.position.x * scale) + 6,
            static_cast<int>(endpoint.position.y * scale) - 6);
        cv::putText(display, label, p, cv::FONT_HERSHEY_SIMPLEX, 0.38,
                    cv::Scalar(245, 245, 245), 1, cv::LINE_AA);
    }

    for (const auto& component : state.model.component_candidates) {
        if (component.semantic_labels.empty())
            continue;
        cv::Point p(
            static_cast<int>(component.bounds.x * scale),
            static_cast<int>(component.bounds.y * scale) - 4);
        cv::putText(display, component.semantic_labels.front(), p,
                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(245, 245, 245), 1, cv::LINE_AA);
    }
}

void overlay_wire_direction(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& wire : state.model.wires) {
        const EndpointCandidate* start =
            find_endpoint(state.model, wire.start_endpoint);
        const EndpointCandidate* end =
            find_endpoint(state.model, wire.end_endpoint);
        if (!start || !end)
            continue;

        const cv::Point a(
            static_cast<int>(start->position.x * scale),
            static_cast<int>(start->position.y * scale));
        const cv::Point b(
            static_cast<int>(end->position.x * scale),
            static_cast<int>(end->position.y * scale));
        const cv::Point mid((a.x + b.x) / 2, (a.y + b.y) / 2);
        cv::arrowedLine(display, mid, b, cv::Scalar(80, 220, 255),
                        2, cv::LINE_AA, 0, 0.25);
    }
}

void overlay_component_bounds(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& component : state.model.component_candidates) {
        cv::Rect r(
            static_cast<int>(component.bounds.x * scale),
            static_cast<int>(component.bounds.y * scale),
            (std::max)(1, static_cast<int>(component.bounds.width * scale)),
            (std::max)(1, static_cast<int>(component.bounds.height * scale)));
        cv::rectangle(display, r, cv::Scalar(180, 100, 220), 1, cv::LINE_AA);
    }
}

void overlay_endpoint_debug(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& endpoint : state.model.endpoint_candidates) {
        cv::Point p(
            static_cast<int>(endpoint.position.x * scale),
            static_cast<int>(endpoint.position.y * scale));
        const cv::Scalar color =
            endpoint.kind == EndpointKind::Ground
                ? cv::Scalar(80, 220, 80)
                : endpoint.terminal_role == TerminalRole::ConnectorTerminal
                    ? cv::Scalar(255, 180, 40)
                    : endpoint.terminal_role == TerminalRole::ComponentTerminal
                        ? cv::Scalar(40, 220, 220)
                        : cv::Scalar(0, 215, 255);
        cv::circle(display, p, 7, color, 1, cv::LINE_AA);
    }
}

void overlay_recognition_evidence(
    cv::Mat& display,
    const GuiState& state,
    double scale) {
    for (const auto& recognition : state.model.component_symbol_recognitions) {
        const ComponentCandidate* component =
            find_component(state.model, recognition.component_id);
        if (!component)
            continue;

        cv::Rect r(
            static_cast<int>(component->bounds.x * scale),
            static_cast<int>(component->bounds.y * scale),
            (std::max)(1, static_cast<int>(component->bounds.width * scale)),
            (std::max)(1, static_cast<int>(component->bounds.height * scale)));
        cv::rectangle(display, r, cv::Scalar(255, 80, 180), 1, cv::LINE_AA);
    }
}

void overlay_shapes(
    cv::Mat& display,
    const GuiState& state,
    double scale) {

    for (const auto& region : state.detection.shapes.regions) {
        cv::Rect r(
            static_cast<int>(region.bounds.x * scale),
            static_cast<int>(region.bounds.y * scale),
            static_cast<int>(region.bounds.width * scale),
            static_cast<int>(region.bounds.height * scale));

        const cv::Scalar color =
            region.kind == ShapeKind::Rectangle
                ? cv::Scalar(255, 120, 0)
                : region.kind == ShapeKind::Circle
                    ? cv::Scalar(180, 0, 255)
                    : cv::Scalar(0, 220, 180);

        cv::rectangle(display, r, color, 2, cv::LINE_AA);
    }
}

// Scales `image` to fit inside `area`, returns the scaled Mat (BGR) plus
// where it lands within `area` (in canvas coordinates) via `out_rect`, and
// the scale factor via `out_scale`. Shared by both the import-choice and
// region-editor renderers so their coordinate math matches exactly.
cv::Mat fit_image_in_area(
    const cv::Mat& image, cv::Rect area, cv::Rect& out_rect, double& out_scale) {
    cv::Mat color_view;
    if (image.channels() == 1) cv::cvtColor(image, color_view, cv::COLOR_GRAY2BGR);
    else if (image.channels() == 4) cv::cvtColor(image, color_view, cv::COLOR_BGRA2BGR);
    else color_view = image.clone();

    const double scale = (std::min)(
        static_cast<double>(area.width - 24) / color_view.cols,
        static_cast<double>(area.height - 24) / color_view.rows);
    out_scale = scale;

    const cv::Size display_size(
        (std::max)(1, static_cast<int>(color_view.cols * scale)),
        (std::max)(1, static_cast<int>(color_view.rows * scale)));
    cv::resize(color_view, color_view, display_size, 0, 0, cv::INTER_AREA);

    out_rect = cv::Rect(
        area.x + (area.width - display_size.width) / 2,
        area.y + (area.height - display_size.height) / 2,
        display_size.width, display_size.height);
    return color_view;
}

void render_import_choice(cv::Mat& canvas, GuiState& state) {
    cv::rectangle(canvas, {0, 0}, {canvas.cols, kToolbarHeight},
                  cv::Scalar(35, 35, 35), cv::FILLED);
    cv::putText(canvas, "Opened: " + state.pending_path,
                {14, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(230, 230, 230), 1, cv::LINE_AA);

    const int strip_height = 96;
    const cv::Rect image_area(
        0, kToolbarHeight, canvas.cols,
        canvas.rows - kToolbarHeight - strip_height);

    cv::Rect placed;
    double scale = 1.0;
    const cv::Mat view = fit_image_in_area(
        state.pending_source, image_area, placed, scale);
    view.copyTo(canvas(placed));

    const int strip_y = canvas.rows - strip_height;
    cv::rectangle(canvas, {0, strip_y}, {canvas.cols, canvas.rows},
                  cv::Scalar(30, 30, 30), cv::FILLED);
    cv::putText(canvas, "How should this diagram be imported?",
                {20, strip_y + 26}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                cv::Scalar(220, 220, 220), 1, cv::LINE_AA);

    draw_button(canvas, {20, strip_y + 40, 230, 40}, "IMPORT AS-IS");
    draw_button(canvas, {264, strip_y + 40, 330, 40},
                "MASK REGIONS BEFORE IMPORT");
    draw_button(canvas, {608, strip_y + 40, 120, 40}, "CANCEL");
}

void render_region_editor(cv::Mat& canvas, GuiState& state) {
    cv::rectangle(canvas, {0, 0}, {canvas.cols, kToolbarHeight},
                  cv::Scalar(35, 35, 35), cv::FILLED);

    // Kept inside the GUI's minimum clamped canvas width (1100px, see
    // render()'s canvas_size clamp) so every button stays reachable even
    // when the window is shrunk below its default 1400px, rather than
    // assuming the default size the way the button positions below do.
    draw_button(canvas, {10, 5, 120, 38}, "INCLUDE MODE",
                !state.editor_exclude_mode);
    draw_button(canvas, {138, 5, 120, 38}, "EXCLUDE MODE",
                state.editor_exclude_mode);
    draw_button(canvas, {266, 5, 80, 38}, "UNDO");
    draw_button(canvas, {354, 5, 80, 38}, "CLEAR");
    draw_button(canvas, {700, 5, 160, 38}, "IMPORT WITH MASK");
    draw_button(canvas, {868, 5, 80, 38}, "BACK");
    draw_button(canvas, {956, 5, 90, 38}, "CANCEL");

    std::size_t include_count = 0, exclude_count = 0;
    for (const auto& r : state.editor_regions)
        (r.exclude ? exclude_count : include_count)++;
    cv::putText(canvas,
                "Include: " + std::to_string(include_count) +
                    "   Exclude: " + std::to_string(exclude_count),
                {450, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(210, 210, 210), 1, cv::LINE_AA);

    const cv::Rect image_area(
        0, kToolbarHeight, canvas.cols, canvas.rows - kToolbarHeight - 30);
    cv::Rect placed;
    double scale = 1.0;
    const cv::Mat view = fit_image_in_area(
        state.pending_source, image_area, placed, scale);
    cv::Mat view_copy = view.clone();

    state.editor_image_screen_rect = placed;
    state.editor_scale = scale;

    auto to_screen = [&](const cv::Rect& source_rect) {
        return cv::Rect(
            placed.x + static_cast<int>(source_rect.x * scale),
            placed.y + static_cast<int>(source_rect.y * scale),
            static_cast<int>(source_rect.width * scale),
            static_cast<int>(source_rect.height * scale));
    };

    for (const auto& region : state.editor_regions) {
        const cv::Rect r = to_screen(region.rect) &
            cv::Rect(0, 0, view_copy.cols, view_copy.rows);
        if (r.width <= 0 || r.height <= 0) continue;
        const cv::Scalar color =
            region.exclude ? cv::Scalar(40, 40, 220) : cv::Scalar(40, 200, 60);
        cv::Mat overlay = view_copy(r).clone();
        cv::rectangle(overlay, {0, 0, r.width, r.height}, color, cv::FILLED);
        cv::addWeighted(overlay, 0.28, view_copy(r), 0.72, 0, view_copy(r));
        cv::rectangle(view_copy, r, color, 2, cv::LINE_AA);
    }

    if (state.editor_dragging) {
        const cv::Rect drag_screen(
            (std::min)(state.editor_drag_start_screen.x, state.editor_drag_now_screen.x),
            (std::min)(state.editor_drag_start_screen.y, state.editor_drag_now_screen.y),
            std::abs(state.editor_drag_now_screen.x - state.editor_drag_start_screen.x),
            std::abs(state.editor_drag_now_screen.y - state.editor_drag_start_screen.y));
        const cv::Rect local = cv::Rect(
            drag_screen.x - placed.x, drag_screen.y - placed.y,
            drag_screen.width, drag_screen.height) &
            cv::Rect(0, 0, view_copy.cols, view_copy.rows);
        if (local.width > 0 && local.height > 0) {
            const cv::Scalar color = state.editor_exclude_mode
                ? cv::Scalar(40, 40, 220) : cv::Scalar(40, 200, 60);
            cv::rectangle(view_copy, local, color, 2, cv::LINE_AA);
        }
    }

    view_copy.copyTo(canvas(placed));

    cv::putText(canvas,
                "Drag to draw a region. Regions default to INCLUDE (only "
                "drawn areas are eligible); switch to EXCLUDE MODE to carve "
                "areas back out. No regions drawn = whole image eligible.",
                {14, canvas.rows - 10}, cv::FONT_HERSHEY_SIMPLEX, 0.4,
                cv::Scalar(170, 170, 170), 1, cv::LINE_AA);
}

cv::Mat render(GuiState& state, cv::Size canvas_size) {
    const bool trace = state.render_trace_pending;
    if (trace) gui_log("RENDER 1: enter");

    canvas_size.width = (std::max)(canvas_size.width, 1100);
    canvas_size.height = (std::max)(canvas_size.height, 940);

    cv::Mat canvas(
        canvas_size, CV_8UC3, cv::Scalar(238, 238, 238));

    // AP-GUI-002: source scoping is a first-class step of opening an image.
    // These modes take over the whole canvas until the user commits to an
    // import decision; the ordinary calibration workbench below is
    // untouched otherwise.
    if (state.mode == GuiMode::ImportChoice) {
        render_import_choice(canvas, state);
        return canvas;
    }
    if (state.mode == GuiMode::RegionEditor) {
        render_region_editor(canvas, state);
        return canvas;
    }

    cv::rectangle(
        canvas, {0, 0}, {canvas.cols, kToolbarHeight},
        cv::Scalar(35, 35, 35), cv::FILLED);

    auto toolbar_button = [&](int x, int width, const std::string& label) {
        draw_button(
            canvas, {x, 5, width, 38}, label, false);
    };

    // Keep generous spacing and enough width for every label.
    toolbar_button(10, 82, "OPEN");
    toolbar_button(100, 92, "EXTRACT");
    toolbar_button(200, 82, "RESET");
    toolbar_button(290, 116, "CONDUCTORS");
    toolbar_button(414, 106, "TOPOLOGY");
    toolbar_button(528, 126, "BUILD / TEST");
    toolbar_button(662, 108, "RELEASE");
    toolbar_button(778, 64, "ZOOM +");
    toolbar_button(850, 64, "ZOOM -");
    toolbar_button(922, 82, "FIT VIEW");
    cv::putText(
        canvas, "Zoom: " + std::to_string(static_cast<int>(state.zoom * 100.0 + 0.5)) + "%",
        {1020, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.48,
        cv::Scalar(220, 220, 220), 1, cv::LINE_AA);

    const int image_width = canvas.cols - kPanelWidth;
    const int image_height =
        canvas.rows - kToolbarHeight - kStatusHeight;

    cv::Mat view = make_view(state);

    if (!view.empty()) {
        cv::Mat color_view;

        if (view.channels() == 1) {
            cv::cvtColor(view, color_view, cv::COLOR_GRAY2BGR);
        } else if (view.channels() == 3) {
            color_view = view.clone();
        } else if (view.channels() == 4) {
            cv::cvtColor(view, color_view, cv::COLOR_BGRA2BGR);
        } else {
            throw std::runtime_error("Unsupported display channel count: " +
                                     std::to_string(view.channels()));
        }

        const double fit_scale = (std::min)(
            static_cast<double>(image_width - 24) / color_view.cols,
            static_cast<double>(image_height - 24) / color_view.rows);
        const double scale = fit_scale * state.zoom;

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
        } else if (state.view == ViewMode::Endpoints) {
            overlay_conductors(color_view, state, scale);
            overlay_endpoints(color_view, state, scale);
        } else if (state.view == ViewMode::Shapes) {
            color_view = state.source.empty()
                ? color_view
                : (state.source.channels() == 1
                    ? [&]() { cv::Mat t; cv::cvtColor(state.source, t, cv::COLOR_GRAY2BGR); return t; }()
                    : state.source.channels() == 4
                        ? [&]() { cv::Mat t; cv::cvtColor(state.source, t, cv::COLOR_BGRA2BGR); return t; }()
                        : state.source.clone());
            cv::resize(color_view, color_view, display_size, 0, 0, cv::INTER_AREA);
            overlay_shapes(color_view, state, scale);
        }

        if (!state.layers.base_diagram &&
            (state.view == ViewMode::Source ||
             state.view == ViewMode::Conductors ||
             state.view == ViewMode::Topology ||
             state.view == ViewMode::Endpoints)) {
            color_view = cv::Mat(display_size, CV_8UC3,
                                 cv::Scalar(238, 238, 238));
        }

        if (state.view == ViewMode::Source ||
            state.view == ViewMode::Conductors ||
            state.view == ViewMode::Topology ||
            state.view == ViewMode::Endpoints) {
            if (state.layers.wires)
                overlay_conductors(color_view, state, scale);
            if (state.layers.wire_colors)
                overlay_wire_colors(color_view, state, scale);
            if (state.layers.symbols)
                overlay_symbols(color_view, state, scale);
            if (state.layers.terminals)
                overlay_terminals(color_view, state, scale);
            if (state.layers.connectors)
                overlay_connectors(color_view, state, scale);
            if (state.layers.splices)
                overlay_splices(color_view, state, scale);
            if (state.layers.grounds)
                overlay_grounds(color_view, state, scale);
            if (state.layers.labels)
                overlay_labels(color_view, state, scale);
            if (state.layers.wire_direction)
                overlay_wire_direction(color_view, state, scale);
            if (state.layers.topology)
                overlay_topology(color_view, state, scale);
            if (state.layers.component_bounds)
                overlay_component_bounds(color_view, state, scale);
            if (state.layers.endpoint_debug)
                overlay_endpoint_debug(color_view, state, scale);
            if (state.layers.recognition_evidence)
                overlay_recognition_evidence(color_view, state, scale);
        }

        const int centered_x = (image_width - color_view.cols) / 2;
        const int centered_y =
            kToolbarHeight +
            (image_height - color_view.rows) / 2;
        const int ox = static_cast<int>(centered_x + state.pan.x);
        const int oy = static_cast<int>(centered_y + state.pan.y);

        const cv::Rect viewport(0, kToolbarHeight, image_width, image_height);
        const cv::Rect image_rect(ox, oy, color_view.cols, color_view.rows);
        const cv::Rect visible = image_rect & viewport;
        if (visible.width > 0 && visible.height > 0) {
            const cv::Rect source_rect(
                visible.x - image_rect.x,
                visible.y - image_rect.y,
                visible.width,
                visible.height);
            color_view(source_rect).copyTo(canvas(visible));
        }
    } else {
        cv::putText(
            canvas, "Open a wiring diagram to begin.",
            {35, 100}, cv::FONT_HERSHEY_SIMPLEX, 0.8,
            cv::Scalar(70, 70, 70), 1, cv::LINE_AA);
    }

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

    const int button_y = 555;

    cv::putText(panel, "VIEW", {18, 550},
                cv::FONT_HERSHEY_SIMPLEX, 0.46,
                cv::Scalar(170, 170, 170), 1, cv::LINE_AA);

    draw_button(panel, {18, button_y, 84, 28},
                "SOURCE", state.view == ViewMode::Source);
    draw_button(panel, {108, button_y, 84, 28},
                "BINARY", state.view == ViewMode::Binary);
    draw_button(panel, {198, button_y, 84, 28},
                "H-MASK", state.view == ViewMode::HorizontalMask);

    draw_button(panel, {18, button_y + 34, 84, 28},
                "V-MASK", state.view == ViewMode::VerticalMask);
    draw_button(panel, {108, button_y + 34, 84, 28},
                "WIRES", state.view == ViewMode::Conductors);
    draw_button(panel, {198, button_y + 34, 84, 28},
                "TOPOLOGY", state.view == ViewMode::Topology);

    draw_button(panel, {18, button_y + 68, 84, 28},
                "ENDPOINTS", state.view == ViewMode::Endpoints);
    draw_button(panel, {108, button_y + 68, 84, 28},
                "SHAPES", state.view == ViewMode::Shapes);

    cv::putText(panel, "ENGINEERING LAYERS", {18, 666},
                cv::FONT_HERSHEY_SIMPLEX, 0.52,
                cv::Scalar(245, 245, 245), 1, cv::LINE_AA);

    auto layer_button = [&](int x, int y, const std::string& label, bool active) {
        draw_button(panel, {x, y, 84, 24}, label, active);
    };

    layer_button(18, 676, "BASE", state.layers.base_diagram);
    layer_button(108, 676, "WIRES", state.layers.wires);
    layer_button(198, 676, "COLORS", state.layers.wire_colors);

    layer_button(18, 704, "SYMBOLS", state.layers.symbols);
    layer_button(108, 704, "TERMINALS", state.layers.terminals);
    layer_button(198, 704, "CONNECT", state.layers.connectors);

    layer_button(18, 732, "SPLICES", state.layers.splices);
    layer_button(108, 732, "GROUNDS", state.layers.grounds);
    layer_button(198, 732, "LABELS", state.layers.labels);

    layer_button(18, 760, "DIRECTION", state.layers.wire_direction);
    layer_button(108, 760, "TOPOLOGY", state.layers.topology);
    layer_button(198, 760, "BOUNDS", state.layers.component_bounds);

    layer_button(18, 788, "ENDPOINTS", state.layers.endpoint_debug);
    layer_button(108, 788, "RECOG.", state.layers.recognition_evidence);

    cv::rectangle(
        canvas,
        {0, canvas.rows - kStatusHeight},
        {canvas.cols, canvas.rows},
        cv::Scalar(35, 35, 35), cv::FILLED);

    // Persistent extraction telemetry. Keep this visually consistent with the
    // original calibration workbench: primary extraction counts on line one,
    // H/V counts and navigation/output information on line two.
    const std::size_t horizontal_count = std::count_if(
        state.normalized_conductors.begin(),
        state.normalized_conductors.end(),
        [](const ConductorSegment& s) {
            return s.geometry.a.y == s.geometry.b.y;
        });

    const std::size_t vertical_count = std::count_if(
        state.normalized_conductors.begin(),
        state.normalized_conductors.end(),
        [](const ConductorSegment& s) {
            return s.geometry.a.x == s.geometry.b.x;
        });

    const std::string output_directory =
        (state.artifact_root / "output").string();

    const std::string status =
        "Conductors: " + std::to_string(state.normalized_conductors.size()) +
        "    Nodes: " + std::to_string(state.topology.nodes.size()) +
        "    Edges: " + std::to_string(state.topology.edges.size()) +
        "    Gaps bridged: " + std::to_string(state.gap_interpretation.inferred_edges.size()) +
        "    Endpoints: " + std::to_string(state.endpoints.candidates.size()) +
        "    Shapes: " + std::to_string(state.detection.shapes.regions.size());

    const std::string detail =
        "H: " + std::to_string(horizontal_count) +
        "    V: " + std::to_string(vertical_count) +
        "    Zoom: " + std::to_string(static_cast<int>(state.zoom * 100.0 + 0.5)) +
        "%    |    Wheel: Zoom    MMB-drag: Pan    |    Output: ";

    cv::putText(
        canvas, status,
        {8, canvas.rows - 42},
        cv::FONT_HERSHEY_SIMPLEX, 0.43,
        cv::Scalar(235, 235, 235), 1, cv::LINE_AA);

    cv::putText(
        canvas, detail,
        {8, canvas.rows - 12},
        cv::FONT_HERSHEY_SIMPLEX, 0.34,
        cv::Scalar(170, 170, 170), 1, cv::LINE_AA);

    const int detail_baseline = canvas.rows - 12;
    const int output_prefix_width = cv::getTextSize(
        detail, cv::FONT_HERSHEY_SIMPLEX, 0.34, 1, nullptr).width;

    cv::putText(
        canvas, output_directory,
        {8 + output_prefix_width, detail_baseline},
        cv::FONT_HERSHEY_SIMPLEX, 0.34,
        cv::Scalar(145, 190, 245), 1, cv::LINE_AA);

    const int output_width = cv::getTextSize(
        output_directory, cv::FONT_HERSHEY_SIMPLEX, 0.34, 1, nullptr).width;

    state.output_link_rect = cv::Rect(
        8 + output_prefix_width,
        canvas.rows - kStatusHeight + 42,
        output_width,
        22);

    return canvas;
}

void handle_mouse_import_choice(GuiState& state, int event, int x, int y) {
    if (event != cv::EVENT_LBUTTONDOWN) return;

    const int strip_height = 96;
    const int strip_y = state.canvas_height - strip_height;
    if (y < strip_y + 40 || y >= strip_y + 80) return;

    if (x >= 20 && x < 250) {
        // Import as-is: no scope at all, matches the pre-AP-GUI-002 flow
        // exactly (pipeline_image_path defaults to image_path).
        load_image(state, state.pending_path, state.pending_source);
        state.pending_source.release();
        state.pending_path.clear();
        state.mode = GuiMode::Normal;
    } else if (x >= 264 && x < 594) {
        state.editor_regions.clear();
        state.editor_exclude_mode = false;
        state.editor_dragging = false;
        state.mode = GuiMode::RegionEditor;
    } else if (x >= 608 && x < 728) {
        state.pending_source.release();
        state.pending_path.clear();
        state.mode = GuiMode::Normal;
    }
}

void handle_mouse_region_editor(GuiState& state, int event, int x, int y) {
    if (event == cv::EVENT_LBUTTONDOWN && y < kToolbarHeight) {
        if (x >= 10 && x < 130) {
            state.editor_exclude_mode = false;
        } else if (x >= 138 && x < 258) {
            state.editor_exclude_mode = true;
        } else if (x >= 266 && x < 346) {
            if (!state.editor_regions.empty()) state.editor_regions.pop_back();
        } else if (x >= 354 && x < 434) {
            state.editor_regions.clear();
        } else if (x >= 700 && x < 860) {
            try {
                apply_mask_and_load(state);
            } catch (const std::exception& e) {
                std::cerr << "mask error: " << e.what() << '\n';
            }
        } else if (x >= 868 && x < 948) {
            state.editor_dragging = false;
            state.mode = GuiMode::ImportChoice;
        } else if (x >= 956 && x < 1046) {
            state.editor_dragging = false;
            state.editor_regions.clear();
            state.pending_source.release();
            state.pending_path.clear();
            state.mode = GuiMode::Normal;
        }
        return;
    }

    const cv::Rect& img = state.editor_image_screen_rect;
    const double scale = state.editor_scale > 0 ? state.editor_scale : 1.0;

    if (event == cv::EVENT_LBUTTONDOWN) {
        if (!img.contains(cv::Point(x, y))) return;
        state.editor_dragging = true;
        state.editor_drag_start_screen = {x, y};
        state.editor_drag_now_screen = {x, y};
        return;
    }

    if (event == cv::EVENT_MOUSEMOVE && state.editor_dragging) {
        state.editor_drag_now_screen = {
            (std::max)(img.x, (std::min)(img.x + img.width, x)),
            (std::max)(img.y, (std::min)(img.y + img.height, y))};
        return;
    }

    if (event == cv::EVENT_LBUTTONUP && state.editor_dragging) {
        state.editor_dragging = false;
        const cv::Point a = state.editor_drag_start_screen;
        const cv::Point b = state.editor_drag_now_screen;
        const int sx = (std::min)(a.x, b.x) - img.x;
        const int sy = (std::min)(a.y, b.y) - img.y;
        const int sw = std::abs(b.x - a.x);
        const int sh = std::abs(b.y - a.y);
        // A drag under 4 screen pixels is almost certainly an accidental
        // click, not an intended region - never silently record a
        // near-zero-area scope region.
        if (sw < 4 || sh < 4) return;

        const cv::Rect source_rect(
            static_cast<int>(sx / scale), static_cast<int>(sy / scale),
            static_cast<int>(sw / scale), static_cast<int>(sh / scale));
        if (source_rect.width <= 0 || source_rect.height <= 0) return;

        state.editor_regions.push_back(
            {source_rect, state.editor_exclude_mode});
    }
}

void handle_mouse(
    GuiState& state,
    int event,
    int x,
    int y) {

    if (state.mode == GuiMode::ImportChoice) {
        handle_mouse_import_choice(state, event, x, y);
        return;
    }
    if (state.mode == GuiMode::RegionEditor) {
        handle_mouse_region_editor(state, event, x, y);
        return;
    }

    if (event == cv::EVENT_MBUTTONDOWN) {
        state.panning = true;
        state.pan_start = {x, y};
        state.pan_origin = state.pan;
        return;
    }

    if (event == cv::EVENT_MBUTTONUP) {
        state.panning = false;
        return;
    }

    if (event == cv::EVENT_LBUTTONDOWN) {
        if (state.output_link_rect.contains(cv::Point(x, y))) {
#ifdef _WIN32
            const std::filesystem::path output_directory =
                state.artifact_root / "output";
            ShellExecuteW(
                nullptr,
                L"open",
                output_directory.wstring().c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL);
#else
            std::cerr << "Output directory: "
                      << (state.artifact_root / "output").string()
                      << '\\n';
#endif
            return;
        }

        if (y < kToolbarHeight) {
            if (x >= 10 && x < 92)
                state.request_open = true;
            else if (x >= 100 && x < 192)
                extract(state);
            else if (x >= 200 && x < 282)
                reset_parameters(state);
            else if (x >= 290 && x < 406)
                state.view = ViewMode::Conductors;
            else if (x >= 414 && x < 520)
                state.view = ViewMode::Topology;
            else if (x >= 528 && x < 654)
                state.request_build_test = true;
            else if (x >= 662 && x < 770)
                state.request_release = true;
            else if (x >= 778 && x < 842)
                state.zoom = (std::min)(8.0, state.zoom * 1.25);
            else if (x >= 850 && x < 914)
                state.zoom = (std::max)(0.25, state.zoom / 1.25);
            else if (x >= 922 && x < 1004)
                reset_view(state);
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
            y >= kToolbarHeight + 505 &&
            y < kToolbarHeight + 601) {
            const int bx = panel_x;
            const int local_y = y - (kToolbarHeight + 505);

            if (local_y < 28) {
                if (bx >= 18 && bx < 102) state.view = ViewMode::Source;
                else if (bx >= 108 && bx < 192) state.view = ViewMode::Binary;
                else if (bx >= 198 && bx < 282) state.view = ViewMode::HorizontalMask;
            } else if (local_y < 62) {
                if (bx >= 18 && bx < 102) state.view = ViewMode::VerticalMask;
                else if (bx >= 108 && bx < 192) state.view = ViewMode::Conductors;
                else if (bx >= 198 && bx < 282) state.view = ViewMode::Topology;
            } else if (local_y < 96) {
                if (bx >= 18 && bx < 102) state.view = ViewMode::Endpoints;
                else if (bx >= 108 && bx < 192) state.view = ViewMode::Shapes;
            }
        }

        if (x >= panel_left &&
            y >= kToolbarHeight + 555 &&
            y < kToolbarHeight + 651) {
            const int bx = panel_x;
            const int local_y = y - (kToolbarHeight + 555);

            if (local_y < 28) {
                if (bx >= 18 && bx < 102) state.view = ViewMode::Source;
                else if (bx >= 108 && bx < 192) state.view = ViewMode::Binary;
                else if (bx >= 198 && bx < 282) state.view = ViewMode::HorizontalMask;
            } else if (local_y < 62) {
                if (bx >= 18 && bx < 102) state.view = ViewMode::VerticalMask;
                else if (bx >= 108 && bx < 192) state.view = ViewMode::Conductors;
                else if (bx >= 198 && bx < 282) state.view = ViewMode::Topology;
            } else if (bx >= 18 && bx < 102) {
                state.view = ViewMode::Endpoints;
            } else if (bx >= 108 && bx < 192) {
                state.view = ViewMode::Shapes;
            }
        }

        if (x >= panel_left &&
            y >= kToolbarHeight + 676 &&
            y < kToolbarHeight + 820) {
            const int bx = panel_x;
            const int local_y = y - (kToolbarHeight + 676);
            const int row = local_y / 28;
            const int col =
                bx < 102 ? 0 :
                bx < 192 ? 1 :
                bx < 282 ? 2 : -1;

            if (col >= 0 && row >= 0 && row <= 4) {
                bool* target = nullptr;
                if (row == 0) target = col == 0 ? &state.layers.base_diagram :
                                      col == 1 ? &state.layers.wires :
                                                 &state.layers.wire_colors;
                else if (row == 1) target = col == 0 ? &state.layers.symbols :
                                           col == 1 ? &state.layers.terminals :
                                                      &state.layers.connectors;
                else if (row == 2) target = col == 0 ? &state.layers.splices :
                                           col == 1 ? &state.layers.grounds :
                                                      &state.layers.labels;
                else if (row == 3) target = col == 0 ? &state.layers.wire_direction :
                                           col == 1 ? &state.layers.topology :
                                                      &state.layers.component_bounds;
                else if (row == 4) target = col == 0 ? &state.layers.endpoint_debug :
                                           col == 1 ? &state.layers.recognition_evidence :
                                                      nullptr;

                if (target)
                    *target = !*target;
            }
        }
    }

    if (event == cv::EVENT_MOUSEMOVE && state.panning) {
        state.pan = state.pan_origin + cv::Point2d(
            x - state.pan_start.x,
            y - state.pan_start.y);
        return;
    }

    if (event == cv::EVENT_MOUSEMOVE && state.dragging) {
        const int panel_left = state.canvas_width - kPanelWidth;
        const int panel_x = x - panel_left;
        set_slider_from_mouse(state, state.active_slider, panel_x);
    }

    if (event == cv::EVENT_MOUSEWHEEL) {
        const int delta = cv::getMouseWheelDelta(0);
        if (delta != 0 && !state.source.empty()) {
            const double old_zoom = state.zoom;
            const double factor = delta > 0 ? 1.25 : 1.0 / 1.25;
            const double new_zoom = (std::max)(0.25, (std::min)(8.0, old_zoom * factor));

            if (new_zoom != old_zoom) {
                const int image_width = state.canvas_width - kPanelWidth;
                const int image_height = state.canvas_height - kToolbarHeight - kStatusHeight;
                const double fit_scale = (std::min)(
                    static_cast<double>(image_width - 24) / state.source.cols,
                    static_cast<double>(image_height - 24) / state.source.rows);

                const double old_scale = fit_scale * old_zoom;
                const double new_scale = fit_scale * new_zoom;
                const double old_w = state.source.cols * old_scale;
                const double old_h = state.source.rows * old_scale;
                const double new_w = state.source.cols * new_scale;
                const double new_h = state.source.rows * new_scale;

                const double old_center_x = (image_width - old_w) / 2.0 + state.pan.x;
                const double old_center_y = kToolbarHeight +
                    (image_height - old_h) / 2.0 + state.pan.y;

                const double source_x = (x - old_center_x) / old_scale;
                const double source_y = (y - old_center_y) / old_scale;

                const double new_center_x = (image_width - new_w) / 2.0;
                const double new_center_y = kToolbarHeight +
                    (image_height - new_h) / 2.0;

                state.zoom = new_zoom;
                state.pan.x = x - (new_center_x + source_x * new_scale);
                state.pan.y = y - (new_center_y + source_y * new_scale);
            }
        }
        return;
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
        std::cerr << "error: " << e.what() << '\\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        GuiState state;
        state.artifact_root = find_project_root(argc > 0 ? argv[0] : ".");
        gui_log("ARTIFACT ROOT: " + state.artifact_root.string());

        if (argc > 1)
            begin_open_image(state, argv[1]);

        cv::namedWindow(kWindow, cv::WINDOW_NORMAL);
        cv::resizeWindow(kWindow, 1400, 850);
        cv::setMouseCallback(kWindow, on_mouse, &state);

        while (true) {
            const cv::Size size =
                cv::getWindowImageRect(kWindow).size();

            state.canvas_width = (std::max)(size.width, 1100);
            state.canvas_height = (std::max)(size.height, 940);
            cv::imshow(kWindow, render(state, size));

            const int key = cv::waitKey(30);

            if (state.request_open) {
                state.request_open = false;
                try {
                    gui_log("OPEN: calling file dialog");
                    const std::string path = open_image_dialog();
                    gui_log("OPEN: dialog returned; path length=" + std::to_string(path.size()));
                    if (!path.empty()) {
                        // AP-GUI-002: opening an image never loads it
                        // straight into extraction anymore - the user
                        // always sees the import-as-is-vs-mask choice
                        // first, right after picking the file.
                        begin_open_image(state, path);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "open error: " << e.what() << '\\n';
                }
            }

            if (state.request_build_test) {
                state.request_build_test = false;
#ifdef _WIN32
                gui_log("BUILD / TEST: closing GUI and launching build/test pipeline");
                cv::destroyAllWindows();
                if (launch_release_pipeline(true))
                    return 0;
#else
                std::cerr << "Build/test automation is currently supported on Windows only.\n";
#endif
            }

            if (state.request_release) {
                state.request_release = false;
#ifdef _WIN32
                gui_log("RELEASE: closing GUI and launching release pipeline");
                cv::destroyAllWindows();
                if (launch_release_pipeline())
                    return 0;
#else
                std::cerr << "Release automation is currently supported on Windows only.\\n";
#endif
            }

            // AP-GUI-002: while the import-choice/region-editor overlay is
            // up, none of the ordinary calibration-workbench shortcuts
            // apply (extracting, view switching, zoom) - Esc/Q back out of
            // the overlay instead of quitting the whole application.
            if (state.mode != GuiMode::Normal) {
                if (key == 27 || key == 'q' || key == 'Q') {
                    state.editor_dragging = false;
                    state.editor_regions.clear();
                    state.pending_source.release();
                    state.pending_path.clear();
                    state.mode = GuiMode::Normal;
                }
                continue;
            }

            if (key == 27 || key == 'q' || key == 'Q')
                break;

            if (key == 'e' || key == 'E')
                extract(state);

            if (key == 'o' || key == 'O')
                state.request_open = true;

            if (key == '+' || key == '=')
                state.zoom = (std::min)(8.0, state.zoom * 1.25);
            if (key == '-' || key == '_')
                state.zoom = (std::max)(0.25, state.zoom / 1.25);
            if (key == '0')
                reset_view(state);

            if (key == '1') state.view = ViewMode::Source;
            if (key == '2') state.view = ViewMode::Binary;
            if (key == '3') state.view = ViewMode::HorizontalMask;
            if (key == '4') state.view = ViewMode::VerticalMask;
            if (key == '5') state.view = ViewMode::Conductors;
            if (key == '6') state.view = ViewMode::Topology;
            if (key == '7') state.view = ViewMode::Endpoints;
            if (key == '8') state.view = ViewMode::Shapes;
        }

        cv::destroyAllWindows();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\\n';
        return 1;
    }
}
