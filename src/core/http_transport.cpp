#include "eke_dx_wire/core/http_transport.hpp"

#include <curl/curl.h>

namespace eke::dx::wire {

namespace {

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

CurlHttpTransport::CurlHttpTransport(long timeout_seconds)
    : timeout_seconds_(timeout_seconds) {}

HttpResponse CurlHttpTransport::post_json(
    const std::string& url,
    const std::vector<HttpHeader>& headers,
    const std::string& json_body) const {

    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.transport_error = true;
        response.transport_error_message = "curl_easy_init failed";
        return response;
    }

    curl_slist* header_list = nullptr;
    for (const auto& header : headers) {
        const std::string line = header.name + ": " + header.value;
        header_list = curl_slist_append(header_list, line.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        response.transport_error = true;
        response.transport_error_message = curl_easy_strerror(result);
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.status_code = status;
    }

    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);

    return response;
}

} // namespace eke::dx::wire
