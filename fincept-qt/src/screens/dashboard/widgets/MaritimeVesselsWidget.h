#pragma once
// MaritimeVesselsWidget — live position summary for a configurable vessel
// list. Subscribes to `maritime:vessel:<imo>` per IMO. Each row: vessel name,
// from→to ports, speed (kn), route progress %.
//
// Config (persisted): { "imos": ["9811000", ...] } — defaults to a small
// well-known set so the widget is non-empty out of the box.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

class MaritimeVesselsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MaritimeVesselsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Row {
        QLabel* name = nullptr;
        QLabel* route = nullptr;
        QLabel* speed = nullptr;
        QLabel* progress = nullptr;
        QWidget* container = nullptr;
    };

    void apply_styles();
    void build_rows();
    void clear_rows();
    void on_vessel(const QString& imo, const QVariant& v);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QStringList imos_;
    QHash<QString, Row> rows_;

    QWidget* header_widget_ = nullptr;
    QFrame* header_sep_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QVector<QLabel*> header_labels_;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
