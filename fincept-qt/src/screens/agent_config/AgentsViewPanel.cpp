// src/screens/agent_config/AgentsViewPanel.cpp
#include "screens/agent_config/AgentsViewPanel.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentProfileRows.h"
#include "screens/agent_config/AgentResultFormat.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/Gauge.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/Card.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

static QLabel* section_hdr(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding:6px 0 2px 0;")
                           .arg(ui::colors::AMBER()));
    return lbl;
}

// Status-line styling in one place (was eight identical inline setStyleSheet pairs).
static void set_status(QLabel* lbl, const QString& text, const QString& color) {
    lbl->setText(text);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(color));
}

static QString input_ss() {
    return QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:" + fincept::ui::fonts::px(-2) + "px;")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED());
}

// Default stylesheet of the result document (rendered markdown / HTML).
static QString result_doc_css() {
    return QString("body { color: %1; background: %2; font-size: " + fincept::ui::fonts::px(-2) + "px; }"
                   "h1, h2, h3, h4 { color: %3; margin: 8px 0 4px 0; }"
                   "h1 { font-size: " + fincept::ui::fonts::px(2) + "px; border-bottom: 1px solid %4; padding-bottom: 4px; }"
                   "h2 { font-size: " + fincept::ui::fonts::px(0) + "px; }"
                   "h3 { font-size: " + fincept::ui::fonts::px(-1) + "px; color: %5; }"
                   "code { background: %6; color: %7; padding: 1px 4px; border-radius: 3px; }"
                   "pre  { background: %6; color: %7; padding: 8px; border-radius: 4px; }"
                   "blockquote { border-left: 3px solid %3; margin: 4px 0; padding-left: 10px; color: %8; }"
                   "a { color: %5; }"
                   "ul, ol { margin: 4px 0; padding-left: 20px; }"
                   "li { margin: 2px 0; }"
                   "strong { color: %1; }"
                   "hr { border: none; border-top: 1px solid %4; }")
        .arg(ui::colors::TEXT_PRIMARY(),  // body color          %1
             ui::colors::BG_BASE(),       // body background     %2
             ui::colors::AMBER(),         // h1/h2 + blockquote  %3
             ui::colors::BORDER_DIM(),    // hr / h1 border      %4
             ui::colors::CYAN(),          // h3 / links          %5
             ui::colors::BG_RAISED(),     // code background     %6
             ui::colors::TEXT_PRIMARY(),  // code color          %7
             ui::colors::TEXT_SECONDARY() // blockquote text     %8
        );
}

// ── Structured output view ──────────────────────────────────────────────────
// run_structured returns one of six typed models (scripts/agents/finagent_core/
// modules/output_models.py). Each gets a laid-out page: KPI tiles for the
// numbers, a chart where the model carries a distribution or score, tables for
// row data and rendered markdown for the prose. Optional numbers the model did
// not supply are null and show the placeholder, never 0.
namespace {
namespace sv {

namespace fmtx = fincept::ui::formatting;
using Tone = ui::StatTile::Tone;

QString tr_sv(const char* text) {
    return QCoreApplication::translate("fincept::screens::AgentsViewPanel", text);
}

/// The field as a number, NaN when missing / null / not a number.
double num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) { // models sometimes quote numbers
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        if (ok)
            return d;
    }
    return std::nan("");
}

QString str(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toString().trimmed();
}

/// "strong_buy" -> "STRONG BUY"; empty -> placeholder.
QString enum_text(const QString& v) {
    if (v.isEmpty())
        return fmtx::placeholder();
    QString s = v;
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    return s.toUpper();
}

/// "key_levels" -> "Key levels".
QString humanize(QString key) {
    key.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!key.isEmpty())
        key[0] = key[0].toUpper();
    return key;
}

/// Buy-side actions / bullish sentiment positive, sell-side / bearish negative.
Tone tone_of_word(const QString& v) {
    const QString s = v.toLower();
    if (s.contains(QLatin1String("buy")) || s.contains(QLatin1String("bull")) ||
        s.contains(QLatin1String("up")) || s == QLatin1String("outperform"))
        return Tone::Positive;
    if (s.contains(QLatin1String("sell")) || s.contains(QLatin1String("bear")) ||
        s.contains(QLatin1String("down")) || s == QLatin1String("underperform"))
        return Tone::Negative;
    return Tone::Neutral;
}

/// Risk level words: high / very_high negative, medium warning, low positive.
Tone tone_of_risk(const QString& v) {
    const QString s = v.toLower();
    if (s.contains(QLatin1String("high")) || s.contains(QLatin1String("extreme")))
        return Tone::Negative;
    if (s.contains(QLatin1String("medium")) || s.contains(QLatin1String("moderate")))
        return Tone::Warning;
    if (s.contains(QLatin1String("low")))
        return Tone::Positive;
    return Tone::Neutral;
}

Tone tone_of_sign(double v) {
    if (!std::isfinite(v) || v == 0.0)
        return Tone::Neutral;
    return v > 0 ? Tone::Positive : Tone::Negative;
}

QString n2(double v) {
    return fmtx::number(v, 2);
}
QString pct_signed(double v) {
    return fmtx::pct(v, 2, true);
}
[[maybe_unused]] QString pct_plain(double v) {
    return fmtx::pct(v, 1, false);
}

struct Builder {
    QWidget* root = nullptr;
    QVBoxLayout* lay = nullptr;

    explicit Builder(QWidget* parent) {
        root = new QWidget(parent);
        lay = new QVBoxLayout(root);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
    }

    void title(const QString& text, const QString& sub = {}) {
        if (text.isEmpty())
            return;
        auto* lbl = new QLabel(root);
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setText(QStringLiteral("<span style='color:%1;font-size:%2px;font-weight:700;'>%3</span>%4")
                         .arg(ui::colors::TEXT_PRIMARY())
                         .arg(ui::fonts::px(2))
                         .arg(text.toHtmlEscaped(),
                              sub.isEmpty() ? QString()
                                            : QStringLiteral("&nbsp;&nbsp;<span style='color:%1;font-size:%2px;'>%3</span>")
                                                  .arg(ui::colors::TEXT_SECONDARY())
                                                  .arg(ui::fonts::px(-2))
                                                  .arg(sub.toHtmlEscaped())));
        lay->addWidget(lbl);
    }

    ui::KpiStrip* strip() {
        auto* s = new ui::KpiStrip(root);
        lay->addWidget(s);
        return s;
    }

    static ui::StatTile* tile(ui::KpiStrip* s, const QString& label, const QString& value, Tone tone = Tone::Neutral,
                              const QString& sub = {}) {
        auto* t = s->add_tile(label);
        t->set_value(value.isEmpty() ? fmtx::placeholder() : value);
        t->set_tone(tone);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        return t;
    }

    void markdown(const QString& heading, const QString& md) {
        if (md.trimmed().isEmpty())
            return;
        auto* card = new ui::Card(heading, root);
        auto* body = new QLabel(card);
        body->setTextFormat(Qt::RichText);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setText(ui::MarkdownRenderer::render(md));
        card->content_layout()->addWidget(body);
        QObject::connect(card, &ui::Card::close_requested, card, &QWidget::hide);
        lay->addWidget(card);
    }

    void bullets(const QString& heading, const QJsonValue& list) {
        QString md;
        for (const auto& v : list.toArray()) {
            QString s;
            if (v.isString())
                s = v.toString();
            else if (v.isDouble())
                s = n2(v.toDouble());
            else if (v.isObject())
                s = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
            else if (v.isArray())
                s = QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
            if (!s.trimmed().isEmpty())
                md += QStringLiteral("- ") + s.trimmed() + QLatin1Char('\n');
        }
        markdown(heading, md);
    }

