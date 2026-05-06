#pragma once
// ChainSubTab — F&O Chain sub-tab content widget.
//
// Composition:
//   ┌─ FnoHeaderBar ──────────────────────────────────────────────────┐
//   │ Broker | Underlying | Expiry | Refresh   Spot ATM PCR ...       │
//   ├──────────────────────────────────────────────────────────────────┤
//   │                       OptionChainTable                           │
//   └──────────────────────────────────────────────────────────────────┘
//
// Lifecycle:
//   - showEvent: subscribes to current option:chain:* topic on the hub.
//   - hideEvent: unsubscribes (P3 visibility-driven lifecycle, D3).
//   - Combo changes: tear down old subscription, subscribe to new topic,
//     fire one-shot DataHub::request(topic, force=true) for cold start.
//
// State:
//   - Chain snapshots arrive via DataHub. Empty state when no broker is
//     connected, no instruments are loaded for the selected broker, or
//     the producer publishes an error.

#include "services/options/OptionChainTypes.h"

#include <QLabel>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

namespace fincept::screens::fno {

class FnoHeaderBar;
class OptionChainTable;

class ChainSubTab : public QWidget {
    Q_OBJECT
  public:
    explicit ChainSubTab(QWidget* parent = nullptr);
    ~ChainSubTab() override;

    // Lightweight save/restore for the F&O screen aggregator.
    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// Group-link entry point — request the picker switch to `underlying`
    /// when it's already in the broker's loaded instrument set. No-op
    /// otherwise. Used by FnoScreen::on_group_symbol_changed (Yellow sync).
    void request_underlying(const QString& underlying);

    /// Currently selected underlying as carried by the picker (empty when
    /// no broker is connected). Used by FnoScreen::current_symbol() so
    /// other Yellow-group panels can follow.
    QString active_underlying() const;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_broker_changed(const QString& broker_id);
    void on_underlying_changed(const QString& underlying);
    void on_expiry_changed(const QString& expiry);
    void on_refresh_clicked();

  private:
    void rebuild_picker_for_broker(const QString& broker_id, bool keep_selection);
    void rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying,
                                         bool keep_selection);
    void resubscribe();
    void show_empty_state(const QString& message);
    void hide_empty_state();

    QString current_topic() const;

    FnoHeaderBar* header_ = nullptr;
    OptionChainTable* table_ = nullptr;
    QLabel* empty_label_ = nullptr;
    class QStackedLayout* body_stack_ = nullptr;  // owns table_ + empty_label_

    /// Topic we're currently subscribed to. Empty when not subscribed.
    QString active_topic_;

    /// Whether the widget is currently visible — gates re-subscribe paths
    /// triggered by combo changes from non-visible state.
    bool is_visible_ = false;

    /// Last-applied selection — used to avoid clobbering user choice on
    /// re-population when broker pickers refresh.
    QString last_broker_;
    QString last_underlying_;
    QString last_expiry_;
};

} // namespace fincept::screens::fno
