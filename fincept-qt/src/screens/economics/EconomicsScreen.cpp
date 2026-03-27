#include "screens/economics/EconomicsScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

// ── Stylesheet ───────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle = QStringLiteral(

    // ── Screen shell
    "#econScreen { background:%1; }"

    // ── Toolbar (top bar)
    "#econToolbar { background:%2; border-bottom:1px solid %8; }"
    "#econToolbarTitle { color:%4; font-size:13px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#econToolbarSub { color:%5; font-size:10px; background:transparent; }"

    // ── Source badge bar
    "#econSourceBar { background:%1; border-bottom:1px solid %8; }"
    "#econSourceBadge { border:1px solid %8; background:%7; color:%5;"
    "  font-size:9px; font-weight:700; padding:3px 10px;"
    "  letter-spacing:0.5px; }"
    "#econSourceBadge:hover { border-color:%9; color:%4; background:%12; }"
    "#econSourceBadge[active=true] { background:rgba(217,119,6,0.12);"
    "  border-color:%3; color:%3; }"

    // ── Fetch button
    "#econFetchBtn { background:%3; color:%1; border:none;"
    "  padding:6px 20px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
    "#econFetchBtn:hover { background:#f59e0b; }"
    "#econFetchBtn:disabled { background:%10; color:%11; }"

    // ── Left panel
    "#econLeftPanel { background:%7; border-right:1px solid %8; }"
    "#econPanelHeader { background:%2; border-bottom:1px solid %8; }"
    "#econPanelTitle { color:%5; font-size:9px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#econCountBadge { background:rgba(217,119,6,0.15); color:%3;"
    "  font-size:9px; font-weight:700; padding:1px 6px; }"

    // ── Search inputs
    "#econSearch { background:%1; color:%4; border:none;"
    "  border-bottom:1px solid %8; padding:5px 10px; font-size:11px; }"
    "#econSearch:focus { border-bottom:1px solid %3; }"

    // ── Lists
    "#econCountryList, #econIndicatorList { background:%1; border:none; outline:none;"
    "  font-size:11px; }"
    "#econCountryList::item { color:%5; padding:5px 12px;"
    "  border-bottom:1px solid %8; }"
    "#econCountryList::item:hover { color:%4; background:%12; }"
    "#econCountryList::item:selected { color:%3; background:rgba(217,119,6,0.08);"
    "  border-left:2px solid %3; padding-left:10px; }"
    "#econIndicatorList::item { color:%5; padding:4px 12px;"
    "  border-bottom:1px solid %8; }"
    "#econIndicatorList::item:hover { color:%4; background:%12; }"
    "#econIndicatorList::item:selected { color:%3; background:rgba(217,119,6,0.08);"
    "  border-left:2px solid %3; padding-left:10px; }"

    // ── Stat cards
    "#econStatsRow { background:%2; border-bottom:1px solid %8; }"
    "#econStatCard { background:%7; border:1px solid %8; }"
    "#econStatCard:hover { border-color:%9; }"
    "#econStatLabel { color:%5; font-size:8px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#econStatValue { color:%13; font-size:15px; font-weight:700; background:transparent; }"
    "#econStatSub { color:%5; font-size:9px; background:transparent; }"
    "#econStatChangePos { color:%6; font-size:14px; font-weight:700; background:transparent; }"
    "#econStatChangeNeg { color:%14; font-size:14px; font-weight:700; background:transparent; }"

    // ── Table area header
    "#econTableHeader { background:%2; border-bottom:1px solid %8; }"
    "#econDataTitle { color:%4; font-size:11px; font-weight:700; background:transparent; }"
    "#econDataStatus { color:%5; font-size:10px; background:transparent; }"
    "#econSourceTag { background:rgba(217,119,6,0.12); color:%3;"
    "  font-size:9px; font-weight:700; padding:2px 8px; }"

    // ── Table
    "QTableWidget { background:%1; color:%4; border:none;"
    "  gridline-color:%8; font-size:11px; alternate-background-color:%7; }"
    "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %8; }"
    "QTableWidget::item:selected { background:rgba(217,119,6,0.12); color:%3; }"
    "QTableWidget::item:alternate { background:%7; }"
    "QHeaderView::section { background:%2; color:%5; border:none;"
    "  border-bottom:2px solid %8; border-right:1px solid %8;"
    "  padding:5px 8px; font-size:10px; font-weight:700; letter-spacing:0.5px; }"
    "QHeaderView::section:first { border-left:none; }"

    // ── Empty state
    "#econEmptyState { background:%1; }"
    "#econEmptyTitle { color:%5; font-size:14px; font-weight:700; background:transparent; }"
    "#econEmptyHint { color:%11; font-size:11px; background:transparent; }"

    // ── Combo / LineEdit
    "QComboBox { background:%1; color:%4; border:1px solid %8;"
    "  padding:4px 10px; font-size:11px; }"
    "QComboBox::drop-down { border:none; width:18px; }"
    "QComboBox QAbstractItemView { background:%2; color:%4; border:1px solid %8;"
    "  selection-background-color:rgba(217,119,6,0.2); selection-color:%3; }"
    "QLineEdit { background:%1; color:%4; border:1px solid %8;"
    "  padding:4px 10px; font-size:11px; }"

    // ── Status bar
    "#econStatusBar { background:%2; border-top:1px solid %8; }"
    "#econStatusText { color:%11; font-size:9px; background:transparent; }"
    "#econStatusSep  { color:%8;  font-size:9px; background:transparent; }"
    "#econStatusVal  { color:%5;  font-size:9px; background:transparent; }"
    "#econStatusHigh { color:%3;  font-size:9px; font-weight:700; background:transparent; }"

    // ── Splitter / scrollbar
    "QSplitter::handle { background:%8; }"
    "QScrollBar:vertical { background:%1; width:5px; }"
    "QScrollBar::handle:vertical { background:%8; min-height:24px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    "QScrollBar:horizontal { background:%1; height:5px; }"
    "QScrollBar::handle:horizontal { background:%8; min-width:24px; }"
    "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }"
)
    .arg(colors::BG_BASE)        // %1
    .arg(colors::BG_RAISED)      // %2
    .arg(colors::AMBER)          // %3
    .arg(colors::TEXT_PRIMARY)   // %4
    .arg(colors::TEXT_SECONDARY) // %5
    .arg(colors::POSITIVE)       // %6
    .arg(colors::BG_SURFACE)     // %7
    .arg(colors::BORDER_DIM)     // %8
    .arg(colors::BORDER_BRIGHT)  // %9
    .arg(colors::AMBER_DIM)      // %10
    .arg(colors::TEXT_DIM)       // %11
    .arg(colors::BG_HOVER)       // %12
    .arg(colors::CYAN)           // %13
    .arg(colors::NEGATIVE)       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Source definitions ───────────────────────────────────────────────────────