    /// A sortable table of an array of objects (union of keys as columns) or
    /// of a key -> value object (two columns). Numbers are right-aligned
    /// NumericTableWidgetItems.
    void table(const QString& heading, const QJsonValue& data, const QString& key_header = {},
               const QString& value_header = {}) {
        QStringList cols;
        QVector<QJsonObject> rows;
        if (data.isArray()) {
            for (const auto& v : data.toArray()) {
                const QJsonObject o = v.toObject();
                if (o.isEmpty())
                    continue;
                for (auto it = o.begin(); it != o.end(); ++it)
                    if (!cols.contains(it.key()))
                        cols << it.key();
                rows << o;
            }
        } else if (data.isObject()) {
            const QJsonObject o = data.toObject();
            const QString k = key_header.isEmpty() ? tr_sv("Item") : key_header;
            const QString v = value_header.isEmpty() ? tr_sv("Value") : value_header;
            cols << k << v;
            for (auto it = o.begin(); it != o.end(); ++it)
                rows << QJsonObject{{k, humanize(it.key())}, {v, it.value()}};
        }
        if (rows.isEmpty())
            return;
        auto* card = new ui::Card(heading, root);
        auto* t = new QTableWidget(int(rows.size()), int(cols.size()), card);
        QStringList headers;
        for (const QString& c : cols)
            headers << (data.isArray() ? humanize(c) : c);
        t->setHorizontalHeaderLabels(headers);
        t->verticalHeader()->setVisible(false);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setShowGrid(false);
        t->setAlternatingRowColors(true);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setStyleSheet(QString("QTableWidget{background:%1;alternate-background-color:%2;color:%3;border:none;"
                                 "font-size:%4px;}"
                                 "QHeaderView::section{background:%5;color:%6;border:none;padding:4px 6px;"
                                 "font-size:%7px;font-weight:700;}")
                             .arg(ui::colors::BG_BASE(), ui::colors::ROW_ALT(), ui::colors::TEXT_PRIMARY())
                             .arg(ui::fonts::px(-2))
                             .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY())
                             .arg(ui::fonts::px(-4)));
        for (int r = 0; r < rows.size(); ++r) {
            for (int c = 0; c < cols.size(); ++c) {
                const QJsonValue v = rows[r].value(cols[c]);
                QTableWidgetItem* item = nullptr;
                if (v.isDouble()) {
                    item = new ui::NumericTableWidgetItem(n2(v.toDouble()), v.toDouble());
                    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                } else if (v.isString()) {
                    bool ok = false;
                    const double d = v.toString().toDouble(&ok);
                    item = ok ? new ui::NumericTableWidgetItem(v.toString(), d) : new QTableWidgetItem(v.toString());
                    if (ok)
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                } else if (v.isBool()) {
                    item = new QTableWidgetItem(v.toBool() ? tr_sv("Yes") : tr_sv("No"));
                } else if (v.isNull() || v.isUndefined()) {
                    item = new QTableWidgetItem(fmtx::placeholder());
                } else {
                    const QJsonDocument doc = v.isArray() ? QJsonDocument(v.toArray()) : QJsonDocument(v.toObject());
                    item = new QTableWidgetItem(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
                }
                t->setItem(r, c, item);
            }
        }
        t->resizeColumnsToContents();
        t->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        t->setSortingEnabled(true);
        // Show every row (the page scrolls, not the table).
        const int h = t->horizontalHeader()->sizeHint().height() + t->verticalHeader()->length() + 4;
        t->setMinimumHeight(std::min(h, 420));
        card->content_layout()->addWidget(t);
        QObject::connect(card, &ui::Card::close_requested, card, &QWidget::hide);
        lay->addWidget(card);
    }

    /// A donut of a {name: weight} object.
    ui::DonutChart* donut(const QJsonValue& alloc, const QString& center_title) {
        QVector<ui::DonutSlice> slices;
        const QJsonObject o = alloc.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            const double w = it.value().toDouble();
            if (w > 0)
                slices.append({it.key(), humanize(it.key()), w, QColor()});
        }
        if (slices.isEmpty())
            return nullptr;
        auto* d = new ui::DonutChart(root);
        d->set_slices(slices);
        d->set_center_text(center_title);
        d->setMinimumHeight(200);
        return d;
    }
};

QWidget* build_trade_signal(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(str(d, "symbol"), tr_sv("Trade signal"));
    const QString action = str(d, "action");
    const double entry = num(d, "entry_price");
    const double stop = num(d, "stop_loss");
    const double target = num(d, "take_profit");
    auto* s = b.strip();
    Builder::tile(s, tr_sv("ACTION"), enum_text(action), tone_of_word(action));
    Builder::tile(s, tr_sv("CONFIDENCE"), fmtx::fraction_pct(num(d, "confidence"), 0, false));
    Builder::tile(s, tr_sv("ENTRY"), fmtx::price(entry));
    auto from_entry = [entry](double px) {
        return std::isfinite(entry) && entry > 0 && std::isfinite(px)
                   ? tr_sv("%1 from entry").arg(fmtx::pct((px - entry) / entry * 100.0, 2, true))
                   : QString();
    };
    Builder::tile(s, tr_sv("STOP LOSS"), fmtx::price(stop), Tone::Negative, from_entry(stop));
    Builder::tile(s, tr_sv("TAKE PROFIT"), fmtx::price(target), Tone::Positive, from_entry(target));
    Builder::tile(s, tr_sv("QTY"), fmtx::quantity(num(d, "quantity")));
    Builder::tile(s, tr_sv("HORIZON"), enum_text(str(d, "time_horizon")));
    Builder::tile(s, tr_sv("RISK"), enum_text(str(d, "risk_level")), tone_of_risk(str(d, "risk_level")));
    b.markdown(tr_sv("REASONING"), str(d, "reasoning"));
    b.lay->addStretch();
    return b.root;
}

QWidget* build_stock_analysis(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(str(d, "symbol"), str(d, "company_name"));
    const QString rec = str(d, "recommendation");
    const double upside = num(d, "upside_potential");
    auto* s1 = b.strip();
    Builder::tile(s1, tr_sv("CURRENT PRICE"), fmtx::price(num(d, "current_price")));
    Builder::tile(s1, tr_sv("TARGET PRICE"), fmtx::price(num(d, "target_price")));
    Builder::tile(s1, tr_sv("UPSIDE"), pct_signed(upside), tone_of_sign(upside));
    Builder::tile(s1, tr_sv("RECOMMENDATION"), enum_text(rec), tone_of_word(rec), str(d, "analyst_rating"));
    Builder::tile(s1, tr_sv("SENTIMENT"), enum_text(str(d, "sentiment")), tone_of_word(str(d, "sentiment")));
    auto* s2 = b.strip();
    Builder::tile(s2, tr_sv("P/E"), fmtx::ratio(num(d, "pe_ratio")));
    Builder::tile(s2, tr_sv("P/B"), fmtx::ratio(num(d, "pb_ratio")));
    Builder::tile(s2, tr_sv("P/S"), fmtx::ratio(num(d, "ps_ratio")));
    Builder::tile(s2, tr_sv("EV/EBITDA"), fmtx::ratio(num(d, "ev_ebitda")));
    const double rev_g = num(d, "revenue_growth");
    const double eps_g = num(d, "earnings_growth");
    Builder::tile(s2, tr_sv("REV GROWTH"), pct_signed(rev_g), tone_of_sign(rev_g));
    Builder::tile(s2, tr_sv("EPS GROWTH"), pct_signed(eps_g), tone_of_sign(eps_g));
    Builder::tile(s2, tr_sv("RSI"), fmtx::number(num(d, "rsi"), 1));
    Builder::tile(s2, tr_sv("TREND"), enum_text(str(d, "trend")), tone_of_word(str(d, "trend")),
                  str(d, "macd_signal").isEmpty() ? QString() : tr_sv("MACD %1").arg(str(d, "macd_signal")));
    b.markdown(tr_sv("SUMMARY"), str(d, "summary"));
    b.bullets(tr_sv("KEY RISKS"), d.value(QLatin1String("key_risks")));
    b.bullets(tr_sv("KEY CATALYSTS"), d.value(QLatin1String("key_catalysts")));
    b.lay->addStretch();
    return b.root;
}

QWidget* build_portfolio_analysis(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(tr_sv("Portfolio analysis"));
    auto* s1 = b.strip();
    Builder::tile(s1, tr_sv("TOTAL VALUE"), n2(num(d, "total_value")));
    Builder::tile(s1, tr_sv("CASH"), n2(num(d, "cash_balance")));
    Builder::tile(s1, tr_sv("INVESTED"), n2(num(d, "invested_value")));
    Builder::tile(s1, tr_sv("RISK LEVEL"), enum_text(str(d, "risk_level")), tone_of_risk(str(d, "risk_level")));
    Builder::tile(s1, tr_sv("DIVERSIFICATION"), fmtx::number(num(d, "diversification_score"), 0),
                  Tone::Neutral, tr_sv("score 0-100"));
    auto* s2 = b.strip();
    const struct {
        const char* key;
        const char* label;
    } rets[] = {{"daily_return", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "DAY")},
                {"weekly_return", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "WEEK")},
                {"monthly_return", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "MTD")},
                {"ytd_return", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "YTD")},
                {"total_return", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "TOTAL RETURN")}};
    for (const auto& r : rets) {
        const double v = num(d, r.key);
        Builder::tile(s2, tr_sv(r.label), pct_signed(v), tone_of_sign(v));
    }
    auto* s3 = b.strip();
    Builder::tile(s3, tr_sv("SHARPE"), n2(num(d, "sharpe_ratio")));
    Builder::tile(s3, tr_sv("SORTINO"), n2(num(d, "sortino_ratio")));
    const double mdd = num(d, "max_drawdown");
    Builder::tile(s3, tr_sv("MAX DRAWDOWN"), std::isfinite(mdd) ? fmtx::pct(-std::fabs(mdd), 2, true) : QString(),
                  Tone::Negative);
    Builder::tile(s3, tr_sv("BETA"), n2(num(d, "beta")));
    Builder::tile(s3, tr_sv("VOLATILITY"), n2(num(d, "volatility")));
    Builder::tile(s3, tr_sv("VAR 95"), n2(num(d, "var_95")));

    auto* sector = b.donut(d.value(QLatin1String("sector_allocation")), tr_sv("Sector"));
    auto* asset = b.donut(d.value(QLatin1String("asset_allocation")), tr_sv("Asset"));
    if (sector || asset) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        if (sector)
            row->addWidget(sector, 1);
        if (asset)
            row->addWidget(asset, 1);
        b.lay->addLayout(row);
    }
    b.table(tr_sv("TOP HOLDINGS"), d.value(QLatin1String("top_holdings")));
    b.bullets(tr_sv("RECOMMENDATIONS"), d.value(QLatin1String("recommendations")));
    b.lay->addStretch();
    return b.root;
}

