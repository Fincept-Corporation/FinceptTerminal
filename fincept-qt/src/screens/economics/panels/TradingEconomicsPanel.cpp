// src/screens/economics/panels/TradingEconomicsPanel.cpp
// Trading Economics panel. Requires TRADING_ECONOMICS_API_KEY env var.
//
// Commands (with key): ratings, us_treasuries, european_bonds, yield_curve <country>,
//   country_data <country>, calendar. Countries go to the API by name ("united states"), the form
//   its URLs take. "ratings" answers one row per country and agency (Country, Date, Agency, Rating,
//   Outlook) — the API has no agency filter, so there is no separate by-agency dataset.
// Response shape varies per command — normalised via extract_te_rows().
#include "screens/economics/panels/TradingEconomicsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kTradingEconomicsScript = "trading_economics_data.py";
static constexpr const char* kTradingEconomicsSourceId = "trading_economics";
static constexpr const char* kTradingEconomicsColor = "#F59E0B"; // amber
} // namespace

struct TeDataset {
    QString label;
    QString command;
    QString country_arg; // "required", "optional", or ""
};

static const QList<TeDataset> kTEDatasets = {
    {"Credit Ratings (All)", "ratings", ""},
    {"US Treasuries", "us_treasuries", ""},
    {"European Bonds", "european_bonds", ""},
    {"Bond Yield Curve", "yield_curve", "required"},
    {"Country Economic Data", "country_data", "required"},
    {"Economic Calendar", "calendar", ""},
};

static const QList<QPair<QString, QString>> kTECountries = {
    {"United States", "united states"},
    {"Germany", "germany"},
    {"Japan", "japan"},
    {"United Kingdom", "united kingdom"},
    {"France", "france"},
    {"Italy", "italy"},
    {"China", "china"},
    {"Brazil", "brazil"},
    {"India", "india"},
    {"Australia", "australia"},
    {"Canada", "canada"},
    {"Spain", "spain"},
};

TradingEconomicsPanel::TradingEconomicsPanel(QWidget* parent)
    : EconPanelBase(kTradingEconomicsSourceId, kTradingEconomicsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &TradingEconomicsPanel::on_result);
}

void TradingEconomicsPanel::activate() {
    show_empty(tr("Set TRADING_ECONOMICS_API_KEY environment variable, then select a dataset and click FETCH\n"
                  "Get a key at: tradingeconomics.com/api"));
}

void TradingEconomicsPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& d : kTEDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(200);

    country_combo_ = new QComboBox;
    for (const auto& c : kTECountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setFixedHeight(26);
    country_combo_->setMinimumWidth(130);
    country_combo_->setToolTip(tr("Required for yield_curve and country_data"));

    thl->addWidget(dataset_lbl_ = lbl(tr("DATASET")));
    thl->addWidget(dataset_combo_);
    thl->addWidget(country_lbl_ = lbl(tr("COUNTRY")));
    thl->addWidget(country_combo_);
}

void TradingEconomicsPanel::on_fetch() {
    const int idx = dataset_combo_->currentIndex();
    const auto& dataset = kTEDatasets[idx];
    const QString country = country_combo_->currentData().toString();

    show_loading(tr("Fetching Trading Economics: %1…").arg(dataset.label));

    // EconomicsService::execute() prepends `command` to the argv. Repeating it
    // here made the script read the command name as the country
    // (`yield_curve yield_curve US` → country="yield_curve"), so every
    // country-scoped Trading Economics query hit the wrong endpoint.
    QStringList args;
    if (dataset.country_arg == "required" || dataset.country_arg == "optional")
        args << country;

    services::EconomicsService::instance().execute(kTradingEconomicsSourceId, kTradingEconomicsScript, dataset.command,
                                                   args,
                                                   "te_" + dataset.command + (args.isEmpty() ? "" : "_" + country));
}

// Normalise various Trading Economics response shapes into displayable rows.
static QJsonArray extract_te_rows(const QJsonObject& data) {
    // Shape 1: direct array in "data" key
    QJsonArray arr = data["data"].toArray();
    if (!arr.isEmpty())
        return arr;

    // Shape 2: ratings array
    arr = data["ratings"].toArray();
    if (!arr.isEmpty())
        return arr;

    // Shape 3: bonds / treasuries / yield_curve
    arr = data["bonds"].toArray();
    if (!arr.isEmpty())
        return arr;

    arr = data["treasuries"].toArray();
    if (!arr.isEmpty())
        return arr;

    arr = data["yield_curve"].toArray();
    if (!arr.isEmpty())
        return arr;

    // Shape 4: events / calendar
    arr = data["events"].toArray();
    if (!arr.isEmpty())
        return arr;

    // Shape 5: country_data — {country, credit_ratings: {data: [...]}, bond_yields: {data: [...]},
    // summary}: list the rating rows and the bond rows (each part may hold an {error} instead).
    if (data.contains("credit_ratings") || data.contains("bond_yields")) {
        QJsonArray rows;
        for (const char* part : {"credit_ratings", "bond_yields"})
            for (const auto& v : data.value(QLatin1String(part)).toObject().value(QStringLiteral("data")).toArray())
                rows.append(v);
        return rows;
    }

    return {};
}

void TradingEconomicsPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kTradingEconomicsSourceId)
        return;
    if (!request_id.startsWith("te_"))
        return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("API key") || msg.contains("TRADING_ECONOMICS_API_KEY")) {
            show_error(tr("Trading Economics API key not configured.\n"
                          "Set TRADING_ECONOMICS_API_KEY environment variable.\n"
                          "Get a key at: tradingeconomics.com/api"));
        } else {
            show_error(msg);
        }
        return;
    }

    // Also check for inline error field
    if (result.data.contains("error") && !result.data["error"].toBool(false)) {
        const QString err = result.data["error"].toString();
        if (!err.isEmpty() && err != "false") {
            if (err.contains("API key") || err.contains("TRADING_ECONOMICS")) {
                show_error(tr("Trading Economics API key not configured.\n"
                              "Set TRADING_ECONOMICS_API_KEY environment variable.\n"
                              "Get a key at: tradingeconomics.com/api"));
            } else {
                show_error(err);
            }
            return;
        }
    }

    const QJsonArray rows = extract_te_rows(result.data);
    if (rows.isEmpty()) {
        show_error(tr("No data returned — API key may be required"));
        return;
    }

    // Label from the request id ("te_<command>[_<country>]"), not the combo the user may have changed.
    QString title = "Trading Economics: " + request_id.mid(3);
    for (const auto& d : kTEDatasets) {
        if (request_id == "te_" + d.command || request_id.startsWith("te_" + d.command + "_")) {
            title = "Trading Economics: " + d.label;
            if (request_id.size() > QString("te_" + d.command + "_").size())
                title += " — " + request_id.mid(QString("te_" + d.command + "_").size());
        }
    }
    display(rows, title);
    LOG_INFO("TradingEconomicsPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void TradingEconomicsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void TradingEconomicsPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (country_combo_)
        country_combo_->setToolTip(tr("Required for yield_curve and country_data"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