static QList<EconSource> build_sources() {
    return {
        {"worldbank", "World Bank",      "worldbank_data.py",  "#00E5FF", false, "NY.GDP.MKTP.CD",
         "80+ countries · 1000+ indicators · No API key"},
        {"imf",       "IMF",             "imf_data.py",        "#0088FF", false, "NGDP_RPCH",
         "Global macro · Quarterly & annual · No API key"},
        {"fred",      "FRED",            "fred_data.py",       "#00D66F", false, "GDP",
         "US Federal Reserve · 800k+ series · No API key"},
        {"oecd",      "OECD",            "oecd_data.py",       "#FFD700", false, "GDP",
         "37 member nations · Policy & social data"},
        {"bis",       "BIS",             "bis_data.py",        "#9D4EDD", false, "WS_CBPOL",
         "Bank for International Settlements · Central banks"},
        {"adb",       "ADB",             "adb_data.py",        "#0072BC", false, "NGDP_XDC",
         "Asian Development Bank · 20 Asia-Pacific nations"},
        {"fed",       "Federal Reserve", "fed_data.py",        "#1A8F9A", false, "federal_funds_rate",
         "US Fed rates, balance sheet & monetary data"},
        {"cftc",      "CFTC",            "cftc_data.py",       "#FF5722", false, "gold",
         "Commitments of Traders · Futures positions"},
        {"eia",       "EIA",             "eia_data.py",        "#4CAF50", true,  "crude_petroleum_stocks",
         "Energy Information Administration · Requires API key"},
        {"bls",       "BLS",             "bls_data.py",        "#AB47BC", true,  "CUSR0000SA0",
         "Bureau of Labor Statistics · Requires API key"},
        {"wto",       "WTO",             "wto_data.py",        "#E91E63", true,  "TP_A_0010",
         "World Trade Organization · Requires API key"},
        {"unesco",    "UNESCO",          "unesco_data.py",     "#00ACC1", false, "GER.1",
         "Education & cultural development indicators"},
        {"fiscaldata","FiscalData",      "fiscaldata.py",      "#FFA726", true,  "total_public_debt",
         "US Treasury fiscal data · Requires API key"},
        {"bea",       "BEA",             "bea_data.py",        "#E65100", true,  "gdp_growth",
         "Bureau of Economic Analysis · Requires API key"},
        {"fincept",   "Fincept Macro",   "fincept_macro.py",   "#d97706", true,  "wgb_central_bank_rates",
         "Fincept proprietary macro data · Requires API key"},
    };
}

// ── Country lists ────────────────────────────────────────────────────────────

static QStringList g_major_countries = {
    "USA","CHN","JPN","DEU","GBR","FRA","IND","ITA","BRA","CAN",
    "KOR","RUS","AUS","ESP","MEX","IDN","NLD","SAU","TUR","CHE",
    "ARG","ZAF","THA","SGP","MYS","PHL","VNM","NGA","EGY","PAK",
};
static QStringList g_oecd_countries = {
    "USA","GBR","DEU","FRA","JPN","CAN","ITA","ESP","AUS","KOR",
    "NLD","CHE","BEL","SWE","AUT","NOR","DNK","FIN","IRL","PRT",
    "NZL","ISR","CZE","POL","HUN","SVK","SVN","LUX","ISL","EST",
    "LVA","LTU","CHL","COL","CRI","MEX","TUR",
};
static QStringList g_adb_countries = {
    "IND","CHN","JPN","KOR","IDN","THA","MYS","PHL","VNM","SGP",
    "BGD","PAK","LKA","MMR","KHM","LAO","MNG","NPL","AFG","FJI",
};

