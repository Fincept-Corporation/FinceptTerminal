#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "ui/tables/DataTable.h"
#include "services/markets/MarketDataService.h"
#include <QPushButton>
#include <algorithm>

namespace fincept::screens::widgets {

/// Top movers widget with gainers/losers tab toggle. Fetches real data via yfinance.
class TopMoversWidget : public BaseWidget {
    Q_OBJECT
public:
    explicit TopMoversWidget(QWidget* parent = nullptr);

private:
    void refresh_data();
    void show_tab(bool gainers);

    ui::DataTable* table_ = nullptr;
    QPushButton* gainers_btn_ = nullptr;
    QPushButton* losers_btn_  = nullptr;
    QVector<services::QuoteData> all_quotes_;
    bool showing_gainers_ = true;
};

} // namespace fincept::screens::widgets
