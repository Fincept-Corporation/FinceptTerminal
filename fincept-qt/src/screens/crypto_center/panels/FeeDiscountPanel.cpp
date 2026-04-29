#include "screens/crypto_center/panels/FeeDiscountPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/billing/FeeDiscountConfig.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QShowEvent>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::panels {

namespace {

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int dp = 2) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

QString format_usd(double v) {
    if (v < 0.0) return QStringLiteral("—");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 2));
}

double threshold_ui(const fincept::wallet::FncptDiscount& d) {
    if (d.threshold_decimals <= 0) return 0.0;
    return static_cast<double>(d.threshold_raw) / std::pow(10.0, d.threshold_decimals);
}

} // namespace

FeeDiscountPanel::FeeDiscountPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("feeDiscountPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &FeeDiscountPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &FeeDiscountPanel::on_wallet_disconnected);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    }

    update_view();
}

FeeDiscountPanel::~FeeDiscountPanel() = default;

void FeeDiscountPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("feeDiscountHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("FEE DISCOUNT"), head);
    title->setObjectName(QStringLiteral("feeDiscountTitle"));
    heading_status_ = new QLabel(QStringLiteral("—"), head);
    heading_status_->setObjectName(QStringLiteral("feeDiscountHeadStatus"));
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(heading_status_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("feeDiscountBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    // Threshold + balance row
    auto* row = new QWidget(body);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(18);

    auto add_kv = [body, rl](const QString& cap, QLabel*& v_out,
                             const QString& obj_name) {
        auto* col = new QVBoxLayout;
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);
        auto* k = new QLabel(cap, body);
        k->setObjectName(QStringLiteral("feeDiscountCaption"));
        v_out = new QLabel(QStringLiteral("—"), body);
        v_out->setObjectName(obj_name);
        col->addWidget(k);
        col->addWidget(v_out);
        rl->addLayout(col);
    };
    add_kv(QStringLiteral("HOLDING"), balance_value_,
           QStringLiteral("feeDiscountValue"));
    add_kv(QStringLiteral("THRESHOLD"), threshold_value_,
           QStringLiteral("feeDiscountValueDim"));
    rl->addStretch(1);
    bl->addWidget(row);

    progress_ = new QProgressBar(body);
    progress_->setObjectName(QStringLiteral("feeDiscountProgress"));
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(8);
    bl->addWidget(progress_);

    auto* skus_caption = new QLabel(QStringLiteral("APPLIED TO"), body);
    skus_caption->setObjectName(QStringLiteral("feeDiscountCaption"));
    bl->addWidget(skus_caption);
    skus_value_ = new QLabel(QStringLiteral("—"), body);
    skus_value_->setObjectName(QStringLiteral("feeDiscountSkus"));
    skus_value_->setWordWrap(true);
    bl->addWidget(skus_value_);

    auto* save_caption = new QLabel(
        tr("PROJECTED SAVINGS  ·  reference $%1 SKU")
            .arg(QLocale::system().toString(
                fincept::billing::FeeDiscountConfig::kReferencePriceUsd, 'f', 2)),
        body);
    save_caption->setObjectName(QStringLiteral("feeDiscountCaption"));
    bl->addWidget(save_caption);
    savings_value_ = new QLabel(QStringLiteral("—"), body);
    savings_value_->setObjectName(QStringLiteral("feeDiscountSavings"));
    bl->addWidget(savings_value_);

    hint_ = new QLabel(
        tr("Hold ≥ %1 $FNCPT to qualify for the discount on premium screens, "
           "AI reports, and deep backtests.")
            .arg(QLocale::system().toString(
                static_cast<double>(
                    fincept::billing::FeeDiscountConfig::kThresholdRaw)
                    / std::pow(10.0, fincept::billing::FeeDiscountConfig::kThresholdDecimals),
                'f', 0)),
        body);
    hint_->setObjectName(QStringLiteral("feeDiscountHint"));
    hint_->setWordWrap(true);
    bl->addWidget(hint_);

    bl->addStretch(1);
    root->addWidget(body, 1);
}

void FeeDiscountPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#feeDiscountPanel { background:%1; }"
        "QWidget#feeDiscountHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#feeDiscountTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#feeDiscountHeadStatus { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#feeDiscountHeadStatusOk { color:%7; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QWidget#feeDiscountBody { background:%1; }"

        "QLabel#feeDiscountCaption { color:%6; font-family:%5; font-size:9px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#feeDiscountValue { color:%8; font-family:%5; font-size:14px;"
        "  font-weight:600; background:transparent; }"
        "QLabel#feeDiscountValueDim { color:%9; font-family:%5; font-size:14px;"
        "  background:transparent; }"
        "QLabel#feeDiscountSkus { color:%9; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#feeDiscountSavings { color:%4; font-family:%5; font-size:14px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#feeDiscountHint { color:%6; font-family:%5; font-size:11px;"
        "  background:transparent; }"

        "QProgressBar#feeDiscountProgress { background:%2; border:1px solid %3;"
        "  border-radius:0; }"
        "QProgressBar#feeDiscountProgress::chunk { background:%4; }"
    )
        .arg(BG_BASE(),         // %1
             BG_RAISED(),       // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             TEXT_TERTIARY(),   // %6
             POSITIVE(),        // %7
             TEXT_PRIMARY(),    // %8
             TEXT_SECONDARY()); // %9

    setStyleSheet(ss);
}

void FeeDiscountPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) refresh_subscriptions();
}

void FeeDiscountPanel::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    balance_topic_.clear();
    discount_topic_.clear();
    current_pubkey_.clear();
    fncpt_held_ = 0.0;
    have_discount_ = false;
    latest_ = {};
    update_view();
}

void FeeDiscountPanel::refresh_subscriptions() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!balance_topic_.isEmpty()) hub.unsubscribe(this, balance_topic_);
    if (!discount_topic_.isEmpty()) hub.unsubscribe(this, discount_topic_);
    balance_topic_.clear();
    discount_topic_.clear();
    if (current_pubkey_.isEmpty()) return;

    balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
    discount_topic_ = QStringLiteral("billing:fncpt_discount:%1").arg(current_pubkey_);
    hub.subscribe(this, balance_topic_,
                  [this](const QVariant& v) { on_balance_update(v); });
    hub.subscribe(this, discount_topic_,
                  [this](const QVariant& v) { on_discount_update(v); });
    hub.request(balance_topic_, /*force=*/true);
    hub.request(discount_topic_, /*force=*/true);
}

void FeeDiscountPanel::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    const auto bal = v.value<fincept::wallet::WalletBalance>();
    fncpt_held_ = bal.fncpt_ui();
    update_view();
}

void FeeDiscountPanel::on_discount_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::FncptDiscount>()) return;
    latest_ = v.value<fincept::wallet::FncptDiscount>();
    have_discount_ = true;
    update_view();
}

void FeeDiscountPanel::update_view() {
    using fincept::billing::FeeDiscountConfig;

    const double threshold = have_discount_
                                 ? threshold_ui(latest_)
                                 : static_cast<double>(FeeDiscountConfig::kThresholdRaw)
                                       / std::pow(10.0, FeeDiscountConfig::kThresholdDecimals);
    const int discount_pct = have_discount_ ? latest_.discount_pct
                                            : FeeDiscountConfig::kDiscountPct;

    balance_value_->setText(
        current_pubkey_.isEmpty()
            ? QStringLiteral("—")
            : QStringLiteral("%1 $FNCPT").arg(format_token(fncpt_held_, 2)));
    threshold_value_->setText(
        QStringLiteral("%1 $FNCPT").arg(format_token(threshold, 0)));

    int pct = 0;
    if (threshold > 0.0) {
        const double frac = std::min(1.0, fncpt_held_ / threshold);
        pct = static_cast<int>(frac * 100.0);
    }
    progress_->setValue(pct);

    // SKUs as bullets
    QStringList sku_lines;
    const auto skus = have_discount_ ? latest_.applied_skus
                                     : FeeDiscountConfig::applied_skus();
    for (const auto& s : skus) {
        sku_lines.append(QStringLiteral("· ") + FeeDiscountConfig::display_label(s));
    }
    skus_value_->setText(sku_lines.join(QStringLiteral("\n")));

    // Eligibility chip + savings
    const bool eligible = have_discount_ ? latest_.eligible
                                         : (fncpt_held_ >= threshold);
    if (eligible) {
        heading_status_->setText(QStringLiteral("● %1% OFF ACTIVE").arg(discount_pct));
        heading_status_->setObjectName(QStringLiteral("feeDiscountHeadStatusOk"));
        const double save = FeeDiscountConfig::kReferencePriceUsd * discount_pct / 100.0;
        const double net = FeeDiscountConfig::kReferencePriceUsd - save;
        savings_value_->setText(
            QStringLiteral("%1 → %2  (you save %3)")
                .arg(format_usd(FeeDiscountConfig::kReferencePriceUsd))
                .arg(format_usd(net))
                .arg(format_usd(save)));
    } else {
        heading_status_->setText(QStringLiteral("LOCKED"));
        heading_status_->setObjectName(QStringLiteral("feeDiscountHeadStatus"));
        const double need = std::max(0.0, threshold - fncpt_held_);
        savings_value_->setText(
            tr("Acquire %1 more $FNCPT to unlock %2% off.")
                .arg(format_token(need, 0))
                .arg(discount_pct));
    }
    heading_status_->style()->unpolish(heading_status_);
    heading_status_->style()->polish(heading_status_);
}

void FeeDiscountPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty()) refresh_subscriptions();
}

void FeeDiscountPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    balance_topic_.clear();
    discount_topic_.clear();
}

} // namespace fincept::screens::panels
