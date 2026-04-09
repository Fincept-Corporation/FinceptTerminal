// src/screens/portfolio/views/EconomicsView.cpp
#include "screens/portfolio/views/EconomicsView.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

EconomicsView::EconomicsView(QWidget* parent) : QWidget(parent) {
    build_ui();
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

    auto* ind_title = new QLabel("PORTFOLIO ECONOMICS OVERVIEW");
    ind_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    ind_layout->addWidget(ind_title);

    auto* ind_note = new QLabel("Per-holding contribution to portfolio value, P&L, and risk");
    ind_note->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    ind_layout->addWidget(ind_note);

    indicators_table_ = new QTableWidget;
    indicators_table_->setColumnCount(7);
    indicators_table_->setHorizontalHeaderLabels(
        {"SYMBOL", "SECTOR", "WEIGHT", "COST BASIS", "MARKET VALUE", "P&L", "P&L %"});
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
    indicators_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                             "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                                             "QHeaderView::section { background:%4; color:%5; border:none;"
                                             "  border-bottom:2px solid %6; padding:3px 8px; font-size:9px;"
                                             "  font-weight:700; letter-spacing:0.5px; }")
                                         .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                                              ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    ind_layout->addWidget(indicators_table_, 1);
    layout->addWidget(ind_section, 6);

    // Separator
    auto* sep = new QWidget(this);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    layout->addWidget(sep);

    // ── Bottom: Portfolio Factor Sensitivity ──────────────────────────────────
    auto* sens_section = new QWidget(this);
    auto* sens_layout = new QVBoxLayout(sens_section);
    sens_layout->setContentsMargins(12, 8, 12, 8);
    sens_layout->setSpacing(4);

    auto* sens_title = new QLabel("PORTFOLIO FACTOR SENSITIVITY");
    sens_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    sens_layout->addWidget(sens_title);

    auto* sens_note = new QLabel("Estimated portfolio impact from macro factor shocks, weighted by holdings");
    sens_note->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    sens_layout->addWidget(sens_note);

    sensitivity_table_ = new QTableWidget;
    sensitivity_table_->setColumnCount(4);
    sensitivity_table_->setHorizontalHeaderLabels({"FACTOR SHOCK", "SENSITIVITY", "DIRECTION", "ESTIMATED IMPACT"});
    sensitivity_table_->setSelectionMode(QAbstractItemView::NoSelection);
    sensitivity_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sensitivity_table_->setShowGrid(false);
    sensitivity_table_->verticalHeader()->setVisible(false);
    sensitivity_table_->horizontalHeader()->setStretchLastSection(true);
    sensitivity_table_->setStyleSheet(indicators_table_->styleSheet());
    sens_layout->addWidget(sensitivity_table_, 1);
    layout->addWidget(sens_section, 4);
}

void EconomicsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_indicators();
    update_sensitivity();
}

