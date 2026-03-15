// Data Mapping Engine — implementation of transform pipeline, JSON extraction,
// URL building, API execution, validation, and post-processing.

#include "data_mapping_engine.h"
#include "http/http_client.h"
#include "storage/database.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <regex>
#include <sstream>
#include <cctype>
#include <set>

namespace fincept::data_mapping {

using json = nlohmann::json;

// ============================================================================
// Transform Registry
// ============================================================================

json TransformRegistry::apply(const std::string& name, const json& value,
                               const json& params) {
    // --- Type conversions ---
    if (name == "toNumber") {
        if (value.is_number()) return value;
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            // Remove commas
            s.erase(std::remove(s.begin(), s.end(), ','), s.end());
            try {
                return json(std::stod(s));
            } catch (...) {
                return json(nullptr);
            }
        }
        return json(nullptr);
    }
    if (name == "toString") {
        if (value.is_string()) return value;
        if (value.is_number_integer()) return json(std::to_string(value.get<int64_t>()));
        if (value.is_number_float()) return json(std::to_string(value.get<double>()));
        if (value.is_boolean()) return json(value.get<bool>() ? "true" : "false");
        return json(value.dump());
    }
    if (name == "toBoolean") {
        if (value.is_boolean()) return value;
        if (value.is_number()) return json(value.get<double>() != 0.0);
        if (value.is_string()) return json(!value.get<std::string>().empty());
        return json(false);
    }

    // --- Date conversions ---
    if (name == "fromUnixTimestamp") {
        if (value.is_number()) {
            const auto ts = value.get<int64_t>();
            // Convert Unix seconds to ISO string (simplified)
            const auto tp = std::chrono::system_clock::from_time_t(static_cast<time_t>(ts));
            const auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm tm_buf{};
#ifdef _WIN32
            gmtime_s(&tm_buf, &tt);
#else
            gmtime_r(&tt, &tm_buf);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &tm_buf);
            return json(std::string(buf));
        }
        return json(nullptr);
    }
    if (name == "fromUnixTimestampMs") {
        if (value.is_number()) {
            const auto ts_ms = value.get<int64_t>();
            const auto ts = ts_ms / 1000;
            const auto tp = std::chrono::system_clock::from_time_t(static_cast<time_t>(ts));
            const auto tt = std::chrono::system_clock::to_time_t(tp);
            std::tm tm_buf{};
#ifdef _WIN32
            gmtime_s(&tm_buf, &tt);
#else
            gmtime_r(&tt, &tm_buf);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", &tm_buf);
            return json(std::string(buf));
        }
        return json(nullptr);
    }
    if (name == "toISODate") {
        // Pass-through strings, convert timestamps
        if (value.is_string()) return value;
        if (value.is_number()) return apply("fromUnixTimestampMs", value, params);
        return json(nullptr);
    }

    // --- Math operations ---
    if (name == "round") {
        if (value.is_number()) {
            const int decimals = params.contains("decimals") ? params["decimals"].get<int>() : 2;
            const double multiplier = std::pow(10.0, decimals);
            return json(std::round(value.get<double>() * multiplier) / multiplier);
        }
        return value;
    }
    if (name == "multiply") {
        if (value.is_number() && params.contains("multiplier")) {
            return json(value.get<double>() * params["multiplier"].get<double>());
        }
        return value;
    }
    if (name == "divide") {
        if (value.is_number() && params.contains("divisor")) {
            const double divisor = params["divisor"].get<double>();
            if (divisor != 0.0) return json(value.get<double>() / divisor);
        }
        return value;
    }
    if (name == "add") {
        if (value.is_number() && params.contains("addend")) {
            return json(value.get<double>() + params["addend"].get<double>());
        }
        return value;
    }
    if (name == "subtract") {
        if (value.is_number() && params.contains("subtrahend")) {
            return json(value.get<double>() - params["subtrahend"].get<double>());
        }
        return value;
    }

    // --- String operations ---
    if (name == "uppercase") {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(),
                          [](unsigned char c) { return std::toupper(c); });
            return json(s);
        }
        return value;
    }
    if (name == "lowercase") {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            return json(s);
        }
        return value;
    }
    if (name == "trim") {
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            const auto start = s.find_first_not_of(" \t\n\r");
            if (start == std::string::npos) return json("");
            const auto end = s.find_last_not_of(" \t\n\r");
            return json(s.substr(start, end - start + 1));
        }
        return value;
    }
    if (name == "replace") {
        if (value.is_string() && params.contains("find") && params.contains("replace")) {
            std::string s = value.get<std::string>();
            const std::string find_str = params["find"].get<std::string>();
            const std::string replace_str = params["replace"].get<std::string>();
            size_t pos = 0;
            while ((pos = s.find(find_str, pos)) != std::string::npos) {
                s.replace(pos, find_str.length(), replace_str);
                pos += replace_str.length();
            }
            return json(s);
        }
        return value;
    }
    if (name == "substring") {
        if (value.is_string()) {
            const std::string s = value.get<std::string>();
            const auto start = static_cast<size_t>(
                params.contains("start") ? params["start"].get<int>() : 0);
            if (params.contains("end")) {
                const auto end = static_cast<size_t>(params["end"].get<int>());
                return json(s.substr(start, end - start));
            }
            return json(s.substr(start));
        }
        return value;
    }

    // --- Financial operations ---
    if (name == "bpsToPercent") {
        if (value.is_number()) return json(value.get<double>() / 100.0);
        return value;
    }
    if (name == "percentToBps") {
        if (value.is_number()) return json(value.get<double>() * 100.0);
        return value;
    }
    if (name == "roundPrice") {
        if (value.is_number()) {
            return json(std::round(value.get<double>() * 100.0) / 100.0);
        }
        return value;
    }

    LOG_WARN("DataMapping", "Unknown transform: %s", name.c_str());
    return value;
}

