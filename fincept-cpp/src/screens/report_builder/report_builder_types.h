#pragma once
// Report Builder Types — document component model and template definitions.

#include <string>
#include <vector>
#include <cstdint>

namespace fincept::report_builder {

// ============================================================================
// Component types that can be added to a report
// ============================================================================

enum class ComponentType {
    heading, subheading, text, table, chart, divider,
    kpi, code_block, image, cover_page, page_break,
    quote, list, disclaimer
};

inline const char* component_type_label(ComponentType t) {
    switch (t) {
        case ComponentType::heading:    return "Heading";
        case ComponentType::subheading: return "Subheading";
        case ComponentType::text:       return "Text";
        case ComponentType::table:      return "Table";
        case ComponentType::chart:      return "Chart";
        case ComponentType::divider:    return "Divider";
        case ComponentType::kpi:        return "KPI Card";
        case ComponentType::code_block: return "Code Block";
        case ComponentType::image:      return "Image";
        case ComponentType::cover_page: return "Cover Page";
        case ComponentType::page_break: return "Page Break";
        case ComponentType::quote:      return "Quote";
        case ComponentType::list:       return "List";
        case ComponentType::disclaimer: return "Disclaimer";
    }
    return "Text";
}

inline const char* component_type_icon(ComponentType t) {
    switch (t) {
        case ComponentType::heading:    return "[H1]";
        case ComponentType::subheading: return "[H2]";
        case ComponentType::text:       return "[T]";
        case ComponentType::table:      return "[TBL]";
        case ComponentType::chart:      return "[CHT]";
        case ComponentType::divider:    return "[---]";
        case ComponentType::kpi:        return "[KPI]";
        case ComponentType::code_block: return "[<>]";
        case ComponentType::image:      return "[IMG]";
        case ComponentType::cover_page: return "[CV]";
        case ComponentType::page_break: return "[PB]";
        case ComponentType::quote:      return "[Q]";
        case ComponentType::list:       return "[LI]";
        case ComponentType::disclaimer: return "[!]";
    }
    return "[?]";
}

// ============================================================================
// Text alignment
// ============================================================================

enum class Alignment { left, center, right };

inline const char* alignment_label(Alignment a) {
    switch (a) {
        case Alignment::left:   return "Left";
        case Alignment::center: return "Center";
        case Alignment::right:  return "Right";
    }
    return "Left";
}

// ============================================================================
// Chart types for chart components
// ============================================================================

enum class ChartType { bar, line, pie, area };

inline const char* chart_type_label(ChartType t) {
    switch (t) {
        case ChartType::bar:  return "Bar";
        case ChartType::line: return "Line";
        case ChartType::pie:  return "Pie";
        case ChartType::area: return "Area";
    }
    return "Bar";
}

// ============================================================================
// Page settings
// ============================================================================

enum class PageSize { a4, letter, legal };

inline const char* page_size_label(PageSize s) {
    switch (s) {
        case PageSize::a4:     return "A4";
        case PageSize::letter: return "Letter";
        case PageSize::legal:  return "Legal";
    }
    return "A4";
}

enum class PageOrientation { portrait, landscape };

// ============================================================================
// KPI entry for KPI cards
// ============================================================================

struct KPIEntry {
    char label[64] = {};
    char value[64] = {};
    double change = 0.0;
    bool trend_up = true;
};

// ============================================================================
// Report Component — a single block in the document
// ============================================================================

struct ReportComponent {
    int id = 0;
    ComponentType type = ComponentType::text;

    // Content (text-based components)
    char content[4096] = {};

    // Config
    Alignment alignment = Alignment::left;
    int font_size = 11;
    bool bold = false;
    bool italic = false;

    // Chart config
    ChartType chart_type = ChartType::bar;
    std::vector<double> chart_data;
    std::vector<std::string> chart_labels;

    // Table config (rows x cols as flat text)
    int table_rows = 3;
    int table_cols = 3;
    std::vector<std::string> table_cells;

    // KPI config
    std::vector<KPIEntry> kpis;

    // List config
    bool ordered = false;
    std::vector<std::string> list_items;

    // Code block
    char language[32] = "python";
};

// ============================================================================
// Report Template / Document
// ============================================================================

struct ReportTemplate {
    char name[128] = "Untitled Report";
    char description[256] = {};
    char author[64] = {};
    char company[64] = {};
    char date[32] = {};

    PageSize page_size = PageSize::a4;
    PageOrientation orientation = PageOrientation::portrait;

    std::vector<ReportComponent> components;
    int next_id = 1;
};

// ============================================================================
// Template presets
// ============================================================================

struct TemplatePreset {
    const char* name;
    const char* description;
    const char* category;
};

inline constexpr TemplatePreset TEMPLATE_PRESETS[] = {
    {"Blank Report",          "Empty document",                    "General"},
    {"Portfolio Summary",     "Investment portfolio overview",     "Finance"},
    {"Market Analysis",       "Market trends and analysis",        "Finance"},
    {"Quarterly Earnings",    "Quarterly earnings report",         "Finance"},
    {"Risk Assessment",       "Risk analysis and mitigation",      "Finance"},
    {"Economic Outlook",      "Macroeconomic analysis",            "Economics"},
    {"Due Diligence",         "M&A due diligence report",          "Legal"},
    {"Client Presentation",   "Client-facing presentation",        "General"},
    {"Research Note",         "Equity research note",              "Research"},
    {"Compliance Report",     "Regulatory compliance summary",     "Legal"},
};
inline constexpr int TEMPLATE_PRESET_COUNT = sizeof(TEMPLATE_PRESETS) / sizeof(TEMPLATE_PRESETS[0]);

// ============================================================================
// Export format
// ============================================================================

enum class ExportFormat { pdf, csv, html, markdown };

inline const char* export_format_label(ExportFormat f) {
    switch (f) {
        case ExportFormat::pdf:      return "PDF";
        case ExportFormat::csv:      return "CSV";
        case ExportFormat::html:     return "HTML";
        case ExportFormat::markdown: return "Markdown";
    }
    return "PDF";
}

} // namespace fincept::report_builder
