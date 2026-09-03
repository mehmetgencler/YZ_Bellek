#pragma once
#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace om {
    using Value = std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<std::string>,
        std::unordered_map<std::string, std::string>
    >;

    inline nlohmann::json value_to_json(const Value& v) {
        nlohmann::json j;
        if (std::holds_alternative<std::monostate>(v)) {
            j = nullptr;
        } else if (std::holds_alternative<bool>(v)) {
            j = std::get<bool>(v);
        } else if (std::holds_alternative<int64_t>(v)) {
            j = std::get<int64_t>(v);
        } else if (std::holds_alternative<double>(v)) {
            j = std::get<double>(v);
        } else if (std::holds_alternative<std::string>(v)) {
            j = std::get<std::string>(v);
        } else if (std::holds_alternative<std::vector<std::string>>(v)) {
            j = std::get<std::vector<std::string>>(v);
        } else if (std::holds_alternative<std::unordered_map<std::string, std::string>>(v)) {
            j = std::get<std::unordered_map<std::string, std::string>>(v);
        }
        return j;
    }

    inline Value value_from_json(const nlohmann::json& j) {
        if (j.is_null()) return std::monostate{};
        if (j.is_boolean()) return j.get<bool>();
        if (j.is_number_integer()) return static_cast<int64_t>(j.get<int64_t>());
        if (j.is_number_float()) return j.get<double>();
        if (j.is_string()) return j.get<std::string>();
        if (j.is_array()) {
            std::vector<std::string> vec;
            for (auto &el : j) vec.push_back(el.get<std::string>());
            return vec;
        }
        if (j.is_object()) {
            std::unordered_map<std::string, std::string> map;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.value().is_string())
                    map[it.key()] = it.value().get<std::string>();
                else
                    map[it.key()] = it.value().dump();
            }
            return map;
        }
        return std::monostate{};
    }
}
