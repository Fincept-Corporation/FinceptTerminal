#include "screens/crypto_center/panels/SwapPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/crypto_center/WalletActionConfirmDialog.h"
#include "screens/crypto_center/WalletActionSummary.h"
#include "services/wallet/PumpFunSwapService.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::panels {

namespace {

constexpr const char* kFncptMint = "9LUqJ5aQTjQiUCL93gi33LZcscUoSBJNhVCYpPzEpump";
constexpr int kDebounceMs = 250;
constexpr int kDefaultSlippageBps = 100;
constexpr int kMaxSlippageBps = 500;
constexpr double kDefaultPriorityFeeSol = 0.00005;
constexpr int kStatusPollMs = 1500;
constexpr int kStatusPollMaxAttempts = 40; // 60 s of polling

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int max_dp = 4) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', max_dp);
}

QString format_bps(int bps) {
    return QStringLiteral("%1.%2%")
        .arg(bps / 100)
        .arg(bps % 100, 2, 10, QChar('0'));
}

} // namespace

SwapPanel::SwapPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("swapPanel"));
    build_ui();
    apply_theme();

    debounce_timer_ = new QTimer(this);
    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(kDebounceMs);
    connect(debounce_timer_, &QTimer::timeout, this, &SwapPanel::recompute_estimate);

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &SwapPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &SwapPanel::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &SwapPanel::on_topic_error);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        on_wallet_disconnected();
    }
}

SwapPanel::~SwapPanel() = default;

void SwapPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("swapPanelHead"));
    head->setFixedHeight(34);
    auto* head_l = new QHBoxLayout(head);
    head_l->setContentsMargins(12, 0, 12, 0);
    head_l->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("SWAP"), head);
    title->setObjectName(QStringLiteral("swapPanelTitle"));
    auto* head_status = new QLabel(QStringLiteral("via Jupiter"), head);
    head_status->setObjectName(QStringLiteral("swapPanelHeadStatus"));
    head_l->addWidget(title);
    head_l->addStretch();
    head_l->addWidget(head_status);
    root->addWidget(head);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("swapPanelBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    // Mode (BUY $FNCPT / SELL $FNCPT)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* lbl = new QLabel(QStringLiteral("DIRECTION"), body);
        lbl->setObjectName(QStringLiteral("swapPanelCaption"));
        mode_combo_ = new QComboBox(body);
        mode_combo_->setObjectName(QStringLiteral("swapPanelCombo"));
        mode_combo_->addItem(tr("BUY $FNCPT (SOL → $FNCPT)"));
        mode_combo_->addItem(tr("SELL $FNCPT ($FNCPT → SOL)"));
        mode_combo_->setFixedHeight(28);
        row->addWidget(lbl);
        row->addWidget(mode_combo_, 1);
        bl->addLayout(row);
    }

    // FROM amount
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* col_l = new QVBoxLayout;
        col_l->setSpacing(2);
        auto* cap = new QLabel(QStringLiteral("YOU PAY"), body);
        cap->setObjectName(QStringLiteral("swapPanelCaption"));
        col_l->addWidget(cap);
        amount_input_ = new QLineEdit(body);
        amount_input_->setObjectName(QStringLiteral("swapPanelInput"));
        amount_input_->setFixedHeight(34);
        amount_input_->setPlaceholderText(QStringLiteral("0.00"));
        auto* validator = new QDoubleValidator(0.0, 1e12, 9, this);
        validator->setNotation(QDoubleValidator::StandardNotation);
        amount_input_->setValidator(validator);
        col_l->addWidget(amount_input_);
        row->addLayout(col_l, 1);

        auto* col_t = new QVBoxLayout;
        col_t->setSpacing(2);
        auto* cap_t = new QLabel(QStringLiteral("TOKEN"), body);
        cap_t->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(cap_t);
        in_token_label_ = new QLabel(QStringLiteral("SOL"), body);
        in_token_label_->setObjectName(QStringLiteral("swapPanelTokenChip"));
        in_token_label_->setFixedHeight(34);
        in_token_label_->setAlignment(Qt::AlignCenter);
        in_token_label_->setMinimumWidth(80);
        col_t->addWidget(in_token_label_);
        row->addLayout(col_t);

        max_button_ = new QPushButton(tr("MAX"), body);
        max_button_->setObjectName(QStringLiteral("swapPanelButton"));
        max_button_->setFixedHeight(34);
        max_button_->setCursor(Qt::PointingHandCursor);
        row->addWidget(max_button_);
        bl->addLayout(row);

        in_balance_label_ = new QLabel(tr("Balance: —"), body);
        in_balance_label_->setObjectName(QStringLiteral("swapPanelMeta"));
        bl->addWidget(in_balance_label_);
    }

    // TO (quoted)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* col_l = new QVBoxLayout;
        col_l->setSpacing(2);
        auto* cap = new QLabel(QStringLiteral("YOU RECEIVE (EST.)"), body);
        cap->setObjectName(QStringLiteral("swapPanelCaption"));
        col_l->addWidget(cap);
        out_amount_label_ = new QLabel(QStringLiteral("—"), body);
        out_amount_label_->setObjectName(QStringLiteral("swapPanelOutAmount"));
        out_amount_label_->setFixedHeight(34);
        col_l->addWidget(out_amount_label_);
        row->addLayout(col_l, 1);

        auto* col_t = new QVBoxLayout;
        col_t->setSpacing(2);
        auto* cap_t = new QLabel(QStringLiteral("TOKEN"), body);
        cap_t->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(cap_t);
        out_token_label_ = new QLabel(QStringLiteral("$FNCPT"), body);
        out_token_label_->setObjectName(QStringLiteral("swapPanelTokenChip"));
        out_token_label_->setFixedHeight(34);
        out_token_label_->setAlignment(Qt::AlignCenter);
        out_token_label_->setMinimumWidth(80);
        col_t->addWidget(out_token_label_);
        row->addLayout(col_t);
        bl->addLayout(row);
    }

    // Quote details (route, impact, slippage)
    {
        auto* details = new QFrame(body);
        details->setObjectName(QStringLiteral("swapPanelDetailsBlock"));
        auto* dl = new QVBoxLayout(details);
        dl->setContentsMargins(10, 8, 10, 8);
        dl->setSpacing(4);

        auto add_kv = [details, dl](const QString& cap, QLabel*& v) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            auto* k = new QLabel(cap, details);
            k->setObjectName(QStringLiteral("swapPanelCaption"));
            v = new QLabel(QStringLiteral("—"), details);
            v->setObjectName(QStringLiteral("swapPanelDetailValue"));
            row->addWidget(k);
            row->addStretch(1);
            row->addWidget(v);
            dl->addLayout(row);
        };
        add_kv(QStringLiteral("ROUTE"), route_label_);
        add_kv(QStringLiteral("PRICE IMPACT"), impact_label_);
        add_kv(QStringLiteral("MAX SLIPPAGE"), slippage_label_);
        bl->addWidget(details);
    }

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("swapPanelErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("swapPanelErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("swapPanelErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    // Submit row
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        status_label_ = new QLabel(tr("Quotes refresh as you type."), body);
        status_label_->setObjectName(QStringLiteral("swapPanelMeta"));
        swap_button_ = new QPushButton(tr("SWAP"), body);
        swap_button_->setObjectName(QStringLiteral("swapPanelPrimaryButton"));
        swap_button_->setFixedHeight(34);
        swap_button_->setCursor(Qt::PointingHandCursor);
        swap_button_->setEnabled(false);
        row->addWidget(status_label_, 1);
        row->addWidget(swap_button_);
        bl->addLayout(row);
    }

    bl->addStretch(1);
    root->addWidget(body, 1);

    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SwapPanel::on_mode_changed);
    connect(amount_input_, &QLineEdit::textEdited, this, &SwapPanel::on_amount_changed);
    connect(max_button_, &QPushButton::clicked, this, &SwapPanel::on_max_clicked);
    connect(swap_button_, &QPushButton::clicked, this, &SwapPanel::on_swap_clicked);
}

void SwapPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#swapPanel { background:%1; }"
        "QWidget#swapPanelHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#swapPanelTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#swapPanelHeadStatus { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QWidget#swapPanelBody { background:%1; }"
        "QLabel#swapPanelCaption { color:%6; font-family:%5; font-size:9px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#swapPanelMeta { color:%6; font-family:%5; font-size:10px;"
        "  background:transparent; }"

        // Inputs
        "QLineEdit#swapPanelInput { background:%2; color:%7; border:1px solid %3;"
        "  font-family:%5; font-size:16px; padding:0 8px; }"
        "QLineEdit#swapPanelInput:focus { border-color:%4; }"
        "QComboBox#swapPanelCombo { background:%2; color:%7; border:1px solid %3;"
        "  font-family:%5; font-size:11px; padding:0 8px; }"
        "QComboBox#swapPanelCombo::drop-down { border:none; width:18px; }"

        // Token chip (read-only, looks like an inert pill)
        "QLabel#swapPanelTokenChip { background:%8; color:%4; border:1px solid %3;"
        "  font-family:%5; font-size:13px; font-weight:700; letter-spacing:1.2px; }"

        // Out amount
        "QLabel#swapPanelOutAmount { background:%8; color:%7; border:1px solid %3;"
        "  font-family:%5; font-size:16px; padding:0 8px; }"

        // Details block
        "QFrame#swapPanelDetailsBlock { background:%8; border:1px solid %3; }"
        "QLabel#swapPanelDetailValue { color:%7; font-family:%5; font-size:11px;"
        "  background:transparent; }"

        // Error strip
        "QFrame#swapPanelErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %9; }"
        "QLabel#swapPanelErrorIcon { color:%9; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#swapPanelErrorText { color:%9; font-family:%5; font-size:11px;"
        "  background:transparent; }"

        // Buttons
        "QPushButton#swapPanelButton { background:%8; color:%6; border:1px solid %3;"
        "  font-family:%5; font-size:11px; font-weight:700; letter-spacing:1px;"
        "  padding:0 14px; }"
        "QPushButton#swapPanelButton:hover { background:%10; color:%7; border-color:%11; }"
        "QPushButton#swapPanelPrimaryButton { background:rgba(217,119,6,0.10); color:%4;"
        "  border:1px solid %12; font-family:%5; font-size:12px; font-weight:700;"
        "  letter-spacing:1.5px; padding:0 18px; }"
        "QPushButton#swapPanelPrimaryButton:hover { background:%4; color:%1; }"
        "QPushButton#swapPanelPrimaryButton:disabled { background:%8; color:%6;"
        "  border-color:%3; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             TEXT_TERTIARY(),   // %6
             TEXT_PRIMARY(),    // %7
             BG_RAISED(),       // %8
             NEGATIVE())        // %9
        .arg(BG_HOVER(),                       // %10
             BORDER_BRIGHT(),                  // %11
             QStringLiteral("#78350f"));       // %12 darker amber

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void SwapPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) {
        auto& hub = fincept::datahub::DataHub::instance();
        if (current_balance_topic_.isEmpty()) {
            current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(pubkey);
            hub.subscribe(this, current_balance_topic_,
                          [this](const QVariant& v) { on_balance_update(v); });
            hub.request(current_balance_topic_, /*force=*/true);
        }
        hub.subscribe(this, QStringLiteral("market:price:fncpt"),
                      [this](const QVariant& v) { on_price_update(v); });
        hub.request(QStringLiteral("market:price:fncpt"), /*force=*/true);
    }
    set_busy(false);
    swap_button_->setEnabled(can_submit());
    update_inputs_from_balance();
}

