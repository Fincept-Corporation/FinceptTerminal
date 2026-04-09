#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Economic Calendar Widget — fetches real macro events from
/// GET http://api.fincept.in/macro/upcoming-events via HttpClient.
class EconomicCalendarWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit EconomicCalendarWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void refresh_data();
    void populate(const QJsonArray& events);

    QWidget* header_widget_ = nullptr;
    QFrame* header_sep_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVector<QLabel*> header_labels_;
};

} // namespace fincept::screens::widgets
