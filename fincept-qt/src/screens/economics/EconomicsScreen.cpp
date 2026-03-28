// src/screens/economics/EconomicsScreen.cpp
// Economics Data Explorer — panel-based shell.
// Switches between 32 per-source EconPanelBase subclasses.
// Panels are lazy-constructed the first time their badge is clicked.
#include "screens/economics/EconomicsScreen.h"

#include "core/logging/Logger.h"
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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Source registry ───────────────────────────────────────────────────────────
// { id, display label, accent color }
static const struct { const char* id; const char* label; const char* color; }
kSources[] = {
    { "worldbank",       "World Bank",         "#2563EB" },
    { "imf",             "IMF",                "#1B5E20" },
    { "fred",            "FRED",               "#B22222" },
    { "fred_analytics",  "FRED Analytics",     "#C0392B" },
    { "oecd",            "OECD",               "#00518A" },
    { "ecb",             "ECB",                "#003F8C" },
    { "eurostat",        "Eurostat",           "#003399" },
    { "bls",             "BLS",                "#1565C0" },
    { "census",          "US Census",          "#4527A0" },
    { "econdb",          "EconDB",             "#00695C" },
    { "trading_economics","Trading Econ",       "#FF6F00" },
    { "econ_calendar",   "Econ Calendar",      "#E65100" },
    { "bcb",             "BCB Brazil",         "#1B5E20" },
    { "akshare_cn",      "AkShare China",      "#C62828" },
    { "ons",             "ONS UK",             "#0277BD" },
    { "global_cb",       "Central Banks",      "#4A148C" },
    { "federal_reserve",  "Federal Reserve",    "#1A237E" },
    { "fiscal_data",     "Fiscal Data",        "#006064" },
    { "owid",            "Our World In Data",  "#6D28D9" },
    { "statcan",         "StatCan",            "#B71C1C" },
    { "ilostat",         "ILO",                "#0D47A1" },
    { "un_comtrade",     "UN Comtrade",        "#1565C0" },
    { "wb_health",       "WB Health",          "#0288D1" },
    { "bis",             "BIS",                "#9D4EDD" },
    { "adb",             "ADB",                "#0072BC" },
    { "cftc",            "CFTC",               "#FF5722" },
    { "eia",             "EIA",                "#4CAF50" },
    { "wto",             "WTO",                "#E91E63" },
    { "unesco",          "UNESCO",             "#00ACC1" },
    { "bea",             "BEA",                "#E65100" },
    { "fincept",         "Fincept Macro",      "#d97706" },
};

// ── Panel factory ─────────────────────────────────────────────────────────────
static EconPanelBase* make_panel(const QString& id, QWidget* parent) {
    if (id == "worldbank")        return new WorldBankPanel(parent);
    if (id == "imf")              return new ImfPanel(parent);
    if (id == "fred")             return new FredPanel(parent);
    if (id == "fred_analytics")   return new FredAnalyticsPanel(parent);
    if (id == "oecd")             return new OecdPanel(parent);
    if (id == "ecb")              return new EcbPanel(parent);
    if (id == "eurostat")         return new EurostatPanel(parent);
    if (id == "bls")              return new BlsPanel(parent);
    if (id == "census")           return new CensusPanel(parent);
    if (id == "econdb")           return new EconDbPanel(parent);
    if (id == "trading_economics") return new TradingEconomicsPanel(parent);
    if (id == "econ_calendar")    return new EconomicCalendarPanel(parent);
    if (id == "bcb")              return new BcbPanel(parent);
    if (id == "akshare_cn")       return new AkShareChinaPanel(parent);
    if (id == "ons")              return new OnsPanel(parent);
    if (id == "global_cb")        return new GlobalCentralBanksPanel(parent);
    if (id == "federal_reserve")  return new FederalReservePanel(parent);
    if (id == "fiscal_data")      return new FiscalDataPanel(parent);
    if (id == "owid")             return new OwIdPanel(parent);
    if (id == "statcan")          return new StatCanPanel(parent);
    if (id == "ilostat")          return new IlostatPanel(parent);
    if (id == "un_comtrade")      return new UnComtradePanel(parent);
    if (id == "wb_health")        return new WorldBankHealthPanel(parent);
    if (id == "bis")              return new BisPanel(parent);
    if (id == "adb")              return new AdbPanel(parent);
    if (id == "cftc")             return new CftcPanel(parent);
    if (id == "eia")              return new EiaPanel(parent);
    if (id == "wto")              return new WtoPanel(parent);
    if (id == "unesco")           return new UnescoPanel(parent);
    if (id == "bea")              return new BeaPanel(parent);
    if (id == "fincept")          return new FinceptMacroPanel(parent);
    return nullptr;
}