void SwapPanel::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    current_pubkey_.clear();
    sol_balance_ = 0.0;
    fncpt_balance_ = 0.0;
    last_fncpt_usd_price_ = 0.0;
    in_balance_label_->setText(tr("Balance: —"));
    out_amount_label_->setText(QStringLiteral("—"));
    route_label_->setText(QStringLiteral("—"));
    impact_label_->setText(QStringLiteral("—"));
    swap_button_->setEnabled(false);
    status_label_->setText(tr("Connect a wallet to swap."));
    clear_error_strip();
    set_busy(false);
}

void SwapPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_pubkey_.isEmpty() && current_balance_topic_.isEmpty()) {
        current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
        hub.subscribe(this, current_balance_topic_,
                      [this](const QVariant& v) { on_balance_update(v); });
    }
    hub.subscribe(this, QStringLiteral("market:price:fncpt"),
                  [this](const QVariant& v) { on_price_update(v); });
    slippage_label_->setText(format_bps(slippage_bps()));
    route_label_->setText(QStringLiteral("PumpSwap (auto)"));
}

void SwapPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    // unsubscribe(this) detaches us from every topic in one call —
    // covers both the balance topic and market:price:fncpt.
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    debounce_timer_->stop();
}

// ── State updates ──────────────────────────────────────────────────────────

void SwapPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    const auto bal = v.value<fincept::wallet::WalletBalance>();
    sol_balance_ = bal.sol();
    fncpt_balance_ = bal.fncpt_ui();
    fncpt_decimals_ = bal.fncpt_decimals();
    update_inputs_from_balance();
}

void SwapPanel::update_inputs_from_balance() {
    if (mode_ == Mode::BuyFncpt) {
        in_token_label_->setText(QStringLiteral("SOL"));
        out_token_label_->setText(QStringLiteral("$FNCPT"));
        in_balance_label_->setText(tr("Balance: %1 SOL").arg(format_token(sol_balance_, 4)));
    } else {
        in_token_label_->setText(QStringLiteral("$FNCPT"));
        out_token_label_->setText(QStringLiteral("SOL"));
        in_balance_label_->setText(
            tr("Balance: %1 $FNCPT").arg(format_token(fncpt_balance_, 2)));
    }
}

void SwapPanel::on_price_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::FncptPrice>()) return;
    const auto p = v.value<fincept::wallet::FncptPrice>();
    if (!p.valid) return;
    last_fncpt_usd_price_ = p.usd;
    last_fncpt_sol_price_ = p.sol;  // FNCPT denominated in SOL
    recompute_estimate();
}

void SwapPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic == QStringLiteral("market:price:fncpt")) {
        // Without a price we can't estimate output — disable submit and warn.
        show_error_strip(tr("Spot price unavailable: %1. Try again in a moment.")
                             .arg(error));
        swap_button_->setEnabled(false);
    }
    // Balance errors handled by HoldingsBar / HomeTab; ignore here.
}

void SwapPanel::on_amount_changed(const QString& /*s*/) {
    // Debounce the local estimate recompute so we don't thrash on every
    // keystroke (mostly cosmetic — the math is cheap, but it gives the
    // user a chance to finish typing before we paint the output).
    debounce_timer_->start();
    out_amount_label_->setText(QStringLiteral("…"));
    swap_button_->setEnabled(false);
    clear_error_strip();
}

void SwapPanel::on_mode_changed(int idx) {
    mode_ = (idx == 0) ? Mode::BuyFncpt : Mode::SellFncpt;
    update_inputs_from_balance();
    debounce_timer_->start();
}

void SwapPanel::on_max_clicked() {
    const double bal = (mode_ == Mode::BuyFncpt) ? sol_balance_ : fncpt_balance_;
    if (bal <= 0.0) return;
    // Leave a small SOL reserve for fees. 0.005 SOL covers a swap + rent.
    const double safe = (mode_ == Mode::BuyFncpt) ? std::max(0.0, bal - 0.005) : bal;
    QSignalBlocker b(amount_input_);
    amount_input_->setText(format_token(safe, 6));
    on_amount_changed(amount_input_->text());
}

