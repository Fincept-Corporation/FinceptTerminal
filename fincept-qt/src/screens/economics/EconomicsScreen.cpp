// src/screens/economics/EconomicsScreen.cpp
// Economics Data Explorer — panel-based shell.
// Switches between 33 per-source EconPanelBase subclasses.
// Panels are lazy-constructed the first time their badge is clicked.
#include "screens/economics/EconomicsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/common/CommandRows.h"
#include "screens/economics/panels/AdbPanel.h"
#include "screens/economics/panels/AkShareChinaPanel.h"
#include "screens/economics/panels/BcbPanel.h"
#include "screens/economics/panels/BeaPanel.h"
#include "screens/economics/panels/BisPanel.h"
#include "screens/economics/panels/BlsPanel.h"
#include "screens/economics/panels/CensusPanel.h"
#include "screens/economics/panels/CftcPanel.h"
#include "screens/economics/panels/EcbPanel.h"
#include "screens/economics/panels/EconDbPanel.h"
#include "screens/economics/panels/EconPanelBase.h"
#include "screens/economics/panels/EconomicCalendarPanel.h"
#include "screens/economics/panels/EiaPanel.h"
#include "screens/economics/panels/EurostatPanel.h"
#include "screens/economics/panels/FederalReservePanel.h"
#include "screens/economics/panels/FinceptMacroPanel.h"
#include "screens/economics/panels/FiscalDataPanel.h"
#include "screens/economics/panels/FredAnalyticsPanel.h"
#include "screens/economics/panels/FredPanel.h"
#include "screens/economics/panels/GlobalCentralBanksPanel.h"
#include "screens/economics/panels/IlostatPanel.h"
#include "screens/economics/panels/MarketModelsPanel.h"
#include "screens/economics/panels/NberPanel.h"
#include "screens/economics/panels/ImfPanel.h"
#include "screens/economics/panels/OecdPanel.h"
#include "screens/economics/panels/OnsPanel.h"
#include "screens/economics/panels/OwIdPanel.h"
#include "screens/economics/panels/StatCanPanel.h"
#include "screens/economics/panels/TradingEconomicsPanel.h"
#include "screens/economics/panels/UnComtradePanel.h"
#include "screens/economics/panels/UnescoPanel.h"
#include "screens/economics/panels/WorldBankHealthPanel.h"
#include "screens/economics/panels/WorldBankPanel.h"
#include "screens/economics/panels/WtoPanel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <iterator>
#include <utility>

