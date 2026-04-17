// src/screens/portfolio/PortfolioSectorPanel.cpp
#include "screens/portfolio/PortfolioSectorPanel.h"

#include "ui/theme/Theme.h"

#include <QChart>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPieSeries>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

namespace fincept::screens {

// 16 sector colors
static const QColor kSectorColors[] = {
    QColor("#0891b2"), // Technology
    QColor("#16a34a"), // Healthcare
    QColor("#d97706"), // Financial Services
    QColor("#ca8a04"), // Energy
    QColor("#9333ea"), // Consumer Cyclical
    QColor("#84cc16"), // Real Estate
    QColor("#0d9488"), // Utilities
    QColor("#f97316"), // Cryptocurrency
    QColor("#60a5fa"), // Communication
    QColor("#f43f5e"), // Consumer Defensive
    QColor("#a78bfa"), // Industrials
    QColor("#fbbf24"), // Basic Materials
    QColor("#2dd4bf"), // Bonds / Fixed Income
    QColor("#fb923c"), // Commodities
    QColor("#818cf8"), // ETF / Index
    QColor("#e879f9"), // Unknown
};

QColor PortfolioSectorPanel::sector_color(int index) {
    return kSectorColors[index % 16];
}

QString PortfolioSectorPanel::infer_sector(const QString& sym) {
    const QString s = sym.toUpper();

    // ── Exact symbol map (300+ common tickers) ───────────────────────────────
    static const QHash<QString, QString> known = {
        // Technology
        {"AAPL", "Technology"},
        {"MSFT", "Technology"},
        {"GOOGL", "Technology"},
        {"GOOG", "Technology"},
        {"AMZN", "Technology"},
        {"NVDA", "Technology"},
        {"META", "Technology"},
        {"AVGO", "Technology"},
        {"ORCL", "Technology"},
        {"CRM", "Technology"},
        {"ADBE", "Technology"},
        {"CSCO", "Technology"},
        {"AMD", "Technology"},
        {"INTC", "Technology"},
        {"QCOM", "Technology"},
        {"TXN", "Technology"},
        {"IBM", "Technology"},
        {"AMAT", "Technology"},
        {"MU", "Technology"},
        {"LRCX", "Technology"},
        {"KLAC", "Technology"},
        {"SNPS", "Technology"},
        {"CDNS", "Technology"},
        {"ADI", "Technology"},
        {"MCHP", "Technology"},
        {"NXPI", "Technology"},
        {"STX", "Technology"},
        {"WDC", "Technology"},
        {"HPQ", "Technology"},
        {"DELL", "Technology"},
        {"ACN", "Technology"},
        {"INTU", "Technology"},
        {"NOW", "Technology"},
        {"SNOW", "Technology"},
        {"DDOG", "Technology"},
        {"ZS", "Technology"},
        {"CRWD", "Technology"},
        {"PANW", "Technology"},
        {"FTNT", "Technology"},
        {"NET", "Technology"},
        {"OKTA", "Technology"},
        {"MDB", "Technology"},
        {"TEAM", "Technology"},
        {"ATLASSIAN", "Technology"},
        {"SHOP", "Technology"},
        {"UBER", "Technology"},
        {"LYFT", "Technology"},
        {"ABNB", "Technology"},
        {"PLTR", "Technology"},
        {"RBLX", "Technology"},
        {"U", "Technology"},
        {"TWLO", "Technology"},
        {"ZM", "Technology"},
        {"DOCUSIGN", "Technology"},
        {"COIN", "Technology"},
        {"HOOD", "Technology"},
        {"APP", "Technology"},
        {"APPF", "Technology"},
        {"GTLB", "Technology"},
        {"PATH", "Technology"},
        // Communication
        {"NFLX", "Communication"},
        {"DIS", "Communication"},
        {"CMCSA", "Communication"},
        {"T", "Communication"},
        {"VZ", "Communication"},
        {"TMUS", "Communication"},
        {"CHTR", "Communication"},
        {"WBD", "Communication"},
        {"PARA", "Communication"},
        {"FOXA", "Communication"},
        {"FOX", "Communication"},
        {"MTCH", "Communication"},
        {"PINS", "Communication"},
        {"SNAP", "Communication"},
        {"RDDT", "Communication"},
        {"SPOT", "Communication"},
        {"LYV", "Communication"},
        {"WMG", "Communication"},
        // Consumer Cyclical
        {"TSLA", "Consumer Cyclical"},
        {"AMZN", "Consumer Cyclical"},
        {"HD", "Consumer Cyclical"},
        {"LOW", "Consumer Cyclical"},
        {"TGT", "Consumer Cyclical"},
        {"BKNG", "Consumer Cyclical"},
        {"MAR", "Consumer Cyclical"},
        {"HLT", "Consumer Cyclical"},
        {"GM", "Consumer Cyclical"},
        {"F", "Consumer Cyclical"},
        {"STLA", "Consumer Cyclical"},
        {"NKE", "Consumer Cyclical"},
        {"SBUX", "Consumer Cyclical"},
        {"MCD", "Consumer Cyclical"},
        {"YUM", "Consumer Cyclical"},
        {"CMG", "Consumer Cyclical"},
        {"DPZ", "Consumer Cyclical"},
        {"ROST", "Consumer Cyclical"},
        {"TJX", "Consumer Cyclical"},
        {"EBAY", "Consumer Cyclical"},
        {"ETSY", "Consumer Cyclical"},
        {"W", "Consumer Cyclical"},
        {"RH", "Consumer Cyclical"},
        {"DHI", "Consumer Cyclical"},
        {"LEN", "Consumer Cyclical"},
        {"PHM", "Consumer Cyclical"},
        {"TOL", "Consumer Cyclical"},
        {"MGM", "Consumer Cyclical"},
        {"WYNN", "Consumer Cyclical"},
        {"LVS", "Consumer Cyclical"},
        {"CCL", "Consumer Cyclical"},
        {"RCL", "Consumer Cyclical"},
        {"NCLH", "Consumer Cyclical"},
        {"UAL", "Consumer Cyclical"},
        {"DAL", "Consumer Cyclical"},
        {"AAL", "Consumer Cyclical"},
        {"LUV", "Consumer Cyclical"},
        {"EXPE", "Consumer Cyclical"},
        // Consumer Defensive
        {"WMT", "Consumer Defensive"},
        {"PG", "Consumer Defensive"},
        {"KO", "Consumer Defensive"},
        {"PEP", "Consumer Defensive"},
        {"COST", "Consumer Defensive"},
        {"PM", "Consumer Defensive"},
        {"MO", "Consumer Defensive"},
        {"MDLZ", "Consumer Defensive"},
        {"KHC", "Consumer Defensive"},
        {"GIS", "Consumer Defensive"},
        {"K", "Consumer Defensive"},
        {"CPB", "Consumer Defensive"},
        {"SJM", "Consumer Defensive"},
        {"HSY", "Consumer Defensive"},
        {"CAG", "Consumer Defensive"},
        {"CL", "Consumer Defensive"},
        {"CHD", "Consumer Defensive"},
        {"KMB", "Consumer Defensive"},
        {"EL", "Consumer Defensive"},
        {"ULTA", "Consumer Defensive"},
        // Healthcare
        {"JNJ", "Healthcare"},
        {"UNH", "Healthcare"},
        {"LLY", "Healthcare"},
        {"ABBV", "Healthcare"},
        {"MRK", "Healthcare"},
        {"PFE", "Healthcare"},
        {"TMO", "Healthcare"},
        {"ABT", "Healthcare"},
        {"DHR", "Healthcare"},
        {"BMY", "Healthcare"},
        {"AMGN", "Healthcare"},
        {"GILD", "Healthcare"},
        {"ISRG", "Healthcare"},
        {"SYK", "Healthcare"},
        {"BSX", "Healthcare"},
        {"MDT", "Healthcare"},
        {"BDX", "Healthcare"},
        {"EW", "Healthcare"},
        {"DXCM", "Healthcare"},
        {"ALGN", "Healthcare"},
        {"HCA", "Healthcare"},
        {"HUM", "Healthcare"},
        {"CI", "Healthcare"},
        {"CVS", "Healthcare"},
        {"MCK", "Healthcare"},
        {"ABC", "Healthcare"},
        {"CAH", "Healthcare"},
        {"BIIB", "Healthcare"},
        {"REGN", "Healthcare"},
        {"VRTX", "Healthcare"},
        {"ILMN", "Healthcare"},
        {"MRNA", "Healthcare"},
        {"BNTX", "Healthcare"},
        {"IQV", "Healthcare"},
        {"A", "Healthcare"},
        {"WAT", "Healthcare"},
        // Financial Services
        {"JPM", "Financial Services"},
        {"BAC", "Financial Services"},
        {"WFC", "Financial Services"},
        {"GS", "Financial Services"},
        {"MS", "Financial Services"},
        {"C", "Financial Services"},
        {"AXP", "Financial Services"},
        {"BLK", "Financial Services"},
        {"SCHW", "Financial Services"},
        {"BRK-B", "Financial Services"},
        {"BRK-A", "Financial Services"},
        {"V", "Financial Services"},
        {"MA", "Financial Services"},
        {"PYPL", "Financial Services"},
        {"SQ", "Financial Services"},
        {"FIS", "Financial Services"},
        {"FI", "Financial Services"},
        {"GPN", "Financial Services"},
        {"ALLY", "Financial Services"},
        {"COF", "Financial Services"},
        {"DFS", "Financial Services"},
        {"USB", "Financial Services"},
        {"PNC", "Financial Services"},
        {"TFC", "Financial Services"},
        {"MTB", "Financial Services"},
        {"RF", "Financial Services"},
        {"KEY", "Financial Services"},
        {"CFG", "Financial Services"},
        {"FITB", "Financial Services"},
        {"HBAN", "Financial Services"},
        {"STT", "Financial Services"},
        {"BK", "Financial Services"},
        {"ICE", "Financial Services"},
        {"CME", "Financial Services"},
        {"CBOE", "Financial Services"},
        {"NDAQ", "Financial Services"},
        {"MCO", "Financial Services"},
        {"SPGI", "Financial Services"},
        {"MMC", "Financial Services"},
        {"AJG", "Financial Services"},
        {"AON", "Financial Services"},
        {"PRU", "Financial Services"},
        {"MET", "Financial Services"},
        {"AFL", "Financial Services"},
        {"ALL", "Financial Services"},
        {"PGR", "Financial Services"},
        {"CB", "Financial Services"},
        {"TRV", "Financial Services"},
        {"HIG", "Financial Services"},
        {"AIG", "Financial Services"},
        // Energy
        {"XOM", "Energy"},
        {"CVX", "Energy"},
        {"COP", "Energy"},
        {"EOG", "Energy"},
        {"SLB", "Energy"},
        {"PXD", "Energy"},
        {"OXY", "Energy"},
        {"MPC", "Energy"},
        {"PSX", "Energy"},
        {"VLO", "Energy"},
        {"WMB", "Energy"},
        {"KMI", "Energy"},
        {"ET", "Energy"},
        {"EPD", "Energy"},
        {"BKR", "Energy"},
        {"HAL", "Energy"},
        {"DVN", "Energy"},
        {"FANG", "Energy"},
        {"HES", "Energy"},
        {"APA", "Energy"},
        {"MRO", "Energy"},
        {"NOG", "Energy"},
        // Utilities
        {"NEE", "Utilities"},
        {"DUK", "Utilities"},
        {"SO", "Utilities"},
        {"D", "Utilities"},
        {"AEP", "Utilities"},
        {"EXC", "Utilities"},
        {"XEL", "Utilities"},
        {"ES", "Utilities"},
        {"WEC", "Utilities"},
        {"AWK", "Utilities"},
        {"ED", "Utilities"},
        {"PPL", "Utilities"},
        {"FE", "Utilities"},
        {"EIX", "Utilities"},
        {"PEG", "Utilities"},
        {"ETR", "Utilities"},
        {"CMS", "Utilities"},
        {"ATO", "Utilities"},
        {"NI", "Utilities"},
        {"OGE", "Utilities"},
        // Industrials
        {"GE", "Industrials"},
        {"HON", "Industrials"},
        {"UPS", "Industrials"},
        {"BA", "Industrials"},
        {"CAT", "Industrials"},
        {"DE", "Industrials"},
        {"LMT", "Industrials"},
        {"RTX", "Industrials"},
        {"NOC", "Industrials"},
        {"GD", "Industrials"},
        {"LHX", "Industrials"},
        {"HII", "Industrials"},
        {"MMM", "Industrials"},
        {"EMR", "Industrials"},
        {"ETN", "Industrials"},
        {"PH", "Industrials"},
        {"ROK", "Industrials"},
        {"DOV", "Industrials"},
        {"AME", "Industrials"},
        {"IEX", "Industrials"},
        {"FDX", "Industrials"},
        {"CSX", "Industrials"},
        {"UNP", "Industrials"},
        {"NSC", "Industrials"},
        {"DAL", "Industrials"},
        {"WM", "Industrials"},
        {"RSG", "Industrials"},
        {"GWW", "Industrials"},
        {"FAST", "Industrials"},
        {"GNRC", "Industrials"},
        {"IR", "Industrials"},
        {"XYL", "Industrials"},
        // Real Estate
        {"PLD", "Real Estate"},
        {"AMT", "Real Estate"},
        {"CCI", "Real Estate"},
        {"EQIX", "Real Estate"},
        {"PSA", "Real Estate"},
        {"SPG", "Real Estate"},
        {"O", "Real Estate"},
        {"WELL", "Real Estate"},
        {"DLR", "Real Estate"},
        {"VTR", "Real Estate"},
        {"AVB", "Real Estate"},
        {"EQR", "Real Estate"},
        {"MAA", "Real Estate"},
        {"UDR", "Real Estate"},
        {"ESS", "Real Estate"},
        {"CPT", "Real Estate"},
        {"NNN", "Real Estate"},
        {"STAG", "Real Estate"},
        {"ARE", "Real Estate"},
        {"BXP", "Real Estate"},
        // Basic Materials
        {"LIN", "Basic Materials"},
        {"APD", "Basic Materials"},
        {"SHW", "Basic Materials"},
        {"ECL", "Basic Materials"},
        {"DD", "Basic Materials"},
        {"DOW", "Basic Materials"},
        {"LYB", "Basic Materials"},
        {"CF", "Basic Materials"},
        {"MOS", "Basic Materials"},
        {"NEM", "Basic Materials"},
        {"FCX", "Basic Materials"},
        {"AA", "Basic Materials"},
        {"NUE", "Basic Materials"},
        {"STLD", "Basic Materials"},
        {"CLF", "Basic Materials"},
        {"X", "Basic Materials"},
        {"RS", "Basic Materials"},
        // ETFs / Index funds
        {"SPY", "ETF/Index"},
        {"QQQ", "ETF/Index"},
        {"IWM", "ETF/Index"},
        {"DIA", "ETF/Index"},
        {"VTI", "ETF/Index"},
        {"VOO", "ETF/Index"},
        {"VEA", "ETF/Index"},
        {"VWO", "ETF/Index"},
        {"GLD", "ETF/Index"},
        {"SLV", "ETF/Index"},
        {"IAU", "ETF/Index"},
        {"USO", "ETF/Index"},
        {"TLT", "ETF/Index"},
        {"IEF", "ETF/Index"},
        {"SHY", "ETF/Index"},
        {"AGG", "ETF/Index"},
        {"BND", "ETF/Index"},
        {"HYG", "ETF/Index"},
        {"LQD", "ETF/Index"},
        {"EMB", "ETF/Index"},
        {"XLK", "ETF/Index"},
        {"XLF", "ETF/Index"},
        {"XLV", "ETF/Index"},
        {"XLE", "ETF/Index"},
        {"XLI", "ETF/Index"},
        {"XLY", "ETF/Index"},
        {"XLP", "ETF/Index"},
        {"XLU", "ETF/Index"},
        {"XLB", "ETF/Index"},
        {"XLRE", "ETF/Index"},
        {"XLC", "ETF/Index"},
        {"ARKK", "ETF/Index"},
        {"ARKW", "ETF/Index"},
        {"ARKG", "ETF/Index"},
        // Crypto
        {"BTC-USD", "Cryptocurrency"},
        {"ETH-USD", "Cryptocurrency"},
        {"BNB-USD", "Cryptocurrency"},
        {"SOL-USD", "Cryptocurrency"},
        {"XRP-USD", "Cryptocurrency"},
        {"ADA-USD", "Cryptocurrency"},
        {"AVAX-USD", "Cryptocurrency"},
        {"DOGE-USD", "Cryptocurrency"},
        {"DOT-USD", "Cryptocurrency"},
        {"MATIC-USD", "Cryptocurrency"},
        {"LINK-USD", "Cryptocurrency"},
        {"LTC-USD", "Cryptocurrency"},
        {"UNI-USD", "Cryptocurrency"},
        {"ATOM-USD", "Cryptocurrency"},
        {"NEAR-USD", "Cryptocurrency"},
        {"ALGO-USD", "Cryptocurrency"},
        {"XLM-USD", "Cryptocurrency"},
        {"VET-USD", "Cryptocurrency"},
        // Indian markets (NSE/BSE)
        {"RELIANCE.NS", "Energy"},
        {"TCS.NS", "Technology"},
        {"INFY.NS", "Technology"},
        {"HDFCBANK.NS", "Financial Services"},
        {"ICICIBANK.NS", "Financial Services"},
        {"SBIN.NS", "Financial Services"},
        {"AXISBANK.NS", "Financial Services"},
        {"KOTAKBANK.NS", "Financial Services"},
        {"BAJFINANCE.NS", "Financial Services"},
        {"WIPRO.NS", "Technology"},
        {"HCLTECH.NS", "Technology"},
        {"TECHM.NS", "Technology"},
        {"LTIM.NS", "Technology"},
        {"PERSISTENT.NS", "Technology"},
        {"SUNPHARMA.NS", "Healthcare"},
        {"CIPLA.NS", "Healthcare"},
        {"DRREDDY.NS", "Healthcare"},
        {"DIVISLAB.NS", "Healthcare"},
        {"APOLLOHOSP.NS", "Healthcare"},
        {"HINDUNILVR.NS", "Consumer Defensive"},
        {"ITC.NS", "Consumer Defensive"},
        {"NESTLEIND.NS", "Consumer Defensive"},
        {"BRITANNIA.NS", "Consumer Defensive"},
        {"TITAN.NS", "Consumer Cyclical"},
        {"MARUTI.NS", "Consumer Cyclical"},
        {"TATAMOTORS.NS", "Consumer Cyclical"},
        {"M&M.NS", "Consumer Cyclical"},
        {"BAJAJ-AUTO.NS", "Consumer Cyclical"},
        {"HEROMOTOCO.NS", "Consumer Cyclical"},
        {"ADANIENT.NS", "Industrials"},
        {"ADANIPORTS.NS", "Industrials"},
        {"POWERGRID.NS", "Utilities"},
        {"NTPC.NS", "Utilities"},
        {"ONGC.NS", "Energy"},
        {"BPCL.NS", "Energy"},
        {"IOC.NS", "Energy"},
        {"COAL.NS", "Basic Materials"},
        {"JSWSTEEL.NS", "Basic Materials"},
        {"TATASTEEL.NS", "Basic Materials"},
        {"HINDALCO.NS", "Basic Materials"},
        {"GRASIM.NS", "Basic Materials"},
        {"ULTRACEMCO.NS", "Basic Materials"},
        {"ACC.NS", "Basic Materials"},
        {"LT.NS", "Industrials"},
        {"SIEMENS.NS", "Industrials"},
        {"ABB.NS", "Industrials"},
        {"BHARTIARTL.NS", "Communication"},
        {"IDEA.NS", "Communication"},
    };

    auto it = known.find(s);
    if (it != known.end())
        return *it;

    // ── Suffix / pattern inference ────────────────────────────────────────────
    // Crypto: ends with -USD, -USDT, -BTC, -ETH
    if (s.endsWith("-USD") || s.endsWith("-USDT") || s.endsWith("-BTC") || s.endsWith("-ETH"))
        return "Cryptocurrency";

    // Indian market suffix
    if (s.endsWith(".NS") || s.endsWith(".BO"))
        return "Other (India)";

    // London
    if (s.endsWith(".L"))
        return "Other (UK)";

    // European
    if (s.endsWith(".DE") || s.endsWith(".PA") || s.endsWith(".MI") || s.endsWith(".AS"))
        return "Other (Europe)";

    // Bond ETF pattern
    if (s.startsWith("TLT") || s.startsWith("AGG") || s.startsWith("BND") || s.contains("BOND") || s.contains("TREAS"))
        return "Fixed Income";

    // Commodity pattern
    if (s.contains("OIL") || s.contains("GOLD") || s.contains("SILV") || s.contains("CORN") || s.contains("WHEAT"))
        return "Commodities";

    return "Other";
}

PortfolioSectorPanel::PortfolioSectorPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioSectorPanel::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Top: Sector Allocation (60%) ──────────────────────────────────────────
    auto* sector_widget = new QWidget(this);
    auto* sector_layout = new QVBoxLayout(sector_widget);
    sector_layout->setContentsMargins(8, 4, 8, 4);
    sector_layout->setSpacing(4);

