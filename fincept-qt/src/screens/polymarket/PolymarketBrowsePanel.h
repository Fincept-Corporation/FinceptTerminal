#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/prediction/PredictionTypes.h"

#include <QLabel>
#include <QListView>
#include <QPushButton>
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

    void set_markets(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void set_events(const QVector<fincept::services::prediction::PredictionEvent>& events);
    void set_loading(bool loading);
    void clear();

    /// Forwarded to the model so the delegate paints with the active
    /// exchange's accent and price formatter.
    void set_presentation(const ExchangePresentation& p);

  signals:
    void market_selected(const fincept::services::prediction::PredictionMarket& market);
    void event_selected(const fincept::services::prediction::PredictionEvent& event);

  private slots:
    void on_item_clicked(const QModelIndex& index);
    void on_prev();
    void on_next();

  private:
    void update_page();

    PolymarketMarketCardModel* model_ = nullptr;
    PolymarketMarketCardDelegate* delegate_ = nullptr;
    QListView* list_view_ = nullptr;
    QLabel* header_ = nullptr;
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel* page_label_ = nullptr;

    int current_page_ = 0;
    static constexpr int PAGE_SIZE = 20;
};

} // namespace fincept::screens::polymarket
