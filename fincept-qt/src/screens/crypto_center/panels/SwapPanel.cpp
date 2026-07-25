#include "screens/crypto_center/panels/SwapPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/crypto_center/WalletActionConfirmDialog.h"
#include "screens/crypto_center/WalletActionSummary.h"
#include "services/wallet/PumpFunSwapService.h"
#include "services/wallet/SolanaRpcClient.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::panels {

namespace {

constexpr int kDebounceMs = 250;
constexpr int kDefaultSlippageBps = 100;
constexpr int kMaxSlippageBps = 500;
constexpr double kDefaultPriorityFeeSol = 0.00005;
constexpr int kStatusPollMs = 1500;
constexpr int kStatusPollMaxAttempts = 40; // 60 s of polling
/// A spot price older than this is not trustworthy enough to size a swap on.
/// We still let the user submit (PumpSwap fills at execution and slippage
/// caps the damage) but the staleness is stated in the ticket and repeated
/// in the confirm dialog.
constexpr qint64 kPriceStaleMs = 60 * 1000;

QString font_stack() {
    return QStringLiteral("'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int max_dp = 4) {
    if (v <= 0.0)
        return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', max_dp);
}

/// Format a token amount for injection into the amount QLineEdit.
///
/// Two properties matter here and neither is optional:
///   1. **Floor, never round.** `QLocale::toString(v,'f',dp)` rounds half-up,
///      so MAX on a balance of 12 304.7 with dp=0 produced "12305" — more than
///      the wallet holds — and `can_submit()` then silently refused the swap.
///   2. **No group separators.** The string is read back with
///      `QLocale::toDouble()` and validated by a QDoubleValidator; a thousands
///      separator round-trips inconsistently across locales.
QString swap_amount_for_input(double v, int decimals) {
    if (v <= 0.0)
        return QStringLiteral("0");
    int dp = decimals;
    if (dp < 0)
        dp = 0;
    if (dp > 9)
        dp = 9;
    const double scale = std::pow(10.0, dp);
    const double floored = std::floor(v * scale) / scale;
    QLocale loc = QLocale::system();
    loc.setNumberOptions(loc.numberOptions() | QLocale::OmitGroupSeparator);
    return loc.toString(floored, 'f', dp);
}

QString format_bps(int bps) {
    return QStringLiteral("%1.%2%").arg(bps / 100).arg(bps % 100, 2, 10, QChar('0'));
}

bool is_native_sol(const QString& mint) {
    return mint == QString::fromLatin1(fincept::wallet::kWrappedSolMint);
}

bool is_fncpt(const QString& mint) {
    return mint == QString::fromLatin1(fincept::wallet::kFncptMint);
}

QString price_topic_for(const QString& mint) {
    return QStringLiteral("market:price:token:%1").arg(mint);
}

QString resolve_symbol(const QString& mint) {
    if (is_native_sol(mint))
        return QStringLiteral("SOL");
    if (is_fncpt(mint))
        return QStringLiteral("$FNCPT");
    auto md = fincept::wallet::TokenMetadataService::instance().lookup(mint);
    if (md && !md->symbol.isEmpty())
        return md->symbol;
    if (mint.size() <= 8)
        return mint;
    return mint.left(4) + QStringLiteral("…") + mint.right(4);
}

} // namespace

SwapPanel::SwapPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("swapPanel"));
    build_ui();
    apply_theme();

    // Pre-seed the synthetic SOL holding once. amount_raw is updated on each
    // balance publish so MAX/can_submit reflect live SOL.
    sol_holding_.mint = QString::fromLatin1(fincept::wallet::kWrappedSolMint);
    sol_holding_.symbol = QStringLiteral("SOL");
    sol_holding_.name = QStringLiteral("Solana");
    sol_holding_.decimals = 9;
    sol_holding_.verified = true;
    sol_holding_.amount_raw = QStringLiteral("0");

    // Default selection: BUY $FNCPT direction.
    from_mint_ = QString::fromLatin1(fincept::wallet::kWrappedSolMint);
    to_mint_ = QString::fromLatin1(fincept::wallet::kFncptMint);

    debounce_timer_ = new QTimer(this);
    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(kDebounceMs);
    connect(debounce_timer_, &QTimer::timeout, this, &SwapPanel::recompute_estimate);

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &SwapPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this, &SwapPanel::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this, &SwapPanel::on_topic_error);

    rebuild_from_combo();
    rebuild_to_combo();

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
    head_title_ = new QLabel(tr("SWAP"), head);
    head_title_->setObjectName(QStringLiteral("swapPanelTitle"));
    head_status_ = new QLabel(tr("via PumpPortal · pool=auto"), head);
    head_status_->setObjectName(QStringLiteral("swapPanelHeadStatus"));
    head_l->addWidget(head_title_);
    head_l->addStretch();
    head_l->addWidget(head_status_);
    root->addWidget(head);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("swapPanelBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    // FROM (token + amount + MAX)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        auto* col_l = new QVBoxLayout;
        col_l->setSpacing(2);
        pay_caption_ = new QLabel(tr("YOU PAY"), body);
        pay_caption_->setObjectName(QStringLiteral("swapPanelCaption"));
        col_l->addWidget(pay_caption_);
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
        from_caption_ = new QLabel(tr("FROM"), body);
        from_caption_->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(from_caption_);
        from_combo_ = new QComboBox(body);
        from_combo_->setObjectName(QStringLiteral("swapPanelCombo"));
        from_combo_->setFixedHeight(34);
        from_combo_->setMinimumWidth(140);
        col_t->addWidget(from_combo_);
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

    // TO (estimate + token combo)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        auto* col_l = new QVBoxLayout;
        col_l->setSpacing(2);
        receive_caption_ = new QLabel(tr("YOU RECEIVE (EST.)"), body);
        receive_caption_->setObjectName(QStringLiteral("swapPanelCaption"));
        col_l->addWidget(receive_caption_);
        out_amount_label_ = new QLabel(QStringLiteral("—"), body);
        out_amount_label_->setObjectName(QStringLiteral("swapPanelOutAmount"));
        out_amount_label_->setFixedHeight(34);
        col_l->addWidget(out_amount_label_);
        row->addLayout(col_l, 1);

        auto* col_t = new QVBoxLayout;
        col_t->setSpacing(2);
        to_caption_ = new QLabel(tr("TO"), body);
        to_caption_->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(to_caption_);
        to_combo_ = new QComboBox(body);
        to_combo_->setObjectName(QStringLiteral("swapPanelCombo"));
        to_combo_->setFixedHeight(34);
        to_combo_->setMinimumWidth(140);
        col_t->addWidget(to_combo_);
        row->addLayout(col_t);
        bl->addLayout(row);
    }

    // Quote details
    {
        auto* details = new QFrame(body);
        details->setObjectName(QStringLiteral("swapPanelDetailsBlock"));
        auto* dl = new QVBoxLayout(details);
        dl->setContentsMargins(10, 8, 10, 8);
        dl->setSpacing(4);

        auto add_kv = [details, dl](const QString& cap, QLabel*& cap_out, QLabel*& v) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            cap_out = new QLabel(cap, details);
            cap_out->setObjectName(QStringLiteral("swapPanelCaption"));
            v = new QLabel(QStringLiteral("—"), details);
            v->setObjectName(QStringLiteral("swapPanelDetailValue"));
            row->addWidget(cap_out);
            row->addStretch(1);
            row->addWidget(v);
            dl->addLayout(row);
        };
        add_kv(tr("ROUTE"), route_caption_, route_label_);
        add_kv(tr("PRICE IMPACT"), impact_caption_, impact_label_);
        add_kv(tr("MAX SLIPPAGE"), slippage_caption_, slippage_label_);
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

    connect(from_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SwapPanel::on_from_changed);
    connect(to_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SwapPanel::on_to_changed);
    connect(amount_input_, &QLineEdit::textEdited, this, &SwapPanel::on_amount_changed);
    connect(max_button_, &QPushButton::clicked, this, &SwapPanel::on_max_clicked);
    connect(swap_button_, &QPushButton::clicked, this, &SwapPanel::on_swap_clicked);

    // ── Accessibility ─────────────────────────────────────────────────────
    // Every control that carries or moves money gets a spoken name, and the
    // ticket has an explicit tab order (amount → from → MAX → to → SWAP) so
    // a keyboard user walks the ticket in the order they reason about it
    // rather than in widget-construction order.
    amount_input_->setAccessibleName(tr("Amount to pay"));
    amount_input_->setAccessibleDescription(tr("Quantity of the FROM token to swap"));
    from_combo_->setAccessibleName(tr("Pay with token"));
    to_combo_->setAccessibleName(tr("Receive token"));
    max_button_->setAccessibleName(tr("Use maximum available balance"));
    swap_button_->setAccessibleName(tr("Build and review swap transaction"));
    out_amount_label_->setAccessibleName(tr("Estimated amount received"));
    in_balance_label_->setAccessibleName(tr("Available balance"));
    status_label_->setAccessibleName(tr("Swap status"));
    error_text_->setAccessibleName(tr("Swap error"));

    setTabOrder(amount_input_, from_combo_);
    setTabOrder(from_combo_, max_button_);
    setTabOrder(max_button_, to_combo_);
    setTabOrder(to_combo_, swap_button_);
}

void SwapPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss =
        QStringLiteral("QWidget#swapPanel { background:%1; }"
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
                       "  border-color:%3; }")
            .arg(BG_BASE(),                  // %1
                 BG_SURFACE(),               // %2
                 BORDER_DIM(),               // %3
                 AMBER(),                    // %4
                 font,                       // %5
                 TEXT_TERTIARY(),            // %6
                 TEXT_PRIMARY(),             // %7
                 BG_RAISED(),                // %8
                 NEGATIVE())                 // %9
            .arg(BG_HOVER(),                 // %10
                 BORDER_BRIGHT(),            // %11
                 QStringLiteral("#78350f")); // %12 darker amber

    setStyleSheet(ss);
}

// ── Public API ─────────────────────────────────────────────────────────────

void SwapPanel::set_from_mint(const QString& mint) {
    if (mint.isEmpty())
        return;
    // Try to apply now; if the mint isn't in the combo yet, queue it for the
    // next rebuild_from_combo() (which fires on every balance publish).
    int idx = from_combo_ ? from_combo_->findData(mint) : -1;
    if (idx >= 0 && from_combo_) {
        from_combo_->setCurrentIndex(idx);
        pending_from_mint_.clear();
    } else {
        pending_from_mint_ = mint;
    }
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void SwapPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) {
        auto& hub = fincept::datahub::DataHub::instance();
        if (current_balance_topic_.isEmpty()) {
            current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(pubkey);
            hub.subscribe(this, current_balance_topic_, [this](const QVariant& v) { on_balance_update(v); });
            hub.request(current_balance_topic_, /*force=*/true);
        }
        resubscribe_prices();
    }
    set_busy(false);
    update_balance_label();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    current_pubkey_.clear();
    latest_balance_ = {};
    sol_holding_.amount_raw = QStringLiteral("0");
    price_usd_.clear();
    price_sol_.clear();
    price_ts_.clear();
    price_topic_.clear();
    last_quote_age_ms_ = -1;
    status_label_->setTextFormat(Qt::PlainText);
    rebuild_from_combo();
    rebuild_to_combo();
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
        hub.subscribe(this, current_balance_topic_, [this](const QVariant& v) { on_balance_update(v); });
        hub.request(current_balance_topic_, /*force=*/true);
    }
    resubscribe_prices();
    slippage_label_->setText(format_bps(slippage_pct() * 100));
    route_label_->setText(QStringLiteral("PumpSwap (auto)"));
}

void SwapPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    // unsubscribe(this) detaches us from every topic (balance + every price).
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    price_topic_.clear();
    debounce_timer_->stop();
}

void SwapPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void SwapPanel::retranslateUi() {
    // Head + fixed captions.
    if (head_title_)
        head_title_->setText(tr("SWAP"));
    if (head_status_)
        head_status_->setText(tr("via PumpPortal · pool=auto"));
    if (pay_caption_)
        pay_caption_->setText(tr("YOU PAY"));
    if (from_caption_)
        from_caption_->setText(tr("FROM"));
    if (receive_caption_)
        receive_caption_->setText(tr("YOU RECEIVE (EST.)"));
    if (to_caption_)
        to_caption_->setText(tr("TO"));
    if (route_caption_)
        route_caption_->setText(tr("ROUTE"));
    if (impact_caption_)
        impact_caption_->setText(tr("PRICE IMPACT"));
    if (slippage_caption_)
        slippage_caption_->setText(tr("MAX SLIPPAGE"));
    if (max_button_)
        max_button_->setText(tr("MAX"));

    // Re-render state-dependent labels (balance, status, estimate, route /
    // impact / slippage values) in the new locale.
    update_balance_label();
    recompute_estimate();
}

// ── State updates ──────────────────────────────────────────────────────────

void SwapPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>())
        return;
    latest_balance_ = v.value<fincept::wallet::WalletBalance>();
    // Mirror native SOL into the synthetic holding so MAX & balance label
    // can read it the same way as any SPL token.
    sol_holding_.amount_raw = QString::number(latest_balance_.sol_lamports);
    rebuild_from_combo();
    rebuild_to_combo();
    if (isVisible())
        resubscribe_prices();
    update_balance_label();
    recompute_estimate();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_price_update(const QString& mint, const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>())
        return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (!p.valid)
        return;
    price_usd_.insert(mint, p.usd);
    price_sol_.insert(mint, p.sol);
    price_ts_.insert(mint, p.ts_ms > 0 ? p.ts_ms : QDateTime::currentMSecsSinceEpoch());
    recompute_estimate();
}

void SwapPanel::on_topic_error(const QString& topic, const QString& error) {
    // Surface only the active leg's price errors. Other topic errors are
    // handled by HoldingsBar / HomeTab.
    const auto from_topic = price_topic_for(from_mint_);
    const auto to_topic = price_topic_for(to_mint_);
    if (topic == from_topic || topic == to_topic) {
        show_error_strip(tr("Price unavailable: %1. Try again in a moment.").arg(error));
        swap_button_->setEnabled(false);
    }
}

void SwapPanel::on_amount_changed(const QString& /*s*/) {
    debounce_timer_->start();
    out_amount_label_->setText(QStringLiteral("…"));
    swap_button_->setEnabled(false);
    clear_error_strip();
}

