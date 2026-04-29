#include "screens/crypto_center/HoldingsBar.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <cmath>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QSet>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>

namespace fincept::screens {

namespace {

constexpr qint64 kStaleThresholdMs = 90 * 1000;
constexpr int kStalenessTickMs = 5 * 1000;

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_token(double v, int max_dp = 4) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', max_dp);
}

QString format_usd(double v) {
    if (v < 0.0) return QStringLiteral("—");
    if (v >= 1.0 || v == 0.0) {
        return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 2));
    }
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 6));
}

QString format_price(double v) {
    if (v <= 0.0) return QStringLiteral("—");
    if (v >= 1.0) {
        return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 4));
    }
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 8));
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

QString rpc_provider_label() {
    auto override_res =
        SecureStorage::instance().retrieve(QStringLiteral("solana.rpc_url"));
    if (override_res.is_ok() && !override_res.value().isEmpty()) {
        return QStringLiteral("CUSTOM");
    }
    auto helius_res =
        SecureStorage::instance().retrieve(QStringLiteral("solana.helius_api_key"));
    if (helius_res.is_ok() && !helius_res.value().isEmpty()) {
        return QStringLiteral("HELIUS");
    }
    return QStringLiteral("PUBLIC");
}

} // namespace

HoldingsBar::HoldingsBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("holdingsBar"));
    setFixedHeight(56);

    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &HoldingsBar::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &HoldingsBar::on_wallet_disconnected);
    connect(&svc, &fincept::wallet::WalletService::balance_mode_changed, this,
            &HoldingsBar::on_mode_changed);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &HoldingsBar::on_topic_error);

    staleness_timer_ = new QTimer(this);
    staleness_timer_->setInterval(kStalenessTickMs);
    connect(staleness_timer_, &QTimer::timeout, this,
            &HoldingsBar::update_staleness);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        set_feed_status(FeedStatus::Idle);
    }
    update_rpc_indicator();
}

HoldingsBar::~HoldingsBar() = default;

void HoldingsBar::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14, 8, 14, 8);
    root->setSpacing(18);

    auto add_metric = [this, root](const QString& label_text, QLabel*& value_out,
                                   const QString& object_name) {
        auto* col = new QVBoxLayout;
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);
        auto* label = new QLabel(label_text, this);
        label->setObjectName(QStringLiteral("holdingsBarLabel"));
        value_out = new QLabel(QStringLiteral("—"), this);
        value_out->setObjectName(object_name);
        value_out->setTextInteractionFlags(Qt::TextSelectableByMouse);
        col->addWidget(label);
        col->addWidget(value_out);
        root->addLayout(col);
    };

    add_metric(QStringLiteral("SOL"), sol_value_,
               QStringLiteral("holdingsBarValue"));
    add_metric(QStringLiteral("$FNCPT"), fncpt_value_,
               QStringLiteral("holdingsBarValueAccent"));
    add_metric(QStringLiteral("TOTAL"), total_value_,
               QStringLiteral("holdingsBarValue"));
    add_metric(QStringLiteral("$FNCPT PRICE"), fncpt_price_value_,
               QStringLiteral("holdingsBarValueDim"));
    add_metric(QStringLiteral("UPDATED"), updated_value_,
               QStringLiteral("holdingsBarValueDim"));

    root->addStretch(1);

    discount_chip_ = new QLabel(QStringLiteral("30% OFF"), this);
    discount_chip_->setObjectName(QStringLiteral("holdingsBarDiscountChip"));
    discount_chip_->hide();
    root->addWidget(discount_chip_);

    feed_status_ = new QLabel(QStringLiteral("○ IDLE"), this);
    feed_status_->setObjectName(QStringLiteral("holdingsBarFeedIdle"));
    root->addWidget(feed_status_);

    rpc_indicator_ = new QLabel(rpc_provider_label(), this);
    rpc_indicator_->setObjectName(QStringLiteral("holdingsBarRpcChip"));
    root->addWidget(rpc_indicator_);
}

void HoldingsBar::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#holdingsBar { background:%1; border-bottom:1px solid %2; }"

        // Metric labels
        "QLabel#holdingsBarLabel { color:%3; font-family:%4; font-size:9px;"
        "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
        "QLabel#holdingsBarValue { color:%5; font-family:%4; font-size:14px;"
        "  font-weight:600; background:transparent; }"
        "QLabel#holdingsBarValueAccent { color:%6; font-family:%4; font-size:14px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#holdingsBarValueDim { color:%7; font-family:%4; font-size:12px;"
        "  background:transparent; }"

        // Discount chip
        "QLabel#holdingsBarDiscountChip { color:%8; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:rgba(34,197,94,0.10);"
        "  border:1px solid %8; padding:2px 8px; }"

        // Feed status (5 variants)
        "QLabel#holdingsBarFeedIdle { color:%3; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent;"
        "  padding:2px 6px; }"
        "QLabel#holdingsBarFeedConnecting { color:%6; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent;"
        "  padding:2px 6px; }"
        "QLabel#holdingsBarFeedLive { color:%8; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent;"
        "  padding:2px 6px; }"
        "QLabel#holdingsBarFeedStale { color:%6; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent;"
        "  padding:2px 6px; }"
        "QLabel#holdingsBarFeedError { color:%9; font-family:%4; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent;"
        "  padding:2px 6px; }"

        // RPC chip
        "QLabel#holdingsBarRpcChip { color:%3; font-family:%4; font-size:9px;"
        "  font-weight:700; letter-spacing:1.2px; background:%10;"
        "  border:1px solid %2; padding:2px 6px; }"
    )
        .arg(BG_SURFACE(),      // %1
             BORDER_DIM(),      // %2
             TEXT_TERTIARY(),   // %3
             font,              // %4
             TEXT_PRIMARY(),    // %5
             AMBER(),           // %6
             TEXT_SECONDARY(),  // %7
             POSITIVE())        // %8
        .arg(NEGATIVE(),        // %9
             BG_RAISED());      // %10

    setStyleSheet(ss);
}

