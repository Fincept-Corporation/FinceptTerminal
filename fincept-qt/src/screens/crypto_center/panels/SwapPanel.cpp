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

#include <algorithm>

namespace fincept::screens::panels {

namespace {

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
    if (is_native_sol(mint)) return QStringLiteral("SOL");
    if (is_fncpt(mint)) return QStringLiteral("$FNCPT");
    auto md = fincept::wallet::TokenMetadataService::instance().lookup(mint);
    if (md && !md->symbol.isEmpty()) return md->symbol;
    if (mint.size() <= 8) return mint;
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
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &SwapPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &SwapPanel::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &SwapPanel::on_topic_error);

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
    auto* title = new QLabel(QStringLiteral("SWAP"), head);
    title->setObjectName(QStringLiteral("swapPanelTitle"));
    auto* head_status =
        new QLabel(QStringLiteral("via PumpPortal · pool=auto"), head);
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

    // FROM (token + amount + MAX)
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
        auto* cap_t = new QLabel(QStringLiteral("FROM"), body);
        cap_t->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(cap_t);
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
        auto* cap_t = new QLabel(QStringLiteral("TO"), body);
        cap_t->setObjectName(QStringLiteral("swapPanelCaption"));
        col_t->addWidget(cap_t);
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

    connect(from_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SwapPanel::on_from_changed);
    connect(to_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SwapPanel::on_to_changed);
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

// ── Public API ─────────────────────────────────────────────────────────────

void SwapPanel::set_from_mint(const QString& mint) {
    if (mint.isEmpty()) return;
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
            hub.subscribe(this, current_balance_topic_,
                          [this](const QVariant& v) { on_balance_update(v); });
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
    price_topic_.clear();
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
        hub.subscribe(this, current_balance_topic_,
                      [this](const QVariant& v) { on_balance_update(v); });
        hub.request(current_balance_topic_, /*force=*/true);
    }
    resubscribe_prices();
    slippage_label_->setText(format_bps(slippage_bps()));
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

// ── State updates ──────────────────────────────────────────────────────────

void SwapPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    latest_balance_ = v.value<fincept::wallet::WalletBalance>();
    // Mirror native SOL into the synthetic holding so MAX & balance label
    // can read it the same way as any SPL token.
    sol_holding_.amount_raw = QString::number(latest_balance_.sol_lamports);
    rebuild_from_combo();
    rebuild_to_combo();
    if (isVisible()) resubscribe_prices();
    update_balance_label();
    recompute_estimate();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_price_update(const QString& mint, const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>()) return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (!p.valid) return;
    price_usd_.insert(mint, p.usd);
    price_sol_.insert(mint, p.sol);
    recompute_estimate();
}

