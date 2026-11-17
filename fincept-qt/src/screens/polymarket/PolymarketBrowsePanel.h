#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "screens/polymarket/PolymarketViewState.h"
#include "services/prediction/PredictionTypes.h"

#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

class PolymarketMarketCardModel;
class PolymarketMarketCardDelegate;

/// Left browse panel: market/event list with pagination.
/// Consumes unified prediction::* types so both Polymarket and Kalshi render
/// through the same delegate.
class PolymarketBrowsePanel : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketBrowsePanel(QWidget* parent = nullptr);

    /// Replace the list. With `keep_position` (a timer / refresh-button reload of
    /// the same view) the page, the scroll offset and the highlighted card (by
    /// market / event id) survive; otherwise the list starts at the top.
    void set_markets(const QVector<fincept::services::prediction::PredictionMarket>& markets,
                     bool keep_position = false);
    void set_events(const QVector<fincept::services::prediction::PredictionEvent>& events,
                    bool keep_position = false);
    /// Loading with nothing listed shows a card skeleton; a list on screen stays.
    void set_loading(bool loading);
    /// The last list load failed: with nothing listed, an error page with RETRY
    /// (retry_requested); with a list on screen it stays and the header says so.
    void set_error(const QString& message);
    /// What the next list is filtered by, for its "no results" page ("Sports",
    /// "\"fed\""; empty = unfiltered). Non-empty adds CLEAR FILTERS.
    void set_filter_context(const QString& description);
    /// Highlight the card of market `market_id` (in events mode: the event that
    /// contains it), else the first card, and give back that card's market (an
    /// event's first market) — the automatic selection when nothing is selected.
    /// Emits nothing. False when there is no selectable card.
    bool pick_default(const QString& market_id, fincept::services::prediction::PredictionMarket* out);
    int item_count() const;
    void clear();

    /// Replace a single market row in place (e.g. after a Kalshi lifecycle
    /// event refetched one market). No-op if the ticker isn't in the list.
    void update_market_row(const fincept::services::prediction::PredictionMarket& market);

    /// Forwarded to the model so the delegate paints with the active
    /// exchange's accent and price formatter.
    void set_presentation(const ExchangePresentation& p);

  signals:
    void market_selected(const fincept::services::prediction::PredictionMarket& market);
    void event_selected(const fincept::services::prediction::PredictionEvent& event);
    /// RETRY on the failure page / REFRESH on the no-results page.
    void retry_requested();
    /// CLEAR FILTERS on the no-results page.
    void clear_filters_requested();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_item_clicked(const QModelIndex& index);
    void on_prev();
    void on_next();

  private:
    /// Refresh the page label / buttons; with `scroll`, also move the view to
    /// the first row of the current page.
    void update_page(bool scroll = true);
    void retranslateUi();
    /// Id (market_id, or event_id in events mode) of the highlighted card, or "".
    QString current_item_id() const;
    /// Re-highlight the card with `id` (if still listed), then put the scroll
    /// offset back to `scroll_value`.
    void restore_position(const QString& id, int scroll_value);
    /// The list, or — with nothing listed — the "no results" page.
    void show_list_or_empty();
    /// "MARKETS  12" / "EVENTS  4" (+ " · UPDATE FAILED" after a failed reload).
    QString header_text() const;

    PolymarketMarketCardModel* model_ = nullptr;
    PolymarketMarketCardDelegate* delegate_ = nullptr;
    QStackedWidget* body_stack_ = nullptr;
    QListView* list_view_ = nullptr;
    PmViewState body_state_;
    QLabel* header_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel* page_label_ = nullptr;

    int current_page_ = 0;
    bool events_mode_ = false;
    bool loading_ = false;
    bool reload_failed_ = false;   ///< the last reload failed while a list stayed on screen
    QString filter_context_;
    static constexpr int PAGE_SIZE = 20;
};

} // namespace fincept::screens::polymarket
