#include "screens/crypto_center/CryptoCenterScreen.h"

#include "core/logging/Logger.h"
#include "screens/common/CommandRows.h"
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
#include "ui/widgets/EmptyState.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString cc_font_stack() {
    return QString(ui::fonts::DATA_FAMILY);
}

void cc_set_object_name(QWidget* w, const QString& name) {
    if (!w || w->objectName() == name)
        return;
    w->setObjectName(name);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

CryptoCenterScreen::CryptoCenterScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("cryptoCenterScreen"));
    apply_theme(); // selector stylesheet before the children exist
    build_ui();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &CryptoCenterScreen::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &CryptoCenterScreen::on_wallet_disconnected);
    // connect_failed had no listener: a rejected signature, a browser callback
    // that never arrived or a failed loopback bind all closed the dialog and
    // left the page looking untouched.
    connect(&svc, &fincept::wallet::WalletService::connect_failed, this, &CryptoCenterScreen::on_connect_failed);
    // The startup restore usually ran before this screen existed: its result
    // (restore_error()) is read by refresh_wallet_views() below.
    connect(&svc, &fincept::wallet::WalletService::restore_failed, this, &CryptoCenterScreen::on_restore_failed);

    // Opening tab: HOME with a wallet (as before), MARKETS without one — it has
    // content; the wallet-bound tabs would only show their connect page.
    select_tab(svc.is_connected() ? kHome : kMarkets);
    refresh_wallet_views();
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

    header_brand_ = new QLabel(QStringLiteral("FINCEPT"), header_);
    header_brand_->setObjectName(QStringLiteral("cryptoCenterBrand"));
    header_separator_ = new QLabel(QStringLiteral("/"), header_);
    header_separator_->setObjectName(QStringLiteral("cryptoCenterSep"));
    header_route_ = new QLabel(tr("CRYPTO CENTER"), header_);
    header_route_->setObjectName(QStringLiteral("cryptoCenterRoute"));
    header_status_ = new QLabel(tr("● DISCONNECTED"), header_);
    header_status_->setObjectName(QStringLiteral("cryptoCenterHeaderStatusOff"));

    hl->addWidget(header_brand_);
    hl->addWidget(header_separator_);
    hl->addWidget(header_route_);
    hl->addStretch();
    hl->addWidget(header_status_);
    root->addWidget(header_);

    build_wallet_strip();
    root->addWidget(wallet_strip_);
    // The HoldingsBar is inserted below the strip when a wallet first connects
    // (ensure_holdings_bar): without a wallet it would only show dashes.

    build_tabs();
    root->addWidget(tab_widget_, 1);
}

void CryptoCenterScreen::build_wallet_strip() {
    wallet_strip_ = new QWidget(this);
    wallet_strip_->setObjectName(QStringLiteral("cryptoCenterWalletStrip"));
    auto* sl = new QHBoxLayout(wallet_strip_);
    sl->setContentsMargins(14, 6, 14, 6);
    sl->setSpacing(8);

    wallet_strip_text_ = new QLabel(wallet_strip_); // text set by refresh_wallet_views()
    wallet_strip_text_->setObjectName(QStringLiteral("cryptoCenterWalletStripText"));
    wallet_strip_text_->setWordWrap(true);
    sl->addWidget(wallet_strip_text_, 1);

    strip_retry_button_ = new QPushButton(tr("RETRY"), wallet_strip_);
    strip_retry_button_->setObjectName(QStringLiteral("cryptoCenterSecondaryButton"));
    strip_retry_button_->setMinimumHeight(26);
    strip_retry_button_->setCursor(Qt::PointingHandCursor);
    strip_retry_button_->setToolTip(tr("Read the saved wallet again"));
    strip_retry_button_->hide();
    sl->addWidget(strip_retry_button_);

    strip_connect_button_ = new QPushButton(tr("CONNECT WALLET"), wallet_strip_);
    strip_connect_button_->setObjectName(QStringLiteral("cryptoCenterPrimaryButton"));
    strip_connect_button_->setProperty("fincept_agent_risk", "account");
    strip_connect_button_->setMinimumHeight(26);
    strip_connect_button_->setCursor(Qt::PointingHandCursor);
    sl->addWidget(strip_connect_button_);

    connect(strip_connect_button_, &QPushButton::clicked, this, &CryptoCenterScreen::request_connect);
    connect(strip_retry_button_, &QPushButton::clicked, this, &CryptoCenterScreen::request_restore_retry);
}