QWidget* build_risk_assessment(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(tr_sv("Risk assessment"));
    const QString overall = str(d, "overall_risk");

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* gauge = new ui::Gauge(b.root);
    gauge->set_range(0, 100);
    gauge->set_zones({{0, 33, ui::GaugeZone::Tone::Good, tr_sv("Low")},
                      {33, 66, ui::GaugeZone::Tone::Caution, tr_sv("Medium")},
                      {66, 100, ui::GaugeZone::Tone::Bad, tr_sv("High")}});
    gauge->set_value(num(d, "risk_score"));
    gauge->set_caption(overall.isEmpty() ? tr_sv("Risk score") : enum_text(overall));
    gauge->setMinimumHeight(150);
    row->addWidget(gauge, 1);

    QStringList cats;
    QVector<double> vals;
    const struct {
        const char* key;
        const char* label;
    } parts[] = {{"market_risk", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "Market")},
                 {"credit_risk", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "Credit")},
                 {"liquidity_risk", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "Liquidity")},
                 {"concentration_risk", QT_TRANSLATE_NOOP("fincept::screens::AgentsViewPanel", "Concentration")}};
    for (const auto& p : parts) {
        cats << tr_sv(p.label);
        vals << num(d, p.key); // NaN = no bar
    }
    auto* bars = new ui::BarChart(b.root);
    bars->set_orientation(Qt::Horizontal);
    bars->set_data(cats, vals, tr_sv("Risk (0-100)"));
    bars->set_show_values(true);
    bars->set_value_formatter([](double v) { return fmtx::number(v, 0); });
    bars->setMinimumHeight(150);
    row->addWidget(bars, 2);
    b.lay->addLayout(row);

    auto* s = b.strip();
    Builder::tile(s, tr_sv("OVERALL RISK"), enum_text(overall), tone_of_risk(overall));
    Builder::tile(s, tr_sv("VAR 1D 95%"), n2(num(d, "var_1d_95")));
    Builder::tile(s, tr_sv("VAR 1D 99%"), n2(num(d, "var_1d_99")));
    Builder::tile(s, tr_sv("EXPECTED SHORTFALL"), n2(num(d, "expected_shortfall")));
    b.table(tr_sv("STRESS SCENARIOS"), d.value(QLatin1String("stress_scenarios")));
    b.bullets(tr_sv("RISK FACTORS"), d.value(QLatin1String("risk_factors")));
    b.bullets(tr_sv("MITIGATION"), d.value(QLatin1String("mitigation_strategies")));
    b.lay->addStretch();
    return b.root;
}

QWidget* build_market_analysis(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(str(d, "market"), tr_sv("Market analysis"));
    const double chg = num(d, "change_pct");
    auto* s = b.strip();
    Builder::tile(s, tr_sv("LEVEL"), n2(num(d, "current_level")));
    Builder::tile(s, tr_sv("CHANGE"), pct_signed(chg), tone_of_sign(chg));
    Builder::tile(s, tr_sv("TREND"), enum_text(str(d, "trend")), tone_of_word(str(d, "trend")),
                  str(d, "trend_strength"));
    Builder::tile(s, tr_sv("SENTIMENT"), enum_text(str(d, "sentiment")), tone_of_word(str(d, "sentiment")));
    Builder::tile(s, tr_sv("FEAR / GREED"), fmtx::number(num(d, "fear_greed_index"), 0), Tone::Neutral,
                  tr_sv("0 fear · 100 greed"));
    const double adv = num(d, "advancing_stocks");
    const double dec = num(d, "declining_stocks");
    Builder::tile(s, tr_sv("ADV / DEC"),
                  std::isfinite(adv) || std::isfinite(dec)
                      ? fmtx::quantity(adv) + QStringLiteral(" / ") + fmtx::quantity(dec)
                      : QString(),
                  std::isfinite(adv) && std::isfinite(dec) ? tone_of_sign(adv - dec) : Tone::Neutral);
    const double hi = num(d, "new_highs");
    const double lo = num(d, "new_lows");
    Builder::tile(s, tr_sv("HIGHS / LOWS"),
                  std::isfinite(hi) || std::isfinite(lo)
                      ? fmtx::quantity(hi) + QStringLiteral(" / ") + fmtx::quantity(lo)
                      : QString());

    const QJsonObject sectors = d.value(QLatin1String("sector_performance")).toObject();
    if (!sectors.isEmpty()) {
        QStringList cats;
        QVector<double> vals;
        for (auto it = sectors.begin(); it != sectors.end(); ++it) {
            cats << humanize(it.key());
            vals << it.value().toDouble(std::nan(""));
        }
        auto* card = new ui::Card(tr_sv("SECTOR PERFORMANCE"), b.root);
        auto* bars = new ui::BarChart(card);
        bars->set_orientation(Qt::Horizontal);
        bars->set_signed_colors(true);
        bars->set_show_values(true);
        bars->set_data(cats, vals, tr_sv("Change %"));
        bars->set_value_formatter([](double v) { return fmtx::pct(v, 2, true); });
        bars->setMinimumHeight(std::max(160, 24 * int(cats.size())));
        card->content_layout()->addWidget(bars);
        QObject::connect(card, &ui::Card::close_requested, card, &QWidget::hide);
        b.lay->addWidget(card);
    }
    b.markdown(tr_sv("SHORT-TERM OUTLOOK"), str(d, "short_term_outlook"));
    b.table(tr_sv("KEY LEVELS"), d.value(QLatin1String("key_levels")), tr_sv("Level"), tr_sv("Value"));
    b.bullets(tr_sv("LEADING SECTORS"), d.value(QLatin1String("leading_sectors")));
    b.bullets(tr_sv("LAGGING SECTORS"), d.value(QLatin1String("lagging_sectors")));
    b.bullets(tr_sv("CATALYSTS"), d.value(QLatin1String("catalysts")));
    b.lay->addStretch();
    return b.root;
}

QWidget* build_research_report(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.title(str(d, "title"), str(d, "symbol"));
    const QString rec = str(d, "recommendation");
    auto* s = b.strip();
    Builder::tile(s, tr_sv("RECOMMENDATION"), enum_text(rec), tone_of_word(rec));
    Builder::tile(s, tr_sv("TARGET PRICE"), fmtx::price(num(d, "target_price")));
    Builder::tile(s, tr_sv("CONFIDENCE"), fmtx::fraction_pct(num(d, "confidence"), 0, false));
    Builder::tile(s, tr_sv("REPORT"), enum_text(str(d, "report_type")), Tone::Neutral, str(d, "date"));
    QString md;
    auto section = [&](const QString& heading, const char* key) {
        const QString text = str(d, key);
        if (!text.isEmpty())
            md += QStringLiteral("## ") + heading + QStringLiteral("\n\n") + text + QStringLiteral("\n\n");
    };
    section(tr_sv("Executive summary"), "executive_summary");
    section(tr_sv("Investment thesis"), "investment_thesis");
    section(tr_sv("Bull case"), "bull_case");
    section(tr_sv("Bear case"), "bear_case");
    b.markdown(tr_sv("REPORT"), md);
    b.bullets(tr_sv("KEY FINDINGS"), d.value(QLatin1String("key_findings")));
    b.bullets(tr_sv("RISKS"), d.value(QLatin1String("risks")));
    b.bullets(tr_sv("DATA SOURCES"), d.value(QLatin1String("data_sources")));
    b.bullets(tr_sv("DISCLAIMERS"), d.value(QLatin1String("disclaimers")));
    b.lay->addStretch();
    return b.root;
}

/// Any other shape: every field as a key / value row (never a blank page).
QWidget* build_generic(const QJsonObject& d, QWidget* parent) {
    Builder b(parent);
    b.table(tr_sv("RESULT"), d, tr_sv("Field"), tr_sv("Value"));
    b.lay->addStretch();
    return b.root;
}

