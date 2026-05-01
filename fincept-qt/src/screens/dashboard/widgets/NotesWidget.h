#pragma once
// NotesWidget — recent / favorite financial notes from NotesRepository.
// Refreshes on EventBus `notes.created` / `notes.deleted`. Click a row to
// jump to the Notes screen via `nav.switch_screen`.
//
// Config (persisted): { "filter": "recent"|"favorites", "max_rows": <int> }

#include "core/events/EventBus.h"
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

class NotesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit NotesWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void apply_styles();
    void refresh_data();
    void clear_rows();
    void subscribe_events();
    void unsubscribe_events();

    QString filter_ = "recent"; // "recent" or "favorites"
    int max_rows_ = 8;

    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QList<EventBus::HandlerId> event_subs_;
};

} // namespace fincept::screens::widgets
