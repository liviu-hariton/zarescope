// SPDX-License-Identifier: GPL-3.0-or-later
//
// httpGet: one HTTPS GET with libcurl, for the few small files Zarescope
// downloads (satellite orbits from CelesTrak, the version file on
// zarescope.eu). Blocking: call it from a worker thread, never the UI's.
//
// Every request names the app in its User-Agent and carries nothing else
// about the user. Answers larger than `maxBytes` are refused, so a wrong or
// hostile server can't fill the memory; redirects are followed (at most 3),
// but only to HTTPS.

#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace zarescope::net {

// The body of a 200 answer, or std::nullopt with `error` saying why not
// ("Couldn't resolve host name", "the server answered HTTP 503", …).
// `allowFile` also accepts file:// URLs, for tests and development
// (ZARESCOPE_UPDATE_URL).
std::optional<std::string> httpGet(const std::string& url, const std::string& userAgent,
                                   size_t maxBytes, std::string& error, bool allowFile = false);

}  // namespace zarescope::net
