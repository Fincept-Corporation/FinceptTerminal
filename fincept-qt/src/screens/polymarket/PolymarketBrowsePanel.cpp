#include "screens/polymarket/PolymarketBrowsePanel.h"

#include "screens/polymarket/PolymarketMarketCard.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

PolymarketBrowsePanel::PolymarketBrowsePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("polyBrowsePanel");

    model_ = new PolymarketMarketCardModel(this);
    delegate_ = new PolymarketMarketCardDelegate(this);
    // The delegate formats volume / prices at paint time: a number / currency
    // format change only needs the cards repainted (no reset, no re-fetch).
    fincept::ui::formatting::refresh_model_on_format_change(model_);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Header bar ───────────────────────────────────────────────────────
    auto* header_bar = new QWidget(this);
    header_bar->setFixedHeight(30);
    header_bar->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
    auto* hbl = new QHBoxLayout(header_bar);
    hbl->setContentsMargins(12, 0, 8, 0);
    hbl->setSpacing(6);

    header_ = new QLabel(tr("MARKETS"));
    header_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; letter-spacing: 0.8px; "
                                   "background: transparent;")
                               .arg(colors::TEXT_SECONDARY()));
    hbl->addWidget(header_);
    hbl->addStretch(1);
    vl->addWidget(header_bar);

    // ── List view (or its loading / no-results / failure page) ───────────
    body_stack_ = new QStackedWidget(this);
    list_view_ = new QListView(body_stack_);
    list_view_->setModel(model_);
    list_view_->setItemDelegate(delegate_);
    list_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_view_->setStyleSheet(QString("QListView {"
                                      "  background: %1;"
                                      "  border: none;"
                                      "  outline: none;"
                                      "}"
                                      "QListView::item { border: none; }"
                                      "QScrollBar:vertical {"
                                      "  background: %2;"
                                      "  width: 4px;"
                                      "  border: none;"
                                      "}"
                                      "QScrollBar::handle:vertical {"
                                      "  background: %3;"
                                      "  min-height: 20px;"
                                      "}"
                                      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                                  .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::BORDER_BRIGHT()));
    list_view_->setAccessibleName(tr("Market list"));
    connect(list_view_, &QListView::clicked, this, &PolymarketBrowsePanel::on_item_clicked);
    // Keyboard: Enter/Return on the highlighted card opens it, matching the
    // click path. Without this the list was mouse-only.
    connect(list_view_, &QListView::activated, this, &PolymarketBrowsePanel::on_item_clicked);
    body_stack_->addWidget(list_view_);
    body_state_ = PmViewState(body_stack_, list_view_, ui::LoadingSkeleton::Shape::Cards, tr("Loading markets…"));
    // The first thing this panel does is wait for a list: open on the skeleton,
    // never on an empty list.
    body_state_.show_loading();
    vl->addWidget(body_stack_, 1);

    // ── Pagination bar ────────────────────────────────────────────────────
    auto* page_bar = new QWidget(this);
    page_bar->setFixedHeight(32);
    page_bar->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* phl = new QHBoxLayout(page_bar);
    phl->setContentsMargins(8, 0, 8, 0);
    phl->setSpacing(4);

    const QString nav_btn_css = QString("QPushButton {"
                                        "  background: transparent;"
                                        "  color: %1;"
                                        "  border: 1px solid %2;"
                                        "  padding: 3px 12px;"
                                        "  font-size: " + fincept::ui::fonts::px(-5) + "px;"
                                        "  font-weight: 700;"
                                        "}"
                                        "QPushButton:hover { color: %3; border-color: %4; }"
                                        "QPushButton:disabled { color: %5; border-color: %2; }")
                                    .arg(colors::TEXT_SECONDARY(), colors::BORDER_DIM(), colors::TEXT_PRIMARY(),
                                         colors::BORDER_BRIGHT(), colors::TEXT_DIM());

    prev_btn_ = new QPushButton("◀");
    prev_btn_->setStyleSheet(nav_btn_css);
    prev_btn_->setMinimumSize(30, 22);
    prev_btn_->setCursor(Qt::PointingHandCursor);
    prev_btn_->setToolTip(tr("Previous page"));
    prev_btn_->setAccessibleName(tr("Previous page"));
    connect(prev_btn_, &QPushButton::clicked, this, &PolymarketBrowsePanel::on_prev);

    page_label_ = new QLabel("1 / 1");
    page_label_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; min-width: 40px;").arg(colors::TEXT_SECONDARY()));
    page_label_->setAlignment(Qt::AlignCenter);

    next_btn_ = new QPushButton("▶");
    next_btn_->setStyleSheet(nav_btn_css);
    next_btn_->setMinimumSize(30, 22);
    next_btn_->setCursor(Qt::PointingHandCursor);
    next_btn_->setToolTip(tr("Next page"));
    next_btn_->setAccessibleName(tr("Next page"));
    connect(next_btn_, &QPushButton::clicked, this, &PolymarketBrowsePanel::on_next);

    phl->addWidget(prev_btn_);
    phl->addStretch(1);
    phl->addWidget(page_label_);
    phl->addStretch(1);
    phl->addWidget(next_btn_);
    vl->addWidget(page_bar);
}

