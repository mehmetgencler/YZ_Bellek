#pragma once
#include "memory_graph.hpp"
#include "procedural.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include <optional>

namespace om {

    using NativeFunction = std::function<Execution(const Procedure&, const std::vector<UUID>& inputs)>;

    class ProcedureEngine {
    public:
        ProcedureEngine(MemoryGraph& graph) : graph_(graph) {}

        void register_native(const std::string& name, NativeFunction fn) {
            std::unique_lock<std::shared_mutex> lock(registry_mutex_);
            native_registry_[name] = fn;
        }

        // Execute a procedure (native or composite)
        Execution execute(const UUID& procedure_id, const std::vector<UUID>& input_facts) {
            // Start transaction
            Transaction tx(graph_);
            auto proc_ptr = tx.read(procedure_id);
            if (!proc_ptr) {
                // return failure execution object
                Execution ex(generate_uuid(), "engine");
                ex.procedure_id = procedure_id;
                ex.status = ExecutionStatus::Failure;
                ex.error_log = "Procedure not found";
                return ex;
            }
            auto proc = std::dynamic_pointer_cast<const Procedure>(proc_ptr);
            if (!proc) {
                Execution ex(generate_uuid(), "engine");
                ex.procedure_id = procedure_id;
                ex.status = ExecutionStatus::Failure;
                ex.error_log = "Object is not a Procedure";
                return ex;
            }

            // Validate contract - minimal: number of inputs
            if (proc->contract.inputs.size() != input_facts.size()) {
                Execution ex(generate_uuid(), "engine");
                ex.procedure_id = procedure_id;
                ex.status = ExecutionStatus::Failure;
                ex.error_log = "Contract input arity mismatch";
                return ex;
            }

            // If native, lookup and call. Must release registry lock before invoking callback.
            if (proc->body_type == Procedure::BodyType::Native) {
                NativeFunction fn;
                {
                    std::shared_lock<std::shared_mutex> rlock(registry_mutex_);
                    auto it = native_registry_.find(proc->native_body.function_name);
                    if (it == native_registry_.end()) {
                        Execution ex(generate_uuid(), "engine");
                        ex.procedure_id = procedure_id;
                        ex.status = ExecutionStatus::Failure;
                        ex.error_log = "Native function not registered";
                        return ex;
                    }
                    fn = it->second;
                }
                // Invoke native function outside registry lock to avoid deadlocks
                Execution result = fn(*proc, input_facts);
                // Attempt commit of resulting Execution object
                auto exec_ptr = std::make_shared<Execution>(result);
                tx.stage_create(exec_ptr);
                if (!tx.commit()) {
                    // Create failure fallback execution
                    Execution ex2(generate_uuid(), "engine");
                    ex2.procedure_id = procedure_id;
                    ex2.status = ExecutionStatus::Failure;
                    ex2.error_log = "Commit conflict/failure";
                    return ex2;
                }
                return result;
            }

            // Composite: sequentially execute children
            if (proc->body_type == Procedure::BodyType::Composite) {
                Execution final_exec(generate_uuid(), "engine");
                final_exec.procedure_id = procedure_id;
                for (auto &child_id : proc->composite_body.procedure_ids) {
                    Execution child = execute(child_id, input_facts); // recursive
                    if (child.status != ExecutionStatus::Success) {
                        final_exec.status = ExecutionStatus::Partial;
                    } else {
                        final_exec.status = ExecutionStatus::Success;
                    }
                }
                // Persist final execution
                auto final_ptr = std::make_shared<Execution>(final_exec);
                tx.stage_create(final_ptr);
                if (!tx.commit()) {
                    Execution ex2(generate_uuid(), "engine");
                    ex2.procedure_id = procedure_id;
                    ex2.status = ExecutionStatus::Failure;
                    ex2.error_log = "Commit conflict on composite finalization";
                    return ex2;
                }
                return final_exec;
            }

            // Opaque recipe - not executable natively here
            Execution ex(generate_uuid(), "engine");
            ex.procedure_id = procedure_id;
            ex.status = ExecutionStatus::Failure;
            ex.error_log = "Opaque recipe cannot be executed by engine";
            return ex;
        }

    private:
        MemoryGraph& graph_;
        std::unordered_map<std::string, NativeFunction> native_registry_;
        std::shared_mutex registry_mutex_;
    };
}