QWidget* build(const QString& model, const QJsonObject& d, bool valid, QWidget* parent) {
    QWidget* page = nullptr;
    if (model == QLatin1String("trade_signal"))
        page = build_trade_signal(d, parent);
    else if (model == QLatin1String("stock_analysis"))
        page = build_stock_analysis(d, parent);
    else if (model == QLatin1String("portfolio_analysis"))
        page = build_portfolio_analysis(d, parent);
    else if (model == QLatin1String("risk_assessment"))
        page = build_risk_assessment(d, parent);
    else if (model == QLatin1String("market_analysis"))
        page = build_market_analysis(d, parent);
    else if (model == QLatin1String("research_report"))
        page = build_research_report(d, parent);
    else
        page = build_generic(d, parent);
    if (!valid) {
        // Keep the validation note above the view, as the JSON answer had it.
        auto* lay = qobject_cast<QVBoxLayout*>(page->layout());
        auto* note = new QLabel(tr_sv("Note: the output did not pass validation against the %1 model. Fields may "
                                      "be missing or out of range.")
                                    .arg(model),
                                page);
        note->setWordWrap(true);
        note->setStyleSheet(QString("color:%1;background:%2;border:1px solid %1;padding:6px 8px;font-size:%3px;")
                                .arg(ui::colors::WARNING(), ui::colors::alpha(ui::colors::WARNING, 0.10))
                                .arg(ui::fonts::px(-2)));
        if (lay)
            lay->insertWidget(0, note);
    }
    return page;
}

} // namespace sv
} // namespace

AgentsViewPanel::AgentsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgentsViewPanel");
    build_ui();
    setup_connections();
    // The result document keeps its own stylesheet, built from the tokens.
    // Rendered markdown and the structured page carry the theme's colours in
    // their HTML / stylesheets: re-render both from their sources, once the
    // appearance change has settled (the structured page is rebuilt — never
    // inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        result_display_->document()->setDefaultStyleSheet(result_doc_css());
        agent_result::rerender(result_display_);
        if (!structured_model_.isEmpty())
            rebuild_structured_view();
    });

    // Seed from cache immediately — handles case where agents_discovered was
    // already emitted before this panel was lazily constructed.
    auto& svc = services::AgentService::instance();
    auto cached = svc.cached_agents();
    if (!cached.isEmpty()) {
        all_agents_ = cached;
        on_category_changed(0);
    }
}

void AgentsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Cold cache: the table skeleton until the local discovery answers (a warm
    // cache answers synchronously from inside discover_agents()).
    discovering_ = all_agents_.isEmpty();
    if (discovering_)
        discover_error_.clear();
    show_list_state();
    services::AgentService::instance().discover_agents();
}

// ── Left: agent list ──────────────────────────────────────────────────────────

QWidget* AgentsViewPanel::build_agent_list_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(200);
    panel->setMaximumWidth(300);
    // Scoped to the panel: the old selector-less sheet drew its border-right on
    // every child (labels, the list states).
    panel->setObjectName(QStringLiteral("agentsListPanel"));
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(QString("QWidget#agentsListPanel{background:%1;border-right:1px solid %2;}")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    list_title_ = new QLabel(tr("AGENTS"));
    list_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hdr->addWidget(list_title_);
    list_count_label_ = new QLabel("0");
    list_count_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;padding:1px 6px;border-radius:2px;")
                                         .arg(ui::colors::CYAN(), ui::colors::BG_RAISED()));
    hdr->addWidget(list_count_label_);
    hdr->addStretch();
    vl->addLayout(hdr);

    category_combo_ = new QComboBox;
    category_combo_->addItem(tr("All Categories"), "");
    category_combo_->setStyleSheet(
        QString("QComboBox{%1}QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%4;selection-color:%5;}")
            .arg(input_ss(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(),
                 ui::colors::TEXT_ON_ACCENT()));
    vl->addWidget(category_combo_);

    // The list, or what stands in for it: a table skeleton while a cold
    // discovery runs (local Python, a few seconds), a state when it failed or
    // found nothing. Every page is built in the stack (its final parent).
    list_stack_ = new QStackedWidget(panel);
    agent_list_ = new QListWidget(list_stack_);
    agent_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                                       "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                       "QListWidget::item:selected{background:%4;color:%6;}"
                                       "QListWidget::item:hover{background:%5;}")
                                   .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                                        ui::colors::AMBER_DIM(), ui::colors::BG_HOVER(), ui::colors::TEXT_ON_ACCENT()));
    list_stack_->addWidget(agent_list_);
    list_skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, list_stack_);
    list_skeleton_->set_row_count(8);
    list_skeleton_->set_message(tr("Discovering agents…"));
    list_stack_->addWidget(list_skeleton_);
    list_state_ = new ui::EmptyState(list_stack_);
    list_state_->set_compact(true);
    list_state_->set_alignment(Qt::AlignTop);
    list_stack_->addWidget(list_state_);
    connect(list_state_, &ui::EmptyState::primary_clicked, this, &AgentsViewPanel::rediscover);
    vl->addWidget(list_stack_, 1);

    list_refresh_btn_ = new QPushButton(tr("REFRESH"));
    list_refresh_btn_->setCursor(Qt::PointingHandCursor);
    list_refresh_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:6px;"
                                             "font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;letter-spacing:1px;}"
                                             "QPushButton:hover{background:%3;color:%4;}")
                                         .arg(ui::colors::BG_RAISED(), ui::colors::AMBER(), ui::colors::AMBER_DIM(),
                                              ui::colors::TEXT_ON_ACCENT()));
    connect(list_refresh_btn_, &QPushButton::clicked, this, &AgentsViewPanel::rediscover);
    vl->addWidget(list_refresh_btn_);
    return panel;
}

void AgentsViewPanel::rediscover() {
    discover_error_.clear();
    discovering_ = all_agents_.isEmpty();
    show_list_state();
    services::AgentService::instance().clear_cache();
    services::AgentService::instance().discover_agents();
}

void AgentsViewPanel::show_list_state() {
    if (!list_stack_)
        return;
    // Hosted by the agents screen (its rail lists the agents): the list column
    // only comes back to say why there is nothing to edit (discovering, failed,
    // none found) — never an empty editor.
    if (list_hidden_by_host_ && list_panel_)
        list_panel_->setVisible(agent_list_->count() == 0);
    if (agent_list_->count() > 0) {
        list_stack_->setCurrentWidget(agent_list_);
        return;
    }
    if (discovering_ && all_agents_.isEmpty()) {
        list_stack_->setCurrentWidget(list_skeleton_);
        return;
    }
    ui::EmptyState::Spec s;
    if (all_agents_.isEmpty() && !discover_error_.isEmpty()) {
        s = ui::EmptyState::error_spec(discover_error_, tr("Agent discovery failed"));
    } else if (all_agents_.isEmpty()) {
        s = ui::EmptyState::no_data_spec(tr("No agents found"),
                                         tr("Discovery returned no agents. Create one in the CREATE view, or try "
                                            "again."),
                                         tr("RETRY"));
    } else {
        s = ui::EmptyState::no_data_spec(tr("No agents in this category"),
                                         tr("Pick another category above, or All Categories."));
    }
    list_state_->apply(s);
    list_stack_->setCurrentWidget(list_state_);
}

// ── Center: config editor ─────────────────────────────────────────────────────

