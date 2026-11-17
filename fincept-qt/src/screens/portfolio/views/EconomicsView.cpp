// src/screens/portfolio/views/EconomicsView.cpp
#include "screens/portfolio/views/EconomicsView.h"

#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "screens/portfolio/PortfolioStress.h"
#include "services/economics/EconomicsService.h"
#include "services/portfolio/PortfolioClassify.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"

#include <QColor>
#include <QCoreApplication>
#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

// Correlates our EconomicsService::execute() call with its result_ready signal.
static const QString kEconViewMacroRequestId = QStringLiteral("portfolio_macro");
// Remembered FRED result (CacheManager, via the pane) and how long it stays fresh.
static const QString kEconViewMacroCacheKey = QStringLiteral("portfolio/economics/fred_macro");
constexpr int kEconViewMacroTtlDays = 7;
constexpr int kEconViewMacroMaxAgeS = 6 * 3600;
// The user's own stress scenarios, as RISK › STRESS stores them (StressTestPanel.cpp).
constexpr const char* kEconViewStressUserKey = "portfolio.stress.user_scenarios";

// Table row height: `design_h` at the default font size, growing with it.
static int econ_row_height(int design_h) {
    return std::max(design_h, ui::fonts::font_px(-3) + design_h - 11);
}

static QString econ_table_qss() {
    return QString("QTableWidget { background:%1; color:%2; border:none; font-size:" + fincept::ui::fonts::px(-3) +
                   "px; }"
                   "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                   "QHeaderView::section { background:%4; color:%5; border:none;"
                   "  border-bottom:2px solid %6; padding:3px 8px; font-size:" +
                   fincept::ui::fonts::px(-5) +
                   "px;"
                   "  font-weight:700; letter-spacing:0.5px; }")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(),
             ui::colors::TEXT_SECONDARY(), ui::colors::AMBER());
}

// One selector stylesheet for the view's own chrome (titles, notes, link buttons),
// set on the view before any child exists.
static QString econ_view_qss() {
    return QString("QLabel#econTitle { color:%1; font-size:" + fincept::ui::fonts::px(-3) +
                   "px; font-weight:700; letter-spacing:1px; }"
                   "QLabel#econNote { color:%2; font-size:" +
                   fincept::ui::fonts::px(-5) +
                   "px; }"
                   "QLabel#econStatus { color:%3; font-size:" +
                   fincept::ui::fonts::px(-5) +
                   "px; }"
                   "QPushButton#econLink { background:transparent; color:%1; border:1px solid %4;"
                   "  font-size:" +
                   fincept::ui::fonts::px(-5) +
                   "px; font-weight:700; letter-spacing:1px; padding:2px 10px; }"
                   "QPushButton#econLink:hover { border-color:%1; }"
                   "QWidget#econSep { background:%5; }")
        .arg(ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_SECONDARY(),
             ui::colors::BORDER_MED(), ui::colors::BORDER_DIM());
}

namespace {
// FRED series → labels and value kind, in display order. Labels are marked with
// QT_TRANSLATE_NOOP so lupdate can extract them.
enum class MacroKind { Rate, Index, UsdPrice };
struct MacroSeries {
    const char* id;
    const char* label; ///< tile label (short)
    const char* name;  ///< full name (tooltip)
    MacroKind kind;
};
const MacroSeries kMacroSeries[] = {
    {"DGS10", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "10Y TREASURY"),
     QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "10Y Treasury Yield"), MacroKind::Rate},
    {"T10YIE", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "INFLATION (10Y BE)"),
     QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Inflation (10Y breakeven)"), MacroKind::Rate},
    {"A191RL1Q225SBEA", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "REAL GDP GROWTH"),
     QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Real GDP Growth (annualised, quarterly)"), MacroKind::Rate},
    {"DTWEXBGS", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "USD INDEX"),
     QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "USD Index (broad, trade-weighted)"), MacroKind::Index},
    {"DCOILWTICO", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "WTI CRUDE"),
     QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "WTI Crude Oil, per barrel"), MacroKind::UsdPrice},
};
constexpr int kMacroSeriesCount = static_cast<int>(sizeof(kMacroSeries) / sizeof(kMacroSeries[0]));

QString econ_tr(const char* s) {
    return QCoreApplication::translate("fincept::screens::EconomicsView", s);
}