void HoldingsBar::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    sol_value_->setText(QStringLiteral("—"));
    fncpt_value_->setText(QStringLiteral("—"));
    total_value_->setText(QStringLiteral("—"));
    fncpt_price_value_->setText(QStringLiteral("—"));
    updated_value_->setText(QStringLiteral("waiting…"));
    last_balance_ts_ = 0;
    first_publish_received_ = false;
    update_rpc_indicator();
    set_feed_status(FeedStatus::Connecting);

    if (isVisible()) {
        refresh_subscription();
        if (!staleness_timer_->isActive()) staleness_timer_->start();
    }
}

void HoldingsBar::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_balance_topic_.clear();
    price_topic_.clear();
    price_usd_.clear();
    current_pubkey_.clear();
    latest_balance_ = {};
    last_balance_ts_ = 0;
    first_publish_received_ = false;
    sol_value_->setText(QStringLiteral("—"));
    fncpt_value_->setText(QStringLiteral("—"));
    total_value_->setText(QStringLiteral("—"));
    fncpt_price_value_->setText(QStringLiteral("—"));
    updated_value_->setText(QStringLiteral("—"));
    discount_chip_->hide();
    set_feed_status(FeedStatus::Idle);
    staleness_timer_->stop();
}

void HoldingsBar::on_mode_changed(bool /*is_stream*/) {
    if (current_pubkey_.isEmpty()) return;
    set_feed_status(FeedStatus::Connecting);
}

void HoldingsBar::refresh_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_balance_topic_.isEmpty()) {
        hub.unsubscribe(this, current_balance_topic_);
        current_balance_topic_.clear();
    }
    if (current_pubkey_.isEmpty()) return;
    current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
    hub.subscribe(this, current_balance_topic_,
                  [this](const QVariant& v) { on_balance_update(v); });
    hub.request(current_balance_topic_, /*force=*/true);

    // Phase 2 §2C: subscribe to fee-discount eligibility for this pubkey.
    const auto discount_topic =
        QStringLiteral("billing:fncpt_discount:%1").arg(current_pubkey_);
    hub.subscribe(this, discount_topic,
                  [this](const QVariant& v) { on_discount_update(v); });
    hub.request(discount_topic, /*force=*/true);
}

void HoldingsBar::on_discount_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::FncptDiscount>()) return;
    const auto d = v.value<fincept::wallet::FncptDiscount>();
    if (!discount_chip_) return;
    if (d.eligible) {
        discount_chip_->setText(QStringLiteral("%1% OFF").arg(d.discount_pct));
        discount_chip_->setToolTip(
            tr("Holding ≥ %1 $FNCPT — you qualify for the fee discount.")
                .arg(d.threshold_raw / std::pow(10.0, d.threshold_decimals), 0, 'f', 0));
        discount_chip_->show();
    } else {
        discount_chip_->hide();
    }
}

void HoldingsBar::resubscribe_prices() {
    auto& hub = fincept::datahub::DataHub::instance();

    // Always include SOL + FNCPT, plus every held token.
    QSet<QString> wanted;
    wanted.insert(QString::fromLatin1(fincept::wallet::kWrappedSolMint));
    wanted.insert(QString::fromLatin1(fincept::wallet::kFncptMint));
    for (const auto& t : latest_balance_.tokens) {
        if (!t.mint.isEmpty()) wanted.insert(t.mint);
    }

    // Drop stale subscriptions.
    for (auto it = price_topic_.begin(); it != price_topic_.end();) {
        if (!wanted.contains(it.key())) {
            hub.unsubscribe(this, it.value());
            price_usd_.remove(it.key());
            it = price_topic_.erase(it);
        } else {
            ++it;
        }
    }

    // Subscribe to anything new.
    for (const auto& mint : wanted) {
        if (price_topic_.contains(mint)) continue;
        const auto topic = QStringLiteral("market:price:token:%1").arg(mint);
        price_topic_.insert(mint, topic);
        hub.subscribe(this, topic, [this, mint](const QVariant& v) {
            on_price_update(mint, v);
        });
        hub.request(topic, /*force=*/true);
    }
}

