#include "screens/crypto_center/tabs/SettingsTab.h"

#include "core/logging/Logger.h"
#include "services/wallet/WalletService.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

constexpr const char* kHeliusKey = "solana.helius_api_key";
constexpr const char* kSlippageKey = "wallet.default_slippage_bps";
constexpr int kDefaultSlippageBps = 100; // 1.00%
constexpr int kMaxSlippageBps = 500;     // 5.00%

QString settings_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_bps(int bps) {
    return QStringLiteral("%1.%2%")
        .arg(bps / 100)
        .arg(bps % 100, 2, 10, QChar('0'));
}

} // namespace

SettingsTab::SettingsTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("settingsTab"));
    build_ui();
    apply_theme();
    load_initial_values();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::balance_mode_changed, this,
            &SettingsTab::on_mode_changed);
}

SettingsTab::~SettingsTab() = default;

void SettingsTab::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto make_panel = [this](const QString& title_text,
                             const QString& subtitle_text) -> std::pair<QFrame*, QVBoxLayout*> {
        auto* panel = new QFrame(this);
        panel->setObjectName(QStringLiteral("settingsTabPanel"));
        auto* outer = new QVBoxLayout(panel);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* head = new QWidget(panel);
        head->setObjectName(QStringLiteral("settingsTabPanelHead"));
        head->setFixedHeight(34);
        auto* head_l = new QHBoxLayout(head);
        head_l->setContentsMargins(12, 0, 12, 0);
        head_l->setSpacing(0);
        auto* title = new QLabel(title_text, head);
        title->setObjectName(QStringLiteral("settingsTabPanelTitle"));
        head_l->addWidget(title);
        head_l->addStretch();
        if (!subtitle_text.isEmpty()) {
            auto* sub = new QLabel(subtitle_text, head);
            sub->setObjectName(QStringLiteral("settingsTabPanelStatus"));
            head_l->addWidget(sub);
        }
        outer->addWidget(head);

        auto* body = new QWidget(panel);
        auto* body_l = new QVBoxLayout(body);
        body_l->setContentsMargins(14, 12, 14, 14);
        body_l->setSpacing(8);
        outer->addWidget(body);
        return {panel, body_l};
    };

    // ── Balance refresh mode ──────────────────────────────────────────────
    {
        auto [panel, body] = make_panel(QStringLiteral("BALANCE REFRESH"),
                                        QStringLiteral("Mirrored on HOME"));
        auto* hint = new QLabel(
            tr("POLL refreshes balances on a TTL via the configured RPC. "
               "STREAM opens a WebSocket account subscription — requires Helius "
               "or a private RPC."),
            panel);
        hint->setObjectName(QStringLiteral("settingsTabHint"));
        hint->setWordWrap(true);
        body->addWidget(hint);

        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        mode_poll_button_ = new QPushButton(QStringLiteral("POLL"), panel);
        mode_poll_button_->setObjectName(QStringLiteral("settingsTabToggle"));
        mode_poll_button_->setCheckable(true);
        mode_poll_button_->setFixedHeight(28);
        mode_poll_button_->setCursor(Qt::PointingHandCursor);
        mode_stream_button_ = new QPushButton(QStringLiteral("STREAM"), panel);
        mode_stream_button_->setObjectName(QStringLiteral("settingsTabToggle"));
        mode_stream_button_->setCheckable(true);
        mode_stream_button_->setFixedHeight(28);
        mode_stream_button_->setCursor(Qt::PointingHandCursor);
        mode_group_ = new QButtonGroup(this);
        mode_group_->setExclusive(true);
        mode_group_->addButton(mode_poll_button_);
        mode_group_->addButton(mode_stream_button_);
        row->addWidget(mode_poll_button_);
        row->addWidget(mode_stream_button_);
        row->addStretch(1);
        body->addLayout(row);

        connect(mode_poll_button_, &QPushButton::toggled, this, [](bool checked) {
            if (!checked) return;
            fincept::wallet::WalletService::instance().set_balance_mode(false);
        });
        connect(mode_stream_button_, &QPushButton::toggled, this, [](bool checked) {
            if (!checked) return;
            fincept::wallet::WalletService::instance().set_balance_mode(true);
        });

        root->addWidget(panel);
    }

    // ── Helius API key ────────────────────────────────────────────────────
    {
        auto [panel, body] = make_panel(QStringLiteral("HELIUS API KEY"),
                                        QStringLiteral("optional"));
        auto* hint = new QLabel(
            tr("Paste a Helius API key for reliable account-subscribe streaming "
               "and parsed transaction history. Stored in SecureStorage; never "
               "transmitted off-machine except in RPC requests to api.helius.xyz."),
            panel);
        hint->setObjectName(QStringLiteral("settingsTabHint"));
        hint->setWordWrap(true);
        body->addWidget(hint);

        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        helius_input_ = new QLineEdit(panel);
        helius_input_->setObjectName(QStringLiteral("settingsTabInput"));
        helius_input_->setPlaceholderText(tr("paste API key…"));
        helius_input_->setEchoMode(QLineEdit::Password);
        helius_input_->setFixedHeight(28);
        row->addWidget(helius_input_, 1);

        helius_save_button_ = new QPushButton(tr("SAVE"), panel);
        helius_save_button_->setObjectName(QStringLiteral("settingsTabButton"));
        helius_save_button_->setFixedHeight(28);
        helius_save_button_->setCursor(Qt::PointingHandCursor);
        row->addWidget(helius_save_button_);

        helius_clear_button_ = new QPushButton(tr("CLEAR"), panel);
        helius_clear_button_->setObjectName(QStringLiteral("settingsTabDangerButton"));
        helius_clear_button_->setFixedHeight(28);
        helius_clear_button_->setCursor(Qt::PointingHandCursor);
        row->addWidget(helius_clear_button_);
        body->addLayout(row);

        helius_status_ = new QLabel(QStringLiteral("—"), panel);
        helius_status_->setObjectName(QStringLiteral("settingsTabHint"));
        body->addWidget(helius_status_);

        connect(helius_save_button_, &QPushButton::clicked, this,
                &SettingsTab::on_save_helius_key);
        connect(helius_clear_button_, &QPushButton::clicked, this,
                &SettingsTab::on_clear_helius_key);

        root->addWidget(panel);
    }

    // ── Default slippage ──────────────────────────────────────────────────
    {
        auto [panel, body] = make_panel(QStringLiteral("DEFAULT SLIPPAGE"),
                                        QStringLiteral("1% – 5%"));
        auto* hint = new QLabel(
            tr("Default slippage tolerance for swaps. Quotes whose route "
               "impact exceeds this value are blocked. Adjustable per-swap on "
               "the TRADE tab."),
            panel);
        hint->setObjectName(QStringLiteral("settingsTabHint"));
        hint->setWordWrap(true);
        body->addWidget(hint);

        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        slippage_slider_ = new QSlider(Qt::Horizontal, panel);
        slippage_slider_->setObjectName(QStringLiteral("settingsTabSlider"));
        slippage_slider_->setMinimum(10);   // 0.10%
        slippage_slider_->setMaximum(kMaxSlippageBps);
        slippage_slider_->setSingleStep(5);
        slippage_slider_->setTickInterval(50);
        slippage_slider_->setTickPosition(QSlider::TicksBelow);
        slippage_slider_->setValue(kDefaultSlippageBps);
        slippage_slider_->setFixedHeight(28);
        slippage_value_ = new QLabel(format_bps(kDefaultSlippageBps), panel);
        slippage_value_->setObjectName(QStringLiteral("settingsTabSliderValue"));
        slippage_value_->setFixedWidth(64);
        row->addWidget(slippage_slider_, 1);
        row->addWidget(slippage_value_);
        body->addLayout(row);

        connect(slippage_slider_, &QSlider::valueChanged, this,
                &SettingsTab::on_slippage_changed);

        root->addWidget(panel);
    }

    // ── Asset filters ─────────────────────────────────────────────────────
    {
        auto [panel, body] = make_panel(QStringLiteral("ASSET FILTERS"),
                                        QStringLiteral("affects holdings"));
        auto* hint = new QLabel(
            tr("Pump.fun-launched wallets accumulate airdropped junk over time. "
               "By default the holdings panel hides tokens that aren't in "
               "Jupiter's verified-tagged list. Toggle this on to see every "
               "SPL token account in the wallet."),
            panel);
        hint->setObjectName(QStringLiteral("settingsTabHint"));
        hint->setWordWrap(true);
        body->addWidget(hint);

        show_unverified_checkbox_ = new QCheckBox(
            tr("Show unverified tokens in the holdings panel"), panel);
        show_unverified_checkbox_->setObjectName(
            QStringLiteral("settingsTabCheckbox"));
        body->addWidget(show_unverified_checkbox_);

        connect(show_unverified_checkbox_, &QCheckBox::toggled, this,
                &SettingsTab::on_show_unverified_toggled);

        root->addWidget(panel);
    }

    root->addStretch(1);
}

