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
namespace pmx = fincept::services::polymarket;
using namespace fincept::services::prediction;

static const QStringList TAB_NAMES = {"OVERVIEW", "ORDER BOOK", "CHART", "TRADES", "HOLDERS", "COMMENTS", "RELATED"};
// Fallback outcome palette used when the first two slots (green/red) don't
// cover a multi-outcome Polymarket market. The first two entries are
// overridden at paint time by the presentation's accent-aware colors.
static const QStringList OUTCOME_COLORS = {"#00D66F", "#FF3B3B", "#FF8800", "#4F8EF7", "#A855F7"};

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
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { set_active_tab(i); });
        thl->addWidget(btn);
        tab_btns_.append(btn);
    }
    thl->addStretch(1);
    vl->addWidget(tab_bar);
    apply_accent_to_tabs();

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

    auto make_stat = [&](const QString& label, QLabel*& val_lbl, QWidget** out_box = nullptr) {
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
        if (out_box) *out_box = box;
    };

    make_stat("VOLUME", volume_label_);
    make_stat("LIQUIDITY", liquidity_label_);
    make_stat("END DATE", end_date_label_);
    make_stat("MIDPOINT", midpoint_label_);
    make_stat("SPREAD", spread_label_);
    make_stat("LAST TRADE", last_trade_label_);
    make_stat("OPEN INT", oi_label_, &oi_box_);

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

void PolymarketDetailPanel::set_market(const PredictionMarket& market) {
    last_market_ = market;
    has_last_market_ = true;

    question_label_->setText(market.question);
    volume_label_->setText(presentation_.format_volume(market.volume));
    liquidity_label_->setText(presentation_.format_liquidity(market.liquidity));
    end_date_label_->setText(market.end_date_iso.left(10));
    midpoint_label_->setText("--");
    spread_label_->setText("--");
    last_trade_label_->setText("--");
    oi_label_->setText("--");

    render_status_badge(market);

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
        // Slot 0 tracks the exchange accent (Polymarket amber / Kalshi teal)
        // so the primary outcome is visually tied to the current exchange.
        // Slots 1+ fall back to the static palette.
        QString color;
        if (i == 0) color = presentation_.accent.name();
        else if (i < OUTCOME_COLORS.size()) color = OUTCOME_COLORS[i];
        else color = colors::TEXT_SECONDARY();
        row->setStyleSheet(QString("background: %1; border-left: 3px solid %2; padding: 4px 8px; margin: 2px 0;")
                               .arg(colors::BG_RAISED(), color));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);
        rl->setSpacing(8);

        auto* name = new QLabel(outcome.name);
        name->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; background: transparent;")
                                .arg(colors::TEXT_PRIMARY()));
        auto* price = new QLabel(presentation_.format_price(outcome.price));
        price->setStyleSheet(
            QString("color: %1; font-size: 13px; font-weight: 700; background: transparent;").arg(color));
        rl->addWidget(name);
        rl->addStretch(1);
        rl->addWidget(price);
        layout->addWidget(row);
    }

    description_label_->setText(market.description.left(500));

    // Outcome labels for chart — dynamic, read from prediction::Outcome::name.
    QStringList labels;
    labels.reserve(market.outcomes.size());
    for (const auto& o : market.outcomes)
        labels.append(o.name);
    price_chart_->set_outcome_labels(labels);

    set_active_tab(0);
}

void PolymarketDetailPanel::set_price_summary(const pmx::PriceSummary& summary) {
    midpoint_label_->setText(presentation_.format_price(summary.midpoint));
    spread_label_->setText(presentation_.format_price(summary.spread));
    last_trade_label_->setText(presentation_.format_price(summary.last_trade_price));
}

void PolymarketDetailPanel::set_order_book(const PredictionOrderBook& book) {
    orderbook_->set_data(book);
}

void PolymarketDetailPanel::set_price_history(const PriceHistory& history) {
    price_chart_->set_price_history(history);
}

void PolymarketDetailPanel::set_trades(const QVector<PredictionTrade>& trades) {
    activity_feed_->set_trades(trades);
}

