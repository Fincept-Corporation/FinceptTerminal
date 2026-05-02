#include "screens/crypto_center/CryptoCenterScreen.h"

#include "core/logging/Logger.h"
#include "screens/crypto_center/HoldingsBar.h"
#include "screens/crypto_center/tabs/ActivityTab.h"
#include "screens/crypto_center/tabs/HomeTab.h"
#include "screens/crypto_center/tabs/MarketsTab.h"
#include "screens/crypto_center/tabs/RoadmapTab.h"
#include "screens/crypto_center/tabs/SettingsTab.h"
#include "screens/crypto_center/tabs/StakeTab.h"
#include "screens/crypto_center/tabs/TradeTab.h"
#include "services/wallet/WalletService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString cc_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

} // namespace

CryptoCenterScreen::CryptoCenterScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("cryptoCenterScreen"));
    build_ui();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &CryptoCenterScreen::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &CryptoCenterScreen::on_wallet_disconnected);

    apply_theme();

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        stack_->setCurrentWidget(empty_page_);
    }
}

CryptoCenterScreen::~CryptoCenterScreen() = default;

// ── Layout ─────────────────────────────────────────────────────────────────

void CryptoCenterScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar
    header_ = new QWidget(this);
    header_->setObjectName(QStringLiteral("cryptoCenterHeader"));
    header_->setFixedHeight(40);
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    header_brand_ = new QLabel(QStringLiteral("FINCEPT"));
    header_brand_->setObjectName(QStringLiteral("cryptoCenterBrand"));
    header_separator_ = new QLabel(QStringLiteral("/"));
    header_separator_->setObjectName(QStringLiteral("cryptoCenterSep"));
    header_route_ = new QLabel(QStringLiteral("CRYPTO CENTER"));
    header_route_->setObjectName(QStringLiteral("cryptoCenterRoute"));
    header_status_ = new QLabel(QStringLiteral("● DISCONNECTED"));
    header_status_->setObjectName(QStringLiteral("cryptoCenterHeaderStatusOff"));

    hl->addWidget(header_brand_);
    hl->addWidget(header_separator_);
    hl->addWidget(header_route_);
    hl->addStretch();
    hl->addWidget(header_status_);
    root->addWidget(header_);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName(QStringLiteral("cryptoCenterBody"));
    root->addWidget(stack_, 1);

    build_empty_page();
    build_connected_page();
}

void CryptoCenterScreen::build_empty_page() {
    empty_page_ = new QWidget(stack_);
    auto* layout = new QVBoxLayout(empty_page_);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);
    layout->addStretch(1);

    auto* center_row = new QHBoxLayout;
    center_row->setSpacing(0);
    center_row->addStretch(1);

    empty_panel_ = new QFrame(empty_page_);
    empty_panel_->setObjectName(QStringLiteral("cryptoCenterEmptyPanel"));
    empty_panel_->setFixedWidth(520);
    auto* panel_l = new QVBoxLayout(empty_panel_);
    panel_l->setContentsMargins(0, 0, 0, 0);
    panel_l->setSpacing(0);

    auto* head = new QWidget(empty_panel_);
    head->setObjectName(QStringLiteral("cryptoCenterPanelHead"));
    head->setFixedHeight(34);
    auto* head_l = new QHBoxLayout(head);
    head_l->setContentsMargins(12, 0, 12, 0);
    head_l->setSpacing(0);
    auto* head_title = new QLabel(QStringLiteral("CONNECT WALLET"), head);
    head_title->setObjectName(QStringLiteral("cryptoCenterPanelTitle"));
    head_l->addWidget(head_title);
    head_l->addStretch();
    auto* head_status = new QLabel(QStringLiteral("READY"), head);
    head_status->setObjectName(QStringLiteral("cryptoCenterPanelStatus"));
    head_l->addWidget(head_status);
    panel_l->addWidget(head);

    auto* body = new QWidget(empty_panel_);
    auto* body_l = new QVBoxLayout(body);
    body_l->setContentsMargins(20, 18, 20, 20);
    body_l->setSpacing(10);

    empty_title_ = new QLabel(tr("No wallet connected"), body);
    empty_title_->setObjectName(QStringLiteral("cryptoCenterEmptyTitle"));
    body_l->addWidget(empty_title_);

    empty_lede_ = new QLabel(
        tr("Connect a Solana wallet to view your $FNCPT balance, SOL holdings, "
           "and live USD valuation. Your private keys never leave your wallet."),
        body);
    empty_lede_->setObjectName(QStringLiteral("cryptoCenterEmptyLede"));
    empty_lede_->setWordWrap(true);
    body_l->addWidget(empty_lede_);

    body_l->addSpacing(4);

    empty_security_label_ = new QLabel(QStringLiteral("SECURITY"), body);
    empty_security_label_->setObjectName(QStringLiteral("cryptoCenterCaptionAccent"));
    body_l->addWidget(empty_security_label_);

    empty_security_text_ = new QLabel(
        tr("· public address read-only\n"
           "· no private keys, no seed phrases\n"
           "· local handshake on 127.0.0.1, single-use token\n"
           "· cryptographic signature challenge before connect"),
        body);
    empty_security_text_->setObjectName(QStringLiteral("cryptoCenterSecurityText"));
    body_l->addWidget(empty_security_text_);

    body_l->addSpacing(6);

    connect_button_ = new QPushButton(tr("CONNECT WALLET"), body);
    connect_button_->setObjectName(QStringLiteral("cryptoCenterPrimaryButton"));
    connect_button_->setFixedHeight(28);
    connect_button_->setCursor(Qt::PointingHandCursor);
    body_l->addWidget(connect_button_);

    panel_l->addWidget(body);
    center_row->addWidget(empty_panel_);
    center_row->addStretch(1);
    layout->addLayout(center_row);
    layout->addStretch(2);

    connect(connect_button_, &QPushButton::clicked, this, [this]() {
        fincept::wallet::WalletService::instance().connect_with_dialog(this);
    });

    stack_->addWidget(empty_page_);
}

