#include "screens/polymarket/PolymarketBrowsePanel.h"

#include "screens/polymarket/PolymarketMarketCard.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

PolymarketBrowsePanel::PolymarketBrowsePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("polyBrowsePanel");

    model_ = new PolymarketMarketCardModel(this);
    delegate_ = new PolymarketMarketCardDelegate(this);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    header_ = new QLabel("MARKETS");
    header_->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                   "letter-spacing: 0.5px; background: transparent; "
                                   "padding: 6px 8px; border-bottom: 1px solid %2;")
                               .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
    vl->addWidget(header_);

    list_view_ = new QListView;
    list_view_->setModel(model_);
    list_view_->setItemDelegate(delegate_);
    list_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_view_->setStyleSheet(QString("QListView { background: %1; border: none; outline: none; }"
                                      "QListView::item { border: none; }")
                                  .arg(colors::BG_BASE()));
    connect(list_view_, &QListView::clicked, this, &PolymarketBrowsePanel::on_item_clicked);
    vl->addWidget(list_view_, 1);

    // Pagination
    auto* page_bar = new QWidget(this);
    page_bar->setFixedHeight(30);
    page_bar->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* phl = new QHBoxLayout(page_bar);
    phl->setContentsMargins(8, 0, 8, 0);
    phl->setSpacing(6);

    prev_btn_ = new QPushButton("PREV");
    prev_btn_->setStyleSheet(QString("background: %1; color: %2; border: 1px solid %3; "
                                     "padding: 3px 10px; font-size: 9px; font-weight: 700;")
                                 .arg(colors::BG_SURFACE(), colors::TEXT_SECONDARY(), colors::BORDER_DIM()));
    prev_btn_->setCursor(Qt::PointingHandCursor);
    connect(prev_btn_, &QPushButton::clicked, this, &PolymarketBrowsePanel::on_prev);

    page_label_ = new QLabel("Page 1");
    page_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(colors::TEXT_SECONDARY()));
    page_label_->setAlignment(Qt::AlignCenter);

    next_btn_ = new QPushButton("NEXT");
    next_btn_->setStyleSheet(prev_btn_->styleSheet());
    next_btn_->setCursor(Qt::PointingHandCursor);
    connect(next_btn_, &QPushButton::clicked, this, &PolymarketBrowsePanel::on_next);

    phl->addWidget(prev_btn_);
    phl->addStretch(1);
    phl->addWidget(page_label_);
    phl->addStretch(1);
    phl->addWidget(next_btn_);
    vl->addWidget(page_bar);
}

void PolymarketBrowsePanel::set_markets(const QVector<Market>& markets) {
    current_page_ = 0;
    model_->set_markets(markets);
    header_->setText(QString("MARKETS (%1)").arg(markets.size()));
    update_page();
}

void PolymarketBrowsePanel::set_events(const QVector<Event>& events) {
    current_page_ = 0;
    model_->set_events(events);
    header_->setText(QString("EVENTS (%1)").arg(events.size()));
    update_page();
}

void PolymarketBrowsePanel::set_loading(bool loading) {
    if (loading)
        header_->setText("LOADING...");
}

void PolymarketBrowsePanel::on_item_clicked(const QModelIndex& index) {
    if (!index.isValid())
        return;
    int row = index.row();

    const auto* mkt = model_->market_at(row);
    if (mkt) {
        emit market_selected(*mkt);
        return;
    }

    const auto* evt = model_->event_at(row);
    if (evt) {
        emit event_selected(*evt);
    }
}

void PolymarketBrowsePanel::on_prev() {
    if (current_page_ > 0) {
        --current_page_;
        update_page();
    }
}

void PolymarketBrowsePanel::on_next() {
    int total = model_->rowCount();
    int pages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    if (current_page_ + 1 < pages) {
        ++current_page_;
        update_page();
    }
}

void PolymarketBrowsePanel::update_page() {
    int total = model_->rowCount();
    int pages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    page_label_->setText(QString("Page %1 / %2").arg(current_page_ + 1).arg(pages));
    prev_btn_->setEnabled(current_page_ > 0);
    next_btn_->setEnabled(current_page_ + 1 < pages);
}

} // namespace fincept::screens::polymarket
