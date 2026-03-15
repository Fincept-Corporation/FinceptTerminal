// Data Mapping Database — SQLite persistence implementation

#include "data_mapping_db.h"
#include "storage/database.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace fincept::data_mapping {
namespace mapping_ops {

using json = nlohmann::json;

// ============================================================================
// Serialization helpers — MappingConfig <-> JSON
// ============================================================================

static json serialize_api_config(const ApiConfig& c) {
    json j;
    j["id"] = c.id;
    j["name"] = c.name;
    j["description"] = c.description;
    j["base_url"] = c.base_url;
    j["endpoint"] = c.endpoint;
    j["method"] = http_method_label(c.method);
    j["timeout_ms"] = c.timeout_ms;
    j["cache_enabled"] = c.cache_enabled;
    j["cache_ttl_seconds"] = c.cache_ttl_seconds;

    // Auth
    json auth;
    auth["type"] = auth_type_id(c.authentication.type);
    if (c.authentication.apikey) {
        auth["apikey_location"] = (c.authentication.apikey->location == ApiKeyLocation::header)
                                   ? "header" : "query";
        auth["apikey_name"] = c.authentication.apikey->key_name;
        auth["apikey_value"] = c.authentication.apikey->value;
    }
    if (c.authentication.bearer) {
        auth["bearer_token"] = c.authentication.bearer->token;
    }
    if (c.authentication.basic) {
        auth["basic_user"] = c.authentication.basic->username;
        auth["basic_pass"] = c.authentication.basic->password;
    }
    if (c.authentication.oauth2) {
        auth["oauth2_token"] = c.authentication.oauth2->access_token;
        if (c.authentication.oauth2->refresh_token) {
            auth["oauth2_refresh"] = *c.authentication.oauth2->refresh_token;
        }
    }
    j["authentication"] = auth;

    // Headers
    j["headers"] = json(c.headers);
    j["query_params"] = json(c.query_params);
    if (c.body) j["body"] = *c.body;
    return j;
}

static ApiConfig deserialize_api_config(const json& j) {
    ApiConfig c;
    c.id = j.value("id", "");
    c.name = j.value("name", "");
    c.description = j.value("description", "");
    c.base_url = j.value("base_url", "");
    c.endpoint = j.value("endpoint", "");
    c.method = http_method_from_string(j.value("method", "GET"));
    c.timeout_ms = j.value("timeout_ms", 30000);
    c.cache_enabled = j.value("cache_enabled", true);
    c.cache_ttl_seconds = j.value("cache_ttl_seconds", 300);

    if (j.contains("authentication")) {
        const auto& auth = j["authentication"];
        c.authentication.type = auth_type_from_string(auth.value("type", "none"));
        if (auth.contains("apikey_name")) {
            ApiKeyConfig ak;
            ak.location = (auth.value("apikey_location", "header") == "query")
                           ? ApiKeyLocation::query : ApiKeyLocation::header;
            ak.key_name = auth.value("apikey_name", "");
            ak.value = auth.value("apikey_value", "");
            c.authentication.apikey = ak;
        }
        if (auth.contains("bearer_token")) {
            c.authentication.bearer = BearerConfig{auth.value("bearer_token", "")};
        }
        if (auth.contains("basic_user")) {
            c.authentication.basic = BasicAuthConfig{
                auth.value("basic_user", ""), auth.value("basic_pass", "")};
        }
        if (auth.contains("oauth2_token")) {
            OAuth2Config oa;
            oa.access_token = auth.value("oauth2_token", "");
            if (auth.contains("oauth2_refresh")) {
                oa.refresh_token = auth.value("oauth2_refresh", "");
            }
            c.authentication.oauth2 = oa;
        }
    }

    if (j.contains("headers") && j["headers"].is_object()) {
        for (auto& [k, v] : j["headers"].items()) {
            c.headers[k] = v.get<std::string>();
        }
    }
    if (j.contains("query_params") && j["query_params"].is_object()) {
        for (auto& [k, v] : j["query_params"].items()) {
            c.query_params[k] = v.get<std::string>();
        }
    }
    if (j.contains("body")) c.body = j["body"].get<std::string>();
    return c;
}

static json serialize_field_mapping(const FieldMapping& fm) {
    json j;
    j["target_field"] = fm.target_field;
    j["source_type"] = static_cast<int>(fm.source_type);
    j["source_path"] = fm.source_path;
    j["static_value"] = fm.static_value;
    j["parser"] = parser_engine_id(fm.parser);
    j["transforms"] = fm.transforms;
    if (fm.default_value) j["default_value"] = *fm.default_value;
    j["required"] = fm.required;
    j["confidence"] = fm.confidence;
    return j;
}

static FieldMapping deserialize_field_mapping(const json& j) {
    FieldMapping fm;
    fm.target_field = j.value("target_field", "");
    fm.source_type = static_cast<FieldSourceType>(j.value("source_type", 0));
    fm.source_path = j.value("source_path", "");
    fm.static_value = j.value("static_value", "");
    fm.parser = parser_engine_from_string(j.value("parser", "direct"));
    if (j.contains("transforms") && j["transforms"].is_array()) {
        for (const auto& t : j["transforms"]) {
            fm.transforms.push_back(t.get<std::string>());
        }
    }
    if (j.contains("default_value")) fm.default_value = j["default_value"].get<std::string>();
    fm.required = j.value("required", false);
    fm.confidence = j.value("confidence", 0.0f);
    return fm;
}

static json serialize_custom_field(const FieldSpec& f) {
    json j;
    j["name"] = f.name;
    j["type"] = field_type_label(f.type);
    j["required"] = f.required;
    j["description"] = f.description;
    j["enum_values"] = f.enum_values;
    if (f.default_value) j["default_value"] = *f.default_value;
    if (f.validation.min) j["min"] = *f.validation.min;
    if (f.validation.max) j["max"] = *f.validation.max;
    if (f.validation.pattern) j["pattern"] = *f.validation.pattern;
    return j;
}

static FieldSpec deserialize_custom_field(const json& j) {
    FieldSpec f;
    f.name = j.value("name", "");
    f.type = field_type_from_string(j.value("type", "string"));
    f.required = j.value("required", false);
    f.description = j.value("description", "");
    if (j.contains("enum_values") && j["enum_values"].is_array()) {
        for (const auto& v : j["enum_values"]) {
            f.enum_values.push_back(v.get<std::string>());
        }
    }
    if (j.contains("default_value")) f.default_value = j["default_value"].get<std::string>();
    if (j.contains("min")) f.validation.min = j["min"].get<double>();
    if (j.contains("max")) f.validation.max = j["max"].get<double>();
    if (j.contains("pattern")) f.validation.pattern = j["pattern"].get<std::string>();
    return f;
}

static json serialize_mapping(const MappingConfig& m) {
    json j;
    j["id"] = m.id;
    j["name"] = m.name;
    j["description"] = m.description;
    j["is_api_source"] = m.is_api_source;
    j["api_config"] = serialize_api_config(m.api_config);
    j["sample_data_json"] = m.sample_data_json;
    j["is_predefined_schema"] = m.is_predefined_schema;
    j["schema_type"] = schema_type_id(m.schema_type);

    json custom_fields_arr = json::array();
    for (const auto& f : m.custom_fields) {
        custom_fields_arr.push_back(serialize_custom_field(f));
    }
    j["custom_fields"] = custom_fields_arr;

    j["extraction"] = json{
        {"engine", parser_engine_id(m.extraction.engine)},
        {"root_path", m.extraction.root_path},
        {"is_array", m.extraction.is_array},
        {"is_array_of_arrays", m.extraction.is_array_of_arrays},
    };

    json mappings_arr = json::array();
    for (const auto& fm : m.field_mappings) {
        mappings_arr.push_back(serialize_field_mapping(fm));
    }
    j["field_mappings"] = mappings_arr;

    // Post-processing
    json pp;
    if (m.post_processing.filter) pp["filter"] = *m.post_processing.filter;
    if (m.post_processing.sort) {
        pp["sort_field"] = m.post_processing.sort->field;
        pp["sort_desc"] = m.post_processing.sort->descending;
    }
    if (m.post_processing.limit) pp["limit"] = *m.post_processing.limit;
    if (m.post_processing.deduplicate_field) pp["deduplicate"] = *m.post_processing.deduplicate_field;
    j["post_processing"] = pp;

    j["validation_enabled"] = m.validation.enabled;
    j["validation_strict"] = m.validation.strict_mode;
    j["version"] = m.version;
    j["tags"] = m.tags;
    j["ai_generated"] = m.ai_generated;
    j["created_at"] = m.created_at;
    j["updated_at"] = m.updated_at;
    return j;
}

static MappingConfig deserialize_mapping(const json& j) {
    MappingConfig m;
    m.id = j.value("id", "");
    m.name = j.value("name", "");
    m.description = j.value("description", "");
    m.is_api_source = j.value("is_api_source", true);
    if (j.contains("api_config")) m.api_config = deserialize_api_config(j["api_config"]);
    m.sample_data_json = j.value("sample_data_json", "");
    m.is_predefined_schema = j.value("is_predefined_schema", true);
    m.schema_type = schema_type_from_string(j.value("schema_type", "OHLCV"));

    if (j.contains("custom_fields") && j["custom_fields"].is_array()) {
        for (const auto& f : j["custom_fields"]) {
            m.custom_fields.push_back(deserialize_custom_field(f));
        }
    }

    if (j.contains("extraction")) {
        const auto& e = j["extraction"];
        m.extraction.engine = parser_engine_from_string(e.value("engine", "direct"));
        m.extraction.root_path = e.value("root_path", "");
        m.extraction.is_array = e.value("is_array", true);
        m.extraction.is_array_of_arrays = e.value("is_array_of_arrays", false);
    }

    if (j.contains("field_mappings") && j["field_mappings"].is_array()) {
        for (const auto& fm : j["field_mappings"]) {
            m.field_mappings.push_back(deserialize_field_mapping(fm));
        }
    }

    if (j.contains("post_processing")) {
        const auto& pp = j["post_processing"];
        if (pp.contains("filter")) m.post_processing.filter = pp["filter"].get<std::string>();
        if (pp.contains("sort_field")) {
            m.post_processing.sort = SortConfig{
                pp["sort_field"].get<std::string>(),
                pp.value("sort_desc", false)
            };
        }
        if (pp.contains("limit")) m.post_processing.limit = pp["limit"].get<int>();
        if (pp.contains("deduplicate")) m.post_processing.deduplicate_field = pp["deduplicate"].get<std::string>();
    }

    m.validation.enabled = j.value("validation_enabled", true);
    m.validation.strict_mode = j.value("validation_strict", false);
    m.version = j.value("version", 1);
    m.tags = j.value("tags", "");
    m.ai_generated = j.value("ai_generated", false);
    m.created_at = j.value("created_at", "");
    m.updated_at = j.value("updated_at", "");
    return m;
}

// ============================================================================
// Table initialization
// ============================================================================

void initialize_tables() {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    database.exec(R"(
        CREATE TABLE IF NOT EXISTS data_mappings (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            config_json TEXT NOT NULL,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        )
    )");

    database.exec(R"(
        CREATE TABLE IF NOT EXISTS mapping_cache (
            cache_key TEXT PRIMARY KEY,
            mapping_id TEXT NOT NULL,
            params_hash TEXT NOT NULL,
            response_json TEXT NOT NULL,
            cached_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        )
    )");

    database.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_mapping_cache_lookup
        ON mapping_cache(mapping_id, params_hash)
    )");

