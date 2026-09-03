#pragma once
#include "uuid.hpp"
#include "memory_object.hpp"
#include "memory_graph.hpp" // forward declaration for StoredObject
#include <optional>

namespace om {
    class StorageTier {
    public:
        virtual ~StorageTier() = default;
        virtual std::optional<StoredObject> get(const UUID& id) = 0;
        virtual bool contains(const UUID& id) = 0;
        virtual bool is_mutable() const = 0;
    };
}