QWidget* AgentsViewPanel::build_config_panel() {
    auto* wrapper = new QWidget(this);
    auto* wl = new QVBoxLayout(wrapper);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->setSpacing(0);

    // Toggle bar
    auto* toggle_bar = new QWidget(this);
    toggle_bar->setMinimumHeight(32);
    toggle_bar->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* tbl = new QHBoxLayout(toggle_bar);
    tbl->setContentsMargins(8, 0, 8, 0);

    json_toggle_btn_ = new QPushButton(tr("JSON EDITOR"));
    json_toggle_btn_->setCheckable(true);
    json_toggle_btn_->setCursor(Qt::PointingHandCursor);
    json_toggle_btn_->setStyleSheet(
        QString(
            "QPushButton{background:transparent;color:%1;border:none;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:600;padding:4px 8px;}"
            "QPushButton:checked{color:%2;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    tbl->addWidget(json_toggle_btn_);
    tbl->addStretch();

    add_team_btn_ = new QPushButton(tr("+ ADD TO TEAM"));
    add_team_btn_->setCursor(Qt::PointingHandCursor);
    add_team_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;font-size:" + fincept::ui::fonts::px(-5) + "px;"
                "font-weight:600;padding:3px 8px;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::CYAN(), ui::colors::BG_HOVER()));
    tbl->addWidget(add_team_btn_);
    wl->addWidget(toggle_bar);

    // Stacked: form vs JSON
    auto* stack = new QStackedWidget;

    // ── Form view ──
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea{background:%1;border:none;}"
                                  "QScrollBar:vertical{background:%1;width:6px;}"
                                  "QScrollBar::handle:vertical{background:%2;min-height:30px;}")
                              .arg(ui::colors::BG_BASE(), ui::colors::BORDER_BRIGHT()));

    form_widget_ = new QWidget(this);
    auto* vl = new QVBoxLayout(form_widget_);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(8);

    agent_name_label_ = new QLabel(tr("Select an agent"));
    agent_name_label_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(2) + "px;font-weight:700;").arg(ui::colors::TEXT_PRIMARY()));
    vl->addWidget(agent_name_label_);

    agent_desc_label_ = new QLabel;
    agent_desc_label_->setWordWrap(true);
    agent_desc_label_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;padding-bottom:8px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(agent_desc_label_);

    // ── LLM PROFILE — per-agent profile picker ────────────────────────────────
    llm_profile_hdr_ = section_hdr(tr("LLM PROFILE"));
    vl->addWidget(llm_profile_hdr_);

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip(
        tr("Select which LLM profile this agent uses. Create profiles in Settings > LLM Config > Profiles."));
    llm_profile_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 10px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                "QComboBox::drop-down{border:none;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    vl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(llm_resolved_lbl_);

    // ── INSTRUCTIONS ──
    instructions_hdr_ = section_hdr(tr("INSTRUCTIONS"));
    vl->addWidget(instructions_hdr_);
    instructions_edit_ = new QPlainTextEdit;
    instructions_edit_->setPlaceholderText(tr("System prompt / instructions..."));
    instructions_edit_->setMaximumHeight(120);
    instructions_edit_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(input_ss()));
    vl->addWidget(instructions_edit_);

    // ── TOOLS ──
    tools_hdr_ = section_hdr(tr("TOOLS"));
    vl->addWidget(tools_hdr_);
    tools_list_ = new QListWidget;
    tools_list_->setMaximumHeight(100);
    tools_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                                       "QListWidget::item{padding:2px 6px;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY()));
    vl->addWidget(tools_list_);

    // ── FEATURES ──
    features_hdr_ = section_hdr(tr("FEATURES"));
    vl->addWidget(features_hdr_);
    const QString chk_style =
        QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;spacing:6px;}").arg(ui::colors::TEXT_PRIMARY());
    auto* feat_grid = new QGridLayout;
    reasoning_check_ = new QCheckBox(tr("Reasoning"));
    reasoning_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(reasoning_check_, 0, 0);
    memory_check_ = new QCheckBox(tr("Memory"));
    memory_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(memory_check_, 0, 1);
    knowledge_check_ = new QCheckBox(tr("Knowledge"));
    knowledge_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(knowledge_check_, 1, 0);
    guardrails_check_ = new QCheckBox(tr("Guardrails"));
    guardrails_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(guardrails_check_, 1, 1);
    tracing_check_ = new QCheckBox(tr("Tracing"));
    tracing_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(tracing_check_, 2, 0);
    agentic_memory_check_ = new QCheckBox(tr("Agentic Memory"));
    agentic_memory_check_->setStyleSheet(chk_style);
    feat_grid->addWidget(agentic_memory_check_, 2, 1);
    vl->addLayout(feat_grid);

    // ── Actions ──
    vl->addSpacing(12);
    auto* acts = new QHBoxLayout;
    save_btn_ = new QPushButton(tr("SAVE CONFIG"));
    save_btn_->setCursor(Qt::PointingHandCursor);
    save_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px 16px;"
                "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;letter-spacing:1px;}QPushButton:hover{background:%3;}")
            .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM()));
    acts->addWidget(save_btn_);
    delete_btn_ = new QPushButton(tr("DELETE"));
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the agent
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %1;padding:8px 16px;"
                "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;}QPushButton:hover{background:%2;}")
            .arg(ui::colors::DANGER(), ui::colors::BG_HOVER()));
    acts->addWidget(delete_btn_);
    acts->addStretch();
    vl->addLayout(acts);
    vl->addStretch();

    scroll->setWidget(form_widget_);
    stack->addWidget(scroll); // index 0 = form

    // ── JSON editor view ──
    json_widget_ = new QWidget(this);
    auto* jl = new QVBoxLayout(json_widget_);
    jl->setContentsMargins(8, 8, 8, 8);
    json_editor_ = new QPlainTextEdit;
    json_editor_->setStyleSheet(QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;"
                                        "font-size:" + fincept::ui::fonts::px(-2) + "px;}")
                                    .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    jl->addWidget(json_editor_);
    json_apply_btn_ = new QPushButton(tr("APPLY JSON"));
    json_apply_btn_->setCursor(Qt::PointingHandCursor);
    json_apply_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                           "font-weight:700;}QPushButton:hover{background:%3;}")
                                       .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM()));
    connect(json_apply_btn_, &QPushButton::clicked, this, [this, stack]() {
        // Malformed JSON used to be swallowed: the editor closed and the form
        // silently kept the old values, so the user believed their edit applied.
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(json_editor_->toPlainText().toUtf8(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::warning(this, tr("Apply JSON"),
                                 tr("Could not parse the JSON at offset %1: %2").arg(perr.offset).arg(
                                     perr.error == QJsonParseError::NoError ? tr("top level must be an object")
                                                                            : perr.errorString()));
            return; // stay in the editor so the user can fix it
        }
        if (selected_agent_idx_ >= 0) {
            filtered_agents_[selected_agent_idx_].config = doc.object();
            load_agent_into_editor(filtered_agents_[selected_agent_idx_]);
        }
        json_toggle_btn_->setChecked(false);
        stack->setCurrentIndex(0);
    });
    jl->addWidget(json_apply_btn_);
    stack->addWidget(json_widget_); // index 1 = JSON

    connect(json_toggle_btn_, &QPushButton::toggled, this, [this, stack](bool checked) {
        json_mode_ = checked;
        if (checked)
            json_editor_->setPlainText(QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Indented));
        stack->setCurrentIndex(checked ? 1 : 0);
    });

    wl->addWidget(stack, 1);
    return wrapper;
}

// ── Right: query + results ────────────────────────────────────────────────────

QWidget* AgentsViewPanel::build_query_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(300);
    panel->setMaximumWidth(450);
    // Scoped to the panel (a selector-less sheet drew border-left on every child).
    panel->setObjectName(QStringLiteral("agentsQueryPanel"));
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(QString("QWidget#agentsQueryPanel{background:%1;border-left:1px solid %2;}")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    test_query_hdr_ = section_hdr(tr("TEST QUERY"));
    vl->addWidget(test_query_hdr_);

    auto* opts = new QHBoxLayout;
    auto_route_check_ = new QCheckBox(tr("Auto-Route"));
    auto_route_check_->setStyleSheet(QString("QCheckBox{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;}").arg(ui::colors::SUCCESS()));
    opts->addWidget(auto_route_check_);

    output_caption_ = new QLabel(tr("Output:"));
    output_caption_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_SECONDARY()));
    opts->addWidget(output_caption_);

    output_model_combo_ = new QComboBox;
    // Item data = finagent_core OutputModelRegistry key (run_structured looks the
    // model up by exactly this name); "text" = plain streaming run.
    output_model_combo_->addItem(tr("text"), QStringLiteral("text"));
    output_model_combo_->addItem(tr("Trade Signal"), QStringLiteral("trade_signal"));
    output_model_combo_->addItem(tr("Stock Analysis"), QStringLiteral("stock_analysis"));
    output_model_combo_->addItem(tr("Portfolio Analysis"), QStringLiteral("portfolio_analysis"));
    output_model_combo_->addItem(tr("Risk Assessment"), QStringLiteral("risk_assessment"));
    output_model_combo_->addItem(tr("Market Analysis"), QStringLiteral("market_analysis"));
    output_model_combo_->addItem(tr("Research Report"), QStringLiteral("research_report"));
    output_model_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:" + fincept::ui::fonts::px(-4) + "px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;selection-color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                 ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));
    opts->addWidget(output_model_combo_);
    opts->addStretch();
    vl->addLayout(opts);

    query_input_ = new QPlainTextEdit;
    query_input_->setPlaceholderText(tr("Enter a query to test this agent..."));
    query_input_->setMaximumHeight(80);
    query_input_->setStyleSheet(
        QString("QPlainTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    vl->addWidget(query_input_);

    run_btn_ = new QPushButton(tr("RUN AGENT"));
    run_btn_->setProperty("fincept_agent_risk", "spend"); // agents: runs the agent (model quota)
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:8px;font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                    "font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(),
                                     ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    vl->addWidget(run_btn_);

    routing_info_label_ = new QLabel;
    routing_info_label_->setWordWrap(true);
    routing_info_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::CYAN()));
    routing_info_label_->hide();
    vl->addWidget(routing_info_label_);

    result_status_ = new QLabel;
    result_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(result_status_);

    result_hdr_ = new QLabel(tr("RESULT"));
    result_hdr_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
    // Structured results open as a laid-out page; RAW flips to the JSON answer.
    raw_toggle_btn_ = new QPushButton(tr("RAW"));
    raw_toggle_btn_->setCheckable(true);
    raw_toggle_btn_->setCursor(Qt::PointingHandCursor);
    raw_toggle_btn_->setToolTip(tr("Show the raw JSON answer"));
    raw_toggle_btn_->setStyleSheet(
        QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:1px 8px;font-size:" +
                fincept::ui::fonts::px(-4) + "px;font-weight:700;}"
                "QPushButton:checked{color:%3;border-color:%3;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::AMBER()));
    raw_toggle_btn_->hide();
    connect(raw_toggle_btn_, &QPushButton::toggled, this, [this](bool raw) {
        if (result_stack_ && !structured_model_.isEmpty())
            result_stack_->setCurrentIndex(raw ? 0 : 1);
    });
    auto* hdr_row = new QHBoxLayout;
    hdr_row->setContentsMargins(0, 0, 0, 0);
    hdr_row->addWidget(result_hdr_);
    hdr_row->addStretch();
    hdr_row->addWidget(raw_toggle_btn_);
    vl->addLayout(hdr_row);

    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_BRIGHT()));
    // Document stylesheet controls rendered markdown colors (headers, code, links, etc.)
    result_display_->document()->setDefaultStyleSheet(result_doc_css());

    structured_scroll_ = new QScrollArea;
    structured_scroll_->setWidgetResizable(true);
    structured_scroll_->setFrameShape(QFrame::NoFrame);
    structured_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    result_stack_ = new QStackedWidget;
    result_stack_->addWidget(result_display_);    // 0: markdown / raw JSON
    result_stack_->addWidget(structured_scroll_); // 1: structured page
    // 2: no result yet. A run sends the query to the agent's LLM (tokens on the
    // user's provider), so it never runs by itself: what it would do + RUN.
    test_empty_ = new ui::EmptyState(result_stack_);
    test_empty_->apply(ui::EmptyState::preview_spec(
        tr("Test this agent"),
        tr("Sends the query above to the selected agent with its LLM profile and tools, and shows the answer here."),
        tr("RUN AGENT"), tr("Uses your LLM provider — billed as tokens there"), ui::EmptyState::Tone::Warning,
        ui::LoadingSkeleton::Shape::Text));
    test_empty_->set_compact(true);
    connect(test_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (query_input_->toPlainText().trimmed().isEmpty()) {
            query_input_->setFocus();
            set_status(result_status_, tr("Type a query first"), ui::colors::WARNING());
            return;
        }
        run_query();
    });
    result_stack_->addWidget(test_empty_);
    result_stack_->setCurrentWidget(test_empty_);
    vl->addWidget(result_stack_, 1);

    return panel;
}