std::vector<TransformDef> TransformRegistry::all_transforms() {
    return {
        {"toNumber",            "Convert value to number",                 TransformCategory::type_conv},
        {"toString",            "Convert value to string",                 TransformCategory::type_conv},
        {"toBoolean",           "Convert value to boolean",                TransformCategory::type_conv},
        {"toISODate",           "Convert to ISO 8601 date string",         TransformCategory::date},
        {"fromUnixTimestamp",   "Unix timestamp (seconds) to ISO date",    TransformCategory::date},
        {"fromUnixTimestampMs", "Unix timestamp (ms) to ISO date",         TransformCategory::date},
        {"round",               "Round to N decimal places",               TransformCategory::math},
        {"multiply",            "Multiply by a constant",                  TransformCategory::math},
        {"divide",              "Divide by a constant",                    TransformCategory::math},
        {"add",                 "Add a constant",                          TransformCategory::math},
        {"subtract",            "Subtract a constant",                     TransformCategory::math},
        {"uppercase",           "Convert string to uppercase",             TransformCategory::string_op},
        {"lowercase",           "Convert string to lowercase",             TransformCategory::string_op},
        {"trim",                "Remove leading/trailing whitespace",      TransformCategory::string_op},
        {"replace",             "Replace text in string",                  TransformCategory::string_op},
        {"substring",           "Extract substring",                       TransformCategory::string_op},
        {"bpsToPercent",        "Basis points to percentage",              TransformCategory::math},
        {"percentToBps",        "Percentage to basis points",              TransformCategory::math},
        {"roundPrice",          "Round price to 2 decimals",               TransformCategory::format},
    };
}

// ============================================================================
// Direct Parser — dot-path based JSON extraction
// ============================================================================