// The 60 s auto-refresh used to reset the list to page 1, scroll to row 0 and
// drop the highlighted card while the user was reading further down. A reload
// of the same view (keep_position) now keeps all three.
void PolymarketBrowsePanel::set_markets(const QVector<PredictionMarket>& markets, bool keep_position) {
    const bool keep = keep_position && !events_mode_;
    const QString selected_id = keep ? current_item_id() : QString();
    const int scroll_value = list_view_->verticalScrollBar()->value();
    events_mode_ = false;
    loading_ = false;
    reload_failed_ = false;
    if (!keep)
        current_page_ = 0;
    model_->set_markets(markets);
    header_->setText(header_text());
    header_->setToolTip(QString());
    update_page(/*scroll=*/!keep);
    show_list_or_empty();
    if (keep)
        restore_position(selected_id, scroll_value);
}

void PolymarketBrowsePanel::set_events(const QVector<PredictionEvent>& events, bool keep_position) {
    const bool keep = keep_position && events_mode_;
    const QString selected_id = keep ? current_item_id() : QString();
    const int scroll_value = list_view_->verticalScrollBar()->value();
    events_mode_ = true;
    loading_ = false;
    reload_failed_ = false;
    if (!keep)
        current_page_ = 0;
    model_->set_events(events);
    header_->setText(header_text());
    header_->setToolTip(QString());
    update_page(/*scroll=*/!keep);
    show_list_or_empty();
    if (keep)
        restore_position(selected_id, scroll_value);
}

QString PolymarketBrowsePanel::current_item_id() const {
    const QModelIndex cur = list_view_->currentIndex();
    if (!cur.isValid())
        return {};
    if (const auto* m = model_->market_at(cur.row()))
        return m->key.market_id;
    if (const auto* e = model_->event_at(cur.row()))
        return e->key.event_id;
    return {};
}

void PolymarketBrowsePanel::restore_position(const QString& id, int scroll_value) {
    if (!id.isEmpty()) {
        const int rows = model_->rowCount();
        for (int r = 0; r < rows; ++r) {
            const auto* m = model_->market_at(r);
            const auto* e = m ? nullptr : model_->event_at(r);
            const QString row_id = m ? m->key.market_id : (e ? e->key.event_id : QString());
            if (row_id == id) {
                // Setting the current index may auto-scroll; the offset is put
                // back right after, so the view does not move.
                list_view_->selectionModel()->setCurrentIndex(model_->index(r, 0),
                                                              QItemSelectionModel::ClearAndSelect);
                break;
            }
        }
    }
    list_view_->verticalScrollBar()->setValue(scroll_value);
}

QString PolymarketBrowsePanel::header_text() const {
    const int total = model_->rowCount();
    QString text = events_mode_ ? tr("EVENTS  %1").arg(total) : tr("MARKETS  %1").arg(total);
    if (reload_failed_)
        text += tr("  ·  UPDATE FAILED");
    return text;
}

void PolymarketBrowsePanel::set_loading(bool loading) {
    loading_ = loading;
    if (loading) {
        header_->setText(tr("LOADING..."));
        // A list on screen stays while the new one loads (refresh, other view).
        if (model_->rowCount() == 0)
            body_state_.show_loading();
    } else {
        header_->setText(header_text());
        show_list_or_empty();
    }
}

void PolymarketBrowsePanel::set_error(const QString& message) {
    loading_ = false;
    if (model_->rowCount() > 0) {
        // Keep what is listed; say that it is not the latest.
        reload_failed_ = true;
        header_->setText(header_text());
        header_->setToolTip(tr("The last update failed: %1").arg(message));
        body_state_.show_content();
        return;
    }
    header_->setText(events_mode_ ? tr("EVENTS") : tr("MARKETS"));
    header_->setToolTip(QString());
    body_state_.show_state(ui::EmptyState::error_spec(message, tr("Could not load the markets")),
                           [this]() { emit retry_requested(); });
}

