#pragma once
// Data Mapping Engine — transform functions, JSON data extraction,
// field mapping execution, validation, and post-processing.
// Mirrors Tauri desktop MappingEngine + transformFunctions + parsers.

#include "data_mapping_types.h"
#include "data_mapping_schemas.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <optional>

namespace fincept::data_mapping {

using json = nlohmann::json;

// ============================================================================
// Transform Registry — built-in transform functions
// ============================================================================
struct TransformRegistry {
    // Apply a named transform to a JSON value
    static json apply(const std::string& name, const json& value,
                      const json& params = json::object());

    // Get all available transform definitions (for UI)
    static std::vector<TransformDef> all_transforms();
};

// ============================================================================
// JSON Direct Parser — extract values using dot-separated paths
// ============================================================================
struct DirectParser {
    // Extract value at path like "data.candles" or "results[0].price"
    static json extract(const json& data, const std::string& path);
};

// ============================================================================
// URL Builder — build URLs from ApiConfig + runtime parameters
// ============================================================================
struct UrlBuilder {
    // Replace {placeholder} tokens in text with parameter values
    static std::string replace_placeholders(const std::string& text,
                                            const std::map<std::string, std::string>& params);

    // Build full URL from ApiConfig and runtime parameters
    static std::string build_url(const ApiConfig& config,
                                 const std::map<std::string, std::string>& params = {});

    // Build HTTP headers from ApiConfig
    static std::map<std::string, std::string> build_headers(const ApiConfig& config);

    // Extract placeholder names from a string
    static std::vector<std::string> extract_placeholders(const std::string& text);

    // Validate URL format (simple check)
    static bool is_valid_url(const std::string& url);
};

// ============================================================================
// Mapping Engine — execute a full mapping pipeline
// ============================================================================
class MappingEngine {
public:
    // Execute full mapping: fetch/use data -> extract -> transform -> validate
    ExecutionResult execute(const MappingConfig& mapping,
                           const json& sample_data = json(),
                           const std::map<std::string, std::string>& params = {});

    // Test mapping with sample data
    ExecutionResult test(const MappingConfig& mapping, const json& sample_data,
                         const std::map<std::string, std::string>& params = {});

    // Execute API request
    ApiResponse fetch_api(const ApiConfig& config,
                          const std::map<std::string, std::string>& params = {});

private:
    // Extract data using configured root path
    json extract_data(const json& raw_data, const MappingConfig& mapping);

    // Transform a single record using field mappings
    json transform_single(const json& source, const MappingConfig& mapping);

    // Extract a single field value from source data
    json extract_field_value(const json& source, const FieldMapping& mapping);

    // Apply post-processing (sort, filter, deduplicate, limit)
    json apply_post_processing(const json& data, const MappingConfig& mapping);

    // Validate transformed data against schema
    ValidationResult validate_data(const json& data, const MappingConfig& mapping);
};

} // namespace fincept::data_mapping