void CryptoCenterScreen::build_tabs() {
    tab_widget_ = new QTabWidget(this);
    tab_widget_->setObjectName(QStringLiteral("cryptoCenterTabs"));
    tab_widget_->setDocumentMode(true);
    tab_widget_->setTabPosition(QTabWidget::North);
    tab_widget_->tabBar()->setExpanding(false);
    tab_widget_->tabBar()->setUsesScrollButtons(true);
    // Tab pages are built inside the tab widget's page stack — the parent
    // addTab() gives them. Built under the tab widget itself, each finished
    // page was moved into the stack, re-computing the stylesheet of every
    // widget in it.
    QWidget* page_parent = tab_widget_->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                                   Qt::FindDirectChildrenOnly);
    if (!page_parent)
        page_parent = tab_widget_;

    // One light page per tab; the tab itself is built on its first visit
    // (build_tab) — building all seven up front made opening this screen take
    // most of a second.
    for (int i = 0; i < kTabCount; ++i) {
        auto* page = new QWidget(page_parent);
        page->setObjectName(QStringLiteral("cryptoCenterTabPage"));
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(0);
        tab_pages_[i] = page;
        tab_widget_->addTab(page, tab_title(i));
    }

    connect(tab_widget_, &QTabWidget::currentChanged, this, [this](int index) {
        if (!selecting_tab_)
            tab_chosen_by_user_ = true;
        show_tab_content(index);
        // The holdings bar's RPC chip only re-read its provider on show/connect;
        // refresh it when the user leaves SETTINGS (where the Helius key is saved).
        if (holdings_bar_)
            holdings_bar_->refresh_rpc_indicator();
    });
}

bool CryptoCenterScreen::is_wallet_tab(int index) {
    return index == kHome || index == kTrade || index == kActivity;
}

QString CryptoCenterScreen::tab_title(int index) const {
    switch (index) {
        case kHome:
            return tr("HOME");
        case kTrade:
            return tr("TRADE");
        case kActivity:
            return tr("ACTIVITY");
        case kSettings:
            return tr("SETTINGS");
        case kStake:
            return tr("STAKE");
        case kMarkets:
            return tr("MARKETS");
        case kRoadmap:
            return tr("ROADMAP");
        default:
            return {};
    }
}

QWidget* CryptoCenterScreen::tab_content(int index) const {
    switch (index) {
        case kHome:
            return home_tab_;
        case kTrade:
            return trade_tab_;
        case kActivity:
            return activity_tab_;
        case kSettings:
            return settings_tab_;
        case kStake:
            return stake_tab_;
        case kMarkets:
            return markets_tab_;
        case kRoadmap:
            return roadmap_tab_;
        default:
            return nullptr;
    }
}