json DirectParser::extract(const json& data, const std::string& path) {
    if (path.empty() || data.is_null()) return data;

    json current = data;
    std::istringstream stream(path);
    std::string token;

    while (std::getline(stream, token, '.')) {
        if (token.empty()) continue;

        // Check for array index notation: field[0]
        const auto bracket_pos = token.find('[');
        if (bracket_pos != std::string::npos) {
            const std::string field_name = token.substr(0, bracket_pos);
            if (!field_name.empty()) {
                if (!current.contains(field_name)) return json(nullptr);
                current = current[field_name];
            }

            // Extract array index
            const auto close_pos = token.find(']', bracket_pos);
            if (close_pos != std::string::npos) {
                const std::string idx_str = token.substr(bracket_pos + 1,
                                                         close_pos - bracket_pos - 1);
                if (idx_str == "*") {
                    continue; // wildcard — keep as array
                }
                try {
                    const auto idx = static_cast<size_t>(std::stoi(idx_str));
                    if (current.is_array() && idx < current.size()) {
                        current = current[idx];
                    } else {
                        return json(nullptr);
                    }
                } catch (...) {
                    return json(nullptr);
                }
            }
        } else {
            // Simple field access
            if (current.is_object() && current.contains(token)) {
                current = current[token];
            } else if (current.is_array()) {
                // Apply field access to each element
                json result = json::array();
                for (const auto& item : current) {
                    if (item.is_object() && item.contains(token)) {
                        result.push_back(item[token]);
                    }
                }
                current = result;
            } else {
                return json(nullptr);
            }
        }
    }
    return current;
}

// ============================================================================
// URL Builder
// ============================================================================

std::string UrlBuilder::replace_placeholders(const std::string& text,
                                              const std::map<std::string, std::string>& params) {
    std::string result = text;
    for (const auto& [key, val] : params) {
        const std::string placeholder = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), val);
            pos += val.length();
        }
    }
    return result;
}

std::string UrlBuilder::build_url(const ApiConfig& config,
                                   const std::map<std::string, std::string>& params) {
    std::string url = config.base_url;
    // Remove trailing slash
    if (!url.empty() && url.back() == '/') url.pop_back();

    // Add endpoint with placeholder substitution
    std::string endpoint = config.endpoint;
    if (!endpoint.empty() && endpoint.front() != '/') endpoint = "/" + endpoint;
    endpoint = replace_placeholders(endpoint, params);
    url += endpoint;

    // Build query string
    std::string query;
    for (const auto& [key, val] : config.query_params) {
        const std::string processed = replace_placeholders(val, params);
        if (!query.empty()) query += "&";
        query += key + "=" + processed;
    }

    // Add API key to query if configured
    if (config.authentication.type == AuthType::apikey &&
        config.authentication.apikey &&
        config.authentication.apikey->location == ApiKeyLocation::query) {
        if (!query.empty()) query += "&";
        query += config.authentication.apikey->key_name + "=" +
                 config.authentication.apikey->value;
    }

    if (!query.empty()) url += "?" + query;
    return url;
}

std::map<std::string, std::string> UrlBuilder::build_headers(const ApiConfig& config) {
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
    };

    // Merge user headers
    for (const auto& [k, v] : config.headers) {
        headers[k] = v;
    }

    // Add auth headers
    const auto& auth = config.authentication;
    if (auth.type == AuthType::apikey && auth.apikey &&
        auth.apikey->location == ApiKeyLocation::header) {
        headers[auth.apikey->key_name] = auth.apikey->value;
    } else if (auth.type == AuthType::bearer && auth.bearer) {
        headers["Authorization"] = "Bearer " + auth.bearer->token;
    } else if (auth.type == AuthType::basic && auth.basic) {
        // Simple base64 placeholder — in production use proper base64 encoding
        headers["Authorization"] = "Basic " + auth.basic->username + ":" + auth.basic->password;
    } else if (auth.type == AuthType::oauth2 && auth.oauth2) {
        headers["Authorization"] = "Bearer " + auth.oauth2->access_token;
    }

    return headers;
}