void AgentsViewPanel::refresh_test_preview() {
    if (!test_empty_)
        return;
    const QString agent = selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size()
                              ? filtered_agents_[selected_agent_idx_].name
                              : tr("none selected");
    test_empty_->set_details({{tr("Agent"), agent}, {tr("Model"), llm_resolved_lbl_ ? llm_resolved_lbl_->text() : QString()}},
                             tr("WHAT WILL RUN"));
    test_empty_->set_note(test_llm_ok_ ? tr("Uses your LLM provider — billed as tokens there")
                                       : tr("No LLM provider — Settings > LLM Configuration"),
                          test_llm_ok_ ? ui::EmptyState::Tone::Warning : ui::EmptyState::Tone::Danger);
    test_empty_->set_primary_enabled(test_llm_ok_);
}

void AgentsViewPanel::clear_structured() {
    structured_model_.clear();
    structured_data_ = QJsonObject();
    structured_valid_ = true;
    if (raw_toggle_btn_) {
        raw_toggle_btn_->blockSignals(true);
        raw_toggle_btn_->setChecked(false);
        raw_toggle_btn_->blockSignals(false);
        raw_toggle_btn_->hide();
    }
    if (structured_scroll_)
        delete structured_scroll_->takeWidget();
    if (result_stack_)
        result_stack_->setCurrentIndex(0);
}

void AgentsViewPanel::rebuild_structured_view() {
    if (!structured_scroll_ || structured_model_.isEmpty())
        return;
    auto* host = new QWidget;
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(0);
    lay->addWidget(sv::build(structured_model_, structured_data_, structured_valid_, host));
    delete structured_scroll_->takeWidget();
    structured_scroll_->setWidget(host);
}

void AgentsViewPanel::show_result(const services::AgentExecutionResult& r) {
    // The JSON / markdown answer always backs page 0 (copy, RAW, draft save).
    agent_result::show_markdown(result_display_, r.response);
    if (r.structured_model.isEmpty() || r.structured_data.isEmpty()) {
        clear_structured();
        return;
    }
    structured_model_ = r.structured_model;
    structured_data_ = r.structured_data;
    structured_valid_ = r.structured_valid;
    rebuild_structured_view();
    raw_toggle_btn_->blockSignals(true);
    raw_toggle_btn_->setChecked(false);
    raw_toggle_btn_->blockSignals(false);
    raw_toggle_btn_->show();
    result_stack_->setCurrentIndex(1);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void AgentsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));
    list_panel_ = build_agent_list_panel();
    splitter->addWidget(list_panel_);
    splitter->addWidget(build_config_panel());
    splitter->addWidget(build_query_panel());
    splitter->setSizes({220, 500, 350});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    root->addWidget(splitter);
}

// ── Connections ───────────────────────────────────────────────────────────────

void AgentsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo> agents, QVector<services::AgentCategory> categories) {
                all_agents_ = agents;
                category_combo_->blockSignals(true);
                const int prev = category_combo_->currentIndex();
                category_combo_->clear();
                category_combo_->addItem(tr("All Categories"), "");
                for (const auto& cat : categories)
                    category_combo_->addItem(QString("%1 (%2)").arg(cat.name).arg(cat.count), cat.name);
                if (prev >= 0 && prev < category_combo_->count())
                    category_combo_->setCurrentIndex(prev);
                category_combo_->blockSignals(false);
                discovering_ = false;
                on_category_changed(category_combo_->currentIndex());
            });
    // Discovery failures used to reach only the screen's status bar, leaving a
    // blank list: with nothing listed, show the failure with RETRY here.
    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& context, const QString& msg) {
        if (context != QLatin1String("discover_agents"))
            return;
        discover_error_ = msg.trimmed().isEmpty() ? tr("The agent runtime did not answer.") : msg.left(400);
        discovering_ = false;
        show_list_state();
    });

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        // Only the run this panel started. Auto-Route keeps execute_routed_query()'s
        // id (route_query()'s id only labels the routing line). An empty pending id
        // must never match: results of other screens (Portfolio, MCP tools) are
        // not ours.
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN AGENT"));
        if (r.success) {
            show_result(r);
            set_status(result_status_, tr("Completed in %1").arg(agent_result::duration(r.execution_time_ms)),
                       ui::colors::SUCCESS());
        } else {
            clear_structured();
            agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            set_status(result_status_, tr("FAILED"), ui::colors::DANGER());
        }
    });

    // ── Streaming signals ─────────────────────────────────────────────────────
    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_)
                    return;
                result_status_->setText(status);
            });

    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_)
                    return;
                QTextCursor cursor = result_display_->textCursor();
                cursor.movePosition(QTextCursor::End);
                result_display_->setTextCursor(cursor);
                result_display_->insertPlainText(token);
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN AGENT"));
        if (r.success) {
            show_result(r);
            set_status(result_status_, tr("Completed in %1").arg(agent_result::duration(r.execution_time_ms)),
                       ui::colors::SUCCESS());
        } else if (r.error == QLatin1String(services::AgentService::kCancelledError)) {
            // User pressed STOP — keep whatever streamed in so far.
            set_status(result_status_, tr("Stopped"), ui::colors::WARNING());
        } else {
            clear_structured();
            agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            set_status(result_status_, tr("FAILED"), ui::colors::DANGER());
        }
    });

    connect(&svc, &services::AgentService::routing_result, this, [this](services::RoutingResult r) {
        if (routing_request_id_.isEmpty() || r.request_id != routing_request_id_)
            return;
        routing_request_id_.clear();
        if (r.success) {
            routing_info_label_->setText(tr("Routed → %1 (intent: %2, confidence: %3%)")
                                             .arg(r.agent_id, r.intent)
                                             .arg(static_cast<int>(r.confidence * 100)));
            routing_info_label_->show();
        }
    });

    // No AgentService::error_occurred handler here: every run this panel starts
    // (run_agent_streaming / run_agent_structured / execute_routed_query) always
    // ends in agent_stream_done or agent_result, including spawn failures and a
    // missing interpreter. error_occurred only reports discovery / task /
    // schedule / plan / config calls — reacting to those flipped a running test
    // to ERROR and then dropped its real result.

    // Reload profile combo when LLM config changes
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            &AgentsViewPanel::load_profile_combo);

    // Update resolved label when user picks a different profile
    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AgentsViewPanel::refresh_llm_pill);

    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AgentsViewPanel::on_category_changed);
    connect(agent_list_, &QListWidget::currentRowChanged, this, &AgentsViewPanel::on_agent_selected);
    connect(run_btn_, &QPushButton::clicked, this, &AgentsViewPanel::run_query);
    connect(save_btn_, &QPushButton::clicked, this, &AgentsViewPanel::save_current_config);
    connect(delete_btn_, &QPushButton::clicked, this, &AgentsViewPanel::delete_current_config);
    connect(add_team_btn_, &QPushButton::clicked, this, [this]() {
        if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
            emit add_agent_to_team(filtered_agents_[selected_agent_idx_]);
    });

    // Populate profile combo on first show
    load_profile_combo();
}

