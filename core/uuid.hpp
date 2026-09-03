#pragma once
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace om {
    using UUID = std::string;

    inline UUID generate_uuid() {
        // simple random hex string, not RFC-4122, satisfies spec requirement
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
        uint64_t a = dis(gen);
        uint64_t b = dis(gen);
        std::ostringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << a
           << std::setw(16) << std::setfill('0') << b;
        return ss.str();
    }
}
