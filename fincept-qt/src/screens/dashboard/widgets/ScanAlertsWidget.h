#pragma once
// ScanAlertsWidget — newest-first feed of the alert/scan triggers fired by
// fincept::algo::ScanMonitor (Algo Trading › Alerts watches, both the polled
// and the realtime-universe mode): time, symbol, condition.
//
// Sources: the persisted history (ScanEventRepository::recent → the
// scan_watch_events table that ScanMonitor writes on every fire) is read on
// show; while visible, ScanMonitor::watch_fired is followed live. Scan alerts
// have no DataHub topic, and the tile owns no refresh timer (D3).
//
// Double-click a row to open its symbol in Equity Research (a "test fire" row
// opens Algo Trading); right-click for the shared symbol menu. The footer /
// empty state opens Algo Trading.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QHash>
#include <QJsonObject>
#include <QMetaObject>
#include <QString>
#include <QVector>

class QLabel;
class QTableWidget;

namespace fincept::screens::widgets {

class ScanAlertsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit ScanAlertsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    struct Alert {
        QString watch_id;
        QString symbol;
        QString detail;
        qint64 fired_at = 0;
    };

    void apply_styles();
    void reload_history();
    void on_watch_fired(const QString& watch_id, const QString& symbol, const QString& detail);
    /// Watch name for the tooltip. Poll-mode watches are deleted once they
    /// fire, so names are cached for the tile's lifetime; a miss is looked up
    /// once (watch_fired is delivered before the watch row is removed).
    QString watch_name(const QString& watch_id);
    void render();
    void on_row_double_clicked(int row);

    QWidget* body_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* empty_label_ = nullptr;
    QLabel* footer_ = nullptr;
    QVector<Alert> alerts_; // newest first, capped
    QHash<QString, QString> watch_names_;
    QMetaObject::Connection fired_conn_; // ScanMonitor::watch_fired, live only while visible
};

} // namespace fincept::screens::widgets
