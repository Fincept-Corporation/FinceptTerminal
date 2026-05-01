#include "screens/crypto_center/panels/TierPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/billing/TierConfig.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::panels {

namespace {

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

double atomic_to_ui(const QString& raw, int decimals) {
    if (raw.isEmpty()) return 0.0;
    bool ok = false;
    const auto u = raw.toULongLong(&ok);
    if (!ok) return 0.0;
    return static_cast<double>(u) / std::pow(10.0, std::max(0, decimals));
}

QString format_token(double v, int dp = 0) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

} // namespace

TierPanel::TierPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("tierPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &TierPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &TierPanel::on_wallet_disconnected);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        on_wallet_disconnected();
    }
}

TierPanel::~TierPanel() = default;

void TierPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("tierHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("TIER"), head);
    title->setObjectName(QStringLiteral("tierTitle"));
    current_label_ = new QLabel(QStringLiteral("current FREE"), head);
    current_label_->setObjectName(QStringLiteral("tierHeadCaption"));
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(current_label_);
    root->addWidget(head);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("tierBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(8);

    auto build_row = [body, bl](TierRow& row, const QString& label,
                                const QString& threshold,
                                const QString& unlocks) {
        row.host = new QFrame(body);
        row.host->setObjectName(QStringLiteral("tierRow"));
        auto* row_l = new QHBoxLayout(row.host);
        row_l->setContentsMargins(10, 8, 10, 8);
        row_l->setSpacing(12);
        row.label = new QLabel(label, row.host);
        row.label->setObjectName(QStringLiteral("tierRowLabel"));
        row.label->setMinimumWidth(80);
        row.threshold = new QLabel(threshold, row.host);
        row.threshold->setObjectName(QStringLiteral("tierRowThreshold"));
        row.threshold->setMinimumWidth(140);
        row.unlocks = new QLabel(unlocks, row.host);
        row.unlocks->setObjectName(QStringLiteral("tierRowUnlocks"));
        row.chip = new QLabel(QStringLiteral("[locked]"), row.host);
        row.chip->setObjectName(QStringLiteral("tierRowChipLocked"));
        row_l->addWidget(row.label);
        row_l->addWidget(row.threshold);
        row_l->addWidget(row.unlocks, 1);
        row_l->addWidget(row.chip);
        bl->addWidget(row.host);
    };

    build_row(bronze_row_, QStringLiteral("BRONZE"),
              QStringLiteral("100+ veFNCPT"),
              tr("basic API quota"));
    build_row(silver_row_, QStringLiteral("SILVER"),
              QStringLiteral("1,000+ veFNCPT"),
              tr("premium screens"));
    build_row(gold_row_, QStringLiteral("GOLD"),
              QStringLiteral("10,000+ veFNCPT"),
              tr("all agents + arena"));

    footer_ = new QLabel(tr("Connect a wallet to see your tier."), body);
    footer_->setObjectName(QStringLiteral("tierFooter"));
    footer_->setAlignment(Qt::AlignLeft);
    bl->addWidget(footer_);

    bl->addStretch(1);
    root->addWidget(body, 1);
}

void TierPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#tierPanel { background:%1; }"
        "QWidget#tierHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#tierTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#tierHeadCaption { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:600; letter-spacing:1.2px; background:transparent; }"
        "QWidget#tierBody { background:%1; }"
        "QFrame#tierRow { background:%8; border:1px solid %3; }"
        "QFrame#tierRowCurrent { background:rgba(217,119,6,0.10); border:1px solid %12; }"
        "QLabel#tierRowLabel { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#tierRowThreshold { color:%7; font-family:%5; font-size:11px;"
        "  background:transparent; }"
        "QLabel#tierRowUnlocks { color:%6; font-family:%5; font-size:11px;"
        "  background:transparent; }"
        "QLabel#tierRowChipAchieved { color:%4; background:rgba(217,119,6,0.10);"
        "  border:1px solid %12; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 6px; }"
        "QLabel#tierRowChipLocked { color:%6; background:transparent;"
        "  border:1px solid %3; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 6px; }"
        "QLabel#tierFooter { color:%6; font-family:%5; font-size:10px;"
        "  background:transparent; padding-top:6px; }"
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

void TierPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("billing:tier:%1").arg(pubkey);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_,
                      [this](const QVariant& v) { on_tier_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void TierPanel::on_wallet_disconnected() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    render_state(fincept::wallet::TierStatus::Tier::Free,
                 QString(), QString(), false);
    footer_->setText(tr("Connect a wallet to see your tier."));
    footer_->show();
}

void TierPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("billing:tier:%1").arg(current_pubkey_);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_,
                      [this](const QVariant& v) { on_tier_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void TierPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

void TierPanel::on_tier_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TierStatus>()) return;
    const auto s = v.value<fincept::wallet::TierStatus>();
    const double weight_ui = atomic_to_ui(s.weight_raw, s.decimals);
    const double next_ui = atomic_to_ui(s.next_threshold_raw, s.decimals);
    render_state(s.tier,
                 QStringLiteral("%1 veFNCPT").arg(format_token(weight_ui, 1)),
                 next_ui > 0.0
                     ? QStringLiteral("%1 veFNCPT").arg(format_token(next_ui, 0))
                     : QString(),
                 s.is_mock);
}

void TierPanel::render_state(fincept::wallet::TierStatus::Tier current,
                              const QString& weight_ui_str,
                              const QString& next_threshold_ui_str,
                              bool is_mock) {
    using Tier = fincept::wallet::TierStatus::Tier;

    auto set_row = [](TierRow& row, bool achieved, bool is_current) {
        row.chip->setText(achieved ? QStringLiteral("[achieved]")
                                   : QStringLiteral("[locked]"));
        row.chip->setObjectName(achieved ? QStringLiteral("tierRowChipAchieved")
                                         : QStringLiteral("tierRowChipLocked"));
        row.host->setObjectName(is_current ? QStringLiteral("tierRowCurrent")
                                            : QStringLiteral("tierRow"));
        row.chip->style()->unpolish(row.chip);
        row.chip->style()->polish(row.chip);
        row.host->style()->unpolish(row.host);
        row.host->style()->polish(row.host);
    };

    set_row(bronze_row_, current >= Tier::Bronze, current == Tier::Bronze);
    set_row(silver_row_, current >= Tier::Silver, current == Tier::Silver);
    set_row(gold_row_,   current >= Tier::Gold,   current == Tier::Gold);

    QString head_text = QStringLiteral("current %1")
                             .arg(fincept::billing::TierConfig::label_for(current));
    if (is_mock) head_text += QStringLiteral(" · DEMO");
    if (!weight_ui_str.isEmpty()) {
        head_text += QStringLiteral("  ·  %1").arg(weight_ui_str);
    }
    current_label_->setText(head_text);

    if (current_pubkey_.isEmpty()) {
        footer_->setText(tr("Connect a wallet to see your tier."));
        footer_->show();
    } else if (!next_threshold_ui_str.isEmpty()) {
        footer_->setText(tr("Next: lock %1 to reach the next tier.")
                              .arg(next_threshold_ui_str));
        footer_->show();
    } else if (current == Tier::Gold) {
        footer_->setText(tr("All Fincept Terminal features unlocked."));
        footer_->show();
    } else {
        footer_->hide();
    }
}

} // namespace fincept::screens::panels