// ── Constructor ───────────────────────────────────────────────────────────────

EconomicsScreen::EconomicsScreen(QWidget* parent)
    : QWidget(parent) {
    setObjectName("econScreen");
    build_ui();
    // Activate first source
    if (!sources_.isEmpty())
        switch_to(sources_.first().id);
}

void EconomicsScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(40);
    header->setStyleSheet("background:#080808; border-bottom:1px solid #1a1a1a;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* title = new QLabel("ECONOMICS DATA EXPLORER");
    title->setStyleSheet(
        "color:#e5e5e5; font-size:12px; font-weight:700;"
        "letter-spacing:1.5px; background:transparent;");

    auto* sub = new QLabel("32 global data sources · 1000+ indicators");
    sub->setStyleSheet("color:#525252; font-size:10px; background:transparent;");

    hl->addWidget(title);
    hl->addWidget(sub);
    hl->addStretch();
    root->addWidget(header);

    // ── Badge bar (scrollable) ────────────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(34);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:#050505; border-bottom:1px solid #1a1a1a;");

    badge_bar_ = new QWidget;
    badge_bar_->setStyleSheet("background:#050505;");
    auto* bhl = new QHBoxLayout(badge_bar_);
    bhl->setContentsMargins(6, 3, 6, 3);
    bhl->setSpacing(4);

    for (const auto& src : kSources) {
        SourceEntry entry;
        entry.id    = src.id;
        entry.label = src.label;
        entry.color = src.color;

        auto* btn = new QPushButton(src.label);
        btn->setCheckable(true);
        btn->setFixedHeight(26);
        btn->setStyleSheet(QString(
            "QPushButton { background:#0a0a0a; color:#525252; border:1px solid #1a1a1a;"
            "  font-size:9px; font-weight:700; padding:0 10px; letter-spacing:0.3px; }"
            "QPushButton:hover { color:#e5e5e5; border-color:#333333; }"
            "QPushButton:checked { background:rgba(%1,0.12); color:%2;"
            "  border-color:%2; }")
            .arg([]( const QString& hex ) {
                // convert #RRGGBB to "R,G,G" for rgba
                QColor c(hex);
                return QString("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
            }(src.color))
            .arg(src.color));

        const QString sid = src.id;
        connect(btn, &QPushButton::clicked, this, [this, sid]() {
            switch_to(sid);
        });

        entry.badge = btn;
        bhl->addWidget(btn);
        sources_.append(entry);
    }
    bhl->addStretch();

    scroll->setWidget(badge_bar_);
    root->addWidget(scroll);

    // ── Panel stack ───────────────────────────────────────────────────────────
    stack_ = new QStackedWidget;
    stack_->setStyleSheet("background:#080808;");
    root->addWidget(stack_, 1);
}

// ── Panel switching ───────────────────────────────────────────────────────────

EconPanelBase* EconomicsScreen::get_or_create_panel(SourceEntry& entry) {
    if (entry.panel)
        return entry.panel;

    EconPanelBase* panel = make_panel(entry.id, nullptr);
    if (!panel) {
        LOG_ERROR("EconomicsScreen",
                  "No panel factory for source: " + entry.id);
        return nullptr;
    }

    entry.panel = panel;
    stack_->addWidget(panel);
    LOG_INFO("EconomicsScreen", "Created panel for: " + entry.id);
    return panel;
}

void EconomicsScreen::switch_to(const QString& source_id) {
    if (active_id_ == source_id) return;
    active_id_ = source_id;

    for (auto& entry : sources_) {
        if (entry.badge)
            static_cast<QPushButton*>(entry.badge)
                ->setChecked(entry.id == source_id);

        if (entry.id == source_id) {
            EconPanelBase* panel = get_or_create_panel(entry);
            if (panel) {
                stack_->setCurrentWidget(panel);
                panel->activate();
            }
        }
    }
}

} // namespace fincept::screens
