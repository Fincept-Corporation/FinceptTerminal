#pragma once
// Data Mapping Types — Enums, structs for API mapping configuration,
// unified schemas, field mappings, and transformation pipeline.
// Mirrors the Tauri desktop DataMapping types.

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace fincept::data_mapping {

// ============================================================================
// Parser Engines — expression languages for data extraction
// ============================================================================
enum class ParserEngine {
    direct,      // Simple dot-path object access
    jsonpath,    // JSONPath queries: $.data.candles[*]
    regex,       // Pattern matching
};

inline const char* parser_engine_label(ParserEngine e) {
    switch (e) {
        case ParserEngine::direct:   return "Direct";
        case ParserEngine::jsonpath: return "JSONPath";
        case ParserEngine::regex:    return "Regex";
    }
    return "?";
}

inline const char* parser_engine_id(ParserEngine e) {
    switch (e) {
        case ParserEngine::direct:   return "direct";
        case ParserEngine::jsonpath: return "jsonpath";
        case ParserEngine::regex:    return "regex";
    }
    return "?";
}

inline ParserEngine parser_engine_from_string(const std::string& s) {
    if (s == "jsonpath") return ParserEngine::jsonpath;
    if (s == "regex")    return ParserEngine::regex;
    return ParserEngine::direct;
}

// ============================================================================
// HTTP Method
// ============================================================================
enum class HttpMethod { get, post, put, del, patch };

inline const char* http_method_label(HttpMethod m) {
    switch (m) {
        case HttpMethod::get:   return "GET";
        case HttpMethod::post:  return "POST";
        case HttpMethod::put:   return "PUT";
        case HttpMethod::del:   return "DELETE";
        case HttpMethod::patch: return "PATCH";
    }
    return "?";
}

inline HttpMethod http_method_from_string(const std::string& s) {
    if (s == "POST")   return HttpMethod::post;
    if (s == "PUT")    return HttpMethod::put;
    if (s == "DELETE") return HttpMethod::del;
    if (s == "PATCH")  return HttpMethod::patch;
    return HttpMethod::get;
}

// ============================================================================
// Authentication type
// ============================================================================
enum class AuthType { none, apikey, bearer, basic, oauth2 };

inline const char* auth_type_label(AuthType t) {
    switch (t) {
        case AuthType::none:   return "None";
        case AuthType::apikey: return "API Key";
        case AuthType::bearer: return "Bearer Token";
        case AuthType::basic:  return "Basic Auth";
        case AuthType::oauth2: return "OAuth2";
    }
    return "?";
}

inline const char* auth_type_id(AuthType t) {
    switch (t) {
        case AuthType::none:   return "none";
        case AuthType::apikey: return "apikey";
        case AuthType::bearer: return "bearer";
        case AuthType::basic:  return "basic";
        case AuthType::oauth2: return "oauth2";
    }
    return "?";
}

inline AuthType auth_type_from_string(const std::string& s) {
    if (s == "apikey") return AuthType::apikey;
    if (s == "bearer") return AuthType::bearer;
    if (s == "basic")  return AuthType::basic;
    if (s == "oauth2") return AuthType::oauth2;
    return AuthType::none;
}

// ============================================================================
// API Key location
// ============================================================================
enum class ApiKeyLocation { header, query };

inline const char* api_key_location_label(ApiKeyLocation l) {
    switch (l) {
        case ApiKeyLocation::header: return "Header";
        case ApiKeyLocation::query:  return "Query Param";
    }
    return "?";
}

// ============================================================================
// Field types for unified schemas
// ============================================================================
enum class FieldType {
    string_type, number_type, boolean_type, datetime_type,
    array_type, object_type, enum_type
};

inline const char* field_type_label(FieldType t) {
    switch (t) {
        case FieldType::string_type:   return "string";
        case FieldType::number_type:   return "number";
        case FieldType::boolean_type:  return "boolean";
        case FieldType::datetime_type: return "datetime";
        case FieldType::array_type:    return "array";
        case FieldType::object_type:   return "object";
        case FieldType::enum_type:     return "enum";
    }
    return "?";
}

inline FieldType field_type_from_string(const std::string& s) {
    if (s == "number")   return FieldType::number_type;
    if (s == "boolean")  return FieldType::boolean_type;
    if (s == "datetime") return FieldType::datetime_type;
    if (s == "array")    return FieldType::array_type;
    if (s == "object")   return FieldType::object_type;
    if (s == "enum")     return FieldType::enum_type;
    return FieldType::string_type;
}

// ============================================================================
// Unified schema types
// ============================================================================
enum class SchemaType { ohlcv, quote, tick, order, position, portfolio, instrument };

inline const char* schema_type_label(SchemaType t) {
    switch (t) {
        case SchemaType::ohlcv:      return "OHLCV";
        case SchemaType::quote:      return "QUOTE";
        case SchemaType::tick:       return "TICK";
        case SchemaType::order:      return "ORDER";
        case SchemaType::position:   return "POSITION";
        case SchemaType::portfolio:  return "PORTFOLIO";
        case SchemaType::instrument: return "INSTRUMENT";
    }
    return "?";
}

inline const char* schema_type_id(SchemaType t) {
    switch (t) {
        case SchemaType::ohlcv:      return "OHLCV";
        case SchemaType::quote:      return "QUOTE";
        case SchemaType::tick:       return "TICK";
        case SchemaType::order:      return "ORDER";
        case SchemaType::position:   return "POSITION";
        case SchemaType::portfolio:  return "PORTFOLIO";
        case SchemaType::instrument: return "INSTRUMENT";
    }
    return "?";
}

inline SchemaType schema_type_from_string(const std::string& s) {
    if (s == "QUOTE")      return SchemaType::quote;
    if (s == "TICK")       return SchemaType::tick;
    if (s == "ORDER")      return SchemaType::order;
    if (s == "POSITION")   return SchemaType::position;
    if (s == "PORTFOLIO")  return SchemaType::portfolio;
    if (s == "INSTRUMENT") return SchemaType::instrument;
    return SchemaType::ohlcv;
}

// ============================================================================
// Schema category
// ============================================================================
enum class SchemaCategory { market_data, trading, portfolio_cat, reference, custom };

inline const char* schema_category_label(SchemaCategory c) {
    switch (c) {
        case SchemaCategory::market_data:    return "Market Data";
        case SchemaCategory::trading:        return "Trading";
        case SchemaCategory::portfolio_cat:  return "Portfolio";
        case SchemaCategory::reference:      return "Reference";
        case SchemaCategory::custom:         return "Custom";
    }
    return "?";
}

// ============================================================================
// Transform category
// ============================================================================
enum class TransformCategory { type_conv, format, math, string_op, date, custom_xform };

inline const char* transform_category_label(TransformCategory c) {
    switch (c) {
        case TransformCategory::type_conv:    return "Type";
        case TransformCategory::format:       return "Format";
        case TransformCategory::math:         return "Math";
        case TransformCategory::string_op:    return "String";
        case TransformCategory::date:         return "Date";
        case TransformCategory::custom_xform: return "Custom";
    }
    return "?";
}

// ============================================================================
// Structs — field spec for schema definition
// ============================================================================
struct FieldValidation {
    std::optional<double> min;
    std::optional<double> max;
    std::optional<std::string> pattern;
};

struct FieldSpec {
    std::string name;
    FieldType type = FieldType::string_type;
    bool required = false;
    std::string description;
    std::vector<std::string> enum_values;
    std::optional<std::string> default_value;
    FieldValidation validation;
};

struct UnifiedSchema {
    std::string name;
    SchemaCategory category = SchemaCategory::market_data;
    std::string description;
    std::vector<FieldSpec> fields;
};

// ============================================================================
// API Authentication config
// ============================================================================
struct ApiKeyConfig {
    ApiKeyLocation location = ApiKeyLocation::header;
    std::string key_name;
    std::string value;
};

struct BearerConfig {
    std::string token;
};

struct BasicAuthConfig {
    std::string username;
    std::string password;
};

struct OAuth2Config {
    std::string access_token;
    std::optional<std::string> refresh_token;
};

struct AuthConfig {
    AuthType type = AuthType::none;
    std::optional<ApiKeyConfig>   apikey;
    std::optional<BearerConfig>   bearer;
    std::optional<BasicAuthConfig> basic;
    std::optional<OAuth2Config>   oauth2;
};

// ============================================================================
// API Configuration
// ============================================================================
struct ApiConfig {
    std::string id;
    std::string name;
    std::string description;
    std::string base_url;
    std::string endpoint;
    HttpMethod method = HttpMethod::get;
    AuthConfig authentication;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::optional<std::string> body;
    int timeout_ms = 30000;
    bool cache_enabled = true;
    int cache_ttl_seconds = 300;
};

// ============================================================================
// Field Mapping — maps source data field to target schema field
// ============================================================================
enum class FieldSourceType { path, static_value, expression };

struct FieldMapping {
    std::string target_field;
    FieldSourceType source_type = FieldSourceType::path;
    std::string source_path;
    std::string static_value;
    ParserEngine parser = ParserEngine::direct;
    std::vector<std::string> transforms;
    std::optional<std::string> default_value;
    bool required = false;
    float confidence = 0.0f;
};

// ============================================================================
// Extraction config — how to locate the data array in the response
// ============================================================================
struct ExtractionConfig {
    ParserEngine engine = ParserEngine::direct;
    std::string root_path;
    bool is_array = true;
    bool is_array_of_arrays = false;
};

// ============================================================================
// Post-processing config
// ============================================================================
struct SortConfig {
    std::string field;
    bool descending = false;
};

struct PostProcessing {
    std::optional<std::string> filter;
    std::optional<SortConfig> sort;
    std::optional<int> limit;
    std::optional<std::string> deduplicate_field;
};

// ============================================================================
// Validation config
// ============================================================================
struct ValidationConfig {
    bool enabled = true;
    bool strict_mode = false;
};

// ============================================================================
// Full Mapping Configuration — persisted to SQLite
// ============================================================================
struct MappingConfig {
    std::string id;
    std::string name;
    std::string description;

    // Source
    bool is_api_source = true;
    ApiConfig api_config;
    std::string sample_data_json;

    // Target
    bool is_predefined_schema = true;
    SchemaType schema_type = SchemaType::ohlcv;
    std::vector<FieldSpec> custom_fields;

    // Extraction
    ExtractionConfig extraction;

    // Field mappings
    std::vector<FieldMapping> field_mappings;

    // Post-processing
    PostProcessing post_processing;

    // Validation
    ValidationConfig validation;

    // Metadata
    std::string created_at;
    std::string updated_at;
    int version = 1;
    std::string tags;   // comma-separated
    bool ai_generated = false;
};

// ============================================================================
// Execution result
// ============================================================================
struct ValidationError {
    std::string field;
    std::string message;
    std::string rule;
};

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;
};