    auto* sector_header = new QLabel("SECTOR ALLOCATION");
    sector_header->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
    sector_layout->addWidget(sector_header);

    auto* donut_row = new QHBoxLayout;

    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    donut_view_ = new QChartView(chart);
    donut_view_->setRenderHint(QPainter::Antialiasing);
    donut_view_->setFixedSize(130, 130);
    donut_view_->setStyleSheet("border:none; background:transparent;");
    donut_row->addWidget(donut_view_);

    legend_widget_ = new QWidget(this);
    legend_widget_->setStyleSheet("background:transparent;");
    donut_row->addWidget(legend_widget_, 1);

    sector_layout->addLayout(donut_row, 1);
    layout->addWidget(sector_widget, 6);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    layout->addWidget(sep);

    // ── Bottom: Correlation (40%) ─────────────────────────────────────────────
    auto* corr_container = new QWidget(this);
    auto* corr_layout = new QVBoxLayout(corr_container);
    corr_layout->setContentsMargins(8, 4, 8, 4);
    corr_layout->setSpacing(4);

    auto* corr_header = new QHBoxLayout;
    auto* corr_title = new QLabel("CORRELATION MATRIX");
    corr_title->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
    corr_header->addWidget(corr_title);

    auto* corr_note = new QLabel("(P&L return proxy)");
    corr_note->setStyleSheet(QString("color:%1; font-size:8px;").arg(ui::colors::TEXT_TERTIARY()));
    corr_header->addWidget(corr_note);
    corr_header->addStretch();
    corr_layout->addLayout(corr_header);