void HoldingsBar::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>()) return;
    latest_balance_ = v.value<fincept::wallet::WalletBalance>();
    sol_value_->setText(format_token(latest_balance_.sol(), 4));
    fncpt_value_->setText(format_token(latest_balance_.fncpt_ui(), 2));
    last_balance_ts_ = latest_balance_.ts_ms;
    first_publish_received_ = true;
    set_feed_status(FeedStatus::Live);
    resubscribe_prices();
    recompute_total();
    update_staleness();
}

void HoldingsBar::on_price_update(const QString& mint, const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TokenPrice>()) return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (!p.valid) return;
    price_usd_[mint] = p.usd;
    // Update the dedicated $FNCPT price cell when that mint refreshes.
    if (mint == QString::fromLatin1(fincept::wallet::kFncptMint)) {
        fncpt_price_value_->setText(format_price(p.usd));
    }
    recompute_total();
}

void HoldingsBar::recompute_total() {
    double total = 0.0;
    int unpriced = 0;
    bool any_known = false;

    // SOL
    const auto sol_mint = QString::fromLatin1(fincept::wallet::kWrappedSolMint);
    const double sol_amt = latest_balance_.sol();
    const double sol_px = price_usd_.value(sol_mint, -1.0);
    if (sol_amt > 0.0) {
        if (sol_px >= 0.0) {
            total += sol_amt * sol_px;
            any_known = true;
        } else {
            ++unpriced;
        }
    }

    // Tokens
    for (const auto& t : latest_balance_.tokens) {
        const double amt = t.ui_amount();
        if (amt <= 0.0) continue;
        const double px = price_usd_.value(t.mint, -1.0);
        if (px >= 0.0) {
            total += amt * px;
            any_known = true;
        } else {
            ++unpriced;
        }
    }

    total_value_->setText(any_known ? format_usd(total) : QStringLiteral("—"));
    if (unpriced > 0) {
        total_value_->setToolTip(
            tr("%1 holding(s) excluded — no live price.").arg(unpriced));
    } else {
        total_value_->setToolTip(QString());
    }
}

void HoldingsBar::on_topic_error(const QString& topic, const QString& error) {
    if (topic == current_balance_topic_) {
        set_feed_status(FeedStatus::Error);
        LOG_DEBUG("HoldingsBar", "balance error: " + error);
    }
}

void HoldingsBar::set_feed_status(FeedStatus s) {
    if (s == feed_status_state_) return;
    feed_status_state_ = s;
    if (!feed_status_) return;
    QString text;
    QString object_name;
    switch (s) {
        case FeedStatus::Idle:
            text = QStringLiteral("○ IDLE");
            object_name = QStringLiteral("holdingsBarFeedIdle");
            break;
        case FeedStatus::Connecting:
            text = QStringLiteral("◌ CONNECTING");
            object_name = QStringLiteral("holdingsBarFeedConnecting");
            break;
        case FeedStatus::Live:
            text = QStringLiteral("● LIVE");
            object_name = QStringLiteral("holdingsBarFeedLive");
            break;
        case FeedStatus::Stale:
            text = QStringLiteral("◐ STALE");
            object_name = QStringLiteral("holdingsBarFeedStale");
            break;
        case FeedStatus::Error:
            text = QStringLiteral("✕ ERROR");
            object_name = QStringLiteral("holdingsBarFeedError");
            break;
    }
    feed_status_->setText(text);
    feed_status_->setObjectName(object_name);
    feed_status_->style()->unpolish(feed_status_);
    feed_status_->style()->polish(feed_status_);
}

void HoldingsBar::update_rpc_indicator() {
    if (!rpc_indicator_) return;
    const auto label = rpc_provider_label();
    rpc_indicator_->setText(label);
    if (label == QStringLiteral("PUBLIC")) {
        rpc_indicator_->setToolTip(
            tr("Public Solana RPC. STREAM may degrade — add a Helius API key "
               "in Settings for reliable WebSocket subscriptions."));
    } else if (label == QStringLiteral("HELIUS")) {
        rpc_indicator_->setToolTip(tr("Helius RPC — STREAM fully supported."));
    } else {
        rpc_indicator_->setToolTip(tr("Custom RPC override active."));
    }
}

void HoldingsBar::update_staleness() {
    if (!first_publish_received_ || last_balance_ts_ == 0) return;
    updated_value_->setText(relative_time(last_balance_ts_));
    const auto age = QDateTime::currentMSecsSinceEpoch() - last_balance_ts_;
    if (age > kStaleThresholdMs && feed_status_state_ == FeedStatus::Live) {
        set_feed_status(FeedStatus::Stale);
    }
}

void HoldingsBar::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty()) {
        refresh_subscription();
        if (!staleness_timer_->isActive()) staleness_timer_->start();
    }
    update_rpc_indicator();
}

void HoldingsBar::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    staleness_timer_->stop();
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_balance_topic_.clear();
    price_topic_.clear();
}

} // namespace fincept::screens
