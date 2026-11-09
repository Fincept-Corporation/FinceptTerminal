#pragma once
// ConnectionHealthWidget — one compact panel answering "are my data paths up?".
//
// Only sources that expose a status the terminal can actually read:
//   FINCEPT API   DataHub `system:health` (ConnectivityService). The tile
//                 registers the producer itself, so it works without the
//                 dashboard status bar.
//   PYTHON        PythonSetupManager — core ready / installing / last run failed.
//   BROKERS       AccountManager — each active account's ConnectionState and
//                 error; DataStreamManager — whether its stream runs; DataHub
//                 last publish on broker:<id>:<acct>:{positions,orders,balance,holdings}
//                 (read only — the tile never subscribes, so it triggers no fetch).
//   EXCHANGE WS   ExchangeSessionManager — each live session's WebSocket
//                 (spawned / connected / dropped / stopped).
//
// No refresh timer (D3): the API probe cadence belongs to the hub; everything
// else re-renders on the owning service's state-change signals while visible.
// Double-click a row to open the screen that manages that source.

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QVector>

class QLabel;
class QTableWidget;

namespace fincept::screens::widgets {

class ConnectionHealthWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit ConnectionHealthWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    enum class Tone { Pos, Warn, Neg, Dim };
    struct Row {
        QString source;
        QString state;
        QString detail;
        QString tooltip;
        QString screen_id; // nav.switch_screen target on double-click; empty = none
        Tone tone = Tone::Dim;
    };

    void apply_styles();
    void hub_subscribe();
    void on_health(const QVariant& v);
    /// Coalesced render, skipped while hidden (showEvent renders on return).
    void request_render();
    void render();
    void set_summary(const QString& text, const QString& tone);
    void append_api_row(QVector<Row>& rows) const;
    void append_python_row(QVector<Row>& rows) const;
    void append_broker_rows(QVector<Row>& rows) const;
    void append_exchange_rows(QVector<Row>& rows) const;
    void on_row_double_clicked(int row);

    QWidget* body_ = nullptr;
    QLabel* summary_ = nullptr;
    QTableWidget* table_ = nullptr;
    QJsonObject last_health_; // latest system:health payload; empty until the first probe
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