    database.exec(R"(
        CREATE TABLE IF NOT EXISTS mapping_preferences (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )");

    LOG_INFO("DataMapping", "Data mapping tables initialized");
}

// ============================================================================
// Mapping CRUD
// ============================================================================

void save_mapping(const MappingConfig& config) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    const json config_json = serialize_mapping(config);
    const std::string json_str = config_json.dump();

    auto stmt = database.prepare(
        "INSERT OR REPLACE INTO data_mappings (id, name, description, config_json, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    stmt.bind_text(1, config.id);
    stmt.bind_text(2, config.name);
    stmt.bind_text(3, config.description);
    stmt.bind_text(4, json_str);
    stmt.bind_text(5, config.created_at);
    stmt.bind_text(6, config.updated_at);
    stmt.execute();
}

std::vector<MappingConfig> get_all_mappings() {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    std::vector<MappingConfig> result;
    auto stmt = database.prepare(
        "SELECT config_json FROM data_mappings ORDER BY updated_at DESC");

    while (stmt.step()) {
        try {
            const auto j = json::parse(stmt.col_text(0));
            result.push_back(deserialize_mapping(j));
        } catch (const std::exception& e) {
            LOG_WARN("DataMapping", "Failed to parse mapping: %s", e.what());
        }
    }
    return result;
}

std::optional<MappingConfig> get_mapping(const std::string& id) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    auto stmt = database.prepare(
        "SELECT config_json FROM data_mappings WHERE id = ?");
    stmt.bind_text(1, id);

