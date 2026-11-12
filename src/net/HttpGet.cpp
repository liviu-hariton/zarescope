// SPDX-License-Identifier: GPL-3.0-or-later

#include "net/HttpGet.h"

#include <curl/curl.h>

namespace zarescope::net {

namespace {

constexpr long kTimeoutSeconds = 20;

struct Sink {
    std::string body;
    size_t maxBytes = 0;
};

size_t collect(char* data, size_t size, size_t count, void* user) {
    auto* sink = static_cast<Sink*>(user);
    const size_t bytes = size * count;
    if (sink->body.size() + bytes > sink->maxBytes) {
        return 0;  // refuse oversized answers: libcurl stops with a write error
    }
    sink->body.append(data, bytes);
    return bytes;
}

}  // namespace

std::optional<std::string> httpGet(const std::string& url, const std::string& userAgent,
                                   size_t maxBytes, std::string& error, bool allowFile) {
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        error = "network unavailable";
        return std::nullopt;
    }
    Sink sink{.body = {}, .maxBytes = maxBytes};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, allowFile ? "https,file" : "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result == CURLE_WRITE_ERROR) {
        error = "the answer is larger than " + std::to_string(maxBytes) + " bytes";
        return std::nullopt;
    }
    if (result != CURLE_OK) {
        error = curl_easy_strerror(result);
        return std::nullopt;
    }
    // A file:// read has no HTTP status (0); a web answer must be 200.
    if (status != 200 && !(allowFile && status == 0)) {
        error = "the server answered HTTP " + std::to_string(status);
        return std::nullopt;
    }
    return std::move(sink.body);
}

}  // namespace zarescope::net
