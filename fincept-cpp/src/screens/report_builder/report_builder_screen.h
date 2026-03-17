#pragma once
// Report Builder Screen — document composition with templates, components, and export.
// Layout: toolbar | [component list | canvas preview | properties panel]

#include "report_builder_types.h"
#include <string>
#include <vector>

namespace fincept::report_builder {

class ReportBuilderScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_toolbar(float w);
    void render_component_list(float x, float y, float w, float h);
    void render_canvas(float x, float y, float w, float h);
    void render_properties_panel(float x, float y, float w, float h);
    void render_template_gallery();
    void render_export_dialog();

    // Canvas sub-renderers (one per component type)
    void render_component_preview(const ReportComponent& comp, float w);
    void render_heading_preview(const ReportComponent& comp, float w);
    void render_text_preview(const ReportComponent& comp, float w);
    void render_table_preview(const ReportComponent& comp, float w);
    void render_chart_preview(const ReportComponent& comp, float w);
    void render_kpi_preview(const ReportComponent& comp, float w);
    void render_code_preview(const ReportComponent& comp, float w);
    void render_list_preview(const ReportComponent& comp, float w);

    // Actions
    void add_component(ComponentType type);
    void delete_component(int id);
    void duplicate_component(int id);
    void move_component(int id, int direction); // -1 = up, +1 = down
    void apply_template_preset(int preset_idx);
    void export_report(ExportFormat format);

    // State
    bool initialized_ = false;

    // Document
    ReportTemplate document_;
    int selected_component_id_ = -1;

    // UI state
    bool show_template_gallery_ = false;
    bool show_export_dialog_ = false;
    ExportFormat export_format_ = ExportFormat::pdf;
    bool is_dirty_ = false;

    // Component palette filter
    char component_search_[64] = {};
};

} // namespace fincept::report_builder
