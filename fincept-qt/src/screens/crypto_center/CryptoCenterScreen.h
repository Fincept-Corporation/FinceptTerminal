#pragma once

#include <QString>
#include <QWidget>

class QFrame;
class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QStackedWidget;
class QTabWidget;

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
/// Phase 2 layout: persistent HoldingsBar above a QTabWidget. Tabs:
///   HOME (Phase 1 panels)
///   TRADE (Stage 2.2)
///   ACTIVITY (Stage 2.4)
///   SETTINGS (mode toggle + Helius key + slippage)
///   STAKE / MARKETS / ROADMAP — placeholders shown via ComingSoonTab.
///
/// The screen itself owns no data subscriptions — those live in HoldingsBar
/// and the individual tabs. CryptoCenterScreen orchestrates the connect /
/// disconnect navigation between the empty page and the connected page only.
class CryptoCenterScreen : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoCenterScreen(QWidget* parent = nullptr);
    ~CryptoCenterScreen() override;

  private:
    void build_ui();
    void build_empty_page();
    void build_connected_page();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();

    // Header
    QWidget* header_ = nullptr;
    QLabel* header_brand_ = nullptr;
    QLabel* header_route_ = nullptr;
    QLabel* header_separator_ = nullptr;
    QLabel* header_status_ = nullptr;

    // Stack
    QStackedWidget* stack_ = nullptr;

    // Empty state
    QWidget* empty_page_ = nullptr;
    QFrame* empty_panel_ = nullptr;
    QLabel* empty_title_ = nullptr;
    QLabel* empty_lede_ = nullptr;
    QLabel* empty_security_label_ = nullptr;
    QLabel* empty_security_text_ = nullptr;
    QPushButton* connect_button_ = nullptr;

    // Connected state
    QWidget* connected_page_ = nullptr;
    HoldingsBar* holdings_bar_ = nullptr;
    QTabWidget* tab_widget_ = nullptr;

    HomeTab* home_tab_ = nullptr;
    TradeTab* trade_tab_ = nullptr;
    ActivityTab* activity_tab_ = nullptr;
    SettingsTab* settings_tab_ = nullptr;
    StakeTab* stake_tab_ = nullptr;       // Phase 3 — veFNCPT lock + tier
    MarketsTab* markets_tab_ = nullptr;   // Phase 4 — internal prediction markets
    RoadmapTab* roadmap_tab_ = nullptr;   // Phase 5 — buyback & burn dashboard
};

} // namespace fincept::screens
