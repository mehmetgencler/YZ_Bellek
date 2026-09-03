#pragma once
#include "memory_object.hpp"
#include "value.hpp"
#include <optional>
#include <vector>
#include <string>

namespace om {

    struct TimeRange {
        std::int64_t start_ms;
        std::int64_t end_ms;
    };

    class Fact : public MemoryObject {
    public:
        std::string entity;
        std::string predicate;
        Value value;
        TimeRange time_range;
        double confidence;
        std::optional<UUID> superseded_by;

        Fact(const UUID& id, const std::string& owner)
            : MemoryObject(id, owner), confidence(1.0) {}

        ObjectType type() const override { return ObjectType::Fact; }
        bool is_valid() const override {
            return !entity.empty() && !predicate.empty();
        }
        std::string serialize() const override {
            nlohmann::json j = to_json();
            j["entity"] = entity;
            j["predicate"] = predicate;
            j["value"] = value_to_json(value);
            j["confidence"] = confidence;
            if (superseded_by) j["superseded_by"] = *superseded_by;
            j["time_range"] = { {"start", time_range.start_ms}, {"end", time_range.end_ms} };
            return j.dump();
        }
    };

    class Event : public MemoryObject {
    public:
        std::string action;
        std::vector<UUID> involved_facts;
        Value payload;

        Event(const UUID& id, const std::string& owner)
            : MemoryObject(id, owner) {}

        ObjectType type() const override { return ObjectType::Event; }
        bool is_valid() const override {
            return !action.empty();
        }
        std::string serialize() const override {
            nlohmann::json j = to_json();
            j["action"] = action;
            j["involved_facts"] = involved_facts;
            j["payload"] = value_to_json(payload);
            return j.dump();
        }
    };

    class Belief : public Fact {
    public:
        std::vector<UUID> evidence;
        Belief(const UUID& id, const std::string& owner) : Fact(id, owner) {}
        ObjectType type() const override { return ObjectType::Belief; }
        std::string serialize() const override {
            nlohmann::json j = to_json();
            j["entity"] = entity;
            j["predicate"] = predicate;
            j["value"] = value_to_json(value);
            j["evidence"] = evidence;
            j["confidence"] = confidence;
            return j.dump();
        }
    };
}
