#pragma once
// Data Mapping Screen — Bloomberg Terminal-inspired API mapping configuration UI.
// 3-panel layout: left sidebar | center content | right status panel.
// Mirrors the Tauri desktop DataMappingTab with 5-step wizard.

#include "data_mapping_types.h"
#include "data_mapping_engine.h"
#include "data_mapping_db.h"
#include <string>
#include <vector>
#include <future>
#include <optional>
#include <map>

namespace fincept::data_mapping {

class DataMappingScreen {
public:
    void render();

private:
    void init();

    // Top-level panels
    void render_command_bar(float w);
    void render_sidebar(float x, float y, float w, float h);
    void render_content(float x, float y, float w, float h);
    void render_status_panel(float x, float y, float w, float h);
    void render_footer(float w, float h);

    // Views
    void render_list_view(float w, float h);
    void render_create_view(float w, float h);
    void render_templates_view(float w, float h);

    // Create wizard steps
    void render_step_api_config(float w, float h);
    void render_step_schema_select(float w, float h);
    void render_step_field_mapping(float w, float h);
    void render_step_cache_settings(float w, float h);
    void render_step_test_save(float w, float h);

    // Component renderers
    void render_auth_config();
    void render_headers_editor();
    void render_query_params_editor();
    void render_field_mapping_row(size_t index);
    void render_json_preview(const std::string& label, const std::string& json_str, float height);
    void render_validation_results();

    // Actions
    void load_mappings();
    void save_current_mapping();
    void delete_mapping_by_id(const std::string& id);
    void start_test();
    void fetch_sample_data();
    void reset_create_form();
    void edit_mapping(const MappingConfig& config);
    void load_template(const MappingTemplate& tmpl);

    // State
    bool initialized_ = false;
    MappingView view_ = MappingView::list;
    CreateStep current_step_ = CreateStep::api_config;

    // Search
    char search_buf_[128] = {};

    // Sidebar
    bool sidebar_minimized_ = false;

    // Saved mappings
    std::vector<MappingConfig> saved_mappings_;
    bool loading_mappings_ = false;

    // Current mapping being created/edited
    MappingConfig current_mapping_;

    // API config form buffers
    char api_name_buf_[128] = {};
    char api_desc_buf_[256] = {};
    char api_base_url_buf_[512] = {};
    char api_endpoint_buf_[512] = {};
    char api_body_buf_[4096] = {};
    int api_method_idx_ = 0;
    int api_auth_type_idx_ = 0;

    // Auth buffers
    char auth_key_name_buf_[128] = {};
    char auth_key_value_buf_[512] = {};
    int auth_key_location_idx_ = 0;
    char auth_bearer_buf_[512] = {};
    char auth_basic_user_buf_[128] = {};
    char auth_basic_pass_buf_[256] = {};

    // Schema selection
    int schema_type_idx_ = 0;
    bool is_custom_schema_ = false;

    // Mapping name/description
    char mapping_name_buf_[128] = {};
    char mapping_desc_buf_[256] = {};

    // Extraction
    char root_path_buf_[256] = {};
    int parser_engine_idx_ = 0;

    // Header/query param editing
    char header_key_buf_[128] = {};
    char header_val_buf_[256] = {};
    char qparam_key_buf_[128] = {};
    char qparam_val_buf_[256] = {};

    // Sample data / test results
    std::string sample_data_json_;
    std::optional<ExecutionResult> test_result_;
    bool testing_ = false;
    std::future<ExecutionResult> test_future_;

    // API fetch
    bool fetching_ = false;
    std::future<ApiResponse> fetch_future_;

    // Template data
    std::vector<MappingTemplate> templates_;
    void load_templates();

    // Engine
    MappingEngine engine_;
};

} // namespace fincept::data_mapping
