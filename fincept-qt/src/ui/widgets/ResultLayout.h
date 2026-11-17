#pragma once
// ResultLayout — classifies an analytics result (any JSON payload) into
// presentation blocks: headline KPIs, key/value details, series / bar /
// distribution / fan / heatmap charts, tables, gauges, captions and nested
// sections. Pure Qt Core (no widgets), so the rules can be exercised against
// recorded responses. ResultView turns the blocks into widgets.
//
//   result::Options o;
//   o.request = request_body;                       // echoed inputs never become headline KPIs
//   o.unit_for = [](const QString& path, const QString& key, double v) { ... };
//   const result::Block root = result::build(payload, o);
//
// Units are inferred conservatively from key names (rate / vol / probability →
// percent only for |v| <= 1.5, *_pct → already percent, *_bps → bp, money words
// → grouped amount, counts → integer); a caller with its own conventions
// overrides them with Options::unit_for.

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <limits>
#include <vector>

namespace fincept::ui::result {

/// How a number is shown.
enum class Unit {
    Auto,     ///< (unit_for only) fall back to the name-based rules
    Plain,    ///< grouped number, significant digits by magnitude
    Integer,  ///< grouped whole number
    Year,     ///< 2025 (no grouping)
    Percent,  ///< value already in percent: 9.9 -> "9.90%"
    Fraction, ///< a fraction shown as percent: 0.099 -> "9.90%"
    Bps,      ///< basis points: 25 -> "25 bp"
    Money,    ///< an amount (Options::money_format, default grouped, no symbol)
    Multiple, ///< 12.3 -> "12.30x"
    Text,     ///< not a number
};

struct Options {
    /// Unit of a numeric value; return Unit::Auto to use the name-based rules.
    /// `path` is the dotted key path from the payload root ("returns.irr_pct").
    std::function<Unit(const QString& path, const QString& key, double value)> unit_for;
    /// Text of a Money value (default: grouped, 2 decimals, no currency symbol).
    std::function<QString(double value)> money_format;
    /// The request body: a top-level result field equal to the request field of
    /// the same name is an echoed input, listed under "Inputs", never a KPI.
    QJsonObject request;
    /// Keys or dotted paths never shown.
    QSet<QString> hidden;
    /// Dotted paths the caller presents itself (skipped by the generic layout).
    QSet<QString> consumed;
    /// Dotted paths of nested sections that start collapsed.
    QSet<QString> collapsed_paths;
    /// Headline KPI paths in display order; empty = picked automatically.
    QStringList kpi_paths;
    int max_kpis = 6;
    /// Extra key names that may serve as a chart's x axis.
    QStringList x_keys;
    /// Matrix key -> (row-label array key, column-label array key).
    QHash<QString, QPair<QString, QString>> matrix_axes;
    /// Table key (array of objects / object of objects) -> numeric column(s),
    /// comma-separated, shown as bars per row label.
    QHash<QString, QString> bar_columns;
    /// Names for unlabelled per-item arrays (weights, matrices) when the counts match.
    QStringList series_labels;
    /// Long numeric arrays without an x axis are samples (histogram), not paths.
    bool samples_default = false;
    /// Label of a bare scalar payload.
    QString result_label;
};

struct Field {
    QString path;  ///< dotted path from the root
    QString key;   ///< raw key
    QString label; ///< human label
    QJsonValue value;
    Unit unit = Unit::Plain;
    /// Signed result quantity (P&L, NPV, change, ...): coloured by sign.
    bool signed_result = false;
};

struct Series {
    QString key;
    QString name;
    QVector<double> values; ///< NaN = gap
    Unit unit = Unit::Plain;
};

struct GaugeBand {
    double from = 0;
    double to = 0;
    int tone = 0; ///< -1 bad, 0 caution, +1 good
    QString label;
};

enum class Kind {
    Kpis,         ///< fields as headline tiles
    Fields,       ///< fields as a two-column table
    Caption,      ///< a long text value
    List,         ///< a list of strings
    Line,         ///< series against a numeric / date x
    Bars,         ///< series against categories
    Donut,        ///< parts of a whole
    Distribution, ///< histogram of samples
    Fan,          ///< percentile fan of simulated paths
    Heatmap,      ///< a 2-D matrix
    Gauge,        ///< a score against its zones
    Table,        ///< rows x columns
    Section,      ///< a titled group of child blocks
};

struct Block {
    Kind kind = Kind::Section;
    QString key;   ///< dotted source path
    QString title; ///< human title (empty for the root)
    int depth = 0;

    // Kpis / Fields
    QVector<Field> fields;
    // Caption / List
    QString text;
    QStringList items;
    // Line / Bars / Donut / Distribution / Fan
    QString x_title;
    QVector<double> xs;     ///< Line / Fan: numeric x (epoch ms when x_dates)
    QStringList categories; ///< Bars / Donut: one label per value
    bool x_dates = false;
    Unit x_unit = Unit::Plain;
    QVector<Series> series;
    bool diagonal = false; ///< Line: draw the y = x reference (ROC / CAP curves)
    QVector<QVector<double>> paths; ///< Fan
    // Heatmap
    QVector<QVector<double>> matrix;
    QStringList row_labels;
    QStringList col_labels;
    QString row_title;
    QString col_title;
    int scale = 0; ///< 0 diverging, 1 correlation (-1..1), 2 sequential
    Unit value_unit = Unit::Plain;
    // Gauge
    double value = std::numeric_limits<double>::quiet_NaN();
    double gauge_min = 0;
    double gauge_max = 1;
    QVector<GaugeBand> bands;
    // Table
    QStringList columns;       ///< column keys
    QStringList column_labels; ///< header text
    QVector<Unit> column_units;
    QVector<QVector<QJsonValue>> rows;
    int total_rows = 0; ///< rows in the data (rows may be capped)
    /// A comparison table (one row per metric): each row's own unit and key,
    /// used for every column after the first instead of column_units.
    QVector<Unit> row_units;
    QStringList row_keys;
    // Section
    std::vector<Block> children;
    bool collapsed = false;
};

/// Rows a Table block keeps (the full data stays in the payload).
constexpr int kMaxTableRows = 5000;

/// The presentation of `payload`: a root Section (empty title).
Block build(const QJsonValue& payload, const Options& options);

/// Unit of `value` under `key` (name rules, then Options::unit_for).
Unit unit_for(const Options& options, const QString& path, const QString& key, double value,
              const QString& parent_key = QString());
/// Display text of a number in `unit`. `sign` adds "+" to positives.
QString format_number(double value, Unit unit, const Options& options, bool sign = false);
/// Display text of any scalar JSON value (null = "not finite").
QString format_value(const QJsonValue& value, Unit unit, const Options& options, bool sign = false);
/// "expected_shortfall" -> "Expected Shortfall", "irr_pct" -> "IRR %".
QString humanize(const QString& key);
/// P&L-like keys whose sign is good / bad news.
bool is_signed_result_key(const QString& key);
/// Indented text outline of a block tree (diagnostics / tests).
QString describe(const Block& block, int indent = 0);

} // namespace fincept::ui::result
