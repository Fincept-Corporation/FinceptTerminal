#include "screens/crypto_center/panels/LockPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "screens/crypto_center/WalletActionConfirmDialog.h"
#include "screens/crypto_center/WalletActionSummary.h"
#include "services/billing/TierConfig.h"
#include "services/wallet/StakingService.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDialog>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::panels {

namespace {

QString font_stack() {
    return QStringLiteral("'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int dp = 0) {
    if (v <= 0.0)
        return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

QString format_usd(double v, int dp = 0) {
    if (v <= 0.0)
        return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', dp));
}

double atomic_to_ui(quint64 raw, int decimals) {
    return static_cast<double>(raw) / std::pow(10.0, std::max(0, decimals));
}

/// Format a token amount for injection into the amount QLineEdit.
/// Floors (never rounds up past the held balance) and omits group separators
/// so the value round-trips through `QLocale::toDouble()` unchanged. The old
/// `format_token(balance, 0)` rounded half-up, so MAX on 12 304.7 $FNCPT
/// produced "12305" and `recompute_preview()` then reported
/// "Amount exceeds available $FNCPT" — the MAX button could not be used.
QString lock_amount_for_input(double v, int decimals) {
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

} // namespace

LockPanel::LockPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("lockPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &LockPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this, &LockPanel::on_wallet_disconnected);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        on_wallet_disconnected();
    }
}

LockPanel::~LockPanel() = default;

void LockPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("lockPanelHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    head_title_ = new QLabel(tr("STAKE / LOCK"), head);
    head_title_->setObjectName(QStringLiteral("lockPanelTitle"));
    head_subtitle_ = new QLabel(tr("veFNCPT — locked $FNCPT earns USDC yield"), head);
    head_subtitle_->setObjectName(QStringLiteral("lockPanelHeadCaption"));
    hl->addWidget(head_title_);
    hl->addStretch();
    hl->addWidget(head_subtitle_);
    root->addWidget(head);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("lockPanelBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    // AMOUNT row
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* col_l = new QVBoxLayout;
        col_l->setSpacing(2);
        amount_caption_ = new QLabel(tr("AMOUNT"), body);
        amount_caption_->setObjectName(QStringLiteral("lockPanelCaption"));
        col_l->addWidget(amount_caption_);
        amount_input_ = new QLineEdit(body);
        amount_input_->setObjectName(QStringLiteral("lockPanelInput"));
        amount_input_->setFixedHeight(34);
        amount_input_->setPlaceholderText(QStringLiteral("0"));
        auto* validator = new QDoubleValidator(0.0, 1e12, 6, this);
        validator->setNotation(QDoubleValidator::StandardNotation);
        amount_input_->setValidator(validator);
        col_l->addWidget(amount_input_);
        row->addLayout(col_l, 1);

        auto* col_t = new QVBoxLayout;
        col_t->setSpacing(2);
        token_caption_ = new QLabel(tr("TOKEN"), body);
        token_caption_->setObjectName(QStringLiteral("lockPanelCaption"));
        col_t->addWidget(token_caption_);
        token_chip_ = new QLabel(tr("$FNCPT"), body);
        token_chip_->setObjectName(QStringLiteral("lockPanelTokenChip"));
        token_chip_->setFixedHeight(34);
        token_chip_->setAlignment(Qt::AlignCenter);
        token_chip_->setMinimumWidth(80);
        col_t->addWidget(token_chip_);
        row->addLayout(col_t);

        max_button_ = new QPushButton(tr("MAX"), body);
        max_button_->setObjectName(QStringLiteral("lockPanelButton"));
        max_button_->setFixedHeight(34);
        max_button_->setCursor(Qt::PointingHandCursor);
        row->addWidget(max_button_);
        bl->addLayout(row);

        available_label_ = new QLabel(tr("Available: —"), body);
        available_label_->setObjectName(QStringLiteral("lockPanelMeta"));
        bl->addWidget(available_label_);
    }

    // DURATION row
    {
        duration_caption_ = new QLabel(tr("DURATION"), body);
        duration_caption_->setObjectName(QStringLiteral("lockPanelCaption"));
        bl->addWidget(duration_caption_);

        auto* dur_row = new QHBoxLayout;
        dur_row->setSpacing(6);
        duration_group_ = new QButtonGroup(this);
        auto add_dur = [this, body, dur_row](Duration d, QRadioButton*& slot, const QString& label) {
            slot = new QRadioButton(label, body);
            slot->setObjectName(QStringLiteral("lockPanelDurationRadio"));
            slot->setCursor(Qt::PointingHandCursor);
            duration_group_->addButton(slot, static_cast<int>(d));
            dur_row->addWidget(slot);
        };
        add_dur(Duration::ThreeMonths, dur_3mo_, tr("3 MO"));
        add_dur(Duration::SixMonths, dur_6mo_, tr("6 MO"));
        add_dur(Duration::OneYear, dur_1yr_, tr("1 YR"));
        add_dur(Duration::TwoYears, dur_2yr_, tr("2 YR"));
        add_dur(Duration::FourYears, dur_4yr_, tr("4 YR"));
        dur_1yr_->setChecked(true);
        dur_row->addStretch(1);
        bl->addLayout(dur_row);
    }

    // Preview block
    {
        auto* preview = new QFrame(body);
        preview->setObjectName(QStringLiteral("lockPanelPreviewBlock"));
        auto* pl = new QVBoxLayout(preview);
        pl->setContentsMargins(10, 8, 10, 8);
        pl->setSpacing(6);

        auto add_kv = [preview, pl](const QString& k, QLabel*& cap_out, QLabel*& v) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            cap_out = new QLabel(k, preview);
            cap_out->setObjectName(QStringLiteral("lockPanelCaption"));
            v = new QLabel(QStringLiteral("—"), preview);
            v->setObjectName(QStringLiteral("lockPanelPreviewValue"));
            row->addWidget(cap_out);
            row->addStretch(1);
            row->addWidget(v);
            pl->addLayout(row);
        };
        add_kv(tr("WEIGHT"), weight_caption_, weight_calc_);
        add_kv(tr("EST. YIELD"), est_yield_caption_, est_yield_);
        add_kv(tr("TIER"), tier_caption_, tier_preview_);
        bl->addWidget(preview);
    }

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("lockPanelErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("lockPanelErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("lockPanelErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    // Submit row
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        status_label_ = new QLabel(tr("Choose an amount and duration."), body);
        status_label_->setObjectName(QStringLiteral("lockPanelMeta"));
        lock_button_ = new QPushButton(tr("LOCK"), body);
        lock_button_->setObjectName(QStringLiteral("lockPanelPrimaryButton"));
        lock_button_->setFixedHeight(34);
        lock_button_->setCursor(Qt::PointingHandCursor);
        lock_button_->setEnabled(false);
        row->addWidget(status_label_, 1);
        row->addWidget(lock_button_);
        bl->addLayout(row);
    }

    bl->addStretch(1);
    root->addWidget(body, 1);

    connect(amount_input_, &QLineEdit::textEdited, this, &LockPanel::on_amount_changed);
    connect(duration_group_, QOverload<int>::of(&QButtonGroup::idClicked), this, &LockPanel::on_duration_changed);
    connect(max_button_, &QPushButton::clicked, this, &LockPanel::on_max_clicked);
    connect(lock_button_, &QPushButton::clicked, this, &LockPanel::on_lock_clicked);

    // ── Accessibility ─────────────────────────────────────────────────────
    amount_input_->setAccessibleName(tr("Amount of FNCPT to lock"));
    amount_input_->setAccessibleDescription(tr("Locked tokens cannot be withdrawn before the unlock date"));
    max_button_->setAccessibleName(tr("Lock the maximum available balance"));
    lock_button_->setAccessibleName(tr("Build and review lock transaction"));
    available_label_->setAccessibleName(tr("Available FNCPT balance"));
    weight_calc_->setAccessibleName(tr("Resulting veFNCPT weight"));
    est_yield_->setAccessibleName(tr("Estimated yield upper bound"));
    tier_preview_->setAccessibleName(tr("Billing tier after lock"));
    status_label_->setAccessibleName(tr("Lock status"));
    if (dur_3mo_)
        dur_3mo_->setAccessibleName(tr("Lock for three months"));
    if (dur_6mo_)
        dur_6mo_->setAccessibleName(tr("Lock for six months"));
    if (dur_1yr_)
        dur_1yr_->setAccessibleName(tr("Lock for one year"));
    if (dur_2yr_)
        dur_2yr_->setAccessibleName(tr("Lock for two years"));
    if (dur_4yr_)
        dur_4yr_->setAccessibleName(tr("Lock for four years"));

    setTabOrder(amount_input_, max_button_);
    setTabOrder(max_button_, dur_3mo_);
    setTabOrder(dur_3mo_, dur_6mo_);
    setTabOrder(dur_6mo_, dur_1yr_);
    setTabOrder(dur_1yr_, dur_2yr_);
    setTabOrder(dur_2yr_, dur_4yr_);
    setTabOrder(dur_4yr_, lock_button_);
}

void LockPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss =
        QStringLiteral("QWidget#lockPanel { background:%1; }"
                       "QWidget#lockPanelHead { background:%2; border-bottom:1px solid %3; }"
                       "QLabel#lockPanelTitle { color:%4; font-family:%5; font-size:11px;"
                       "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                       "QLabel#lockPanelHeadCaption { color:%6; font-family:%5; font-size:10px;"
                       "  font-weight:600; letter-spacing:0.8px; background:transparent; }"
                       "QWidget#lockPanelBody { background:%1; }"
                       "QLabel#lockPanelCaption { color:%6; font-family:%5; font-size:9px;"
                       "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                       "QLabel#lockPanelMeta { color:%6; font-family:%5; font-size:10px;"
                       "  background:transparent; }"
                       "QLineEdit#lockPanelInput { background:%2; color:%7; border:1px solid %3;"
                       "  font-family:%5; font-size:16px; padding:0 8px; }"
                       "QLineEdit#lockPanelInput:focus { border-color:%4; }"
                       "QLabel#lockPanelTokenChip { background:%8; color:%4; border:1px solid %3;"
                       "  font-family:%5; font-size:13px; font-weight:700; letter-spacing:1.2px; }"

                       "QRadioButton#lockPanelDurationRadio { color:%7; font-family:%5;"
                       "  font-size:11px; font-weight:700; letter-spacing:1px;"
                       "  background:transparent; padding:6px 10px; border:1px solid %3; }"
                       "QRadioButton#lockPanelDurationRadio:checked { color:%4; border-color:%12;"
                       "  background:rgba(217,119,6,0.10); }"
                       "QRadioButton#lockPanelDurationRadio::indicator { width:0; height:0; }"

                       "QFrame#lockPanelPreviewBlock { background:%8; border:1px solid %3; }"
                       "QLabel#lockPanelPreviewValue { color:%7; font-family:%5; font-size:11px;"
                       "  background:transparent; }"

                       "QFrame#lockPanelErrorStrip { background:rgba(220,38,38,0.10);"
                       "  border:1px solid %9; }"
                       "QLabel#lockPanelErrorIcon { color:%9; font-family:%5; font-size:13px;"
                       "  font-weight:700; background:transparent; }"
                       "QLabel#lockPanelErrorText { color:%9; font-family:%5; font-size:11px;"
                       "  background:transparent; }"

                       "QPushButton#lockPanelButton { background:%8; color:%6; border:1px solid %3;"
                       "  font-family:%5; font-size:11px; font-weight:700; letter-spacing:1px;"
                       "  padding:0 14px; }"
                       "QPushButton#lockPanelButton:hover { background:%10; color:%7; border-color:%11; }"
                       "QPushButton#lockPanelPrimaryButton { background:rgba(217,119,6,0.10); color:%4;"
                       "  border:1px solid %12; font-family:%5; font-size:12px; font-weight:700;"
                       "  letter-spacing:1.5px; padding:0 18px; }"
                       "QPushButton#lockPanelPrimaryButton:hover { background:%4; color:%1; }"
                       "QPushButton#lockPanelPrimaryButton:disabled { background:%8; color:%6;"
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

// ── Lifecycle ──────────────────────────────────────────────────────────────

void LockPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible())
        resubscribe();
    set_busy(false);
    recompute_preview();
}

void LockPanel::on_wallet_disconnected() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_balance_topic_.clear();
    current_vefncpt_topic_.clear();
    current_tier_topic_.clear();
    current_pubkey_.clear();
    fncpt_balance_ui_ = 0.0;
    current_user_weight_raw_ = 0;
    current_tier_ = fincept::wallet::TierStatus::Tier::Free;
    available_label_->setText(tr("Available: —"));
    status_label_->setText(tr("Connect a wallet to lock $FNCPT."));
    lock_button_->setEnabled(false);
    clear_error_strip();
}

void LockPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    resubscribe();
}

void LockPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_balance_topic_.clear();
    current_vefncpt_topic_.clear();
    current_tier_topic_.clear();
}

void LockPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void LockPanel::retranslateUi() {
    if (head_title_)
        head_title_->setText(tr("STAKE / LOCK"));
    if (head_subtitle_)
        head_subtitle_->setText(tr("veFNCPT — locked $FNCPT earns USDC yield"));
    if (amount_caption_)
        amount_caption_->setText(tr("AMOUNT"));
    if (token_caption_)
        token_caption_->setText(tr("TOKEN"));
    if (token_chip_)
        token_chip_->setText(tr("$FNCPT"));
    if (duration_caption_)
        duration_caption_->setText(tr("DURATION"));
    if (weight_caption_)
        weight_caption_->setText(tr("WEIGHT"));
    if (est_yield_caption_)
        est_yield_caption_->setText(tr("EST. YIELD"));
    if (tier_caption_)
        tier_caption_->setText(tr("TIER"));
    if (dur_3mo_)
        dur_3mo_->setText(tr("3 MO"));
    if (dur_6mo_)
        dur_6mo_->setText(tr("6 MO"));
    if (dur_1yr_)
        dur_1yr_->setText(tr("1 YR"));
    if (dur_2yr_)
        dur_2yr_->setText(tr("2 YR"));
    if (dur_4yr_)
        dur_4yr_->setText(tr("4 YR"));
    if (max_button_)
        max_button_->setText(tr("MAX"));
    if (lock_button_)
        lock_button_->setText(tr("LOCK"));

    // AVAILABLE line — re-apply with the cached balance (or the placeholder).
    if (available_label_) {
        if (current_pubkey_.isEmpty()) {
            available_label_->setText(tr("Available: —"));
        } else {
            available_label_->setText(tr("Available: %1 $FNCPT").arg(format_token(fncpt_balance_ui_, 0)));
        }
    }

    // Re-render preview (weight / est. yield / tier) + status line in the new
    // locale from cached hub state.
    recompute_preview();
}