/// First line of a script / service error, without a traceback, at most 300 characters.
QString econ_plain_error(const QString& raw) {
    QString s = raw.trimmed();
    const qsizetype tb = s.indexOf(QLatin1String("Traceback"));
    if (tb > 0)
        s = s.left(tb).trimmed();
    const qsizetype nl = s.indexOf(QLatin1Char('\n'));
    if (nl > 0)
        s = s.left(nl).trimmed();
    if (s.size() > 300)
        s = s.left(300) + QStringLiteral("…");
    return s;
}

// The macro presets of RISK › STRESS: a scenario driven by rates, inflation, currency,
// commodities or the broad equity market. Order = display order of the driver label.
struct MacroPreset {
    const char* id;
    const char* driver;
};
const MacroPreset kMacroPresets[] = {
    {"rates_300", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Rates")},
    {"uk_gilts_2022", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Rates")},
    {"taper_2013", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Rates · FX")},
    {"inflation", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Inflation")},
    {"rates_2022", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Inflation · Rates")},
    {"em_fx_crisis", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Currency")},
    {"brexit_2016", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Currency")},
    {"geopolitical", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Commodities")},
    {"gfc_2008", QT_TRANSLATE_NOOP("fincept::screens::EconomicsView", "Equity market")},
};
} // namespace

EconomicsView::EconomicsView(QWidget* parent) : QWidget(parent) {
    setStyleSheet(econ_view_qss()); // before any child exists
    build_ui();
    // Table items carry their colours: refill them once the new appearance has settled
    // (the refill sizes the shocks table's columns — never inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &EconomicsView::refresh_theme);
    // Currency / number format changed: re-format the held book and the last FRED levels.
    ui::formatting::on_format_changed(this, [this]() {
        if (has_data_) {
            update_indicators();
            update_shocks();
        }
        if (macro_pane_ && macro_pane_->has_result())
            render_macro(macro_pane_->payload());
    });

    // Macro levels arrive through EconomicsService (the gateway for fred_data.py:
    // it caches and paces the Python spawn). It has no cancel token: a reply for a
    // request the pane no longer waits for (left the view, superseded) is dropped.
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            [this](const QString& request_id, const services::EconomicsResult& res) {
                if (request_id != kEconViewMacroRequestId || macro_pending_id_ == 0)
                    return;
                const quint64 id = std::exchange(macro_pending_id_, 0);
                if (!macro_pane_ || !macro_pane_->is_current(id))
                    return;
                if (!res.success) {
                    const QString why = econ_plain_error(res.error);
                    LOG_WARN("EconomicsView", logf("FRED macro fetch failed", {{"error", why}}));
                    macro_pane_->fail(id, why.isEmpty() ? tr("FRED did not answer. Check the connection and try again.")
                                                        : why);
                    return;
                }
                if (res.data.value(QStringLiteral("error")).isString()) {
                    macro_pane_->fail(id, econ_plain_error(res.data.value(QStringLiteral("error")).toString()));
                    return;
                }

                // Per series: latest value + date, the value ~1 month earlier (Δ 1M) and
                // the ~120 days fetched as a sparkline.
                QJsonObject series;
                int got = 0;
                QString first_error;
                for (const auto v : res.data.value(QStringLiteral("data")).toArray()) {
                    const QJsonObject o = v.toObject();
                    const QString sid = o.value(QStringLiteral("series_id")).toString();
                    if (sid.isEmpty())
                        continue;
                    if (o.contains(QStringLiteral("error"))) {
                        const QString why = econ_plain_error(o.value(QStringLiteral("error")).toString());
                        if (first_error.isEmpty())
                            first_error = why;
                        series.insert(sid, QJsonObject{{QStringLiteral("error"), why}});
                        continue;
                    }
                    const QJsonArray obs = o.value(QStringLiteral("observations")).toArray();
                    QJsonArray spark;
                    QString last_date;
                    double last = std::nan("");
                    for (const auto ov : obs) {
                        const QJsonObject ob = ov.toObject();
                        const QJsonValue val = ob.value(QStringLiteral("value"));
                        if (!val.isDouble() || !std::isfinite(val.toDouble()))
                            continue;
                        spark.append(val.toDouble());
                        last = val.toDouble();
                        last_date = ob.value(QStringLiteral("date")).toString();
                    }
                    if (!std::isfinite(last)) {
                        series.insert(sid, QJsonObject{{QStringLiteral("error"), tr("No observation in the last 120 days.")}});
                        continue;
                    }
                    QJsonObject s{{QStringLiteral("value"), last}, {QStringLiteral("date"), last_date},
                                  {QStringLiteral("spark"), spark}};
                    const QDate ld = QDate::fromString(last_date, Qt::ISODate);
                    if (ld.isValid()) {
                        const QString cutoff = ld.addMonths(-1).toString(Qt::ISODate);
                        for (qsizetype i = obs.size() - 1; i >= 0; --i) {
                            const QJsonObject ob = obs.at(i).toObject();
                            if (ob.value(QStringLiteral("date")).toString() <= cutoff &&
                                ob.value(QStringLiteral("value")).isDouble()) {
                                s.insert(QStringLiteral("prior"), ob.value(QStringLiteral("value")).toDouble());
                                break;
                            }
                        }
                    }
                    series.insert(sid, s);
                    ++got;
                }
                if (got == 0) {
                    macro_pane_->fail(id, first_error.isEmpty() ? tr("FRED returned no observations for these series.")
                                                                : first_error);
                    return;
                }
                const QDateTime at = res.fetched_at.isValid() ? res.fetched_at : QDateTime::currentDateTimeUtc();
                macro_pane_->deliver(id, QJsonObject{{QStringLiteral("series"), series}}, at);
            });

    // Betas from the market-analytics run move each equity line in MACRO SHOCKS.
    connect(&services::PortfolioService::instance(), &services::PortfolioService::market_analytics_loaded, this,
            [this](const QString& pid, bool ok, const QString&) {
                if (ok && has_data_ && pid == summary_.portfolio.id && isVisible())
                    update_shocks();
            });
}

EconomicsView::~EconomicsView() {
    // The pane outlives this object's members (it is deleted with the children):
    // its destructor must not call back into them.
    if (macro_pane_)
        macro_pane_->set_cancel({});
}

void EconomicsView::refresh_theme() {
    // The pane's minimum height follows the font size.
    const int min_h = ui::fonts::font_px(0) * 14;
    if (macro_pane_ && macro_pane_->minimumHeight() != min_h)
        macro_pane_->setMinimumHeight(min_h);
    // Table items carry their colours; the KPI strip, chart and panes paint from the live theme.
    if (has_data_) {
        update_indicators();
        update_shocks();
    }
}

void EconomicsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top: Portfolio Holdings Economics ────────────────────────────────────
    auto* ind_section = new QWidget(this);
    auto* ind_layout = new QVBoxLayout(ind_section);
    ind_layout->setContentsMargins(12, 8, 12, 8);
    ind_layout->setSpacing(4);

    ind_title_ = new QLabel(tr("PORTFOLIO ECONOMICS OVERVIEW"), ind_section);
    ind_title_->setObjectName(QStringLiteral("econTitle"));
    ind_layout->addWidget(ind_title_);

    ind_note_ = new QLabel(tr("Per-holding contribution to portfolio value, P&L, and risk"), ind_section);
    ind_note_->setObjectName(QStringLiteral("econNote"));
    ind_layout->addWidget(ind_note_);

    indicators_table_ = new QTableWidget(ind_section);
    indicators_table_->setColumnCount(7);
    indicators_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("SECTOR"), tr("WEIGHT"), tr("COST BASIS"), tr("MARKET VALUE"), tr("P&L"), tr("P&L %")});
    indicators_table_->setSelectionMode(QAbstractItemView::NoSelection);
    indicators_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    indicators_table_->setShowGrid(false);
    indicators_table_->verticalHeader()->setVisible(false);
    indicators_table_->horizontalHeader()->setStretchLastSection(true);
    indicators_table_->setColumnWidth(0, 80);
    indicators_table_->setColumnWidth(1, 150);
    indicators_table_->setColumnWidth(2, 70);
    indicators_table_->setColumnWidth(3, 110);
    indicators_table_->setColumnWidth(4, 110);
    indicators_table_->setColumnWidth(5, 110);
    indicators_table_->setStyleSheet(econ_table_qss());
    ind_layout->addWidget(indicators_table_, 1);
    layout->addWidget(ind_section, 4);

    auto* sep = new QWidget(this);
    sep->setObjectName(QStringLiteral("econSep"));
    sep->setAttribute(Qt::WA_StyledBackground, true);
    sep->setFixedHeight(1);
    layout->addWidget(sep);

    layout->addWidget(build_macro_section(this), 3);

    auto* sep2 = new QWidget(this);
    sep2->setObjectName(QStringLiteral("econSep"));
    sep2->setAttribute(Qt::WA_StyledBackground, true);
    sep2->setFixedHeight(1);
    layout->addWidget(sep2);

    layout->addWidget(build_shocks_section(this), 4);
}

// ── US macro conditions (FRED) ───────────────────────────────────────────────

QWidget* EconomicsView::build_macro_section(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* sl = new QVBoxLayout(section);
    sl->setContentsMargins(12, 8, 12, 8);
    sl->setSpacing(6);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(8);
    macro_title_ = new QLabel(tr("US MACRO CONDITIONS  (LIVE · FRED)"), section);
    macro_title_->setObjectName(QStringLiteral("econTitle"));
    macro_title_->setToolTip(tr("United States series from FRED (Federal Reserve Bank of St. Louis). They describe the "
                                "US economy whatever this portfolio's base currency."));
    head->addWidget(macro_title_);
    macro_status_ = new QLabel(section);
    macro_status_->setObjectName(QStringLiteral("econStatus"));
    head->addWidget(macro_status_);
    head->addStretch(1);
    // Inline key entry next to the needs-setup page's "OPEN SETTINGS > CREDENTIALS".
    macro_key_btn_ = new QPushButton(tr("PASTE FRED KEY HERE"), section);
    macro_key_btn_->setObjectName(QStringLiteral("econLink"));
    macro_key_btn_->setProperty("fincept_agent_risk", "account"); // stores an API key
    macro_key_btn_->setCursor(Qt::PointingHandCursor);
    macro_key_btn_->setToolTip(tr("Store a free FRED API key (FRED_API_KEY) without leaving this view."));
    macro_key_btn_->setVisible(false);
    connect(macro_key_btn_, &QPushButton::clicked, this, &EconomicsView::prompt_fred_key);
    head->addWidget(macro_key_btn_);
    macro_refresh_btn_ = new QPushButton(tr("REFRESH"), section);
    macro_refresh_btn_->setObjectName(QStringLiteral("econLink"));
    macro_refresh_btn_->setCursor(Qt::PointingHandCursor);
    macro_refresh_btn_->setToolTip(tr("Fetch the latest observations from FRED"));
    head->addWidget(macro_refresh_btn_);
    sl->addLayout(head);

    macro_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, section);
    macro_strip_ = new ui::KpiStrip(macro_pane_->content_parent());
    for (const auto& m : kMacroSeries) {
        auto* t = macro_strip_->add_tile(QString::fromLatin1(m.id), econ_tr(m.label));
        t->set_tooltip_text(econ_tr(m.name));
    }
    macro_pane_->set_content(macro_strip_);
    macro_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Cards);
    macro_pane_->set_loading_text(tr("Loading 5 US macro series from FRED…"));
    macro_pane_->set_cache_key(kEconViewMacroCacheKey, kEconViewMacroTtlDays);
    macro_pane_->set_max_age(kEconViewMacroMaxAgeS);
    macro_pane_->set_timeout(90);
    macro_pane_->set_requirements({ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))});
    macro_pane_->set_status_label(macro_status_);
    macro_pane_->set_start([this](quint64 id) { start_macro(id); });
    macro_pane_->set_cancel([this](quint64 id) {
        if (macro_pending_id_ == id)
            macro_pending_id_ = 0; // the late reply is dropped
    });
    macro_pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render_macro(p); });
    // Room for the needs-setup / error page (glyph, title, message, button) at any font size.
    macro_pane_->setMinimumHeight(ui::fonts::font_px(0) * 14);
    connect(macro_pane_, &ui::AutoLoadPane::notice_action, macro_pane_, &ui::AutoLoadPane::run);
    connect(macro_pane_, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State st) {
        const bool needs_key = st == ui::AutoLoadPane::State::NeedsSetup;
        if (macro_key_btn_->isVisibleTo(this) != needs_key)
            macro_key_btn_->setVisible(needs_key);
        if (macro_refresh_btn_->isVisibleTo(this) == needs_key)
            macro_refresh_btn_->setVisible(!needs_key);
    });
    connect(macro_refresh_btn_, &QPushButton::clicked, macro_pane_, &ui::AutoLoadPane::run);
    sl->addWidget(macro_pane_, 1);
    return section;
}

