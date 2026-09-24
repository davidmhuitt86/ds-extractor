#include "eke_dx_wire/topology/anthropic_vision_text_recognition_provider.hpp"

#include <opencv2/core.hpp>

#include <cassert>
#include <cstdlib>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>

using namespace eke::dx::wire;

namespace {

// setenv/unsetenv are POSIX-only; MSVC does not provide them. The
// production code under test already treats an empty environment string
// the same as an absent one (see anthropic_vision_config_from_environment),
// so _putenv_s(name, "") is an equivalent stand-in for unsetenv on Windows.
void test_setenv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void test_unsetenv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class FakeHttpTransport final : public HttpTransport {
public:
    mutable std::deque<HttpResponse> queued_responses;
    mutable std::vector<std::string> requested_urls;
    mutable std::vector<std::string> request_bodies;

    HttpResponse post_json(
        const std::string& url,
        const std::vector<HttpHeader>&,
        const std::string& json_body) const override {
        requested_urls.push_back(url);
        request_bodies.push_back(json_body);
        if (queued_responses.empty()) {
            throw std::runtime_error("FakeHttpTransport: no response queued");
        }
        HttpResponse response = queued_responses.front();
        queued_responses.pop_front();
        return response;
    }
};

TextRegion region(const std::string& id, int x, int y) {
    TextRegion r;
    r.id = id;
    r.bounds = BoundingBox{x, y, 20, 10};
    return r;
}

HttpResponse ok_response(const std::string& observations_json_escaped) {
    HttpResponse response;
    response.status_code = 200;
    response.body =
        "{\"content\": [{\"type\": \"text\", \"text\": \"" +
        observations_json_escaped + "\"}]}";
    return response;
}

} // namespace

int main() {
    // build_request_body: model/effort/regions/image are all present.
    {
        auto transport = std::make_shared<FakeHttpTransport>();
        AnthropicVisionRecognitionConfig config;
        config.api_key = "test-key";
        config.model = "claude-sonnet-5";
        config.effort = "low";

        AnthropicVisionTextRecognitionProvider provider(transport, config);
        const std::vector<TextRegion> regions = {region("r1", 10, 20)};
        const std::vector<uint8_t> fake_png = {0x89, 0x50, 0x4E, 0x47};

        const std::string body = provider.build_request_body(regions, fake_png);
        assert(body.find("\"model\":\"claude-sonnet-5\"") != std::string::npos);
        assert(body.find("\"effort\":\"low\"") != std::string::npos);
        assert(body.find("r1") != std::string::npos);
        assert(body.find("\"type\":\"image\"") != std::string::npos);
        assert(body.find("base64") != std::string::npos);
    }

    // recognize(): batches requests and aggregates + maps confidence.
    {
        auto transport = std::make_shared<FakeHttpTransport>();
        transport->queued_responses.push_back(ok_response(
            "{\\\"observations\\\": [{\\\"text_region_id\\\": \\\"r1\\\", "
            "\\\"raw_text\\\": \\\"GND\\\", \\\"confidence\\\": \\\"high\\\"}, "
            "{\\\"text_region_id\\\": \\\"r2\\\", \\\"raw_text\\\": \\\"B/W\\\", "
            "\\\"confidence\\\": \\\"medium\\\"}]}"));
        transport->queued_responses.push_back(ok_response(
            "{\\\"observations\\\": [{\\\"text_region_id\\\": \\\"r3\\\", "
            "\\\"raw_text\\\": \\\"IGN\\\", \\\"confidence\\\": \\\"unresolved\\\"}]}"));

        AnthropicVisionRecognitionConfig config;
        config.api_key = "test-key";
        config.regions_per_request = 2;

        AnthropicVisionTextRecognitionProvider provider(transport, config);

        const std::vector<TextRegion> regions = {
            region("r1", 10, 20), region("r2", 30, 40), region("r3", 50, 60)};
        const cv::Mat image = cv::Mat::zeros(10, 10, CV_8UC1);

        const auto evidence = provider.recognize(image, regions, "fixture", 0);

        assert(transport->requested_urls.size() == 2);
        assert(evidence.size() == 2);
        assert(evidence[0].text_region_id == "r1");
        assert(evidence[0].raw_text == "GND");
        assert(evidence[0].confidence == ConfidenceClass::High);
        assert(evidence[0].provider == "anthropic-vision");
        assert(evidence[1].text_region_id == "r2");
        assert(evidence[1].confidence == ConfidenceClass::Medium);
        // r3 was unresolved and must be dropped, not asserted.
    }

    // Non-200 status must throw rather than silently return no evidence.
    {
        auto transport = std::make_shared<FakeHttpTransport>();
        HttpResponse error_response;
        error_response.status_code = 401;
        error_response.body = "{\"error\": {\"message\": \"invalid key\"}}";
        transport->queued_responses.push_back(error_response);

        AnthropicVisionRecognitionConfig config;
        config.api_key = "bad-key";
        AnthropicVisionTextRecognitionProvider provider(transport, config);

        const std::vector<TextRegion> regions = {region("r1", 10, 20)};
        const cv::Mat image = cv::Mat::zeros(10, 10, CV_8UC1);

        bool threw = false;
        try {
            const auto unused = provider.recognize(image, regions, "fixture", 0);
            (void)unused;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    // No configured key -> factory returns nullopt.
    {
        test_unsetenv("EKE_DX_WIRE_ANTHROPIC_API_KEY");
        test_unsetenv("ANTHROPIC_API_KEY");
        assert(!anthropic_vision_config_from_environment().has_value());

        test_setenv("EKE_DX_WIRE_ANTHROPIC_API_KEY", "from-env");
        const auto config = anthropic_vision_config_from_environment();
        assert(config.has_value());
        assert(config->api_key == "from-env");
        test_unsetenv("EKE_DX_WIRE_ANTHROPIC_API_KEY");
    }

    return 0;
}
