#pragma once
#include "uuid.hpp"
#include "object_type.hpp"
#include <chrono>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace om {

    using Clock = std::chrono::system_clock;

    class MemoryObject {
    protected:
        UUID id_;
        std::string owner_;
        Clock::time_point recorded_at_;
        mutable Clock::time_point last_accessed_;
        mutable size_t serialized_size_ = 0;

    public:
        MemoryObject(const UUID& id, const std::string& owner)
            : id_(id),
              owner_(owner),
              recorded_at_(Clock::now()),
              last_accessed_(Clock::now()),
              serialized_size_(0)
        {}

        virtual ~MemoryObject() = default;

        // No copies/moves: enforce immutability
        MemoryObject(const MemoryObject&) = delete;
        MemoryObject& operator=(const MemoryObject&) = delete;
        MemoryObject(MemoryObject&&) = delete;
        MemoryObject& operator=(MemoryObject&&) = delete;

        const UUID& id() const { return id_; }
        const std::string& owner() const { return owner_; }
        Clock::time_point recorded_at() const { return recorded_at_; }
        Clock::time_point last_accessed() const { return last_accessed_; }
        size_t serialized_size() const { return serialized_size_; }

        virtual ObjectType type() const = 0;
        virtual bool is_valid() const = 0;
        virtual std::string serialize() const = 0;

        // Static dispatcher - minimal factory that returns a lightweight container object
        static std::shared_ptr<const MemoryObject> deserialize(const std::string& data);

        virtual nlohmann::json to_json() const {
            nlohmann::json j;
            j["id"] = id_;
            j["owner"] = owner_;
            j["recorded_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(recorded_at_.time_since_epoch()).count();
            j["last_accessed"] = std::chrono::duration_cast<std::chrono::milliseconds>(last_accessed_.time_since_epoch()).count();
            j["type"] = static_cast<int>(type());
            return j;
        }
    };

    // A minimal concrete object used for deserialization fallback / cold storage load:
    class RawObject : public MemoryObject {
        nlohmann::json payload_;
        ObjectType obj_type_;
    public:
        RawObject(const UUID& id, const std::string& owner, ObjectType t, nlohmann::json payload)
            : MemoryObject(id, owner), payload_(std::move(payload)), obj_type_(t) {}

        ObjectType type() const override { return obj_type_; }
        bool is_valid() const override { return true; }
        std::string serialize() const override { return payload_.dump(); }
        nlohmann::json payload() const { return payload_; }

        nlohmann::json to_json() const override {
            nlohmann::json j = MemoryObject::to_json();
            j["payload"] = payload_;
            return j;
        }
    };
}