    corr_widget_ = new QWidget(this);
    corr_widget_->setStyleSheet("background:transparent;");
    corr_layout->addWidget(corr_widget_, 1);

    layout->addWidget(corr_container, 4);
}

void PortfolioSectorPanel::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    selected_sector_.clear(); // reset filter on fresh data
    corr_matrix_.clear();     // invalidate stale correlation data
    update_donut();
    update_correlation();
}

void PortfolioSectorPanel::set_correlation(const QHash<QString, double>& matrix) {
    corr_matrix_ = matrix;
    update_correlation();
}

void PortfolioSectorPanel::on_sector_legend_clicked(const QString& sector) {
    if (selected_sector_ == sector)
        selected_sector_.clear(); // toggle off
    else
        selected_sector_ = sector;
    emit sector_selected(selected_sector_);
    update_donut(); // refresh to show highlight
}

void PortfolioSectorPanel::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioSectorPanel::update_donut() {
    auto* chart = donut_view_->chart();
    chart->removeAllSeries();

    if (holdings_.isEmpty())
        return;

    // Group by inferred sector using actual portfolio weights
    QHash<QString, double> sector_weights;
    QHash<QString, double> sector_pnl;
    QHash<QString, int> sector_counts;

    for (const auto& h : holdings_) {
        QString sector = infer_sector(h.symbol);
        sector_weights[sector] += h.weight;
        sector_pnl[sector] += h.unrealized_pnl;
        sector_counts[sector]++;
    }

    auto* series = new QPieSeries;
    series->setHoleSize(0.55);

    QVector<QPair<QString, double>> sorted;
    for (auto it = sector_weights.begin(); it != sector_weights.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    for (int i = 0; i < sorted.size(); ++i) {
        const QString& sec = sorted[i].first;
        auto* slice = series->append(sec, sorted[i].second);
        slice->setColor(sector_color(i));
        const bool active = (sec == selected_sector_);
        slice->setBorderColor(active ? QColor(ui::colors::AMBER()) : QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(active ? 2 : 1);
        if (active)
            slice->setExploded(true);
    }

    // Wire slice clicks
    connect(series, &QPieSeries::clicked, this, [this](QPieSlice* slice) { on_sector_legend_clicked(slice->label()); });

    chart->addSeries(series);

    // Rebuild legend
    if (auto* old = legend_widget_->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    auto* legend_layout = new QVBoxLayout(legend_widget_);
    legend_layout->setContentsMargins(4, 0, 0, 0);
    legend_layout->setSpacing(2);

    for (int i = 0; i < sorted.size(); ++i) {
        const QString& sec = sorted[i].first;
        const bool active = (sec == selected_sector_);

        // Wrap the row in a clickable QWidget for hit-testing
        auto* row_widget = new QWidget(this);
        row_widget->setCursor(Qt::PointingHandCursor);
        row_widget->setStyleSheet(active ? QString("background:%1; border-radius:2px;").arg(ui::colors::BG_HOVER())
                                         : "background:transparent;");

        auto* row = new QHBoxLayout(row_widget);
        row->setContentsMargins(2, 1, 2, 1);
        row->setSpacing(4);

        auto* swatch = new QWidget(this);
        swatch->setFixedSize(8, 8);
        swatch->setStyleSheet(QString("background:%1; border-radius:1px;").arg(sector_color(i).name()));
        row->addWidget(swatch);

        auto* name = new QLabel(QString("%1 (%2)").arg(sec).arg(sector_counts[sec]));
        name->setStyleSheet(active ? QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::AMBER())
                                   : QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_PRIMARY()));
        row->addWidget(name);
        row->addStretch();

        double pnl = sector_pnl[sec];
        auto* pnl_lbl = new QLabel(QString("%1%2").arg(pnl >= 0 ? "+" : "").arg(QString::number(pnl, 'f', 0)));
        const char* pc = pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        pnl_lbl->setStyleSheet(QString("color:%1; font-size:8px;").arg(pc));
        row->addWidget(pnl_lbl);

        auto* weight = new QLabel(QString(" %1%").arg(QString::number(sorted[i].second, 'f', 1)));
        weight->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::TEXT_SECONDARY()));
        row->addWidget(weight);

        // Install click via event filter on row_widget
        row_widget->installEventFilter(this);
        row_widget->setProperty("sector_name", sec);

        legend_layout->addWidget(row_widget);
    }
    legend_layout->addStretch();
}

