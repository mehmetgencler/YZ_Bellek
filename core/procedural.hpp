#pragma once
#include "memory_object.hpp"
#include "value.hpp"
#include <vector>
#include <optional>
#include <string>
#include <chrono>

namespace om {

    enum class ProcedureState {
        Draft,
        Active,
        Deprecated,
        Archived
    };

    struct Contract {
        std::vector<Value> inputs;
        std::vector<Value> outputs;
    };

    struct NativeBody {
        std::string function_name;
    };
    struct CompositeBody {
        std::vector<UUID> procedure_ids;
    };
    struct OpaqueRecipe {
        std::string recipe;
    };

    class Procedure : public MemoryObject {
    public:
        std::string name;
        std::string description;
        Contract contract;
        ProcedureState state = ProcedureState::Draft;
        std::optional<UUID> superseded_by;
        // variant for body
        enum class BodyType { Native, Composite, Opaque } body_type;
        NativeBody native_body;
        CompositeBody composite_body;
        OpaqueRecipe opaque_body;

        Procedure(const UUID& id, const std::string& owner)
            : MemoryObject(id, owner), body_type(BodyType::Opaque) {}

        ObjectType type() const override { return ObjectType::Procedure; }
        bool is_valid() const override { return !name.empty(); }
        std::string serialize() const override {
            nlohmann::json j = to_json();
            j["name"] = name;
            j["description"] = description;
            j["state"] = static_cast<int>(state);
            // contract
            j["contract"] = nlohmann::json::array();
            for (auto &in : contract.inputs) j["contract"].push_back(value_to_json(in));
            // body
            j["body_type"] = static_cast<int>(body_type);
            if (body_type == BodyType::Native) j["native"] = native_body.function_name;
            if (body_type == BodyType::Composite) j["composite"] = composite_body.procedure_ids;
            if (body_type == BodyType::Opaque) j["opaque"] = opaque_body.recipe;
            return j.dump();
        }
    };

    enum class ExecutionStatus {
        Success,
        Failure,
        Partial
    };

    class Execution : public MemoryObject {
    public:
        UUID procedure_id;
        std::vector<UUID> input_facts;
        std::vector<UUID> output_facts;
        ExecutionStatus status = ExecutionStatus::Failure;
        std::string error_log;
        std::chrono::milliseconds elapsed_time{0};

        Execution(const UUID& id, const std::string& owner)
            : MemoryObject(id, owner) {}

        ObjectType type() const override { return ObjectType::Execution; }
        bool is_valid() const override { return !procedure_id.empty(); }
        std::string serialize() const override {
            nlohmann::json j = to_json();
            j["procedure_id"] = procedure_id;
            j["input_facts"] = input_facts;
            j["output_facts"] = output_facts;
            j["status"] = static_cast<int>(status);
            j["error_log"] = error_log;
            j["elapsed_ms"] = elapsed_time.count();
            return j.dump();
        }
    };
}
