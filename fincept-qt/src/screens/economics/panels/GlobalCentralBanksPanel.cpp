// src/screens/economics/panels/GlobalCentralBanksPanel.cpp
// Global Central Banks — BOE, RBA, Bank of Canada, Riksbank, SNB, Norges Bank.
// No API key required for any source.
//
// Response shapes (all use {success, data:[{date, <value_col>}]} or similar):
//   BOE:     { success, data:[{date:"02 Jan 1975", <CDID>:value}] }
//   RBA:     { success, table, data:[{date:"04-Jan-2011", <col>:value}] }
//   BOC:     { success, series, data:[{date:"2026-02-01", <series_id>:value}] }
//   Riksbank:{ success, series_id, data:[{date:"2025-03-28", value:2.25}] }
//   SNB:     { success, cube, data:[{date:"2026-03-16", <col>:value}] }
//   Norges:  { success, flow, data:[{date:"2026-03-17", <col>:value}] }
#include "screens/economics/panels/GlobalCentralBanksPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kGlobalCentralBanksSourceId = "global_cb";
static constexpr const char* kGlobalCentralBanksColor = "#6366F1"; // indigo
} // namespace

// ── Per-bank descriptor ──────────────────────────────────────────────────────

struct CbSeries {
    QString label;
    QString command;
};

struct CbBank {
    QString label;      // display name
    QString script;     // python script filename
    QString req_prefix; // request_id prefix
    QList<CbSeries> series;
};