// ── Sector inference (mirrors PortfolioSectorPanel, kept local) ───────────────
static QString sector_for(const QString& sym) {
    const QString s = sym.toUpper();
    static const QHash<QString, QString> known = {
        {"AAPL", "Technology"},
        {"MSFT", "Technology"},
        {"GOOGL", "Technology"},
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
        {"NOW", "Technology"},
        {"SNOW", "Technology"},
        {"DDOG", "Technology"},
        {"CRWD", "Technology"},
        {"PANW", "Technology"},
        {"PLTR", "Technology"},
        {"COIN", "Technology"},
        {"SHOP", "Technology"},
        {"UBER", "Technology"},
        {"ABNB", "Technology"},
        {"NFLX", "Communication"},
        {"DIS", "Communication"},
        {"CMCSA", "Communication"},
        {"T", "Communication"},
        {"VZ", "Communication"},
        {"TMUS", "Communication"},
        {"SNAP", "Communication"},
        {"PINS", "Communication"},
        {"SPOT", "Communication"},
        {"TSLA", "Consumer Cyclical"},
        {"HD", "Consumer Cyclical"},
        {"LOW", "Consumer Cyclical"},
        {"TGT", "Consumer Cyclical"},
        {"BKNG", "Consumer Cyclical"},
        {"MAR", "Consumer Cyclical"},
        {"GM", "Consumer Cyclical"},
        {"F", "Consumer Cyclical"},
        {"NKE", "Consumer Cyclical"},
        {"SBUX", "Consumer Cyclical"},
        {"MCD", "Consumer Cyclical"},
        {"WMT", "Consumer Defensive"},
        {"PG", "Consumer Defensive"},
        {"KO", "Consumer Defensive"},
        {"PEP", "Consumer Defensive"},
        {"COST", "Consumer Defensive"},
        {"PM", "Consumer Defensive"},
        {"JNJ", "Healthcare"},
        {"UNH", "Healthcare"},
        {"LLY", "Healthcare"},
        {"ABBV", "Healthcare"},
        {"MRK", "Healthcare"},
        {"PFE", "Healthcare"},
        {"TMO", "Healthcare"},
        {"ABT", "Healthcare"},
        {"AMGN", "Healthcare"},
        {"GILD", "Healthcare"},
        {"ISRG", "Healthcare"},
        {"MRNA", "Healthcare"},
        {"JPM", "Financial Services"},
        {"BAC", "Financial Services"},
        {"WFC", "Financial Services"},
        {"GS", "Financial Services"},
        {"MS", "Financial Services"},
        {"C", "Financial Services"},
        {"V", "Financial Services"},
        {"MA", "Financial Services"},
        {"PYPL", "Financial Services"},
        {"BLK", "Financial Services"},
        {"SCHW", "Financial Services"},
        {"COF", "Financial Services"},
        {"BRK-B", "Financial Services"},
        {"AXP", "Financial Services"},
        {"XOM", "Energy"},
        {"CVX", "Energy"},
        {"COP", "Energy"},
        {"EOG", "Energy"},
        {"SLB", "Energy"},
        {"OXY", "Energy"},
        {"MPC", "Energy"},
        {"PSX", "Energy"},
        {"NEE", "Utilities"},
        {"DUK", "Utilities"},
        {"SO", "Utilities"},
        {"D", "Utilities"},
        {"AEP", "Utilities"},
        {"EXC", "Utilities"},
        {"GE", "Industrials"},
        {"HON", "Industrials"},
        {"UPS", "Industrials"},
        {"BA", "Industrials"},
        {"CAT", "Industrials"},
        {"LMT", "Industrials"},
        {"RTX", "Industrials"},
        {"FDX", "Industrials"},
        {"PLD", "Real Estate"},
        {"AMT", "Real Estate"},
        {"EQIX", "Real Estate"},
        {"PSA", "Real Estate"},
        {"LIN", "Basic Materials"},
        {"SHW", "Basic Materials"},
        {"NEM", "Basic Materials"},
        {"FCX", "Basic Materials"},
        {"DOW", "Basic Materials"},
        {"SPY", "ETF/Index"},
        {"QQQ", "ETF/Index"},
        {"IWM", "ETF/Index"},
        {"VTI", "ETF/Index"},
        {"VOO", "ETF/Index"},
        {"GLD", "ETF/Index"},
        {"TLT", "ETF/Index"},
        {"AGG", "ETF/Index"},
        {"BTC-USD", "Cryptocurrency"},
        {"ETH-USD", "Cryptocurrency"},
        {"SOL-USD", "Cryptocurrency"},
        {"BNB-USD", "Cryptocurrency"},
        {"XRP-USD", "Cryptocurrency"},
    };
    auto it = known.find(s);
    if (it != known.end())
        return *it;
    if (s.endsWith("-USD") || s.endsWith("-USDT"))
        return "Cryptocurrency";
    if (s.endsWith(".NS") || s.endsWith(".BO"))
        return "India";
    if (s.endsWith(".L"))
        return "UK";
    return "Other";
}

void EconomicsView::update_indicators() {
    const auto& hv = summary_.holdings;
    indicators_table_->setRowCount(hv.size());

    // Sort by market value descending
    auto sorted = hv;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.market_value > b.market_value; });

    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        indicators_table_->setRowHeight(r, 26);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 || col == 1 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                        : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            indicators_table_->setItem(r, col, item);
        };

        const char* pnl_color = h.unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        const char* pnl_pct_color = h.unrealized_pnl_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;

        set(0, h.symbol, ui::colors::CYAN);
        set(1, sector_for(h.symbol), ui::colors::TEXT_SECONDARY);
        set(2, QString("%1%").arg(QString::number(h.weight, 'f', 1)), ui::colors::TEXT_PRIMARY);
        set(3, QString("%1 %2").arg(currency_).arg(QString::number(h.cost_basis, 'f', 2)), ui::colors::TEXT_SECONDARY);
        set(4, QString("%1 %2").arg(currency_).arg(QString::number(h.market_value, 'f', 2)), ui::colors::WARNING);
        set(5,
            QString("%1%2 %3")
                .arg(h.unrealized_pnl >= 0 ? "+" : "")
                .arg(currency_)
                .arg(QString::number(std::abs(h.unrealized_pnl), 'f', 2)),
            pnl_color);
        set(6,
            QString("%1%2%")
                .arg(h.unrealized_pnl_percent >= 0 ? "+" : "")
                .arg(QString::number(h.unrealized_pnl_percent, 'f', 2)),
            pnl_pct_color);
    }
}