// ── LLM profile combo ─────────────────────────────────────────────────────────

void AgentsViewPanel::load_profile_combo() {
    // Preserve current selection
    const QString prev_id = llm_profile_combo_->currentData().toString();
    llm_profile_combo_->blockSignals(true);
    llm_profile_combo_->clear();

    // First item = use global default (no explicit assignment)
    llm_profile_combo_->addItem(tr("Default (Global)"), QString{});

    // Usable profiles, then harness profiles as disabled rows (no id: never restored below).
    agent_profiles::add_profile_rows(llm_profile_combo_, tr(" [default]"), QStringLiteral("agent"));

    // Restore previous selection if still present
    int restore = 0;
    if (!prev_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == prev_id) {
                restore = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(restore);
    // Trigger resolved label update
    refresh_llm_pill();
}

void AgentsViewPanel::refresh_llm_pill() {
    const QString profile_id = llm_profile_combo_->currentData().toString();

    // Get agent_id for current selection (may be empty if none selected yet)
    QString agent_id;
    if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
        agent_id = filtered_agents_[selected_agent_idx_].id;

    // Resolve: if user picked "Default (Global)" use agent-id-based resolution,
    // otherwise resolve the explicitly chosen profile
    ResolvedLlmProfile resolved;
    if (profile_id.isEmpty()) {
        // Resolve via assignment chain (entity → type-default → global → legacy)
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);
    } else {
        // User explicitly chose a profile — look it up directly
        const auto pr2 = LlmProfileRepository::instance().list_profiles();
        const auto profiles = pr2.is_ok() ? pr2.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles) {
            if (p.id == profile_id) {
                resolved.profile_id = p.id;
                resolved.profile_name = p.name;
                resolved.provider = p.provider;
                resolved.model_id = p.model_id;
                resolved.api_key = p.api_key;
                resolved.base_url = p.base_url;
                resolved.temperature = p.temperature;
                resolved.max_tokens = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
        // Fallback to resolution if not found
        if (resolved.profile_id.isEmpty())
            resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);
    }

    if (!resolved.provider.isEmpty()) {
        QString text = resolved.provider.toUpper() + " / " + resolved.model_id;
        if (text.length() > 60)
            text = text.left(58) + "..";
        if (profile_id.isEmpty())
            text += tr(" (inherited)");
        llm_resolved_lbl_->setText(text);
        llm_resolved_lbl_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    } else {
        llm_resolved_lbl_->setText(tr("No provider configured — go to Settings > LLM Config"));
        llm_resolved_lbl_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::DANGER()));
    }
    test_llm_ok_ = !resolved.provider.isEmpty();
    refresh_test_preview();
}

// ── Population ────────────────────────────────────────────────────────────────

void AgentsViewPanel::populate_agent_list(const QVector<services::AgentInfo>& agents) {
    // The agent shown in the editor before this refill (discovery re-emits the
    // list on every show).
    const QString prev_id = selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size()
                                ? filtered_agents_[selected_agent_idx_].id
                                : QString();
    {
        const QSignalBlocker block(agent_list_); // no currentRowChanged(-1) while refilling
        agent_list_->clear();
        filtered_agents_ = agents;
        for (const auto& a : agents) {
            auto* item = new QListWidgetItem(a.name);
            item->setToolTip(a.description);
            item->setData(Qt::UserRole, a.id);
            agent_list_->addItem(item);
        }
    }
    list_count_label_->setText(QString::number(agents.size()));

    // A list with a detail pane never leaves it on "Select an agent": keep the
    // same agent selected WITHOUT reloading the editor (unsaved edits survive a
    // re-discovery), else select the first one (which loads it).
    int keep = -1;
    for (int i = 0; !prev_id.isEmpty() && i < agents.size(); ++i)
        if (agents[i].id == prev_id) {
            keep = i;
            break;
        }
    if (keep >= 0) {
        const QSignalBlocker block(agent_list_);
        agent_list_->setCurrentRow(keep);
        selected_agent_idx_ = keep;
    } else if (!agents.isEmpty()) {
        agent_list_->setCurrentRow(0); // -> on_agent_selected(0)
    } else {
        selected_agent_idx_ = -1;
    }
    show_list_state();
}

void AgentsViewPanel::on_category_changed(int index) {
    const QString cat = category_combo_->itemData(index).toString();
    if (cat.isEmpty()) {
        populate_agent_list(all_agents_);
    } else {
        QVector<services::AgentInfo> filtered;
        for (const auto& a : all_agents_)
            if (a.category == cat)
                filtered.append(a);
        populate_agent_list(filtered);
    }
}

void AgentsViewPanel::on_agent_selected(int row) {
    if (row < 0 || row >= filtered_agents_.size()) {
        selected_agent_idx_ = -1;
        return;
    }
    selected_agent_idx_ = row;
    load_agent_into_editor(filtered_agents_[row]);
    refresh_test_preview();
}

void AgentsViewPanel::load_agent_into_editor(const services::AgentInfo& agent) {
    agent_name_label_->setText(agent.name);
    agent_desc_label_->setText(agent.description);

    // Restore the profile assignment for this agent in the combo
    const QString assigned_id = LlmProfileRepository::instance().get_assignment("agent", agent.id);
    llm_profile_combo_->blockSignals(true);
    int select_idx = 0; // default = "Default (Global)"
    if (!assigned_id.isEmpty()) {
        for (int i = 1; i < llm_profile_combo_->count(); ++i) {
            if (llm_profile_combo_->itemData(i).toString() == assigned_id) {
                select_idx = i;
                break;
            }
        }
    }
    llm_profile_combo_->blockSignals(false);
    llm_profile_combo_->setCurrentIndex(select_idx);
    refresh_llm_pill();

    const QJsonObject config = agent.config;
    instructions_edit_->setPlainText(config["instructions"].toString());

    tools_list_->clear();
    const QJsonArray tools = config["tools"].toArray();
    for (const auto& t : tools)
        tools_list_->addItem(new QListWidgetItem(t.toString()));

    reasoning_check_->setChecked(config["reasoning"].toBool());
    memory_check_->setChecked(config["memory"].toBool());
    knowledge_check_->setChecked(config["knowledge"].toBool());
    guardrails_check_->setChecked(config["guardrails"].toBool());
    tracing_check_->setChecked(config["tracing"].toBool());
    agentic_memory_check_->setChecked(config["agentic_memory"].toBool());

    if (json_mode_)
        json_editor_->setPlainText(QJsonDocument(config).toJson(QJsonDocument::Indented));
}

QJsonObject AgentsViewPanel::build_config_from_editor() const {
    QJsonObject config;

    // Resolve the LLM profile for this agent.
    // If the user explicitly chose a profile in the combo, use that.
    // Otherwise fall back to the assignment chain.
    const QString profile_id = llm_profile_combo_->currentData().toString();
    QString agent_id;
    if (selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size())
        agent_id = filtered_agents_[selected_agent_idx_].id;

    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        // Explicit profile selection: look it up by ID
        const auto pr3 = LlmProfileRepository::instance().list_profiles();
        const auto profiles = pr3.is_ok() ? pr3.value() : QVector<LlmProfile>{};
        for (const auto& p : profiles) {
            if (p.id == profile_id) {
                resolved.profile_id = p.id;
                resolved.profile_name = p.name;
                resolved.provider = p.provider;
                resolved.model_id = p.model_id;
                resolved.api_key = p.api_key;
                resolved.base_url = p.base_url;
                resolved.temperature = p.temperature;
                resolved.max_tokens = p.max_tokens;
                resolved.system_prompt = p.system_prompt;
                break;
            }
        }
    }
    if (resolved.provider.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile("agent", agent_id);

    config["model"] = ai_chat::LlmService::profile_to_json(resolved);

    config["instructions"] = instructions_edit_->toPlainText();

    QJsonArray tools;
    for (int i = 0; i < tools_list_->count(); ++i)
        tools.append(tools_list_->item(i)->text());
    config["tools"] = tools;

    config["reasoning"] = reasoning_check_->isChecked();
    config["memory"] = memory_check_->isChecked();
    config["knowledge"] = knowledge_check_->isChecked();
    config["guardrails"] = guardrails_check_->isChecked();
    config["tracing"] = tracing_check_->isChecked();
    config["agentic_memory"] = agentic_memory_check_->isChecked();
    return config;
}

// ── Actions ───────────────────────────────────────────────────────────────────

