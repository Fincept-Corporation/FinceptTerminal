#pragma once
// OpenPositionsWidget — dashboard tile that mirrors one broker account's
// open positions, sourced from the hub topic `broker:<id>:<account>:positions`
// (produced by DataStreamManager, Phase 7).
//
// Config shape (persisted per tile):
//   { "account_id": "<uuid>" }   // picked via the gear-icon dialog
//
// If account_id is missing or stale, the widget auto-selects the first active
// account from AccountManager and shows an inline hint that config is required.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QTableWidget>
#include <QVector>

namespace fincept::screens::widgets {

class OpenPositionsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit OpenPositionsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const QVector<trading::BrokerPosition>& rows);
    void ensure_stream_running();
    void hub_resubscribe();
    void hub_unsubscribe_all();
    QString resolve_account_id() const; // falls back to first active account
    QString positions_topic() const;
    /// Account name in the header, or why the broker session is unusable.
    void update_session_state();
    /// No account: "No broker account connected" over the tile (BaseWidget helper).
    void show_no_account_state();

    QString account_id_;
    QString broker_id_;

    QTableWidget* table_ = nullptr;
    QLabel* header_hint_ = nullptr; // top label: account name / config required

    bool hub_active_ = false;
    bool showing_empty_ = false; // table currently shows the "No open positions" row
    /// The rows on screen — re-rendered on a theme change (P&L colours live in the items).
    QVector<trading::BrokerPosition> positions_;
};

} // namespace fincept::screens::widgets
