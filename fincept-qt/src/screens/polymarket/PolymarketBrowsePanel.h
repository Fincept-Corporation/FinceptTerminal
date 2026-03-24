#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::polymarket {

class PolymarketMarketCardModel;
class PolymarketMarketCardDelegate;

/// Left browse panel: market/event list with pagination.
class PolymarketBrowsePanel : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketBrowsePanel(QWidget* parent = nullptr);

    void set_markets(const QVector<services::polymarket::Market>& markets);
    void set_events(const QVector<services::polymarket::Event>& events);
    void set_loading(bool loading);

  signals:
    void market_selected(const services::polymarket::Market& market);
    void event_selected(const services::polymarket::Event& event);

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