// ── Indicator definitions ────────────────────────────────────────────────────

struct IndicatorDef { QString id, name, category; };

static QList<IndicatorDef> g_worldbank_indicators = {
    {"NY.GDP.MKTP.CD",    "GDP (current US$)",          "GDP"},
    {"NY.GDP.MKTP.KD.ZG", "GDP growth (annual %)",      "GDP"},
    {"NY.GDP.PCAP.CD",    "GDP per capita (US$)",        "GDP"},
    {"FP.CPI.TOTL.ZG",   "Inflation, CPI (annual %)",   "Prices"},
    {"SL.UEM.TOTL.ZS",   "Unemployment (%)",             "Labor"},
    {"NE.TRD.GNFS.ZS",   "Trade (% of GDP)",             "Trade"},
    {"NE.EXP.GNFS.ZS",   "Exports (% of GDP)",           "Trade"},
    {"NE.IMP.GNFS.ZS",   "Imports (% of GDP)",           "Trade"},
    {"BX.KLT.DINV.CD.WD","FDI inflows (US$)",            "Investment"},
    {"SP.POP.TOTL",       "Population, total",            "Demographics"},
    {"SP.DYN.LE00.IN",    "Life expectancy",              "Demographics"},
    {"SE.XPD.TOTL.GD.ZS", "Education spending (% GDP)",  "Social"},
    {"SH.XPD.CHEX.GD.ZS", "Health spending (% GDP)",     "Social"},
    {"GC.DOD.TOTL.GD.ZS", "Govt debt (% of GDP)",        "Fiscal"},
    {"FM.LBL.BMNY.GD.ZS", "Broad money (% of GDP)",      "Financial"},
};
static QList<IndicatorDef> g_fred_indicators = {
    {"GDP",             "Gross Domestic Product",        "GDP"},
    {"GDPC1",           "Real GDP",                      "GDP"},
    {"A939RX0Q048SBEA", "Real GDP per capita",           "GDP"},
    {"CPIAUCSL",        "CPI All Urban",                 "Prices"},
    {"CPILFESL",        "Core CPI",                      "Prices"},
    {"UNRATE",          "Unemployment Rate",             "Labor"},
    {"PAYEMS",          "Total Nonfarm Payrolls",        "Labor"},
    {"FEDFUNDS",        "Federal Funds Rate",            "Rates"},
    {"DGS10",           "10-Year Treasury",              "Rates"},
    {"DGS2",            "2-Year Treasury",               "Rates"},
    {"DEXUSEU",         "USD/EUR Exchange Rate",         "FX"},
    {"M2SL",            "M2 Money Stock",                "Money"},
    {"WALCL",           "Fed Total Assets",              "Fed"},
    {"HOUST",           "Housing Starts",                "Housing"},
    {"INDPRO",          "Industrial Production",         "Output"},
};
static QList<IndicatorDef> g_imf_indicators = {
    {"NGDP_RPCH",    "Real GDP growth (%)",       "GDP"},
    {"NGDPD",        "Nominal GDP (bn USD)",       "GDP"},
    {"NGDPDPC",      "GDP per capita (USD)",       "GDP"},
    {"PCPIPCH",      "Inflation rate (%)",         "Prices"},
    {"LUR",          "Unemployment rate (%)",      "Labor"},
    {"BCA_NGDPD",    "Current account (% GDP)",   "External"},
    {"GGXWDG_NGDP",  "Govt gross debt (% GDP)",   "Fiscal"},
    {"GGR_NGDP",     "Govt revenue (% GDP)",       "Fiscal"},
    {"GGX_NGDP",     "Govt spending (% GDP)",      "Fiscal"},
    {"PPPEX",        "PPP exchange rate",          "FX"},
};
static QList<IndicatorDef> g_generic_indicators = {
    {"default", "Default Indicator", "General"},
};

// ── Constructor ──────────────────────────────────────────────────────────────

EconomicsScreen::EconomicsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("econScreen");
    setStyleSheet(kStyle);
    sources_ = build_sources();
    setup_ui();
    LOG_INFO("Economics", "Screen constructed");
}

// ── UI Setup ─────────────────────────────────────────────────────────────────

void EconomicsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_toolbar());
    root->addWidget(create_source_bar());

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(240);
    left->setMaximumWidth(320);

    splitter->addWidget(left);
    splitter->addWidget(create_right_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({270, 900});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    on_source_changed(0);
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_toolbar() {
    auto* bar = new QWidget;
    bar->setObjectName("econToolbar");
    bar->setFixedHeight(46);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 12, 0);
    hl->setSpacing(10);

    // Title block
    auto* title_col = new QWidget;
    title_col->setFixedWidth(200);
    auto* tcl = new QVBoxLayout(title_col);
    tcl->setContentsMargins(0, 0, 0, 0);
    tcl->setSpacing(0);
    auto* t1 = new QLabel("ECONOMIC DATA EXPLORER");
    t1->setObjectName("econToolbarTitle");
    auto* t2 = new QLabel("15 global sources · 1000+ indicators");
    t2->setObjectName("econToolbarSub");
    tcl->addWidget(t1);
    tcl->addWidget(t2);
    hl->addWidget(title_col);

    hl->addSpacing(16);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color:#1a1a1a;");
    hl->addWidget(sep);

    hl->addSpacing(10);

    // Source dropdown label
    auto* src_lbl = new QLabel("SOURCE");
    src_lbl->setObjectName("econStatusText");
    hl->addWidget(src_lbl);

    source_combo_ = new QComboBox;
    source_combo_->setFixedWidth(160);
    for (const auto& s : sources_) {
        QString label = s.name;
        if (s.needs_api_key)
            label += "  ⚿";
        source_combo_->addItem(label);
    }
    connect(source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EconomicsScreen::on_source_changed);
    hl->addWidget(source_combo_);

    hl->addSpacing(14);

    // Date range label
    auto* dr_lbl = new QLabel("RANGE");
    dr_lbl->setObjectName("econStatusText");
    hl->addWidget(dr_lbl);

    date_preset_ = new QComboBox;
    date_preset_->addItems({"1Y", "2Y", "5Y", "10Y", "ALL", "Custom"});
    date_preset_->setCurrentIndex(2);
    date_preset_->setFixedWidth(72);
    connect(date_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EconomicsScreen::on_date_preset);
    hl->addWidget(date_preset_);

    date_start_ = new QLineEdit;
    date_start_->setPlaceholderText("YYYY-MM-DD");
    date_start_->setFixedWidth(100);
    date_start_->hide();
    hl->addWidget(date_start_);

    auto* dash = new QLabel("→");
    dash->setObjectName("econStatusText");
    dash->hide();
    hl->addWidget(dash);
    // keep ref to show/hide with custom dates
    connect(date_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [dash](int i){ dash->setVisible(i == 5); });

    date_end_ = new QLineEdit;
    date_end_->setPlaceholderText("YYYY-MM-DD");
    date_end_->setFixedWidth(100);
    date_end_->hide();
    hl->addWidget(date_end_);

    hl->addStretch(1);

    fetch_btn_ = new QPushButton("  FETCH DATA  ");
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    fetch_btn_->setFixedHeight(30);
    connect(fetch_btn_, &QPushButton::clicked, this, &EconomicsScreen::on_fetch);
    hl->addWidget(fetch_btn_);

    return bar;
}

// ── Source badge strip ───────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_source_bar() {
    source_badge_bar_ = new QWidget;
    source_badge_bar_->setObjectName("econSourceBar");
    source_badge_bar_->setFixedHeight(34);

    auto* hl = new QHBoxLayout(source_badge_bar_);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(6);

    for (int i = 0; i < sources_.size(); ++i) {
        const auto& s = sources_[i];
        auto* btn = new QPushButton(s.name);
        btn->setObjectName("econSourceBadge");
        btn->setProperty("active", i == 0);
        btn->setProperty("source_index", i);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        // Accent dot color via left border
        btn->setStyleSheet(
            QString("QPushButton#econSourceBadge[active=true]{"
                    " border-left:2px solid %1; color:%1;"
                    " background:rgba(%2,%3,%4,0.1); }")
                .arg(s.color)
                .arg(QColor(s.color).red())
                .arg(QColor(s.color).green())
                .arg(QColor(s.color).blue()));
        connect(btn, &QPushButton::clicked, this, [this, i](){
            source_combo_->setCurrentIndex(i);
        });
        hl->addWidget(btn);
    }
    hl->addStretch(1);

    // Tip label
    auto* tip = new QLabel("⚿ = API key required");
    tip->setObjectName("econStatusText");
    tip->setStyleSheet("color:#404040; font-size:9px; background:transparent;");
    hl->addWidget(tip);

    return source_badge_bar_;
}

// ── Left panel ───────────────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_left_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("econLeftPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Country section ──
    auto* c_hdr = new QWidget;
    c_hdr->setObjectName("econPanelHeader");
    c_hdr->setFixedHeight(28);
    auto* chl = new QHBoxLayout(c_hdr);
    chl->setContentsMargins(10, 0, 8, 0);
    auto* c_title = new QLabel("COUNTRY");
    c_title->setObjectName("econPanelTitle");
    country_count_ = new QLabel("0");
    country_count_->setObjectName("econCountBadge");
    chl->addWidget(c_title);
    chl->addStretch(1);
    chl->addWidget(country_count_);
    vl->addWidget(c_hdr);

    country_search_ = new QLineEdit;
    country_search_->setObjectName("econSearch");
    country_search_->setPlaceholderText("Search countries…");
    country_search_->setFixedHeight(28);
    connect(country_search_, &QLineEdit::textChanged,
            this, &EconomicsScreen::on_country_search);
    vl->addWidget(country_search_);

    country_list_ = new QListWidget;
    country_list_->setObjectName("econCountryList");
    country_list_->setAlternatingRowColors(false);
    connect(country_list_, &QListWidget::itemClicked,
            this, &EconomicsScreen::on_country_clicked);
    vl->addWidget(country_list_, 1);

    // ── Divider ──
    auto* div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet("color:#1a1a1a;");
    vl->addWidget(div);

    // ── Indicator section ──
    auto* i_hdr = new QWidget;
    i_hdr->setObjectName("econPanelHeader");
    i_hdr->setFixedHeight(28);
    auto* ihl = new QHBoxLayout(i_hdr);
    ihl->setContentsMargins(10, 0, 8, 0);
    auto* i_title = new QLabel("INDICATOR");
    i_title->setObjectName("econPanelTitle");
    indicator_count_ = new QLabel("0");
    indicator_count_->setObjectName("econCountBadge");
    ihl->addWidget(i_title);
    ihl->addStretch(1);
    ihl->addWidget(indicator_count_);
    vl->addWidget(i_hdr);

    indicator_search_ = new QLineEdit;
    indicator_search_->setObjectName("econSearch");
    indicator_search_->setPlaceholderText("Search indicators…");
    indicator_search_->setFixedHeight(28);
    connect(indicator_search_, &QLineEdit::textChanged,
            this, &EconomicsScreen::on_indicator_search);
    vl->addWidget(indicator_search_);

    indicator_list_ = new QListWidget;
    indicator_list_->setObjectName("econIndicatorList");
    connect(indicator_list_, &QListWidget::itemClicked,
            this, &EconomicsScreen::on_indicator_clicked);
    vl->addWidget(indicator_list_, 1);

    return panel;
}

