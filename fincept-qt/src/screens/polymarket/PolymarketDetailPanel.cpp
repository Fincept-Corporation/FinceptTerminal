#include "screens/polymarket/PolymarketDetailPanel.h"

#include "screens/polymarket/PolymarketActivityFeed.h"
#include "screens/polymarket/PolymarketOrderBook.h"
#include "screens/polymarket/PolymarketPriceChart.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

static const QStringList TAB_NAMES = {"OVERVIEW", "ORDER BOOK", "CHART", "TRADES", "HOLDERS", "COMMENTS", "RELATED"};
static const QStringList OUTCOME_COLORS = {"#00D66F", "#FF3B3B", "#FF8800", "#4F8EF7", "#A855F7"};

static QString fmt_vol(double v) {
    if (v >= 1e9)
        return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
    if (v >= 1e6)
        return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
    if (v >= 1e3)
        return QString("$%1K").arg(v / 1e3, 0, 'f', 1);
    return QString("$%1").arg(v, 0, 'f', 0);
}

static QString fmt_price(double p) {
    return QString("%1c").arg(qRound(p * 100));
}

PolymarketDetailPanel::PolymarketDetailPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("polyDetailPanel");
    build_ui();
}

void PolymarketDetailPanel::build_ui() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Tab bar
    auto* tab_bar = new QWidget(this);
    tab_bar->setFixedHeight(32);
    tab_bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* thl = new QHBoxLayout(tab_bar);
    thl->setContentsMargins(8, 0, 8, 0);
    thl->setSpacing(0);

    for (int i = 0; i < TAB_NAMES.size(); ++i) {
        auto* btn = new QPushButton(TAB_NAMES[i]);
        btn->setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: none; "
                                   "  font-size: 9px; font-weight: 700; padding: 6px 10px; "
                                   "  border-bottom: 2px solid transparent; }"
                                   "QPushButton:hover { color: %2; }"
                                   "QPushButton[active=\"true\"] { color: %2; border-bottom-color: %3; }")
                               .arg(colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(), colors::AMBER()));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { set_active_tab(i); });
        thl->addWidget(btn);
        tab_btns_.append(btn);
    }
    thl->addStretch(1);
    vl->addWidget(tab_bar);

    // Stacked pages
    stack_ = new QStackedWidget;
    stack_->addWidget(create_overview_page()); // 0

    orderbook_ = new PolymarketOrderBook;
    stack_->addWidget(orderbook_); // 1

    price_chart_ = new PolymarketPriceChart;
    connect(price_chart_, &PolymarketPriceChart::interval_changed, this, &PolymarketDetailPanel::interval_changed);
    connect(price_chart_, &PolymarketPriceChart::outcome_changed, this, &PolymarketDetailPanel::outcome_changed);
    stack_->addWidget(price_chart_); // 2

    activity_feed_ = new PolymarketActivityFeed;
    stack_->addWidget(activity_feed_); // 3

    stack_->addWidget(create_holders_page());  // 4
    stack_->addWidget(create_comments_page()); // 5
    stack_->addWidget(create_related_page());  // 6

    vl->addWidget(stack_, 1);
}

QWidget* PolymarketDetailPanel::create_overview_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    question_label_ = new QLabel("Select a market to view details");
    question_label_->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: 700; background: transparent;").arg(colors::TEXT_PRIMARY()));
    question_label_->setWordWrap(true);
    vl->addWidget(question_label_);

    // Stats grid
    auto* stats = new QWidget(this);
    auto* sgl = new QHBoxLayout(stats);
    sgl->setContentsMargins(0, 0, 0, 0);
    sgl->setSpacing(16);

    auto lbl_style =
        QString("color: %1; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; background: transparent;")
            .arg(colors::TEXT_SECONDARY());
    auto val_style =
        QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;").arg(colors::CYAN());

    auto make_stat = [&](const QString& label, QLabel*& val_lbl) {
        auto* box = new QWidget(this);
        auto* bvl = new QVBoxLayout(box);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(1);
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(lbl_style);
        val_lbl = new QLabel("--");
        val_lbl->setStyleSheet(val_style);
        bvl->addWidget(lbl);
        bvl->addWidget(val_lbl);
        sgl->addWidget(box);
    };

    make_stat("VOLUME", volume_label_);
    make_stat("LIQUIDITY", liquidity_label_);
    make_stat("END DATE", end_date_label_);
    make_stat("MIDPOINT", midpoint_label_);
    make_stat("SPREAD", spread_label_);
    make_stat("LAST TRADE", last_trade_label_);
    make_stat("OPEN INT", oi_label_);

    status_label_ = new QLabel;
    sgl->addWidget(status_label_);
    sgl->addStretch(1);
    vl->addWidget(stats);

    // Outcomes
    outcome_container_ = new QWidget(this);
    auto* ocl = new QVBoxLayout(outcome_container_);
    ocl->setContentsMargins(0, 0, 0, 0);
    ocl->setSpacing(2);
    vl->addWidget(outcome_container_);

    // Description
    description_label_ = new QLabel;
    description_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent;").arg(colors::TEXT_SECONDARY()));
    description_label_->setWordWrap(true);
    vl->addWidget(description_label_);

    vl->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget* PolymarketDetailPanel::create_holders_page() {
    holders_table_ = new QTableWidget;
    holders_table_->setColumnCount(4);
    holders_table_->setHorizontalHeaderLabels({"RANK", "TRADER", "SIZE", "AVG PRICE"});
    holders_table_->horizontalHeader()->setStretchLastSection(true);
    holders_table_->verticalHeader()->setVisible(false);
    holders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holders_table_->setStyleSheet(
        QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; font-size: 11px; }"
                "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %3; }"
                "QHeaderView::section { background: %4; color: %5; border: none; "
                "  border-bottom: 1px solid %3; padding: 4px 6px; font-size: 10px; font-weight: 700; }")
            .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    return holders_table_;
}