void SwapPanel::on_topic_error(const QString& topic, const QString& error) {
    // Surface only the active leg's price errors. Other topic errors are
    // handled by HoldingsBar / HomeTab.
    const auto from_topic = price_topic_for(from_mint_);
    const auto to_topic = price_topic_for(to_mint_);
    if (topic == from_topic || topic == to_topic) {
        show_error_strip(tr("Price unavailable: %1. Try again in a moment.")
                             .arg(error));
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
    if (!from_combo_ || idx < 0) return;
    const auto mint = from_combo_->itemData(idx).toString();
    if (mint.isEmpty() || mint == from_mint_) return;
    from_mint_ = mint;
    // If FROM == TO, flip the TO to the canonical counterpart so the user
    // never sees a same-token "swap".
    if (to_mint_ == from_mint_) {
        to_mint_ = is_fncpt(from_mint_)
            ? QString::fromLatin1(fincept::wallet::kWrappedSolMint)
            : QString::fromLatin1(fincept::wallet::kFncptMint);
        rebuild_to_combo();
    }
    resubscribe_prices();
    update_balance_label();
    debounce_timer_->start();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_to_changed(int idx) {
    if (!to_combo_ || idx < 0) return;
    const auto mint = to_combo_->itemData(idx).toString();
    if (mint.isEmpty() || mint == to_mint_) return;
    to_mint_ = mint;
    resubscribe_prices();
    debounce_timer_->start();
    swap_button_->setEnabled(can_submit());
}

void SwapPanel::on_max_clicked() {
    const auto* h = holding_for(from_mint_);
    if (!h) return;
    double bal = h->ui_amount();
    if (bal <= 0.0) return;
    // Reserve a small SOL cushion for fees only when paying in SOL.
    if (is_native_sol(from_mint_)) bal = std::max(0.0, bal - 0.005);
    QSignalBlocker b(amount_input_);
    amount_input_->setText(format_token(bal, 6));
    on_amount_changed(amount_input_->text());
}

// ── Combo population ───────────────────────────────────────────────────────

void SwapPanel::rebuild_from_combo() {
    if (!from_combo_) return;
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
    bool show_unverified = false;
    auto sr = SecureStorage::instance().retrieve(
        QStringLiteral("wallet.show_unverified_tokens"));
    if (sr.is_ok()) show_unverified = (sr.value() == QLatin1String("1"));

    for (const auto& t : latest_balance_.tokens) {
        if (is_native_sol(t.mint)) continue;
        if (!t.verified && !show_unverified && !is_fncpt(t.mint)) continue;
        if (t.ui_amount() <= 0.0) continue;
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
    if (idx < 0) idx = 0;
    from_combo_->setCurrentIndex(idx);
    if (idx >= 0) {
        const auto picked = from_combo_->itemData(idx).toString();
        if (!picked.isEmpty()) from_mint_ = picked;
    }
    if (!pending_from_mint_.isEmpty() && from_mint_ == pending_from_mint_) {
        pending_from_mint_.clear();
    }
}

void SwapPanel::rebuild_to_combo() {
    if (!to_combo_) return;
    QSignalBlocker b(to_combo_);
    to_combo_->clear();

    // Phase 2 valid TO targets are the counterpart of FROM. Even though
    // PumpPortal only supports SOL↔FNCPT, we expose the combo so the user
    // sees the surface area; can_submit() gates the actual SWAP.
    auto add = [this](const QString& mint) {
        if (mint == from_mint_) return; // never list FROM==TO
        const int idx = to_combo_->findData(mint);
        if (idx < 0) to_combo_->addItem(resolve_symbol(mint), mint);
    };
    add(QString::fromLatin1(fincept::wallet::kFncptMint));
    add(QString::fromLatin1(fincept::wallet::kWrappedSolMint));

    // Append other holdings as TO targets (display-only for Phase 2; SWAP
    // gated for unsupported pairs).
    for (const auto& t : latest_balance_.tokens) {
        if (is_native_sol(t.mint)) continue;
        if (is_fncpt(t.mint)) continue;
        if (!t.verified) continue;
        add(t.mint);
    }

    int idx = to_combo_->findData(to_mint_);
    if (idx < 0) idx = 0;
    to_combo_->setCurrentIndex(idx);
    if (idx >= 0) {
        const auto picked = to_combo_->itemData(idx).toString();
        if (!picked.isEmpty()) to_mint_ = picked;
    }
}

// ── Subscriptions ──────────────────────────────────────────────────────────

void SwapPanel::resubscribe_prices() {
    if (!isVisible()) return;
    auto& hub = fincept::datahub::DataHub::instance();
    // Drop any prior price subs we own before resubscribing the active legs.
    // (We can't selectively unsubscribe by topic; unsubscribe(this) drops
    // everything including the balance topic. Resubscribe both legs +
    // balance every time — it's cheap and keeps lifetimes obvious.)
    hub.unsubscribe(this);
    price_topic_.clear();

    if (!current_balance_topic_.isEmpty()) {
        hub.subscribe(this, current_balance_topic_,
                      [this](const QVariant& v) { on_balance_update(v); });
    }

    auto sub_price = [this, &hub](const QString& mint) {
        if (mint.isEmpty()) return;
        const auto topic = price_topic_for(mint);
        price_topic_.insert(mint, topic);
        hub.subscribe(this, topic,
                      [this, mint](const QVariant& v) { on_price_update(mint, v); });
        hub.request(topic, /*force=*/false);
    };
    sub_price(from_mint_);
    sub_price(to_mint_);
}

void SwapPanel::update_balance_label() {
    const auto* h = holding_for(from_mint_);
    const QString sym = resolve_symbol(from_mint_);
    if (!h) {
        in_balance_label_->setText(tr("Balance: 0 %1").arg(sym));
        return;
    }
    const int dp = is_native_sol(from_mint_) ? 4 : (is_fncpt(from_mint_) ? 2 : 4);
    in_balance_label_->setText(
        tr("Balance: %1 %2").arg(format_token(h->ui_amount(), dp)).arg(sym));
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
        out_amount_label_->setText(QStringLiteral("estimate unavailable"));
        status_label_->setText(tr("Waiting for spot prices…"));
        swap_button_->setEnabled(false);
        return;
    }

    // SOL-denominated cross: out_units = in_units * (from_sol / to_sol).
    const double slippage_factor = 1.0 - (slippage_pct() / 100.0);
    const double est_out = ui_amount * (from_sol / to_sol) * slippage_factor;
    const double est_usd = ui_amount * from_usd;

    const QString to_sym = resolve_symbol(to_mint_);
    const int dp = is_native_sol(to_mint_) ? 6 : (is_fncpt(to_mint_) ? 2 : 4);
    const QString out_text = QStringLiteral("≈ %1 %2  (~$%3)")
                                 .arg(format_token(est_out, dp))
                                 .arg(to_sym)
                                 .arg(format_token(est_usd, 2));
    out_amount_label_->setText(out_text);
    route_label_->setText(QStringLiteral("PumpSwap (auto)"));
    impact_label_->setText(tr("set by PumpSwap; capped by slippage"));
    slippage_label_->setText(format_bps(slippage_bps()));

    if (!is_supported_pair()) {
        // Show the estimate so the user understands what they're trying to
        // do, but block submit and explain.
        status_label_->setText(unsupported_pair_message());
        swap_button_->setEnabled(false);
        return;
    }
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
    if (from_combo_) from_combo_->setEnabled(!busy);
    if (to_combo_) to_combo_->setEnabled(!busy);
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
    if (busy_) return false;
    if (current_pubkey_.isEmpty()) return false;
    if (!is_supported_pair()) return false;
    if (price_sol_.value(from_mint_, 0.0) <= 0.0) return false;
    if (price_sol_.value(to_mint_, 0.0) <= 0.0) return false;
    bool ok = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || ui_amount <= 0.0) return false;
    const auto* h = holding_for(from_mint_);
    if (!h) return false;
    if (ui_amount > h->ui_amount()) return false;
    return true;
}

const fincept::wallet::TokenHolding* SwapPanel::holding_for(const QString& mint) const {
    if (mint.isEmpty()) return nullptr;
    if (is_native_sol(mint)) return &sol_holding_;
    for (const auto& t : latest_balance_.tokens) {
        if (t.mint == mint) return &t;
    }
    return nullptr;
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
    QPointer<SwapPanel> guard = this;
    QObject::connect(poll_timer, &QTimer::timeout, this,
                     [guard, rpc, sig, attempts, poll_timer]() {
        if (!guard) {
            poll_timer->stop();
            poll_timer->deleteLater();
            return;
        }
        if (++(*attempts) > kStatusPollMaxAttempts) {
            poll_timer->stop();
            poll_timer->deleteLater();
            guard->set_busy(false);
            guard->show_error_strip(
                QObject::tr("No confirmation after 60 s. Check Solscan."));
            guard->status_label_->setText(QObject::tr("Timed out."));
            return;
        }
        rpc->get_signature_statuses(QStringList{sig},
            [guard, sig, poll_timer](
                Result<std::vector<fincept::wallet::SolanaRpcClient::SignatureStatus>> r) {
            if (!guard) return;
            if (r.is_err()) return; // transient — keep polling
            const auto& vec = r.value();
            if (vec.empty() || !vec[0].found) return;
            const auto& s = vec[0];
            if (!s.err.isEmpty()) {
                poll_timer->stop();
                poll_timer->deleteLater();
                guard->set_busy(false);
                guard->show_error_strip(
                    QObject::tr("Tx failed on-chain: %1").arg(s.err));
                guard->status_label_->setText(QObject::tr("Reverted."));
                return;
            }
            if (s.confirmation_status == QStringLiteral("confirmed")
                || s.confirmation_status == QStringLiteral("finalized")) {
                poll_timer->stop();
                poll_timer->deleteLater();
                guard->set_busy(false);
                guard->amount_input_->clear();
                guard->out_amount_label_->setText(QStringLiteral("—"));
                guard->status_label_->setText(
                    QObject::tr("Confirmed: %1…").arg(sig.left(12)));
                fincept::wallet::WalletService::instance()
                    .force_balance_refresh();
            }
        });
    });
    poll_timer->start();
}

void SwapPanel::on_swap_clicked() {
    if (!can_submit()) return;
    auto* svc = fincept::wallet::WalletService::instance().swap_service();
    if (!svc) {
        show_error_strip(tr("Swap service unavailable."));
        return;
    }
    bool ok_d = false;
    const double ui_amount = QLocale::system().toDouble(amount_input_->text(), &ok_d);
    if (!ok_d || ui_amount <= 0.0) return;

    const auto pubkey = current_pubkey_;
    const int slip_pct = slippage_pct();
    const int slip_bps_for_display = slippage_bps();
    // The supported-pair gate above guarantees this is SOL→FNCPT or FNCPT→SOL.
    const bool buying_fncpt = is_native_sol(from_mint_) && is_fncpt(to_mint_);
    const auto action = buying_fncpt
                            ? fincept::wallet::PumpFunSwapService::Action::Buy
                            : fincept::wallet::PumpFunSwapService::Action::Sell;
    // BUY: amount is in SOL (denominatedInSol=true).
    // SELL: amount is in FNCPT (denominatedInSol=false).
    const bool denom_in_sol = buying_fncpt;

    const double from_sol_p = price_sol_.value(from_mint_, 0.0);
    const double to_sol_p = price_sol_.value(to_mint_, 0.0);
    const double est_out = (to_sol_p > 0.0) ? (ui_amount * from_sol_p / to_sol_p) : 0.0;

    set_busy(true);
    clear_error_strip();
    status_label_->setText(tr("Building swap transaction…"));

    QPointer<SwapPanel> self = this;
    svc->build_swap(action,
                    QString::fromLatin1(fincept::wallet::kFncptMint),
                    ui_amount,
                    denom_in_sol,
                    pubkey,
                    slip_pct,
                    kDefaultPriorityFeeSol,
                    [self, ui_amount, buying_fncpt, est_out, slip_bps_for_display](
                        Result<fincept::wallet::PumpFunSwapService::SwapTransaction> r) {
        if (!self) return;
        if (r.is_err()) {
            self->set_busy(false);
            self->show_error_strip(
                QObject::tr("build_swap failed: %1")
                    .arg(QString::fromStdString(r.error())));
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
        rpc->simulate_transaction(tx.tx_base64,
            [self, tx, ui_amount, buying_fncpt, est_out, slip_bps_for_display, rpc](
                Result<fincept::wallet::SolanaRpcClient::SimulationResult> sr) {
            rpc->deleteLater();
            if (!self) return;
            if (sr.is_err()) {
                self->set_busy(false);
                self->show_error_strip(
                    QObject::tr("Simulation failed: %1. Refusing to sign.")
                        .arg(QString::fromStdString(sr.error())));
                self->status_label_->setText(QObject::tr("Aborted."));
                return;
            }
            const auto sim = sr.value();
            if (!sim.ok) {
                LOG_WARN("SwapPanel",
                         "simulateTransaction reported err: " + sim.err);
                self->set_busy(false);
                self->show_error_strip(
                    QObject::tr("This swap would fail on-chain: %1. "
                                "Refusing to sign.").arg(sim.err));
                self->status_label_->setText(QObject::tr("Aborted."));
                return;
            }

            // ── Confirm dialog ──────────────────────────────────────────
            WalletActionSummary summary;
            summary.title = QStringLiteral("SWAP");
            summary.lede = QObject::tr(
                "Approve in your wallet to forward this transaction "
                "to the network. The terminal does not hold any funds.");
            summary.rows.append({QStringLiteral("ROUTE"),
                                 QStringLiteral("PumpSwap (pool=auto)"), true});
            if (buying_fncpt) {
                summary.rows.append({QStringLiteral("YOU PAY"),
                                     QStringLiteral("%1 SOL")
                                         .arg(format_token(ui_amount, 6)),
                                     true});
                summary.rows.append({QStringLiteral("YOU RECEIVE"),
                                     QObject::tr("≈ %1 $FNCPT (PumpSwap fills at execution)")
                                         .arg(format_token(est_out, 2)),
                                     true});
            } else {
                summary.rows.append({QStringLiteral("YOU PAY"),
                                     QStringLiteral("%1 $FNCPT")
                                         .arg(format_token(ui_amount, 2)),
                                     true});
                summary.rows.append({QStringLiteral("YOU RECEIVE"),
                                     QObject::tr("≈ %1 SOL (PumpSwap fills at execution)")
                                         .arg(format_token(est_out, 6)),
                                     true});
            }
            summary.rows.append({QStringLiteral("MAX SLIPPAGE"),
                                 format_bps(slip_bps_for_display), true});
            summary.rows.append({QStringLiteral("PRIORITY FEE"),
                                 QStringLiteral("%1 SOL")
                                     .arg(format_token(kDefaultPriorityFeeSol, 6)),
                                 true});
            summary.rows.append({QStringLiteral("RPC SIMULATION"),
                                 QStringLiteral("OK · %1 CU")
                                     .arg(QString::number(sim.units_consumed)),
                                 true});
            summary.warnings.append(QObject::tr(
                "PumpSwap will reject the trade if execution drifts more than "
                "the slippage tolerance above. Your funds stay in your wallet."));
            summary.primary_button_text = QStringLiteral("SWAP");
            summary.primary_is_safe = true;
            summary.arm_delay_ms = 1500;

            auto* dlg = new WalletActionConfirmDialog(summary, self);
            const QString tx_b64 = tx.tx_base64;
            QObject::connect(dlg, &WalletActionConfirmDialog::confirmed, self,
                             [self, tx_b64]() {
                if (!self) return;

                // ── Freshness gate (Phase 2 §2) ───────────────────────────
                // The tx may have been sitting in the dialog for several
                // seconds. Re-simulate with replace=false so an expired
                // blockhash is caught before the wallet signs. If the
                // simulation fails with BlockhashNotFound (or any other
                // freshness-related error), abort and ask the user to retry.
                self->status_label_->setText(QObject::tr("Re-checking freshness…"));
                auto* fresh_rpc = new fincept::wallet::SolanaRpcClient(self);
                fresh_rpc->reload_endpoint();
                fresh_rpc->simulate_transaction(tx_b64,
                    [self, tx_b64, fresh_rpc](
                        Result<fincept::wallet::SolanaRpcClient::SimulationResult> fr) {
                    fresh_rpc->deleteLater();
                    if (!self) return;
                    if (fr.is_err()) {
                        self->set_busy(false);
                        self->show_error_strip(
                            QObject::tr("Could not verify freshness: %1. "
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
                        self->show_error_strip(
                            QObject::tr("This swap is no longer fresh: %1. "
                                        "Click SWAP again to rebuild.")
                                .arg(fsim.err));
                        self->status_label_->setText(QObject::tr("Stale."));
                        return;
                    }

                    // ── Sign and send ─────────────────────────────────────
                    self->status_label_->setText(QObject::tr("Awaiting wallet signature…"));
                    fincept::wallet::WalletService::instance().sign_and_send(
                        tx_b64,
                        QObject::tr("Sign swap"),
                        QObject::tr("Approve the swap in your wallet to complete the trade."),
                        self,
                        [self](Result<QString> sr2) {
                            if (!self) return;
                            if (sr2.is_err()) {
                                self->set_busy(false);
                                self->show_error_strip(
                                    QObject::tr("Signing failed: %1")
                                        .arg(QString::fromStdString(sr2.error())));
                                self->status_label_->setText(QObject::tr("Cancelled."));
                                return;
                            }
                            const auto sig = sr2.value();
                            self->status_label_->setText(
                                QObject::tr("Sent. Waiting for confirmation…"));
                            LOG_INFO("SwapPanel", "submitted: " + sig);
                            self->start_status_poll(sig);
                        });
                }, /*replace_recent_blockhash=*/false);
            });
            QObject::connect(dlg, &WalletActionConfirmDialog::cancelled, self,
                             [self]() {
                if (!self) return;
                self->set_busy(false);
                self->status_label_->setText(self->tr("Cancelled."));
            });
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
        });
    });
}

} // namespace fincept::screens::panels