void EconomicsView::start_macro(quint64 request_id) {
    macro_pending_id_ = request_id;
    // The last ~120 days, so the latest observation, the one a month earlier and a
    // sparkline come in one call.
    QStringList args;
    for (const auto& m : kMacroSeries)
        args << QString::fromLatin1(m.id);
    args << QDate::currentDate().addDays(-120).toString(Qt::ISODate);

    // fred_data.py multiple <series...> <start>; the result lands in the result_ready
    // handler connected in the constructor (synchronously on a cache hit).
    auto& svc = services::EconomicsService::instance();
    if (std::exchange(macro_force_, false)) {
        // A new key was just stored: refresh() drops any cached answer for this exact call and
        // re-runs the script, so the key is actually used instead of replaying the old result.
        svc.refresh({svc.remember_topic(QStringLiteral("fred"), QStringLiteral("fred_data.py"),
                                        QStringLiteral("multiple"), args, kEconViewMacroRequestId)});
        return;
    }
    svc.execute(QStringLiteral("fred"), QStringLiteral("fred_data.py"), QStringLiteral("multiple"), args,
                kEconViewMacroRequestId);
}

void EconomicsView::render_macro(const QJsonValue& payload) {
    const QJsonObject series = payload.toObject().value(QStringLiteral("series")).toObject();
    int got = 0;
    for (const auto& m : kMacroSeries) {
        ui::StatTile* t = macro_strip_->tile(QString::fromLatin1(m.id));
        if (!t)
            continue;
        const QJsonObject s = series.value(QString::fromLatin1(m.id)).toObject();
        if (!s.value(QStringLiteral("value")).isDouble()) {
            t->set_value(QString());
            t->set_sub_label(QString());
            t->clear_sparkline();
            t->set_state(ui::StatTile::State::Error, s.value(QStringLiteral("error")).toString(tr("Not returned by FRED")));
            continue;
        }
        ++got;
        const double v = s.value(QStringLiteral("value")).toDouble();
        switch (m.kind) {
            case MacroKind::Rate:
                t->set_value(ui::formatting::pct(v, 2, /*signed_=*/false));
                break;
            case MacroKind::Index:
                t->set_value(ui::formatting::number(v, 2));
                break;
            case MacroKind::UsdPrice:
                // Deliberate USD: the FRED series is published in US dollars (WTI $/bbl);
                // a data unit, never converted.
                t->set_value(ui::formatting::money(v, QStringLiteral("USD"), ui::formatting::Compact::None, 2));
                break;
        }
        // Δ 1M as neutral text (a macro move is not a gain or a loss): percentage points
        // for rates, percent for index and price levels.
        QString delta;
        if (s.value(QStringLiteral("prior")).isDouble()) {
            const double prior = s.value(QStringLiteral("prior")).toDouble();
            if (m.kind == MacroKind::Rate)
                delta = tr("Δ1M %1 pp").arg(ui::formatting::signed_number(v - prior, 2));
            else if (std::abs(prior) > 1e-9)
                delta = tr("Δ1M %1").arg(ui::formatting::pct((v / prior - 1.0) * 100.0, 1));
        }
        const QDate d = QDate::fromString(s.value(QStringLiteral("date")).toString(), Qt::ISODate);
        const QString date = d.isValid() ? ui::formatting::calendar_date(d, ui::formatting::DateStyle::Short)
                                         : s.value(QStringLiteral("date")).toString();
        t->set_sub_label(delta.isEmpty() ? date : tr("%1 · %2").arg(delta, date));
        QVector<double> spark;
        for (const auto x : s.value(QStringLiteral("spark")).toArray())
            spark.append(x.toDouble());
        if (spark.size() >= 2)
            t->set_sparkline(spark);
        else
            t->clear_sparkline();
        t->set_state(ui::StatTile::State::Normal);
    }
    if (got < kMacroSeriesCount)
        macro_pane_->set_notice(tr("%1 of %2 series loaded — FRED returned nothing for the others (hover a tile "
                                   "for the reason).")
                                    .arg(got)
                                    .arg(kMacroSeriesCount),
                                ui::EmptyState::Tone::Warning, tr("RETRY"));
    else
        macro_pane_->clear_notice();
}

