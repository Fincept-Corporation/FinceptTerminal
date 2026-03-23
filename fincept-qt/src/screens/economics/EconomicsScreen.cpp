#include "screens/economics/EconomicsScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

// ── Stylesheet ──────────────────────────────────────────────────────────────

namespace {
using namespace fincept::ui;
static const QString kStyle = QStringLiteral(
    "#econScreen { background: %1; }"

    "#econHeader { background: %2; border-bottom: 2px solid %3; }"
    "#econHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
    "#econHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"

    "#econFetchBtn { background: %3; color: %1; border: none; padding: 5px 16px; "
    "  font-size: 9px; font-weight: 700; }"
    "#econFetchBtn:hover { background: #FF8800; }"
    "#econFetchBtn:disabled { background: %10; color: %11; }"

    "#econLeftPanel { background: %7; border-right: 1px solid %8; }"
    "#econSectionTitle { color: %5; font-size: 10px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; padding: 6px 8px; "
    "  border-bottom: 1px solid %8; }"

    "#econSearchInput { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 3px 8px; font-size: 11px; }"
    "#econSearchInput:focus { border-color: %9; }"

    "#econCountLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  background: transparent; padding: 2px 8px; }"

    "#econCountryList, #econIndicatorList { background: %1; border: none; "
    "  outline: none; font-size: 11px; }"
    "#econCountryList::item, #econIndicatorList::item { color: %5; "
    "  padding: 4px 8px; border-bottom: 1px solid %8; }"
    "#econCountryList::item:hover, #econIndicatorList::item:hover "
    "  { color: %4; background: %12; }"
    "#econCountryList::item:selected, #econIndicatorList::item:selected "
    "  { color: %3; background: rgba(217,119,6,0.1); border-left: 2px solid %3; }"

    "#econRightPanel { background: %1; }"
    "#econStatsBar { background: %7; border-bottom: 1px solid %8; }"
    "#econStatLabel { color: %5; font-size: 9px; font-weight: 700; "
    "  letter-spacing: 0.5px; background: transparent; }"
    "#econStatValue { color: %13; font-size: 13px; font-weight: 700; "
    "  background: transparent; }"
    "#econStatChange { font-size: 11px; font-weight: 700; background: transparent; }"
    "#econDataTitle { color: %4; font-size: 11px; font-weight: 700; background: transparent; }"
    "#econDataStatus { color: %5; font-size: 11px; background: transparent; }"

    "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
    "  font-size: 11px; }"
    "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
    "QTableWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"
    "QHeaderView::section { background: %2; color: %5; border: none; "
    "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
    "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

    "QComboBox { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 3px 8px; font-size: 11px; }"
    "QComboBox::drop-down { border: none; width: 16px; }"
    "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
    "  selection-background-color: %3; }"

    "QLineEdit { background: %1; color: %4; border: 1px solid %8; "
    "  padding: 3px 8px; font-size: 11px; }"

    "#econStatusBar { background: %2; border-top: 1px solid %8; }"
    "#econStatusText { color: %5; font-size: 9px; background: transparent; }"
    "#econStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

    "#econErrorPanel { background: rgba(220,38,38,0.1); border: 1px solid %14; "
    "  padding: 8px 12px; }"
    "#econErrorText { color: %14; font-size: 10px; background: transparent; }"

    "QSplitter::handle { background: %8; }"
    "QScrollBar:vertical { background: %1; width: 6px; }"
    "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
)
    .arg(colors::BG_BASE)         // %1
    .arg(colors::BG_RAISED)       // %2
    .arg(colors::AMBER)           // %3
    .arg(colors::TEXT_PRIMARY)    // %4
    .arg(colors::TEXT_SECONDARY)  // %5
    .arg(colors::POSITIVE)        // %6  (reserved)
    .arg(colors::BG_SURFACE)      // %7
    .arg(colors::BORDER_DIM)      // %8
    .arg(colors::BORDER_BRIGHT)   // %9
    .arg(colors::AMBER_DIM)       // %10
    .arg(colors::TEXT_DIM)         // %11
    .arg(colors::BG_HOVER)        // %12
    .arg(colors::CYAN)             // %13
    .arg(colors::NEGATIVE)        // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Source definitions ──────────────────────────────────────────────────────

static QList<EconSource> build_sources() {
    return {
        {"worldbank",  "World Bank",     "worldbank_data.py",  "#00E5FF", false, "NY.GDP.MKTP.CD"},
        {"bis",        "BIS",            "bis_data.py",        "#9D4EDD", false, "WS_CBPOL"},
        {"imf",        "IMF",            "imf_data.py",        "#0088FF", false, "NGDP_RPCH"},
        {"fred",       "FRED",           "fred_data.py",       "#00D66F", false, "GDP"},
        {"oecd",       "OECD",           "oecd_data.py",       "#FFD700", false, "GDP"},
        {"wto",        "WTO",            "wto_data.py",        "#E91E63", true,  "TP_A_0010"},
        {"cftc",       "CFTC",           "cftc_data.py",       "#FF5722", false, "gold"},
        {"eia",        "EIA",            "eia_data.py",        "#4CAF50", true,  "crude_petroleum_stocks"},
        {"adb",        "ADB",            "adb_data.py",        "#0072BC", false, "NGDP_XDC"},
        {"fed",        "Federal Reserve", "fed_data.py",       "#1A5F7A", false, "federal_funds_rate"},
        {"bls",        "BLS",            "bls_data.py",        "#AB47BC", true,  "CUSR0000SA0"},
        {"unesco",     "UNESCO",         "unesco_data.py",     "#00ACC1", false, "GER.1"},
        {"fiscaldata", "FiscalData",     "fiscaldata.py",      "#FFA726", true,  "total_public_debt"},
        {"bea",        "BEA",            "bea_data.py",        "#E65100", true,  "gdp_growth"},
        {"fincept",    "Fincept Macro",  "fincept_macro.py",   "#FF8800", true,  "wgb_central_bank_rates"},
    };
}

// ── Country lists per source ────────────────────────────────────────────────

static QStringList g_major_countries = {
    "USA", "CHN", "JPN", "DEU", "GBR", "FRA", "IND", "ITA", "BRA", "CAN",
    "KOR", "RUS", "AUS", "ESP", "MEX", "IDN", "NLD", "SAU", "TUR", "CHE",
    "ARG", "ZAF", "THA", "SGP", "MYS", "PHL", "VNM", "NGA", "EGY", "PAK",
};

static QStringList g_oecd_countries = {
    "USA", "GBR", "DEU", "FRA", "JPN", "CAN", "ITA", "ESP", "AUS", "KOR",
    "NLD", "CHE", "BEL", "SWE", "AUT", "NOR", "DNK", "FIN", "IRL", "PRT",
    "NZL", "ISR", "CZE", "POL", "HUN", "SVK", "SVN", "LUX", "ISL", "EST",
    "LVA", "LTU", "CHL", "COL", "CRI", "MEX", "TUR",
};

static QStringList g_adb_countries = {
    "IND", "CHN", "JPN", "KOR", "IDN", "THA", "MYS", "PHL", "VNM", "SGP",
    "BGD", "PAK", "LKA", "MMR", "KHM", "LAO", "MNG", "NPL", "AFG", "FJI",
};

// ── Indicator lists (simplified — key ones per source) ──────────────────────

struct IndicatorDef {
    QString id;
    QString name;
    QString category;
};

static QList<IndicatorDef> g_worldbank_indicators = {
    {"NY.GDP.MKTP.CD", "GDP (current US$)", "GDP"},
    {"NY.GDP.MKTP.KD.ZG", "GDP growth (annual %)", "GDP"},
    {"NY.GDP.PCAP.CD", "GDP per capita (US$)", "GDP"},
    {"FP.CPI.TOTL.ZG", "Inflation, CPI (annual %)", "Prices"},
    {"SL.UEM.TOTL.ZS", "Unemployment (%)", "Labor"},
    {"NE.TRD.GNFS.ZS", "Trade (% of GDP)", "Trade"},
    {"BX.KLT.DINV.CD.WD", "FDI inflows (US$)", "Investment"},
    {"SP.POP.TOTL", "Population, total", "Demographics"},
    {"SP.DYN.LE00.IN", "Life expectancy", "Demographics"},
    {"SE.XPD.TOTL.GD.ZS", "Education spending (% GDP)", "Social"},
    {"SH.XPD.CHEX.GD.ZS", "Health spending (% GDP)", "Social"},
    {"NE.EXP.GNFS.ZS", "Exports (% of GDP)", "Trade"},
    {"NE.IMP.GNFS.ZS", "Imports (% of GDP)", "Trade"},
    {"GC.DOD.TOTL.GD.ZS", "Govt debt (% of GDP)", "Fiscal"},
    {"FM.LBL.BMNY.GD.ZS", "Broad money (% of GDP)", "Financial"},
};

static QList<IndicatorDef> g_fred_indicators = {
    {"GDP", "Gross Domestic Product", "GDP"},
    {"GDPC1", "Real GDP", "GDP"},
    {"A939RX0Q048SBEA", "Real GDP per capita", "GDP"},
    {"CPIAUCSL", "CPI All Urban", "Prices"},
    {"CPILFESL", "Core CPI", "Prices"},
    {"UNRATE", "Unemployment Rate", "Labor"},
    {"PAYEMS", "Total Nonfarm Payrolls", "Labor"},
    {"FEDFUNDS", "Federal Funds Rate", "Rates"},
    {"DGS10", "10-Year Treasury", "Rates"},
    {"DGS2", "2-Year Treasury", "Rates"},
    {"DEXUSEU", "USD/EUR Exchange Rate", "FX"},
    {"M2SL", "M2 Money Stock", "Money"},
    {"WALCL", "Fed Total Assets", "Fed"},
    {"HOUST", "Housing Starts", "Housing"},
    {"INDPRO", "Industrial Production", "Output"},
};

static QList<IndicatorDef> g_imf_indicators = {
    {"NGDP_RPCH", "Real GDP growth (%)", "GDP"},
    {"NGDPD", "Nominal GDP (bn USD)", "GDP"},
    {"NGDPDPC", "GDP per capita (USD)", "GDP"},
    {"PCPIPCH", "Inflation rate (%)", "Prices"},
    {"LUR", "Unemployment rate (%)", "Labor"},
    {"BCA_NGDPD", "Current account (% GDP)", "External"},
    {"GGXWDG_NGDP", "Govt gross debt (% GDP)", "Fiscal"},
    {"GGR_NGDP", "Govt revenue (% GDP)", "Fiscal"},
    {"GGX_NGDP", "Govt spending (% GDP)", "Fiscal"},
    {"PPPEX", "PPP exchange rate", "FX"},
};

static QList<IndicatorDef> g_generic_indicators = {
    {"default", "Default Indicator", "General"},
};

// ── Constructor ─────────────────────────────────────────────────────────────

EconomicsScreen::EconomicsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("econScreen");
    setStyleSheet(kStyle);
    sources_ = build_sources();
    setup_ui();
    LOG_INFO("Economics", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void EconomicsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());

