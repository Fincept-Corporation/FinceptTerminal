#pragma once
// Data Sources Types — Enums, structs, and field definitions for connectors

#include <string>
#include <vector>

namespace fincept::data_sources {

// ============================================================================
// Categories
// ============================================================================
enum class Category {
    Database,
    Api,
    File,
    Streaming,
    Cloud,
    TimeSeries,
    MarketData,
};

inline const char* category_label(Category c) {
    switch (c) {
        case Category::Database:   return "DATABASE";
        case Category::Api:        return "API";
        case Category::File:       return "FILE";
        case Category::Streaming:  return "STREAMING";
        case Category::Cloud:      return "CLOUD";
        case Category::TimeSeries: return "TIMESERIES";
        case Category::MarketData: return "MARKET DATA";
    }
    return "?";
}

inline const char* category_id(Category c) {
    switch (c) {
        case Category::Database:   return "database";
        case Category::Api:        return "api";
        case Category::File:       return "file";
        case Category::Streaming:  return "streaming";
        case Category::Cloud:      return "cloud";
        case Category::TimeSeries: return "timeseries";
        case Category::MarketData: return "market-data";
    }
    return "?";
}

// ============================================================================
// Field definitions for config forms
// ============================================================================
enum class FieldType { Text, Password, Number, Url, Select, Textarea, Checkbox };

struct SelectOption {
    const char* label;
    const char* value;
};

struct FieldDef {
    const char* name;
    const char* label;
    FieldType type;
    const char* placeholder = "";
    bool required = false;
    const char* default_value = "";
    std::vector<SelectOption> options;
};

// ============================================================================
// Data source config definition
// ============================================================================
struct DataSourceDef {
    const char* id;
    const char* name;
    const char* type;
    Category category;
    const char* icon;  // short label (2 chars)
    const char* description;
    bool testable;
    bool requires_auth;
    std::vector<FieldDef> fields;
};

// ============================================================================
// Connection status
// ============================================================================
enum class ConnectionStatus {
    Disconnected,
    Connected,
    Testing,
    Error,
};

inline const char* status_label(ConnectionStatus s) {
    switch (s) {
        case ConnectionStatus::Disconnected: return "DISCONNECTED";
        case ConnectionStatus::Connected:    return "CONNECTED";
        case ConnectionStatus::Testing:      return "TESTING";
        case ConnectionStatus::Error:        return "ERROR";
    }
    return "?";
}

// ============================================================================
// Registry — collects all connectors from separate files
// ============================================================================
std::vector<DataSourceDef> get_all_data_source_defs();

} // namespace fincept::data_sources
