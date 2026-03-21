#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include <QScrollArea>

namespace fincept::screens::widgets {

/// Market news widget — fetches real news via yfinance for major tickers.
class NewsWidget : public BaseWidget {
    Q_OBJECT
public:
    explicit NewsWidget(QWidget* parent = nullptr);

private:
    void refresh_data();
    void populate(const QJsonArray& articles);

    QVBoxLayout* news_layout_ = nullptr;
};

} // namespace fincept::screens::widgets