void EconomicsView::prompt_fred_key() {
    bool ok = false;
    const QString key = QInputDialog::getText(
        this, tr("FRED API Key"),
        tr("Paste your free FRED API key (get one at https://fredaccount.stlouisfed.org/apikeys).\n"
           "It is stored encrypted and shared with all FRED-backed features (Settings > Credentials)."),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || key.trimmed().isEmpty())
        return;
    fincept::SecureStorage::instance().store(QStringLiteral("FRED_API_KEY"), key.trimmed());
    macro_force_ = true; // the next fetch bypasses the cached answer made without the key
    macro_pane_->recheck_requirements();
}

// ── Macro shocks (RISK › STRESS presets, applied locally) ────────────────────

QWidget* EconomicsView::build_shocks_section(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* sl = new QVBoxLayout(section);
    sl->setContentsMargins(12, 8, 12, 8);
    sl->setSpacing(6);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(8);
    sens_title_ = new QLabel(tr("MACRO SHOCKS"), section);
    sens_title_->setObjectName(QStringLiteral("econTitle"));
    head->addWidget(sens_title_);
    head->addStretch(1);
    shocks_open_btn_ = new QPushButton(tr("OPEN RISK › STRESS"), section);
    shocks_open_btn_->setObjectName(QStringLiteral("econLink"));
    shocks_open_btn_->setCursor(Qt::PointingHandCursor);
    shocks_open_btn_->setToolTip(tr("All scenarios, per-holding detail and replays on real prices"));
    connect(shocks_open_btn_, &QPushButton::clicked, this, &EconomicsView::open_stress_requested);
    head->addWidget(shocks_open_btn_);
    sl->addLayout(head);

    sens_note_ = new QLabel(section);
    sens_note_->setObjectName(QStringLiteral("econNote"));
    sens_note_->setWordWrap(true);
    sl->addWidget(sens_note_);

    shocks_stack_ = new QStackedWidget(section);
    auto* body = new QWidget(shocks_stack_);
    auto* bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(12);
    shocks_chart_ = new ui::BarChart(body);
    shocks_chart_->set_orientation(Qt::Horizontal);
    shocks_chart_->set_signed_colors(true); // impact on the book: gain / loss direction
    shocks_chart_->set_show_values(true);
    shocks_chart_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1); });
    bl->addWidget(shocks_chart_, 3);

    shocks_table_ = new QTableWidget(body);
    shocks_table_->setColumnCount(4);
    shocks_table_->setHorizontalHeaderLabels({tr("SCENARIO"), tr("DRIVER"), tr("IMPACT"), tr("EST. P&L")});
    shocks_table_->setSelectionMode(QAbstractItemView::NoSelection);
    shocks_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    shocks_table_->setShowGrid(false);
    shocks_table_->verticalHeader()->setVisible(false);
    shocks_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    shocks_table_->horizontalHeader()->setStretchLastSection(true);
    shocks_table_->setStyleSheet(econ_table_qss());
    bl->addWidget(shocks_table_, 2);
    shocks_stack_->addWidget(body);

    shocks_empty_ = ui::EmptyState::create(
        ui::EmptyState::no_data_spec(tr("No positions yet"),
                                     tr("Add a position to see how rate, inflation, currency, commodity and equity "
                                        "shocks would move this book."),
                                     tr("BUY")),
        shocks_stack_);
    shocks_empty_->set_compact(true);
    connect(shocks_empty_, &ui::EmptyState::primary_clicked, this, &EconomicsView::add_position_requested);
    shocks_stack_->addWidget(shocks_empty_);
    sl->addWidget(shocks_stack_, 1);
    return section;
}

