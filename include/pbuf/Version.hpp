// Predictive Buffer API - version. Semantic versioning; see docs/VERSIONING.md.
#pragma once

#define PBUF_API_VERSION_MAJOR 0
#define PBUF_API_VERSION_MINOR 1
#define PBUF_API_VERSION_PATCH 0

namespace pbuf {

/// major * 10000 + minor * 100 + patch
constexpr int kApiVersion = PBUF_API_VERSION_MAJOR * 10000 + PBUF_API_VERSION_MINOR * 100 + PBUF_API_VERSION_PATCH;
constexpr int apiMajor(int v) { return v / 10000; }
constexpr int apiMinor(int v) { return (v / 100) % 100; }

} // namespace pbuf
