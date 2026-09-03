#pragma once
#include "memory_object.hpp"
#include <unordered_map>
#include <list>
#include <shared_mutex>
#include <functional>
#include <stdexcept>

namespace om {

    using MemoryObjectPtr = std::shared_ptr<const MemoryObject>;
    using EvictionCallback = std::function<bool(MemoryObjectPtr)>;

    // Header-only LRU HotCache
    class HotCache {
    public:
        HotCache(size_t capacity = om::DEFAULT_HOT_CACHE_SIZE)
            : capacity_(capacity) {}

        // Thread-safe public API
        bool contains(const UUID& id) {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return cache_.find(id) != cache_.end();
        }

        std::optional<MemoryObjectPtr> get(const UUID& id) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = cache_.find(id);
            if (it == cache_.end()) return std::nullopt;
            touch_locked(it->first);
            return it->second;
        }

        // Put must be called while holding graph lock per lock-order rule.
        // on_evicted can throw; if it does we must reinsert the evicted object and rethrow.
        void put(MemoryObjectPtr obj, EvictionCallback on_evicted = nullptr) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            const UUID id = obj->id();
            auto it = cache_.find(id);
            if (it != cache_.end()) {
                touch_locked(id);
                return;
            }
            // Insert front
            lru_list_.push_front(id);
            lru_iters_[id] = lru_list_.begin();
            cache_[id] = obj;
            // Evict if needed
            if (cache_.size() > capacity_) {
                UUID evict_id = lru_list_.back();
                auto evicted_obj = cache_[evict_id];
                // Remove from structures first
                lru_iters_.erase(evict_id);
                lru_list_.pop_back();
                cache_.erase(evict_id);
                // Call eviction callback without holding the graph lock (but we still hold hot mutex).
                try {
                    if (on_evicted) {
                        bool keep_evicted_saved = on_evicted(evicted_obj);
                        // callback returns true = saved successfully / ok, nothing to do
                        (void)keep_evicted_saved;
                    }
                } catch (...) {
                    // reinstate evicted object to preserve data (no silent loss), then rethrow
                    lru_list_.push_back(evict_id);
                    auto lit = std::prev(lru_list_.end());
                    lru_iters_[evict_id] = lit;
                    cache_[evict_id] = evicted_obj;
                    throw;
                }
            }
        }

        size_t size() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return cache_.size();
        }

        void clear() {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            cache_.clear();
            lru_list_.clear();
            lru_iters_.clear();
        }

    private:
        void touch_locked(const UUID& id) {
            // assumes mutex_ locked
            auto it = lru_iters_.find(id);
            if (it == lru_iters_.end()) return;
            lru_list_.erase(it->second);
            lru_list_.push_front(id);
            lru_iters_[id] = lru_list_.begin();
            auto cit = cache_.find(id);
            if (cit != cache_.end()) {
                // update last_accessed_
                // Note: caching object is const ptr to MemoryObject; last_accessed_ is mutable
                cit->second->last_accessed_ = Clock::now();
            }
        }

        size_t capacity_;
        mutable std::shared_mutex mutex_;
        std::unordered_map<UUID, MemoryObjectPtr> cache_;
        std::list<UUID> lru_list_;
        std::unordered_map<UUID, std::list<UUID>::iterator> lru_iters_;
    };
}
