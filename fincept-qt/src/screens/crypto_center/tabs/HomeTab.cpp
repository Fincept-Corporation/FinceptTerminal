#include "screens/crypto_center/tabs/HomeTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/crypto_center/panels/HoldingsTable.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString home_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString shorten_pubkey(const QString& pk) {
    if (pk.size() < 16) return pk;
    return pk.left(8) + QStringLiteral("…") + pk.right(8);
}

QString relative_time(qint64 ts_ms) {
    if (ts_ms == 0) return QStringLiteral("never");
    const auto delta = QDateTime::currentMSecsSinceEpoch() - ts_ms;
    if (delta < 0) return QStringLiteral("just now");
    const auto seconds = delta / 1000;
    if (seconds < 60) return QStringLiteral("%1s ago").arg(seconds);
    if (seconds < 3600) return QStringLiteral("%1m ago").arg(seconds / 60);
    if (seconds < 86400) return QStringLiteral("%1h ago").arg(seconds / 3600);
    return QStringLiteral("%1d ago").arg(seconds / 86400);
}

} // namespace

HomeTab::HomeTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("homeTab"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &HomeTab::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &HomeTab::on_wallet_disconnected);
    connect(&svc, &fincept::wallet::WalletService::balance_mode_changed, this,
            &HomeTab::on_mode_changed);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &HomeTab::on_topic_error);

    apply_mode_to_buttons(svc.balance_mode_is_stream());
    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    }
}

HomeTab::~HomeTab() = default;