void SwapPanel::recompute_estimate() {
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0) {
        out_amount_label_->setText(QStringLiteral("—"));
        swap_button_->setEnabled(false);
        return;
    }
    if (last_fncpt_sol_price_ <= 0.0 || last_fncpt_usd_price_ <= 0.0) {
        out_amount_label_->setText(QStringLiteral("…"));
        status_label_->setText(tr("Waiting for spot price…"));
        swap_button_->setEnabled(false);
        return;
    }

    // Local estimate from spot. PumpPortal returns the exact output at
    // build_swap time; this is just the user-facing preview.
    //
    // Spot inputs:
    //   last_fncpt_usd_price_ = USD per 1 FNCPT
    //   last_fncpt_sol_price_ = SOL per 1 FNCPT
    //
    // BUY  (SOL → FNCPT): user types SOL_in
    //                     est_fncpt_out = SOL_in / SOL_per_FNCPT
    // SELL (FNCPT → SOL): user types FNCPT_in
    //                     est_sol_out  = FNCPT_in * SOL_per_FNCPT
    //
    // Slippage is the user-set lower bound on what they'll accept; show the
    // worst-case to set expectations correctly.
    const double slippage_factor = 1.0 - (slippage_pct() / 100.0);
    QString out_text;
    if (mode_ == Mode::BuyFncpt) {
        const double est_fncpt = (ui_amount / last_fncpt_sol_price_) * slippage_factor;
        const double est_usd = ui_amount * last_fncpt_usd_price_ / last_fncpt_sol_price_;
        out_text = QStringLiteral("≈ %1 $FNCPT  (~$%2)")
                       .arg(format_token(est_fncpt, 2))
                       .arg(format_token(est_usd, 2));
    } else {
        const double est_sol = ui_amount * last_fncpt_sol_price_ * slippage_factor;
        const double est_usd = ui_amount * last_fncpt_usd_price_;
        out_text = QStringLiteral("≈ %1 SOL  (~$%2)")
                       .arg(format_token(est_sol, 6))
                       .arg(format_token(est_usd, 2));
    }
    out_amount_label_->setText(out_text);
    route_label_->setText(QStringLiteral("PumpSwap (auto)"));
    impact_label_->setText(tr("set by PumpSwap; capped by slippage"));
    slippage_label_->setText(format_bps(slippage_bps()));
    status_label_->setText(tr("Ready. Click SWAP to build the transaction."));
    clear_error_strip();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
    error_text_->setText(msg);
    error_strip_->show();
}

void SwapPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void SwapPanel::set_busy(bool busy) {
    busy_ = busy;
    amount_input_->setEnabled(!busy);
    mode_combo_->setEnabled(!busy);
    max_button_->setEnabled(!busy);
    swap_button_->setEnabled(!busy && can_submit());
}

bool SwapPanel::can_submit() const {
    if (busy_) return false;
    if (current_pubkey_.isEmpty()) return false;
    if (last_fncpt_sol_price_ <= 0.0) return false;
    if (last_fncpt_usd_price_ <= 0.0) return false;
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0) return false;
    // Don't let the user buy/sell more than they have. PumpPortal would
    // reject the tx anyway; this is the friendlier early-exit.
    const double bal = (mode_ == Mode::BuyFncpt) ? sol_balance_ : fncpt_balance_;
    if (ui_amount > bal) return false;
    return true;
}

int SwapPanel::slippage_bps() const {
    auto r = SecureStorage::instance().retrieve(QStringLiteral("wallet.default_slippage_bps"));
    if (r.is_ok()) {
        bool ok = false;
        const auto v = r.value().toInt(&ok);
        if (ok && v >= 10 && v <= kMaxSlippageBps) return v;
    }
    return kDefaultSlippageBps;
}

int SwapPanel::slippage_pct() const {
    // PumpPortal expects integer percent; round up so we never under-tolerate.
    const int bps = slippage_bps();
    int pct = (bps + 99) / 100;
    if (pct < 1) pct = 1;
    if (pct > 5) pct = 5;
    return pct;
}