namespace fincept::screens {

// ── Source registry ───────────────────────────────────────────────────────────
// { id, display label, accent color }. The accent is the provider's identity (brand) colour as it
// looks on the dark theme; refresh_theme() draws it through ui::colors::legible() / tint().
static const struct {
    const char* id;
    const char* label;
    const char* color;
} kSources[] = {
    {"worldbank", "World Bank", "#2563EB"},
    {"imf", "IMF", "#1B5E20"},
    {"fred", "FRED", "#B22222"},
    {"fred_analytics", "FRED Analytics", "#C0392B"},
    {"market_models", "Market Models", "#14B8A6"},
    {"oecd", "OECD", "#00518A"},
    {"ecb", "ECB", "#003F8C"},
    {"eurostat", "Eurostat", "#003399"},
    {"bls", "BLS", "#1565C0"},
    {"census", "US Census", "#4527A0"},
    {"econdb", "EconDB", "#00695C"},
    {"trading_economics", "Trading Econ", "#FF6F00"},
    {"econ_calendar", "Econ Calendar", "#E65100"},
    {"bcb", "BCB Brazil", "#1B5E20"},
    {"akshare_cn", "AkShare China", "#C62828"},
    {"ons", "ONS UK", "#0277BD"},
    {"global_cb", "Central Banks", "#4A148C"},
    {"federal_reserve", "Federal Reserve", "#1A237E"},
    {"fiscal_data", "Fiscal Data", "#006064"},
    {"nber", "NBER Cycles", "#7C3AED"},
    {"owid", "Our World In Data", "#6D28D9"},
    {"statcan", "StatCan", "#B71C1C"},
    {"ilostat", "ILO", "#0D47A1"},
    {"un_comtrade", "UN Comtrade", "#1565C0"},
    {"wb_health", "WB Health", "#0288D1"},
    {"bis", "BIS", "#9D4EDD"},
    {"adb", "ADB", "#0072BC"},
    {"cftc", "CFTC", "#FF5722"},
    {"eia", "EIA", "#4CAF50"},
    {"wto", "WTO", "#E91E63"},
    {"unesco", "UNESCO", "#00ACC1"},
    {"bea", "BEA", "#E65100"},
    {"fincept", "Fincept Macro", "#d97706"},
};

// ── Panel factory ─────────────────────────────────────────────────────────────
static EconPanelBase* make_panel(const QString& id, QWidget* parent) {
    if (id == "worldbank")
        return new WorldBankPanel(parent);
    if (id == "imf")
        return new ImfPanel(parent);
    if (id == "fred")
        return new FredPanel(parent);
    if (id == "fred_analytics")
        return new FredAnalyticsPanel(parent);
    if (id == "market_models")
        return new MarketModelsPanel(parent);
    if (id == "oecd")
        return new OecdPanel(parent);
    if (id == "ecb")
        return new EcbPanel(parent);
    if (id == "eurostat")
        return new EurostatPanel(parent);
    if (id == "bls")
        return new BlsPanel(parent);
    if (id == "census")
        return new CensusPanel(parent);
    if (id == "econdb")
        return new EconDbPanel(parent);
    if (id == "trading_economics")
        return new TradingEconomicsPanel(parent);
    if (id == "econ_calendar")
        return new EconomicCalendarPanel(parent);
    if (id == "bcb")
        return new BcbPanel(parent);
    if (id == "akshare_cn")
        return new AkShareChinaPanel(parent);
    if (id == "ons")
        return new OnsPanel(parent);
    if (id == "global_cb")
        return new GlobalCentralBanksPanel(parent);
    if (id == "federal_reserve")
        return new FederalReservePanel(parent);
    if (id == "fiscal_data")
        return new FiscalDataPanel(parent);
    if (id == "nber")
        return new NberPanel(parent);
    if (id == "owid")
        return new OwIdPanel(parent);
    if (id == "statcan")
        return new StatCanPanel(parent);
    if (id == "ilostat")
        return new IlostatPanel(parent);
    if (id == "un_comtrade")
        return new UnComtradePanel(parent);
    if (id == "wb_health")
        return new WorldBankHealthPanel(parent);
    if (id == "bis")
        return new BisPanel(parent);
    if (id == "adb")
        return new AdbPanel(parent);
    if (id == "cftc")
        return new CftcPanel(parent);
    if (id == "eia")
        return new EiaPanel(parent);
    if (id == "wto")
        return new WtoPanel(parent);
    if (id == "unesco")
        return new UnescoPanel(parent);
    if (id == "bea")
        return new BeaPanel(parent);
    if (id == "fincept")
        return new FinceptMacroPanel(parent);
    return nullptr;
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconomicsScreen::EconomicsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("econScreen");
    setStyleSheet(screen_style()); // once, before the children exist
    build_ui();
    fit_to_font();
    // Sizes that follow the font are re-fitted once an appearance change has settled (never inside
    // theme_changed); colours follow through ThemeManager's re-style pass.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &EconomicsScreen::fit_to_font);
}

void EconomicsScreen::showEvent(QShowEvent* event) {
    // No panel is built in the constructor: restore_state() opens the remembered source; otherwise the
    // first one opens here.
    if (active_id_.isEmpty() && !sources_.isEmpty())
        switch_to(sources_.first().id);
    QWidget::showEvent(event);
}

void EconomicsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    header_ = new QWidget(this);
    header_->setObjectName(QStringLiteral("econHeader"));
    header_->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    title_ = new QLabel(tr("ECONOMICS DATA EXPLORER"));
    title_->setObjectName(QStringLiteral("econScreenTitle"));
    // Derive the count from the registry — the hard-coded "32" was one more
    // than the number of panels actually registered.
    subtitle_ = new QLabel(tr("%1 global data sources · 1000+ indicators").arg(static_cast<int>(std::size(kSources))));
    subtitle_->setObjectName(QStringLiteral("econScreenSubtitle"));

    hl->addWidget(title_);
    hl->addWidget(subtitle_);
    hl->addStretch();
    root->addWidget(header_);