void LockPanel::resubscribe() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    current_vefncpt_topic_.clear();
    current_tier_topic_.clear();

    if (!current_pubkey_.isEmpty()) {
        current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
        hub.subscribe(this, current_balance_topic_, [this](const QVariant& v) { on_balance_update(v); });
        hub.request(current_balance_topic_, /*force=*/false);

        current_vefncpt_topic_ = QStringLiteral("wallet:vefncpt:%1").arg(current_pubkey_);
        hub.subscribe(this, current_vefncpt_topic_, [this](const QVariant& v) { on_vefncpt_update(v); });
        hub.request(current_vefncpt_topic_, /*force=*/false);

        current_tier_topic_ = QStringLiteral("billing:tier:%1").arg(current_pubkey_);
        hub.subscribe(this, current_tier_topic_, [this](const QVariant& v) { on_tier_update(v); });
        hub.request(current_tier_topic_, /*force=*/false);
    }

    // Terminal-wide topics — same for all users.
    hub.subscribe(this, QStringLiteral("treasury:revenue"), [this](const QVariant& v) { on_revenue_update(v); });
    hub.request(QStringLiteral("treasury:revenue"), /*force=*/false);

    hub.subscribe(this, QStringLiteral("market:price:fncpt"), [this](const QVariant& v) { on_price_update(v); });
    hub.request(QStringLiteral("market:price:fncpt"), /*force=*/false);
}

