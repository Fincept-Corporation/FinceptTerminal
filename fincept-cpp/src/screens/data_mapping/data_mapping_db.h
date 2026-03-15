#pragma once
// Data Mapping Database — SQLite persistence for mapping configs and cache.
// Mirrors Tauri desktop MappingDatabase service.

#include "data_mapping_types.h"
#include <string>
#include <vector>
#include <optional>

namespace fincept::data_mapping {

// ============================================================================
// Database operations for data mappings
// ============================================================================
namespace mapping_ops {

// Initialize tables (called once at startup)
void initialize_tables();

// --- Mapping CRUD ---
void save_mapping(const MappingConfig& config);
std::vector<MappingConfig> get_all_mappings();
std::optional<MappingConfig> get_mapping(const std::string& id);
void delete_mapping(const std::string& id);
MappingConfig duplicate_mapping(const std::string& id);

// --- Cache ---
std::optional<std::string> get_cached_response(const std::string& mapping_id,
                                                const std::string& params_hash);
void set_cached_response(const std::string& mapping_id,
                          const std::string& params_hash,
                          const std::string& response_json,
                          int ttl_seconds);
void clear_cache(const std::string& mapping_id = "");
void clean_expired_cache();

// --- Preferences ---
std::optional<std::string> get_preference(const std::string& key);
void set_preference(const std::string& key, const std::string& value);

} // namespace mapping_ops
} // namespace fincept::data_mapping
