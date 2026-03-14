#pragma once
// Government Data Tab — provider configurations and data types

#include <string>
#include <vector>
#include <imgui.h>

namespace fincept::gov_data {

// =============================================================================
// Provider identifiers
// =============================================================================
enum class ProviderId {
    USTreasury,
    USCongress,
    CanadaGov,
    OpenAfrica,
    SpainData,
    FinlandPxWeb,
    SwissGov,
    FranceGov,
    UniversalCkan,
    HongKongGov,
    COUNT
};

// =============================================================================
// Provider configuration
// =============================================================================
struct ProviderConfig {
    ProviderId id;
    const char* name;
    const char* full_name;
    const char* description;
    ImVec4 color;
    const char* country;
    const char* flag;       // emoji flag (UTF-8)
    const char* script;     // Python script filename
};

// =============================================================================
// Endpoint definition
// =============================================================================
struct EndpointDef {
    const char* id;
    const char* name;
    const char* description;
    const char* command;
};

// =============================================================================
// Generic dataset / resource types used across CKAN-based providers
// =============================================================================
struct DatasetRecord {
    std::string id;
    std::string name;
    std::string title;
    std::string notes;
    std::string organization;
    std::string modified;
    std::string created;
    int num_resources = 0;
    std::vector<std::string> tags;
};

struct ResourceRecord {
    std::string id;
    std::string name;
    std::string description;
    std::string format;
    std::string url;
    std::string modified;
    int64_t size = 0;
};

struct OrganizationRecord {
    std::string id;
    std::string name;
    std::string display_name;
    std::string description;
    int dataset_count = 0;
    std::string created;
};

// =============================================================================
// CKAN portal info (for Universal CKAN provider)
// =============================================================================
struct CkanPortal {
    const char* code;
    const char* name;
    const char* flag;
    const char* url;
};

} // namespace fincept::gov_data
