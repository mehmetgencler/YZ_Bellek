#pragma once
#include "memory_graph.hpp"
#include <unordered_map>
#include <optional>
#include <utility>

namespace om {

    class Transaction {
    public:
        Transaction(MemoryGraph& graph)
            : graph_(graph), snapshot_version_(graph_.global_version_counter_.load()), committed_(false), failed_(false) {}

        using ObjectPtr = std::shared_ptr<const MemoryObject>;

        ObjectPtr read(const UUID& id) {
            // Check write_set first
            auto wit = write_set_.find(id);
            if (wit != write_set_.end()) return wit->second;
            // Check read_set cache
            auto rit = read_set_.find(id);
            if (rit != read_set_.end()) return rit->second.first;
            // Else read from graph
            {
                std::shared_lock<std::shared_mutex> lock(graph_.mutex_);
                auto it = graph_.objects_.find(id);
                if (it != graph_.objects_.end()) {
                    read_set_[id] = std::make_pair(it->second.obj, it->second.version);
                    return it->second.obj;
                }
            }
            // Try cold
            auto loaded = graph_.cold_.load(id);
            if (loaded) {
                // Do not modify graph_ without lock; cache locally
                read_set_[id] = std::make_pair(loaded->obj, loaded->version);
                return loaded->obj;
            }
            return nullptr;
        }

        void stage_create(ObjectPtr obj) {
            write_set_[obj->id()] = obj;
        }

        bool commit() {
            // Phase 1: write payloads to temp files (NO graph lock)
            for (auto &p : write_set_) {
                auto &obj = p.second;
                try {
                    graph_.cold_.write_temp(obj->id(), obj->serialize());
                } catch (...) {
                    failed_ = true;
                    return false;
                }
            }

            // Phase 2: obtain new version(s), finalize writes and insert into graph under lock
            uint64_t assigned_version = graph_.global_version_counter_.fetch_add(1);

            // Build wrapper and finalize each object with version
            for (auto &p : write_set_) {
                try {
                    graph_.cold_.save_with_version(p.first, *p.second, assigned_version);
                } catch (...) {
                    failed_ = true;
                    return false;
                }
            }

            // Acquire graph unique lock to validate and insert
            {
                std::unique_lock<std::shared_mutex> lock(graph_.mutex_);
                // Validate read_set observed versions
                for (auto &r : read_set_) {
                    auto it = graph_.objects_.find(r.first);
                    uint64_t observed = r.second.second;
                    if (it != graph_.objects_.end()) {
                        if (it->second.version != observed) {
                            // conflict
                            failed_ = true;
                            return false;
                        }
                    } else {
                        // if not present in graph and observed != 0 -> conflict
                        if (observed != 0) {
                            failed_ = true;
                            return false;
                        }
                    }
                }
                // Insert writes
                for (auto &p : write_set_) {
                    StoredObject so;
                    so.obj = p.second;
                    so.version = assigned_version;
                    graph_.objects_[p.first] = so;
                    graph_.owner_index_[p.second->owner()].push_back(p.first);
                    // Per lock-order: call hot_.put while holding graph_.mutex_ to ensure graph -> hot order
                    graph_.hot_.put(p.second, [&](std::shared_ptr<const MemoryObject> o){
                        try {
                            graph_.cold_.save_with_version(o->id(), *o, assigned_version);
                            return true;
                        } catch (...) {
                            throw;
                        }
                    });
                }
            }

            committed_ = true;
            return true;
        }

        void rollback() {
            write_set_.clear();
            read_set_.clear();
        }

    private:
        MemoryGraph& graph_;
        uint64_t snapshot_version_;
        std::unordered_map<UUID, std::pair<ObjectPtr, uint64_t>> read_set_;
        std::unordered_map<UUID, ObjectPtr> write_set_;
        bool committed_;
        bool failed_;
    };
}