// ── Stat card factory ────────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_stat_card(const QString& label,
                                            QLabel*& value_out,
                                            QLabel*& sub_out) {
    auto* card = new QWidget;
    card->setObjectName("econStatCard");
    card->setMinimumWidth(90);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(2);

    auto* lbl = new QLabel(label);
    lbl->setObjectName("econStatLabel");

    value_out = new QLabel("—");
    value_out->setObjectName("econStatValue");

    sub_out = new QLabel("");
    sub_out->setObjectName("econStatSub");

    vl->addWidget(lbl);
    vl->addWidget(value_out);
    vl->addWidget(sub_out);
    return card;
}

// ── Right panel ──────────────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_right_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("econRightPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Stats row ──
    auto* stats_row = new QWidget;
    stats_row->setObjectName("econStatsRow");
    stats_row->setFixedHeight(72);
    auto* shl = new QHBoxLayout(stats_row);
    shl->setContentsMargins(10, 8, 10, 8);
    shl->setSpacing(6);

    shl->addWidget(create_stat_card("LATEST VALUE",  stat_latest_val_, stat_latest_sub_));
    shl->addWidget(create_stat_card("PERIOD CHANGE", stat_change_val_, stat_change_sub_));
    stat_change_val_->setObjectName("econStatChangePos");
    shl->addWidget(create_stat_card("MINIMUM",       stat_min_val_,    stat_min_sub_));
    shl->addWidget(create_stat_card("MAXIMUM",       stat_max_val_,    stat_max_sub_));
    shl->addWidget(create_stat_card("AVERAGE",       stat_avg_val_,    stat_avg_sub_));
    shl->addWidget(create_stat_card("DATA POINTS",   stat_count_val_,  stat_count_sub_));
    shl->addStretch(1);
    vl->addWidget(stats_row);

    // ── Table header bar ──
    auto* tbl_hdr = new QWidget;
    tbl_hdr->setObjectName("econTableHeader");
    tbl_hdr->setFixedHeight(34);
    auto* thl = new QHBoxLayout(tbl_hdr);
    thl->setContentsMargins(12, 0, 12, 0);
    thl->setSpacing(10);

    data_title_ = new QLabel("Select a country and indicator to fetch data");
    data_title_->setObjectName("econDataTitle");
    thl->addWidget(data_title_);
    thl->addStretch(1);
    data_status_ = new QLabel;
    data_status_->setObjectName("econDataStatus");
    thl->addWidget(data_status_);
    vl->addWidget(tbl_hdr);

    // ── Stacked: empty state vs table ──
    table_stack_ = new QStackedWidget;

    // Page 0 — empty state
    empty_state_ = new QWidget;
    empty_state_->setObjectName("econEmptyState");
    auto* evl = new QVBoxLayout(empty_state_);
    evl->setAlignment(Qt::AlignCenter);
    auto* e1 = new QLabel("NO DATA LOADED");
    e1->setObjectName("econEmptyTitle");
    e1->setAlignment(Qt::AlignCenter);
    auto* e2 = new QLabel("Select a source · pick a country · click an indicator");
    e2->setObjectName("econEmptyHint");
    e2->setAlignment(Qt::AlignCenter);
    evl->addWidget(e1);
    evl->addSpacing(6);
    evl->addWidget(e2);
    table_stack_->addWidget(empty_state_);

    // Page 1 — data table
    data_table_ = new QTableWidget;
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->verticalHeader()->setVisible(false);
    data_table_->horizontalHeader()->setStretchLastSection(true);
    data_table_->horizontalHeader()->setSortIndicatorShown(true);
    data_table_->setSortingEnabled(true);
    data_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    data_table_->setAlternatingRowColors(true);
    data_table_->setShowGrid(true);
    table_stack_->addWidget(data_table_);

    vl->addWidget(table_stack_, 1);
    return panel;
}

// ── Status bar ───────────────────────────────────────────────────────────────

