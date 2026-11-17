#pragma once

#include "screens/common/ISectionLinked.h"

#include <QEvent>
#include <QString>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTabWidget;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class ActivityTab;
class HoldingsBar;
class HomeTab;
class TradeTab;
class SettingsTab;
class RoadmapTab;
class StakeTab;
class MarketsTab;

/// Crypto Center — Obsidian-themed wallet hub.
///
/// Layout: header, then the HoldingsBar (wallet connected) or a one-line
/// "no wallet" strip with CONNECT WALLET (and RETRY when the saved wallet
/// could not be read), then a QTabWidget that is always shown:
///   HOME (Phase 1 panels)            — needs a wallet
///   TRADE (Stage 2.2)                — needs a wallet
///   ACTIVITY (Stage 2.4)             — needs a wallet
///   SETTINGS (mode toggle + Helius key + slippage)
///   STAKE (veFNCPT lock calculator + tiers) / MARKETS (prediction markets, demo mode) / ROADMAP (buyback dashboard).
///
/// Without a wallet the wallet-bound tabs show a ui::EmptyState (what the tab
/// shows, the security facts, CONNECT WALLET — the user's click, never an
/// automatic connect); a saved wallet that could not be read shows RETRY
/// (re-runs the restore) with CONNECT WALLET as the alternative. MARKETS opens
/// first without a wallet, HOME with one.
///
/// Each tab is built on its first visit (only the visible one is constructed);
/// the screen owns no data subscriptions — those live in HoldingsBar and the
/// individual tabs.
class CryptoCenterScreen : public QWidget, public ISectionLinked {
    Q_OBJECT
  public:
    explicit CryptoCenterScreen(QWidget* parent = nullptr);
    ~CryptoCenterScreen() override;

    /// ISectionLinked — the tabs "home" "trade" "activity" "settings" "stake" "markets" "roadmap"
    /// (selected as a click does; wallet-bound tabs show their connect page without a wallet —
    /// nothing connects, signs or trades). focus unused.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void changeEvent(QEvent* event) override;

  private:
    /// Tab order of tab_widget_.
    enum Tab : int { kHome = 0, kTrade, kActivity, kSettings, kStake, kMarkets, kRoadmap, kTabCount };

    void build_ui();
    void build_wallet_strip();
    void build_tabs();
    void apply_theme();
    void retranslateUi();

    static bool is_wallet_tab(int index);
    QString tab_title(int index) const;
    /// The real tab behind page `index` (nullptr until built).
    QWidget* tab_content(int index) const;
    /// Builds the real tab on its page (first visit only).
    QWidget* build_tab(int index);
    /// Shows page `index`'s content — or, for a wallet-bound tab without a
    /// wallet, its connect / unreadable page.
    void show_tab_content(int index);
    /// Builds (first time) and fills the connect / unreadable page of a wallet-bound tab.
    void apply_wallet_gate(int index);
    /// Programmatic tab change (does not count as the user's choice).
    void select_tab(int index);
    /// Header status, holdings bar / no-wallet strip and the wallet-bound pages
    /// from the wallet service's current state.
    void refresh_wallet_views();
    void ensure_holdings_bar();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_connect_failed(const QString& reason);
    /// The saved wallet exists but could not be read: say so, offer RETRY.
    void on_restore_failed(const QString& reason);
    /// CONNECT WALLET (strip or page): the connect dialog asks the user.
    void request_connect();
    /// RETRY: read the saved wallet again.
    void request_restore_retry();

    // Header
    QWidget* header_ = nullptr;
    QLabel* header_brand_ = nullptr;
    QLabel* header_route_ = nullptr;
    QLabel* header_separator_ = nullptr;
    QLabel* header_status_ = nullptr;

    // No-wallet strip (in place of the holdings bar while no wallet is connected)
    QWidget* wallet_strip_ = nullptr;
    QLabel* wallet_strip_text_ = nullptr;
    QPushButton* strip_retry_button_ = nullptr;   ///< saved wallet unreadable only
    QPushButton* strip_connect_button_ = nullptr;

    HoldingsBar* holdings_bar_ = nullptr; ///< built when a wallet first connects
    QTabWidget* tab_widget_ = nullptr;
    QWidget* tab_pages_[kTabCount] = {};                      ///< page per tab (holds content or gate)
    fincept::ui::EmptyState* wallet_gates_[kTabCount] = {};   ///< wallet-bound tabs only, built on first need

    QString last_connect_error_;   ///< reason the last connect attempt failed (cleared on the next attempt)
    bool tab_chosen_by_user_ = false;
    bool selecting_tab_ = false;

    HomeTab* home_tab_ = nullptr;
    TradeTab* trade_tab_ = nullptr;
    ActivityTab* activity_tab_ = nullptr;
    SettingsTab* settings_tab_ = nullptr;
    StakeTab* stake_tab_ = nullptr;     // Phase 3 — veFNCPT lock + tier (not live: coming soon)
    MarketsTab* markets_tab_ = nullptr; // Phase 4 — internal prediction markets
    RoadmapTab* roadmap_tab_ = nullptr; // Phase 5 — buyback & burn dashboard
};

} // namespace fincept::screens
