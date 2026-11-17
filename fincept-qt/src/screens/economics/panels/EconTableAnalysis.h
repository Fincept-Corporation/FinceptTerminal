// src/screens/economics/panels/EconTableAnalysis.h
// The pure half of the economics results view (EconPanelBase): everything that walks the rows of a
// result — column order, the newest-first sort, the stat cards (LATEST · CHANGE · YoY · MIN · MAX ·
// AVG), the chart lines, a sort by any column, the CSV / clipboard text — computed from a table
// payload without touching a widget, so EconPanelBase runs it on a worker thread and the GUI thread
// only applies the finished result (a 13,113-row Bank of England series used to stall it for
// seconds: every row's date went through a dozen regular expressions three times on the GUI thread).
//
// QtCore only; any thread. Strings that need translation stay out of here (enums and raw labels
// come back; EconPanelBase words them). A standalone timing harness links this file.
#pragma once

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::econ_table {

// ── Period / number helpers ─────────────────────────────────────────────────
// Economics providers spell dates every possible way ("2024-03", "2024-Q1", "2024M03", "02 Jan 1975",
// "04-Jan-2011", "2025年09月份", bare years as JSON numbers…). Everything that orders observations
// goes through parse_period().

enum class Gran { Unknown, Day, Week, Month, Quarter, Half, Year };

struct Period {
    QDate date;
    Gran gran = Gran::Unknown;
};

Period parse_period(const QString& raw);
/// The period text of a date cell — JSON numbers (WTO "Year", IMF "year") become "2024", and bare
/// short years are zero-padded so "999" can never sort after "1999".
QString period_text(const QJsonValue& v);
/// A JSON number, or a numeric string ("4.33", "1,234.5", "12%").
bool to_number(const QJsonValue& v, double* out);
const QStringList& date_keys();
bool is_date_key(const QString& key);
const QStringList& value_keys();
bool is_id_like(const QString& key);
Gran gran_from_text(const QString& text);
/// `pts` sorted ascending.
Gran infer_gran(const QVector<Period>& pts);
/// The description says the values are percentages / rates (changes then in percentage points).
bool text_says_percent(const QString& text);
bool text_says_bp(const QString& units);
/// Plain text of a cell (CSV, clipboard, grouping, sorting): numbers with 15 significant digits,
/// nested values as compact JSON.
QString cell_text(const QJsonValue& v);
/// Columns holding a change / growth figure ("change", "pct_change", "yoy", "netChange", …).
bool is_change_key(const QString& key);
/// A string cell that is a plain decimal number ("4.33", "-12345.6") — not a code with leading zeros.
bool plain_number_string(const QString& s, double* out);

// ── Analysis of a table payload ─────────────────────────────────────────────

/// The series description EconPanelBase::SeriesMeta carries (the "meta" object of a table payload).
struct Meta {
    QString units, frequency, seasonal_adj, source, last_updated, note;
    int percent = -1; ///< 1: rates / percentages, 0: levels, -1: infer
    QString value_column;
    QDate stats_cutoff;
};
Meta meta_from_json(const QJsonObject& o);

struct Stats {
    bool applicable = false; ///< a single dated series: the cards apply
    QString column;          ///< the column the cards summarise ("" = none found)
    Gran gran = Gran::Unknown;
    bool pct = false; ///< changes in percentage points, values with "%"
    bool bp = false;  ///< basis points
    QString latest_label;
    double latest = 0.0;
    bool has_prev = false;
    QString prev_label;
    double prev = 0.0;
    bool has_yoy = false;
    QString yoy_label;
    double yoy = 0.0;
    double min = 0.0, max = 0.0, avg = 0.0;
    int count = 0;    ///< observations summarised (projections excluded)
    int excluded = 0; ///< projected periods kept out of the cards
};

struct Line {
    QString name;
    QVector<QPointF> points; ///< x = epoch ms (UTC period start), ascending; full resolution
};

enum class RangeKind { Daily, Monthly, Coarse }; ///< which range presets suit the spacing

struct Analysis {
    QJsonArray rows;       ///< the rows as objects (a bare value became {"value": v})
    QStringList columns;   ///< display order
    QVector<int> order;    ///< display position -> row index
    int sort_column = -1;  ///< -1 = the order the source sent
    bool sort_descending = true;
    QString title;
    Meta meta;
    Stats stats;
    QVector<Line> lines;   ///< chart lines (empty = no chart)
    bool chart_pct = false;
    bool chart_bp = false;
    double x_max = 0.0;
    RangeKind range_kind = RangeKind::Coarse;
    qint64 approx_bytes = 0; ///< estimate of the rows held in memory (AutoLoadPane's size estimator)
};

/// Everything EconPanelBase shows of a table payload {rows, title, order, keep, meta}.
Analysis analyse(const QJsonObject& table);

/// Display order of `rows` sorted by `key` (dates by date, numbers by value, else text; blanks last
/// in either direction; stable).
QVector<int> sorted_order(const QJsonArray& rows, const QString& key, bool ascending);

/// At most ~2 points per pixel column: min and max of each x bucket in their x order, plus the first
/// and last point — never an extreme dropped. [from, to] gets `px` buckets; the parts outside it a
/// quarter of that (they are off screen until the user pans; a range change asks again). A series
/// with no more than 2 * px + 2 points comes back unchanged.
QVector<QPointF> decimate(const QVector<QPointF>& points, double from, double to, int px);

/// Tab- or comma-separated text of `rows` in `order` (all rows when `order` is empty).
/// CSV quotes fields that need it; TSV replaces tabs / newlines inside a cell by spaces.
QString rows_text(const QJsonArray& rows, const QStringList& columns, const QVector<int>& order, bool csv);

} // namespace fincept::screens::econ_table