QWidget* CryptoCenterScreen::build_tab(int index) {
    if (index < 0 || index >= kTabCount)
        return nullptr;
    if (QWidget* existing = tab_content(index))
        return existing;
    QWidget* page = tab_pages_[index];
    QWidget* w = nullptr;
    switch (index) {
        case kHome:
            home_tab_ = new HomeTab(page);
            // Bridge HomeTab → TradeTab: clicking a row in the holdings table
            // pre-fills SwapPanel's FROM combo and switches the user to TRADE.
            // (Stage 2A.5.10.)
            connect(home_tab_, &HomeTab::select_token, this, [this](const QString& mint) {
                if (mint.isEmpty())
                    return;
                build_tab(kTrade);
                if (trade_tab_)
                    trade_tab_->set_from_mint(mint);
                tab_widget_->setCurrentIndex(kTrade);
            });
            w = home_tab_;
            break;
        case kTrade:
            trade_tab_ = new TradeTab(page);
            w = trade_tab_;
            break;
        case kActivity:
            activity_tab_ = new ActivityTab(page);
            w = activity_tab_;
            break;
        case kSettings:
            settings_tab_ = new SettingsTab(page);
            w = settings_tab_;
            break;
        case kStake:
            // STAKE tab (veFNCPT lock calculator + active locks + planned tiers).
            // On-chain staking is not live (StakingService::staking_live() == false):
            // LOCK / EXTEND / WITHDRAW are disabled and labelled COMING SOON, and no
            // lock position or tier is ever shown as the wallet's own. The
            // calculator works without a wallet ("Connect a wallet to lock $FNCPT.").
            stake_tab_ = new StakeTab(page);
            w = stake_tab_;
            break;
        case kMarkets:
            // Phase 4 §4.2: internal prediction markets. Ships in demo mode (curated
            // 3-market dataset) until `fincept.markets_endpoint` is configured and
            // the `fincept_market` Anchor program is deployed. MarketsListPanel shows
            // an amber DEMO pill and prefixes every sample row with "SAMPLE ·".
            markets_tab_ = new MarketsTab(page);
            w = markets_tab_;
            break;
        case kRoadmap:
            // Phase 5 ships the real roadmap dashboard (buyback ticker, supply
            // chart, treasury card). Until the buyback worker is deployed, the
            // service publishes mock data with `is_mock=true` and the panel head
            // pills read "DEMO".
            roadmap_tab_ = new RoadmapTab(page);
            w = roadmap_tab_;
            break;
        default:
            break;
    }
    if (w && page->layout())
        page->layout()->addWidget(w);
    return w;
}

void CryptoCenterScreen::show_tab_content(int index) {
    if (index < 0 || index >= kTabCount)
        return;
    const bool connected = fincept::wallet::WalletService::instance().is_connected();
    if (is_wallet_tab(index) && !connected) {
        // The tab itself is not built (or stays hidden) without a wallet.
        if (QWidget* content = tab_content(index))
            content->hide();
        apply_wallet_gate(index);
        return;
    }
    if (wallet_gates_[index])
        wallet_gates_[index]->hide();
    if (QWidget* content = build_tab(index))
        content->show();
}

void CryptoCenterScreen::apply_wallet_gate(int index) {
    if (!is_wallet_tab(index))
        return;
    ui::EmptyState*& gate = wallet_gates_[index];
    if (!gate) {
        gate = new ui::EmptyState(tab_pages_[index]);
        if (tab_pages_[index]->layout())
            tab_pages_[index]->layout()->addWidget(gate);
        // RETRY when the saved wallet could not be read, else CONNECT WALLET.
        connect(gate, &ui::EmptyState::primary_clicked, this, [this]() {
            if (!fincept::wallet::WalletService::instance().restore_error().isEmpty())
                request_restore_retry();
            else
                request_connect();
        });
        connect(gate, &ui::EmptyState::secondary_clicked, this, &CryptoCenterScreen::request_connect);
    }

    QString what;
    ui::LoadingSkeleton::Shape shape = ui::LoadingSkeleton::Shape::Table;
    switch (index) {
        case kHome:
            what = tr("HOME shows your wallet address, your SOL and $FNCPT balances with their live USD value, "
                      "and every token the wallet holds.");
            shape = ui::LoadingSkeleton::Shape::Cards;
            break;
        case kTrade:
            what = tr("TRADE swaps SOL and $FNCPT through PumpSwap. Every swap is shown to you first and signed "
                      "in your own wallet.");
            shape = ui::LoadingSkeleton::Shape::Form;
            break;
        default: // kActivity
            what = tr("ACTIVITY lists the wallet's latest transactions — swaps, sends and receives — each with a "
                      "link to Solscan.");
            break;
    }

    const QString restore_error = fincept::wallet::WalletService::instance().restore_error();
    ui::EmptyState::Spec spec;
    spec.illustration = true;
    spec.illustration_shape = shape;
    // The security facts of the connect handshake (formerly the SECURITY list of the connect card).
    spec.details_heading = tr("SECURITY");
    spec.details = {
        {tr("Address"), tr("public address, read-only")},
        {tr("Keys"), tr("no private keys, no seed phrases")},
        {tr("Handshake"), tr("local on 127.0.0.1, single-use token")},
        {tr("Proof"), tr("signature challenge before connect")},
    };
    if (!restore_error.isEmpty()) {
        // Not "no wallet": the saved one exists but could not be read. Nothing was
        // changed; connecting another wallet stays the user's explicit choice.
        spec.glyph = QStringLiteral("!");
        spec.glyph_tone = ui::EmptyState::Tone::Warning;
        spec.title = tr("Saved wallet could not be read");
        spec.message =
            tr("%1 Your saved wallet was not changed — retry reading it, or connect a wallet.").arg(restore_error);
        spec.primary = tr("RETRY");
        spec.secondary = tr("CONNECT WALLET");
    } else {
        spec.glyph = QStringLiteral("◇"); // ◇
        spec.glyph_tone = ui::EmptyState::Tone::Accent;
        spec.title = tr("Connect a wallet");
        spec.message = what + QLatin1Char(' ') + tr("Your private keys never leave your wallet.");
        spec.primary = tr("CONNECT WALLET");
    }
    if (!last_connect_error_.isEmpty()) {
        spec.note = tr("Connection not completed: %1").arg(last_connect_error_);
        spec.note_tone = ui::EmptyState::Tone::Danger;
    }
    gate->apply(spec);
    gate->show();
}

