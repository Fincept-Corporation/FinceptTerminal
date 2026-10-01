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

#include <QEvent>
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

    OptionChainTable* table() const { return table_; }

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

    /// Screen-level lifecycle, driven by FnoScreen. The chain topic must stay
    /// subscribed for as long as the F&O SCREEN is visible, not just this sub-tab:
    /// the OI / Builder / Multi-Straddle / Screener tabs all read the chain this tab
    /// subscribes to, and the hub only refreshes topics with a concrete subscriber —
    /// so switching to any of them used to freeze every chain after the first view.
    void on_screen_shown();
    void on_screen_hidden();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_broker_changed(const QString& broker_id);
    void on_underlying_changed(const QString& underlying);
    void on_expiry_changed(const QString& expiry);
    void on_refresh_clicked();
    /// Right-click Buy/Sell on a chain leg → build a 1-lot market order, confirm,
    /// and route to the connected broker (live or paper per its trading mode).
    void on_order_requested(qint64 token, double strike, bool is_call, bool is_buy);

  private:
    void retranslateUi();
    void rebuild_picker_for_broker(const QString& broker_id, bool keep_selection);
    void rebuild_expiries_for_underlying(const QString& broker_id, const QString& underlying, bool keep_selection);
    void resubscribe();
    /// Unsubscribe the chain topic + per-leg tick pattern (hub + error channel).
    void drop_subscription();
    /// True while the owning FnoScreen is on screen (this tab may be a hidden page).
    bool owning_screen_visible() const;
    void show_empty_state(const QString& message);
    void hide_empty_state();

    QString current_topic() const;
    /// True when the table currently holds rows belonging to `topic`'s
    /// (broker, underlying, expiry) — i.e. what's on screen is not a leftover
    /// from a previous picker selection.
    bool holds_rows_for(const QString& topic) const;

    FnoHeaderBar* header_ = nullptr;
    OptionChainTable* table_ = nullptr;
    QLabel* empty_label_ = nullptr;
    class QStackedLayout* body_stack_ = nullptr; // owns table_ + empty_label_

    /// Topic we're currently subscribed to. Empty when not subscribed.
    QString active_topic_;

    /// Per-leg live-tick pattern we're subscribed to (`option:tick:<broker>:*`),
    /// used to patch single cells from the WS feed. Empty when not subscribed.
    QString tick_pattern_;

    /// Whether the chain subscription is wanted: this tab is shown, or the owning F&O
    /// screen is (see on_screen_shown). Gates re-subscribe paths triggered by combo
    /// changes.
    bool is_visible_ = false;

    /// Last-applied selection — used to avoid clobbering user choice on
    /// re-population when broker pickers refresh.
    QString last_broker_;
    QString last_underlying_;
    QString last_expiry_;
};

} // namespace fincept::screens::fno