void SettingsTab::apply_theme() {
    using namespace ui::colors;
    const QString font = settings_font_stack();

    const QString ss = QStringLiteral(
        "QWidget#settingsTab { background:%1; }"
        "QFrame#settingsTabPanel { background:%2; border:1px solid %3; }"
        "QWidget#settingsTabPanelHead { background:%4; border-bottom:1px solid %3; }"
        "QLabel#settingsTabPanelTitle { color:%5; font-family:%6; font-size:11px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#settingsTabPanelStatus { color:%7; font-family:%6; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#settingsTabHint { color:%8; font-family:%6; font-size:11px;"
        "  background:transparent; }"

        // Inputs
        "QLineEdit#settingsTabInput { background:%4; color:%9; border:1px solid %3;"
        "  font-family:%6; font-size:12px; padding:0 8px; }"
        "QLineEdit#settingsTabInput:focus { border-color:%5; }"

        // Buttons
        "QPushButton#settingsTabButton { background:%4; color:%8; border:1px solid %3;"
        "  font-family:%6; font-size:11px; font-weight:700; letter-spacing:1px; padding:0 14px; }"
        "QPushButton#settingsTabButton:hover { background:%10; color:%9; border-color:%11; }"
        "QPushButton#settingsTabDangerButton { background:rgba(220,38,38,0.10); color:%12;"
        "  border:1px solid %13; font-family:%6; font-size:11px; font-weight:700;"
        "  letter-spacing:1px; padding:0 14px; }"
        "QPushButton#settingsTabDangerButton:hover { background:%12; color:%9; }"

        // Toggle
        "QPushButton#settingsTabToggle { background:transparent; color:%7; border:1px solid %3;"
        "  font-family:%6; font-size:10px; font-weight:700; letter-spacing:1.2px;"
        "  padding:0 14px; }"
        "QPushButton#settingsTabToggle:hover { color:%9; border-color:%11; }"
        "QPushButton#settingsTabToggle:checked { background:rgba(217,119,6,0.12);"
        "  color:%5; border-color:%5; }"

        // Slider
        "QSlider#settingsTabSlider::groove:horizontal { background:%4; border:1px solid %3;"
        "  height:6px; border-radius:0; }"
        "QSlider#settingsTabSlider::handle:horizontal { background:%5; border:1px solid %5;"
        "  width:14px; margin:-6px 0; }"
        "QSlider#settingsTabSlider::sub-page:horizontal { background:rgba(217,119,6,0.30); }"
        "QLabel#settingsTabSliderValue { color:%5; font-family:%6; font-size:14px;"
        "  font-weight:700; background:transparent; }"

        // Checkbox (asset filter)
        "QCheckBox#settingsTabCheckbox { color:%9; font-family:%6; font-size:12px;"
        "  background:transparent; spacing:8px; }"
        "QCheckBox#settingsTabCheckbox::indicator { width:14px; height:14px;"
        "  border:1px solid %3; background:%4; }"
        "QCheckBox#settingsTabCheckbox::indicator:checked { background:%5;"
        "  border-color:%5; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             BG_RAISED(),       // %4
             AMBER(),           // %5
             font,              // %6
             TEXT_TERTIARY(),   // %7
             TEXT_SECONDARY(),  // %8
             TEXT_PRIMARY())    // %9
        .arg(BG_HOVER(),                // %10
             BORDER_BRIGHT(),           // %11
             NEGATIVE(),                // %12
             QStringLiteral("#7f1d1d"));// %13

    setStyleSheet(ss);
}

