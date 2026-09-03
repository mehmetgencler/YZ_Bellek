#include "cold_storage.hpp"
#include "memory_object.hpp"
#include <fstream>
#include <iostream>

using namespace om;
using namespace std::filesystem;

ColdStorage::ColdStorage(const path& root_dir)
    : root_(root_dir),
      data_dir_(root_ / "data"),
      index_file_(root_ / "index.json")
{
    std::error_code ec;
    create_directories(data_dir_, ec);
    // load index if exists (best-effort)
    if (exists(index_file_)) {
        std::ifstream in(index_file_);
        if (in) {
            nlohmann::json j;
            in >> j;
            std::unique_lock<std::shared_mutex> lock(file_map_mutex_);
            for (auto it = j.begin(); it != j.end(); ++it) {
                file_map_[it.key()] = data_dir_ / it.value().get<std::string>();
            }
        }
    }
}

void ColdStorage::write_temp(const UUID& id, const std::string& payload_json) {
    path temp_path = data_dir_ / (id + ".tmp");
    std::ofstream out(temp_path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open temp file for writing: " + temp_path.string());
    out << payload_json;
    out.close();
}

void ColdStorage::write_final(const UUID& id, const std::string& wrapper_json) {
    path final_path = data_dir_ / (id + ".json");
    path temp_path = data_dir_ / (id + ".tmp");
    std::ofstream out(temp_path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open temp file for final write: " + temp_path.string());
    out << wrapper_json;
    out.close();
    std::error_code ec;
    rename(temp_path, final_path, ec);
    if (ec) {
        std::remove(temp_path.string().c_str());
        throw std::runtime_error("Failed to rename temp to final: " + ec.message());
    }
    {
        std::unique_lock<std::shared_mutex> lock(file_map_mutex_);
        file_map_[id] = final_path;
    }
}

void ColdStorage::save_with_version(const UUID& id, const MemoryObject& obj, uint64_t version) {
    // Build wrapper JSON
    nlohmann::json wrapper;
    wrapper["version"] = version;
    wrapper["type"] = static_cast<int>(obj.type());
    wrapper["recorded_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(obj.recorded_at().time_since_epoch()).count();
    wrapper["last_accessed"] = std::chrono::duration_cast<std::chrono::milliseconds>(obj.last_accessed().time_since_epoch()).count();
    try {
        // If obj.serialize() returns JSON text, try to parse it to embed as object
        wrapper["payload"] = nlohmann::json::parse(obj.serialize());
    } catch (...) {
        // Fallback: store the serialized string
        wrapper["payload"] = obj.serialize();
    }

    path final_path = data_dir_ / (id + ".json");
    path temp_path = data_dir_ / (id + ".tmp");
    {
        std::ofstream out(temp_path, std::ios::binary);
        if (!out) throw std::runtime_error("Failed to open temp file for save_with_version: " + temp_path.string());
        out << wrapper.dump();
        out.close();
    }
    std::error_code ec;
    rename(temp_path, final_path, ec);
    if (ec) {
        std::remove(temp_path.string().c_str());
        throw std::runtime_error("Failed to rename temp to final in save_with_version: " + ec.message());
    }

    {
        std::unique_lock<std::shared_mutex> lock(file_map_mutex_);
        file_map_[id] = final_path;
    }

    persist_index_nolock();
}

std::optional<StoredObject> ColdStorage::load(const UUID& id) {
    path final_path;
    {
        std::shared_lock<std::shared_mutex> lock(file_map_mutex_);
        auto it = file_map_.find(id);
        if (it == file_map_.end()) {
            final_path = data_dir_ / (id + ".json");
        } else {
            final_path = it->second;
        }
    }
    if (!exists(final_path)) return std::nullopt;
    std::ifstream in(final_path);
    if (!in) return std::nullopt;
    nlohmann::json j;
    in >> j;

    uint64_t file_version = j.value("version", (uint64_t)om::DEFAULT_VERSION_FALLBACK);
    int t = j.value("type", static_cast<int>(ObjectType::Fact));
    ObjectType objt = static_cast<ObjectType>(t);

    nlohmann::json payload = j.value("payload", nlohmann::json::object());
    UUID owner = "cold";
    if (j.contains("owner") && j["owner"].is_string()) {
        owner = j["owner"].get<std::string>();
    } else if (payload.is_object() && payload.contains("owner") && payload["owner"].is_string()) {
        owner = payload["owner"].get<std::string>();
    }

    auto obj = std::make_shared<RawObject>(id, owner, objt, payload);
    StoredObject s;
    s.obj = obj;
    s.version = file_version;
    return s;
}

void ColdStorage::persist_index_nolock() {
    nlohmann::json j;
    {
        std::shared_lock<std::shared_mutex> lock(file_map_mutex_);
        for (auto &p : file_map_) {
            j[p.first] = std::filesystem::relative(p.second, root_).string();
        }
    }
    path temp = root_ / "index.tmp";
    std::ofstream out(temp, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open index temp file");
    out << j.dump();
    out.close();
    std::error_code ec;
    rename(temp, index_file_, ec);
    if (ec) {
        std::remove(temp.string().c_str());
        throw std::runtime_error("Failed to rename index temp: " + ec.message());
    }
}

bool ColdStorage::contains(const UUID& id) {
    std::shared_lock<std::shared_mutex> lock(file_map_mutex_);
    auto it = file_map_.find(id);
    if (it != file_map_.end()) return exists(it->second);
    return exists(data_dir_ / (id + ".json"));
}
