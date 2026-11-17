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
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QComboBox>
#include <QJsonObject>
#include <QLabel>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <initializer_list>

namespace fincept::screens {
namespace {

static constexpr const char* kCftcScript = "cftc_data.py";
static constexpr const char* kCftcSourceId = "cftc";
static constexpr const char* kCftcColor = "#FF5722"; // deep orange
} // namespace

static const QList<QPair<QString, QString>> kMarkets = {
    // Metals
    {"Gold", "gold"},
    {"Silver", "silver"},
    {"Copper", "copper"},
    {"Platinum", "platinum"},
    {"Palladium", "palladium"},
    // Energy
    {"Crude Oil (WTI)", "crude_oil"},
    {"Natural Gas", "natural_gas"},
    {"Gasoline", "gasoline"},
    {"Heating Oil", "heating_oil"},
    // Agricultural
    {"Corn", "corn"},
    {"Wheat", "wheat"},
    {"Soybeans", "soybeans"},
    {"Cotton", "cotton"},
    {"Coffee", "coffee"},
    {"Sugar", "sugar"},
    {"Cocoa", "cocoa"},
    {"Live Cattle", "live_cattle"},
    {"Lean Hogs", "lean_hogs"},
    // FX
    {"Euro (EUR/USD)", "euro"},
    {"Japanese Yen", "jpy"},
    {"British Pound", "british_pound"},
    {"Swiss Franc", "swiss_franc"},
    {"Canadian Dollar", "canadian_dollar"},
    {"Australian Dollar", "australian_dollar"},
    // Indices
    {"S&P 500", "s&p_500"},
    {"Nasdaq 100", "nasdaq_100"},
    {"Dow Jones", "dow_jones"},
    {"Nikkei 225", "nikkei"},
    {"VIX", "vix"},
    // Rates
    {"T-Bonds (30Y)", "treasury_bonds"},
    {"T-Notes (10Y)", "treasury_notes_10y"},
    {"T-Notes (5Y)", "treasury_notes_5y"},
    {"T-Notes (2Y)", "treasury_notes_2y"},
    {"Fed Funds", "fed_funds"},
    // Crypto
    {"Bitcoin", "bitcoin"},
    {"Ethereum", "ether"},
    // FX
    {"US Dollar Index", "us_dollar_index"},
};

static const QList<QPair<QString, QString>> kReportTypes = {
    {"Legacy", "legacy"},
    {"Disaggregated", "disaggregated"},
    {"Financial (TFF)", "financial"},
};

static const QList<QPair<QString, QString>> kViews = {
    {"COT Table", "cot"},
    {"Market Sentiment", "sentiment"},
    {"Historical Trend", "trend"},
};

// Which report carries which market: Legacy covers every market, Disaggregated only physical
// commodities, Financial (TFF) only currencies / indices / rates / crypto. A mismatch used to end in
// "No COT data found for identifier: bitcoin".
static bool cftc_is_financial_market(const QString& code) {
    static const QStringList kFinancial = {
        "euro",         "jpy",       "british_pound", "swiss_franc", "canadian_dollar", "australian_dollar",
        "s&p_500",      "nasdaq_100", "dow_jones",    "nikkei",      "vix",             "treasury_bonds",
        "treasury_notes_10y", "treasury_notes_5y", "treasury_notes_2y", "fed_funds", "bitcoin", "ether",
        "us_dollar_index"};
    return kFinancial.contains(code);
}

/// Grey out the report type the selected market is not published in, and move off it if selected
/// (Disaggregated ↔ Financial are each other's detailed counterpart).
static void cftc_sync_report_types(QComboBox* market_combo, QComboBox* report_combo) {
    if (!market_combo || !report_combo)
        return;
    const bool financial = cftc_is_financial_market(market_combo->currentData().toString());
    const QString invalid = financial ? QStringLiteral("disaggregated") : QStringLiteral("financial");
    const QString counterpart = financial ? QStringLiteral("financial") : QStringLiteral("disaggregated");
    if (auto* model = qobject_cast<QStandardItemModel*>(report_combo->model())) {
        for (int i = 0; i < report_combo->count(); ++i)
            if (QStandardItem* item = model->item(i))
                item->setEnabled(report_combo->itemData(i).toString() != invalid);
    }
    if (report_combo->currentData().toString() == invalid) {
        const int idx = report_combo->findData(counterpart);
        if (idx >= 0)
            report_combo->setCurrentIndex(idx);
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

CftcPanel::CftcPanel(QWidget* parent) : EconPanelBase(kCftcSourceId, kCftcColor, parent) {
    build_base_ui(this);
    // Localize fixed combo item text (report types / views) to the active language.
    retranslateUi();
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &CftcPanel::on_result);
    // VIEW switches what is shown for the same market — like a tab, it loads at once.
    connect(view_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (isVisible())
            run_query();
    });
}

void CftcPanel::activate() {
    // Nothing to prepare: the results pane loads the COT report on show (free, no key).
}

EconPanelBase::LoadPolicy CftcPanel::load_policy() const {
    LoadPolicy p;
    p.max_age_s = 12 * 3600; // weekly report (Tuesday positions, published Friday)
    return p;
}

QJsonValue CftcPanel::page_payload(int page) const {
    return page == sentiment_page_ ? QJsonValue(last_sentiment_) : QJsonValue();
}

bool CftcPanel::restore_page(int page, const QJsonValue& payload) {
    build_sentiment_widget();
    if (page != sentiment_page_ || !payload.isObject() || payload.toObject().isEmpty())
        return false;
    show_sentiment(payload.toObject());
    return true;
}

// ── Controls ──────────────────────────────────────────────────────────────────

void CftcPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    market_combo_ = new QComboBox;
    for (const auto& m : kMarkets)
        market_combo_->addItem(m.first, m.second);
    market_combo_->setMinimumHeight(26);
    market_combo_->setMinimumWidth(160);