// ── Hub callbacks ──────────────────────────────────────────────────────────

void LockPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>())
        return;
    const auto bal = v.value<fincept::wallet::WalletBalance>();
    fncpt_balance_ui_ = bal.fncpt_ui();
    fncpt_decimals_ = bal.fncpt_decimals();
    available_label_->setText(tr("Available: %1 $FNCPT").arg(format_token(fncpt_balance_ui_, 0)));
    recompute_preview();
}

void LockPanel::on_vefncpt_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::VeFncptAggregate>())
        return;
    const auto agg = v.value<fincept::wallet::VeFncptAggregate>();
    bool ok = false;
    current_user_weight_raw_ = agg.total_weight_raw.toULongLong(&ok);
    if (!ok)
        current_user_weight_raw_ = 0;
    recompute_preview();
}

void LockPanel::on_revenue_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryRevenue>())
        return;
    const auto rev = v.value<fincept::wallet::TreasuryRevenue>();
    weekly_revenue_usd_ = rev.total_usd;
    revenue_is_mock_ = rev.is_mock;
    recompute_preview();
}

void LockPanel::on_price_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>())
        return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (p.valid)
        fncpt_usd_price_ = p.usd;
    recompute_preview();
}

void LockPanel::on_tier_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TierStatus>())
        return;
    current_tier_ = v.value<fincept::wallet::TierStatus>().tier;
    recompute_preview();
}