    // Main splitter: left (countries + indicators) | right (stats + table)
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(260);
    left->setMaximumWidth(340);

    auto* right = create_right_panel();

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 800});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());

    // Initialize with first source
    on_source_changed(0);
}

QWidget* EconomicsScreen::create_header() {
    auto* bar = new QWidget;
    bar->setObjectName("econHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    // Title
    auto* title = new QLabel("ECONOMIC DATA");
    title->setObjectName("econHeaderTitle");
    hl->addWidget(title);

    hl->addSpacing(12);

    // Source selector
    source_combo_ = new QComboBox;
    source_combo_->setFixedWidth(180);
    for (const auto& s : sources_) {
        QString label = s.name;
        if (s.needs_api_key) label += " (API Key)";
        source_combo_->addItem(label);
    }
    connect(source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EconomicsScreen::on_source_changed);
    hl->addWidget(source_combo_);

    hl->addSpacing(8);

    // Date range
    date_preset_ = new QComboBox;
    date_preset_->addItems({"1Y", "2Y", "5Y", "10Y", "ALL", "Custom"});
    date_preset_->setCurrentIndex(2); // 5Y default
    date_preset_->setFixedWidth(70);
    connect(date_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EconomicsScreen::on_date_preset);
    hl->addWidget(date_preset_);

    date_start_ = new QLineEdit;
    date_start_->setPlaceholderText("Start");
    date_start_->setFixedWidth(90);
    date_start_->hide();

    date_end_ = new QLineEdit;
    date_end_->setPlaceholderText("End");
    date_end_->setFixedWidth(90);
    date_end_->hide();

    hl->addWidget(date_start_);
    hl->addWidget(date_end_);

    hl->addStretch(1);

    // Fetch button
    fetch_btn_ = new QPushButton("FETCH DATA");
    fetch_btn_->setObjectName("econFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &EconomicsScreen::on_fetch);
    hl->addWidget(fetch_btn_);

    return bar;
}

QWidget* EconomicsScreen::create_left_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("econLeftPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Countries ──
    auto* c_header = new QWidget;
    auto* chl = new QHBoxLayout(c_header);
    chl->setContentsMargins(8, 6, 8, 4);
    chl->setSpacing(4);
    auto* c_title = new QLabel("COUNTRY");
    c_title->setObjectName("econSectionTitle");
    c_title->setStyleSheet("border: none; padding: 0;");
    country_count_ = new QLabel;
    country_count_->setObjectName("econCountLabel");
    chl->addWidget(c_title);
    chl->addStretch(1);
    chl->addWidget(country_count_);
    vl->addWidget(c_header);

    country_search_ = new QLineEdit;
    country_search_->setObjectName("econSearchInput");
    country_search_->setPlaceholderText("Search countries...");
    country_search_->setContentsMargins(8, 0, 8, 0);
    connect(country_search_, &QLineEdit::textChanged, this, &EconomicsScreen::on_country_search);
    auto* cs_wrap = new QWidget;
    auto* cswl = new QHBoxLayout(cs_wrap);
    cswl->setContentsMargins(8, 0, 8, 6);
    cswl->addWidget(country_search_);
    vl->addWidget(cs_wrap);

    country_list_ = new QListWidget;
    country_list_->setObjectName("econCountryList");
    connect(country_list_, &QListWidget::itemClicked, this, &EconomicsScreen::on_country_clicked);
    vl->addWidget(country_list_, 1);

    // ── Indicators ──
    auto* i_header = new QWidget;
    auto* ihl = new QHBoxLayout(i_header);
    ihl->setContentsMargins(8, 6, 8, 4);
    ihl->setSpacing(4);
    auto* i_title = new QLabel("INDICATORS");
    i_title->setObjectName("econSectionTitle");
    i_title->setStyleSheet("border: none; padding: 0;");
    indicator_count_ = new QLabel;
    indicator_count_->setObjectName("econCountLabel");
    ihl->addWidget(i_title);
    ihl->addStretch(1);
    ihl->addWidget(indicator_count_);
    vl->addWidget(i_header);

    indicator_search_ = new QLineEdit;
    indicator_search_->setObjectName("econSearchInput");
    indicator_search_->setPlaceholderText("Search indicators...");
    connect(indicator_search_, &QLineEdit::textChanged, this, &EconomicsScreen::on_indicator_search);
    auto* is_wrap = new QWidget;
    auto* iswl = new QHBoxLayout(is_wrap);
    iswl->setContentsMargins(8, 0, 8, 6);
    iswl->addWidget(indicator_search_);
    vl->addWidget(is_wrap);

    indicator_list_ = new QListWidget;
    indicator_list_->setObjectName("econIndicatorList");
    connect(indicator_list_, &QListWidget::itemClicked, this, &EconomicsScreen::on_indicator_clicked);
    vl->addWidget(indicator_list_, 1);

    return panel;
}

QWidget* EconomicsScreen::create_right_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("econRightPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Stats bar
    auto* stats = new QWidget;
    stats->setObjectName("econStatsBar");
    stats->setFixedHeight(52);
    auto* shl = new QHBoxLayout(stats);
    shl->setContentsMargins(12, 4, 12, 4);
    shl->setSpacing(20);

    auto make_stat = [&](const QString& label, QLabel*& value_lbl) {
        auto* box = new QWidget;
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("econStatLabel");
        value_lbl = new QLabel("--");
        value_lbl->setObjectName("econStatValue");
        bvl->addWidget(lbl);
        bvl->addWidget(value_lbl);
        shl->addWidget(box);
    };

    make_stat("LATEST", stat_latest_);
    make_stat("CHANGE", stat_change_);
    stat_change_->setObjectName("econStatChange");
    make_stat("MIN", stat_min_);
    make_stat("MAX", stat_max_);
    make_stat("AVG", stat_avg_);
    make_stat("POINTS", stat_count_);
    shl->addStretch(1);
    vl->addWidget(stats);

    // Data title + status
    auto* title_bar = new QWidget;
    title_bar->setFixedHeight(30);
    auto* tbl = new QHBoxLayout(title_bar);
    tbl->setContentsMargins(12, 0, 12, 0);
    data_title_ = new QLabel("Select a source and indicator");
    data_title_->setObjectName("econDataTitle");
    data_status_ = new QLabel;
    data_status_->setObjectName("econDataStatus");
    tbl->addWidget(data_title_);
    tbl->addStretch(1);
    tbl->addWidget(data_status_);
    vl->addWidget(title_bar);

    // Data table
    data_table_ = new QTableWidget;
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->verticalHeader()->setVisible(false);
    data_table_->horizontalHeader()->setStretchLastSection(true);
    data_table_->horizontalHeader()->setSortIndicatorShown(true);
    data_table_->setSortingEnabled(true);
    data_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vl->addWidget(data_table_, 1);

    return panel;
}

QWidget* EconomicsScreen::create_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("econStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("ECONOMICS");
    left->setObjectName("econStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_source_ = new QLabel("SOURCE: World Bank");
    status_source_->setObjectName("econStatusText");
    hl->addWidget(status_source_);

    hl->addSpacing(12);
    status_country_ = new QLabel("COUNTRY: USA");
    status_country_->setObjectName("econStatusText");
    hl->addWidget(status_country_);

    hl->addSpacing(12);
    status_indicator_ = new QLabel;
    status_indicator_->setObjectName("econStatusHighlight");
    hl->addWidget(status_indicator_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void EconomicsScreen::on_source_changed(int index) {
    if (index < 0 || index >= sources_.size()) return;
    active_source_ = index;
    active_indicator_ = sources_[index].default_indicator;

    status_source_->setText("SOURCE: " + sources_[index].name);

    populate_countries(index);
    populate_indicators(index);

    LOG_INFO("Economics", "Source: " + sources_[index].name);
}

void EconomicsScreen::on_country_clicked(QListWidgetItem* item) {
    if (!item) return;
    active_country_ = item->data(Qt::UserRole).toString();
    status_country_->setText("COUNTRY: " + active_country_);
}

void EconomicsScreen::on_indicator_clicked(QListWidgetItem* item) {
    if (!item) return;
    active_indicator_ = item->data(Qt::UserRole).toString();
    status_indicator_->setText(active_indicator_);

    // Auto-fetch
    on_fetch();
}

void EconomicsScreen::on_fetch() {
    if (loading_ || active_indicator_.isEmpty()) return;

    const auto& src = sources_[active_source_];
    QStringList args;
    args << active_indicator_ << active_country_;

    // Add date range if custom
    if (date_preset_->currentIndex() == 5) { // Custom
        if (!date_start_->text().isEmpty()) args << date_start_->text();
        if (!date_end_->text().isEmpty()) args << date_end_->text();
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
        bool match = text.isEmpty() ||
                     item->text().contains(text, Qt::CaseInsensitive) ||
                     item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void EconomicsScreen::on_indicator_search(const QString& text) {
    for (int i = 0; i < indicator_list_->count(); ++i) {
        auto* item = indicator_list_->item(i);
        bool match = text.isEmpty() ||
                     item->text().contains(text, Qt::CaseInsensitive) ||
                     item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

// ── Population ──────────────────────────────────────────────────────────────

void EconomicsScreen::populate_countries(int source_index) {
    country_list_->clear();

    const auto& src = sources_[source_index];
    QStringList countries;

    if (src.id == "oecd") countries = g_oecd_countries;
    else if (src.id == "adb") countries = g_adb_countries;
    else if (src.id == "fred" || src.id == "fed" || src.id == "bls" ||
             src.id == "fiscaldata" || src.id == "bea") {
        countries = {"USA"}; // US-only sources
    } else {
        countries = g_major_countries;
    }

    for (const auto& c : countries) {
        auto* item = new QListWidgetItem(c);
        item->setData(Qt::UserRole, c);
        country_list_->addItem(item);
    }

    country_count_->setText(QString::number(countries.size()));

    // Select USA by default
    if (!countries.isEmpty()) {
        country_list_->setCurrentRow(countries.indexOf("USA") >= 0 ? countries.indexOf("USA") : 0);
        active_country_ = country_list_->currentItem()->data(Qt::UserRole).toString();
    }
}

void EconomicsScreen::populate_indicators(int source_index) {
    indicator_list_->clear();

    const auto& src = sources_[source_index];
    QList<IndicatorDef> indicators;

    if (src.id == "worldbank") indicators = g_worldbank_indicators;
    else if (src.id == "fred") indicators = g_fred_indicators;
    else if (src.id == "imf") indicators = g_imf_indicators;
    else indicators = g_generic_indicators;

    // Group by category
    QString last_cat;
    for (const auto& ind : indicators) {
        if (ind.category != last_cat) {
            auto* header = new QListWidgetItem(ind.category.toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(QColor(colors::AMBER));
            auto f = header->font();
            f.setBold(true);
            f.setPointSize(8);
            header->setFont(f);
            indicator_list_->addItem(header);
            last_cat = ind.category;
        }

        auto* item = new QListWidgetItem("  " + ind.name);
        item->setData(Qt::UserRole, ind.id);
        item->setToolTip(ind.id);
        indicator_list_->addItem(item);
    }

    indicator_count_->setText(QString::number(indicators.size()));
    active_indicator_ = src.default_indicator;
    status_indicator_->setText(active_indicator_);
}

// ── Data fetching ───────────────────────────────────────────────────────────

void EconomicsScreen::execute_fetch(const QString& script, const QStringList& args) {
    set_loading(true);
    data_title_->setText("Fetching " + active_indicator_ + "...");

    QPointer<EconomicsScreen> self = this;

    python::PythonRunner::instance().run(
        script, args,
        [self](const python::PythonResult& result) {
            if (!self) return;
            self->set_loading(false);

            if (!result.success) {
                self->display_error(result.error.isEmpty() ? "Fetch failed" : result.error);
                return;
            }

            QString json_str = python::extract_json(result.output);
            if (json_str.isEmpty()) {
                self->display_error("No data returned");
                return;
            }

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull()) {
                self->display_error("JSON parse error: " + err.errorString());
                return;
            }

            auto obj = doc.isObject() ? doc.object() : QJsonObject();
            if (obj.contains("error")) {
                self->display_error(obj["error"].toString());
                return;
            }

            // Extract data array
            QJsonArray data_array;
            if (obj.contains("data") && obj["data"].isArray()) {
                data_array = obj["data"].toArray();
            } else if (doc.isArray()) {
                data_array = doc.array();
            } else if (obj.contains("data")) {
                QJsonObject wrapper;
                wrapper["value"] = obj["data"];
                data_array.append(wrapper);
            }

            if (data_array.isEmpty()) {
                self->display_error("No data points returned");
                return;
            }

            self->display_data(data_array, self->active_indicator_);
            self->display_stats(data_array);
            LOG_INFO("Economics", self->active_indicator_ + ": " +
                     QString::number(data_array.size()) + " points");
        });
}

// ── Display ─────────────────────────────────────────────────────────────────

void EconomicsScreen::display_data(const QJsonArray& data, const QString& title) {
    data_table_->setSortingEnabled(false);
    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    if (data.isEmpty()) return;

    // Extract columns
    QStringList columns;
    auto first = data[0].toObject();
    for (auto it = first.begin(); it != first.end(); ++it) {
        columns << it.key();
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
                text = QString::number(val.toDouble(), 'g', 10);
            } else if (val.isNull()) {
                text = "--";
            } else {
                text = val.toVariant().toString();
            }

            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            if (val.isDouble()) {
                item->setForeground(QColor(val.toDouble() < 0 ? colors::NEGATIVE : colors::CYAN));
            }

            data_table_->setItem(row, col, item);
        }
    }

    if (columns.size() <= 15) {
        data_table_->resizeColumnsToContents();
    }
    data_table_->setSortingEnabled(true);

    data_title_->setText(title + " — " + active_country_);
    data_status_->setText(QString::number(data.size()) + " data points");
}

void EconomicsScreen::display_stats(const QJsonArray& data) {
    // Find the value column (try "value", then first numeric)
    if (data.isEmpty()) return;

    QVector<double> values;
    for (const auto& item : data) {
        auto obj = item.toObject();
        double v = 0;
        if (obj.contains("value") && obj["value"].isDouble()) {
            v = obj["value"].toDouble();
        } else {
            // Find first numeric field
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.value().isDouble()) {
                    v = it.value().toDouble();
                    break;
                }
            }
        }
        values.append(v);
    }

    if (values.isEmpty()) return;

    double latest = values.last();
    double prev = values.size() > 1 ? values[values.size() - 2] : latest;
    double change = (prev != 0) ? ((latest - prev) / std::abs(prev)) * 100.0 : 0;
    double min_val = *std::min_element(values.begin(), values.end());
    double max_val = *std::max_element(values.begin(), values.end());
    double sum = 0;
    for (double v : values) sum += v;
    double avg = sum / values.size();

    auto fmt = [](double v) {
        if (std::abs(v) >= 1e9) return QString::number(v / 1e9, 'f', 2) + "B";
        if (std::abs(v) >= 1e6) return QString::number(v / 1e6, 'f', 2) + "M";
        if (std::abs(v) >= 1e3) return QString::number(v / 1e3, 'f', 2) + "K";
        return QString::number(v, 'f', 2);
    };

    stat_latest_->setText(fmt(latest));
    stat_change_->setText(QString("%1%2%")
                              .arg(change >= 0 ? "+" : "")
                              .arg(change, 0, 'f', 2));
    stat_change_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;")
                                    .arg(change >= 0 ? colors::POSITIVE : colors::NEGATIVE));
    stat_min_->setText(fmt(min_val));
    stat_max_->setText(fmt(max_val));
    stat_avg_->setText(fmt(avg));
    stat_count_->setText(QString::number(values.size()));
}

void EconomicsScreen::display_error(const QString& error) {
    data_title_->setText("Error");
    data_status_->setText("");
    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    // Reset stats
    stat_latest_->setText("--");
    stat_change_->setText("--");
    stat_min_->setText("--");
    stat_max_->setText("--");
    stat_avg_->setText("--");
    stat_count_->setText("--");

    data_title_->setText("Error: " + error);
    LOG_ERROR("Economics", error);
}

void EconomicsScreen::set_loading(bool loading) {
    loading_ = loading;
    fetch_btn_->setEnabled(!loading);
    fetch_btn_->setText(loading ? "LOADING..." : "FETCH DATA");
}

} // namespace fincept::screens
