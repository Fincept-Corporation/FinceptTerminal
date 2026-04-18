#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"

#include "storage/repositories/PortfolioHoldingsRepository.h"
#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

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
    summary_card_ = new QWidget(this);
    auto* sl = new QGridLayout(summary_card_);
    sl->setContentsMargins(10, 8, 10, 8);
    sl->setHorizontalSpacing(16);
    sl->setVerticalSpacing(4);

    auto make_metric = [&](const QString& label, QLabel*& value_out, int row, int col) {
        auto* lbl = new QLabel(label);
        metric_labels_.append(lbl);
        sl->addWidget(lbl, row * 2, col);

        value_out = new QLabel("--");
        metric_values_.append(value_out);
        sl->addWidget(value_out, row * 2 + 1, col);
    };

    make_metric("TOTAL VALUE", total_value_lbl_, 0, 0);
    make_metric("DAY P&L", day_pnl_lbl_, 0, 1);
    make_metric("TOTAL P&L", total_pnl_lbl_, 1, 0);
    make_metric("HOLDINGS", num_holdings_lbl_, 1, 1);

    vl->addWidget(summary_card_);

    // ── Holdings list header ──
    header_row_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_row_);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr_lbl = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setAlignment(a);
        header_labels_.append(l);
        hl->addWidget(l, s);
    };
    make_hdr_lbl("SYM", 1);
    make_hdr_lbl("SHARES", 1, Qt::AlignRight);
    make_hdr_lbl("PRICE", 1, Qt::AlignRight);
    make_hdr_lbl("VALUE", 1, Qt::AlignRight);
    make_hdr_lbl("P&L", 1, Qt::AlignRight);
    vl->addWidget(header_row_);

    // Scrollable holdings list
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);

    auto* list_widget = new QWidget(this);
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    list_layout_->addStretch();

    scroll_area_->setWidget(list_widget);
    vl->addWidget(scroll_area_, 1);

    connect(this, &BaseWidget::refresh_requested, this, [this] { load_holdings(); });

    apply_styles();
    set_loading(true);

}

void PortfolioSummaryWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        load_holdings();
}

void PortfolioSummaryWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void PortfolioSummaryWidget::apply_styles() {
    summary_card_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : metric_labels_)
        lbl->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
    for (auto* val : metric_values_)
        val->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_PRIMARY()));
    header_row_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    for (auto* lbl : header_labels_)
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    scroll_area_->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 4px; background: transparent; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::BORDER_MED()));
}

void PortfolioSummaryWidget::on_theme_changed() {
    apply_styles();
    if (!last_holdings_.isEmpty() && !last_quotes_.isEmpty())
        render(last_holdings_, last_quotes_);
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
    last_holdings_ = holdings;
    hub_resubscribe(holdings);
}


void PortfolioSummaryWidget::hub_resubscribe(const QVector<Holding>& holdings) {
    auto& hub = datahub::DataHub::instance();
    // Holdings set may have changed since last subscribe — wipe all and
    // re-register so we don't leave stale topic subs behind.
    hub.unsubscribe(this);
    row_cache_.clear();
    for (const auto& h : holdings) {
        const QString sym = h.symbol;
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading(false);
            rebuild_from_cache();
        });
    }
    hub_active_ = true;
}

void PortfolioSummaryWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void PortfolioSummaryWidget::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(row_cache_.size());
    for (const auto& h : last_holdings_) {
        auto it = row_cache_.constFind(h.symbol);
        if (it != row_cache_.constEnd())
            quotes.append(it.value());
    }
    if (!quotes.isEmpty())
        render(last_holdings_, quotes);
}


void PortfolioSummaryWidget::render(const QVector<Holding>& holdings, const QVector<services::QuoteData>& quotes) {
    last_holdings_ = holdings;
    last_quotes_ = quotes;

    QMap<QString, const services::QuoteData*> qmap;
    for (const auto& q : last_quotes_)
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
        auto* row = new QWidget(this);
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
                                    .arg(day_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));

    total_pnl_lbl_->setText(tot_str);
    total_pnl_lbl_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                      .arg(total_pnl >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
}

} // namespace fincept::screens::widgets