void PolymarketBrowsePanel::set_filter_context(const QString& description) {
    filter_context_ = description;
}

void PolymarketBrowsePanel::show_list_or_empty() {
    if (model_->rowCount() > 0) {
        body_state_.show_content();
        return;
    }
    if (loading_) {
        body_state_.show_loading();
        return;
    }
    if (!filter_context_.isEmpty()) {
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            tr("Nothing matches %1").arg(filter_context_),
            tr("No open market matches this filter right now. Clear it to see every market, or pick another "
               "category."),
            tr("CLEAR FILTERS"));
        spec.secondary = tr("REFRESH");
        body_state_.show_state(
            spec, [this]() { emit clear_filters_requested(); }, [this]() { emit retry_requested(); });
        return;
    }
    body_state_.show_state(
        ui::EmptyState::no_data_spec(events_mode_ ? tr("No events in this view") : tr("No markets in this view"),
                                     tr("The exchange sent back an empty list. Refresh in a moment, or pick "
                                        "another view above."),
                                     tr("REFRESH")),
        [this]() { emit retry_requested(); });
}

int PolymarketBrowsePanel::item_count() const {
    return model_->rowCount();
}

bool PolymarketBrowsePanel::pick_default(const QString& market_id, PredictionMarket* out) {
    if (!out)
        return false;
    const int rows = model_->rowCount();
    int pick = -1;
    const PredictionMarket* chosen = nullptr;
    if (!market_id.isEmpty()) {
        for (int r = 0; r < rows && pick < 0; ++r) {
            if (const auto* m = model_->market_at(r)) {
                if (m->key.market_id == market_id) {
                    pick = r;
                    chosen = m;
                }
            } else if (const auto* e = model_->event_at(r)) {
                for (const auto& em : e->markets) {
                    if (em.key.market_id == market_id) {
                        pick = r;
                        chosen = &em;
                        break;
                    }
                }
            }
        }
    }
    for (int r = 0; r < rows && pick < 0; ++r) {
        if (const auto* m = model_->market_at(r)) {
            pick = r;
            chosen = m;
        } else if (const auto* e = model_->event_at(r)) {
            if (!e->markets.isEmpty()) {
                pick = r;
                chosen = &e->markets.first();
            }
        }
    }
    if (pick < 0 || !chosen)
        return false;
    *out = *chosen;
    // Highlight it the way a click would, without emitting a selection.
    const QModelIndex idx = model_->index(pick, 0);
    list_view_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
    current_page_ = pick / PAGE_SIZE;
    update_page(/*scroll=*/false);
    list_view_->scrollTo(idx, QAbstractItemView::EnsureVisible);
    return true;
}

void PolymarketBrowsePanel::clear() {
    events_mode_ = false;
    reload_failed_ = false;
    model_->set_markets({});
    header_->setText(tr("MARKETS"));
    header_->setToolTip(QString());
    current_page_ = 0;
    update_page();
}

void PolymarketBrowsePanel::set_presentation(const ExchangePresentation& p) {
    if (model_)
        model_->set_presentation(p);
}

void PolymarketBrowsePanel::update_market_row(const PredictionMarket& market) {
    if (model_)
        model_->update_market(market);
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

void PolymarketBrowsePanel::update_page(bool scroll) {
    int total = model_->rowCount();
    int pages = qMax(1, (total + PAGE_SIZE - 1) / PAGE_SIZE);
    if (current_page_ >= pages)
        current_page_ = pages - 1;
    page_label_->setText(QString("%1 / %2").arg(current_page_ + 1).arg(pages));
    prev_btn_->setEnabled(current_page_ > 0);
    next_btn_->setEnabled(current_page_ + 1 < pages);
    // The ◀ / ▶ buttons previously only relabelled themselves — the list view
    // never moved, so paging was a no-op control. Scroll the view to the first
    // row of the page so the buttons actually navigate.
    if (scroll && list_view_ && total > 0) {
        const int row = qBound(0, current_page_ * PAGE_SIZE, total - 1);
        const auto idx = model_->index(row, 0);
        if (idx.isValid())
            list_view_->scrollTo(idx, QAbstractItemView::PositionAtTop);
    }
}

void PolymarketBrowsePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketBrowsePanel::retranslateUi() {
    // Re-derive the header caption (mode word + live count) in the new
    // language, without touching the loading / page state.
    if (header_)
        header_->setText(loading_ ? tr("LOADING...") : header_text());
    // Pagination arrows + "n / m" page label are symbols/numbers; nav nothing.
}

} // namespace fincept::screens::polymarket
