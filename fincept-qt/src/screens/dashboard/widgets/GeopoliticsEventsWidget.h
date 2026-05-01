#pragma once
// GeopoliticsEventsWidget — rolling list of global conflict / geopolitical
// news events. Subscribes to `geopolitics:events` (EventsPage payload, sorted
// newest-first). Each row: category color dot, title, country/city, source.
//
// Config (persisted): { "max_rows": <int> } — defaults to 15.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

class GeopoliticsEventsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit GeopoliticsEventsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const QVariant& payload);
    void clear_rows();
    void hub_resubscribe();
    void hub_unsubscribe_all();

    int max_rows_ = 15;
    QWidget* header_widget_ = nullptr;
    QFrame* header_sep_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVector<QLabel*> header_labels_;
    QVariant last_payload_; // cached for theme-change re-populate
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
