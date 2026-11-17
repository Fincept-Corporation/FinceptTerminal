#include "screens/node_editor/properties/ExecutionResultsPanel.h"

#include "ui/charts/CandleChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/tables/DataTable.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::workflow {

// ── Helpers ────────────────────────────────────────────────────────────────

namespace {

QString erp_duration(int ms) {
    if (ms < 1000)
        return QString("%1ms").arg(ms);
    return QString("%1s").arg(ms / 1000.0, 0, 'f', 1);
}

// Full precision (6 significant digits turned 1234.5678 into 1234.57 and an order
// quantity of 1234567 into 1.23457e+06).
QString erp_number(double v) {
    if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 1e15)
        return QString::number(static_cast<qint64>(v));
    return QString::number(v, 'g', 12);
}

QString erp_value(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return erp_number(v.toDouble());
    if (v.isBool())
        return v.toBool() ? "true" : "false";
    if (v.isNull() || v.isUndefined())
        return "—";
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

QString erp_tr(const char* text) {
    return QCoreApplication::translate("ExecutionResultsPanel", text);
}

QLabel* erp_label(const QString& text, const char* object_name) {
    auto* l = new QLabel(text);
    l->setObjectName(object_name);
    return l;
}

// Header bar height: room for one line of the panel's small font.
int erp_header_h() {
    return qMax(32, QFontMetrics(ui::fonts::font(-4)).height() + 12);
}

// Status badge column: wide enough for the longest badge text in bold.
int erp_badge_w() {
    const QFontMetrics fm(ui::fonts::font(-4, QFont::Bold));
    int w = 0;
    for (const char* t : {"OK", "FAILED", "SKIPPED"})
        w = qMax(w, fm.horizontalAdvance(erp_tr(t)));
    w = qMax(w, fm.horizontalAdvance(ExecutionResultsPanel::tr("RUNNING")));
    return qMax(56, w + 8);
}

// One run-log row. Styling comes from the panel stylesheet (objectName + "state"
// property) — no per-widget setStyleSheet, these are created once per node per run.
QWidget* erp_make_card(const QString& state, const QString& badge_text, const QString& title,
                       const QString& title_tip, const QString& right_text) {
    auto* card = new QWidget(nullptr);
    card->setObjectName("resultCard");
    card->setProperty("state", state);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(4);

    auto* header_row = new QWidget(card);
    auto* hl = new QHBoxLayout(header_row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    auto* badge = erp_label(badge_text, "resultBadge");
    badge->setProperty("state", state);
    badge->setFixedWidth(erp_badge_w());
    hl->addWidget(badge);

    auto* name = erp_label(title, "resultName");
    name->setToolTip(title_tip);
    hl->addWidget(name);
    hl->addStretch();
    hl->addWidget(erp_label(right_text, "resultDim"));
    vl->addWidget(header_row);
    return card;
}

// ── Typed result renderers ───────────────────────────────────────────────────
// A node's output is shown by shape: headline numbers in a KpiStrip, row sets in
// a sortable DataTable (with a candle / line chart when the rows are a time
// series), correlation matrices as a MatrixHeatmap, weights as a DonutChart, an
// agent's answer as Markdown, gates as a PASS / FAIL badge. Anything else falls
// back to a key / value table. The raw JSON stays one click away (RAW).

enum class ErpUnit { Plain, Fraction, Percent, Ratio, Price, Money, Volume, Count, Time };

constexpr int kErpMaxRows = 200;    // rows shown per table ("showing N of M")
constexpr int kErpMaxColumns = 14;  // columns shown per row table
constexpr int kErpChartHeight = 180;
constexpr int kErpRawChars = 20000; // RAW view text cap

// "change_percent" / "changePercent" -> "changepercent"
QString erp_norm_key(const QString& key) {
    QString k = key.toLower();
    k.remove(QLatin1Char('_'));
    return k;
}

bool erp_is_time_key(const QString& key) {
    const QString k = erp_norm_key(key);
    return k == QLatin1String("timestamp") || k == QLatin1String("time") || k == QLatin1String("date") ||
           k == QLatin1String("datetime") || k.endsWith(QLatin1String("date")) || k == QLatin1String("published");
}

ErpUnit erp_unit_for(const QString& key) {
    const QString k = erp_norm_key(key);
    static const QSet<QString> kFraction = {
        QStringLiteral("totalreturn"),   QStringLiteral("annualizedreturn"),    QStringLiteral("volatility"),
        QStringLiteral("annualizedvolatility"), QStringLiteral("maxdrawdown"), QStringLiteral("var"),
        QStringLiteral("cvar"),          QStringLiteral("meandailyreturn"),     QStringLiteral("probprofit"),
        QStringLiteral("drawdown"),      QStringLiteral("weight"),              QStringLiteral("confidence"),
        QStringLiteral("riskfreerate"),  QStringLiteral("winrate"),
    };
    if (kFraction.contains(k))
        return ErpUnit::Fraction;
    if (k.endsWith(QLatin1String("pct")) || k.endsWith(QLatin1String("percent")))
        return ErpUnit::Percent;
    if (k.contains(QLatin1String("sharpe")) || k.contains(QLatin1String("sortino")) ||
        k.contains(QLatin1String("calmar")) || k.endsWith(QLatin1String("ratio")) || k.contains(QLatin1String("zscore")) ||
        k == QLatin1String("beta") || k == QLatin1String("alpha") || k.contains(QLatin1String("correlation")) ||
        k == QLatin1String("profitfactor"))
        return ErpUnit::Ratio;
    if (k.contains(QLatin1String("volume")))
        return ErpUnit::Volume;
    if (erp_is_time_key(key))
        return ErpUnit::Time;
    if (k == QLatin1String("count") || k.endsWith(QLatin1String("count")) || k == QLatin1String("datapoints") ||
        k == QLatin1String("observations") || k == QLatin1String("simulations") || k == QLatin1String("horizondays") ||
        k == QLatin1String("quantity") || k == QLatin1String("qty") || k == QLatin1String("shares") ||
        k.endsWith(QLatin1String("trades")) || k.startsWith(QLatin1String("trades")))
        return ErpUnit::Count;
    if (k == QLatin1String("price") || k.endsWith(QLatin1String("price")) || k == QLatin1String("close") ||
        k == QLatin1String("open") || k == QLatin1String("high") || k == QLatin1String("low") ||
        k == QLatin1String("ltp") || k == QLatin1String("last") || k == QLatin1String("bid") ||
        k == QLatin1String("ask") || k == QLatin1String("mid") || k.endsWith(QLatin1String("final")))
        return ErpUnit::Price;
    if (k.contains(QLatin1String("pnl")) || k.contains(QLatin1String("balance")) || k.contains(QLatin1String("capital")) ||
        k.contains(QLatin1String("margin")) || k.endsWith(QLatin1String("value")) || k == QLatin1String("change"))
        return ErpUnit::Money;
    return ErpUnit::Plain;
}

// Values whose sign is good / bad news get gain / loss colouring.
bool erp_is_signed_key(const QString& key) {
    const QString k = erp_norm_key(key);
    return k.contains(QLatin1String("pnl")) || k.contains(QLatin1String("return")) || k.startsWith(QLatin1String("change"));
}

QString erp_time_text(double v) {
    if (!std::isfinite(v) || v <= 0)
        return ui::formatting::placeholder();
    const qint64 secs = v < 1e11 ? static_cast<qint64>(v) : static_cast<qint64>(v / 1000.0);
    return ui::formatting::date_time(secs, ui::formatting::DateStyle::Short);
}

QString erp_format_number(const QString& key, double v) {
    namespace fmt = ui::formatting;
    if (!std::isfinite(v))
        return fmt::placeholder();
    const bool sign = erp_is_signed_key(key);
    switch (erp_unit_for(key)) {
        case ErpUnit::Fraction:
            return fmt::fraction_pct(v, 2, sign);
        case ErpUnit::Percent:
            return fmt::pct(v, 2, sign);
        case ErpUnit::Ratio:
            return fmt::number(v, 2);
        case ErpUnit::Volume:
            return fmt::volume(v);
        case ErpUnit::Count:
            return fmt::quantity(v);
        case ErpUnit::Time:
            return erp_time_text(v);
        case ErpUnit::Price:
            return fmt::price(v);
        case ErpUnit::Money:
            return sign ? fmt::signed_number(v, 2) : fmt::number(v, 2);
        case ErpUnit::Plain:
            break;
    }
    if (v == std::floor(v) && std::abs(v) < 1e15)
        return fmt::number(v, 0);
    return fmt::number(v, std::abs(v) < 1.0 ? 4 : 2);
}

// "change_percent" -> "CHANGE %", "sharpeRatio" -> "SHARPE RATIO".
QString erp_heading(const QString& key) {
    QString out;
    for (int i = 0; i < key.size(); ++i) {
        const QChar c = key[i];
        if (c == QLatin1Char('_')) {
            out += QLatin1Char(' ');
        } else {
            if (c.isUpper() && i > 0 && key[i - 1].isLower())
                out += QLatin1Char(' ');
            out += c;
        }
    }
    out = out.toUpper();
    out.replace(QLatin1String(" PERCENT"), QLatin1String(" %"));
    out.replace(QLatin1String(" PCT"), QLatin1String(" %"));
    return out;
}

QString erp_scalar_text(const QString& key, const QJsonValue& v) {
    if (v.isDouble())
        return erp_format_number(key, v.toDouble());
    if (v.isBool())
        return v.toBool() ? erp_tr("Yes") : erp_tr("No");
    if (v.isString())
        return v.toString();
    if (v.isNull() || v.isUndefined())
        return ui::formatting::placeholder();
    QString s = erp_value(v);
    if (s.size() > 160)
        s = s.left(160) + QStringLiteral("…");
    return s;
}

QColor erp_sign_color(double v) {
    if (v > 0)
        return QColor(ui::colors::POSITIVE());
    if (v < 0)
        return QColor(ui::colors::NEGATIVE());
    return QColor(ui::colors::TEXT_PRIMARY());
}

// Keys that are executor / node bookkeeping, never shown.
bool erp_is_hidden_key(const QString& key) {
    return key.startsWith(QLatin1Char('_')) || key == QLatin1String("display") || key == QLatin1String("node_type") ||
           key == QLatin1String("success");
}

// A table sized to its rows (the run log scrolls; the table only up to a cap).
void erp_fit_table_height(ui::DataTable* table, int rows) {
    const int row_h = table->verticalHeader()->defaultSectionSize();
    const int head_h = table->horizontalHeader()->sizeHint().height();
    table->setFixedHeight(qMin(row_h * 9 + head_h + 4, head_h + row_h * qMax(1, rows) + 4));
}

QTableWidgetItem* erp_cell(const QString& key, const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        auto* item = new ui::NumericTableWidgetItem(erp_format_number(key, d), d);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setForeground(erp_is_signed_key(key) ? erp_sign_color(d) : QColor(ui::colors::TEXT_PRIMARY()));
        return item;
    }
    auto* item = new QTableWidgetItem(erp_scalar_text(key, v));
    item->setForeground(QColor(v.isNull() || v.isUndefined() ? ui::colors::TEXT_TERTIARY() : ui::colors::TEXT_PRIMARY()));
    if (v.isString() && v.toString().size() > 60)
        item->setToolTip(v.toString().left(2000));
    return item;
}

// Two-column FIELD / VALUE table for scalars that are not headline numbers.
QWidget* erp_fields_table(const QJsonObject& obj, const QStringList& keys) {
    auto* table = new ui::DataTable;
    table->set_headers({erp_tr("FIELD"), erp_tr("VALUE")});
    table->setRowCount(keys.size());
    for (int r = 0; r < keys.size(); ++r) {
        auto* k = new QTableWidgetItem(erp_heading(keys[r]));
        k->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table->setItem(r, 0, k);
        table->setItem(r, 1, erp_cell(keys[r], obj.value(keys[r])));
    }
    table->resizeColumnToContents(0);
    erp_fit_table_height(table, keys.size());
    return table;
}

ui::KpiStrip* erp_kpi_strip(const QJsonObject& obj, const QStringList& keys, const QHash<QString, QString>& labels = {}) {
    auto* strip = new ui::KpiStrip;
    strip->set_max_columns(6);
    for (const QString& key : keys) {
        const QJsonValue v = obj.value(key);
        auto* tile = strip->add_tile(key, labels.value(key, erp_heading(key)));
        tile->set_compact(true);
        tile->set_value(erp_scalar_text(key, v));
        if (v.isDouble() && erp_is_signed_key(key) && v.toDouble() != 0.0)
            tile->set_tone(v.toDouble() > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative);
    }
    return strip;
}

double erp_time_ms(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        return d < 1e11 ? d * 1000.0 : d;
    }
    if (v.isString()) {
        QDateTime dt = QDateTime::fromString(v.toString(), Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime(QDate::fromString(v.toString().left(10), Qt::ISODate), QTime(0, 0), QTimeZone::UTC);
        if (dt.isValid())
            return static_cast<double>(dt.toMSecsSinceEpoch());
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// Columns of a row set: time first, then labels, OHLC in order, the rest, volume last.
QStringList erp_row_columns(const QJsonArray& rows) {
    QStringList keys;
    for (int i = 0; i < rows.size() && i < 50; ++i) {
        if (!rows[i].isObject())
            continue;
        const QJsonObject o = rows[i].toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            if (!keys.contains(it.key()) && !erp_is_hidden_key(it.key()))
                keys << it.key();
    }
    auto rank = [](const QString& k) {
        const QString n = erp_norm_key(k);
        if (erp_is_time_key(k))
            return 0;
        if (n == QLatin1String("symbol") || n == QLatin1String("name") || n == QLatin1String("headline") ||
            n == QLatin1String("title"))
            return 1;
        if (n == QLatin1String("open"))
            return 2;
        if (n == QLatin1String("high"))
            return 3;
        if (n == QLatin1String("low"))
            return 4;
        if (n == QLatin1String("close"))
            return 5;
        if (n.contains(QLatin1String("volume")))
            return 7;
        return 6;
    };
    std::stable_sort(keys.begin(), keys.end(), [&](const QString& a, const QString& b) { return rank(a) < rank(b); });
    if (keys.size() > kErpMaxColumns)
        keys = keys.mid(0, kErpMaxColumns);
    return keys;
}

QString erp_find_key(const QStringList& keys, const char* norm_name) {
    for (const QString& k : keys)
        if (erp_norm_key(k) == QLatin1String(norm_name))
            return k;
    return {};
}

// Candles for OHLC rows, else a line of the chosen numeric columns, when the rows
// carry a time column. nullptr when the rows are not a time series.
QWidget* erp_series_chart(const QJsonArray& rows, const QStringList& columns, const QStringList& series_hint) {
    QString time_key;
    for (const QString& c : columns)
        if (erp_is_time_key(c)) {
            time_key = c;
            break;
        }
    if (time_key.isEmpty() || rows.size() < 3)
        return nullptr;

    const QString o = erp_find_key(columns, "open"), h = erp_find_key(columns, "high"), l = erp_find_key(columns, "low"),
                  c = erp_find_key(columns, "close"), vol = erp_find_key(columns, "volume");
    if (!o.isEmpty() && !h.isEmpty() && !l.isEmpty() && !c.isEmpty() && series_hint.isEmpty()) {
        QVector<ui::CandleChart::Bar> bars;
        bars.reserve(rows.size());
        for (const QJsonValue& rv : rows) {
            const QJsonObject r = rv.toObject();
            ui::CandleChart::Bar b;
            b.time_ms = erp_time_ms(r.value(time_key));
            b.open = r.value(o).toDouble(std::numeric_limits<double>::quiet_NaN());
            b.high = r.value(h).toDouble(std::numeric_limits<double>::quiet_NaN());
            b.low = r.value(l).toDouble(std::numeric_limits<double>::quiet_NaN());
            b.close = r.value(c).toDouble(std::numeric_limits<double>::quiet_NaN());
            b.volume = vol.isEmpty() ? 0.0 : r.value(vol).toDouble(0);
            if (std::isfinite(b.time_ms))
                bars.append(b);
        }
        if (bars.size() < 3)
            return nullptr;
        auto* chart = new ui::CandleChart;
        chart->set_bars(bars);
        chart->set_show_volume(!vol.isEmpty());
        chart->set_default_visible_bars(static_cast<int>(bars.size()));
        chart->reset_view();
        chart->setFixedHeight(kErpChartHeight + 40);
        return chart;
    }

    // Line chart: the hinted columns (an indicator's own), else close / value / the first number.
    QStringList plot = series_hint;
    if (plot.isEmpty()) {
        for (const char* pref : {"close", "value", "price"}) {
            const QString k = erp_find_key(columns, pref);
            if (!k.isEmpty()) {
                plot << k;
                break;
            }
        }
    }
    if (plot.isEmpty()) {
        for (const QString& k : columns) {
            if (k != time_key && rows.first().toObject().value(k).isDouble()) {
                plot << k;
                break;
            }
        }
    }
    if (plot.isEmpty())
        return nullptr;

    auto* chart = new ui::TimeSeriesChart;
    chart->set_show_mode_buttons(false);
    int added = 0;
    for (const QString& key : plot.mid(0, 4)) {
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = erp_heading(key);
        for (const QJsonValue& rv : rows) {
            const QJsonObject r = rv.toObject();
            const double x = erp_time_ms(r.value(time_key));
            const QJsonValue y = r.value(key);
            if (std::isfinite(x) && y.isDouble())
                spec.points.append(QPointF(x, y.toDouble()));
        }
        if (spec.points.size() >= 2) {
            chart->add_series(spec);
            ++added;
        }
    }
    if (added == 0) {
        delete chart;
        return nullptr;
    }
    chart->setFixedHeight(kErpChartHeight);
    return chart;
}

// A row set: optional chart, then a sortable table capped at kErpMaxRows.
QWidget* erp_render_rows(const QJsonArray& rows, const QStringList& series_hint = {}) {
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);

    const QStringList columns = erp_row_columns(rows);
    if (columns.isEmpty()) {
        vl->addWidget(erp_label(erp_tr("No rows"), "resultDim"));
        return box;
    }
    if (QWidget* chart = erp_series_chart(rows, columns, series_hint))
        vl->addWidget(chart);

    const int total = static_cast<int>(rows.size());
    const int shown = qMin(total, kErpMaxRows);
    auto* table = new ui::DataTable;
    QStringList headers;
    for (const QString& c : columns)
        headers << erp_heading(c);
    table->set_headers(headers);
    table->setRowCount(shown);
    // Newest first for time series (rows arrive oldest first).
    const bool time_first = erp_is_time_key(columns.first());
    for (int r = 0; r < shown; ++r) {
        const int src = time_first ? total - 1 - r : r;
        const QJsonObject row = rows[src].toObject();
        for (int c = 0; c < columns.size(); ++c)
            table->setItem(r, c, erp_cell(columns[c], row.value(columns[c])));
    }
    table->setSortingEnabled(true);
    table->resizeColumnsToContents();
    erp_fit_table_height(table, shown);
    vl->addWidget(table);
    if (shown < total)
        vl->addWidget(erp_label(erp_tr("Showing %1 of %2 rows (newest first) — COPY has them all")
                                    .arg(shown)
                                    .arg(total),
                                "resultDim"));
    return box;
}

QLabel* erp_section_title(const QString& text) {
    auto* l = erp_label(text, "resultKey");
    l->setContentsMargins(0, 4, 0, 0);
    return l;
}

QWidget* erp_render_object(const QJsonObject& obj, const QString& node_type, int depth);

// Any JSON value as a widget (never blank).
QWidget* erp_render_value(const QJsonValue& v, const QString& node_type, int depth) {
    if (v.isObject())
        return erp_render_object(v.toObject(), node_type, depth);
    if (v.isArray()) {
        const QJsonArray arr = v.toArray();
        if (arr.isEmpty())
            return erp_label(erp_tr("No output (empty list)"), "resultDim");
        bool objects = true, numbers = true;
        for (const QJsonValue& e : arr) {
            objects = objects && e.isObject();
            numbers = numbers && e.isDouble();
        }
        if (objects)
            return erp_render_rows(arr);
        if (numbers && arr.size() >= 3) {
            auto* chart = new ui::TimeSeriesChart;
            chart->set_x_is_time(false);
            chart->set_show_mode_buttons(false);
            ui::TimeSeriesChart::SeriesSpec spec;
            spec.name = erp_tr("Value");
            for (int i = 0; i < arr.size(); ++i)
                spec.points.append(QPointF(i, arr[i].toDouble()));
            chart->add_series(spec);
            chart->setFixedHeight(kErpChartHeight);
            return chart;
        }
        // A list of scalars / mixed values: one per line.
        QStringList lines;
        for (int i = 0; i < arr.size() && i < 50; ++i)
            lines << QStringLiteral("• ") + erp_scalar_text(QString(), arr[i]);
        if (arr.size() > 50)
            lines << erp_tr("… %1 more").arg(arr.size() - 50);
        auto* l = erp_label(lines.join(QLatin1Char('\n')), "resultValue");
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    }
    if (v.isNull() || v.isUndefined())
        return erp_label(erp_tr("No output"), "resultDim");
    auto* l = erp_label(erp_scalar_text(QString(), v), "resultValue");
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

// Headline keys per node type (in display order); others pick the first numbers.
QStringList erp_kpi_keys(const QString& node_type, const QJsonObject& obj) {
    static const QHash<QString, QStringList> kByType = {
        {QStringLiteral("market.get_quote"),
         {"price", "change", "change_percent", "volume", "high", "low"}},
        {QStringLiteral("analytics.performance_metrics"),
         {"total_return", "annualized_return", "volatility", "max_drawdown", "sharpe_ratio", "sortino_ratio"}},
        {QStringLiteral("analytics.risk_analysis"), {"var", "cvar", "annualized_volatility", "confidence", "horizon_days"}},
        {QStringLiteral("analytics.sharpe_ratio"), {"value", "annualized_return", "annualized_volatility", "max_drawdown"}},
        {QStringLiteral("analytics.monte_carlo"), {"start_price", "median_final", "p5", "p95", "prob_profit"}},
        {QStringLiteral("analytics.backtest"),
         {"totalReturn", "annualizedReturn", "sharpeRatio", "maxDrawdown", "winRate", "totalTrades"}},
        {QStringLiteral("trading.place_order"), {"status", "side", "quantity", "fill_price", "mode"}},
    };
    QStringList keys;
    for (const QString& k : kByType.value(node_type))
        if (obj.contains(k) && !obj.value(k).isObject() && !obj.value(k).isArray())
            keys << k;
    if (!keys.isEmpty())
        return keys;
    for (auto it = obj.constBegin(); it != obj.constEnd() && keys.size() < 6; ++it)
        if (it.value().isDouble() && !erp_is_hidden_key(it.key()) && !erp_is_time_key(it.key()))
            keys << it.key();
    return keys;
}

QWidget* erp_render_object(const QJsonObject& obj_in, const QString& node_type, int depth) {
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);

    QJsonObject obj = obj_in;
    QSet<QString> consumed;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        if (erp_is_hidden_key(it.key()))
            consumed.insert(it.key());

    // Results Display: its data under its label (label / data / display are bookkeeping).
    if (node_type == QLatin1String("output.results_display") && obj.contains(QStringLiteral("data"))) {
        auto* card = new ui::Card(obj.value(QStringLiteral("label")).toString(erp_tr("Output")));
        card->content_layout()->addWidget(erp_render_value(obj.value(QStringLiteral("data")), QString(), depth));
        vl->addWidget(card);
        return box;
    }

    // Backtest: its metrics are the headline (they sit one level down).
    if (node_type == QLatin1String("analytics.backtest") && obj.value(QStringLiteral("metrics")).isObject()) {
        const QJsonObject m = obj.value(QStringLiteral("metrics")).toObject();
        const QStringList keys = erp_kpi_keys(node_type, m);
        if (!keys.isEmpty())
            vl->addWidget(erp_kpi_strip(m, keys));
        QStringList rest;
        for (auto it = m.constBegin(); it != m.constEnd(); ++it)
            if (!keys.contains(it.key()))
                rest << it.key();
        if (!rest.isEmpty())
            vl->addWidget(erp_fields_table(m, rest));
        consumed.insert(QStringLiteral("metrics"));
    }

    // Agent / LLM answer: Markdown.
    if (obj.value(QStringLiteral("response")).isString()) {
        QString response = obj.value(QStringLiteral("response")).toString();
        response.replace(QLatin1String("\\n"), QLatin1String("\n"));
        auto* md = new QTextBrowser;
        md->setObjectName(QStringLiteral("resultMarkdown"));
        md->setOpenExternalLinks(true);
        md->setHtml(ui::MarkdownRenderer::render(response));
        md->setMinimumHeight(80);
        md->setMaximumHeight(260);
        vl->addWidget(md);
        consumed.insert(QStringLiteral("response"));
    }

    // Gates: PASS / FAIL and the reasons.
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!it.key().endsWith(QLatin1String("_passed")) || !it.value().isBool())
            continue;
        const bool pass = it.value().toBool();
        auto* badge = erp_label(pass ? erp_tr("PASS") : erp_tr("FAIL"), "resultGate");
        badge->setProperty("state", pass ? "ok" : "error");
        vl->addWidget(badge, 0, Qt::AlignLeft);
        consumed.insert(it.key());
        for (auto f = obj.constBegin(); f != obj.constEnd(); ++f) {
            const bool list = f.key().endsWith(QLatin1String("_failures")) && f.value().isArray();
            const bool single = f.key().endsWith(QLatin1String("_failure")) && f.value().isString();
            if (!list && !single)
                continue;
            QStringList reasons;
            if (list)
                for (const QJsonValue& r : f.value().toArray())
                    reasons << QStringLiteral("• ") + r.toString();
            else
                reasons << QStringLiteral("• ") + f.value().toString();
            auto* why = erp_label(reasons.join(QLatin1Char('\n')), "resultError");
            why->setWordWrap(true);
            vl->addWidget(why);
            consumed.insert(f.key());
        }
    }

    // Correlation matrix.
    if (obj.value(QStringLiteral("matrix")).isArray() && obj.value(QStringLiteral("labels")).isArray()) {
        QStringList labels;
        for (const QJsonValue& l : obj.value(QStringLiteral("labels")).toArray())
            labels << l.toString();
        QVector<QVector<double>> m;
        for (const QJsonValue& row : obj.value(QStringLiteral("matrix")).toArray()) {
            QVector<double> r;
            for (const QJsonValue& c : row.toArray())
                r << (c.isDouble() ? c.toDouble() : std::numeric_limits<double>::quiet_NaN());
            m << r;
        }
        if (!labels.isEmpty() && m.size() == labels.size()) {
            auto* heat = new ui::MatrixHeatmap;
            heat->set_matrix(labels, labels, m);
            heat->set_diverging_scale(-1.0, 1.0, 0.0);
            heat->setMinimumHeight(qMax(kErpChartHeight, 26 * static_cast<int>(labels.size()) + 60));
            vl->addWidget(heat);
            consumed.insert(QStringLiteral("matrix"));
            consumed.insert(QStringLiteral("labels"));
        }
    }

    // Weights {symbol: fraction}: a donut plus the numbers in the table below.
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!it.key().contains(QLatin1String("weight"), Qt::CaseInsensitive) || !it.value().isObject())
            continue;
        const QJsonObject w = it.value().toObject();
        QVector<ui::DonutSlice> slices;
        for (auto s = w.constBegin(); s != w.constEnd(); ++s)
            if (s.value().isDouble() && s.value().toDouble() > 0)
                slices.append({s.key(), s.key(), s.value().toDouble(), QColor()});
        if (slices.isEmpty())
            continue;
        auto* donut = new ui::DonutChart;
        donut->set_slices(slices);
        donut->set_center_text(erp_heading(it.key()), QStringLiteral(" "));
        donut->setFixedHeight(kErpChartHeight + 20);
        vl->addWidget(donut);
        QStringList wkeys = w.keys();
        auto* wt = new ui::DataTable;
        wt->set_headers({erp_tr("SYMBOL"), erp_tr("WEIGHT")});
        wt->setRowCount(wkeys.size());
        for (int r = 0; r < wkeys.size(); ++r) {
            wt->setItem(r, 0, new QTableWidgetItem(wkeys[r]));
            wt->setItem(r, 1, erp_cell(QStringLiteral("weight"), w.value(wkeys[r])));
        }
        wt->setSortingEnabled(true);
        erp_fit_table_height(wt, wkeys.size());
        vl->addWidget(wt);
        consumed.insert(it.key());
    }

    // Headline numbers.
    QStringList kpis;
    if (depth <= 1) {
        for (const QString& k : erp_kpi_keys(node_type, obj))
            if (!consumed.contains(k))
                kpis << k;
        if (!kpis.isEmpty()) {
            QHash<QString, QString> labels;
            // An indicator's latest value is labelled with the indicator ("RSI").
            if (obj.value(QStringLiteral("indicator")).isString() && kpis.contains(QStringLiteral("value")))
                labels.insert(QStringLiteral("value"), obj.value(QStringLiteral("indicator")).toString().toUpper());
            vl->addWidget(erp_kpi_strip(obj, kpis, labels));
            for (const QString& k : kpis)
                consumed.insert(k);
        }
    }

    // Technical-indicator rows: chart the indicator's own columns, not the close.
    QStringList indicator_cols;
    if (obj.value(QStringLiteral("data")).isArray() && obj.value(QStringLiteral("columns")).isArray()) {
        for (const QJsonValue& c : obj.value(QStringLiteral("columns")).toArray())
            indicator_cols << c.toString();
        consumed.insert(QStringLiteral("columns"));
    }

    // Nested arrays / objects, then the remaining scalars as a FIELD / VALUE table.
    QStringList scalars;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString& key = it.key();
        if (consumed.contains(key))
            continue;
        const QJsonValue& v = it.value();
        if (v.isArray()) {
            const QJsonArray arr = v.toArray();
            if (arr.isEmpty()) {
                scalars << key;
                continue;
            }
            vl->addWidget(erp_section_title(erp_heading(key)));
            const QStringList hint = key == QLatin1String("data") ? indicator_cols : QStringList();
            vl->addWidget(arr.first().isObject() ? erp_render_rows(arr, hint) : erp_render_value(arr, {}, depth + 1));
        } else if (v.isObject()) {
            if (depth >= 2) {
                scalars << key; // deep nesting: compact text in the table
                continue;
            }
            vl->addWidget(erp_section_title(erp_heading(key)));
            vl->addWidget(erp_render_object(v.toObject(), {}, depth + 1));
        } else {
            scalars << key;
        }
    }
    scalars.removeIf([&](const QString& k) { return consumed.contains(k); });
    if (!scalars.isEmpty())
        vl->addWidget(erp_fields_table(obj, scalars));

    if (vl->count() == 0)
        vl->addWidget(erp_label(erp_tr("No output"), "resultDim"));
    return box;
}

