#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QJsonArray>

namespace fincept::screens::widgets {

/// Economic Calendar Widget — fetches real macro events from
/// GET http://api.fincept.in/macro/upcoming-events via HttpClient.
class EconomicCalendarWidget : public BaseWidget {
    Q_OBJECT
public:
    explicit EconomicCalendarWidget(QWidget* parent = nullptr);

private:
    void refresh_data();
    void populate(const QJsonArray& events);

    QVBoxLayout* list_layout_ = nullptr;
    QLabel*      status_label_ = nullptr;
};

} // namespace fincept::screens::widgets