void CryptoCenterScreen::build_connected_page() {
    connected_page_ = new QWidget(stack_);
    connected_page_->setObjectName(QStringLiteral("cryptoCenterConnectedPage"));
    auto* root = new QVBoxLayout(connected_page_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    holdings_bar_ = new HoldingsBar(connected_page_);
    root->addWidget(holdings_bar_);

    tab_widget_ = new QTabWidget(connected_page_);
    tab_widget_->setObjectName(QStringLiteral("cryptoCenterTabs"));
    tab_widget_->setDocumentMode(true);
    tab_widget_->setTabPosition(QTabWidget::North);
    tab_widget_->tabBar()->setExpanding(false);

    home_tab_ = new HomeTab(tab_widget_);
    trade_tab_ = new TradeTab(tab_widget_);
    activity_tab_ = new ActivityTab(tab_widget_);
    settings_tab_ = new SettingsTab(tab_widget_);

    // Phase 3 ships the real STAKE tab (veFNCPT lock + active locks + tier).
    // Until the fincept_lock Anchor program is deployed, the producers ship
    // mock data with `is_mock=true` and the panel pills read "DEMO".
    stake_tab_ = new StakeTab(tab_widget_);
    // Phase 4 §4.2: internal prediction markets. Ships in demo mode (curated
    // 3-market dataset) until `fincept.markets_endpoint` is configured and
    // the `fincept_market` Anchor program is deployed. The status pill on
    // MarketsListPanel reads "DEMO" until then.
    markets_tab_ = new MarketsTab(tab_widget_);
    // Phase 5 ships the real roadmap dashboard (buyback ticker, supply
    // chart, treasury card). Until the buyback worker is deployed, the
    // service publishes mock data with `is_mock=true` and the panel head
    // pills read "DEMO".
    roadmap_tab_ = new RoadmapTab(tab_widget_);

    tab_widget_->addTab(home_tab_, tr("HOME"));
    tab_widget_->addTab(trade_tab_, tr("TRADE"));
    tab_widget_->addTab(activity_tab_, tr("ACTIVITY"));
    tab_widget_->addTab(settings_tab_, tr("SETTINGS"));
    tab_widget_->addTab(stake_tab_, tr("STAKE"));
    tab_widget_->addTab(markets_tab_, tr("MARKETS"));
    tab_widget_->addTab(roadmap_tab_, tr("ROADMAP"));

    // Bridge HomeTab → TradeTab: clicking a row in the holdings table
    // pre-fills SwapPanel's FROM combo and switches the user to TRADE.
    // (Stage 2A.5.10.)
    connect(home_tab_, &HomeTab::select_token, this,
            [this](const QString& mint) {
                if (mint.isEmpty()) return;
                if (trade_tab_) trade_tab_->set_from_mint(mint);
                const int trade_idx = tab_widget_->indexOf(trade_tab_);
                if (trade_idx >= 0) tab_widget_->setCurrentIndex(trade_idx);
            });

    root->addWidget(tab_widget_, 1);
    stack_->addWidget(connected_page_);
}

// ── Theme ──────────────────────────────────────────────────────────────────

void CryptoCenterScreen::apply_theme() {
    using namespace ui::colors;
    const QString font = cc_font_stack();

    const QString ss = QStringLiteral(
        // Root
        "QWidget#cryptoCenterScreen { background:%1; }"
        "QStackedWidget#cryptoCenterBody { background:%1; }"
        "QWidget#cryptoCenterConnectedPage { background:%1; }"

        // Header bar
        "QWidget#cryptoCenterHeader { background:%2; border-bottom:1px solid %3; }"
        "QLabel#cryptoCenterBrand { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
        "QLabel#cryptoCenterSep { color:%6; font-family:%5; font-size:11px;"
        "  background:transparent; }"
        "QLabel#cryptoCenterRoute { color:%7; font-family:%5; font-size:11px;"
        "  font-weight:600; letter-spacing:1.2px; background:transparent; }"
        "QLabel#cryptoCenterHeaderStatusOff { color:%8; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#cryptoCenterHeaderStatusOn { color:%9; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"

        // Empty-state panel
        "QFrame#cryptoCenterEmptyPanel { background:%2; border:1px solid %3; }"
        "QWidget#cryptoCenterPanelHead { background:%10; border-bottom:1px solid %3; }"
        "QLabel#cryptoCenterPanelTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#cryptoCenterPanelStatus { color:%8; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#cryptoCenterEmptyTitle { color:%7; font-family:%5; font-size:14px;"
        "  font-weight:600; background:transparent; }"
        "QLabel#cryptoCenterEmptyLede { color:%11; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#cryptoCenterCaptionAccent { color:%4; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
        "QLabel#cryptoCenterSecurityText { color:%11; font-family:%5; font-size:11px;"
        "  background:transparent; }"

        // Primary connect button
        "QPushButton#cryptoCenterPrimaryButton { background:rgba(217,119,6,0.10); color:%4;"
        "  border:1px solid %12; font-family:%5; font-size:12px; font-weight:700;"
        "  letter-spacing:1.5px; padding:0 16px; }"
        "QPushButton#cryptoCenterPrimaryButton:hover { background:%4; color:%1; }"

        // Tab bar — Bloomberg-flat with object-name styling (Phase 1.5 §1.5.3)
        "QTabWidget#cryptoCenterTabs::pane { background:%1; border:none;"
        "  border-top:1px solid %3; }"
        "QTabWidget#cryptoCenterTabs QTabBar::tab { background:%2; color:%8;"
        "  font-family:%5; font-size:10px; font-weight:700; letter-spacing:1.4px;"
        "  padding:8px 14px; border-right:1px solid %3; }"
        "QTabWidget#cryptoCenterTabs QTabBar::tab:hover { background:%10; color:%7; }"
        "QTabWidget#cryptoCenterTabs QTabBar::tab:selected { background:%1; color:%4;"
        "  border-bottom:2px solid %4; }"
        "QTabWidget#cryptoCenterTabs QTabBar { background:%2; border-bottom:1px solid %3; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             BORDER_BRIGHT(),   // %6
             TEXT_PRIMARY(),    // %7
             TEXT_TERTIARY(),   // %8
             POSITIVE())        // %9
        .arg(BG_RAISED(),       // %10
             TEXT_SECONDARY(),  // %11
             QStringLiteral("#78350f")); // %12 darker amber

    setStyleSheet(ss);
}

// ── State handlers ─────────────────────────────────────────────────────────

void CryptoCenterScreen::on_wallet_connected(const QString& /*pubkey*/,
                                             const QString& /*label*/) {
    header_status_->setText(QStringLiteral("● CONNECTED"));
    header_status_->setObjectName(QStringLiteral("cryptoCenterHeaderStatusOn"));
    header_status_->style()->unpolish(header_status_);
    header_status_->style()->polish(header_status_);
    stack_->setCurrentWidget(connected_page_);
}

void CryptoCenterScreen::on_wallet_disconnected() {
    header_status_->setText(QStringLiteral("● DISCONNECTED"));
    header_status_->setObjectName(QStringLiteral("cryptoCenterHeaderStatusOff"));
    header_status_->style()->unpolish(header_status_);
    header_status_->style()->polish(header_status_);
    stack_->setCurrentWidget(empty_page_);
}

} // namespace fincept::screens
