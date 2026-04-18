#pragma once
// AgentErrorsWidget — rolling list of recent agent execution errors.
// Pattern-subscribes to `agent:error:*`. Each publish is `{context, message}`.
// Displays the last N with timestamp (local arrival time).
//
// Config (persisted): { "max_rows": <int> }  — defaults to 10.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QDateTime>
#include <QString>
#include <QTableWidget>
#include <QVariant>

namespace fincept::screens::widgets {

class AgentErrorsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit AgentErrorsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct ErrorEntry {
        QDateTime when;
        QString context;
        QString message;
    };

    void apply_styles();
    void render();
    void on_error(const QString& topic, const QVariant& value);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    int max_rows_ = 10;
    QVector<ErrorEntry> entries_;
    QTableWidget* table_ = nullptr;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
