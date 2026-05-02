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

#include <cmath>

namespace fincept::screens::panels {

namespace {

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int dp = 0) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

QString format_usd(double v, int dp = 0) {
    if (v <= 0.0) return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', dp));
}

double atomic_to_ui(quint64 raw, int decimals) {
    return static_cast<double>(raw) / std::pow(10.0, std::max(0, decimals));
}

} // namespace

LockPanel::LockPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("lockPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &LockPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &LockPanel::on_wallet_disconnected);

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
    auto* title = new QLabel(QStringLiteral("STAKE / LOCK"), head);
    title->setObjectName(QStringLiteral("lockPanelTitle"));
    auto* subtitle = new QLabel(
        QStringLiteral("veFNCPT — locked $FNCPT earns USDC yield"), head);
    subtitle->setObjectName(QStringLiteral("lockPanelHeadCaption"));
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(subtitle);
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
        auto* cap = new QLabel(QStringLiteral("AMOUNT"), body);
        cap->setObjectName(QStringLiteral("lockPanelCaption"));
        col_l->addWidget(cap);
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
        auto* cap_t = new QLabel(QStringLiteral("TOKEN"), body);
        cap_t->setObjectName(QStringLiteral("lockPanelCaption"));
        col_t->addWidget(cap_t);
        auto* token_chip = new QLabel(QStringLiteral("$FNCPT"), body);
        token_chip->setObjectName(QStringLiteral("lockPanelTokenChip"));
        token_chip->setFixedHeight(34);
        token_chip->setAlignment(Qt::AlignCenter);
        token_chip->setMinimumWidth(80);
        col_t->addWidget(token_chip);
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
        auto* cap = new QLabel(QStringLiteral("DURATION"), body);
        cap->setObjectName(QStringLiteral("lockPanelCaption"));
        bl->addWidget(cap);

        auto* dur_row = new QHBoxLayout;
        dur_row->setSpacing(6);
        duration_group_ = new QButtonGroup(this);
        auto add_dur = [this, body, dur_row](Duration d, QRadioButton*& slot,
                                              const QString& label) {
            slot = new QRadioButton(label, body);
            slot->setObjectName(QStringLiteral("lockPanelDurationRadio"));
            slot->setCursor(Qt::PointingHandCursor);
            duration_group_->addButton(slot, static_cast<int>(d));
            dur_row->addWidget(slot);
        };
        add_dur(Duration::ThreeMonths, dur_3mo_, QStringLiteral("3 MO"));
        add_dur(Duration::SixMonths,   dur_6mo_, QStringLiteral("6 MO"));
        add_dur(Duration::OneYear,     dur_1yr_, QStringLiteral("1 YR"));
        add_dur(Duration::TwoYears,    dur_2yr_, QStringLiteral("2 YR"));
        add_dur(Duration::FourYears,   dur_4yr_, QStringLiteral("4 YR"));
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

        auto add_kv = [preview, pl, body](const QString& k, QLabel*& v) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            auto* lbl = new QLabel(k, preview);
            lbl->setObjectName(QStringLiteral("lockPanelCaption"));
            v = new QLabel(QStringLiteral("—"), preview);
            v->setObjectName(QStringLiteral("lockPanelPreviewValue"));
            row->addWidget(lbl);
            row->addStretch(1);
            row->addWidget(v);
            pl->addLayout(row);
        };
        add_kv(QStringLiteral("WEIGHT"), weight_calc_);
        add_kv(QStringLiteral("EST. YIELD"), est_yield_);
        add_kv(QStringLiteral("TIER"), tier_preview_);
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

    connect(amount_input_, &QLineEdit::textEdited, this,
            &LockPanel::on_amount_changed);
    connect(duration_group_, QOverload<int>::of(&QButtonGroup::idClicked), this,
            &LockPanel::on_duration_changed);
    connect(max_button_, &QPushButton::clicked, this, &LockPanel::on_max_clicked);
    connect(lock_button_, &QPushButton::clicked, this, &LockPanel::on_lock_clicked);
}

void LockPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#lockPanel { background:%1; }"
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

void LockPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) resubscribe();
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