void PolymarketDetailPanel::set_top_holders(const QVector<pmx::TopHolder>& holders) {
    holders_table_->setSortingEnabled(false);
    holders_table_->setRowCount(holders.size());
    for (int i = 0; i < holders.size(); ++i) {
        const pmx::TopHolder& h = holders[i];
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

void PolymarketDetailPanel::set_comments(const QVector<pmx::Comment>& comments) {
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

void PolymarketDetailPanel::set_related_markets(const QVector<PredictionMarket>& markets) {
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
            card->setText(m.question.left(60) + "\n" + presentation_.format_volume(m.volume));
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
    oi_label_->setText(presentation_.format_volume(oi));
}

void PolymarketDetailPanel::set_polymarket_extras_enabled(bool enabled) {
    // The Holders / Comments / Related tabs exist only for Polymarket.
    // When the active exchange doesn't offer them, hide the tab buttons and
    // wipe any lingering content so stale Polymarket data is never shown
    // under a Kalshi market.
    const int extra_tabs[] = {kTabHolders, kTabComments, kTabRelated};
    for (int idx : extra_tabs) {
        if (idx >= 0 && idx < tab_btns_.size())
            tab_btns_[idx]->setVisible(enabled);
    }
    if (!enabled) {
        if (holders_table_) {
            holders_table_->setRowCount(0);
        }
        auto clear_container = [this](QWidget* container, const QString& empty_msg) {
            if (!container) return;
            auto* layout = container->layout();
            while (layout->count() > 0) {
                auto* item = layout->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
            auto* empty = new QLabel(empty_msg);
            empty->setStyleSheet(
                QString("color: %1; font-size: 13px; background: transparent;").arg(colors::TEXT_DIM()));
            empty->setAlignment(Qt::AlignCenter);
            layout->addWidget(empty);
            static_cast<QVBoxLayout*>(layout)->addStretch(1);
        };
        clear_container(comments_container_, tr("Comments are a Polymarket-only feature"));
        clear_container(related_container_, tr("Related markets are a Polymarket-only feature"));
        // If the user is currently sitting on a Polymarket-only tab, bounce
        // them back to Overview so they don't stare at a hidden tab.
        const int current = stack_ ? stack_->currentIndex() : 0;
        if (current == kTabHolders || current == kTabComments || current == kTabRelated)
            set_active_tab(0);
    }
}

void PolymarketDetailPanel::clear() {
    has_last_market_ = false;
    last_market_ = {};
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

// ── Presentation wiring ─────────────────────────────────────────────────────

void PolymarketDetailPanel::set_presentation(const ExchangePresentation& p) {
    const bool accent_changed = p.accent != presentation_.accent;
    const bool oi_changed = p.has_open_interest != presentation_.has_open_interest;
    const bool extras_changed = p.has_polymarket_extras != presentation_.has_polymarket_extras;

    presentation_ = p;

    if (accent_changed) apply_accent_to_tabs();
    if (oi_changed) apply_presentation_to_stats();
    if (extras_changed) set_polymarket_extras_enabled(p.has_polymarket_extras);

    // Re-render the currently-selected market so volume / price / status
    // pick up the new formatters. If no market is selected yet, the next
    // set_market() will apply the profile automatically.
    if (has_last_market_) set_market(last_market_);
}

void PolymarketDetailPanel::apply_accent_to_tabs() {
    const QColor& a = presentation_.accent;
    const QString accent_css =
        QStringLiteral("rgba(%1,%2,%3,1.0)").arg(a.red()).arg(a.green()).arg(a.blue());
    const QString css =
        QStringLiteral("QPushButton { background: transparent; color: %1; border: none; "
                       "  font-size: 9px; font-weight: 700; padding: 6px 10px; "
                       "  border-bottom: 2px solid transparent; }"
                       "QPushButton:hover { color: %2; }"
                       "QPushButton[active=\"true\"] { color: %2; border-bottom-color: %3; }")
            .arg(colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(), accent_css);
    for (auto* btn : tab_btns_) {
        btn->setStyleSheet(css);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void PolymarketDetailPanel::apply_presentation_to_stats() {
    if (oi_box_) oi_box_->setVisible(presentation_.has_open_interest);
}

void PolymarketDetailPanel::render_status_badge(const PredictionMarket& market) {
    const auto badge = presentation_.status_badge(market);
    status_label_->setText(badge.text);

    const QString bg_css = badge.bg.alpha() > 0
        ? QStringLiteral("rgba(%1,%2,%3,%4)")
              .arg(badge.bg.red()).arg(badge.bg.green()).arg(badge.bg.blue())
              .arg(badge.bg.alphaF(), 0, 'f', 2)
        : QStringLiteral("transparent");
    status_label_->setStyleSheet(
        QString("color: %1; background: %2; font-size: 9px; font-weight: 700; padding: 2px 6px;")
            .arg(badge.fg.name(), bg_css));
}

} // namespace fincept::screens::polymarket