    if (stmt.step()) {
        try {
            const auto j = json::parse(stmt.col_text(0));
            return deserialize_mapping(j);
        } catch (...) {}
    }
    return std::nullopt;
}

void delete_mapping(const std::string& id) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    auto stmt = database.prepare("DELETE FROM data_mappings WHERE id = ?");
    stmt.bind_text(1, id);
    stmt.execute();

    // Also clear associated cache
    clear_cache(id);
}

MappingConfig duplicate_mapping(const std::string& id) {
    const auto original = get_mapping(id);
    if (!original) return {};

    MappingConfig copy = *original;
    copy.id = db::generate_uuid();
    copy.name = original->name + " (Copy)";

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    copy.created_at = buf;
    copy.updated_at = buf;

    save_mapping(copy);
    return copy;
}

// ============================================================================
// Cache
// ============================================================================

std::optional<std::string> get_cached_response(const std::string& mapping_id,
                                                const std::string& params_hash) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    const std::string cache_key = mapping_id + "_" + params_hash;
    const auto now_ts = db::now_timestamp();

    auto stmt = database.prepare(
        "SELECT response_json FROM mapping_cache WHERE cache_key = ? AND expires_at > ?");
    stmt.bind_text(1, cache_key);
    stmt.bind_int(2, now_ts);

    if (stmt.step()) {
        return stmt.col_text(0);
    }
    return std::nullopt;
}