QWidget* EconomicsScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("econStatusBar");
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(6);

    auto sep = [&]() {
        auto* l = new QLabel("·");
        l->setObjectName("econStatusSep");
        return l;
    };

    auto* left = new QLabel("ECONOMICS");
    left->setObjectName("econStatusText");
    hl->addWidget(left);

    hl->addWidget(sep());

    auto* src_lbl = new QLabel("SOURCE:");
    src_lbl->setObjectName("econStatusText");
    hl->addWidget(src_lbl);
    status_source_ = new QLabel("World Bank");
    status_source_->setObjectName("econStatusVal");
    hl->addWidget(status_source_);

    hl->addWidget(sep());

    auto* ctry_lbl = new QLabel("COUNTRY:");
    ctry_lbl->setObjectName("econStatusText");
    hl->addWidget(ctry_lbl);
    status_country_ = new QLabel("USA");
    status_country_->setObjectName("econStatusVal");
    hl->addWidget(status_country_);

    hl->addWidget(sep());

    auto* ind_lbl = new QLabel("INDICATOR:");
    ind_lbl->setObjectName("econStatusText");
    hl->addWidget(ind_lbl);
    status_indicator_ = new QLabel("—");
    status_indicator_->setObjectName("econStatusHigh");
    hl->addWidget(status_indicator_);

    hl->addStretch(1);

    status_points_ = new QLabel;
    status_points_->setObjectName("econStatusText");
    hl->addWidget(status_points_);

    return bar;
}

// ── Slots ────────────────────────────────────────────────────────────────────

void EconomicsScreen::on_source_changed(int index) {
    if (index < 0 || index >= sources_.size())
        return;
    active_source_    = index;
    active_indicator_ = sources_[index].default_indicator;

    status_source_->setText(sources_[index].name);
    update_source_badge(index);
    populate_countries(index);
    populate_indicators(index);

    LOG_INFO("Economics", "Source: " + sources_[index].name);
}

void EconomicsScreen::on_country_clicked(QListWidgetItem* item) {
    if (!item) return;
    active_country_ = item->data(Qt::UserRole).toString();
    status_country_->setText(active_country_);
}

void EconomicsScreen::on_indicator_clicked(QListWidgetItem* item) {
    if (!item) return;
    active_indicator_ = item->data(Qt::UserRole).toString();
    if (active_indicator_.isEmpty()) return; // category header
    status_indicator_->setText(active_indicator_);
    on_fetch();
}

void EconomicsScreen::on_fetch() {
    if (loading_ || active_indicator_.isEmpty())
        return;

    const auto& src = sources_[active_source_];
    QStringList args;

    if (src.id == "worldbank") {
        args << "indicators" << active_country_ << active_indicator_;
    } else if (src.id == "fred") {
        args << "series" << active_indicator_;
        if (date_preset_->currentIndex() == 5) {
            if (!date_start_->text().isEmpty()) args << date_start_->text();
            if (!date_end_->text().isEmpty())   args << date_end_->text();
        }
    } else if (src.id == "imf") {
        args << "economic_indicators" << active_country_ << active_indicator_;
    } else {
        args << active_indicator_ << active_country_;
        if (date_preset_->currentIndex() == 5) {
            if (!date_start_->text().isEmpty()) args << date_start_->text();
            if (!date_end_->text().isEmpty())   args << date_end_->text();
        }
    }

    execute_fetch(src.script, args);
}

void EconomicsScreen::on_date_preset(int index) {
    bool custom = (index == 5);
    date_start_->setVisible(custom);
    date_end_->setVisible(custom);
}