QWidget* PolymarketDetailPanel::create_comments_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    comments_container_ = new QWidget(this);
    auto* vl = new QVBoxLayout(comments_container_);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);
    auto* empty = new QLabel("No comments yet");
    empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
    empty->setAlignment(Qt::AlignCenter);
    vl->addWidget(empty);
    vl->addStretch(1);
    scroll->setWidget(comments_container_);
    return scroll;
}

QWidget* PolymarketDetailPanel::create_related_page() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    related_container_ = new QWidget(this);
    auto* vl = new QVBoxLayout(related_container_);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(4);
    auto* empty = new QLabel("No related markets");
    empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
    empty->setAlignment(Qt::AlignCenter);
    vl->addWidget(empty);
    vl->addStretch(1);
    scroll->setWidget(related_container_);
    return scroll;
}

void PolymarketDetailPanel::set_active_tab(int tab) {
    stack_->setCurrentIndex(tab);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        tab_btns_[i]->setProperty("active", i == tab);
        tab_btns_[i]->style()->unpolish(tab_btns_[i]);
        tab_btns_[i]->style()->polish(tab_btns_[i]);
    }
    emit tab_changed(tab);
}

// ── Data setters ────────────────────────────────────────────────────────────

void PolymarketDetailPanel::set_market(const Market& market) {
    question_label_->setText(market.question);
    volume_label_->setText(fmt_vol(market.volume));
    liquidity_label_->setText(fmt_vol(market.liquidity));
    end_date_label_->setText(market.end_date.left(10));
    midpoint_label_->setText("--");
    spread_label_->setText("--");
    last_trade_label_->setText("--");
    oi_label_->setText("--");

    // Status badge
    if (market.closed) {
        status_label_->setText("RESOLVED");
        status_label_->setStyleSheet(QString("color: %1; background: rgba(22,163,74,0.15); "
                                             "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::POSITIVE()));
    } else if (market.active) {
        status_label_->setText("ACTIVE");
        status_label_->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.15); "
                                             "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::AMBER()));
    } else {
        status_label_->setText("INACTIVE");
        status_label_->setStyleSheet(QString("color: %1; background: transparent; "
                                             "font-size: 9px; font-weight: 700; padding: 2px 6px;")
                                         .arg(colors::TEXT_DIM()));
    }

    // Outcomes
    auto* layout = outcome_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < market.outcomes.size(); ++i) {
        const auto& outcome = market.outcomes[i];
        auto* row = new QWidget(this);
        QString color = (i < OUTCOME_COLORS.size()) ? OUTCOME_COLORS[i] : colors::TEXT_SECONDARY;
        row->setStyleSheet(QString("background: %1; border-left: 3px solid %2; padding: 4px 8px; margin: 2px 0;")
                               .arg(colors::BG_RAISED(), color));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);
        rl->setSpacing(8);

        auto* name = new QLabel(outcome.name);
        name->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;")
                                .arg(colors::TEXT_PRIMARY()));
        auto* price = new QLabel(fmt_price(outcome.price));
        price->setStyleSheet(
            QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;").arg(color));
        rl->addWidget(name);
        rl->addStretch(1);
        rl->addWidget(price);
        layout->addWidget(row);
    }

    // Description
    description_label_->setText(market.description.left(500));

    // Token labels for chart
    QStringList labels;
    for (const auto& o : market.outcomes)
        labels.append(o.name);
    price_chart_->set_token_labels(labels);

    set_active_tab(0);
}

void PolymarketDetailPanel::set_price_summary(const PriceSummary& summary) {
    midpoint_label_->setText(fmt_price(summary.midpoint));
    spread_label_->setText(fmt_price(summary.spread));
    last_trade_label_->setText(fmt_price(summary.last_trade_price));
}