void AgentsViewPanel::save_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    const auto& agent = filtered_agents_[selected_agent_idx_];

    // Persist profile assignment for this agent
    const QString profile_id = llm_profile_combo_->currentData().toString();
    auto& repo = LlmProfileRepository::instance();
    if (profile_id.isEmpty())
        repo.remove_assignment("agent", agent.id); // revert to global default
    else
        repo.assign_profile("agent", agent.id, profile_id); // pin to chosen profile

    AgentConfig db;
    db.id = agent.id;
    db.name = agent.name;
    db.description = agent.description;
    db.category = agent.category;
    db.config_json = QString::fromUtf8(QJsonDocument(build_config_from_editor()).toJson(QJsonDocument::Compact));
    services::AgentService::instance().save_config(db);
    set_status(result_status_, tr("Config saved"), ui::colors::SUCCESS());
}

void AgentsViewPanel::delete_current_config() {
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;
    // Deleting an agent config is irreversible and was a single unguarded click
    // right next to SAVE CONFIG.
    const QString name = filtered_agents_[selected_agent_idx_].name;
    if (QMessageBox::question(this, tr("Delete Agent"),
                              tr("Delete the agent configuration \"%1\"?\n\nThis cannot be undone.").arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    services::AgentService::instance().delete_config(filtered_agents_[selected_agent_idx_].id);
    set_status(result_status_, tr("Config deleted"), ui::colors::WARNING());
}

void AgentsViewPanel::run_query() {
    // While a streaming run is in flight the button doubles as STOP (the agent can
    // otherwise spend minutes of paid LLM/tool calls with no way to interrupt it).
    if (executing_) {
        services::AgentService::instance().cancel_run(pending_request_id_);
        return;
    }
    const QString q = query_input_->toPlainText().trimmed();
    if (q.isEmpty())
        return;

    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText(tr("RUNNING..."));
    clear_structured();
    agent_result::show_plain(result_display_, QString());
    routing_info_label_->hide();
    set_status(result_status_, tr("Executing..."), ui::colors::AMBER());

    const QJsonObject config = build_config_from_editor();
    auto& svc = services::AgentService::instance();

    if (auto_route_check_->isChecked()) {
        // The routing call only labels the run; the result is execute_routed_query's.
        routing_request_id_ = svc.route_query(q);
        pending_request_id_ = svc.execute_routed_query(q, config);
    } else {
        routing_request_id_.clear();
        // Registry key (item data), not the display label.
        const QString om = output_model_combo_->currentData().toString();
        if (!om.isEmpty() && om != QLatin1String("text")) {
            pending_request_id_ = svc.run_agent_structured(q, config, om);
        } else {
            pending_request_id_ = svc.run_agent_streaming(q, config);
            // Streaming runs can be cancelled — re-enable the button as STOP.
            run_btn_->setEnabled(true);
            run_btn_->setText(tr("STOP"));
        }
    }
}

void AgentsViewPanel::toggle_json_editor() {
    json_toggle_btn_->setChecked(!json_toggle_btn_->isChecked());
}

void AgentsViewPanel::apply_tools_selection(const QStringList& /*tools*/) {
    // Reload the currently-selected agent's tools directly from the repository
    // so we always show the persisted state, not stale in-memory AgentInfo.config.
    if (selected_agent_idx_ < 0 || selected_agent_idx_ >= filtered_agents_.size())
        return;

    const QString agent_id = filtered_agents_[selected_agent_idx_].id;
    auto result = AgentConfigRepository::instance().get(agent_id);
    if (!result.is_ok())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(result.value().config_json.toUtf8());
    if (doc.isNull())
        return;

    tools_list_->clear();
    const QJsonArray tools_arr = doc.object()["tools"].toArray();
    for (const auto& t : tools_arr)
        tools_list_->addItem(new QListWidgetItem(t.toString()));
}

// ── Draft persistence ────────────────────────────────────────────────────────

void AgentsViewPanel::set_list_visible(bool visible) {
    list_hidden_by_host_ = !visible;
    if (list_panel_)
        list_panel_->setVisible(visible || agent_list_->count() == 0);
}

bool AgentsViewPanel::select_agent(const QString& agent_id) {
    if (agent_id.isEmpty())
        return false;
    auto row_of = [this, &agent_id]() {
        for (int i = 0; i < filtered_agents_.size(); ++i)
            if (filtered_agents_[i].id == agent_id)
                return i;
        return -1;
    };
    int row = row_of();
    if (row < 0 && category_combo_->currentIndex() != 0) {
        category_combo_->setCurrentIndex(0); // "All Categories" -> on_category_changed refills the list
        row = row_of();
    }
    if (row < 0)
        return false;
    if (agent_list_->currentRow() == row)
        return true; // already in the editor (unsaved edits stay)
    agent_list_->setCurrentRow(row); // -> on_agent_selected(row)
    return true;
}

QString AgentsViewPanel::selected_agent_id() const {
    return selected_agent_idx_ >= 0 && selected_agent_idx_ < filtered_agents_.size()
               ? filtered_agents_[selected_agent_idx_].id
               : QString();
}

QVariantMap AgentsViewPanel::save_draft() const {
    QVariantMap d;
    if (query_input_ && !query_input_->toPlainText().isEmpty())
        d["query"] = query_input_->toPlainText();
    // The markdown source (not the rendered plain text), so a restore re-renders it.
    if (result_display_) {
        const QString saved = agent_result::saved_text(result_display_);
        if (!saved.isEmpty())
            d["result"] = saved;
    }
    return d;
}

void AgentsViewPanel::restore_draft(const QVariantMap& d) {
    if (d.isEmpty())
        return;
    if (query_input_ && d.contains("query"))
        query_input_->setPlainText(d["query"].toString());
    if (result_display_ && d.contains("result")) {
        clear_structured();
        agent_result::show_markdown(result_display_, d["result"].toString());
    }
}

// ── Re-translation ───────────────────────────────────────────────────────────

void AgentsViewPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AgentsViewPanel::retranslateUi() {
    // Left panel.
    if (list_title_)
        list_title_->setText(tr("AGENTS"));
    if (list_refresh_btn_)
        list_refresh_btn_->setText(tr("REFRESH"));
    // Category combo item 0 is the fixed "All Categories" entry (empty data).
    if (category_combo_ && category_combo_->count() > 0 && category_combo_->itemData(0).toString().isEmpty())
        category_combo_->setItemText(0, tr("All Categories"));

    // Center toolbar + headers.
    if (json_toggle_btn_)
        json_toggle_btn_->setText(tr("JSON EDITOR"));
    if (add_team_btn_)
        add_team_btn_->setText(tr("+ ADD TO TEAM"));
    if (llm_profile_hdr_)
        llm_profile_hdr_->setText(tr("LLM PROFILE"));
    if (instructions_hdr_)
        instructions_hdr_->setText(tr("INSTRUCTIONS"));
    if (tools_hdr_)
        tools_hdr_->setText(tr("TOOLS"));
    if (features_hdr_)
        features_hdr_->setText(tr("FEATURES"));
    if (llm_profile_combo_)
        llm_profile_combo_->setToolTip(
            tr("Select which LLM profile this agent uses. Create profiles in Settings > LLM Config > Profiles."));
    if (instructions_edit_)
        instructions_edit_->setPlaceholderText(tr("System prompt / instructions..."));

    // Feature checkboxes.
    if (reasoning_check_)
        reasoning_check_->setText(tr("Reasoning"));
    if (memory_check_)
        memory_check_->setText(tr("Memory"));
    if (knowledge_check_)
        knowledge_check_->setText(tr("Knowledge"));
    if (guardrails_check_)
        guardrails_check_->setText(tr("Guardrails"));
    if (tracing_check_)
        tracing_check_->setText(tr("Tracing"));
    if (agentic_memory_check_)
        agentic_memory_check_->setText(tr("Agentic Memory"));

    if (save_btn_)
        save_btn_->setText(tr("SAVE CONFIG"));
    if (delete_btn_)
        delete_btn_->setText(tr("DELETE"));
    if (json_apply_btn_)
        json_apply_btn_->setText(tr("APPLY JSON"));

    // Right panel.
    if (test_query_hdr_)
        test_query_hdr_->setText(tr("TEST QUERY"));
    if (auto_route_check_)
        auto_route_check_->setText(tr("Auto-Route"));
    if (output_caption_)
        output_caption_->setText(tr("Output:"));
    if (query_input_)
        query_input_->setPlaceholderText(tr("Enter a query to test this agent..."));
    if (run_btn_ && !executing_)
        run_btn_->setText(tr("RUN AGENT"));
    if (result_hdr_)
        result_hdr_->setText(tr("RESULT"));

    // agent_name_label_ holds the selected agent's name; only restore the static
    // prompt when nothing is selected. The LLM profile combo / resolved label
    // re-derive their text from data, so refresh the resolved pill.
    if (agent_name_label_ && selected_agent_idx_ < 0)
        agent_name_label_->setText(tr("Select an agent"));
    refresh_llm_pill();
}

} // namespace fincept::screens