void EconomicsView::update_shocks() {
    if (!shocks_stack_)
        return;
    namespace ps = portfolio_stress;
    bool any_valued = false;
    for (const auto& h : summary_.holdings)
        any_valued = any_valued || (h.valued() && std::isfinite(h.weight));
    if (summary_.holdings.isEmpty() || !any_valued) {
        if (summary_.holdings.isEmpty())
            shocks_empty_->apply(ui::EmptyState::no_data_spec(
                tr("No positions yet"),
                tr("Add a position to see how rate, inflation, currency, commodity and equity shocks would move this "
                   "book."),
                tr("BUY")));
        else
            shocks_empty_->apply(ui::EmptyState::no_data_spec(
                tr("No position can be valued yet"),
                tr("The shocks are applied to the valued book. Prices or exchange rates for these holdings have not "
                   "arrived — they appear here as soon as they do.")));
        shocks_empty_->set_compact(true);
        sens_note_->setText(QString());
        shocks_stack_->setCurrentWidget(shocks_empty_);
        return;
    }

    // Presets (macro drivers) + the user's own scenarios from RISK › STRESS.
    struct Row {
        QString name;
        QString driver;
        QString description;
        ps::Result res;
    };
    const QVector<ps::Scenario> presets = ps::preset_scenarios();
    QVector<ps::Scenario> picked;
    QStringList drivers;
    for (const auto& mp : kMacroPresets) {
        for (const auto& s : presets) {
            if (s.id == QLatin1String(mp.id)) {
                picked.append(s);
                drivers.append(econ_tr(mp.driver));
                break;
            }
        }
    }
    if (auto r = SettingsRepository::instance().get(QString::fromLatin1(kEconViewStressUserKey));
        r.is_ok() && !r.value().isEmpty()) {
        for (const auto& v : QJsonDocument::fromJson(r.value().toUtf8()).array()) {
            if (!v.isObject())
                continue;
            picked.append(ps::scenario_from_json(v.toObject()));
            drivers.append(tr("Your scenario"));
        }
    }

    // Each equity line moves by its own beta when the market-analytics run has one.
    const auto analytics = services::PortfolioService::instance().last_market_analytics(summary_.portfolio.id);
    const bool have_betas = analytics.portfolio_id == summary_.portfolio.id && !analytics.holdings.isEmpty();
    const QHash<QString, portfolio::HoldingRisk>* risk = have_betas ? &analytics.holdings : nullptr;

    QVector<Row> rows;
    rows.reserve(picked.size());
    for (int i = 0; i < picked.size(); ++i)
        rows.append(Row{ps::scenario_name(picked[i]), drivers.value(i), ps::scenario_description(picked[i]),
                        ps::apply(picked[i], summary_, risk)});
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row& a, const Row& b) { return a.res.impact_pct < b.res.impact_pct; });

    QStringList cats;
    QVector<double> vals;
    for (const auto& r : rows) {
        cats.append(r.name);
        vals.append(r.res.impact_pct);
    }
    shocks_chart_->set_data(cats, vals, tr("Impact on the book"));
    shocks_chart_->set_state(ui::PaintedChart::Ready);

    const QString ccy = currency_.isEmpty() ? pfmt::base_ccy(summary_) : currency_;
    shocks_table_->setUpdatesEnabled(false);
    shocks_table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const Row& row = rows[r];
        shocks_table_->setRowHeight(r, econ_row_height(26));
        auto* name = new QTableWidgetItem(row.name);
        name->setToolTip(row.description);
        name->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        shocks_table_->setItem(r, 0, name);
        auto* drv = new QTableWidgetItem(row.driver);
        drv->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        shocks_table_->setItem(r, 1, drv);
        // Signed text and the direction colour: never colour alone.
        auto* imp = new QTableWidgetItem(ui::formatting::pct(row.res.impact_pct, 1));
        imp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        imp->setForeground(pfmt::dir_color(row.res.impact_pct));
        shocks_table_->setItem(r, 2, imp);
        auto* pnl = new QTableWidgetItem(
            ui::formatting::signed_money(row.res.pnl, ccy, ui::formatting::Compact::FromSettings, 0));
        pnl->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl->setForeground(pfmt::dir_color(row.res.pnl));
        shocks_table_->setItem(r, 3, pnl);
    }
    shocks_table_->resizeColumnsToContents(); // once, after the fill
    shocks_table_->setUpdatesEnabled(true);

    int equity = 0, beta = 0, unvalued = 0;
    if (!rows.isEmpty()) {
        equity = rows.first().res.equity_lines;
        beta = rows.first().res.beta_lines;
        unvalued = rows.first().res.unvalued;
    }
    QString note = tr("The rate, inflation, currency, commodity and equity presets of RISK › STRESS (and your own "
                      "scenarios) applied to today's book — approximate peak-to-trough moves, no price download.");
    if (equity > 0)
        note += QLatin1Char(' ') + tr("%1 of %2 equity line(s) moved by their own beta.").arg(beta).arg(equity);
    if (unvalued > 0)
        note += QLatin1Char(' ') + tr("%n holding(s) without a price left out.", nullptr, unvalued);
    sens_note_->setText(note);
    shocks_stack_->setCurrentIndex(0);
}

