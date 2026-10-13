#include "screens/crypto_center/panels/TierPanel.h"

#include "datahub/DataHub.h"
#include "services/billing/TierConfig.h"
#include "services/wallet/StakingService.h"
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
    return QStringLiteral("'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

double atomic_to_ui(const QString& raw, int decimals) {
    if (raw.isEmpty())
        return 0.0;
    bool ok = false;
    const auto u = raw.toULongLong(&ok);
    if (!ok)
        return 0.0;
    return static_cast<double>(u) / std::pow(10.0, std::max(0, decimals));
}

QString format_token(double v, int dp = 0) {
    if (v <= 0.0)
        return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

bool staking_is_live() {
    return fincept::wallet::StakingService::instance().staking_live();
}

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

TierPanel::TierPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("tierPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &TierPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this, &TierPanel::on_wallet_disconnected);

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
    title_ = new QLabel(tr("TIER"), head);
    title_->setObjectName(QStringLiteral("tierTitle"));
    current_label_ = new QLabel(head);
    current_label_->setObjectName(QStringLiteral("tierHeadCaption"));
    hl->addWidget(title_);
    hl->addStretch();
    hl->addWidget(current_label_);
    root->addWidget(head);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("tierBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(8);

    auto build_row = [body, bl](TierRow& row) {
        row.host = new QFrame(body);
        row.host->setObjectName(QStringLiteral("tierRow"));
        auto* row_l = new QHBoxLayout(row.host);
        row_l->setContentsMargins(10, 8, 10, 8);
        row_l->setSpacing(12);
        row.label = new QLabel(row.host);
        row.label->setObjectName(QStringLiteral("tierRowLabel"));
        row.label->setMinimumWidth(80);
        row.threshold = new QLabel(row.host);
        row.threshold->setObjectName(QStringLiteral("tierRowThreshold"));
        row.threshold->setMinimumWidth(140);
        row.unlocks = new QLabel(row.host);
        row.unlocks->setObjectName(QStringLiteral("tierRowUnlocks"));
        row.chip = new QLabel(row.host);
        row.chip->setObjectName(QStringLiteral("tierRowChipLocked"));
        row_l->addWidget(row.label);
        row_l->addWidget(row.threshold);
        row_l->addWidget(row.unlocks, 1);
        row_l->addWidget(row.chip);
        bl->addWidget(row.host);
    };

    build_row(bronze_row_);
    build_row(silver_row_);
    build_row(gold_row_);
    apply_row_texts();

    footer_ = new QLabel(body);
    footer_->setObjectName(QStringLiteral("tierFooter"));
    footer_->setAlignment(Qt::AlignLeft);
    footer_->setWordWrap(true);
    bl->addWidget(footer_);

    bl->addStretch(1);
    root->addWidget(body, 1);
}

void TierPanel::apply_row_texts() {
    // The benefit column is PLANNED copy: no feature in this build checks a
    // tier, so the old "basic API quota / premium screens / all agents + arena"
    // read as entitlements the user did not actually get.
    bronze_row_.label->setText(tr("BRONZE"));
    bronze_row_.threshold->setText(tr("100+ veFNCPT"));
    bronze_row_.unlocks->setText(tr("planned: basic API quota"));
    silver_row_.label->setText(tr("SILVER"));
    silver_row_.threshold->setText(tr("1,000+ veFNCPT"));
    silver_row_.unlocks->setText(tr("planned: premium screens"));
    gold_row_.label->setText(tr("GOLD"));
    gold_row_.threshold->setText(tr("10,000+ veFNCPT"));
    gold_row_.unlocks->setText(tr("planned: all agents + arena"));
}

void TierPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral("QWidget#tierPanel { background:%1; }"
                                      "QWidget#tierHead { background:%2; border-bottom:1px solid %3; }"
                                      "QLabel#tierTitle { color:%4; font-family:%5; font-size:11px;"
                                      "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                                      "QLabel#tierHeadCaption { color:%6; font-family:%5; font-size:10px;"
                                      "  font-weight:600; letter-spacing:1.2px; background:transparent; }"
                                      "QWidget#tierBody { background:%1; }"
                                      "QFrame#tierRow { background:%8; border:1px solid %3; }"
                                      "QFrame#tierRowCurrent { background:rgba(217,119,6,0.10); border:1px solid %9; }"
                                      "QLabel#tierRowLabel { color:%4; font-family:%5; font-size:11px;"
                                      "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                                      "QLabel#tierRowThreshold { color:%7; font-family:%5; font-size:11px;"
                                      "  background:transparent; }"
                                      "QLabel#tierRowUnlocks { color:%6; font-family:%5; font-size:11px;"
                                      "  background:transparent; }"
                                      "QLabel#tierRowChipAchieved { color:%4; background:rgba(217,119,6,0.10);"
                                      "  border:1px solid %9; font-family:%5; font-size:9px; font-weight:700;"
                                      "  letter-spacing:1.2px; padding:2px 6px; }"
                                      "QLabel#tierRowChipLocked { color:%6; background:transparent;"
                                      "  border:1px solid %3; font-family:%5; font-size:9px; font-weight:700;"
                                      "  letter-spacing:1.2px; padding:2px 6px; }"
                                      "QLabel#tierFooter { color:%6; font-family:%5; font-size:10px;"
                                      "  background:transparent; padding-top:6px; }")
                           // One arg per marker, in ascending marker order.
                           // QString::arg fills the LOWEST marker present, so a
                           // gap in the numbering silently shifts every later
                           // colour by one and warns "Argument missing" for the
                           // overflow — that is how %12 used to be painted with
                           // NEGATIVE (#dc2626) instead of the dark amber.
                           .arg(BG_BASE(),                  // %1
                                BG_SURFACE(),               // %2
                                BORDER_DIM(),               // %3
                                AMBER(),                    // %4
                                font,                       // %5
                                TEXT_TERTIARY(),            // %6
                                TEXT_PRIMARY(),             // %7
                                BG_RAISED(),                // %8
                                QStringLiteral("#78350f")); // %9 darker amber
    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void TierPanel::subscribe_if_live() {
    if (!staking_is_live() || current_pubkey_.isEmpty() || !current_topic_.isEmpty())
        return;
    current_topic_ = QStringLiteral("billing:tier:%1").arg(current_pubkey_);
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, current_topic_, [this](const QVariant& v) { on_tier_update(v); });
    hub.request(current_topic_, /*force=*/false);
}

void TierPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (!staking_is_live()) {
        render_planned();
        return;
    }
    if (isVisible())
        subscribe_if_live();
}

void TierPanel::on_wallet_disconnected() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    if (!staking_is_live()) {
        render_planned();
        return;
    }
    render_state(fincept::wallet::TierStatus::Tier::Free, QString(), QString());
}

void TierPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    subscribe_if_live();
}

void TierPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

void TierPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TierPanel::retranslateUi() {
    if (title_)
        title_->setText(tr("TIER"));
    apply_row_texts();
    if (!staking_is_live()) {
        render_planned();
        return;
    }
    // Chips: re-apply text using the objectName as the reached flag (set by
    // render_state) so the live state survives the locale switch. The head and
    // footer refresh on the next tier update.
    auto retr_chip = [](QLabel* chip) {
        const bool reached = chip->objectName() == QLatin1String("tierRowChipAchieved");
        chip->setText(reached ? TierPanel::tr("[reached]") : TierPanel::tr("[not reached]"));
    };
    retr_chip(bronze_row_.chip);
    retr_chip(silver_row_.chip);
    retr_chip(gold_row_.chip);
    if (footer_ && current_pubkey_.isEmpty())
        footer_->setText(tr("Connect a wallet to see your tier."));
}

void TierPanel::on_tier_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TierStatus>())
        return;
    const auto s = v.value<fincept::wallet::TierStatus>();
    const double weight_ui = atomic_to_ui(s.weight_raw, s.decimals);
    // next_threshold_raw is the ABSOLUTE weight of the next tier (e.g. 1,000),
    // not what is still missing — show the remainder.
    const double next_ui = atomic_to_ui(s.next_threshold_raw, s.decimals);
    const double remaining_ui = std::ceil(std::max(0.0, next_ui - weight_ui));
    render_state(s.tier, QStringLiteral("%1 veFNCPT").arg(format_token(weight_ui, 1)),
                 (next_ui > 0.0 && remaining_ui > 0.0) ? format_token(remaining_ui, 0) : QString());
}

void TierPanel::render_planned() {
    for (TierRow* row : {&bronze_row_, &silver_row_, &gold_row_}) {
        row->chip->setText(tr("[planned]"));
        row->chip->setObjectName(QStringLiteral("tierRowChipLocked"));
        row->host->setObjectName(QStringLiteral("tierRow"));
        repolish(row->chip);
        repolish(row->host);
    }
    current_label_->setText(tr("PLANNED · NOT ACTIVE"));
    footer_->setText(tr("Tiers are planned, not active: no feature in this build is unlocked or restricted by "
                        "tier, and holding $FNCPT does not change what you can use. Tiers will be computed from "
                        "locked veFNCPT weight once on-chain staking launches."));
    footer_->show();
}

void TierPanel::render_state(fincept::wallet::TierStatus::Tier current, const QString& weight_ui_str,
                             const QString& remaining_ui_str) {
    using Tier = fincept::wallet::TierStatus::Tier;

    auto set_row = [](TierRow& row, bool reached, bool is_current) {
        row.chip->setText(reached ? TierPanel::tr("[reached]") : TierPanel::tr("[not reached]"));
        row.chip->setObjectName(reached ? QStringLiteral("tierRowChipAchieved") : QStringLiteral("tierRowChipLocked"));
        row.host->setObjectName(is_current ? QStringLiteral("tierRowCurrent") : QStringLiteral("tierRow"));
        repolish(row.chip);
        repolish(row.host);
    };

    set_row(bronze_row_, current >= Tier::Bronze, current == Tier::Bronze);
    set_row(silver_row_, current >= Tier::Silver, current == Tier::Silver);
    set_row(gold_row_, current >= Tier::Gold, current == Tier::Gold);

    QString head_text = tr("current %1").arg(fincept::billing::TierConfig::label_for(current));
    if (!weight_ui_str.isEmpty())
        head_text += QStringLiteral("  ·  %1").arg(weight_ui_str);
    current_label_->setText(head_text);

    // Every footer variant carries the "planned" qualifier — reaching a tier
    // does not unlock anything until tier gating exists.
    if (current_pubkey_.isEmpty()) {
        footer_->setText(tr("Connect a wallet to see your tier."));
    } else if (!remaining_ui_str.isEmpty()) {
        footer_->setText(tr("%1 more veFNCPT weight needed to reach the next tier. Tier benefits are planned "
                            "and not active yet.")
                             .arg(remaining_ui_str));
    } else if (current == Tier::Gold) {
        footer_->setText(tr("Top tier reached. Tier benefits are planned and not active yet."));
    } else {
        footer_->setText(tr("Tier benefits are planned and not active yet."));
    }
    footer_->show();
}

} // namespace fincept::screens::panels