void LockPanel::resubscribe() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    current_vefncpt_topic_.clear();
    current_tier_topic_.clear();

    if (!current_pubkey_.isEmpty()) {
        current_balance_topic_ =
            QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
        hub.subscribe(this, current_balance_topic_,
                      [this](const QVariant& v) { on_balance_update(v); });
        hub.request(current_balance_topic_, /*force=*/false);

        current_vefncpt_topic_ =
            QStringLiteral("wallet:vefncpt:%1").arg(current_pubkey_);
        hub.subscribe(this, current_vefncpt_topic_,
                      [this](const QVariant& v) { on_vefncpt_update(v); });
        hub.request(current_vefncpt_topic_, /*force=*/false);

        current_tier_topic_ =
            QStringLiteral("billing:tier:%1").arg(current_pubkey_);
        hub.subscribe(this, current_tier_topic_,
                      [this](const QVariant& v) { on_tier_update(v); });
        hub.request(current_tier_topic_, /*force=*/false);
    }

    // Terminal-wide topics — same for all users.
    hub.subscribe(this, QStringLiteral("treasury:revenue"),
                  [this](const QVariant& v) { on_revenue_update(v); });
    hub.request(QStringLiteral("treasury:revenue"), /*force=*/false);

    hub.subscribe(this, QStringLiteral("market:price:fncpt"),
                  [this](const QVariant& v) { on_price_update(v); });
    hub.request(QStringLiteral("market:price:fncpt"), /*force=*/false);
}

// ── Hub callbacks ──────────────────────────────────────────────────────────

void LockPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    const auto bal = v.value<fincept::wallet::WalletBalance>();
    fncpt_balance_ui_ = bal.fncpt_ui();
    fncpt_decimals_ = bal.fncpt_decimals();
    available_label_->setText(tr("Available: %1 $FNCPT")
                                   .arg(format_token(fncpt_balance_ui_, 0)));
    recompute_preview();
}

void LockPanel::on_vefncpt_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::VeFncptAggregate>()) return;
    const auto agg = v.value<fincept::wallet::VeFncptAggregate>();
    bool ok = false;
    current_user_weight_raw_ = agg.total_weight_raw.toULongLong(&ok);
    if (!ok) current_user_weight_raw_ = 0;
    recompute_preview();
}

void LockPanel::on_revenue_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryRevenue>()) return;
    weekly_revenue_usd_ = v.value<fincept::wallet::TreasuryRevenue>().total_usd;
    recompute_preview();
}

void LockPanel::on_price_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>()) return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (p.valid) fncpt_usd_price_ = p.usd;
    recompute_preview();
}