void SettingsTab::load_initial_values() {
    apply_mode_to_buttons(
        fincept::wallet::WalletService::instance().balance_mode_is_stream());

    auto helius_res =
        SecureStorage::instance().retrieve(QString::fromLatin1(kHeliusKey));
    if (helius_res.is_ok() && !helius_res.value().isEmpty()) {
        helius_status_->setText(tr("Stored — input is hidden. Type to replace."));
    } else {
        helius_status_->setText(tr("No key stored. Public RPC will be used."));
    }

    auto slip_res =
        SecureStorage::instance().retrieve(QString::fromLatin1(kSlippageKey));
    int bps = kDefaultSlippageBps;
    if (slip_res.is_ok()) {
        bool ok = false;
        const auto v = slip_res.value().toInt(&ok);
        if (ok && v >= 10 && v <= kMaxSlippageBps) bps = v;
    }
    QSignalBlocker b(slippage_slider_);
    slippage_slider_->setValue(bps);
    slippage_value_->setText(format_bps(bps));

    // Asset filters: show-unverified toggle.
    auto unv_res = SecureStorage::instance().retrieve(
        QStringLiteral("wallet.show_unverified_tokens"));
    const bool show_unverified =
        unv_res.is_ok() &&
        unv_res.value().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    if (show_unverified_checkbox_) {
        QSignalBlocker cb(show_unverified_checkbox_);
        show_unverified_checkbox_->setChecked(show_unverified);
    }
}