    // Report types and views are fixed UI labels — text set in retranslateUi();
    // only the code is meaningful as item data here.
    report_type_combo_ = new QComboBox;
    for (const auto& r : kReportTypes)
        report_type_combo_->addItem(r.first, r.second);
    report_type_combo_->setMinimumHeight(26);
    report_type_combo_->setMinimumWidth(130);

    view_combo_ = new QComboBox;
    for (const auto& v : kViews)
        view_combo_->addItem(v.first, v.second);
    view_combo_->setMinimumHeight(26);
    view_combo_->setMinimumWidth(145);

    cftc_sync_report_types(market_combo_, report_type_combo_);
    connect(market_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { cftc_sync_report_types(market_combo_, report_type_combo_); });

    thl->addWidget(market_lbl_ = make_lbl(tr("MARKET")));
    thl->addWidget(market_combo_);
    thl->addWidget(report_lbl_ = make_lbl(tr("REPORT")));
    thl->addWidget(report_type_combo_);
    thl->addWidget(view_lbl_ = make_lbl(tr("VIEW")));
    thl->addWidget(view_combo_);
}

// ── Sentiment widget ──────────────────────────────────────────────────────────

void CftcPanel::build_sentiment_widget() {
    if (sentiment_widget_)
        return;
    using namespace ui::colors;

    // One selector stylesheet, set before the children exist (ThemeManager's re-style pass keeps it
    // in step with theme and font changes); the bias values use the panel's econStatPos / Neg.
    sentiment_widget_ = new QWidget(content_host());
    sentiment_widget_->setObjectName(QStringLiteral("cftcSentiment"));
    sentiment_widget_->setStyleSheet(
        QString("QWidget#cftcSentiment { background:%1; }"
                "QWidget#cftcCard { background:%2; border:1px solid %3; }"
                "QLabel#cftcCardTitle { color:%4; font-size:%7px; font-weight:700; letter-spacing:1px;"
                "  background:transparent; border:none; }"
                "QLabel#cftcTitle { color:%5; font-size:%8px; font-weight:700; background:transparent; border:none; }"
                "QLabel#cftcMarket { color:%9; font-size:%10px; font-weight:700; background:transparent; border:none; }"
                "QLabel#cftcMuted { color:%6; font-size:%11px; background:transparent; border:none; }"
                "QLabel#cftcDesc { color:%6; font-size:%7px; background:transparent; border:none; }"
                "QLabel#cftcOi { color:%5; font-size:%12px; font-weight:700; background:transparent; border:none; }")
            .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), TEXT_SECONDARY(), TEXT_PRIMARY(), TEXT_TERTIARY())
            .arg(ui::fonts::px(-5), ui::fonts::px(-2), legible(QColor(color_)), ui::fonts::px(-3), ui::fonts::px(-4),
                 ui::fonts::px(-1)));
    sentiment_widget_->setAttribute(Qt::WA_StyledBackground, true);
    auto* root = new QVBoxLayout(sentiment_widget_);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    // Header
    auto* hdr = new QWidget(sentiment_widget_);
    hdr->setObjectName(QStringLiteral("cftcCard"));
    hdr->setAttribute(Qt::WA_StyledBackground, true);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(14, 10, 14, 10);

    sent_title_lbl_ = new QLabel(tr("COT MARKET SENTIMENT"), hdr);
    sent_title_lbl_->setObjectName(QStringLiteral("cftcTitle"));

    sent_market_lbl_ = new QLabel(QStringLiteral("—"), hdr);
    sent_market_lbl_->setObjectName(QStringLiteral("cftcMarket"));
    sent_date_lbl_ = new QLabel(QStringLiteral("—"), hdr);
    sent_date_lbl_->setObjectName(QStringLiteral("cftcMuted"));

    hdr_hl->addWidget(sent_title_lbl_);
    hdr_hl->addStretch(1);
    hdr_hl->addWidget(sent_market_lbl_);
    hdr_hl->addWidget(sent_date_lbl_);
    root->addWidget(hdr);

    // Bias cards
    auto* cards_hl = new QHBoxLayout;
    cards_hl->setContentsMargins(0, 0, 0, 0);
    cards_hl->setSpacing(10);

    auto make_card = [&](const QString& title_text, QLabel*& bias_out, QLabel*& net_out, const QString& desc,
                         QLabel*& title_out, QLabel*& desc_out) {
        auto* card = new QWidget(sentiment_widget_);
        card->setObjectName(QStringLiteral("cftcCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(14, 12, 14, 12);
        cvl->setSpacing(4);

        auto* ttl = new QLabel(title_text, card);
        ttl->setObjectName(QStringLiteral("cftcCardTitle"));
        title_out = ttl;

        bias_out = new QLabel(QStringLiteral("—"), card);
        bias_out->setObjectName("econStatVal");

        net_out = new QLabel(tr("Net: —"), card);
        net_out->setObjectName(QStringLiteral("cftcMuted"));

        auto* d = new QLabel(desc, card);
        d->setObjectName(QStringLiteral("cftcDesc"));
        d->setWordWrap(true);
        desc_out = d;

        cvl->addWidget(ttl);
        cvl->addWidget(bias_out);
        cvl->addWidget(net_out);
        cvl->addStretch(1);
        cvl->addWidget(d);
        cards_hl->addWidget(card, 1);
    };

    make_card(tr("COMMERCIAL TRADERS"), sent_comm_bias_, sent_comm_net_,
              tr("Hedgers & producers — usually contrarian signal"), sent_comm_card_lbl_, sent_comm_card_desc_);
    make_card(tr("NON-COMMERCIAL (SPECULATORS)"), sent_noncomm_bias_, sent_noncomm_net_,
              tr("Managed money & funds — trend-following signal"), sent_noncomm_card_lbl_, sent_noncomm_card_desc_);

    // OI & trend row
    auto* oi_row = new QWidget(sentiment_widget_);
    oi_row->setObjectName(QStringLiteral("cftcCard"));
    oi_row->setAttribute(Qt::WA_StyledBackground, true);
    auto* oi_hl = new QHBoxLayout(oi_row);
    oi_hl->setContentsMargins(14, 8, 14, 8);
    oi_hl->setSpacing(20);

    sent_oi_caption_ = new QLabel(tr("OPEN INTEREST"), oi_row);
    sent_oi_caption_->setObjectName(QStringLiteral("cftcCardTitle"));
    sent_oi_lbl_ = new QLabel(QStringLiteral("—"), oi_row);
    sent_oi_lbl_->setObjectName(QStringLiteral("cftcOi"));

    sent_oi_trend_caption_ = new QLabel(tr("OI TREND"), oi_row);
    sent_oi_trend_caption_->setObjectName(QStringLiteral("cftcCardTitle"));
    sent_oi_trend_ = new QLabel(QStringLiteral("—"), oi_row);
    sent_oi_trend_->setObjectName("econStatVal");

    oi_hl->addWidget(sent_oi_caption_);
    oi_hl->addWidget(sent_oi_lbl_);
    oi_hl->addWidget(sent_oi_trend_caption_);
    oi_hl->addWidget(sent_oi_trend_);
    oi_hl->addStretch(1);

    root->addLayout(cards_hl);
    root->addWidget(oi_row);
    root->addStretch(1);

    // Register the sentiment card view as a real page of the shared content
    // stack. Previously this whole widget tree was built, parented to the panel
    // and then never added to any layout — it floated at (0,0) over the toolbar
    // and the Market Sentiment view silently degraded to a one-row table.
    sentiment_page_ = add_content_page(sentiment_widget_);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void CftcPanel::on_fetch() {
    // A restored session can pair a market with a report it is not published in.
    cftc_sync_report_types(market_combo_, report_type_combo_);
    const QString market = market_combo_->currentData().toString();
    const QString report_type = report_type_combo_->currentData().toString();
    const QString view = view_combo_->currentData().toString();

    if (market.isEmpty()) {
        show_empty(tr("Select a futures market"));
        return;
    }

    show_loading(tr("Fetching CFTC data for %1…").arg(market_combo_->currentText()));

    if (view == "cot") {
        execute_query(kCftcSourceId, kCftcScript, "cot_data", {market, report_type},
                      "cftc_cot_" + market);
    } else if (view == "sentiment") {
        execute_query(kCftcSourceId, kCftcScript, "market_sentiment",
                      {market, report_type}, "cftc_sent_" + market);
    } else {
        execute_query(kCftcSourceId, kCftcScript, "cot_historical_trend",
                      {market, report_type}, "cftc_trend_" + market);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void CftcPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kCftcSourceId)
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Table/trend views are time series — restore the aggregate stat row that
    // the sentiment view hides.
    if (!request_id.startsWith("cftc_sent_"))
        set_stats_visible(true);

    // Sentiment view: data is an object, not an array
    if (request_id.startsWith("cftc_sent_")) {
        const QJsonObject sentiment = result.data["data"].toObject();
        if (sentiment.isEmpty()) {
            show_error(tr("No sentiment data returned"));
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
        const QString err_str =
            result.data.contains("error") ? result.data["error"].toObject()["error"].toString() : "";
        show_empty(err_str.isEmpty() ? tr("No data returned — try a different market or report type") : err_str);
        return;
    }

    // For COT table, keep only the most useful columns for display
    // (the raw records have 40+ fields — we pick the key ones)
    if (request_id.startsWith("cftc_cot_")) {
        QJsonArray clean;
        // CFTC's column names differ per report type — legacy: comm_*/noncomm_*, disaggregated:
        // prod_merc_*/m_money_*, financial (TFF): dealer_*/lev_money_*. Take the first one present.
        const auto pick = [](const QJsonObject& o, std::initializer_list<const char*> names) {
            for (const char* name : names) {
                const QJsonValue v = o[QLatin1String(name)];
                if (!v.isUndefined())
                    return v;
            }
            return QJsonValue();
        };
        for (const auto& v : rows) {
            const auto obj = v.toObject();
            QJsonObject row;
            row["date"] = obj["report_date_as_yyyy_mm_dd"];
            row["market"] = obj["market_and_exchange_names"];
            row["open_interest"] = obj["open_interest_all"];
            row["comm_long"] = pick(obj, {"comm_long_all", "comm_positions_long_all", "prod_merc_positions_long",
                                          "dealer_positions_long_all"});
            row["comm_short"] = pick(obj, {"comm_short_all", "comm_positions_short_all", "prod_merc_positions_short",
                                           "dealer_positions_short_all"});
            row["noncomm_long"] = pick(obj, {"noncomm_long_all", "noncomm_positions_long_all",
                                             "m_money_positions_long_all", "lev_money_positions_long"});
            row["noncomm_short"] = pick(obj, {"noncomm_short_all", "noncomm_positions_short_all",
                                              "m_money_positions_short_all", "lev_money_positions_short"});
            // Speculative net position — the number a COT reader actually tracks week to week.
            const auto as_num = [](const QJsonValue& v, bool* ok) {
                if (v.isDouble()) {
                    *ok = true;
                    return v.toDouble();
                }
                return v.toString().toDouble(ok);
            };
            bool ok_l = false;
            bool ok_s = false;
            const double nl = as_num(row.value(QStringLiteral("noncomm_long")), &ok_l);
            const double ns = as_num(row.value(QStringLiteral("noncomm_short")), &ok_s);
            if (ok_l && ok_s)
                row["noncomm_net"] = nl - ns;
            clean.append(row);
        }
        SeriesMeta meta;
        meta.units = tr("Contracts");
        meta.frequency = tr("Weekly");
        meta.percent = 0;
        meta.value_column = QStringLiteral("noncomm_net");
        meta.note = tr("Positions as of Tuesday, published Friday · net = non-commercial long − short");
        set_series_meta(meta);
        display(clean, "CFTC COT: " + market_combo_->currentText() + " (" + report_type_combo_->currentText() + ")",
                {QStringLiteral("date"), QStringLiteral("noncomm_net"), QStringLiteral("noncomm_long"),
                 QStringLiteral("noncomm_short"), QStringLiteral("comm_long"), QStringLiteral("comm_short"),
                 QStringLiteral("open_interest"), QStringLiteral("market")});
    } else {
        // Historical trend — rows already have clean keys
        SeriesMeta meta;
        meta.units = tr("Contracts");
        meta.frequency = tr("Weekly");
        meta.percent = 0;
        set_series_meta(meta);
        display(rows, "CFTC Historical Trend: " + market_combo_->currentText());
    }

    LOG_DEBUG("CftcPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(request_id));
}

// ── Sentiment display ─────────────────────────────────────────────────────────

void CftcPanel::show_sentiment(const QJsonObject& s) {
    build_sentiment_widget();
    last_sentiment_ = s;

    // Populate labels
    sent_market_lbl_->setText(s["market_name"].toString(market_combo_->currentText()));
    sent_date_lbl_->setText(tr("Report: %1").arg(s["latest_report"].toString("—")));

    const double oi = s["open_interest"].toDouble();
    sent_oi_lbl_->setText(oi > 0 ? ui::formatting::number(oi, 0) : QStringLiteral("—"));

    const QString oi_trend = s["overall_sentiment"].toObject()["oi_trend"].toString();
    sent_oi_trend_->setText(oi_trend.isEmpty() ? "—" : oi_trend.toUpper());
    sent_oi_trend_->setObjectName(oi_trend == "increasing" ? "econStatPos" : "econStatNeg");
    sent_oi_trend_->style()->unpolish(sent_oi_trend_);
    sent_oi_trend_->style()->polish(sent_oi_trend_);

    const QString comm_bias = s["overall_sentiment"].toObject()["commercial_bias"].toString();
    sent_comm_bias_->setText(comm_bias.isEmpty() ? "—" : comm_bias.toUpper());
    sent_comm_bias_->setObjectName(comm_bias == "bullish" ? "econStatPos" : "econStatNeg");
    sent_comm_bias_->style()->unpolish(sent_comm_bias_);
    sent_comm_bias_->style()->polish(sent_comm_bias_);

    const QString noncomm_bias = s["overall_sentiment"].toObject()["non_commercial_bias"].toString();
    sent_noncomm_bias_->setText(noncomm_bias.isEmpty() ? "—" : noncomm_bias.toUpper());
    sent_noncomm_bias_->setObjectName(noncomm_bias == "bullish" ? "econStatPos" : "econStatNeg");
    sent_noncomm_bias_->style()->unpolish(sent_noncomm_bias_);
    sent_noncomm_bias_->style()->polish(sent_noncomm_bias_);

    // NB: QString::arg(qlonglong, int fieldWidth) — the previous `.arg(v, 'd')`
    // passed the *char literal* 'd' (=100) as the field width, padding every
    // net-position number with 100 leading spaces. Format the number first.
    // Signed, grouped contract counts in the user's number format.
    const QJsonObject comm = s["commercial_positions"].toObject();
    const double comm_net = comm["net"].toDouble();
    sent_comm_net_->setText(tr("Net: %1 contracts").arg(ui::formatting::signed_number(comm_net, 0)));

    const QJsonObject noncomm = s["non_commercial_positions"].toObject();
    const double noncomm_net = noncomm["net"].toDouble();
    sent_noncomm_net_->setText(tr("Net: %1 contracts").arg(ui::formatting::signed_number(noncomm_net, 0)));

    // Populate the shared table too (one summary row) so CSV export still has
    // something to write, then switch the stack to the sentiment card view.
    QJsonArray rows;
    QJsonObject row;
    row["market"] = s["market_name"].toString(market_combo_->currentText());
    row["report_date"] = s["latest_report"];
    row["open_interest"] = oi;
    row["comm_net"] = comm_net;
    row["noncomm_net"] = noncomm_net;
    row["comm_bias"] = comm_bias;
    row["noncomm_bias"] = noncomm_bias;
    row["oi_trend"] = oi_trend;
    rows.append(row);
    // A one-row snapshot has no meaningful LATEST/CHANGE/MIN/MAX/AVG.
    set_stats_visible(false);
    display(rows, "CFTC Sentiment: " + s["market_name"].toString(market_combo_->currentText()));
    show_content_page(sentiment_page_);

    LOG_DEBUG("CftcPanel", "Displayed sentiment for " + market_combo_->currentText());
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void CftcPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void CftcPanel::retranslateUi() {
    // Toolbar labels
    if (market_lbl_)
        market_lbl_->setText(tr("MARKET"));
    if (report_lbl_)
        report_lbl_->setText(tr("REPORT"));
    if (view_lbl_)
        view_lbl_->setText(tr("VIEW"));

    // Fixed combo items (report types / views) — markets stay as data.
    if (report_type_combo_ && report_type_combo_->count() >= 3) {
        report_type_combo_->setItemText(0, tr("Legacy"));
        report_type_combo_->setItemText(1, tr("Disaggregated"));
        report_type_combo_->setItemText(2, tr("Financial (TFF)"));
    }
    if (view_combo_ && view_combo_->count() >= 3) {
        view_combo_->setItemText(0, tr("COT Table"));
        view_combo_->setItemText(1, tr("Market Sentiment"));
        view_combo_->setItemText(2, tr("Historical Trend"));
    }

    // Sentiment widget static labels
    if (sent_title_lbl_)
        sent_title_lbl_->setText(tr("COT MARKET SENTIMENT"));
    if (sent_comm_card_lbl_)
        sent_comm_card_lbl_->setText(tr("COMMERCIAL TRADERS"));
    if (sent_comm_card_desc_)
        sent_comm_card_desc_->setText(tr("Hedgers & producers — usually contrarian signal"));
    if (sent_noncomm_card_lbl_)
        sent_noncomm_card_lbl_->setText(tr("NON-COMMERCIAL (SPECULATORS)"));
    if (sent_noncomm_card_desc_)
        sent_noncomm_card_desc_->setText(tr("Managed money & funds — trend-following signal"));
    if (sent_oi_caption_)
        sent_oi_caption_->setText(tr("OPEN INTEREST"));
    if (sent_oi_trend_caption_)
        sent_oi_trend_caption_->setText(tr("OI TREND"));

    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
