// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/* Small https download for Movie Sync artwork. On Windows it uses the system's own HTTP stack
   (WinHTTP): OpenRGB's Windows build ships Qt without a TLS backend, so Qt cannot fetch https
   there. Elsewhere it reports "unsupported" and the caller uses Qt (which has OpenSSL there).
   Blocking: call it from a worker thread. */

#include <cstdint>
#include <string>
#include <vector>

namespace MovieSync
{

/** true when this build has a native downloader (Windows). */
bool NativeHttpsAvailable();

/** Decode any image Windows can read (JPEG, PNG, WebP, and AVIF / HEIF when their Windows
    extensions are installed) to 32-bit premultiplied BGRA, at most max_side pixels on the long
    side. Windows only (false elsewhere). Call from a worker thread. */
bool DecodeImageNative(const std::vector<std::uint8_t>& bytes, int max_side, int* width, int* height,
                       std::vector<std::uint8_t>* bgra);

/** GET an https URL into `out` (at most max_bytes). false + a short reason on failure. */
bool HttpsGet(const std::string& url, std::vector<std::uint8_t>* out, std::string* error, std::size_t max_bytes,
              int timeout_ms);

} // namespace MovieSync