// ── Submit flow ────────────────────────────────────────────────────────────

void SwapPanel::on_swap_clicked() {
    if (!can_submit()) return;
    auto* svc = fincept::wallet::WalletService::instance().swap_service();
    if (!svc) {
        show_error_strip(tr("Swap service unavailable."));
        return;
    }
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0) return;

    const auto pubkey = current_pubkey_;
    const int slip_pct = slippage_pct();
    const int slip_bps_for_display = slippage_bps();
    const auto action = (mode_ == Mode::BuyFncpt)
                            ? fincept::wallet::PumpFunSwapService::Action::Buy
                            : fincept::wallet::PumpFunSwapService::Action::Sell;
    // BUY: amount is in SOL (denominatedInSol=true).
    // SELL: amount is in FNCPT (denominatedInSol=false).
    const bool denom_in_sol = (mode_ == Mode::BuyFncpt);

    // Build the user-facing summary from local estimate. PumpPortal will
    // produce the canonical numbers when build_swap returns; the wallet
    // shows the decoded transaction independently. Confirm dialog values
    // here are the panel's expectation; mismatches with PumpPortal's
    // result would be visible in the wallet popup.
    WalletActionSummary summary;
    summary.title = QStringLiteral("SWAP");
    summary.lede = tr("Approve in your wallet to forward this transaction "
                      "to the network. The terminal does not hold any funds.");
    if (mode_ == Mode::BuyFncpt) {
        const double est_fncpt = ui_amount / last_fncpt_sol_price_;
        summary.rows.append({QStringLiteral("ROUTE"),
                             QStringLiteral("PumpSwap (pool=auto)"), true});
        summary.rows.append({QStringLiteral("YOU PAY"),
                             QStringLiteral("%1 SOL").arg(format_token(ui_amount, 6)),
                             true});
        summary.rows.append({QStringLiteral("YOU RECEIVE"),
                             tr("≈ %1 $FNCPT (PumpSwap fills at execution)")
                                 .arg(format_token(est_fncpt, 2)),
                             true});
    } else {
        const double est_sol = ui_amount * last_fncpt_sol_price_;
        summary.rows.append({QStringLiteral("ROUTE"),
                             QStringLiteral("PumpSwap (pool=auto)"), true});
        summary.rows.append({QStringLiteral("YOU PAY"),
                             QStringLiteral("%1 $FNCPT").arg(format_token(ui_amount, 2)),
                             true});
        summary.rows.append({QStringLiteral("YOU RECEIVE"),
                             tr("≈ %1 SOL (PumpSwap fills at execution)")
                                 .arg(format_token(est_sol, 6)),
                             true});
    }
    summary.rows.append({QStringLiteral("MAX SLIPPAGE"),
                         format_bps(slip_bps_for_display), true});
    summary.rows.append({QStringLiteral("PRIORITY FEE"),
                         QStringLiteral("%1 SOL")
                             .arg(format_token(kDefaultPriorityFeeSol, 6)),
                         true});
    summary.warnings.append(
        tr("PumpSwap will reject the trade if execution drifts more than "
           "the slippage tolerance above. Your funds stay in your wallet."));
    summary.primary_button_text = QStringLiteral("SWAP");
    summary.primary_is_safe = true;
    summary.arm_delay_ms = 1500;

    auto* dlg = new WalletActionConfirmDialog(summary, this);
    QPointer<SwapPanel> self = this;
    connect(dlg, &WalletActionConfirmDialog::confirmed, this,
            [self, svc, pubkey, ui_amount, action, denom_in_sol, slip_pct]() {
        if (!self) return;
        self->set_busy(true);
        self->status_label_->setText(self->tr("Building swap transaction…"));

        QPointer<SwapPanel> guard = self;
        svc->build_swap(action,
                        QString::fromLatin1(kFncptMint),
                        ui_amount,
                        denom_in_sol,
                        pubkey,
                        slip_pct,
                        kDefaultPriorityFeeSol,
                        [guard](Result<fincept::wallet::PumpFunSwapService::SwapTransaction> r) {
            if (!guard) return;
            if (r.is_err()) {
                guard->set_busy(false);
                guard->show_error_strip(
                    QObject::tr("build_swap failed: %1")
                        .arg(QString::fromStdString(r.error())));
                guard->status_label_->setText(QObject::tr("Failed."));
                return;
            }
            const auto tx = r.value();
            guard->status_label_->setText(QObject::tr("Awaiting wallet signature…"));
            fincept::wallet::WalletService::instance().sign_and_send(
                tx.tx_base64,
                QObject::tr("Sign swap"),
                QObject::tr("Approve the swap in your wallet to complete the trade."),
                guard,
                [guard](Result<QString> sr) {
                    if (!guard) return;
                    if (sr.is_err()) {
                        guard->set_busy(false);
                        guard->show_error_strip(
                            QObject::tr("Signing failed: %1")
                                .arg(QString::fromStdString(sr.error())));
                        guard->status_label_->setText(QObject::tr("Cancelled."));
                        return;
                    }
                    const auto sig = sr.value();
                    guard->status_label_->setText(
                        QObject::tr("Sent. Waiting for confirmation…"));
                    LOG_INFO("SwapPanel", "submitted: " + sig);

                    // Poll getSignatureStatuses until confirmed (or give up).
                    auto* rpc = new fincept::wallet::SolanaRpcClient(guard);
                    rpc->reload_endpoint();
                    auto attempts = std::make_shared<int>(0);
                    auto* poll_timer = new QTimer(guard);
                    poll_timer->setInterval(kStatusPollMs);
                    QPointer<SwapPanel> poll_guard = guard;
                    QObject::connect(poll_timer, &QTimer::timeout, guard,
                                     [poll_guard, rpc, sig, attempts, poll_timer]() {
                        if (!poll_guard) {
                            poll_timer->stop();
                            poll_timer->deleteLater();
                            return;
                        }
                        if (++(*attempts) > kStatusPollMaxAttempts) {
                            poll_timer->stop();
                            poll_timer->deleteLater();
                            poll_guard->set_busy(false);
                            poll_guard->show_error_strip(
                                QObject::tr("No confirmation after 60 s. Check Solscan."));
                            poll_guard->status_label_->setText(QObject::tr("Timed out."));
                            return;
                        }
                        rpc->get_signature_statuses(QStringList{sig},
                            [poll_guard, sig, poll_timer](
                                Result<std::vector<fincept::wallet::SolanaRpcClient::SignatureStatus>> r) {
                            if (!poll_guard) return;
                            if (r.is_err()) return; // transient — keep polling
                            const auto& vec = r.value();
                            if (vec.empty() || !vec[0].found) return;
                            const auto& s = vec[0];
                            if (!s.err.isEmpty()) {
                                poll_timer->stop();
                                poll_timer->deleteLater();
                                poll_guard->set_busy(false);
                                poll_guard->show_error_strip(
                                    QObject::tr("Tx failed on-chain: %1").arg(s.err));
                                poll_guard->status_label_->setText(QObject::tr("Reverted."));
                                return;
                            }
                            if (s.confirmation_status == QStringLiteral("confirmed")
                                || s.confirmation_status == QStringLiteral("finalized")) {
                                poll_timer->stop();
                                poll_timer->deleteLater();
                                poll_guard->set_busy(false);
                                poll_guard->amount_input_->clear();
                                poll_guard->out_amount_label_->setText(QStringLiteral("—"));
                                poll_guard->status_label_->setText(
                                    QObject::tr("Confirmed: %1…").arg(sig.left(12)));
                                fincept::wallet::WalletService::instance()
                                    .force_balance_refresh();
                            }
                        });
                    });
                    poll_timer->start();
                });
        });
    });
    connect(dlg, &WalletActionConfirmDialog::cancelled, this, [self]() {
        if (!self) return;
        self->status_label_->setText(self->tr("Cancelled."));
    });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
}

} // namespace fincept::screens::panels