void SettingsTab::on_mode_changed(bool is_stream) {
    apply_mode_to_buttons(is_stream);
}

void SettingsTab::on_show_unverified_toggled(bool checked) {
    SecureStorage::instance().store(
        QStringLiteral("wallet.show_unverified_tokens"),
        checked ? QStringLiteral("true") : QStringLiteral("false"));
    LOG_INFO("SettingsTab",
             QStringLiteral("show_unverified_tokens = %1")
                 .arg(checked ? "true" : "false"));
}

void SettingsTab::on_save_helius_key() {
    const auto v = helius_input_->text().trimmed();
    if (v.isEmpty()) {
        helius_status_->setText(tr("Empty input — use CLEAR to remove a stored key."));
        return;
    }
    auto r = SecureStorage::instance().store(QString::fromLatin1(kHeliusKey), v);
    if (r.is_err()) {
        helius_status_->setText(
            tr("Failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    helius_input_->clear();
    helius_status_->setText(tr("Saved. Restart streaming to use the new key."));
    LOG_INFO("SettingsTab", "Helius key stored");
}

void SettingsTab::on_clear_helius_key() {
    auto r = SecureStorage::instance().remove(QString::fromLatin1(kHeliusKey));
    if (r.is_err()) {
        helius_status_->setText(
            tr("Failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    helius_input_->clear();
    helius_status_->setText(tr("Cleared. Public RPC will be used."));
    LOG_INFO("SettingsTab", "Helius key cleared");
}

void SettingsTab::on_slippage_changed(int bps) {
    slippage_value_->setText(format_bps(bps));
    SecureStorage::instance().store(QString::fromLatin1(kSlippageKey),
                                    QString::number(bps));
}

void SettingsTab::apply_mode_to_buttons(bool is_stream) {
    QSignalBlocker b1(mode_poll_button_);
    QSignalBlocker b2(mode_stream_button_);
    mode_poll_button_->setChecked(!is_stream);
    mode_stream_button_->setChecked(is_stream);
}

} // namespace fincept::screens
