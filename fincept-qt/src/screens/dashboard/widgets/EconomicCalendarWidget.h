#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

/// Economic Calendar Widget — consumes the DataHub topic
/// `econ:fincept:upcoming_events` (HTTP-backed by `MacroCalendarService`).
/// All cadence is owned by the hub scheduler.
class EconomicCalendarWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit EconomicCalendarWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void apply_styles();
    void clear_list();
    void show_status(const QString& text);
    void hub_subscribe();
    void hub_unsubscribe();
    void populate(const QJsonArray& events);

    QWidget* header_widget_ = nullptr;
    QFrame* header_sep_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVector<QLabel*> header_labels_;
    QJsonArray last_events_; // cached for theme-change re-populate
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
