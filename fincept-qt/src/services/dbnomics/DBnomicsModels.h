// src/services/dbnomics/DBnomicsModels.h
#pragma once
#include <QString>
#include <QVector>
#include <QColor>

namespace fincept::services {

// ── Core domain objects ──────────────────────────────────────────────────────

struct DbnProvider {
    QString code;   // "BLS"
    QString name;   // "Bureau of Labor Statistics"
};

struct DbnDataset {
    QString code;   // "ln"
    QString name;   // "Labor Force Statistics"
};

struct DbnSeriesInfo {
    QString code;   // "LNS14000000"
    QString name;   // "Civilian Unemployment Rate"
};

struct DbnObservation {
    QString period; // "2024-01", "2024-01-01"
    double  value;  // 3.5
    bool    valid;  // false if the API returned NA/null
};

// A fully loaded series ready to plot
struct DbnDataPoint {
    QString             series_id;   // "BLS/ln/LNS14000000"
    QString             series_name;
    QVector<DbnObservation> observations;
    QColor              color;
};

// ── Search ───────────────────────────────────────────────────────────────────

struct DbnSearchResult {
    QString provider_code;
    QString provider_name;
    QString dataset_code;
    QString dataset_name;
    int     nb_series = 0;
};

// ── Pagination ───────────────────────────────────────────────────────────────

struct DbnPagination {
    int offset   = 0;
    int limit    = 50;
    int total    = 0;
    bool has_more() const { return offset + limit < total; }
};

// ── UI enumerations ──────────────────────────────────────────────────────────

enum class DbnChartType {
    Line,
    Area,
    Bar,
    Scatter
};

enum class DbnViewMode {
    Single,
    Comparison
};

// A comparison slot holds multiple series for one chart panel
struct DbnSlot {
    QVector<DbnDataPoint> series;
    DbnChartType          chart_type = DbnChartType::Line;
};

} // namespace fincept::services