// ── Data ─────────────────────────────────────────────────────────────────────

void EconomicsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_indicators();
    update_shocks();
    // US macro data is portfolio-independent: the pane loads it on first show (and
    // after 6 h), not on every summary refresh.
}

void EconomicsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EconomicsView::retranslateUi() {
    if (ind_title_)
        ind_title_->setText(tr("PORTFOLIO ECONOMICS OVERVIEW"));
    if (ind_note_)
        ind_note_->setText(tr("Per-holding contribution to portfolio value, P&L, and risk"));
    if (macro_title_)
        macro_title_->setText(tr("US MACRO CONDITIONS  (LIVE · FRED)"));
    if (macro_key_btn_)
        macro_key_btn_->setText(tr("PASTE FRED KEY HERE"));
    if (macro_refresh_btn_)
        macro_refresh_btn_->setText(tr("REFRESH"));
    if (macro_strip_) {
        for (const auto& m : kMacroSeries) {
            if (auto* t = macro_strip_->tile(QString::fromLatin1(m.id))) {
                t->set_label(econ_tr(m.label));
                t->set_tooltip_text(econ_tr(m.name));
            }
        }
    }
    if (sens_title_)
        sens_title_->setText(tr("MACRO SHOCKS"));
    if (shocks_open_btn_)
        shocks_open_btn_->setText(tr("OPEN RISK › STRESS"));
    if (shocks_table_)
        shocks_table_->setHorizontalHeaderLabels({tr("SCENARIO"), tr("DRIVER"), tr("IMPACT"), tr("EST. P&L")});
    if (indicators_table_)
        indicators_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("SECTOR"), tr("WEIGHT"), tr("COST BASIS"), tr("MARKET VALUE"), tr("P&L"), tr("P&L %")});

    if (has_data_) {
        update_indicators();
        update_shocks();
    }
}