void PortfolioSectorPanel::update_correlation() {
    if (auto* old = corr_widget_->layout()) {
        QLayoutItem* item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    if (holdings_.size() < 2) {
        auto* layout = new QVBoxLayout(corr_widget_);
        auto* msg = new QLabel("Need 2+ holdings for correlation");
        msg->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
        msg->setAlignment(Qt::AlignCenter);
        layout->addWidget(msg);
        return;
    }

    // Top 6 by weight
    auto sorted = holdings_;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });
    int n = static_cast<int>(std::min(qsizetype{6}, sorted.size()));

    // Use real Pearson matrix if available; otherwise show "--" pending fetch.
    const bool has_real_corr = !corr_matrix_.isEmpty();

    auto corr_pair = [&](int i, int j) -> std::optional<double> {
        if (i == j)
            return 1.0;
        if (has_real_corr) {
            const QString key_ab = sorted[i].symbol + "|" + sorted[j].symbol;
            const QString key_ba = sorted[j].symbol + "|" + sorted[i].symbol;
            if (corr_matrix_.contains(key_ab))
                return corr_matrix_[key_ab];
            if (corr_matrix_.contains(key_ba))
                return corr_matrix_[key_ba];
            return std::nullopt; // symbol not in matrix (fetch may be partial)
        }
        // No real data yet — return nullopt so cell shows "…"
        return std::nullopt;
    };

    auto* grid = new QGridLayout(corr_widget_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);

    // Header row
    grid->addWidget(new QLabel, 0, 0);
    for (int i = 0; i < n; ++i) {
        auto* lbl = new QLabel(sorted[i].symbol.left(4));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(lbl, 0, i + 1);
    }

    // Matrix
    for (int r = 0; r < n; ++r) {
        auto* row_label = new QLabel(sorted[r].symbol.left(4));
        row_label->setStyleSheet(QString("color:%1; font-size:7px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        grid->addWidget(row_label, r + 1, 0);

        for (int c = 0; c < n; ++c) {
            const auto opt_corr = corr_pair(r, c);
            const bool is_diag = (r == c);
            const double corr = opt_corr.value_or(0.0);
            const QString label = is_diag ? "1.00" : opt_corr ? QString::number(corr, 'f', 2) : "\u2026"; // "…" pending

            auto* cell = new QLabel(label);
            cell->setAlignment(Qt::AlignCenter);
            cell->setFixedSize(32, 18);

            QColor bg;
            if (is_diag) {
                bg = QColor(ui::colors::TEXT_PRIMARY());
            } else if (!opt_corr) {
                bg = QColor(ui::colors::BG_RAISED());
            } else if (corr > 0) {
                bg = QColor(22, 163, 74, static_cast<int>(std::abs(corr) * 140 + 20));
            } else {
                bg = QColor(220, 38, 38, static_cast<int>(std::abs(corr) * 140 + 20));
            }

            const char* text_color = is_diag ? ui::colors::BG_BASE : ui::colors::TEXT_PRIMARY;
            cell->setStyleSheet(QString("background:rgba(%1,%2,%3,%4); color:%5;"
                                        "font-size:7px; font-weight:600;")
                                    .arg(bg.red())
                                    .arg(bg.green())
                                    .arg(bg.blue())
                                    .arg(bg.alpha())
                                    .arg(text_color));

            grid->addWidget(cell, r + 1, c + 1);
        }
    }
}

bool PortfolioSectorPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        const QString sec = obj->property("sector_name").toString();
        if (!sec.isEmpty()) {
            on_sector_legend_clicked(sec);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace fincept::screens