void set_cached_response(const std::string& mapping_id,
                          const std::string& params_hash,
                          const std::string& response_json,
                          int ttl_seconds) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    const std::string cache_key = mapping_id + "_" + params_hash;
    const auto now_ts = db::now_timestamp();
    const auto expires_ts = now_ts + static_cast<int64_t>(ttl_seconds);

    auto stmt = database.prepare(
        "INSERT OR REPLACE INTO mapping_cache "
        "(cache_key, mapping_id, params_hash, response_json, cached_at, expires_at) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    stmt.bind_text(1, cache_key);
    stmt.bind_text(2, mapping_id);
    stmt.bind_text(3, params_hash);
    stmt.bind_text(4, response_json);
    stmt.bind_int(5, now_ts);
    stmt.bind_int(6, expires_ts);
    stmt.execute();
}

void clear_cache(const std::string& mapping_id) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    if (mapping_id.empty()) {
        database.exec("DELETE FROM mapping_cache");
    } else {
        auto stmt = database.prepare("DELETE FROM mapping_cache WHERE mapping_id = ?");
        stmt.bind_text(1, mapping_id);
        stmt.execute();
    }
}

void clean_expired_cache() {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    const auto now_ts = db::now_timestamp();
    auto stmt = database.prepare("DELETE FROM mapping_cache WHERE expires_at < ?");
    stmt.bind_int(1, now_ts);
    stmt.execute();
}

// ============================================================================
// Preferences
// ============================================================================

std::optional<std::string> get_preference(const std::string& key) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    auto stmt = database.prepare("SELECT value FROM mapping_preferences WHERE key = ?");
    stmt.bind_text(1, key);
    if (stmt.step()) {
        return stmt.col_text(0);
    }
    return std::nullopt;
}

void set_preference(const std::string& key, const std::string& value) {
    auto& database = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(database.mutex());

    auto stmt = database.prepare(
        "INSERT OR REPLACE INTO mapping_preferences (key, value) VALUES (?, ?)");
    stmt.bind_text(1, key);
    stmt.bind_text(2, value);
    stmt.execute();
}

} // namespace mapping_ops
} // namespace fincept::data_mapping