void SwapPanel::on_from_changed(int idx) {
    if (!from_combo_ || idx < 0)
        return;
    const auto mint = from_combo_->itemData(idx).toString();
    if (mint.isEmpty() || mint == from_mint_)
        return;
    from_mint_ = mint;
    // If FROM == TO, flip the TO to the canonical counterpart so the user
    // never sees a same-token "swap".
    if (to_mint_ == from_mint_) {
        to_mint_ = is_fncpt(from_mint_) ? QString::fromLatin1(fincept::wallet::kWrappedSolMint)
                                        : QString::fromLatin1(fincept::wallet::kFncptMint);
        rebuild_to_combo();
    }
    resubscribe_prices();
    update_balance_label();
    debounce_timer_->start();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_to_changed(int idx) {
    if (!to_combo_ || idx < 0)
        return;
    const auto mint = to_combo_->itemData(idx).toString();
    if (mint.isEmpty() || mint == to_mint_)
        return;
    to_mint_ = mint;
    resubscribe_prices();
    debounce_timer_->start();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_max_clicked() {
    const auto* h = holding_for(from_mint_);
    if (!h)
        return;
    double bal = h->ui_amount();
    if (bal <= 0.0)
        return;
    // Reserve a small SOL cushion for fees only when paying in SOL.
    if (is_native_sol(from_mint_))
        bal = std::max(0.0, bal - 0.005);
    QSignalBlocker b(amount_input_);
    // Floor at the token's own decimals so the value can never exceed the
    // held balance (which would make can_submit() reject MAX outright).
    amount_input_->setText(swap_amount_for_input(bal, h->decimals));
    on_amount_changed(amount_input_->text());
}

// ── Combo population ───────────────────────────────────────────────────────

void SwapPanel::rebuild_from_combo() {
    if (!from_combo_)
        return;
    QSignalBlocker b(from_combo_);
    from_combo_->clear();

    // Always include native SOL at the top so an empty wallet still has a
    // recognisable FROM (matches how PumpPortal frames trades — SOL is the
    // canonical paying side).
    from_combo_->addItem(resolve_symbol(QString::fromLatin1(fincept::wallet::kWrappedSolMint)),
                         QString::fromLatin1(fincept::wallet::kWrappedSolMint));

    // Holdings sorted by USD value (already done at publish time in
    // WalletBalanceProducer); skip the wSOL entry from `tokens` to avoid a
    // duplicate (native SOL is already at index 0). Same for unverified
    // tokens unless the user opted in via SettingsTab.
    // SettingsTab / HoldingsTable persist this flag as "true"/"false"; the old
    // comparison against "1" here never matched, so the user's opt-in was
    // silently ignored in the FROM combo. Accept both spellings.
    bool show_unverified = false;
    auto sr = SecureStorage::instance().retrieve(QStringLiteral("wallet.show_unverified_tokens"));
    if (sr.is_ok()) {
        const QString v = sr.value().trimmed();
        show_unverified = (v.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) || v == QLatin1String("1");
    }

    for (const auto& t : latest_balance_.tokens) {
        if (is_native_sol(t.mint))
            continue;
        if (!t.verified && !show_unverified && !is_fncpt(t.mint))
            continue;
        if (t.ui_amount() <= 0.0)
            continue;
        from_combo_->addItem(resolve_symbol(t.mint), t.mint);
    }

    // Also ensure FNCPT is always present (so the user can SELL even if they
    // hold zero — e.g. just to see the UI), positioned after holdings.
    if (from_combo_->findData(QString::fromLatin1(fincept::wallet::kFncptMint)) < 0) {
        from_combo_->addItem(resolve_symbol(QString::fromLatin1(fincept::wallet::kFncptMint)),
                             QString::fromLatin1(fincept::wallet::kFncptMint));
    }

    // Restore selection: pending override > current from_mint_ > index 0.
    QString want = !pending_from_mint_.isEmpty() ? pending_from_mint_ : from_mint_;
    int idx = from_combo_->findData(want);
    if (idx < 0 && !pending_from_mint_.isEmpty()) {
        // Pending mint isn't in the combo yet — keep the queue and pick
        // current from_mint_ (or first row).
        idx = from_combo_->findData(from_mint_);
    }
    if (idx < 0)
        idx = 0;
    from_combo_->setCurrentIndex(idx);
    if (idx >= 0) {
        const auto picked = from_combo_->itemData(idx).toString();
        if (!picked.isEmpty())
            from_mint_ = picked;
    }
    if (!pending_from_mint_.isEmpty() && from_mint_ == pending_from_mint_) {
        pending_from_mint_.clear();
    }
}

void SwapPanel::rebuild_to_combo() {
    if (!to_combo_)
        return;
    QSignalBlocker b(to_combo_);
    to_combo_->clear();

    // Phase 2 valid TO targets are the counterpart of FROM. Even though
    // PumpPortal only supports SOL↔FNCPT, we expose the combo so the user
    // sees the surface area; can_submit() gates the actual SWAP.
    auto add = [this](const QString& mint) {
        if (mint == from_mint_)
            return; // never list FROM==TO
        const int idx = to_combo_->findData(mint);
        if (idx < 0)
            to_combo_->addItem(resolve_symbol(mint), mint);
    };
    add(QString::fromLatin1(fincept::wallet::kFncptMint));
    add(QString::fromLatin1(fincept::wallet::kWrappedSolMint));

    // Append other holdings as TO targets (display-only for Phase 2; SWAP
    // gated for unsupported pairs).
    for (const auto& t : latest_balance_.tokens) {
        if (is_native_sol(t.mint))
            continue;
        if (is_fncpt(t.mint))
            continue;
        if (!t.verified)
            continue;
        add(t.mint);
    }

    int idx = to_combo_->findData(to_mint_);
    if (idx < 0)
        idx = 0;
    to_combo_->setCurrentIndex(idx);
    if (idx >= 0) {
        const auto picked = to_combo_->itemData(idx).toString();
        if (!picked.isEmpty())
            to_mint_ = picked;
    }
}

// ── Subscriptions ──────────────────────────────────────────────────────────

void SwapPanel::resubscribe_prices() {
    if (!isVisible())
        return;
    auto& hub = fincept::datahub::DataHub::instance();

    // NOTE — do NOT `unsubscribe(this)` here. This method is reached from
    // `on_balance_update`, which is itself a hub callback. Dropping *all*
    // subscriptions and re-subscribing the balance topic makes DataHub
    // re-deliver the cached balance via `deliver_initial_value` (a queued
    // invoke), which re-enters `on_balance_update` → `resubscribe_prices` →
    // … forever. That spun the event loop at 100 % CPU for as long as the
    // panel was visible. Diff the price topics instead and leave the balance
    // subscription alone (mirrors HoldingsBar / HoldingsTable).
    QSet<QString> wanted;
    if (!from_mint_.isEmpty())
        wanted.insert(from_mint_);
    if (!to_mint_.isEmpty())
        wanted.insert(to_mint_);

    for (auto it = price_topic_.begin(); it != price_topic_.end();) {
        if (!wanted.contains(it.key())) {
            hub.unsubscribe(this, it.value());
            price_usd_.remove(it.key());
            price_sol_.remove(it.key());
            price_ts_.remove(it.key());
            it = price_topic_.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& mint : wanted) {
        if (price_topic_.contains(mint))
            continue;
        const auto topic = price_topic_for(mint);
        price_topic_.insert(mint, topic);
        hub.subscribe(this, topic, [this, mint](const QVariant& v) { on_price_update(mint, v); });
        hub.request(topic, /*force=*/false);
    }
}

void SwapPanel::update_balance_label() {
    const auto* h = holding_for(from_mint_);
    const QString sym = resolve_symbol(from_mint_);
    if (!h) {
        in_balance_label_->setText(tr("Balance: 0 %1").arg(sym));
        return;
    }
    const int dp = is_native_sol(from_mint_) ? 4 : (is_fncpt(from_mint_) ? 2 : 4);
    in_balance_label_->setText(tr("Balance: %1 %2").arg(format_token(h->ui_amount(), dp)).arg(sym));
}

void SwapPanel::recompute_estimate() {
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0) {
        out_amount_label_->setText(QStringLiteral("—"));
        swap_button_->setEnabled(false);
        return;
    }

    const double from_sol = price_sol_.value(from_mint_, 0.0);
    const double from_usd = price_usd_.value(from_mint_, 0.0);
    const double to_sol = price_sol_.value(to_mint_, 0.0);
    const double to_usd = price_usd_.value(to_mint_, 0.0);

    if (from_sol <= 0.0 || from_usd <= 0.0 || to_sol <= 0.0 || to_usd <= 0.0) {
        out_amount_label_->setText(tr("estimate unavailable"));
        status_label_->setText(tr("Waiting for spot prices…"));
        swap_button_->setEnabled(false);
        return;
    }

    // SOL-denominated cross: out_units = in_units * (from_sol / to_sol).
    // `est_out_mid` is the unadjusted cross; `est_out_min` is what the user is
    // guaranteed at worst given the slippage budget. Both are surfaced — the
    // panel used to show only the min under an "EST." caption while the confirm
    // dialog showed only the mid, so the two screens disagreed by the whole
    // slippage budget on the single number the user is checking.
    const double slippage_factor = 1.0 - (slippage_pct() / 100.0);
    const double est_out_mid = ui_amount * (from_sol / to_sol);
    const double est_out_min = est_out_mid * slippage_factor;
    const double est_usd = ui_amount * from_usd;

    const QString to_sym = resolve_symbol(to_mint_);
    const int dp = is_native_sol(to_mint_) ? 6 : (is_fncpt(to_mint_) ? 2 : 4);
    const QString out_text = QStringLiteral("≈ %1 %2  (min %3 · ~$%4)")
                                 .arg(format_token(est_out_mid, dp))
                                 .arg(to_sym)
                                 .arg(format_token(est_out_min, dp))
                                 .arg(format_token(est_usd, 2));
    out_amount_label_->setText(out_text);
    route_label_->setText(QStringLiteral("PumpSwap (auto)"));
    impact_label_->setText(tr("set by PumpSwap; capped by slippage"));
    slippage_label_->setText(format_bps(slippage_pct() * 100));

    // Quote freshness — a crypto price with no age on it is dangerous right
    // before a swap. Track the oldest of the two legs.
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    const qint64 from_ts = price_ts_.value(from_mint_, 0);
    const qint64 to_ts = price_ts_.value(to_mint_, 0);
    last_quote_age_ms_ = (from_ts > 0 && to_ts > 0) ? (now_ms - std::min(from_ts, to_ts)) : -1;

    if (!is_supported_pair()) {
        // Show the estimate so the user understands what they're trying to
        // do, but block submit and explain.
        status_label_->setText(unsupported_pair_message());
        swap_button_->setEnabled(false);
        return;
    }
    if (last_quote_age_ms_ > kPriceStaleMs) {
        show_error_strip(tr("Spot price is %1 s old — the estimate above may be wrong. "
                            "PumpSwap still fills at execution within your slippage budget.")
                             .arg(last_quote_age_ms_ / 1000));
        status_label_->setText(tr("Ready — quote %1 s old.").arg(last_quote_age_ms_ / 1000));
    } else {
        clear_error_strip();
        status_label_->setText(tr("Ready. Click SWAP to build the transaction."));
    }
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::show_error_strip(const QString& msg) {
    if (!error_strip_)
        return;
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
    if (from_combo_)
        from_combo_->setEnabled(!busy);
    if (to_combo_)
        to_combo_->setEnabled(!busy);
    max_button_->setEnabled(!busy);
    swap_button_->setEnabled(!busy && can_submit());
}

bool SwapPanel::is_supported_pair() const {
    // PumpPortal `trade-local` only routes SOL↔FNCPT for $FNCPT trades.
    const bool sol_to_fncpt = is_native_sol(from_mint_) && is_fncpt(to_mint_);
    const bool fncpt_to_sol = is_fncpt(from_mint_) && is_native_sol(to_mint_);
    return sol_to_fncpt || fncpt_to_sol;
}

QString SwapPanel::unsupported_pair_message() const {
    return tr("This pair isn't routable in Phase 2. PumpPortal supports "
              "SOL ↔ $FNCPT only; a generalised router lands in Phase 3.");
}

bool SwapPanel::can_submit() const {
    if (busy_)
        return false;
    if (current_pubkey_.isEmpty())
        return false;
    if (!is_supported_pair())
        return false;
    if (price_sol_.value(from_mint_, 0.0) <= 0.0)
        return false;
    if (price_sol_.value(to_mint_, 0.0) <= 0.0)
        return false;
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0)
        return false;
    const auto* h = holding_for(from_mint_);
    if (!h)
        return false;
    if (ui_amount > h->ui_amount())
        return false;
    return true;
}

const fincept::wallet::TokenHolding* SwapPanel::holding_for(const QString& mint) const {
    if (mint.isEmpty())
        return nullptr;
    if (is_native_sol(mint))
        return &sol_holding_;
    for (const auto& t : latest_balance_.tokens) {
        if (t.mint == mint)
            return &t;
    }
    return nullptr;
}

int SwapPanel::slippage_bps() const {
    auto r = SecureStorage::instance().retrieve(QStringLiteral("wallet.default_slippage_bps"));
    if (r.is_ok()) {
        bool ok = false;
        const auto v = r.value().toInt(&ok);
        // Floor at 100 bps: PumpPortal takes an integer percent clamped to
        // [1,5], so anything below 1.00 % is executed as 1.00 % anyway.
        // Accepting a smaller stored value here only produced a display that
        // disagreed with what was enforced.
        if (ok && v >= kDefaultSlippageBps && v <= kMaxSlippageBps)
            return v;
    }
    return kDefaultSlippageBps;
}

int SwapPanel::slippage_pct() const {
    // PumpPortal expects integer percent; round up so we never under-tolerate.
    const int bps = slippage_bps();
    int pct = (bps + 99) / 100;
    if (pct < 1)
        pct = 1;
    if (pct > 5)
        pct = 5;
    return pct;
}

// ── Submit flow ────────────────────────────────────────────────────────────
//
// Sequence:
//   1. build_swap → returns the unsigned tx body from PumpPortal.
//   2. simulate_transaction → MITM gate (Phase 2 plan §2). If simulation
//      reports an `err`, bail before the wallet ever pops.
//   3. WalletActionConfirmDialog → user reviews decoded summary.
//   4. sign_and_send → wallet signs and forwards to RPC.
//   5. Poll getSignatureStatuses until confirmed.
//
// Each step checks the QPointer guard and surfaces failures to the error
// strip / status label. set_busy(true) at step 1; set_busy(false) on any
// terminal outcome.

void SwapPanel::start_status_poll(const QString& sig) {
    auto* rpc = new fincept::wallet::SolanaRpcClient(this);
    rpc->reload_endpoint();
    auto attempts = std::make_shared<int>(0);
    auto* poll_timer = new QTimer(this);
    poll_timer->setInterval(kStatusPollMs);

    // A Solscan link so the user can independently verify the transaction
    // landed — previously the only trace of a submitted swap was a truncated
    // signature in a plain label.
    const QString explorer_url = QStringLiteral("https://solscan.io/tx/") + sig;
    status_label_->setTextFormat(Qt::RichText);
    status_label_->setOpenExternalLinks(true);
    status_label_->setTextInteractionFlags(Qt::TextBrowserInteraction);

    QPointer<SwapPanel> guard = this;
    // `rpc` and `poll_timer` are children of the panel, so they die with it;
    // but a swap-per-swap leak would still accumulate for a long-lived panel.
    // stop_poll tears both down on every terminal outcome.
    auto stop_poll = [rpc, poll_timer]() {
        poll_timer->stop();
        poll_timer->deleteLater();
        rpc->deleteLater();
    };
    QObject::connect(poll_timer, &QTimer::timeout, this,
                     [guard, rpc, sig, explorer_url, attempts, poll_timer, stop_poll]() {
                         if (!guard) {
                             stop_poll();
                             return;
                         }
                         if (++(*attempts) > kStatusPollMaxAttempts) {
                             stop_poll();
                             guard->set_busy(false);
                             guard->show_error_strip(QObject::tr("No confirmation after 60 s. "
                                                                 "The transaction may still land — check the explorer."));
                             guard->status_label_->setText(
                                 QObject::tr("Timed out. <a href=\"%1\">View on Solscan</a>").arg(explorer_url));
                             return;
                         }
                         rpc->get_signature_statuses(
                             QStringList{sig},
                             [guard, sig, explorer_url,
                              stop_poll](Result<std::vector<fincept::wallet::SolanaRpcClient::SignatureStatus>> r) {
                                 if (!guard)
                                     return;
                                 if (r.is_err())
                                     return; // transient — keep polling
                                 const auto& vec = r.value();
                                 if (vec.empty() || !vec[0].found)
                                     return;
                                 const auto& s = vec[0];
                                 if (!s.err.isEmpty()) {
                                     stop_poll();
                                     guard->set_busy(false);
                                     guard->show_error_strip(QObject::tr("Tx failed on-chain: %1").arg(s.err));
                                     guard->status_label_->setText(
                                         QObject::tr("Reverted. <a href=\"%1\">View on Solscan</a>").arg(explorer_url));
                                     return;
                                 }
                                 if (s.confirmation_status == QStringLiteral("confirmed") ||
                                     s.confirmation_status == QStringLiteral("finalized")) {
                                     stop_poll();
                                     guard->set_busy(false);
                                     guard->amount_input_->clear();
                                     guard->out_amount_label_->setText(QStringLiteral("—"));
                                     guard->status_label_->setText(QObject::tr("Confirmed %1… "
                                                                               "<a href=\"%2\">View on Solscan</a>")
                                                                       .arg(sig.left(12), explorer_url));
                                     fincept::wallet::WalletService::instance().force_balance_refresh();
                                 }
                             });
                     });
    poll_timer->start();
}

void SwapPanel::on_swap_clicked() {
    if (!can_submit())
        return;
    auto* svc = fincept::wallet::WalletService::instance().swap_service();
    if (!svc) {
        show_error_strip(tr("Swap service unavailable."));
        return;
    }
    bool ok_d = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok_d);
    if (!ok_d || ui_amount <= 0.0)
        return;

    const auto pubkey = current_pubkey_;
    const int slip_pct = slippage_pct();
    // Display the tolerance actually sent and enforced — slippage_pct() rounds
    // the raw bps up to a whole percent (floor 1, cap 5) and PumpPortal re-clamps
    // to [1,5]. Showing the raw bps here would advertise a tighter tolerance than
    // is executed (e.g. 50 bps sends/enforces 1% but would read "0.50%").
    const int slip_bps_for_display = slip_pct * 100;
    // The supported-pair gate above guarantees this is SOL→FNCPT or FNCPT→SOL.
    const bool buying_fncpt = is_native_sol(from_mint_) && is_fncpt(to_mint_);
    const auto action = buying_fncpt ? fincept::wallet::PumpFunSwapService::Action::Buy
                                     : fincept::wallet::PumpFunSwapService::Action::Sell;
    // BUY: amount is in SOL (denominatedInSol=true).
    // SELL: amount is in FNCPT (denominatedInSol=false).
    const bool denom_in_sol = buying_fncpt;

    const double from_sol_p = price_sol_.value(from_mint_, 0.0);
    const double to_sol_p = price_sol_.value(to_mint_, 0.0);
    const double est_out = (to_sol_p > 0.0) ? (ui_amount * from_sol_p / to_sol_p) : 0.0;
    // Worst case the user is guaranteed. The panel shows both; the dialog used
    // to show only the mid, which reads as a better fill than is contracted.
    const double est_out_min = est_out * (1.0 - slip_pct / 100.0);
    const qint64 quote_age_ms = last_quote_age_ms_;

    set_busy(true);
    clear_error_strip();
    status_label_->setTextFormat(Qt::PlainText);
    status_label_->setText(tr("Building swap transaction…"));

    QPointer<SwapPanel> self = this;
    svc->build_swap(
        action, QString::fromLatin1(fincept::wallet::kFncptMint), ui_amount, denom_in_sol, pubkey, slip_pct,
        kDefaultPriorityFeeSol,
        [self, ui_amount, buying_fncpt, est_out, est_out_min, quote_age_ms, pubkey,
         slip_bps_for_display](Result<fincept::wallet::PumpFunSwapService::SwapTransaction> r) {
            if (!self)
                return;
            if (r.is_err()) {
                self->set_busy(false);
                self->show_error_strip(QObject::tr("build_swap failed: %1").arg(QString::fromStdString(r.error())));
                self->status_label_->setText(QObject::tr("Failed."));
                return;
            }
            const auto tx = r.value();

            // ── MITM gate (Phase 2 §2) ──────────────────────────────────────
            // Simulate the unsigned tx against the user's RPC. If it would
            // revert, refuse to show the wallet — a malicious PumpPortal
            // response that built a draining or malformed tx is caught here.
            self->status_label_->setText(QObject::tr("Validating with RPC…"));
            auto* rpc = new fincept::wallet::SolanaRpcClient(self);
            rpc->reload_endpoint();
            // Which cluster is actually going to execute this. Mainnet vs
            // devnet confusion is silent otherwise — the panel chrome never
            // says which RPC is configured.
            const QString rpc_host = QUrl(rpc->http_endpoint()).host();
            rpc->simulate_transaction(tx.tx_base64, [self, tx, ui_amount, buying_fncpt, est_out, est_out_min,
                                                     quote_age_ms, pubkey, rpc_host, slip_bps_for_display,
                                                     rpc](
                                                        Result<fincept::wallet::SolanaRpcClient::SimulationResult> sr) {
                rpc->deleteLater();
                if (!self)
                    return;
                if (sr.is_err()) {
                    self->set_busy(false);
                    self->show_error_strip(QObject::tr("Simulation failed: %1. Refusing to sign.")
                                               .arg(QString::fromStdString(sr.error())));
                    self->status_label_->setText(QObject::tr("Aborted."));
                    return;
                }
                const auto sim = sr.value();
                if (!sim.ok) {
                    LOG_WARN("SwapPanel", "simulateTransaction reported err: " + sim.err);
                    self->set_busy(false);
                    self->show_error_strip(QObject::tr("This swap would fail on-chain: %1. "
                                                       "Refusing to sign.")
                                               .arg(sim.err));
                    self->status_label_->setText(QObject::tr("Aborted."));
                    return;
                }

                // ── Confirm dialog ──────────────────────────────────────────
                WalletActionSummary summary;
                summary.title = QObject::tr("SWAP");
                summary.lede = QObject::tr("Approve in your wallet to forward this transaction "
                                           "to the network. The terminal does not hold any funds.");
                summary.rows.append({QObject::tr("ROUTE"), QStringLiteral("PumpSwap (pool=auto)"), true});
                if (buying_fncpt) {
                    summary.rows.append(
                        {QObject::tr("YOU PAY"), QStringLiteral("%1 SOL").arg(format_token(ui_amount, 6)), true});
                    summary.rows.append(
                        {QObject::tr("YOU RECEIVE"),
                         QObject::tr("≈ %1 $FNCPT (PumpSwap fills at execution)").arg(format_token(est_out, 2)), true});
                    summary.rows.append({QObject::tr("MINIMUM RECEIVED"),
                                         QStringLiteral("%1 $FNCPT").arg(format_token(est_out_min, 2)), true});
                } else {
                    summary.rows.append(
                        {QObject::tr("YOU PAY"), QStringLiteral("%1 $FNCPT").arg(format_token(ui_amount, 2)), true});
                    summary.rows.append(
                        {QObject::tr("YOU RECEIVE"),
                         QObject::tr("≈ %1 SOL (PumpSwap fills at execution)").arg(format_token(est_out, 6)), true});
                    summary.rows.append({QObject::tr("MINIMUM RECEIVED"),
                                         QStringLiteral("%1 SOL").arg(format_token(est_out_min, 6)), true});
                }
                summary.rows.append({QObject::tr("MAX SLIPPAGE"), format_bps(slip_bps_for_display), true});
                summary.rows.append({QObject::tr("PRIORITY FEE"),
                                     QStringLiteral("%1 SOL").arg(format_token(kDefaultPriorityFeeSol, 6)), true});
                // Verifiable identity rows. The full mint and the full signing
                // pubkey are printed verbatim — a truncated address is not
                // something a user can check against their wallet.
                summary.rows.append(
                    {QObject::tr("TOKEN MINT"), QString::fromLatin1(fincept::wallet::kFncptMint), true});
                summary.rows.append({QObject::tr("SIGNING WALLET"), pubkey, true});
                summary.rows.append({QObject::tr("NETWORK"),
                                     rpc_host.isEmpty() ? QObject::tr("unknown RPC") : rpc_host, true});
                summary.rows.append({QObject::tr("QUOTE AGE"),
                                     quote_age_ms >= 0 ? QObject::tr("%1 s").arg(quote_age_ms / 1000)
                                                       : QObject::tr("unknown"),
                                     true});
                summary.rows.append({QObject::tr("RPC SIMULATION"),
                                     QObject::tr("OK · %1 CU").arg(QString::number(sim.units_consumed)), true});
                summary.warnings.append(QObject::tr("PumpSwap will reject the trade if execution drifts more than "
                                                    "the slippage tolerance above. Your funds stay in your wallet."));
                if (quote_age_ms > kPriceStaleMs) {
                    summary.warnings.append(
                        QObject::tr("The spot price used for the estimate is %1 s old. The amounts above are "
                                    "indicative only — the exchange rate is decided at execution.")
                            .arg(quote_age_ms / 1000));
                }
                summary.primary_button_text = QObject::tr("SWAP");
                summary.primary_is_safe = true;
                summary.arm_delay_ms = 1500;

                auto* dlg = new WalletActionConfirmDialog(summary, self);
                const QString tx_b64 = tx.tx_base64;
                QObject::connect(dlg, &WalletActionConfirmDialog::confirmed, self, [self, tx_b64]() {
                    if (!self)
                        return;

                    // ── Freshness gate (Phase 2 §2) ───────────────────────────
                    // The tx may have been sitting in the dialog for several
                    // seconds. Re-simulate with replace=false so an expired
                    // blockhash is caught before the wallet signs. If the
                    // simulation fails with BlockhashNotFound (or any other
                    // freshness-related error), abort and ask the user to retry.
                    self->status_label_->setText(QObject::tr("Re-checking freshness…"));
                    auto* fresh_rpc = new fincept::wallet::SolanaRpcClient(self);
                    fresh_rpc->reload_endpoint();
                    fresh_rpc->simulate_transaction(
                        tx_b64,
                        [self, tx_b64, fresh_rpc](Result<fincept::wallet::SolanaRpcClient::SimulationResult> fr) {
                            fresh_rpc->deleteLater();
                            if (!self)
                                return;
                            if (fr.is_err()) {
                                self->set_busy(false);
                                self->show_error_strip(QObject::tr("Could not verify freshness: %1. "
                                                                   "Try the swap again.")
                                                           .arg(QString::fromStdString(fr.error())));
                                self->status_label_->setText(QObject::tr("Aborted."));
                                return;
                            }
                            const auto fsim = fr.value();
                            if (!fsim.ok) {
                                // BlockhashNotFound is the canonical staleness error.
                                // Anything else here also means "don't sign."
                                self->set_busy(false);
                                self->show_error_strip(QObject::tr("This swap is no longer fresh: %1. "
                                                                   "Click SWAP again to rebuild.")
                                                           .arg(fsim.err));
                                self->status_label_->setText(QObject::tr("Stale."));
                                return;
                            }

                            // ── Sign and send ─────────────────────────────────────
                            self->status_label_->setText(QObject::tr("Awaiting wallet signature…"));
                            fincept::wallet::WalletService::instance().sign_and_send(
                                tx_b64, QObject::tr("Sign swap"),
                                QObject::tr("Approve the swap in your wallet to complete the trade."), self,
                                [self](Result<QString> sr2) {
                                    if (!self)
                                        return;
                                    if (sr2.is_err()) {
                                        self->set_busy(false);
                                        self->show_error_strip(
                                            QObject::tr("Signing failed: %1").arg(QString::fromStdString(sr2.error())));
                                        self->status_label_->setText(QObject::tr("Cancelled."));
                                        return;
                                    }
                                    const auto sig = sr2.value();
                                    self->status_label_->setText(QObject::tr("Sent. Waiting for confirmation…"));
                                    LOG_INFO("SwapPanel", "submitted: " + sig);
                                    self->start_status_poll(sig);
                                });
                        },
                        /*replace_recent_blockhash=*/false);
                });
                // `cancelled()` only fires from the CANCEL button. Esc, the
                // window close box and any programmatic reject() left the panel
                // permanently busy (inputs disabled, SWAP dead) with no way
                // out short of a wallet reconnect. `rejected()` covers every
                // dismissal path and the handler is idempotent.
                QObject::connect(dlg, &QDialog::rejected, self, [self]() {
                    if (!self)
                        return;
                    self->set_busy(false);
                    self->status_label_->setTextFormat(Qt::PlainText);
                    self->status_label_->setText(self->tr("Cancelled."));
                });
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->open();
            });
        });
}

} // namespace fincept::screens::panels