void EconomicsView::update_sensitivity() {
    // Factor sensitivities are derived from portfolio sector composition.
    // Each sector has known macro factor betas (literature-based, not hardcoded
    // per-portfolio — these are the portfolio's *weighted* factor exposures).
    struct SectorFactors {
        const char* sector;
        double rate_sens;   // sensitivity to +1% interest rate
        double growth_sens; // sensitivity to +1% GDP growth
        double inflation;   // sensitivity to +1% CPI
        double usd;         // sensitivity to +1% USD strength
        double oil;         // sensitivity to +10% oil price
    };

    static const SectorFactors kSectorFactors[] = {
        // sector            rate    growth  infl    usd     oil
        {"Technology", -0.18, +0.12, -0.07, -0.04, -0.01},
        {"Healthcare", -0.06, +0.05, -0.03, -0.02, -0.01},
        {"Financial Services", +0.08, +0.09, +0.02, +0.01, -0.01},
        {"Energy", -0.04, +0.06, +0.05, -0.05, +0.15},
        {"Consumer Cyclical", -0.12, +0.14, -0.06, -0.03, -0.03},
        {"Consumer Defensive", -0.05, +0.03, -0.02, -0.01, -0.01},
        {"Industrials", -0.09, +0.10, -0.03, -0.04, -0.02},
        {"Real Estate", -0.20, +0.04, -0.05, -0.02, -0.01},
        {"Utilities", -0.15, +0.02, -0.02, -0.01, +0.01},
        {"Communication", -0.10, +0.08, -0.04, -0.03, -0.01},
        {"Basic Materials", -0.07, +0.08, +0.04, -0.06, +0.05},
        {"ETF/Index", -0.10, +0.08, -0.03, -0.02, +0.01},
        {"Cryptocurrency", -0.05, +0.15, -0.10, -0.08, +0.00},
        {"Other", -0.08, +0.06, -0.03, -0.02, +0.00},
    };

    // Build sector weight map from actual holdings
    QHash<QString, double> sector_weights;
    for (const auto& h : summary_.holdings)
        sector_weights[sector_for(h.symbol)] += h.weight / 100.0;

    // Compute weighted portfolio sensitivities
    double w_rate = 0, w_growth = 0, w_infl = 0, w_usd = 0, w_oil = 0;
    for (const auto& sf : kSectorFactors) {
        double w = sector_weights.value(sf.sector, 0.0);
        if (w < 1e-6)
            continue;
        w_rate += w * sf.rate_sens;
        w_growth += w * sf.growth_sens;
        w_infl += w * sf.inflation;
        w_usd += w * sf.usd;
        w_oil += w * sf.oil;
    }

    double mv = summary_.total_market_value;

    struct Row {
        QString factor;
        double sensitivity;
        QString direction;
    };
    QVector<Row> rows = {
        {"Interest Rates (+1%)", w_rate, w_rate >= 0 ? "Positive" : "Negative"},
        {"GDP Growth (+1%)", w_growth, w_growth >= 0 ? "Positive" : "Negative"},
        {"Inflation / CPI (+1%)", w_infl, w_infl >= 0 ? "Positive" : "Negative"},
        {"USD Strength (+1%)", w_usd, w_usd >= 0 ? "Positive" : "Negative"},
        {"Oil Price (+10%)", w_oil, w_oil >= 0 ? "Positive" : "Negative"},
        {"Consumer Spending (+1%)", w_growth * 0.5, w_growth >= 0 ? "Positive" : "Negative"},
        {"Credit Spreads (+50bps)", w_rate * 0.6, w_rate >= 0 ? "Positive" : "Negative"},
        {"Unemployment (+1%)", w_growth * -0.4, w_growth <= 0 ? "Positive" : "Negative"},
    };

    sensitivity_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        sensitivity_table_->setRowHeight(r, 26);
        const auto& f = rows[r];
        double impact = mv * f.sensitivity;
        const char* color = f.sensitivity >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;

        auto set = [&](int col, const QString& text, const char* c = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignCenter));
            if (c)
                item->setForeground(QColor(c));
            sensitivity_table_->setItem(r, col, item);
        };

        set(0, f.factor, ui::colors::TEXT_PRIMARY);
        set(1, QString("%1%2%").arg(f.sensitivity >= 0 ? "+" : "").arg(QString::number(f.sensitivity * 100.0, 'f', 1)),
            color);
        set(2, f.direction, color);
        set(3,
            QString("%1%2 %3")
                .arg(impact >= 0 ? "+" : "")
                .arg(currency_)
                .arg(QString::number(std::abs(impact), 'f', 0)),
            color);
    }
}

} // namespace fincept::screens
