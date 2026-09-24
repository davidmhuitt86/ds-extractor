#pragma once

#include <string>
#include <vector>

namespace eke::dx::wire {

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpResponse {
    long status_code = 0;
    std::string body;
    bool transport_error = false;
    std::string transport_error_message;
};

/**
 * Injectable boundary between a recognition provider and the HTTP request
 * it needs to make. Providers depend on this interface, not on libcurl
 * directly, so their request-building and response-parsing logic can be
 * unit-tested without a live network connection.
 */
class HttpTransport {
public:
    virtual ~HttpTransport() = default;

    [[nodiscard]] virtual HttpResponse post_json(
        const std::string& url,
        const std::vector<HttpHeader>& headers,
        const std::string& json_body) const = 0;
};

/**
 * Default transport. Performs a real HTTPS POST via libcurl with TLS
 * verification enabled. This is the only part of the recognition-provider
 * boundary that touches the network.
 */
class CurlHttpTransport final : public HttpTransport {
public:
    explicit CurlHttpTransport(long timeout_seconds = 120);

    [[nodiscard]] HttpResponse post_json(
        const std::string& url,
        const std::vector<HttpHeader>& headers,
        const std::string& json_body) const override;

private:
    long timeout_seconds_;
};

} // namespace eke::dx::wire
