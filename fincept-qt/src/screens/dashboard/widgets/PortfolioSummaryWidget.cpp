#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"

#include "storage/repositories/PortfolioHoldingsRepository.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QScrollArea>

namespace fincept::screens::widgets {

// Demo holdings if no DB portfolio exists
static const QVector<PortfolioSummaryWidget::Holding> kDemoHoldings = {
    {"AAPL", 10.0, 178.50}, {"MSFT", 5.0, 415.00}, {"NVDA", 8.0, 880.00},
    {"GOOGL", 3.0, 165.00}, {"TSLA", 6.0, 210.00}, {"SPY", 12.0, 520.00},
};

PortfolioSummaryWidget::PortfolioSummaryWidget(QWidget* parent)
    : BaseWidget("PORTFOLIO SUMMARY", parent, ui::colors::POSITIVE) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // ── Summary card ──
    auto* summary = new QWidget;
    summary->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED));
    auto* sl = new QGridLayout(summary);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setHorizontalSpacing(16);
    sl->setVerticalSpacing(4);

    auto make_metric = [&](const QString& label, QLabel*& value_out, int row, int col) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY));
        sl->addWidget(lbl, row * 2, col);

        value_out = new QLabel("--");
        value_out->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY));
        sl->addWidget(value_out, row * 2 + 1, col);
    };

    make_metric("TOTAL VALUE", total_value_lbl_, 0, 0);
    make_metric("DAY P&L", day_pnl_lbl_, 0, 1);
    make_metric("TOTAL P&L", total_pnl_lbl_, 1, 0);
    make_metric("HOLDINGS", num_holdings_lbl_, 1, 1);

    vl->addWidget(summary);

    // ── Holdings list header ──
    auto* hdr = new QWidget;
    hdr->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr_lbl = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                             .arg(ui::colors::TEXT_TERTIARY));
        l->setAlignment(a);
        hl->addWidget(l, s);
    };
    make_hdr_lbl("SYM", 1);
    make_hdr_lbl("SHARES", 1, Qt::AlignRight);
    make_hdr_lbl("PRICE", 1, Qt::AlignRight);
    make_hdr_lbl("VALUE", 1, Qt::AlignRight);
    make_hdr_lbl("P&L", 1, Qt::AlignRight);
    vl->addWidget(hdr);

    // Scrollable holdings list
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { width: 4px; background: transparent; }"
                          + QString("QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }").arg(ui::colors::BORDER_MED) +
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* list_widget = new QWidget;
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    list_layout_->addStretch();

    scroll->setWidget(list_widget);
    vl->addWidget(scroll, 1);

    connect(this, &BaseWidget::refresh_requested, this, [this] { load_holdings(); });
    set_loading(true);
    load_holdings();
}

void PortfolioSummaryWidget::load_holdings() {
    QVector<Holding> holdings;

    // Read from portfolio_holdings via repository
    auto result = fincept::PortfolioHoldingsRepository::instance().get_active();
    if (result.is_ok()) {
        for (const auto& ph : result.value()) {
            Holding h;
            h.symbol = ph.symbol;
            h.shares = ph.shares;
            h.avg_cost = ph.avg_cost;
            if (!h.symbol.isEmpty() && h.shares > 0)
                holdings.append(h);
        }
    }

    // Fall back to demo portfolio
    if (holdings.isEmpty()) {
        holdings = kDemoHoldings;
    }

    fetch_prices(holdings);
}

void PortfolioSummaryWidget::fetch_prices(const QVector<Holding>& holdings) {
    QStringList symbols;
    for (const auto& h : holdings)
        symbols << h.symbol;

    services::MarketDataService::instance().fetch_quotes(
        symbols, [this, holdings](bool ok, QVector<services::QuoteData> quotes) {
            set_loading(false);
            if (!ok || quotes.isEmpty())
                return;
            render(holdings, quotes);
        });
}

void PortfolioSummaryWidget::render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes) {
    QMap<QString, const services::QuoteData*> qmap;
    for (const auto& q : quotes)
        qmap[q.symbol] = &q;

    double total_value = 0;
    double total_cost = 0;
    double day_pnl = 0;

    // Clear list
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    bool alt = false;
    for (const auto& h : holdings) {
        const services::QuoteData* q = qmap.value(h.symbol, nullptr);
        double price = q ? q->price : 0;
        double value = price * h.shares;
        double cost = h.avg_cost * h.shares;
        double pnl = value - cost;
        double day_chg = q ? (q->change * h.shares) : 0;

        total_value += value;
        total_cost += cost;
        day_pnl += day_chg;

        // Row
        auto* row = new QWidget;
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        auto cell = [&](const QString& text, int stretch, Qt::Alignment align, const QString& color) {
            auto* lbl = new QLabel(text);
            lbl->setAlignment(align);
            lbl->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(color));
            rl->addWidget(lbl, stretch);
        };

        cell(h.symbol, 1, Qt::AlignLeft, ui::colors::TEXT_PRIMARY);
        cell(QString::number(h.shares, 'f', h.shares == (int)h.shares ? 0 : 2), 1, Qt::AlignRight,
             ui::colors::TEXT_SECONDARY);
        cell(price > 0 ? QString("$%1").arg(price, 0, 'f', 2) : "--", 1, Qt::AlignRight, ui::colors::TEXT_PRIMARY);
        cell(value > 0 ? QString("$%1").arg(value, 0, 'f', 0) : "--", 1, Qt::AlignRight, ui::colors::TEXT_PRIMARY);

        QString pnl_str = pnl >= 0 ? QString("+$%1").arg(pnl, 0, 'f', 0) : QString("-$%1").arg(-pnl, 0, 'f', 0);
        cell(pnl_str, 1, Qt::AlignRight, pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

        list_layout_->addWidget(row);
        alt = !alt;
    }
    list_layout_->addStretch();

    // Update summary labels
    total_value_lbl_->setText(QString("$%1").arg(total_value, 0, 'f', 0));
    num_holdings_lbl_->setText(QString::number(holdings.size()));

    double total_pnl = total_value - total_cost;
    QString day_str = day_pnl >= 0 ? QString("+$%1").arg(day_pnl, 0, 'f', 0) : QString("-$%1").arg(-day_pnl, 0, 'f', 0);
    QString tot_str =
        total_pnl >= 0 ? QString("+$%1").arg(total_pnl, 0, 'f', 0) : QString("-$%1").arg(-total_pnl, 0, 'f', 0);

    day_pnl_lbl_->setText(day_str);
    day_pnl_lbl_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                    .arg(day_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE));

    total_pnl_lbl_->setText(tot_str);
    total_pnl_lbl_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                      .arg(total_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
}

} // namespace fincept::screens::widgets