    // ── Badge bar (scrollable) ────────────────────────────────────────────────
    scroll_ = new QScrollArea;
    scroll_->setObjectName(QStringLiteral("econBadgeScroll"));
    scroll_->setAccessibleName(tr("Economics data source selector"));
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);

    badge_bar_ = new QWidget(this);
    badge_bar_->setObjectName(QStringLiteral("econBadgeBar"));
    auto* bhl = new QHBoxLayout(badge_bar_);
    bhl->setContentsMargins(6, 3, 6, 3);
    bhl->setSpacing(4);

    for (const auto& src : kSources) {
        SourceEntry entry;
        entry.id = src.id;
        entry.label = src.label;
        entry.color = src.color;

        auto* btn = new QPushButton(src.label);
        btn->setObjectName(QStringLiteral("econBadge_") + src.id); // styled by the badge bar's stylesheet
        btn->setCheckable(true);
        btn->setAccessibleName(tr("Data source: %1").arg(QString::fromUtf8(src.label)));
        btn->setToolTip(tr("Switch to the %1 data source").arg(QString::fromUtf8(src.label)));

        const QString sid = src.id;
        connect(btn, &QPushButton::clicked, this, [this, sid]() { switch_to(sid); });

        entry.badge = btn;
        bhl->addWidget(btn);
        sources_.append(entry);
    }
    bhl->addStretch();

    // Left-to-right keyboard traversal across the source badges.
    for (int i = 1; i < sources_.size(); ++i)
        QWidget::setTabOrder(sources_[i - 1].badge, sources_[i].badge);

    scroll_->setWidget(badge_bar_);
    // Wheel events over a badge travel up to the bar; eventFilter() scrolls the bar sideways.
    badge_bar_->installEventFilter(this);
    root->addWidget(scroll_);

    // ── Panel stack ───────────────────────────────────────────────────────────
    stack_ = new QStackedWidget;
    stack_->setObjectName(QStringLiteral("econPanelStack"));
    root->addWidget(stack_, 1);
}

void EconomicsScreen::fit_to_font() {
    // Badge text is base-5 px (9 px at the default size): 26 px at the default size, growing with the font.
    const int badge_h = std::max(26, ui::fonts::font_px(-5) + 17);
    for (const auto& entry : std::as_const(sources_))
        if (entry.badge && entry.badge->height() != badge_h)
            entry.badge->setFixedHeight(badge_h);
    if (scroll_ && scroll_->height() != badge_h + 8)
        scroll_->setFixedHeight(badge_h + 8);
    const int header_h = std::max(40, ui::fonts::font_px(-2) + 26);
    if (header_ && header_->height() != header_h)
        header_->setFixedHeight(header_h);
}

// ── Theme refresh ─────────────────────────────────────────────────────────────