// ── Sector ───────────────────────────────────────────────────────────────────
//
// The stored / resolved sector (import payload or SectorResolver), like every
// other portfolio surface. Without one, the asset class from the quote type —
// never a hard-coded list of US tickers (which disagreed with the rest of the
// screen and called every non-US stock "Other").
static QString econ_sector_of(const portfolio::HoldingWithQuote& h) {
    if (!h.sector.isEmpty())
        return h.sector;
    const auto ac = portfolio::classify_asset(h.symbol, h.quote_type, h.name);
    if (ac.cls == portfolio::AssetClass::Equity && !ac.fund)
        return QCoreApplication::translate("fincept::screens::EconomicsView", "Unclassified");
    return pfmt::slice_label(portfolio::AllocationDimension::AssetClass, portfolio::asset_class_key(ac.cls));
}

void EconomicsView::update_indicators() {
    const auto& hv = summary_.holdings;

    // Empty state — an empty grid gives the user nothing to act on.
    indicators_table_->clearSpans();
    if (hv.isEmpty()) {
        indicators_table_->setRowCount(1);
        indicators_table_->setRowHeight(0, econ_row_height(40));
        auto* msg = new QTableWidgetItem(tr("No holdings in this portfolio yet."));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        msg->setFlags(Qt::ItemIsEnabled);
        indicators_table_->setItem(0, 0, msg);
        indicators_table_->setSpan(0, 0, 1, indicators_table_->columnCount());
        return;
    }

    const QString ccy = currency_.isEmpty() ? pfmt::base_ccy(summary_) : currency_;
    indicators_table_->setSortingEnabled(false);
    indicators_table_->setRowCount(hv.size());

    // Largest first; positions that could not be valued (NaN) last.
    auto sorted = hv;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        const double x = std::isfinite(a.market_value) ? a.market_value : -1.0;
        const double y = std::isfinite(b.market_value) ? b.market_value : -1.0;
        return x > y;
    });

    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        indicators_table_->setRowHeight(r, econ_row_height(26));

        auto set = [&](int col, const QString& text, const QColor& color, double sort_value) {
            QTableWidgetItem* item = (col >= 2) ? new ui::NumericTableWidgetItem(text, std::isfinite(sort_value) ? sort_value : -1e300)
                                                : new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 || col == 1 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                        : (Qt::AlignRight | Qt::AlignVCenter));
            if (color.isValid())
                item->setForeground(color);
            indicators_table_->setItem(r, col, item);
        };

        set(0, h.valued() ? h.symbol : QStringLiteral("⚠ ") + h.symbol,
            QColor(h.valued() ? ui::colors::CYAN() : ui::colors::WARNING()), 0);
        set(1, econ_sector_of(h), QColor(ui::colors::TEXT_SECONDARY()), 0);
        set(2, pfmt::upct(h.weight, 1), QColor(ui::colors::TEXT_PRIMARY()), h.weight);
        set(3, pfmt::money(h.cost_basis, ccy), QColor(ui::colors::TEXT_SECONDARY()), h.cost_basis);
        set(4, pfmt::money(h.market_value, ccy), QColor(ui::colors::TEXT_PRIMARY()), h.market_value);
        set(5, pfmt::smoney(h.unrealized_pnl, ccy), pfmt::dir_color(h.unrealized_pnl), h.unrealized_pnl);
        set(6, pfmt::pct(h.unrealized_pnl_percent), pfmt::dir_color(h.unrealized_pnl_percent), h.unrealized_pnl_percent);
    }
    indicators_table_->setSortingEnabled(true);
}

} // namespace fincept::screens
