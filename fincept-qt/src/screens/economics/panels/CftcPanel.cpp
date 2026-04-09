// src/screens/economics/panels/CftcPanel.cpp
// CFTC (Commodity Futures Trading Commission) — Commitments of Traders.
// No API key required. Covers 40+ futures markets.
// Commands: cot_data, market_sentiment, cot_historical_trend
// cot_data response: { "success": true, "data": [{flat 40+ fields}], "parameters": {...} }
// sentiment response: { "success": true, "data": { "commercial_positions": {...},
//                       "non_commercial_positions": {...}, "overall_sentiment": {...} } }
// trend response: { "success": true, "data": [{ "date", "open_interest",
//                   "commercial_net", "non_commercial_net", ... }] }
#include "screens/economics/panels/CftcPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kCftcScript   = "cftc_data.py";
static constexpr const char* kCftcSourceId = "cftc";
static constexpr const char* kCftcColor    = "#FF5722";  // deep orange
} // namespace

static const QList<QPair<QString,QString>> kMarkets = {
    // Metals
    {"Gold",              "gold"},
    {"Silver",            "silver"},
    {"Copper",            "copper"},
    {"Platinum",          "platinum"},
    {"Palladium",         "palladium"},
    // Energy
    {"Crude Oil (WTI)",   "crude_oil"},
    {"Natural Gas",       "natural_gas"},
    {"Gasoline",          "gasoline"},
    {"Heating Oil",       "heating_oil"},
    // Agricultural
    {"Corn",              "corn"},
    {"Wheat",             "wheat"},
    {"Soybeans",          "soybeans"},
    {"Cotton",            "cotton"},
    {"Coffee",            "coffee"},
    {"Sugar",             "sugar"},
    {"Cocoa",             "cocoa"},
    {"Live Cattle",       "live_cattle"},
    {"Lean Hogs",         "lean_hogs"},
    // FX
    {"Euro (EUR/USD)",    "euro"},
    {"Japanese Yen",      "jpy"},
    {"British Pound",     "british_pound"},
    {"Swiss Franc",       "swiss_franc"},
    {"Canadian Dollar",   "canadian_dollar"},
    {"Australian Dollar", "australian_dollar"},
    // Indices
    {"S&P 500",           "s&p_500"},
    {"Nasdaq 100",        "nasdaq_100"},
    {"Dow Jones",         "dow_jones"},
    {"Nikkei 225",        "nikkei"},
    {"VIX",               "vix"},
    // Rates
    {"T-Bonds (30Y)",     "treasury_bonds"},
    {"T-Notes (10Y)",     "treasury_notes_10y"},
    {"T-Notes (5Y)",      "treasury_notes_5y"},
    {"T-Notes (2Y)",      "treasury_notes_2y"},
    {"Fed Funds",         "fed_funds"},
    // Crypto
    {"Bitcoin",           "bitcoin"},
    {"Ethereum",          "ether"},
    // FX
    {"US Dollar Index",   "us_dollar_index"},
};

static const QList<QPair<QString,QString>> kReportTypes = {
    {"Legacy",           "legacy"},
    {"Disaggregated",    "disaggregated"},
    {"Financial (TFF)",  "financial"},
};