void EconomicsScreen::on_country_search(const QString& text) {
    for (int i = 0; i < country_list_->count(); ++i) {
        auto* item = country_list_->item(i);
        bool match = text.isEmpty()
            || item->text().contains(text, Qt::CaseInsensitive)
            || item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void EconomicsScreen::on_indicator_search(const QString& text) {
    for (int i = 0; i < indicator_list_->count(); ++i) {
        auto* item = indicator_list_->item(i);
        bool match = text.isEmpty()
            || item->text().contains(text, Qt::CaseInsensitive)
            || item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

// ── Population ───────────────────────────────────────────────────────────────

void EconomicsScreen::populate_countries(int source_index) {
    country_list_->clear();
    const auto& src = sources_[source_index];
    QStringList countries;

    if (src.id == "oecd")
        countries = g_oecd_countries;
    else if (src.id == "adb")
        countries = g_adb_countries;
    else if (src.id == "fred" || src.id == "fed" || src.id == "bls"
             || src.id == "fiscaldata" || src.id == "bea")
        countries = {"USA"};
    else
        countries = g_major_countries;

    for (const auto& c : countries) {
        auto* item = new QListWidgetItem(c);
        item->setData(Qt::UserRole, c);
        country_list_->addItem(item);
    }
    country_count_->setText(QString::number(countries.size()));

    int usa_row = countries.indexOf("USA");
    country_list_->setCurrentRow(usa_row >= 0 ? usa_row : 0);
    if (country_list_->currentItem())
        active_country_ = country_list_->currentItem()->data(Qt::UserRole).toString();
}

void EconomicsScreen::populate_indicators(int source_index) {
    indicator_list_->clear();
    const auto& src = sources_[source_index];

    QList<IndicatorDef> indicators;
    if (src.id == "worldbank") indicators = g_worldbank_indicators;
    else if (src.id == "fred") indicators = g_fred_indicators;
    else if (src.id == "imf")  indicators = g_imf_indicators;
    else                       indicators = g_generic_indicators;

    QString last_cat;
    for (const auto& ind : indicators) {
        if (ind.category != last_cat) {
            auto* header = new QListWidgetItem("  " + ind.category.toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(QColor(colors::AMBER));
            auto f = header->font();
            f.setBold(true);
            f.setPointSize(8);
            header->setFont(f);
            indicator_list_->addItem(header);
            last_cat = ind.category;
        }
        auto* item = new QListWidgetItem("    " + ind.name);
        item->setData(Qt::UserRole, ind.id);
        item->setToolTip(ind.id);
        indicator_list_->addItem(item);
    }

    indicator_count_->setText(QString::number(indicators.size()));
    active_indicator_ = src.default_indicator;
    status_indicator_->setText(active_indicator_);
}

// ── Source badge highlight ───────────────────────────────────────────────────

void EconomicsScreen::update_source_badge(int index) {
    if (!source_badge_bar_) return;
    const auto& src = sources_[index];
    auto* hl = qobject_cast<QHBoxLayout*>(source_badge_bar_->layout());
    if (!hl) return;
    for (int i = 0; i < sources_.size(); ++i) {
        auto* w = hl->itemAt(i) ? hl->itemAt(i)->widget() : nullptr;
        if (!w) continue;
        bool active = (i == index);
        w->setProperty("active", active);
        if (active) {
            const auto& s = sources_[i];
            w->setStyleSheet(
                QString("QPushButton#econSourceBadge {"
                        " border:1px solid %1; border-left:2px solid %1;"
                        " color:%1; background:rgba(%2,%3,%4,0.12);"
                        " font-size:9px; font-weight:700; padding:3px 10px; }")
                    .arg(s.color)
                    .arg(QColor(s.color).red())
                    .arg(QColor(s.color).green())
                    .arg(QColor(s.color).blue()));
        } else {
            w->setStyleSheet("");
        }
        // force style recalc
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

// ── Data fetch ───────────────────────────────────────────────────────────────

void EconomicsScreen::execute_fetch(const QString& script, const QStringList& args) {
    set_loading(true);
    data_title_->setText("Fetching " + active_indicator_ + "…");
    data_status_->clear();

    QPointer<EconomicsScreen> self = this;

    python::PythonRunner::instance().run(script, args, [self](const python::PythonResult& result) {
        if (!self) return;
        self->set_loading(false);

        if (!result.success) {
            self->display_error(result.error.isEmpty() ? "Fetch failed" : result.error);
            return;
        }

        QString json_str = python::extract_json(result.output);
        if (json_str.isEmpty()) {
            self->display_error("No data returned from script");
            return;
        }

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
        if (doc.isNull()) {
            self->display_error("JSON parse error: " + err.errorString());
            return;
        }

        auto obj = doc.isObject() ? doc.object() : QJsonObject();

        // Check for error field
        if (obj.contains("error") && !obj["error"].isNull()) {
            QString err_msg;
            if (obj["error"].isString())
                err_msg = obj["error"].toString();
            else if (obj["error"].isObject())
                err_msg = obj["error"].toObject()["message"].toString("Unknown error");
            if (!err_msg.isEmpty()) {
                self->display_error(err_msg);
                return;
            }
        }

        // Extract data array — handle all known shapes
        QJsonArray data_array;
        if (doc.isArray()) {
            data_array = doc.array();
        } else if (obj.contains("data") && obj["data"].isArray()) {
            data_array = obj["data"].toArray();
        } else if (obj.contains("observations") && obj["observations"].isArray()) {
            data_array = obj["observations"].toArray(); // FRED
        } else if (obj.contains("results") && obj["results"].isArray()) {
            data_array = obj["results"].toArray();
        } else if (obj.contains("series") && obj["series"].isArray()) {
            data_array = obj["series"].toArray();
        } else {
            data_array.append(obj); // wrap single object
        }

        if (data_array.isEmpty()) {
            self->display_error("No data points returned");
            return;
        }

        self->display_data(data_array, self->active_indicator_);
        self->display_stats(data_array);
        LOG_INFO("Economics", self->active_indicator_ + ": "
                 + QString::number(data_array.size()) + " points");
    });
}

// ── Display ──────────────────────────────────────────────────────────────────

void EconomicsScreen::display_data(const QJsonArray& data, const QString& title) {
    data_table_->setSortingEnabled(false);
    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    if (data.isEmpty()) return;

    // Collect all unique column keys from first row
    QStringList columns;
    auto first = data[0].toObject();
    for (auto it = first.begin(); it != first.end(); ++it)
        columns << it.key();

    // Prefer date/period column first
    for (const auto& preferred : QStringList{"date", "period", "year", "time", "Date", "Year"}) {
        int idx = columns.indexOf(preferred);
        if (idx > 0) { columns.move(idx, 0); break; }
    }

    data_table_->setColumnCount(columns.size());
    data_table_->setHorizontalHeaderLabels(columns);

    int max_rows = qMin(data.size(), 5000);
    data_table_->setRowCount(max_rows);

    for (int row = 0; row < max_rows; ++row) {
        auto obj = data[row].toObject();
        for (int col = 0; col < columns.size(); ++col) {
            auto val = obj.value(columns[col]);
            QString text;
            if (val.isDouble()) {
                double d = val.toDouble();
                // Use integer display for large whole numbers
                if (d == std::floor(d) && std::abs(d) < 1e15)
                    text = QString::number(static_cast<long long>(d));
                else
                    text = QString::number(d, 'g', 8);
            } else if (val.isNull() || val.isUndefined()) {
                text = "—";
            } else {
                text = val.toVariant().toString();
            }

            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignVCenter |
                (val.isDouble() ? Qt::AlignRight : Qt::AlignLeft));

            if (val.isDouble()) {
                double d = val.toDouble();
                if (d < 0)
                    item->setForeground(QColor(colors::NEGATIVE));
                else if (d > 0)
                    item->setForeground(QColor(colors::CYAN));
            }

            data_table_->setItem(row, col, item);
        }
    }

    if (columns.size() <= 12)
        data_table_->resizeColumnsToContents();
    data_table_->setSortingEnabled(true);
    table_stack_->setCurrentIndex(1);

    data_title_->setText(title + "  ·  " + active_country_);
    data_status_->setText(QString::number(data.size()) + " rows");
    status_points_->setText(QString::number(data.size()) + " data points");
}

void EconomicsScreen::display_stats(const QJsonArray& data) {
    if (data.isEmpty()) return;

    QVector<double> values;
    for (const auto& item : data) {
        auto obj = item.toObject();
        bool found = false;

        if (obj.contains("value")) {
            auto v = obj["value"];
            if (v.isDouble()) {
                values.append(v.toDouble()); found = true;
            } else if (v.isString()) {
                bool ok = false;
                double d = v.toString().toDouble(&ok);
                if (ok) { values.append(d); found = true; }
            }
        }
        if (!found) {
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.value().isDouble()) {
                    values.append(it.value().toDouble());
                    found = true;
                    break;
                }
            }
        }
    }

    if (values.isEmpty()) return;

    auto fmt = [](double v) -> QString {
        if (std::abs(v) >= 1e12) return QString::number(v / 1e12, 'f', 2) + "T";
        if (std::abs(v) >= 1e9)  return QString::number(v / 1e9,  'f', 2) + "B";
        if (std::abs(v) >= 1e6)  return QString::number(v / 1e6,  'f', 2) + "M";
        if (std::abs(v) >= 1e3)  return QString::number(v / 1e3,  'f', 2) + "K";
        return QString::number(v, 'f', 2);
    };

    double latest = values.last();
    double prev   = values.size() > 1 ? values[values.size() - 2] : latest;
    double change = (prev != 0) ? ((latest - prev) / std::abs(prev)) * 100.0 : 0.0;
    double min_v  = *std::min_element(values.begin(), values.end());
    double max_v  = *std::max_element(values.begin(), values.end());
    double sum    = 0; for (double v : values) sum += v;
    double avg    = sum / values.size();

    stat_latest_val_->setText(fmt(latest));
    stat_latest_sub_->setText("most recent");

    stat_change_val_->setText(QString("%1%2%")
        .arg(change >= 0 ? "+" : "").arg(change, 0, 'f', 2));
    stat_change_val_->setObjectName(change >= 0 ? "econStatChangePos" : "econStatChangeNeg");
    stat_change_val_->style()->unpolish(stat_change_val_);
    stat_change_val_->style()->polish(stat_change_val_);
    stat_change_sub_->setText("vs prior period");

    stat_min_val_->setText(fmt(min_v));
    stat_min_sub_->setText("all-time low");

    stat_max_val_->setText(fmt(max_v));
    stat_max_sub_->setText("all-time high");

    stat_avg_val_->setText(fmt(avg));
    stat_avg_sub_->setText("period average");

    stat_count_val_->setText(QString::number(values.size()));
    stat_count_sub_->setText("observations");
}

void EconomicsScreen::display_error(const QString& error) {
    table_stack_->setCurrentIndex(0); // show empty state

    // Reset stats
    for (auto* lbl : {stat_latest_val_, stat_change_val_, stat_min_val_,
                      stat_max_val_,    stat_avg_val_,    stat_count_val_}) {
        if (lbl) lbl->setText("—");
    }
    for (auto* lbl : {stat_latest_sub_, stat_change_sub_, stat_min_sub_,
                      stat_max_sub_,    stat_avg_sub_,    stat_count_sub_}) {
        if (lbl) lbl->setText("");
    }

    data_title_->setText("Error: " + error);
    data_status_->clear();
    LOG_ERROR("Economics", error);
}

void EconomicsScreen::set_loading(bool loading) {
    loading_ = loading;
    fetch_btn_->setEnabled(!loading);
    fetch_btn_->setText(loading ? "  LOADING…  " : "  FETCH DATA  ");
    if (loading) {
        data_title_->setText("Fetching data…");
        data_status_->clear();
    }
}

} // namespace fincept::screens
