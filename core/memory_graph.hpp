#pragma once
#include "memory_object.hpp"
#include "hot_cache.hpp"
#include "cold_storage.hpp"
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <atomic>

namespace om {

    struct StoredObject {
        std::shared_ptr<const MemoryObject> obj;
        uint64_t version;
    };

    class MemoryGraph {
    public:
        MemoryGraph(const std::filesystem::path& cold_root)
            : cold_(cold_root), global_version_counter_{1} {}

        // Lock ordering CRITICAL:
        // ALWAYS acquire graph_.mutex_ before hot_.mutex_ (i.e., hold this graph mutex while calling hot_.put).
        std::optional<StoredObject> get_object(const UUID& id) {
            {
                std::shared_lock<std::shared_mutex> lock(mutex_);
                auto it = objects_.find(id);
                if (it != objects_.end()) {
                    // move to hot cache (call while holding graph lock, per ordering)
                    hot_.put(it->second.obj, [&](std::shared_ptr<const MemoryObject> o){ 
                        // eviction callback saves to cold; we'll write final with version
                        try {
                            cold_.save_with_version(o->id(), *o, it->second.version);
                            return true;
                        } catch (...) {
                            throw;
                        }
                    });
                    return it->second;
                }
            }
            // Miss -> try load from cold (no graph lock)
            if (cold_.contains(id)) {
                auto loaded = cold_.load(id);
                if (!loaded) return std::nullopt;
                // Insert into graph under unique lock and call hot_.put while holding the graph lock
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_);
                    objects_[id] = *loaded;
                    // reindex/owner index
                    owner_index_[loaded->obj->owner()].push_back(id);
                    // Per lock ordering, hot_.put must be called while holding graph mutex
                    hot_.put(loaded->obj, [&](std::shared_ptr<const MemoryObject> o){
                        try {
                            cold_.save_with_version(o->id(), *o, loaded->version);
                            return true;
                        } catch (...) {
                            throw;
                        }
                    });
                }
                return loaded;
            }
            return std::nullopt;
        }

        // Other members
        std::vector<UUID> find_facts(const std::string& entity, const std::string& predicate) {
            std::vector<UUID> out;
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (auto &p : objects_) {
                if (p.second.obj->type() == ObjectType::Fact) {
                    // crude: deserialize payload if RawObject else skip detailed checks
                    auto raw = std::dynamic_pointer_cast<const RawObject>(p.second.obj);
                    if (raw) {
                        // best-effort checks
                        auto payload = raw->payload();
                        if (payload.contains("entity") && payload["entity"] == entity &&
                            payload.contains("predicate") && payload["predicate"] == predicate) {
                            out.push_back(p.first);
                        }
                    }
                }
            }
            // dedupe - values are unique keys, but ensure uniqueness
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return out;
        }

        void reindex_object(std::shared_ptr<const MemoryObject> obj) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto &vec = owner_index_[obj->owner()];
            auto it = std::find(vec.begin(), vec.end(), obj->id());
            if (it == vec.end()) vec.push_back(obj->id());
        }

        // Expose for Transaction
        std::unordered_map<UUID, StoredObject> objects_;
        std::unordered_map<std::string, std::vector<UUID>> owner_index_;
        HotCache hot_{om::DEFAULT_HOT_CACHE_SIZE};
        ColdStorage cold_;
        mutable std::shared_mutex mutex_;
        std::atomic<uint64_t> global_version_counter_;
    };
}