// RAW toggle: the node's JSON, secondary to the rendered view.
QWidget* erp_raw_toggle(const QJsonValue& output) {
    auto* box = new QWidget;
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);
    auto* btn = new QPushButton(erp_tr("RAW ▸"));
    btn->setObjectName(QStringLiteral("erpLinkBtn"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setToolTip(erp_tr("Show the node's raw JSON output"));
    auto* raw = new QPlainTextEdit;
    raw->setObjectName(QStringLiteral("resultRaw"));
    raw->setReadOnly(true);
    raw->setMaximumHeight(180);
    raw->hide();
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(btn);
    row->addStretch();
    vl->addLayout(row);
    vl->addWidget(raw);
    QObject::connect(btn, &QPushButton::clicked, raw, [btn, raw, output]() {
        const bool show = !raw->isVisible();
        if (show && raw->toPlainText().isEmpty()) {
            QString text = output.isObject() ? QString::fromUtf8(QJsonDocument(output.toObject()).toJson(QJsonDocument::Indented))
                                             : QString::fromUtf8(QJsonDocument(output.toArray()).toJson(QJsonDocument::Indented));
            if (text.size() > kErpRawChars)
                text = text.left(kErpRawChars) + QStringLiteral("\n… ") + erp_tr("(truncated — COPY has the full output)");
            raw->setPlainText(text);
        }
        raw->setVisible(show);
        btn->setText(show ? erp_tr("RAW ▾") : erp_tr("RAW ▸"));
    });
    return box;
}

QWidget* erp_result_card(const NodeExecutionResult& result, const QString& node_label, const QString& node_type) {
    const bool ok = result.success;
    // The executor marks a node that never ran (branch not taken / gate not passed) with _skipped.
    const bool skipped = ok && result.output.isObject() && result.output.toObject().value("_skipped").toBool(false);
    const QString state = !ok ? QStringLiteral("error") : skipped ? QStringLiteral("skipped") : QStringLiteral("ok");
    const QString badge = !ok ? erp_tr("FAILED") : skipped ? erp_tr("SKIPPED") : erp_tr("OK");
    const QString title = node_label.isEmpty() ? result.node_id.left(8) + "…" : node_label;

    auto* card = erp_make_card(state, badge, title, result.node_id, skipped ? QString() : erp_duration(result.duration_ms));
    auto* vl = qobject_cast<QVBoxLayout*>(card->layout());

    if (!ok) {
        auto* err = erp_label(result.error.isEmpty() ? erp_tr("Unknown error") : result.error, "resultError");
        err->setWordWrap(true);
        err->setTextInteractionFlags(Qt::TextSelectableByMouse);
        vl->addWidget(err);
        return card;
    }

    if (skipped) {
        // A node may say why it stopped the flow (a Price Alert whose condition is not met).
        const QString reason = result.output.toObject().value("reason").toString();
        auto* val = erp_label(reason.isEmpty() ? erp_tr("Did not run — an upstream branch or gate did not let data "
                                                        "through")
                                               : erp_tr("Not triggered — %1").arg(reason),
                              "resultDim");
        val->setWordWrap(true);
        vl->addWidget(val);
        return card;
    }

    // A trading result names its mode first so a paper fill can never be mistaken for a live one.
    if (result.output.isObject()) {
        const QString mode = result.output.toObject().value(QStringLiteral("mode")).toString();
        if (node_type.startsWith(QLatin1String("trading.")) && !mode.isEmpty()) {
            auto* m = erp_label(mode.toUpper() == QLatin1String("LIVE") ? erp_tr("LIVE — real broker order")
                                                                        : erp_tr("PAPER — simulated"),
                                "resultGate");
            m->setProperty("state", mode.toUpper() == QLatin1String("LIVE") ? "error" : "ok");
            vl->addWidget(m, 0, Qt::AlignLeft);
        }
    }

    vl->addWidget(erp_render_value(result.output, node_type, 1));
    if (result.output.isObject() && result.output.toObject().contains(QStringLiteral("exec_ms")))
        vl->addWidget(erp_label(
            erp_tr("Agent ran in %1").arg(erp_duration(result.output.toObject().value(QStringLiteral("exec_ms")).toInt())),
            "resultDim"));
    if ((result.output.isObject() && !result.output.toObject().isEmpty()) ||
        (result.output.isArray() && !result.output.toArray().isEmpty()))
        vl->addWidget(erp_raw_toggle(result.output));
    return card;
}

void erp_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ── ExecutionResultsPanel ──────────────────────────────────────────────────

ExecutionResultsPanel::ExecutionResultsPanel(QWidget* parent) : QWidget(parent) {
    setFixedHeight(erp_header_h() + 2); // header-only until execution starts (collapsed by default)
    setObjectName("executionResultsPanel");
    build_ui();
    hide();

    // The header height follows the font size.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        if (auto* header = findChild<QWidget*>(QStringLiteral("erpHeader")))
            header->setFixedHeight(erp_header_h());
        set_collapsed(collapsed_);
    });
}

