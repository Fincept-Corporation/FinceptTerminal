#pragma once
// Data Sources Screen — shared types for connector configs, connections, and UI state.

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::datasources {

// ── Field types for dynamic forms ──────────────────────────────────────────

enum class FieldType { Text, Password, Number, Url, Select, Textarea, Checkbox, File };

struct SelectOption {
    QString label;
    QString value;
};

struct FieldDef {
    QString name;
    QString label;
    FieldType type = FieldType::Text;
    QString placeholder;
    bool required = false;
    QString default_value;
    QVector<SelectOption> options; // for Select type
};

// ── Connector config (static definition of a data source type) ─────────────

enum class Category { Database, Api, File, Streaming, Cloud, TimeSeries, MarketData, SearchAnalytics, DataWarehouse };

inline QString category_str(Category c) {
    switch (c) {
        case Category::Database:
            return "database";
        case Category::Api:
            return "api";
        case Category::File:
            return "file";
        case Category::Streaming:
            return "streaming";
        case Category::Cloud:
            return "cloud";
        case Category::TimeSeries:
            return "timeseries";
        case Category::MarketData:
            return "market-data";
        case Category::SearchAnalytics:
            return "search";
        case Category::DataWarehouse:
            return "warehouse";
    }
    return "database";
}

inline QString category_label(Category c) {
    switch (c) {
        case Category::Database:
            return "Databases";
        case Category::Api:
            return "APIs";
        case Category::File:
            return "File Sources";
        case Category::Streaming:
            return "Streaming";
        case Category::Cloud:
            return "Cloud Storage";
        case Category::TimeSeries:
            return "Time Series";
        case Category::MarketData:
            return "Market Data";
        case Category::SearchAnalytics:
            return "Search & Analytics";
        case Category::DataWarehouse:
            return "Data Warehouses";
    }
    return "Other";
}

struct ConnectorConfig {
    QString id;
    QString name;
    QString type;
    Category category;
    QString icon;  // emoji
    QString color; // hex
    QString description;
    bool testable = true;
    bool requires_auth = false;
    QVector<FieldDef> fields;
};

// ── Connection status ──────────────────────────────────────────────────────

enum class ConnStatus { Connected, Disconnected, Error, Testing };

// ── UI view modes ──────────────────────────────────────────────────────────

enum class ViewMode { Gallery, Connections };

// ── Obsidian colors ────────────────────────────────────────────────────────

inline const QColor BG_BASE = QColor("#080808");
inline const QColor BG_SURFACE = QColor("#0a0a0a");
inline const QColor BG_RAISED = QColor("#111111");
inline const QColor BG_HOVER = QColor("#161616");
inline const QColor BORDER_DIM = QColor("#1a1a1a");
inline const QColor BORDER_MED = QColor("#222222");
inline const QColor TEXT_PRIMARY = QColor("#e5e5e5");
inline const QColor TEXT_SECONDARY = QColor("#808080");
inline const QColor TEXT_TERTIARY = QColor("#525252");
inline const QColor COLOR_ACCENT = QColor("#d97706");
inline const QColor COLOR_BUY = QColor("#16a34a");
inline const QColor COLOR_SELL = QColor("#dc2626");
inline const QColor COLOR_WARNING = QColor("#ca8a04");

} // namespace fincept::screens::datasources
