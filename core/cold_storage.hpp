#pragma once
#include "memory_object.hpp"
#include "memory_graph.hpp"
#include <filesystem>
#include <shared_mutex>
#include <unordered_map>
#include <optional>

namespace om {

    class ColdStorage {
    public:
        ColdStorage(const std::filesystem::path& root_dir);

        // Phase 1 (temp write)
        void write_temp(const UUID& id, const std::string& payload_json);

        // Phase 2 (finalize) - throws on error
        void write_final(const UUID& id, const std::string& wrapper_json);

        // Used by eviction callback; throws on error
        void save_with_version(const UUID& id, const MemoryObject& obj, uint64_t version);

        // Load and parse wrapper.json; returns StoredObject with RawObject payload
        std::optional<StoredObject> load(const UUID& id);

        // Index persisting (atomic temp -> rename)
        void persist_index_nolock();

        bool contains(const UUID& id);

    private:
        std::filesystem::path root_;
        std::filesystem::path data_dir_;
        std::filesystem::path index_file_;
        std::shared_mutex file_map_mutex_;
        std::unordered_map<UUID, std::filesystem::path> file_map_;
    };
}
