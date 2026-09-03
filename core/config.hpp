#pragma once
#include <cstddef>
#include <chrono>

namespace om {
    inline constexpr size_t DEFAULT_HOT_CACHE_SIZE = 1024;
    inline constexpr size_t DEFAULT_COLD_MAX_SIZE_BYTES = 1024ull * 1024ull * 1024ull; // 1 GB
    inline constexpr uint64_t DEFAULT_VERSION_FALLBACK = 1;
}