void LockPanel::on_amount_changed(const QString& /*s*/) {
    clear_error_strip();
    recompute_preview();
}

void LockPanel::on_duration_changed(int /*id*/) {
    recompute_preview();
}

void LockPanel::on_max_clicked() {
    if (fncpt_balance_ui_ <= 0.0)
        return;
    QSignalBlocker b(amount_input_);
    amount_input_->setText(lock_amount_for_input(fncpt_balance_ui_, fncpt_decimals_));
    recompute_preview();
}

// ── Preview ────────────────────────────────────────────────────────────────

LockPanel::Duration LockPanel::current_duration() const {
    const int id = duration_group_->checkedId();
    return static_cast<Duration>(id);
}

void LockPanel::recompute_preview() {
    bool ok = false;
    const double amount_ui = QLocale::system().toDouble(amount_input_->text(), &ok);
    const Duration d = current_duration();
    const double mult = fincept::wallet::StakingService::multiplier_for(d);
    const QString dur_label = fincept::wallet::StakingService::label_for(d);

    if (!ok || amount_ui <= 0.0) {
        weight_calc_->setText(QStringLiteral("—"));
        est_yield_->setText(QStringLiteral("—"));
        tier_preview_->setText(QStringLiteral("—"));
        lock_button_->setEnabled(false);
        return;
    }

    // WEIGHT line: "1,000 veFNCPT × 0.25 (1 yr) = 250 veFNCPT"
    const double new_weight_ui = amount_ui * mult;
    weight_calc_->setText(QStringLiteral("%1 × %2 (%3) = %4 veFNCPT")
                              .arg(format_token(amount_ui, 0))
                              .arg(QString::number(mult, 'f', 4))
                              .arg(dur_label)
                              .arg(format_token(new_weight_ui, 1)));

    // EST. YIELD: project against the next epoch's distribution.
    //   distribution_usdc = weekly_revenue_usd × 25 % (staker share, plan §3.4)
    //   user_share = (current_weight + new_weight) / (current_weight + new_weight + global_other_weight)
    // We don't have the global weight published yet (would need a separate
    // topic from the program), so we model the conservative case
    // "user is the only locker" — projection ≤ this in any real scenario.
    constexpr double kStakerShare = 0.25;
    const double total_user_weight_after = atomic_to_ui(current_user_weight_raw_, fncpt_decimals_) + new_weight_ui;
    const double distribution = weekly_revenue_usd_ * kStakerShare;
    const double est_weekly = (total_user_weight_after > 0.0) ? distribution : 0.0;
    // Real-yield % expressed against the user's USD stake value at lock time.
    const double stake_usd = amount_ui * fncpt_usd_price_;
    const double pct_weekly = (stake_usd > 0.0) ? (100.0 * est_weekly / stake_usd) : 0.0;
    if (weekly_revenue_usd_ > 0.0 && fncpt_usd_price_ > 0.0) {
        // The projection assumes the user is the ONLY locker (no global weight
        // topic exists yet), so it is an upper bound, not an expectation. Say
        // so on the line itself — an unqualified "$42 / week — 2.18% weekly
        // real yield" reads as a forecast and is the kind of number people
        // make irreversible 4-year lock decisions on. Prefix DEMO when the
        // revenue feed is the producer's built-in mock.
        QString yield_text = tr("≤ %1 / week (USDC) — ≤ %2% weekly at %3 stake "
                                "· upper bound, assumes no other lockers")
                                 .arg(format_usd(est_weekly, 0))
                                 .arg(QString::number(pct_weekly, 'f', 2))
                                 .arg(format_usd(stake_usd, 0));
        if (revenue_is_mock_)
            yield_text = tr("DEMO · ") + yield_text;
        est_yield_->setText(yield_text);
    } else {
        est_yield_->setText(tr("waiting for revenue + spot price…"));
    }

    // TIER preview: current → tier-after-lock.
    const quint64 new_weight_raw = static_cast<quint64>(new_weight_ui * std::pow(10.0, fncpt_decimals_));
    const auto tier_after = fincept::billing::TierConfig::tier_from_weight(current_user_weight_raw_ + new_weight_raw);
    if (tier_after == current_tier_) {
        tier_preview_->setText(fincept::billing::TierConfig::label_for(current_tier_));
    } else {
        tier_preview_->setText(QStringLiteral("%1 → %2  (after lock)")
                                   .arg(fincept::billing::TierConfig::label_for(current_tier_))
                                   .arg(fincept::billing::TierConfig::label_for(tier_after)));
    }

    // Submit gating — the LOCK button must stay disabled until the on-chain
    // program is actually deployed, so a demo-looking screen can never build a
    // real staking transaction.
    const bool program_ready = fincept::wallet::StakingService::instance().program_is_configured();
    bool can_submit =
        program_ready && !busy_ && !current_pubkey_.isEmpty() && amount_ui > 0.0 && amount_ui <= fncpt_balance_ui_;
    lock_button_->setEnabled(can_submit);
    if (!program_ready) {
        status_label_->setText(tr("DEMO — fincept_lock not deployed; configure SecureStorage "
                                  "fincept.lock_program_id to enable real locks."));
    } else if (can_submit) {
        status_label_->setText(tr("Ready. Click LOCK to build the transaction."));
    } else if (amount_ui > fncpt_balance_ui_) {
        status_label_->setText(tr("Amount exceeds available $FNCPT."));
    } else {
        status_label_->setText(tr("Choose an amount and duration."));
    }
}