void ExecutionResultsPanel::build_ui() {
    // Every row and label below is styled from this one sheet.
    setStyleSheet(
        QString("QWidget#erpHeader { background: %1; }"
                "QFrame#erpSep { background: %2; border: none; }"
                "QLabel { font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent; }"
                "QLabel#erpTitle { color: %3; font-weight: bold; letter-spacing: 0.5px; }"
                "QLabel#erpCounter { color: %4; }"
                "QLabel#erpCounter[errors=\"true\"] { color: %5; }"
                "QLabel#erpStatus { color: %4; font-weight: bold; }"
                "QLabel#erpStatus[state=\"running\"] { color: %3; }"
                "QLabel#erpStatus[state=\"done\"] { color: %6; }"
                "QLabel#erpStatus[state=\"failed\"] { color: %5; }"
                "QLabel#erpBanner { color: %7; padding: 4px 10px; font-weight: bold; border-bottom: 1px solid %2; }"
                "QLabel#erpBanner[live=\"true\"] { color: %5; }"
                "QPushButton#erpLinkBtn { background: transparent; color: %4; border: none;"
                "  font-size: " + fincept::ui::fonts::px(-4) + "px; }"
                "QPushButton#erpLinkBtn:hover { color: %8; }"
                "QWidget#erpResults { background: %9; }"
                "QWidget#resultCard { background: %10; border-left: 3px solid %2; border-bottom: 1px solid %2; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::DANGER(), ui::colors::SUCCESS(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED())
            .arg(ui::colors::BG_HOVER()) +
        QString("QWidget#resultCard[state=\"ok\"] { border-left: 3px solid %1; }"
                "QWidget#resultCard[state=\"error\"] { border-left: 3px solid %2; }"
                "QWidget#resultCard[state=\"running\"] { border-left: 3px solid %3; }"
                "QLabel#resultBadge { font-weight: bold; color: %4; }"
                "QLabel#resultBadge[state=\"ok\"] { color: %1; }"
                "QLabel#resultBadge[state=\"error\"] { color: %2; }"
                "QLabel#resultBadge[state=\"running\"] { color: %3; }"
                "QLabel#resultName { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: bold; }"
                "QLabel#resultDim { color: %4; font-size: " + fincept::ui::fonts::px(-5) + "px; }"
                "QLabel#resultError { color: %2; padding: 2px 0; }"
                "QLabel#resultText, QLabel#resultValue { color: %5; }"
                "QLabel#resultKey { color: %6; }"
                "QScrollArea#erpScroll { background: %7; border: none; }"
                "QScrollArea#erpScroll QScrollBar:vertical { background: %7; width: 5px; border: none; }"
                "QScrollArea#erpScroll QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                "QScrollArea#erpScroll QScrollBar::add-line:vertical,"
                "QScrollArea#erpScroll QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::SUCCESS(), ui::colors::DANGER(), ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_BRIGHT()) +
        // Rendered outputs: gate / mode badge, Markdown answer, RAW JSON.
        QString("QLabel#resultGate { font-weight: bold; padding: 1px 6px; border: 1px solid %3; color: %3; }"
                "QLabel#resultGate[state=\"ok\"] { color: %1; border-color: %1; }"
                "QLabel#resultGate[state=\"error\"] { color: %2; border-color: %2; }"
                "QTextBrowser#resultMarkdown { background: %4; border: 1px solid %5; }"
                "QPlainTextEdit#resultRaw { background: %4; color: %3; border: 1px solid %5;"
                "  font-size: " + fincept::ui::fonts::px(-4) + "px; }")
            .arg(ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_BASE(),
                 ui::colors::BORDER_DIM()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sep = new QFrame;
    sep->setObjectName("erpSep");
    sep->setFixedHeight(1);
    root->addWidget(sep);

    // ── Header bar ────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("erpHeader");
    header->setFixedHeight(erp_header_h());
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(8);

    collapse_btn_ = new QPushButton("▸");
    collapse_btn_->setObjectName("erpLinkBtn");
    collapse_btn_->setMinimumSize(18, 18);
    connect(collapse_btn_, &QPushButton::clicked, this, [this]() { set_collapsed(!collapsed_); });
    hl->addWidget(collapse_btn_);

    title_label_ = erp_label(QString(), "erpTitle");
    hl->addWidget(title_label_);
    node_counter_ = erp_label(QString(), "erpCounter");
    hl->addWidget(node_counter_);
    hl->addStretch();
    status_label_ = erp_label(QString(), "erpStatus");
    hl->addWidget(status_label_);

    auto* vsep = new QFrame;
    vsep->setObjectName("erpSep");
    vsep->setFixedSize(1, 16);
    hl->addWidget(vsep);

    clear_btn_ = new QPushButton;
    clear_btn_->setObjectName("erpLinkBtn");
    clear_btn_->setMinimumHeight(20);
    connect(clear_btn_, &QPushButton::clicked, this, &ExecutionResultsPanel::clear);
    hl->addWidget(clear_btn_);

    copy_btn_ = new QPushButton;
    copy_btn_->setObjectName("erpLinkBtn");
    copy_btn_->setMinimumHeight(20);
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(copy_buffer_);
        copy_btn_->setText(tr("COPIED!"));
        QTimer::singleShot(1500, copy_btn_, [this]() { copy_btn_->setText(tr("COPY")); });
    });
    hl->addWidget(copy_btn_);

    root->addWidget(header);

    mode_banner_ = erp_label(QString(), "erpBanner");
    mode_banner_->setWordWrap(true);
    mode_banner_->hide();
    root->addWidget(mode_banner_);

    // ── Scrollable results area ────────────────────────────────────
    scroll_area_ = new QScrollArea;
    scroll_area_->setObjectName("erpScroll");
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    results_container_ = new QWidget(this);
    results_container_->setObjectName("erpResults");
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(0);
    results_layout_->addStretch();

    scroll_area_->setWidget(results_container_);
    scroll_area_->hide(); // starts collapsed
    root->addWidget(scroll_area_, 1);

    retranslateUi();
}

void ExecutionResultsPanel::set_collapsed(bool collapsed) {
    collapsed_ = collapsed;
    collapse_btn_->setText(collapsed_ ? "▸" : "▾");
    scroll_area_->setVisible(!collapsed_);
    const int banner_h = mode_banner_->isVisible() ? mode_banner_->sizeHint().height() : 0;
    // Expanded: room for a rendered result (KPIs + chart) without scrolling at once.
    setFixedHeight(collapsed_ ? erp_header_h() + 2 + banner_h : 360);
}

void ExecutionResultsPanel::clear() {
    while (results_layout_->count() > 1) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    running_cards_.clear();
    node_count_ = 0;
    error_count_ = 0;
    skipped_count_ = 0;
    copy_buffer_.clear();
    run_state_ = RunState::Idle;
    refresh_node_counter();
    refresh_status_label();
}

void ExecutionResultsPanel::set_started(const QString& workflow_id) {
    Q_UNUSED(workflow_id);
    clear();
    show();
    set_collapsed(false); // auto-expand when execution starts
    run_state_ = RunState::Running;
    refresh_status_label();
}

void ExecutionResultsPanel::set_mode_banner(const QString& text, bool live) {
    mode_banner_->setText(text);
    mode_banner_->setVisible(!text.isEmpty());
    mode_banner_->setProperty("live", live);
    erp_repolish(mode_banner_);
    if (!text.isEmpty())
        copy_buffer_ += text + "\n\n";
}

void ExecutionResultsPanel::insert_card(QWidget* card, int pos) {
    if (pos < 0 || pos > results_layout_->count() - 1)
        pos = results_layout_->count() - 1; // before trailing stretch
    results_layout_->insertWidget(pos, card);
}

void ExecutionResultsPanel::scroll_to_bottom() {
    QTimer::singleShot(0, scroll_area_, [this]() {
        scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void ExecutionResultsPanel::add_node_running(const QString& node_id, const QString& node_label) {
    if (running_cards_.contains(node_id))
        return;
    auto* card = erp_make_card(QStringLiteral("running"), tr("RUNNING"),
                               node_label.isEmpty() ? node_id.left(8) + "…" : node_label, node_id, QString());
    insert_card(card);
    running_cards_.insert(node_id, card);
    scroll_to_bottom();
}

void ExecutionResultsPanel::add_node_result(const NodeExecutionResult& result, const QString& node_label,
                                            const QString& node_type) {
    ++node_count_;
    const bool skipped =
        result.success && result.output.isObject() && result.output.toObject().value("_skipped").toBool(false);
    if (!result.success)
        ++error_count_;
    else if (skipped)
        ++skipped_count_;
    refresh_node_counter();

    // Plain-text copy of the log
    copy_buffer_ += QString("[%1] %2  %3\n")
                        .arg(!result.success ? "FAILED" : skipped ? "SKIPPED" : "OK")
                        .arg(node_label.isEmpty() ? result.node_id.left(8) : node_label)
                        .arg(erp_duration(result.duration_ms));
    if (!result.success) {
        copy_buffer_ += result.error + "\n";
    } else if (result.output.isObject() && !skipped) {
        auto obj = result.output.toObject();
        if (obj.contains("response")) {
            QString r = obj.value("response").toString();
            r.replace("\\n", "\n");
            copy_buffer_ += r + "\n";
        } else {
            copy_buffer_ += QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)) + "\n";
        }
    } else if (!skipped) {
        copy_buffer_ += erp_value(result.output) + "\n";
    }
    copy_buffer_ += "\n";

    // Replace the node's RUNNING row in place, so the log keeps launch order.
    int pos = -1;
    if (QWidget* running = running_cards_.take(result.node_id)) {
        pos = results_layout_->indexOf(running);
        results_layout_->removeWidget(running);
        running->deleteLater();
    }
    insert_card(erp_result_card(result, node_label, node_type), pos);
    scroll_to_bottom();
}

void ExecutionResultsPanel::set_finished(const WorkflowExecutionResult& result) {
    last_total_ms_ = result.total_duration_ms;
    if (result.success) {
        run_state_ = RunState::Done;
    } else if (result.error.startsWith("Execution stopped")) {
        run_state_ = RunState::Stopped;
        last_failure_.clear();
    } else {
        run_state_ = RunState::Failed;
        last_failure_ = result.error;
    }
    // A run that ended before anything launched (cycle, refused) explains itself in the log.
    if (!result.success && node_count_ == 0 && running_cards_.isEmpty() && !result.error.isEmpty()) {
        NodeExecutionResult nr;
        nr.success = false;
        nr.error = result.error;
        insert_card(erp_result_card(nr, tr("Workflow"), QString()));
    }
    refresh_status_label();
    copy_buffer_ += tr("Result: %1 in %2").arg(result.success ? tr("completed") : tr("failed: %1").arg(result.error),
                                                erp_duration(result.total_duration_ms)) +
                    "\n";
}

// Re-render the node/error counter for the current counts.
void ExecutionResultsPanel::refresh_node_counter() {
    if (!node_counter_)
        return;
    if (node_count_ == 0) {
        node_counter_->setText("");
    } else {
        QString counter = tr("%n node(s)", "", node_count_);
        if (error_count_ > 0)
            counter += "  " + tr("%n failed", "", error_count_);
        if (skipped_count_ > 0)
            counter += "  " + tr("%n skipped", "", skipped_count_);
        node_counter_->setText(counter);
    }
    node_counter_->setProperty("errors", error_count_ > 0);
    erp_repolish(node_counter_);
}

// Re-render the status badge for the current run state.
void ExecutionResultsPanel::refresh_status_label() {
    if (!status_label_)
        return;
    QString text;
    QString state;
    switch (run_state_) {
        case RunState::Idle:
            text = tr("IDLE");
            break;
        case RunState::Running:
            text = tr("RUNNING…");
            state = QStringLiteral("running");
            break;
        case RunState::Done:
            text = tr("DONE  %1").arg(erp_duration(last_total_ms_));
            state = QStringLiteral("done");
            break;
        case RunState::Stopped:
            text = tr("STOPPED");
            state = QStringLiteral("failed");
            break;
        case RunState::Failed:
            text = tr("FAILED  %1").arg(last_failure_.left(160));
            state = QStringLiteral("failed");
            break;
    }
    status_label_->setText(text);
    status_label_->setToolTip(run_state_ == RunState::Failed ? last_failure_ : QString());
    status_label_->setProperty("state", state);
    erp_repolish(status_label_);
}

void ExecutionResultsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ExecutionResultsPanel::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("RUN LOG"));
    if (collapse_btn_)
        collapse_btn_->setToolTip(tr("Expand / Collapse"));
    if (clear_btn_)
        clear_btn_->setText(tr("CLEAR"));
    if (copy_btn_) {
        copy_btn_->setText(tr("COPY"));
        copy_btn_->setToolTip(tr("Copy the run log to the clipboard"));
    }
    refresh_status_label();
    refresh_node_counter();
    // Result cards are data-driven snapshots of past executions — left as rendered.
}

} // namespace fincept::workflow