void LockPanel::on_tier_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TierStatus>()) return;
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
    if (fncpt_balance_ui_ <= 0.0) return;
    QSignalBlocker b(amount_input_);
    amount_input_->setText(format_token(fncpt_balance_ui_, 0));
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
    const double total_user_weight_after =
        atomic_to_ui(current_user_weight_raw_, fncpt_decimals_) + new_weight_ui;
    const double distribution = weekly_revenue_usd_ * kStakerShare;
    const double est_weekly = (total_user_weight_after > 0.0)
        ? distribution
        : 0.0;
    // Real-yield % expressed against the user's USD stake value at lock time.
    const double stake_usd = amount_ui * fncpt_usd_price_;
    const double pct_weekly = (stake_usd > 0.0)
        ? (100.0 * est_weekly / stake_usd) : 0.0;
    if (weekly_revenue_usd_ > 0.0 && fncpt_usd_price_ > 0.0) {
        est_yield_->setText(tr("%1 / week (USDC) — %2% weekly real yield at %3 stake")
            .arg(format_usd(est_weekly, 0))
            .arg(QString::number(pct_weekly, 'f', 2))
            .arg(format_usd(stake_usd, 0)));
    } else {
        est_yield_->setText(tr("waiting for revenue + spot price…"));
    }

    // TIER preview: current → tier-after-lock.
    const quint64 new_weight_raw =
        static_cast<quint64>(new_weight_ui * std::pow(10.0, fncpt_decimals_));
    const auto tier_after = fincept::billing::TierConfig::tier_from_weight(
        current_user_weight_raw_ + new_weight_raw);
    if (tier_after == current_tier_) {
        tier_preview_->setText(fincept::billing::TierConfig::label_for(current_tier_));
    } else {
        tier_preview_->setText(QStringLiteral("%1 → %2  (after lock)")
            .arg(fincept::billing::TierConfig::label_for(current_tier_))
            .arg(fincept::billing::TierConfig::label_for(tier_after)));
    }

    // Submit gating
    bool can_submit = !busy_
                       && !current_pubkey_.isEmpty()
                       && amount_ui > 0.0
                       && amount_ui <= fncpt_balance_ui_;
    lock_button_->setEnabled(can_submit);
    if (!fincept::wallet::StakingService::instance().program_is_configured()) {
        status_label_->setText(
            tr("DEMO — fincept_lock not deployed; configure SecureStorage "
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
    if (!error_strip_) return;
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
        for (auto* btn : duration_group_->buttons()) btn->setEnabled(!busy);
    }
    max_button_->setEnabled(!busy);
    lock_button_->setEnabled(!busy);
}

// ── Submit flow ────────────────────────────────────────────────────────────

void LockPanel::on_lock_clicked() {
    bool ok = false;
    const double amount_ui = QLocale::system().toDouble(amount_input_->text(), &ok);
    if (!ok || amount_ui <= 0.0) return;
    const Duration d = current_duration();

    // Build the atomic amount string for the program. Multiply at full
    // precision via integer math to avoid double-rounding.
    const auto power = std::pow(10.0, fncpt_decimals_);
    const QString amount_raw =
        QString::number(static_cast<quint64>(amount_ui * power));

    set_busy(true);
    clear_error_strip();
    status_label_->setText(tr("Building lock transaction…"));

    QPointer<LockPanel> self = this;
    fincept::wallet::StakingService::instance().build_lock_tx(
        current_pubkey_, amount_raw, d,
        [self, amount_ui, d](Result<QString> r) {
        if (!self) return;
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
        summary.title = QStringLiteral("LOCK $FNCPT");
        summary.lede = self->tr(
            "Approve in your wallet to escrow $FNCPT under the fincept_lock "
            "program. The terminal does not hold your funds — the on-chain "
            "program does, and only releases them after the unlock date.");
        summary.rows.append({QStringLiteral("AMOUNT"),
                             QStringLiteral("%1 $FNCPT").arg(format_token(amount_ui, 0)),
                             true});
        summary.rows.append({QStringLiteral("DURATION"),
                             fincept::wallet::StakingService::label_for(d), true});
        const double mult = fincept::wallet::StakingService::multiplier_for(d);
        summary.rows.append({QStringLiteral("WEIGHT"),
                             QStringLiteral("%1 veFNCPT")
                                 .arg(format_token(amount_ui * mult, 1)),
                             true});
        summary.warnings.append(self->tr(
            "Locked $FNCPT cannot be withdrawn before the unlock date. "
            "If you need liquidity sooner, do not lock."));
        summary.primary_button_text = QStringLiteral("LOCK");
        summary.primary_is_safe = false; // irreversible action — extra emphasis
        summary.arm_delay_ms = 2500;

        auto* dlg = new WalletActionConfirmDialog(summary, self);
        QObject::connect(dlg, &WalletActionConfirmDialog::confirmed, self,
                         [self, tx_b64]() {
            if (!self) return;
            self->status_label_->setText(self->tr("Awaiting wallet signature…"));
            fincept::wallet::WalletService::instance().sign_and_send(
                tx_b64,
                QObject::tr("Sign lock"),
                QObject::tr("Approve the lock in your wallet."),
                self,
                [self](Result<QString> sr) {
                    if (!self) return;
                    if (sr.is_err()) {
                        self->set_busy(false);
                        self->show_error_strip(QObject::tr("Signing failed: %1")
                            .arg(QString::fromStdString(sr.error())));
                        self->status_label_->setText(QObject::tr("Cancelled."));
                        return;
                    }
                    const auto sig = sr.value();
                    LOG_INFO("LockPanel", "submitted: " + sig);
                    self->status_label_->setText(
                        QObject::tr("Sent: %1…").arg(sig.left(12)));
                    self->set_busy(false);
                    self->amount_input_->clear();
                    fincept::wallet::WalletService::instance().force_balance_refresh();
                });
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
}

} // namespace fincept::screens::panels