std::vector<std::string> UrlBuilder::extract_placeholders(const std::string& text) {
    std::vector<std::string> result;
    std::regex re(R"(\{([^}]+)\})");
    std::sregex_iterator it(text.begin(), text.end(), re);
    const std::sregex_iterator end;
    for (; it != end; ++it) {
        result.push_back((*it)[1].str());
    }
    return result;
}

bool UrlBuilder::is_valid_url(const std::string& url) {
    return url.size() > 8 &&
           (url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://");
}

// ============================================================================
// Mapping Engine — execute full pipeline
// ============================================================================

ApiResponse MappingEngine::fetch_api(const ApiConfig& config,
                                      const std::map<std::string, std::string>& params) {
    ApiResponse result;
    const auto start = std::chrono::steady_clock::now();

    try {
        const std::string url = UrlBuilder::build_url(config, params);
        const auto headers = UrlBuilder::build_headers(config);

        result.url = url;
        result.method = http_method_label(config.method);

        // Convert to http::Headers
        http::Headers http_headers;
        for (const auto& [k, v] : headers) {
            http_headers[k] = v;
        }

        const std::string body = config.body.value_or("");
        auto& client = http::HttpClient::instance();

        http::HttpResponse response;
        const auto method_str = http_method_label(config.method);
        if (method_str == "GET")        response = client.get(url, http_headers);
        else if (method_str == "POST")  response = client.post(url, body, http_headers);
        else if (method_str == "PUT")   response = client.put(url, body, http_headers);
        else if (method_str == "DELETE")response = client.del(url, body, http_headers);
        else                            response = client.get(url, http_headers);

        const auto end = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        result.status_code = response.status_code;

        if (response.status_code >= 200 && response.status_code < 300) {
            result.success = true;
            result.data_json = response.body;
        } else if (response.status_code == 401 || response.status_code == 403) {
            result.error_type = ApiErrorType::auth;
            result.error_message = "HTTP " + std::to_string(response.status_code) +
                                   ": " + response.body.substr(0, 200);
        } else if (response.status_code >= 400 && response.status_code < 500) {
            result.error_type = ApiErrorType::client;
            result.error_message = "HTTP " + std::to_string(response.status_code) +
                                   ": " + response.body.substr(0, 200);
        } else {
            result.error_type = ApiErrorType::server;
            result.error_message = "HTTP " + std::to_string(response.status_code) +
                                   ": " + response.body.substr(0, 200);
        }
    } catch (const std::exception& e) {
        const auto end = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        result.error_type = ApiErrorType::network;
        result.error_message = e.what();
    }

    return result;
}

json MappingEngine::extract_data(const json& raw_data, const MappingConfig& mapping) {
    json data = raw_data;

    // Apply root path extraction
    if (!mapping.extraction.root_path.empty()) {
        data = DirectParser::extract(raw_data, mapping.extraction.root_path);
    }

    // Ensure array if expected
    if (mapping.extraction.is_array && !data.is_array()) {
        json arr = json::array();
        arr.push_back(data);
        data = arr;
    }

    return data;
}

json MappingEngine::extract_field_value(const json& source, const FieldMapping& mapping) {
    if (mapping.source_type == FieldSourceType::static_value) {
        try {
            return json::parse(mapping.static_value);
        } catch (...) {
            return json(mapping.static_value);
        }
    }

    if (mapping.source_path.empty()) {
        if (mapping.default_value) {
            try {
                return json::parse(*mapping.default_value);
            } catch (...) {
                return json(*mapping.default_value);
            }
        }
        return json(nullptr);
    }

    // Use direct parser for extraction
    json result = DirectParser::extract(source, mapping.source_path);

    // Handle array-of-arrays indexing (e.g., "$[0]" for first element)
    if (result.is_null() && mapping.source_path.size() >= 2 &&
        mapping.source_path[0] == '$' && mapping.source_path[1] == '[') {
        // Try as array index on source itself
        try {
            const auto idx_str = mapping.source_path.substr(2,
                mapping.source_path.find(']') - 2);
            const auto idx = static_cast<size_t>(std::stoi(idx_str));
            if (source.is_array() && idx < source.size()) {
                result = source[idx];
            }
        } catch (...) {}
    }

    return result;
}

json MappingEngine::transform_single(const json& source, const MappingConfig& mapping) {
    json result = json::object();

    for (const auto& fm : mapping.field_mappings) {
        try {
            json value = extract_field_value(source, fm);

            // Apply transforms
            for (const auto& transform_name : fm.transforms) {
                if (!value.is_null()) {
                    value = TransformRegistry::apply(transform_name, value);
                }
            }

            // Use default if null
            if (value.is_null() && fm.default_value) {
                try {
                    value = json::parse(*fm.default_value);
                } catch (...) {
                    value = json(*fm.default_value);
                }
            }

            // Required check
            if (value.is_null() && fm.required) {
                LOG_WARN("DataMapping", "Required field %s is missing", fm.target_field.c_str());
            }

            result[fm.target_field] = value;
        } catch (const std::exception& e) {
            LOG_WARN("DataMapping", "Error mapping field %s: %s", fm.target_field.c_str(), e.what());
            if (fm.default_value) {
                result[fm.target_field] = json(*fm.default_value);
            } else {
                result[fm.target_field] = json(nullptr);
            }
        }
    }

    return result;
}

json MappingEngine::apply_post_processing(const json& data, const MappingConfig& mapping) {
    if (!data.is_array()) return data;

    json processed = data;
    const auto& pp = mapping.post_processing;

    // Sort
    if (pp.sort) {
        const std::string sort_field = pp.sort->field;
        const bool desc = pp.sort->descending;
        std::vector<json> items(processed.begin(), processed.end());
        std::sort(items.begin(), items.end(),
            [&](const json& a, const json& b) {
                const auto& va = a.contains(sort_field) ? a[sort_field] : json(nullptr);
                const auto& vb = b.contains(sort_field) ? b[sort_field] : json(nullptr);
                if (va.is_number() && vb.is_number()) {
                    return desc ? va.get<double>() > vb.get<double>()
                                : va.get<double>() < vb.get<double>();
                }
                if (va.is_string() && vb.is_string()) {
                    return desc ? va.get<std::string>() > vb.get<std::string>()
                                : va.get<std::string>() < vb.get<std::string>();
                }
                return false;
            });
        processed = json(items);
    }

    // Deduplicate
    if (pp.deduplicate_field) {
        std::vector<json> unique_items;
        std::set<std::string> seen;
        for (const auto& item : processed) {
            std::string key;
            if (item.contains(*pp.deduplicate_field)) {
                key = item[*pp.deduplicate_field].dump();
            }
            if (seen.find(key) == seen.end()) {
                seen.insert(key);
                unique_items.push_back(item);
            }
        }
        processed = json(unique_items);
    }

    // Limit
    if (pp.limit && *pp.limit > 0 && processed.size() > static_cast<size_t>(*pp.limit)) {
        json limited = json::array();
        for (size_t i = 0; i < static_cast<size_t>(*pp.limit); ++i) {
            limited.push_back(processed[i]);
        }
        processed = limited;
    }

    return processed;
}

ValidationResult MappingEngine::validate_data(const json& data,
                                                const MappingConfig& mapping) {
    ValidationResult result;
    result.valid = true;

    if (!mapping.validation.enabled) return result;

    // Get field specs from schema
    std::vector<FieldSpec> fields;
    if (mapping.is_predefined_schema) {
        const auto* schema = get_schema_for_type(mapping.schema_type);
        if (schema) fields = schema->fields;
    } else {
        fields = mapping.custom_fields;
    }

    if (fields.empty()) return result;

    // Validate each item
    const auto items = data.is_array() ? data : json::array({data});
    for (const auto& item : items) {
        for (const auto& field : fields) {
            const bool has_field = item.contains(field.name) && !item[field.name].is_null();

            // Required check
            if (field.required && !has_field) {
                result.valid = false;
                result.errors.push_back({field.name,
                    "Field '" + field.name + "' is required but missing", "required"});
                continue;
            }

            if (!has_field) continue;

            const auto& value = item[field.name];

            // Type check (basic)
            if (field.type == FieldType::number_type && !value.is_number()) {
                result.valid = false;
                result.errors.push_back({field.name,
                    "Field '" + field.name + "' should be number", "type"});
            }
            if (field.type == FieldType::string_type && !value.is_string()) {
                result.valid = false;
                result.errors.push_back({field.name,
                    "Field '" + field.name + "' should be string", "type"});
            }

            // Range validation
            if (value.is_number() && field.validation.min &&
                value.get<double>() < *field.validation.min) {
                result.valid = false;
                result.errors.push_back({field.name,
                    "Field '" + field.name + "' must be >= " +
                    std::to_string(*field.validation.min), "range"});
            }
            if (value.is_number() && field.validation.max &&
                value.get<double>() > *field.validation.max) {
                result.valid = false;
                result.errors.push_back({field.name,
                    "Field '" + field.name + "' must be <= " +
                    std::to_string(*field.validation.max), "range"});
            }
        }
    }

    return result;
}

ExecutionResult MappingEngine::execute(const MappingConfig& mapping,
                                        const json& sample_data,
                                        const std::map<std::string, std::string>& params) {
    ExecutionResult result;
    const auto start = std::chrono::steady_clock::now();

    try {
        // 1. Get data
        json raw_data;
        if (!sample_data.is_null()) {
            raw_data = sample_data;
        } else if (mapping.is_api_source) {
            const auto api_response = fetch_api(mapping.api_config, params);
            if (!api_response.success) {
                result.errors.push_back("API request failed: " + api_response.error_message);
                const auto end = std::chrono::steady_clock::now();
                result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count();
                return result;
            }
            try {
                raw_data = json::parse(api_response.data_json);
            } catch (const std::exception& e) {
                result.errors.push_back(std::string("Failed to parse API response: ") + e.what());
                const auto end = std::chrono::steady_clock::now();
                result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count();
                return result;
            }
        } else if (!mapping.sample_data_json.empty()) {
            try {
                raw_data = json::parse(mapping.sample_data_json);
            } catch (...) {
                result.errors.push_back("Failed to parse sample data");
                return result;
            }
        } else {
            result.errors.push_back("No data source provided");
            return result;
        }

        result.raw_data_json = raw_data.dump();

        // 2. Extract data
        json extracted = extract_data(raw_data, mapping);

        // 3. Transform
        json transformed;
        if (extracted.is_array()) {
            transformed = json::array();
            for (const auto& item : extracted) {
                transformed.push_back(transform_single(item, mapping));
            }
            result.records_processed = static_cast<int>(extracted.size());
        } else {
            transformed = transform_single(extracted, mapping);
            result.records_processed = 1;
        }

        // 4. Post-processing
        transformed = apply_post_processing(transformed, mapping);

        // 5. Validate
        if (mapping.validation.enabled) {
            result.validation = validate_data(transformed, mapping);
            if (mapping.validation.strict_mode && !result.validation.valid) {
                result.errors.push_back("Validation failed");
                const auto end = std::chrono::steady_clock::now();
                result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count();
                return result;
            }
        }

        result.success = true;
        result.data_json = transformed.dump(2);
        result.records_returned = transformed.is_array()
            ? static_cast<int>(transformed.size()) : 1;

    } catch (const std::exception& e) {
        result.errors.push_back(e.what());
    }

    const auto end = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    return result;
}

ExecutionResult MappingEngine::test(const MappingConfig& mapping, const json& sample_data,
                                     const std::map<std::string, std::string>& params) {
    return execute(mapping, sample_data, params);
}

} // namespace fincept::data_mapping