void HomeTab::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto* top_row = new QHBoxLayout;
    top_row->setSpacing(10);

    // ── Wallet panel ──────────────────────────────────────────────────────
    {
        wallet_panel_ = new QFrame(this);
        wallet_panel_->setObjectName(QStringLiteral("homeTabPanel"));
        auto* outer = new QVBoxLayout(wallet_panel_);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* head = new QWidget(wallet_panel_);
        head->setObjectName(QStringLiteral("homeTabPanelHead"));
        head->setFixedHeight(34);
        auto* head_l = new QHBoxLayout(head);
        head_l->setContentsMargins(12, 0, 12, 0);
        head_l->setSpacing(0);
        auto* title = new QLabel(QStringLiteral("WALLET"), head);
        title->setObjectName(QStringLiteral("homeTabPanelTitle"));
        auto* status = new QLabel(QStringLiteral("● CONNECTED"), head);
        status->setObjectName(QStringLiteral("homeTabPanelStatusOk"));
        head_l->addWidget(title);
        head_l->addStretch();
        head_l->addWidget(status);
        outer->addWidget(head);

        auto* body = new QWidget(wallet_panel_);
        auto* body_l = new QVBoxLayout(body);
        body_l->setContentsMargins(0, 0, 0, 0);
        body_l->setSpacing(0);

        auto add_row = [body_l, body](const QString& caption, QLabel*& val_out,
                                      bool last) {
            auto* row = new QWidget(body);
            row->setObjectName(QStringLiteral("homeTabRow"));
            row->setProperty("isLast", last);
            row->setFixedHeight(32);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(12, 0, 12, 0);
            rl->setSpacing(8);
            auto* cap = new QLabel(caption, row);
            cap->setObjectName(QStringLiteral("homeTabCaption"));
            val_out = new QLabel(QStringLiteral("—"), row);
            val_out->setObjectName(QStringLiteral("homeTabRowValue"));
            val_out->setTextInteractionFlags(Qt::TextSelectableByMouse);
            rl->addWidget(cap);
            rl->addStretch(1);
            rl->addWidget(val_out);
            body_l->addWidget(row);
        };
        add_row(QStringLiteral("PROVIDER"), row_label_value_, false);
        add_row(QStringLiteral("ADDRESS"), row_pubkey_value_, false);
        add_row(QStringLiteral("CONNECTED"), row_connected_value_, true);
        outer->addWidget(body);

        auto* btn_row = new QWidget(wallet_panel_);
        btn_row->setObjectName(QStringLiteral("homeTabPanelFoot"));
        btn_row->setFixedHeight(40);
        auto* br_l = new QHBoxLayout(btn_row);
        br_l->setContentsMargins(10, 6, 10, 6);
        br_l->setSpacing(6);
        copy_button_ = new QPushButton(tr("COPY ADDRESS"), btn_row);
        copy_button_->setObjectName(QStringLiteral("homeTabButton"));
        copy_button_->setFixedHeight(26);
        copy_button_->setCursor(Qt::PointingHandCursor);
        disconnect_button_ = new QPushButton(tr("DISCONNECT"), btn_row);
        disconnect_button_->setObjectName(QStringLiteral("homeTabDangerButton"));
        disconnect_button_->setFixedHeight(26);
        disconnect_button_->setCursor(Qt::PointingHandCursor);
        br_l->addWidget(copy_button_);
        br_l->addStretch(1);
        br_l->addWidget(disconnect_button_);
        outer->addWidget(btn_row);

        top_row->addWidget(wallet_panel_, 1);
    }

    // ── Holdings panel (replaces three stat boxes per Stage 2A.5) ─────────
    {
        balance_panel_ = new QFrame(this);
        balance_panel_->setObjectName(QStringLiteral("homeTabPanel"));
        auto* outer = new QVBoxLayout(balance_panel_);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* head = new QWidget(balance_panel_);
        head->setObjectName(QStringLiteral("homeTabPanelHead"));
        head->setFixedHeight(34);
        auto* head_l = new QHBoxLayout(head);
        head_l->setContentsMargins(12, 0, 12, 0);
        head_l->setSpacing(8);
        auto* title = new QLabel(QStringLiteral("HOLDINGS"), head);
        title->setObjectName(QStringLiteral("homeTabPanelTitle"));

        // Mode toggle — mirrored from Settings.
        mode_poll_button_ = new QPushButton(QStringLiteral("POLL"), head);
        mode_poll_button_->setObjectName(QStringLiteral("homeTabToggle"));
        mode_poll_button_->setCheckable(true);
        mode_poll_button_->setFixedHeight(22);
        mode_poll_button_->setCursor(Qt::PointingHandCursor);
        mode_stream_button_ = new QPushButton(QStringLiteral("STREAM"), head);
        mode_stream_button_->setObjectName(QStringLiteral("homeTabToggle"));
        mode_stream_button_->setCheckable(true);
        mode_stream_button_->setFixedHeight(22);
        mode_stream_button_->setCursor(Qt::PointingHandCursor);
        mode_group_ = new QButtonGroup(this);
        mode_group_->setExclusive(true);
        mode_group_->addButton(mode_poll_button_);
        mode_group_->addButton(mode_stream_button_);

        auto* mode_label = new QLabel(QStringLiteral("MAINNET"), head);
        mode_label->setObjectName(QStringLiteral("homeTabPanelStatus"));

        head_l->addWidget(title);
        head_l->addStretch();
        head_l->addWidget(mode_poll_button_);
        head_l->addWidget(mode_stream_button_);
        head_l->addSpacing(6);
        head_l->addWidget(mode_label);
        outer->addWidget(head);

        // Body wraps the embedded HoldingsTable + an error strip.
        auto* body = new QWidget(balance_panel_);
        auto* body_l = new QVBoxLayout(body);
        body_l->setContentsMargins(0, 0, 0, 0);
        body_l->setSpacing(0);

        holdings_table_ = new panels::HoldingsTable(body);
        holdings_table_->setMinimumHeight(220);
        connect(holdings_table_, &panels::HoldingsTable::select_token, this,
                [this](const QString& mint) { emit select_token(mint); });
        body_l->addWidget(holdings_table_, 1);

        // Tab-local error strip (hidden by default).
        error_strip_ = new QFrame(body);
        error_strip_->setObjectName(QStringLiteral("homeTabErrorStrip"));
        auto* es_l = new QHBoxLayout(error_strip_);
        es_l->setContentsMargins(10, 6, 10, 6);
        es_l->setSpacing(8);
        auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
        es_icon->setObjectName(QStringLiteral("homeTabErrorIcon"));
        error_strip_text_ = new QLabel(QString(), error_strip_);
        error_strip_text_->setObjectName(QStringLiteral("homeTabErrorText"));
        error_strip_text_->setWordWrap(true);
        es_l->addWidget(es_icon);
        es_l->addWidget(error_strip_text_, 1);
        error_strip_->hide();
        body_l->addWidget(error_strip_);
        outer->addWidget(body, 1);

        // Foot: REFRESH button.
        auto* foot = new QWidget(balance_panel_);
        foot->setObjectName(QStringLiteral("homeTabPanelFoot"));
        foot->setFixedHeight(40);
        auto* fl = new QHBoxLayout(foot);
        fl->setContentsMargins(10, 6, 10, 6);
        fl->setSpacing(6);
        fl->addStretch(1);
        refresh_button_ = new QPushButton(tr("REFRESH"), foot);
        refresh_button_->setObjectName(QStringLiteral("homeTabButton"));
        refresh_button_->setFixedHeight(26);
        refresh_button_->setCursor(Qt::PointingHandCursor);
        fl->addWidget(refresh_button_);
        outer->addWidget(foot);

        top_row->addWidget(balance_panel_, 2);
    }

    root->addLayout(top_row);

    // ── Roadmap panel ─────────────────────────────────────────────────────
    {
        roadmap_panel_ = new QFrame(this);
        roadmap_panel_->setObjectName(QStringLiteral("homeTabPanel"));
        auto* outer = new QVBoxLayout(roadmap_panel_);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* head = new QWidget(roadmap_panel_);
        head->setObjectName(QStringLiteral("homeTabPanelHead"));
        head->setFixedHeight(34);
        auto* head_l = new QHBoxLayout(head);
        head_l->setContentsMargins(12, 0, 12, 0);
        head_l->setSpacing(0);
        auto* title = new QLabel(QStringLiteral("$FNCPT ROADMAP"), head);
        title->setObjectName(QStringLiteral("homeTabPanelTitle"));
        auto* phase = new QLabel(QStringLiteral("PHASE 2"), head);
        phase->setObjectName(QStringLiteral("homeTabPanelStatus"));
        head_l->addWidget(title);
        head_l->addStretch();
        head_l->addWidget(phase);
        outer->addWidget(head);

        roadmap_body_ = new QLabel(
            QStringLiteral("PHASE 1   WALLET & BALANCE        SHIPPED        connect Solana wallet, view $FNCPT + SOL\n"
                           "PHASE 2   SWAP & FEE DISCOUNT     IN PROGRESS    buy $FNCPT via PumpPortal, fee discount\n"
                           "PHASE 3   STAKING & TIERS         UPCOMING       lock $FNCPT for bronze / silver / gold tiers\n"
                           "PHASE 4   PREDICTION MARKETS      UPCOMING       earnings, fed, weather — settled in $FNCPT\n"
                           "PHASE 5   BUYBACK & BURN          UPCOMING       terminal revenue auto-buys & burns $FNCPT"),
            roadmap_panel_);
        roadmap_body_->setObjectName(QStringLiteral("homeTabRoadmapBody"));
        roadmap_body_->setContentsMargins(14, 12, 14, 14);
        outer->addWidget(roadmap_body_);
    }
    root->addWidget(roadmap_panel_);
    root->addStretch(1);

    // Wiring
    connect(copy_button_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(current_pubkey_);
        copy_button_->setText(tr("COPIED"));
        QTimer::singleShot(1200, this, [this]() {
            if (copy_button_) copy_button_->setText(tr("COPY ADDRESS"));
        });
    });
    connect(disconnect_button_, &QPushButton::clicked, this, []() {
        fincept::wallet::WalletService::instance().disconnect();
    });
    connect(refresh_button_, &QPushButton::clicked, this, [this]() {
        if (current_pubkey_.isEmpty()) return;
        clear_error_strip();
        fincept::wallet::WalletService::instance().force_balance_refresh();
    });
    connect(mode_poll_button_, &QPushButton::toggled, this, [](bool checked) {
        if (!checked) return;
        fincept::wallet::WalletService::instance().set_balance_mode(false);
    });
    connect(mode_stream_button_, &QPushButton::toggled, this, [](bool checked) {
        if (!checked) return;
        fincept::wallet::WalletService::instance().set_balance_mode(true);
    });
}

