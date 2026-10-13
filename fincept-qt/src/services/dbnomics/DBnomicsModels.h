// src/services/dbnomics/DBnomicsModels.h
#pragma once
#include <QColor>
#include <QDate>
#include <QString>
#include <QVector>

namespace fincept::services {

// ── Core domain objects ──────────────────────────────────────────────────────

struct DbnProvider {
    QString code; // "BLS"
    QString name; // "Bureau of Labor Statistics"
};

struct DbnDataset {
    QString code; // "ln"
    QString name; // "Labor Force Statistics"
};

struct DbnSeriesInfo {
    QString code; // "LNS14000000"
    QString name; // "Civilian Unemployment Rate"
};

struct DbnObservation {
    QString period;     // "2024-01", "2024-Q1", "2024-01-01"
    double value = 0.0; // 3.5
    bool valid = true;  // false if the API returned NA/null
    QDate start;        // first day of the period (DBnomics period_start_day) — the time axis / sort key
};

// A fully loaded series ready to plot
struct DbnDataPoint {
    QString series_id; // "BLS/ln/LNS14000000"
    QString series_name;
    QVector<DbnObservation> observations; // ascending by period start
    QColor color = QColor("#ea580c");     // default: orange; caller should override with chart_color(index)
    // What the numbers are, from the series' own metadata (empty when the provider does not say).
    QString frequency;     // "monthly", "quarterly", … (DBnomics @frequency)
    QString unit;          // label of the series' unit dimension, e.g. "Annual rate of change"
    QString seasonal;      // label of its seasonal-adjustment dimension, e.g. "Seasonally Adjusted"
    QString provider_name; // "U.S. Bureau of Labor Statistics"
    QString indexed_at;    // when DBnomics last indexed the series (ISO timestamp) — its vintage
};

// ── Search ───────────────────────────────────────────────────────────────────

struct DbnSearchResult {
    QString provider_code;
    QString provider_name;
    QString dataset_code;
    QString dataset_name;
    int nb_series = 0;
};

// ── Pagination ───────────────────────────────────────────────────────────────

struct DbnPagination {
    int offset = 0;
    int limit = 50;
    int total = 0;
    [[nodiscard]] bool has_more() const { return offset + limit < total; }
};

// ── UI enumerations ──────────────────────────────────────────────────────────

enum class DbnChartType { Line, Area, Bar, Scatter };

enum class DbnViewMode { Single, Comparison };

// A comparison slot holds multiple series for one chart panel
struct DbnSlot {
    QVector<DbnDataPoint> series;
    DbnChartType chart_type = DbnChartType::Line;
};

} // namespace fincept::services

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::services::DbnDataPoint)