static const QList<CbBank> kBanks = {
    {"BOE — Bank of England",
     "boe_data.py",
     "boe",
     {
         {"Bank Rate", "bank_rate"},
         {"SONIA Overnight Rate", "sonia"},
         {"Exchange Rates (GBP)", "exchange_rates"},
         {"Monetary Aggregates (M0/M4)", "monetary_aggregates"},
         {"Quoted Interest Rates", "quoted_rates"},
     }},
    {"RBA — Reserve Bank of Australia",
     "rba_data.py",
     "rba",
     {
         {"Cash Rate (F1)", "cash_rate"},
         {"Bond Yields (F2)", "bond_yields"},
         {"Exchange Rates (F11)", "exchange_rates"},
         {"Inflation / CPI", "inflation"},
         {"Lending Rates", "lending_rates"},
         {"Monetary Aggregates", "monetary"},
     }},
    {"BOC — Bank of Canada",
     "boc_data.py",
     "boc",
     {
         {"Overnight Policy Rate", "policy_rate"},
         {"CORRA", "corra"},
         {"Prime Rate", "prime"},
         {"USD/CAD", "usd"},
         {"EUR/CAD", "eur"},
     }},
    {"Riksbank — Sweden",
     "riksbank_data.py",
     "riksbank",
     {
         {"Policy Rate", "policy_rate"},
         {"Policy + Deposit + Lending", "policy_all"},
         {"T-Bills (1M-6M)", "tbills"},
         {"Mortgage Bond Yields", "mortgage"},
     }},
    {"SNB — Swiss National Bank",
     "snb_data.py",
     "snb",
     {
         {"Policy Rate + SARON", "policy_rate"},
         {"Bond Yields (Monthly)", "bond_yields"},
         {"Bond Yields (Daily)", "bond_yields_d"},
         {"CHF Exchange Rates", "exchange_rates"},
     }},
    {"Norges Bank — Norway",
     "norges_bank_data.py",
     "norges",
     {
         {"Policy Rate Announcements", "policy_rate"},
         {"Exchange Rates (NOK)", "exchange_rates"},
         {"NIBOR / Interest Rates", "interest_rates"},
     }},
    // ── Central & Eastern Europe, Middle East, SE Asia ───────────────────────
    // These eight connectors already shipped in scripts/ and were already
    // exposed as MCP tools (DataConnectorManifest.inc) — the AI could query
    // them, a human could not. None requires an API key. Commands below are
    // taken verbatim from that manifest.
    {"CNB — Czech National Bank",
     "cnb_data.py",
     "cnb",
     {
         {"PRIBOR (History)", "pribor_history"},
         {"PRIBOR (Year)", "pribor_year"},
         {"PRIBOR (Latest)", "pribor"},
         {"CZEONIA (Year)", "czeonia_year"},
         {"CZEONIA (Latest)", "czeonia"},
         {"Exchange Rates (Year)", "exchange_rates_y"},
         {"Exchange Rates (Latest)", "exchange_rates"},
         {"Monthly Average FX", "monthly_avg"},
         {"Open Market Operations", "omo"},
         {"Overview", "overview"},
     }},
    {"NBP — National Bank of Poland",
     "nbp_data.py",
     "nbp",
     {
         {"Exchange Rates (Range)", "range"},
         {"Major Currencies", "major"},
         {"USD/PLN", "usd"},
         {"EUR/PLN", "eur"},
         {"Bid/Ask Spreads", "bid_ask"},
         {"Single Currency", "currency"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"MNB — National Bank of Hungary",
     "mnb_data.py",
     "mnb",
     {
         {"Exchange Rates (Range)", "range"},
         {"Major Currencies", "major"},
         {"USD/HUF", "usd"},
         {"EUR/HUF", "eur"},
         {"Single Currency", "currency"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"BNR — National Bank of Romania",
     "bnr_data.py",
     "bnr",
     {
         {"Exchange Rates (Year)", "year"},
         {"Exchange Rates (Range)", "range"},
         {"Single Currency", "currency"},
         {"Major Currencies", "major"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"HNB — Croatian National Bank",
     "hnb_data.py",
     "hnb",
     {
         {"Exchange Rates (Range)", "range"},
         {"Single Currency", "currency"},
         {"USD/EUR", "usd"},
         {"GBP", "gbp"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"TCMB — Central Bank of Türkiye",
     "tcmb_data.py",
     "tcmb",
     {
         {"Exchange Rates (Range)", "range"},
         {"Major Currencies", "major"},
         {"Single Currency", "currency"},
         {"By Date", "date"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"BOI — Bank of Israel",
     "boi_data.py",
     "boi",
     {
         {"All Exchange Rates", "all"},
         {"USD/ILS", "usd"},
         {"EUR/ILS", "eur"},
         {"Today", "today"},
         {"Overview", "overview"},
     }},
    {"BNM — Bank Negara Malaysia",
     "bnm_data.py",
     "bnm",
     {
         {"Overnight Policy Rate (OPR)", "opr"},
         {"Major Currencies", "major"},
         {"ASEAN Currencies", "asean"},
         {"Single Currency", "currency"},
         {"Trading Sessions", "sessions"},
         {"Overview", "overview"},
     }},
};

// ── Per-series extra arguments ───────────────────────────────────────────────
// Several connector commands cannot run without an argument ("range requires <start> <end>",
// "currency requires <CCY>", "date requires <YYYY-MM-DD>") and the panel used to call them with
// none, so those series always answered with a usage error.
enum class CbArgKind { None, Ccy, Range, CcyRange, Date };

static CbArgKind cb_arg_kind(const QString& script, const QString& command) {
    if (command == "range")
        return script == "hnb_data.py" ? CbArgKind::None // HNB defaults to the last 20 bulletins
                                       : CbArgKind::Range;
    if (command == "currency")
        return (script == "hnb_data.py" || script == "bnm_data.py") ? CbArgKind::Ccy : CbArgKind::CcyRange;
    if (command == "sessions" && script == "bnm_data.py")
        return CbArgKind::Ccy;
    if (command == "date" && script == "tcmb_data.py")
        return CbArgKind::Date;
    return CbArgKind::None;
}

// ── Flatten helpers ──────────────────────────────────────────────────────────

// Bank responses put their payload under "data" with a "date" key and one or
// more numeric value columns. We keep all columns as-is.
//
// The shape is NOT uniform per script — it varies per COMMAND. Series commands
// ("year", "range", "currency", "pribor_history") return an array of rows, but
// point-in-time commands ("today", "date", "overview", "skd") return a single
// object: e.g. bnr_data.py returns `"data": latest`, cnb_data.py returns
// `"data": rows[0] if rows else {}`, tcmb_data.py returns `"data": snapshot`.
// A bare .toArray() silently yields an empty array for all of those, so the
// panel rendered "no data" for a request that actually succeeded. Normalise
// both into a row list.
static QJsonArray extract_cb_rows(const QJsonObject& data) {
    const QJsonValue payload = data["data"];
    if (payload.isArray())
        return payload.toArray();
    if (payload.isObject()) {
        const QJsonObject obj = payload.toObject();
        if (obj.isEmpty())
            return {};
        // Some point-in-time payloads nest the real rows one level down
        // (boi_data.py "overview" → {"data": {"rates": [...]}}). Prefer a
        // nested array over presenting the wrapper object as a single row.
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it.value().isArray() && !it.value().toArray().isEmpty())
                return it.value().toArray();
        }
        // A single wrapper key holding one object (cnb "czeonia" gives {"czeoniaDaily": {...}}):
        // that object is the row.
        if (obj.size() == 1 && obj.begin().value().isObject())
            return QJsonArray{obj.begin().value().toObject()};
        // Object of objects keyed by currency (tcmb "today" gives {"USD": {...}, "EUR": {...}}): one
        // row per key, with the key as a "currency" column. As a single row every value was an
        // object (rendered blank, then dropped as "no numeric value" -> "No data returned").
        bool all_objects = true;
        for (auto it = obj.begin(); it != obj.end(); ++it)
            all_objects = all_objects && it.value().isObject();
        if (all_objects) {
            QJsonArray rows;
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                QJsonObject row = it.value().toObject();
                row.insert(QStringLiteral("currency"), it.key());
                rows.append(row);
            }
            return rows;
        }
        return QJsonArray{obj}; // genuine single row
    }
    return {};
}

// ── Panel ────────────────────────────────────────────────────────────────────

GlobalCentralBanksPanel::GlobalCentralBanksPanel(QWidget* parent)
    : EconPanelBase(kGlobalCentralBanksSourceId, kGlobalCentralBanksColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &GlobalCentralBanksPanel::on_result);
}

void GlobalCentralBanksPanel::activate() {
    show_empty(tr("Select a central bank and series, then click FETCH\n"
                  "Sources: BOE, RBA, Bank of Canada, Riksbank, SNB, Norges Bank,\n"
                  "CNB (Czechia), NBP (Poland), MNB (Hungary), BNR (Romania),\n"
                  "HNB (Croatia), TCMB (Türkiye), BOI (Israel), BNM (Malaysia)\n"
                  "No API key required for any source"));
}

void GlobalCentralBanksPanel::build_controls(QHBoxLayout* thl) {
    bank_lbl_ = new QLabel(tr("BANK"));
    bank_lbl_->setStyleSheet(ctrl_label_style());

    bank_combo_ = new QComboBox;
    for (const auto& b : kBanks)
        bank_combo_->addItem(b.label);
    bank_combo_->setFixedHeight(26);
    bank_combo_->setMinimumWidth(230);

    series_lbl_ = new QLabel(tr("SERIES"));
    series_lbl_->setStyleSheet(ctrl_label_style());

    series_combo_ = new QComboBox;
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(200);

    ccy_lbl_ = new QLabel(tr("CCY"));
    ccy_lbl_->setObjectName(QStringLiteral("econCtrlLabel")); // styled by the panel stylesheet (no per-label parse)
    ccy_edit_ = new QLineEdit(QStringLiteral("USD"));
    ccy_edit_->setMaxLength(3);
    ccy_edit_->setFixedWidth(52);
    ccy_edit_->setFixedHeight(26);
    ccy_edit_->setToolTip(tr("ISO currency code for the single-currency series (e.g. USD, EUR, GBP)"));
    ccy_edit_->setAccessibleName(tr("Currency code"));

    days_lbl_ = new QLabel(tr("DAYS"));
    days_lbl_->setObjectName(QStringLiteral("econCtrlLabel")); // styled by the panel stylesheet (no per-label parse)
    days_spin_ = new QSpinBox;
    days_spin_->setRange(1, 365);
    days_spin_->setValue(30);
    days_spin_->setFixedHeight(26);
    days_spin_->setToolTip(tr("Look-back window for date-range series"));
    days_spin_->setAccessibleName(tr("Look-back days"));

    // Populate series for the initial bank
    update_series_for_bank(0);

    connect(bank_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &GlobalCentralBanksPanel::update_series_for_bank);
    connect(series_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { update_arg_controls(); });

    thl->addWidget(bank_lbl_);
    thl->addWidget(bank_combo_);
    thl->addSpacing(8);
    thl->addWidget(series_lbl_);
    thl->addWidget(series_combo_);
    thl->addSpacing(8);
    thl->addWidget(ccy_lbl_);
    thl->addWidget(ccy_edit_);
    thl->addWidget(days_lbl_);
    thl->addWidget(days_spin_);
    update_arg_controls();
}

void GlobalCentralBanksPanel::update_series_for_bank(int bank_idx) {
    if (bank_idx < 0 || bank_idx >= kBanks.size())
        return;
    series_combo_->clear();
    for (const auto& s : kBanks[bank_idx].series)
        series_combo_->addItem(s.label, s.command);
    update_arg_controls();
}

void GlobalCentralBanksPanel::update_arg_controls() {
    if (!ccy_edit_ || !days_spin_)
        return;
    const int bi = bank_combo_ ? bank_combo_->currentIndex() : -1;
    const QString command = series_combo_ ? series_combo_->currentData().toString() : QString();
    CbArgKind kind = CbArgKind::None;
    if (bi >= 0 && bi < kBanks.size())
        kind = cb_arg_kind(kBanks[bi].script, command);
    const bool needs_ccy = kind == CbArgKind::Ccy || kind == CbArgKind::CcyRange;
    const bool needs_days = kind == CbArgKind::Range || kind == CbArgKind::CcyRange;
    ccy_edit_->setEnabled(needs_ccy);
    ccy_lbl_->setEnabled(needs_ccy);
    days_spin_->setEnabled(needs_days);
    days_lbl_->setEnabled(needs_days);
}

void GlobalCentralBanksPanel::on_fetch() {
    const int bi = bank_combo_->currentIndex();
    const int si = series_combo_->currentIndex();
    if (bi < 0 || bi >= kBanks.size())
        return;
    if (si < 0 || si >= kBanks[bi].series.size())
        return;

    const auto& bank = kBanks[bi];
    const auto& series = bank.series[si];

    // Arguments the selected command cannot run without (see cb_arg_kind()).
    QStringList args;
    const CbArgKind kind = cb_arg_kind(bank.script, series.command);
    const QString ccy = ccy_edit_ ? ccy_edit_->text().trimmed().toUpper() : QString();
    if ((kind == CbArgKind::Ccy || kind == CbArgKind::CcyRange) && ccy.size() != 3) {
        show_empty(tr("Enter a 3-letter currency code (e.g. USD)"));
        return;
    }
    if (kind == CbArgKind::Ccy || kind == CbArgKind::CcyRange)
        args << ccy;
    if (kind == CbArgKind::Range || kind == CbArgKind::CcyRange) {
        const QDate end = QDate::currentDate();
        args << end.addDays(-(days_spin_ ? days_spin_->value() : 30)).toString(Qt::ISODate)
             << end.toString(Qt::ISODate);
    }
    if (kind == CbArgKind::Date) {
        QDate d = QDate::currentDate();
        while (d.dayOfWeek() >= 6) // bulletins are published on business days only
            d = d.addDays(-1);
        args << d.toString(Qt::ISODate);
    }

    show_loading(tr("Fetching %1: %2…").arg(bank.label, series.label));
    services::EconomicsService::instance().execute(kGlobalCentralBanksSourceId, bank.script, series.command, args,
                                                   bank.req_prefix + "_" + series.command);
}

void GlobalCentralBanksPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kGlobalCentralBanksSourceId)
        return;

    // Check this result belongs to one of our banks
    bool matched = false;
    int matched_bank = -1;
    int matched_series = -1;
    for (int bi = 0; bi < kBanks.size(); ++bi) {
        const auto& bank = kBanks[bi];
        if (!request_id.startsWith(bank.req_prefix + "_"))
            continue;
        matched = true;
        matched_bank = bi;
        const QString cmd = request_id.mid(bank.req_prefix.size() + 1);
        for (int si = 0; si < bank.series.size(); ++si) {
            if (bank.series[si].command == cmd) {
                matched_series = si;
                break;
            }
        }
        break;
    }
    if (!matched)
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        show_error(inline_err);
        return;
    }

    QJsonArray raw = extract_cb_rows(result.data);

    // Filter rows: keep only those with at least one numeric value
    QJsonArray rows;
    for (const auto& rv : raw) {
        const QJsonObject r = rv.toObject();
        bool has_value = false;
        for (const auto& key : r.keys()) {
            if (key == "date")
                continue;
            if (r[key].isDouble()) {
                has_value = true;
                break;
            }
        }
        if (has_value)
            rows.append(r);
    }

    if (rows.isEmpty()) {
        show_error(tr("No data returned"));
        return;
    }

    const QString bank_name = matched_bank >= 0 ? kBanks[matched_bank].label : "";
    const QString series_name =
        (matched_bank >= 0 && matched_series >= 0) ? kBanks[matched_bank].series[matched_series].label : request_id;
    const QString title = bank_name + ": " + series_name;

    display(rows, title);
    LOG_INFO("GlobalCentralBanksPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void GlobalCentralBanksPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void GlobalCentralBanksPanel::retranslateUi() {
    if (bank_lbl_)
        bank_lbl_->setText(tr("BANK"));
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    if (ccy_lbl_)
        ccy_lbl_->setText(tr("CCY"));
    if (days_lbl_)
        days_lbl_->setText(tr("DAYS"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