void HomeTab::apply_theme() {
    using namespace ui::colors;
    const QString font = home_font_stack();

    const QString ss = QStringLiteral(
        "QWidget#homeTab { background:%1; }"
        "QFrame#homeTabPanel { background:%2; border:1px solid %3; }"
        "QWidget#homeTabPanelHead { background:%10; border-bottom:1px solid %3; }"
        "QWidget#homeTabPanelFoot { background:%10; border-top:1px solid %3; }"
        "QLabel#homeTabPanelTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#homeTabPanelStatus { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#homeTabPanelStatusOk { color:%9; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QWidget#homeTabRow { background:transparent; border-bottom:1px solid %3; }"
        "QWidget#homeTabRow[isLast=\"true\"] { border-bottom:none; }"
        "QLabel#homeTabCaption { color:%7; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#homeTabRowValue { color:%8; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QFrame#homeTabErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %11; }"
        "QLabel#homeTabErrorIcon { color:%11; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#homeTabErrorText { color:%11; font-family:%5; font-size:11px;"
        "  background:transparent; }"
        "QPushButton#homeTabButton { background:%10; color:%7; border:1px solid %3;"
        "  font-family:%5; font-size:11px; font-weight:700; letter-spacing:1px; padding:0 12px; }"
        "QPushButton#homeTabButton:hover { background:%12; color:%8; border-color:%13; }"
        "QPushButton#homeTabDangerButton { background:rgba(220,38,38,0.10); color:%11;"
        "  border:1px solid %14; font-family:%5; font-size:11px; font-weight:700;"
        "  letter-spacing:1px; padding:0 12px; }"
        "QPushButton#homeTabDangerButton:hover { background:%11; color:%8; }"
        "QPushButton#homeTabToggle { background:transparent; color:%6; border:1px solid %3;"
        "  font-family:%5; font-size:10px; font-weight:700; letter-spacing:1.2px;"
        "  padding:0 10px; }"
        "QPushButton#homeTabToggle:hover { color:%8; border-color:%13; }"
        "QPushButton#homeTabToggle:checked { background:rgba(217,119,6,0.12); color:%4;"
        "  border-color:%4; }"
        "QLabel#homeTabRoadmapBody { color:%7; font-family:%5; font-size:11px;"
        "  background:transparent; }"
    )
        .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), AMBER(), font,
             TEXT_TERTIARY(), TEXT_SECONDARY(), TEXT_PRIMARY(), POSITIVE())
        .arg(BG_RAISED(), NEGATIVE(), BG_HOVER(), BORDER_BRIGHT(),
             QStringLiteral("#7f1d1d"));

    setStyleSheet(ss);
}