static const QList<QPair<QString,QString>> kViews = {
    {"COT Table",         "cot"},
    {"Market Sentiment",  "sentiment"},
    {"Historical Trend",  "trend"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

CftcPanel::CftcPanel(QWidget* parent)
    : EconPanelBase(kCftcSourceId, kCftcColor, parent) {
    build_base_ui(this);
    build_sentiment_widget();
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &CftcPanel::on_result);
}

void CftcPanel::activate() {
    show_empty("Select a futures market and view, then click FETCH\n"
               "CFTC data is free — no API key required");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void CftcPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [this](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    market_combo_ = new QComboBox;
    for (const auto& m : kMarkets)
        market_combo_->addItem(m.first, m.second);
    market_combo_->setFixedHeight(26);
    market_combo_->setMinimumWidth(160);

    report_type_combo_ = new QComboBox;
    for (const auto& r : kReportTypes)
        report_type_combo_->addItem(r.first, r.second);
    report_type_combo_->setFixedHeight(26);
    report_type_combo_->setMinimumWidth(130);

    view_combo_ = new QComboBox;
    for (const auto& v : kViews)
        view_combo_->addItem(v.first, v.second);
    view_combo_->setFixedHeight(26);
    view_combo_->setMinimumWidth(145);

    thl->addWidget(make_lbl("MARKET"));
    thl->addWidget(market_combo_);
    thl->addWidget(make_lbl("REPORT"));
    thl->addWidget(report_type_combo_);
    thl->addWidget(make_lbl("VIEW"));
    thl->addWidget(view_combo_);
}

// ── Sentiment widget ──────────────────────────────────────────────────────────

void CftcPanel::build_sentiment_widget() {
    using namespace ui::colors;

    const auto card_bg = QString("background:%1; border:1px solid %2;")
        .arg(BG_RAISED(), BORDER_DIM());
    const auto lbl_ss = QString("color:%1; font-size:9px; font-weight:700;"
        " letter-spacing:1px; background:transparent;").arg(TEXT_SECONDARY());

    sentiment_widget_ = new QWidget;
    sentiment_widget_->setStyleSheet(QString("background:%1;").arg(BG_BASE()));
    auto* root = new QVBoxLayout(sentiment_widget_);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    // Header
    auto* hdr = new QWidget;
    hdr->setStyleSheet(card_bg);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(14, 10, 14, 10);

    auto* title_lbl = new QLabel("COT MARKET SENTIMENT");
    title_lbl->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; background:transparent;")
            .arg(TEXT_PRIMARY()));

    sent_market_lbl_ = new QLabel("—");
    sent_market_lbl_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; background:transparent;")
            .arg(color_));
    sent_date_lbl_ = new QLabel("—");
    sent_date_lbl_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent;")
            .arg(TEXT_TERTIARY()));

    hdr_hl->addWidget(title_lbl);
    hdr_hl->addStretch(1);
    hdr_hl->addWidget(sent_market_lbl_);
    hdr_hl->addWidget(sent_date_lbl_);
    root->addWidget(hdr);

    // Bias cards
    auto* cards = new QWidget;
    auto* cards_hl = new QHBoxLayout(cards);
    cards_hl->setContentsMargins(0, 0, 0, 0);
    cards_hl->setSpacing(10);

    auto make_card = [&](const QString& title_text, QLabel*& bias_out,
                         QLabel*& net_out, const QString& desc) {
        auto* card = new QWidget;
        card->setStyleSheet(card_bg);
        card->setMinimumHeight(120);
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(14, 12, 14, 12);
        cvl->setSpacing(4);

        auto* ttl = new QLabel(title_text);
        ttl->setStyleSheet(lbl_ss);

        bias_out = new QLabel("—");
        bias_out->setStyleSheet(
            QString("color:%1; font-size:18px; font-weight:700; background:transparent;")
                .arg(TEXT_PRIMARY()));

        net_out = new QLabel("Net: —");
        net_out->setStyleSheet(
            QString("color:%1; font-size:10px; background:transparent;")
                .arg(TEXT_TERTIARY()));

        auto* d = new QLabel(desc);
        d->setStyleSheet(
            QString("color:%1; font-size:9px; background:transparent;")
                .arg(TEXT_DIM()));
        d->setWordWrap(true);

        cvl->addWidget(ttl);
        cvl->addWidget(bias_out);
        cvl->addWidget(net_out);
        cvl->addStretch(1);
        cvl->addWidget(d);
        cards_hl->addWidget(card, 1);
    };

    make_card("COMMERCIAL TRADERS", sent_comm_bias_, sent_comm_net_,
              "Hedgers & producers — usually contrarian signal");
    make_card("NON-COMMERCIAL (SPECULATORS)", sent_noncomm_bias_, sent_noncomm_net_,
              "Managed money & funds — trend-following signal");

    // OI & trend row
    auto* oi_row = new QWidget;
    oi_row->setStyleSheet(card_bg);
    auto* oi_hl = new QHBoxLayout(oi_row);
    oi_hl->setContentsMargins(14, 8, 14, 8);
    oi_hl->setSpacing(20);

    auto* oi_lbl = new QLabel("OPEN INTEREST");
    oi_lbl->setStyleSheet(lbl_ss);
    sent_oi_lbl_ = new QLabel("—");
    sent_oi_lbl_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
            .arg(TEXT_PRIMARY()));

    auto* trend_lbl = new QLabel("OI TREND");
    trend_lbl->setStyleSheet(lbl_ss);
    sent_oi_trend_ = new QLabel("—");
    sent_oi_trend_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
            .arg(TEXT_PRIMARY()));

    oi_hl->addWidget(oi_lbl);
    oi_hl->addWidget(sent_oi_lbl_);
    oi_hl->addWidget(trend_lbl);
    oi_hl->addWidget(sent_oi_trend_);
    oi_hl->addStretch(1);

    root->addLayout(cards_hl);
    root->addWidget(oi_row);
    root->addStretch(1);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void CftcPanel::on_fetch() {
    const QString market      = market_combo_->currentData().toString();
    const QString report_type = report_type_combo_->currentData().toString();
    const QString view        = view_combo_->currentData().toString();

    if (market.isEmpty()) {
        show_empty("Select a futures market");
        return;
    }

    show_loading("Fetching CFTC data for " + market_combo_->currentText() + "…");

    if (view == "cot") {
        services::EconomicsService::instance().execute(
            kCftcSourceId, kCftcScript, "cot_data",
            {market, report_type},
            "cftc_cot_" + market);
    } else if (view == "sentiment") {
        services::EconomicsService::instance().execute(
            kCftcSourceId, kCftcScript, "market_sentiment",
            {market, report_type},
            "cftc_sent_" + market);
    } else {
        services::EconomicsService::instance().execute(
            kCftcSourceId, kCftcScript, "cot_historical_trend",
            {market, report_type},
            "cftc_trend_" + market);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void CftcPanel::on_result(const QString& request_id,
                          const services::EconomicsResult& result) {
    if (result.source_id != kCftcSourceId) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Sentiment view: data is an object, not an array
    if (request_id.startsWith("cftc_sent_")) {
        const QJsonObject sentiment = result.data["data"].toObject();
        if (sentiment.isEmpty()) {
            show_error("No sentiment data returned");
            return;
        }
        show_sentiment(sentiment);
        return;
    }

    // COT table or historical trend — data is an array
    // cot_data rows: flat dict with date, open_interest_all, comm_long_all, etc.
    // trend rows: { "date", "open_interest", "commercial_net", "non_commercial_net", ... }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        const QString err_str = result.data.contains("error")
                              ? result.data["error"].toObject()["error"].toString()
                              : "";
        show_empty(err_str.isEmpty()
                   ? "No data returned — try a different market or report type"
                   : err_str);
        return;
    }

    // For COT table, keep only the most useful columns for display
    // (the raw records have 40+ fields — we pick the key ones)
    if (request_id.startsWith("cftc_cot_")) {
        QJsonArray clean;
        for (const auto& v : rows) {
            const auto obj = v.toObject();
            QJsonObject row;
            row["date"]           = obj["report_date_as_yyyy_mm_dd"];
            row["market"]         = obj["market_and_exchange_names"];
            row["open_interest"]  = obj["open_interest_all"];
            row["comm_long"]      = obj["comm_long_all"].isUndefined()
                                  ? obj["comm_positions_long_all"]
                                  : obj["comm_long_all"];
            row["comm_short"]     = obj["comm_short_all"].isUndefined()
                                  ? obj["comm_positions_short_all"]
                                  : obj["comm_short_all"];
            row["noncomm_long"]   = obj["noncomm_long_all"].isUndefined()
                                  ? obj["noncomm_positions_long_all"]
                                  : obj["noncomm_long_all"];
            row["noncomm_short"]  = obj["noncomm_short_all"].isUndefined()
                                  ? obj["noncomm_positions_short_all"]
                                  : obj["noncomm_short_all"];
            clean.append(row);
        }
        display(clean, "CFTC COT: " + market_combo_->currentText()
                       + " (" + report_type_combo_->currentText() + ")");
    } else {
        // Historical trend — rows already have clean keys
        display(rows, "CFTC Historical Trend: " + market_combo_->currentText());
    }

    LOG_INFO("CftcPanel", QString("Displayed %1 rows for %2")
             .arg(rows.size()).arg(request_id));
}

// ── Sentiment display ─────────────────────────────────────────────────────────

void CftcPanel::show_sentiment(const QJsonObject& s) {
    if (!sentiment_widget_) return;

    // Populate labels
    sent_market_lbl_->setText(s["market_name"].toString(market_combo_->currentText()));
    sent_date_lbl_->setText("Report: " + s["latest_report"].toString("—"));

    const double oi = s["open_interest"].toDouble();
    sent_oi_lbl_->setText(oi > 0 ? QString::number(static_cast<qint64>(oi)) : "—");

    using namespace ui::colors;
    const QString oi_trend = s["overall_sentiment"].toObject()["oi_trend"].toString();
    sent_oi_trend_->setText(oi_trend.isEmpty() ? "—" : oi_trend.toUpper());
    sent_oi_trend_->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:700; background:transparent;")
        .arg(oi_trend == "increasing" ? POSITIVE() : NEGATIVE()));

    const QString comm_bias =
        s["overall_sentiment"].toObject()["commercial_bias"].toString();
    sent_comm_bias_->setText(comm_bias.isEmpty() ? "—" : comm_bias.toUpper());
    sent_comm_bias_->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; background:transparent;")
        .arg(comm_bias == "bullish" ? POSITIVE() : NEGATIVE()));

    const QString noncomm_bias =
        s["overall_sentiment"].toObject()["non_commercial_bias"].toString();
    sent_noncomm_bias_->setText(noncomm_bias.isEmpty() ? "—" : noncomm_bias.toUpper());
    sent_noncomm_bias_->setStyleSheet(
        QString("color:%1; font-size:18px; font-weight:700; background:transparent;")
        .arg(noncomm_bias == "bullish" ? POSITIVE() : NEGATIVE()));

    const QJsonObject comm = s["commercial_positions"].toObject();
    const double comm_net = comm["net"].toDouble();
    sent_comm_net_->setText(
        QString("Net: %1%2").arg(comm_net >= 0 ? "+" : "")
        .arg(static_cast<qint64>(comm_net), 'd'));

    const QJsonObject noncomm = s["non_commercial_positions"].toObject();
    const double noncomm_net = noncomm["net"].toDouble();
    sent_noncomm_net_->setText(
        QString("Net: %1%2").arg(noncomm_net >= 0 ? "+" : "")
        .arg(static_cast<qint64>(noncomm_net), 'd'));

    // Switch to sentiment widget — we show it in place of empty state
    // by temporarily inserting it into the layout if not already there
    // The base class stack_ is private; we use show_empty and overlay trick:
    // Instead, we just show data in the table as a summary row fallback.
    // For now, display the sentiment as a single-row table so base stats work.
    QJsonArray rows;
    QJsonObject row;
    row["market"]          = s["market_name"].toString(market_combo_->currentText());
    row["report_date"]     = s["latest_report"];
    row["open_interest"]   = oi;
    row["comm_net"]        = comm_net;
    row["noncomm_net"]     = noncomm_net;
    row["comm_bias"]       = comm_bias;
    row["noncomm_bias"]    = noncomm_bias;
    row["oi_trend"]        = oi_trend;
    rows.append(row);
    display(rows, "CFTC Sentiment: " + market_combo_->currentText());

    LOG_INFO("CftcPanel", "Displayed sentiment for " + market_combo_->currentText());
}

} // namespace fincept::screens