void CryptoCenterScreen::select_tab(int index) {
    selecting_tab_ = true;
    tab_widget_->setCurrentIndex(index);
    selecting_tab_ = false;
    show_tab_content(index); // no currentChanged when it already was the current tab
}

void CryptoCenterScreen::ensure_holdings_bar() {
    if (holdings_bar_)
        return;
    holdings_bar_ = new HoldingsBar(this);
    if (auto* root = qobject_cast<QVBoxLayout*>(layout()))
        root->insertWidget(root->indexOf(wallet_strip_) + 1, holdings_bar_);
}

// ── Theme ──────────────────────────────────────────────────────────────────

void CryptoCenterScreen::apply_theme() {
    using namespace ui::colors;
    const QString font = cc_font_stack();

    const QString ss = QString(
                           // Root
                           "QWidget#cryptoCenterScreen { background:%1; }"
                           "QWidget#cryptoCenterTabPage { background:%1; }"

                           // Header bar
                           "QWidget#cryptoCenterHeader { background:%2; border-bottom:1px solid %3; }"
                           "QLabel#cryptoCenterBrand { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                           "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
                           "QLabel#cryptoCenterSep { color:%6; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                           "  background:transparent; }"
                           "QLabel#cryptoCenterRoute { color:%7; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                           "  font-weight:600; letter-spacing:1.2px; background:transparent; }"
                           "QLabel#cryptoCenterHeaderStatusOff { color:%8; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                           "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
                           "QLabel#cryptoCenterHeaderStatusOn { color:%9; font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                           "  font-weight:700; letter-spacing:1.2px; background:transparent; }"

                           // No-wallet strip (in place of the holdings bar)
                           "QWidget#cryptoCenterWalletStrip { background:%2; border-bottom:1px solid %3; }"
                           "QLabel#cryptoCenterWalletStripText { color:%11; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                           "  background:transparent; }"

                           // Primary (connect) and secondary (retry) buttons
                           "QPushButton#cryptoCenterPrimaryButton { background:" + ui::colors::alpha(AMBER, 0.10) + "; color:%4;"
                           "  border:1px solid %12; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                           "  letter-spacing:1.5px; padding:0 14px; }"
                           "QPushButton#cryptoCenterPrimaryButton:hover { background:%4; color:" + QString(TEXT_ON_ACCENT()) + "; }"
                           "QPushButton#cryptoCenterSecondaryButton { background:transparent; color:%7;"
                           "  border:1px solid %6; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700;"
                           "  letter-spacing:1.5px; padding:0 14px; }"
                           "QPushButton#cryptoCenterSecondaryButton:hover { color:%4; border-color:%4; }"

                           // Tab bar — Fincept-flat with object-name styling (Phase 1.5 §1.5.3)
                           "QTabWidget#cryptoCenterTabs::pane { background:%1; border:none;"
                           "  border-top:1px solid %3; }"
                           "QTabWidget#cryptoCenterTabs QTabBar::tab { background:%2; color:%8;"
                           "  font-family:%5; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1.4px;"
                           "  padding:8px 14px; border-right:1px solid %3; }"
                           "QTabWidget#cryptoCenterTabs QTabBar::tab:hover { background:%10; color:%7; }"
                           "QTabWidget#cryptoCenterTabs QTabBar::tab:selected { background:%1; color:%4;"
                           "  border-bottom:2px solid %4; }"
                           "QTabWidget#cryptoCenterTabs QTabBar { background:%2; border-bottom:1px solid %3; }")
                           .arg(BG_BASE(),                  // %1
                                BG_SURFACE(),               // %2
                                BORDER_DIM(),               // %3
                                AMBER(),                    // %4
                                font,                       // %5
                                BORDER_BRIGHT(),            // %6
                                TEXT_PRIMARY(),             // %7
                                TEXT_TERTIARY(),            // %8
                                SUCCESS())                  // %9
                           .arg(BG_RAISED(),                // %10
                                TEXT_SECONDARY(),           // %11
                                AMBER_DIM());               // %12 darker amber

    setStyleSheet(ss);
}