struct ExecutionResult {
    bool success = false;
    std::string data_json;        // Transformed data as JSON
    std::string raw_data_json;    // Raw API response
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    int64_t duration_ms = 0;
    int records_processed = 0;
    int records_returned = 0;
    ValidationResult validation;
};

// ============================================================================
// API Response
// ============================================================================
enum class ApiErrorType { none, network, auth, client, server, timeout, parse };

inline const char* api_error_type_label(ApiErrorType t) {
    switch (t) {
        case ApiErrorType::none:    return "None";
        case ApiErrorType::network: return "Network";
        case ApiErrorType::auth:    return "Auth";
        case ApiErrorType::client:  return "Client";
        case ApiErrorType::server:  return "Server";
        case ApiErrorType::timeout: return "Timeout";
        case ApiErrorType::parse:   return "Parse";
    }
    return "?";
}

struct ApiResponse {
    bool success = false;
    std::string data_json;
    ApiErrorType error_type = ApiErrorType::none;
    int status_code = 0;
    std::string error_message;
    int64_t duration_ms = 0;
    std::string url;
    std::string method;
};

// ============================================================================
// Template — pre-built mapping configuration for known APIs/brokers
// ============================================================================
struct MappingTemplate {
    std::string id;
    std::string name;
    std::string description;
    std::string category;          // "broker", "data-provider", "exchange", "custom"
    std::vector<std::string> tags;
    bool verified = false;
    bool official = false;
    MappingConfig config;          // Pre-configured mapping
    std::string instructions;
};

// ============================================================================
// Transform definition — describes a transform for the UI
// ============================================================================
struct TransformDef {
    const char* name;
    const char* description;
    TransformCategory category;
};

// ============================================================================
// UI State
// ============================================================================
enum class MappingView { list, create, templates };

enum class CreateStep {
    api_config, schema_select, field_mapping, cache_settings, test_save
};

inline const char* create_step_label(CreateStep s) {
    switch (s) {
        case CreateStep::api_config:     return "API Config";
        case CreateStep::schema_select:  return "Schema";
        case CreateStep::field_mapping:  return "Field Mapping";
        case CreateStep::cache_settings: return "Cache";
        case CreateStep::test_save:      return "Test & Save";
    }
    return "?";
}

inline const char* create_step_short(CreateStep s) {
    switch (s) {
        case CreateStep::api_config:     return "API";
        case CreateStep::schema_select:  return "SCHEMA";
        case CreateStep::field_mapping:  return "MAPPING";
        case CreateStep::cache_settings: return "CACHE";
        case CreateStep::test_save:      return "TEST";
    }
    return "?";
}

} // namespace fincept::data_mapping