QString EconomicsScreen::screen_style() const {
    using namespace ui::colors;
    QString qss =
        QString("QWidget#econHeader { background:%1; border-bottom:1px solid %2; }"
                "QLabel#econScreenTitle { color:%3; font-size:%5px; font-weight:700; letter-spacing:1.5px;"
                "  background:transparent; }"
                "QLabel#econScreenSubtitle { color:%4; font-size:%6px; background:transparent; }"
                "QScrollArea#econBadgeScroll { background:%1; border:none; border-bottom:1px solid %2; }"
                "QWidget#econBadgeBar { background:%1; }"
                "QStackedWidget#econPanelStack { background:%1; }")
            .arg(BG_BASE(), BORDER_DIM(), TEXT_PRIMARY(), TEXT_TERTIARY(), ui::fonts::px(-2), ui::fonts::px(-4));
    // One rule per source badge (id selectors): the provider's identity colour, made readable on the
    // active theme (legible_color / tint are recomputed by the re-style pass on a theme change).
    for (const auto& src : kSources) {
        const QColor c = legible_color(QColor(QString::fromLatin1(src.color)));
        qss += QString("QPushButton#econBadge_%1 { background:%2; color:%3; border:1px solid %4;"
                       "  font-size:%9px; font-weight:700; padding:0 10px; letter-spacing:0.3px; }"
                       "QPushButton#econBadge_%1:hover { color:%5; border-color:%6; }"
                       "QPushButton#econBadge_%1:checked { background:%7; color:%8; border-color:%8; }")
                   .arg(QString::fromLatin1(src.id), BG_SURFACE(), TEXT_SECONDARY(), BORDER_MED(), TEXT_PRIMARY(),
                        BORDER_BRIGHT(), tint(c, 0.12), c.name(), ui::fonts::px(-5));
    }
    return qss;
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EconomicsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EconomicsScreen::retranslateUi() {
    if (title_)
        title_->setText(tr("ECONOMICS DATA EXPLORER"));
    if (subtitle_)
        subtitle_->setText(
            tr("%1 global data sources · 1000+ indicators").arg(static_cast<int>(std::size(kSources))));
    // Source badge labels are brand/source names (data) and are not translated.
}

// ── Panel switching ───────────────────────────────────────────────────────────

EconPanelBase* EconomicsScreen::get_or_create_panel(SourceEntry& entry) {
    if (entry.panel)
        return entry.panel;

    EconPanelBase* panel = make_panel(entry.id, nullptr);
    if (!panel) {
        LOG_ERROR("EconomicsScreen", "No panel factory for source: " + entry.id);
        return nullptr;
    }

    entry.panel = panel;
    stack_->addWidget(panel);
    // Persist the panel's inputs whenever the user runs a query, not only when the screen hides —
    // a crash or kill after a long session otherwise loses every query typed since it opened.
    connect(panel, &EconPanelBase::query_started, this,
            [this]() { ScreenStateManager::instance().notify_changed(this); });
    LOG_DEBUG("EconomicsScreen", "Created panel for: " + entry.id);

    // Apply the state saved for this panel (restore_state() ran before the panel existed).
    const QString state_key = entry.id + "_panel";
    if (pending_panel_states_.contains(state_key))
        panel->restore_panel_state(pending_panel_states_.take(state_key).toMap());
    return panel;
}

void EconomicsScreen::switch_to(const QString& source_id) {
    if (active_id_ == source_id)
        return;
    active_id_ = source_id;
    ScreenStateManager::instance().notify_changed(this);

    for (auto& entry : sources_) {
        if (entry.badge)
            static_cast<QPushButton*>(entry.badge)->setChecked(entry.id == source_id);

        if (entry.id == source_id) {
            if (entry.badge && scroll_)
                scroll_->ensureWidgetVisible(entry.badge, 40, 0);
            EconPanelBase* panel = get_or_create_panel(entry);
            if (panel) {
                stack_->setCurrentWidget(panel);
                // activate() prepares what a panel needs besides its query (selector lists, today's
                // date); the panel's results pane shows its remembered result or runs the query on
                // show. Skipped while the panel shows a result or has a query in flight.
                if (!panel->keeps_state_on_activate())
                    panel->activate();
            }
        }
    }
}

bool EconomicsScreen::eventFilter(QObject* watched, QEvent* event) {
    if (watched == badge_bar_ && event->type() == QEvent::Wheel && scroll_) {
        auto* we = static_cast<QWheelEvent*>(event);
        const QPoint d = we->angleDelta();
        QScrollBar* bar = scroll_->horizontalScrollBar();
        // A sideways wheel already scrolls the bar; only the up/down wheel needs redirecting.
        if (bar && bar->maximum() > 0 && d.y() != 0 && qAbs(d.y()) >= qAbs(d.x())) {
            bar->setValue(bar->value() - d.y());
            we->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap EconomicsScreen::save_state() const {
    // Start from the saved state of panels not built this session, so saving after opening only
    // some panels doesn't drop everyone else's inputs.
    QVariantMap state = pending_panel_states_;
    state["source_id"] = active_id_;
    for (const auto& entry : sources_) {
        if (entry.panel) {
            auto ps = entry.panel->save_panel_state();
            if (!ps.isEmpty())
                state[entry.id + "_panel"] = ps;
        }
    }
    return state;
}

void EconomicsScreen::restore_state(const QVariantMap& state) {
    // Panels are lazy: queue the state of the ones that don't exist yet BEFORE switching, so the
    // target panel is built with its restored inputs already in place (and is not activated with
    // defaults first), then apply to panels that already exist.
    QSet<QString> queued;
    for (const auto& entry : std::as_const(sources_)) {
        const QString key = entry.id + "_panel";
        if (!entry.panel && state.contains(key)) {
            pending_panel_states_[key] = state.value(key);
            queued.insert(key);
        }
    }
    const QString id = state.value("source_id").toString();
    if (!id.isEmpty())
        switch_to(id);
    for (auto& entry : sources_) {
        const QString key = entry.id + "_panel";
        if (entry.panel && state.contains(key) && !queued.contains(key))
            entry.panel->restore_panel_state(state.value(key).toMap());
    }
}

// ── ISectionLinked ────────────────────────────────────────────────────────────

bool EconomicsScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed();
    if (key.isEmpty())
        return true; // no panel takes a focus of its own
    QString rest;
    if (command_rows::split_kind(key, "panel", &rest))
        key = rest;
    key = key.toLower();
    key.replace(QLatin1Char('-'), QLatin1Char('_'));
    QString view;
    if (const int slash = key.indexOf(QLatin1Char('/')); slash >= 0) {
        view = key.mid(slash + 1);
        key = key.left(slash);
    }
    SourceEntry* target = nullptr;
    for (auto& entry : sources_)
        if (entry.id == key)
            target = &entry;
    if (!target)
        return false;
    const bool models = target->id == QLatin1String("market_models");
    if (!view.isEmpty() && (!models || MarketModelsPanel::view_for_id(view) < 0))
        return false; // an unknown view: nothing changes
    if (view.isEmpty() && models && MarketModelsPanel::view_for_id(focus) >= 0)
        view = focus.trimmed();
    switch_to(target->id); // the badge's own path (lazy build, state, activate)
    if (!view.isEmpty())
        if (auto* mm = qobject_cast<MarketModelsPanel*>(target->panel))
            mm->select_view(view);
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

struct EconCmdRow {
    const char* id;
    const char* title;
    const char* what;
    const char* keywords;
};

const EconCmdRow kEconCmdRows[] = {
    {"worldbank", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "World Bank"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "World Bank open data (WDI)"),
     "world bank, wb, wdi, world development indicators, gdp, poverty, population, development"},
    {"imf", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "IMF"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "IMF DataMapper / World Economic Outlook"),
     "imf, international monetary fund, imf weo, weo, world economic outlook, datamapper, gdp forecast, government debt"},
    {"fred", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "FRED"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "FRED economic series (St. Louis Fed)"),
     "fred, federal reserve economic data, st louis fed, us economic data, series, cpi, unemployment rate, fed funds"},
    {"fred_analytics", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "FRED Analytics"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "yield curve, money supply, credit, stress, sentiment, PCE"),
     "fred, yield curve, money supply, m2, credit, financial stress, consumer sentiment, pce"},
    {"market_models", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Market Models"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "business cycle, equity risk premium, yield curve analytics"),
     "business cycle, equity risk premium, erp, yield curve, nelson siegel, recession signals, vix"},
    {"oecd", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "OECD"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "OECD statistics"),
     "oecd, gdp, cpi, unemployment, trade, interest rates, leading indicators"},
    {"ecb", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "ECB"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "European Central Bank: exchange rates, inflation, money supply"),
     "ecb, european central bank, euro, eurozone, exchange rates, hicp, euro inflation, money supply"},
    {"eurostat", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Eurostat"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "EU statistics"),
     "eurostat, eu, europe, industrial production, retail, energy, trade"},
    {"bls", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "BLS"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Bureau of Labor Statistics"),
     "bls, bureau of labor statistics, jobs, payrolls, nfp, cpi, inflation, unemployment, wages"},
    {"census", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Census"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Census Bureau: population and housing"),
     "census, acs, population, demographics, housing, states"},
    {"econdb", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "EconDB"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "macro indicators for 49 countries"), "econdb, macro indicators, countries"},
    {"trading_economics", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Trading Economics"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "bonds, yield curves, country data"),
     "trading economics, tradingeconomics, bonds, yield curves, country data"},
    {"econ_calendar", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Economic calendar"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "this week's releases (Forex Factory)"),
     "economic calendar, forex factory, events, data releases, nfp, cpi release, fomc, high impact"},
    {"bcb", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Brazil central bank (BCB)"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Banco Central do Brasil"),
     "bcb, banco central do brasil, brazil, selic, ipca, brazil inflation"},
    {"akshare_cn", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "China macro (AkShare)"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "China CPI, PPI, GDP, PMI"),
     "akshare, china, chinese economy, china cpi, ppi, pmi, china gdp"},
    {"ons", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "UK ONS"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "UK Office for National Statistics"),
     "ons, office for national statistics, uk, britain, uk inflation, uk gdp"},
    {"global_cb", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Central banks"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "BoE, RBA, BoC, Riksbank, SNB, Norges Bank"),
     "central banks, policy rates, interest rates, boe, bank of england, rba, boc, bank of canada, riksbank, snb, "
     "norges bank"},
    {"federal_reserve", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Federal Reserve"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Federal Reserve data"),
     "fed, federal reserve, h15, fed funds, treasury yields, money stock"},
    {"fiscal_data", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Treasury fiscal data"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US debt, interest rates, exchange rates"),
     "fiscal data, treasury, national debt, debt to the penny, us debt"},
    {"nber", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "NBER recessions"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US business cycle dates"),
     "nber, recession, recessions, business cycle dating, expansions"},
    {"owid", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Our World in Data"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "CO2, energy, life expectancy, poverty, GDP"),
     "owid, our world in data, co2, emissions, energy, life expectancy, poverty"},
    {"statcan", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Statistics Canada"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Canadian GDP, CPI, jobs, housing"),
     "statcan, statistics canada, canada, canadian gdp, cpi, unemployment, housing"},
    {"ilostat", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "ILO labour statistics"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "ILOSTAT: unemployment, labour force"),
     "ilo, ilostat, labour, labor force, unemployment, employment, participation"},
    {"un_comtrade", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "UN Comtrade"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "international trade statistics"),
     "comtrade, un comtrade, international trade, imports, exports, trade flows"},
    {"wb_health", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "World Bank health"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "health and development indicators"),
     "health, life expectancy, infant mortality, literacy, gini, hdi"},
    {"bis", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "BIS"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Bank for International Settlements statistics"),
     "bis, bank for international settlements, credit, house prices, central bank rates, effective exchange rates"},
    {"adb", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "ADB"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Asian Development Bank: Asia-Pacific data"),
     "adb, asian development bank, asia pacific, asia economy"},
    {"cftc", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "CFTC Commitments of Traders"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "CFTC COT reports"),
     "cftc, cot, cftc cot, commitments of traders, cot report, positioning, futures positions, speculators"},
    {"eia", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "EIA petroleum"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Energy Information Administration: weekly petroleum"),
     "eia, energy information administration, oil, crude inventories, petroleum, wpsr, gasoline"},
    {"wto", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "WTO"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "World Trade Organization"), "wto, world trade organization, tariffs, trade"},
    {"unesco", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "UNESCO"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "UNESCO Institute for Statistics"), "unesco, uis, education, science, culture"},
    {"bea", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "BEA"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "US Bureau of Economic Analysis: GDP, income, PCE"),
     "bea, bureau of economic analysis, gdp, personal income, pce, national accounts"},
    {"fincept", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Fincept Macro"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "government bonds, sovereign CDS, ratings (Fincept credits)"),
     "fincept macro, government bonds, sovereign cds, 10y yields, credit ratings, inverted curves"},
};
const EconCmdRow kEconModelViews[] = {
    {"market_models/cycle", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Business cycle dashboard"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Market Models › business cycle"),
     "business cycle, recession, vix, sp500 trend, yield spread"},
    {"market_models/erp", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Equity risk premium"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Market Models › equity risk premium"),
     "erp, equity risk premium, earnings yield, implied erp"},
    {"market_models/curve", QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Yield curve analytics"), QT_TRANSLATE_NOOP("fincept::screens::EconomicsScreen", "Market Models › yield curve"),
     "yield curve, bootstrap, forward curve, nelson siegel, curve shape, treasury curve"},
};

QString econ_tr(const char* s) {
    return QCoreApplication::translate("fincept::screens::EconomicsScreen", s);
}

QList<command::CommandItem> econ_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("economics");
    const QString where = econ_tr("Economics");
    QList<command::CommandItem> out;
    auto add = [&](const EconCmdRow& r) {
        QStringList kw = QString::fromLatin1(r.keywords).split(QStringLiteral(", "), Qt::SkipEmptyParts);
        kw << QStringLiteral("economics") << QStringLiteral("macro");
        out.append(rows::section(screen, QString::fromLatin1(r.id), econ_tr(r.title), kw, where,
                                 rows::where(where, econ_tr(r.what))));
    };
    for (const EconCmdRow& r : kEconCmdRows)
        add(r);
    for (const EconCmdRow& r : kEconModelViews)
        add(r);
    return out;
}

void econ_install_commands() {
    command_rows::install(QStringLiteral("economics"), &econ_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(econ_install_commands)

} // namespace fincept::screens