void HomeTab::on_wallet_connected(const QString& pubkey, const QString& label) {
    current_pubkey_ = pubkey;
    row_label_value_->setText(label.isEmpty() ? tr("Solana wallet") : label.toUpper());
    row_pubkey_value_->setText(shorten_pubkey(pubkey));
    row_pubkey_value_->setToolTip(pubkey);
    const auto& s = fincept::wallet::WalletService::instance().state();
    if (s.connected_at_ms > 0) {
        row_connected_value_->setText(relative_time(s.connected_at_ms));
    } else {
        row_connected_value_->setText(tr("restored from storage"));
    }
    clear_error_strip();
}

void HomeTab::on_wallet_disconnected() {
    current_pubkey_.clear();
    row_label_value_->setText(QStringLiteral("—"));
    row_pubkey_value_->setText(QStringLiteral("—"));
    row_connected_value_->setText(QStringLiteral("—"));
    clear_error_strip();
}

void HomeTab::on_mode_changed(bool is_stream) {
    apply_mode_to_buttons(is_stream);
    if (current_pubkey_.isEmpty()) return;
    clear_error_strip();
}

void HomeTab::on_topic_error(const QString& topic, const QString& error) {
    if (topic.startsWith(QStringLiteral("wallet:balance:"))) {
        show_error_strip(tr("Balance fetch failed: %1").arg(error));
    }
}

void HomeTab::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_strip_text_->clear();
    }
}

void HomeTab::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
    error_strip_text_->setText(msg);
    error_strip_->show();
}

void HomeTab::apply_mode_to_buttons(bool is_stream) {
    QSignalBlocker b1(mode_poll_button_);
    QSignalBlocker b2(mode_stream_button_);
    mode_poll_button_->setChecked(!is_stream);
    mode_stream_button_->setChecked(is_stream);
}

void HomeTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // HoldingsTable manages its own subscriptions; nothing else to do here.
}

void HomeTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

} // namespace fincept::screens