void LockPanel::show_error_strip(const QString& msg) {
    if (!error_strip_)
        return;
    error_text_->setText(msg);
    error_strip_->show();
}

void LockPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void LockPanel::set_busy(bool busy) {
    busy_ = busy;
    amount_input_->setEnabled(!busy);
    if (duration_group_) {
        for (auto* btn : duration_group_->buttons())
            btn->setEnabled(!busy);
    }
    max_button_->setEnabled(!busy);
    if (busy) {
        lock_button_->setEnabled(false);
        return;
    }
    // Un-busying must NOT blanket-enable LOCK: that re-armed the button in
    // mock mode (fincept_lock not deployed), with an empty amount, or with an
    // amount above the balance. Re-derive the gate instead. recompute_preview
    // reads busy_ (already false) and applies program_is_configured() +
    // amount/balance checks.
    if (current_pubkey_.isEmpty()) {
        lock_button_->setEnabled(false);
        return;
    }
    recompute_preview();
}

// ── Submit flow ────────────────────────────────────────────────────────────

void LockPanel::on_lock_clicked() {
    bool ok = false;
    const double amount_ui = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || amount_ui <= 0.0)
        return;
    const Duration d = current_duration();

    // Build the atomic amount string for the program.
    //
    // `static_cast<quint64>` TRUNCATES, and `amount_ui * 10^decimals` is not
    // exact in binary: 8.7 × 1e6 evaluates to 8699999.999999999, which
    // truncated escrows 8.699999 $FNCPT instead of 8.7. Round to the nearest
    // atomic unit instead. Rounding can never exceed the balance because
    // `amount_ui <= fncpt_balance_ui_` was checked above and the balance is
    // itself an exact multiple of one atomic unit.
    const auto power = std::pow(10.0, fncpt_decimals_);
    const double atomic = amount_ui * power;
    const QString amount_raw =
        QString::number(atomic > 0.0 ? static_cast<quint64>(std::llround(atomic)) : static_cast<quint64>(0));

    set_busy(true);
    clear_error_strip();
    status_label_->setText(tr("Building lock transaction…"));

    QPointer<LockPanel> self = this;
    fincept::wallet::StakingService::instance().build_lock_tx(
        current_pubkey_, amount_raw, d, [self, amount_ui, d](Result<QString> r) {
            if (!self)
                return;
            if (r.is_err()) {
                self->set_busy(false);
                self->show_error_strip(QString::fromStdString(r.error()));
                self->status_label_->setText(self->tr("Aborted."));
                return;
            }
            const QString tx_b64 = r.value();

            // Build the user-facing decoded summary. The wallet shows its own
            // decoded preview independently; this is the pre-sign sanity check.
            WalletActionSummary summary;
            summary.title = self->tr("LOCK $FNCPT");
            summary.lede = self->tr("Approve in your wallet to escrow $FNCPT under the fincept_lock "
                                    "program. The terminal does not hold your funds — the on-chain "
                                    "program does, and only releases them after the unlock date.");
            summary.rows.append(
                {self->tr("AMOUNT"), QStringLiteral("%1 $FNCPT").arg(format_token(amount_ui, 0)), true});
            summary.rows.append({self->tr("DURATION"), fincept::wallet::StakingService::label_for(d), true});
            // The exact unlock instant, not just "4 YR". This is the single
            // most consequential fact about an irreversible lock and it was
            // not shown anywhere before signing.
            const qint64 unlock_ms =
                QDateTime::currentMSecsSinceEpoch() + fincept::wallet::StakingService::seconds_for(d) * 1000LL;
            summary.rows.append({self->tr("UNLOCKS ON"),
                                 QDateTime::fromMSecsSinceEpoch(unlock_ms).toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                                 true});
            summary.rows.append({self->tr("WALLET"), self->current_pubkey_, true});
            const double mult = fincept::wallet::StakingService::multiplier_for(d);
            summary.rows.append(
                {self->tr("WEIGHT"), QStringLiteral("%1 veFNCPT").arg(format_token(amount_ui * mult, 1)), true});
            summary.warnings.append(self->tr("Locked $FNCPT cannot be withdrawn before the unlock date. "
                                             "If you need liquidity sooner, do not lock."));
            summary.primary_button_text = self->tr("LOCK");
            summary.primary_is_safe = false; // irreversible action — extra emphasis
            summary.arm_delay_ms = 2500;

            auto* dlg = new WalletActionConfirmDialog(summary, self);
            QObject::connect(dlg, &WalletActionConfirmDialog::confirmed, self, [self, tx_b64]() {
                if (!self)
                    return;
                self->status_label_->setText(self->tr("Awaiting wallet signature…"));
                fincept::wallet::WalletService::instance().sign_and_send(
                    tx_b64, QObject::tr("Sign lock"), QObject::tr("Approve the lock in your wallet."), self,
                    [self](Result<QString> sr) {
                        if (!self)
                            return;
                        if (sr.is_err()) {
                            self->set_busy(false);
                            self->show_error_strip(
                                QObject::tr("Signing failed: %1").arg(QString::fromStdString(sr.error())));
                            self->status_label_->setText(QObject::tr("Cancelled."));
                            return;
                        }
                        const auto sig = sr.value();
                        LOG_INFO("LockPanel", "submitted: " + sig);
                        // set_busy(false) re-derives the status line via
                        // recompute_preview(), so the submitted-signature
                        // message has to be written AFTER it or it is
                        // immediately overwritten by "Choose an amount…".
                        self->set_busy(false);
                        self->amount_input_->clear();
                        self->status_label_->setText(
                            QObject::tr("Sent: %1… (not yet confirmed on-chain)").arg(sig.left(12)));
                        fincept::wallet::WalletService::instance().force_balance_refresh();
                    });
            });
            // `cancelled()` only fires from the CANCEL button. Esc / the window
            // close box left the panel permanently busy. `rejected()` covers
            // every dismissal path; the handler is idempotent.
            QObject::connect(dlg, &QDialog::rejected, self, [self]() {
                if (!self)
                    return;
                self->set_busy(false);
                self->status_label_->setText(self->tr("Cancelled."));
            });
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
        });
}

} // namespace fincept::screens::panels