// ── State handlers ─────────────────────────────────────────────────────────

void CryptoCenterScreen::refresh_wallet_views() {
    const auto& svc = fincept::wallet::WalletService::instance();
    const bool connected = svc.is_connected();
    const bool unreadable = !connected && !svc.restore_error().isEmpty();

    header_status_->setText(connected    ? tr("● CONNECTED")
                            : unreadable ? tr("● SAVED WALLET UNREADABLE")
                                         : tr("● DISCONNECTED"));
    cc_set_object_name(header_status_, connected ? QStringLiteral("cryptoCenterHeaderStatusOn")
                                                 : QStringLiteral("cryptoCenterHeaderStatusOff"));

    if (connected) {
        ensure_holdings_bar();
        holdings_bar_->show();
        wallet_strip_->hide();
    } else {
        if (holdings_bar_)
            holdings_bar_->hide(); // its own handlers already dropped the wallet's feeds
        QString text = unreadable
                           ? tr("The saved wallet could not be read. MARKETS, STAKE, ROADMAP and SETTINGS work "
                                "without a wallet; HOME, TRADE and ACTIVITY need one.")
                           : tr("No wallet connected. MARKETS, STAKE, ROADMAP and SETTINGS work without one; HOME, "
                                "TRADE and ACTIVITY need a wallet.");
        if (!last_connect_error_.isEmpty())
            text += QLatin1Char(' ') + tr("Connection not completed: %1").arg(last_connect_error_);
        wallet_strip_text_->setText(text);
        strip_retry_button_->setVisible(unreadable);
        wallet_strip_->show();
    }

    // Wallet-bound tabs: without a wallet their built content is hidden behind the
    // connect / unreadable page (pages already built are refreshed; the others are
    // filled when shown). The current tab is brought in line either way.
    const int current = tab_widget_->currentIndex();
    for (int i : {int(kHome), int(kTrade), int(kActivity)}) {
        if (connected || i == current)
            continue;
        if (QWidget* content = tab_content(i))
            content->hide();
        if (wallet_gates_[i])
            apply_wallet_gate(i);
    }
    show_tab_content(current);
}

void CryptoCenterScreen::on_connect_failed(const QString& reason) {
    last_connect_error_ = reason;
    refresh_wallet_views();
}

void CryptoCenterScreen::on_restore_failed(const QString& /*reason*/) {
    // WalletService::restore_error() keeps the sentence; the pages read it.
    refresh_wallet_views();
}

void CryptoCenterScreen::on_wallet_connected(const QString& /*pubkey*/, const QString& /*label*/) {
    last_connect_error_.clear();
    // Opened without a wallet and the user has not picked a tab since: show the
    // wallet now that it is there (HOME, as when opening with a wallet).
    if (!tab_chosen_by_user_ && !is_wallet_tab(tab_widget_->currentIndex()))
        select_tab(kHome);
    refresh_wallet_views();
}

void CryptoCenterScreen::on_wallet_disconnected() {
    refresh_wallet_views();
}

