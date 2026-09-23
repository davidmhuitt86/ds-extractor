#include "eke_dx_wire/image/image_loader.hpp"

#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <roapi.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

#include <cstdint>
#include <vector>
#endif

namespace eke::dx::wire {

#ifdef _WIN32
namespace {

bool has_pdf_extension(const std::string& path) {
    if (path.size() < 4) {
        return false;
    }

    const std::string extension = path.substr(path.size() - 4);
    return extension == ".pdf" || extension == ".PDF" ||
           extension == ".Pdf" || extension == ".pDf" ||
           extension == ".pdF" || extension == ".PDf" ||
           extension == ".PdF" || extension == ".pDF";
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);

    if (required <= 0) {
        throw std::runtime_error("Unable to convert PDF path to UTF-16: " + value);
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), required) <= 0) {
        throw std::runtime_error("Unable to convert PDF path to UTF-16: " + value);
    }

    return result;
}

cv::Mat load_pdf_first_page(const std::string& path) {
    const HRESULT ro_result = RoInitialize(RO_INIT_SINGLETHREADED);
    const bool initialized = SUCCEEDED(ro_result);

    if (FAILED(ro_result) && ro_result != RPC_E_CHANGED_MODE) {
        throw std::runtime_error(
            "Unable to initialize Windows Runtime for PDF loading: HRESULT 0x" +
            std::to_string(static_cast<unsigned long>(ro_result)));
    }

    try {
        const std::wstring wide_path = utf8_to_wide(path);

        const auto file =
            winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(
                winrt::hstring(wide_path)).get();

        const auto document =
            winrt::Windows::Data::Pdf::PdfDocument::LoadFromFileAsync(file).get();

        if (document.PageCount() == 0) {
            throw std::runtime_error("PDF contains no pages: " + path);
        }

        const auto page = document.GetPage(0);
        const auto size = page.Size();

        winrt::Windows::Data::Pdf::PdfPageRenderOptions options;
        options.DestinationWidth(
            static_cast<std::uint32_t>(size.Width * 2.0));
        options.DestinationHeight(
            static_cast<std::uint32_t>(size.Height * 2.0));

        winrt::Windows::Storage::Streams::InMemoryRandomAccessStream stream;
        page.RenderToStreamAsync(stream, options).get();

        const auto stream_size = stream.Size();
        if (stream_size == 0) {
            throw std::runtime_error("PDF page rendered to an empty image: " + path);
        }

        if (stream_size > UINT32_MAX) {
            throw std::runtime_error("Rendered PDF page is too large: " + path);
        }

        winrt::Windows::Storage::Streams::DataReader reader{
            stream.GetInputStreamAt(0)};

        reader.LoadAsync(static_cast<std::uint32_t>(stream_size)).get();

        std::vector<std::uint8_t> encoded(
            static_cast<std::size_t>(stream_size));
        reader.ReadBytes(encoded);

        cv::Mat image = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
        if (image.empty()) {
            throw std::runtime_error(
                "Unable to decode rendered PDF page: " + path);
        }

        if (initialized) {
            RoUninitialize();
        }

        return image;
    } catch (...) {
        if (initialized) {
            RoUninitialize();
        }
        throw;
    }
}

} // namespace
#endif

cv::Mat ImageLoader::load(const std::string& path) {
#ifdef _WIN32
    if (has_pdf_extension(path)) {
        return load_pdf_first_page(path);
    }
#else
    if (path.size() >= 4) {
        const std::string extension = path.substr(path.size() - 4);
        if (extension == ".pdf" || extension == ".PDF") {
            throw std::runtime_error(
                "PDF input is currently supported on Windows only: " + path);
        }
    }
#endif

    cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);

    if (image.empty()) {
        throw std::runtime_error("Unable to load image: " + path);
    }

    return image;
}

} // namespace eke::dx::wire