void PolymarketDetailPanel::set_order_book(const OrderBook& book) {
    orderbook_->set_data(book);
}

void PolymarketDetailPanel::set_price_history(const PriceHistory& history) {
    price_chart_->set_price_history(history);
}

void PolymarketDetailPanel::set_trades(const QVector<Trade>& trades) {
    activity_feed_->set_trades(trades);
}

void PolymarketDetailPanel::set_top_holders(const QVector<TopHolder>& holders) {
    holders_table_->setSortingEnabled(false);
    holders_table_->setRowCount(holders.size());
    for (int i = 0; i < holders.size(); ++i) {
        const auto& h = holders[i];
        auto* rank = new QTableWidgetItem(QString::number(h.rank > 0 ? h.rank : i + 1));
        rank->setTextAlignment(Qt::AlignCenter);
        holders_table_->setItem(i, 0, rank);
        holders_table_->setItem(i, 1, new QTableWidgetItem(h.display_name));
        auto* size = new QTableWidgetItem(QString::number(h.position_size, 'f', 2));
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 2, size);
        auto* price = new QTableWidgetItem(QString::number(h.entry_price, 'f', 4));
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        holders_table_->setItem(i, 3, price);
    }
    holders_table_->resizeColumnsToContents();
    holders_table_->setSortingEnabled(true);
}

void PolymarketDetailPanel::set_comments(const QVector<Comment>& comments) {
    auto* layout = comments_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (comments.isEmpty()) {
        auto* empty = new QLabel("No comments yet");
        empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
    } else {
        for (const auto& c : comments) {
            auto* card = new QWidget(this);
            card->setStyleSheet(QString("background: %1; padding: 8px; margin: 2px 0; "
                                        "border-left: 2px solid %2;")
                                    .arg(colors::BG_RAISED(), colors::BORDER_MED()));
            auto* cvl = new QVBoxLayout(card);
            cvl->setContentsMargins(8, 4, 8, 4);
            cvl->setSpacing(2);

            auto* author = new QLabel(c.author.isEmpty() ? c.author_address.left(10) + "..." : c.author);
            author->setStyleSheet(
                QString("color: %1; font-size: 10px; font-weight: 700; background: transparent;").arg(colors::AMBER()));
            cvl->addWidget(author);

            auto* body = new QLabel(c.body.left(300));
            body->setStyleSheet(
                QString("color: %1; font-size: 11px; background: transparent;").arg(colors::TEXT_PRIMARY()));
            body->setWordWrap(true);
            cvl->addWidget(body);

            auto* meta = new QLabel(QDateTime::fromSecsSinceEpoch(c.created_at, Qt::UTC).toString("yyyy-MM-dd HH:mm") +
                                    (c.likes > 0 ? QString("  %1 likes").arg(c.likes) : ""));
            meta->setStyleSheet(QString("color: %1; font-size: 9px; background: transparent;").arg(colors::TEXT_DIM()));
            cvl->addWidget(meta);

            layout->addWidget(card);
        }
    }

    // Add stretch at end
    static_cast<QVBoxLayout*>(layout)->addStretch(1);
}

void PolymarketDetailPanel::set_related_markets(const QVector<Market>& markets) {
    auto* layout = related_container_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (markets.isEmpty()) {
        auto* empty = new QLabel("No related markets");
        empty->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
        empty->setAlignment(Qt::AlignCenter);
        layout->addWidget(empty);
    } else {
        for (const auto& m : markets) {
            auto* card = new QPushButton;
            card->setText(m.question.left(60) + "\n" + fmt_vol(m.volume));
            card->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                        "  text-align: left; padding: 8px; font-size: 11px; }"
                                        "QPushButton:hover { background: %4; color: %5; }")
                                    .arg(colors::BG_RAISED(), colors::TEXT_SECONDARY(), colors::BORDER_DIM(),
                                         colors::BG_HOVER(), colors::TEXT_PRIMARY()));
            card->setCursor(Qt::PointingHandCursor);
            connect(card, &QPushButton::clicked, this, [this, m]() { emit related_market_clicked(m); });
            layout->addWidget(card);
        }
    }

    static_cast<QVBoxLayout*>(layout)->addStretch(1);
}

void PolymarketDetailPanel::set_open_interest(double oi) {
    oi_label_->setText(fmt_vol(oi));
}

void PolymarketDetailPanel::clear() {
    question_label_->setText("Select a market to view details");
    volume_label_->setText("--");
    liquidity_label_->setText("--");
    end_date_label_->setText("--");
    midpoint_label_->setText("--");
    spread_label_->setText("--");
    last_trade_label_->setText("--");
    oi_label_->setText("--");
    status_label_->clear();
    description_label_->clear();
    orderbook_->clear();
    activity_feed_->clear();
}

} // namespace fincept::screens::polymarket