void CryptoCenterScreen::request_connect() {
    last_connect_error_.clear();
    refresh_wallet_views();
    fincept::wallet::WalletService::instance().connect_with_dialog(this);
}

void CryptoCenterScreen::request_restore_retry() {
    last_connect_error_.clear();
    // Synchronous: emits wallet_connected or restore_failed — or nothing when the
    // read now works and no wallet is saved, which refresh_wallet_views() covers.
    fincept::wallet::WalletService::instance().retry_restore();
    refresh_wallet_views();
}

// ── i18n ───────────────────────────────────────────────────────────────────

void CryptoCenterScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CryptoCenterScreen::retranslateUi() {
    // Header (brand + separator are not translatable).
    if (header_route_)
        header_route_->setText(tr("CRYPTO CENTER"));
    if (strip_retry_button_) {
        strip_retry_button_->setText(tr("RETRY"));
        strip_retry_button_->setToolTip(tr("Read the saved wallet again"));
    }
    if (strip_connect_button_)
        strip_connect_button_->setText(tr("CONNECT WALLET"));
    if (tab_widget_) {
        for (int i = 0; i < kTabCount; ++i)
            tab_widget_->setTabText(i, tab_title(i));
        // Header status, strip text and the wallet pages in the new language.
        refresh_wallet_views();
    }
}

namespace {
/// Section id of each tab, in Tab order (kHome … kRoadmap).
const char* const kCcSectionIds[] = {"home", "trade", "activity", "settings", "stake", "markets", "roadmap"};
} // namespace

bool CryptoCenterScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus);
    const QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    for (int i = 0; i < kTabCount; ++i) {
        if (key != QLatin1String(kCcSectionIds[i]))
            continue;
        // The tab's click (counts as the user's choice: a later wallet connect does not move away).
        if (tab_widget_->currentIndex() != i)
            tab_widget_->setCurrentIndex(i);
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> cc_command_rows() {
    namespace rows = command_rows;
    const QString where = CryptoCenterScreen::tr("Crypto Center");
    const QString screen = QStringLiteral("crypto_center");
    return {
        rows::section(screen, QStringLiteral("home"), CryptoCenterScreen::tr("Wallet home"),
                      {QStringLiteral("wallet"), QStringLiteral("balances"), QStringLiteral("sol"),
                       QStringLiteral("fncpt"), QStringLiteral("solana")},
                      where),
        rows::section(screen, QStringLiteral("trade"), CryptoCenterScreen::tr("Wallet swap"),
                      {QStringLiteral("swap"), QStringLiteral("trade"), QStringLiteral("buy fncpt"),
                       QStringLiteral("jupiter")},
                      where),
        rows::section(screen, QStringLiteral("activity"), CryptoCenterScreen::tr("Wallet activity"),
                      {QStringLiteral("activity"), QStringLiteral("transactions"), QStringLiteral("history"),
                       QStringLiteral("transfers")},
                      where),
        rows::section(screen, QStringLiteral("settings"), CryptoCenterScreen::tr("Wallet settings"),
                      {QStringLiteral("helius"), QStringLiteral("rpc"), QStringLiteral("slippage"),
                       QStringLiteral("wallet mode")},
                      where),
        rows::section(screen, QStringLiteral("stake"), CryptoCenterScreen::tr("Staking"),
                      {QStringLiteral("stake"), QStringLiteral("lock"), QStringLiteral("vefncpt"),
                       QStringLiteral("tiers")},
                      where),
        rows::section(screen, QStringLiteral("markets"), CryptoCenterScreen::tr("Crypto prediction markets"),
                      {QStringLiteral("prediction"), QStringLiteral("markets"), QStringLiteral("bets")}, where),
        rows::section(screen, QStringLiteral("roadmap"), CryptoCenterScreen::tr("Buyback roadmap"),
                      {QStringLiteral("roadmap"), QStringLiteral("buyback"), QStringLiteral("burn"),
                       QStringLiteral("treasury")},
                      where),
    };
}

void cc_install_commands() {
    command_rows::install(QStringLiteral("crypto_center"), &cc_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(cc_install_commands)

} // namespace fincept::screens
