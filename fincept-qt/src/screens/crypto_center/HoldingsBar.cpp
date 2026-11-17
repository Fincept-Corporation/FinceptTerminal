#include "screens/crypto_center/HoldingsBar.h"
#include "screens/crypto_center/CryptoCenterText.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/markets/FxRateService.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QSet>
#include <QShowEvent>
#include <QStringList>
#include <QStyle>
#include <QTimer>

#include <cmath>

namespace cc_fmt = fincept::ui::formatting;
namespace cc_fx = fincept::services::fx;

namespace fincept::screens {

namespace {

constexpr qint64 kStaleThresholdMs = 90 * 1000;
constexpr int kStalenessTickMs = 5 * 1000;

using fincept::screens::crypto_center_text::font_stack; // CryptoCenterText.h (one definition)

/// A token amount: a quantity of that token (SOL, $FNCPT), never money.
QString format_token(double v, int max_dp = 4) {
    if (v <= 0.0)
        return QStringLiteral("0");
    return cc_fmt::quantity(v, max_dp);
}

/// A token's spot price: the price feed quotes USD — a price, never converted.
QString format_price(double v) {
    if (v <= 0.0)
        return QStringLiteral("—");
    return cc_fmt::price_with_symbol(v, cc_fmt::InstrumentFmt{QStringLiteral("USD")});
}

using fincept::screens::crypto_center_text::relative_time; // CryptoCenterText.h (one definition)

/// "CUSTOM" / "HELIUS" / "PUBLIC" — or empty when the stored setting that decides
/// it cannot be read (`error` then gets the reason): that is not PUBLIC.
QString rpc_provider_label(QString* error = nullptr) {
    const SecretRead override_res = SecureStorage::instance().read(QStringLiteral("solana.rpc_url"));
    if (override_res.failed()) {
        if (error)
            *error = override_res.reason();
        return {};
    }
    if (override_res.found() && !override_res.value.isEmpty()) {
        return QStringLiteral("CUSTOM");
    }
    const SecretRead helius_res = SecureStorage::instance().read(QStringLiteral("solana.helius_api_key"));
    if (helius_res.failed()) {
        if (error)
            *error = helius_res.reason();
        return {};
    }
    if (helius_res.found() && !helius_res.value.isEmpty()) {
        return QStringLiteral("HELIUS");
    }
    return QStringLiteral("PUBLIC");
}

} // namespace

HoldingsBar::HoldingsBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("holdingsBar"));
    // At least 56px; taller when the user's font size needs it.
    setMinimumHeight(56);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &HoldingsBar::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this, &HoldingsBar::on_wallet_disconnected);
    connect(&svc, &fincept::wallet::WalletService::balance_mode_changed, this, &HoldingsBar::on_mode_changed);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this, &HoldingsBar::on_topic_error);

    staleness_timer_ = new QTimer(this);
    staleness_timer_->setInterval(kStalenessTickMs);
    connect(staleness_timer_, &QTimer::timeout, this, &HoldingsBar::update_staleness);

    // TOTAL is shown in the display currency: redraw it when an FX rate lands.
    connect(&fincept::services::FxRateService::instance(), &fincept::services::FxRateService::rates_changed, this,
            [this]() {
                if (first_publish_received_)
                    recompute_total();
            });
    // A number / currency format change re-renders the figures already held (no re-fetch).
    cc_fmt::on_format_changed(this, [this]() {
        if (!first_publish_received_)
            return;
        sol_value_->setText(format_token(latest_balance_.sol(), 4));
        fncpt_value_->setText(format_token(latest_balance_.fncpt_ui(), 2));
        const QString fncpt_mint = QString::fromLatin1(fincept::wallet::kFncptMint);
        if (price_usd_.contains(fncpt_mint))
            fncpt_price_value_->setText(format_price(price_usd_.value(fncpt_mint)));
        recompute_total();
    });

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

    auto add_metric = [this, root](const QString& label_text, QLabel*& label_out, QLabel*& value_out,
                                   const QString& object_name) {
        auto* col = new QVBoxLayout;
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);
        label_out = new QLabel(label_text, this);
        label_out->setObjectName(QStringLiteral("holdingsBarLabel"));
        value_out = new QLabel(QStringLiteral("—"), this);
        value_out->setObjectName(object_name);
        value_out->setTextInteractionFlags(Qt::TextSelectableByMouse);
        col->addWidget(label_out);
        col->addWidget(value_out);
        root->addLayout(col);
    };

    add_metric(tr("SOL"), sol_caption_, sol_value_, QStringLiteral("holdingsBarValue"));
    add_metric(tr("$FNCPT"), fncpt_caption_, fncpt_value_, QStringLiteral("holdingsBarValueAccent"));
    add_metric(tr("TOTAL"), total_caption_, total_value_, QStringLiteral("holdingsBarValue"));
    add_metric(tr("$FNCPT PRICE"), fncpt_price_caption_, fncpt_price_value_, QStringLiteral("holdingsBarValueDim"));
    add_metric(tr("UPDATED"), updated_caption_, updated_value_, QStringLiteral("holdingsBarValueDim"));

    root->addStretch(1);

    discount_chip_ = new QLabel(this); // text set when eligibility arrives
    discount_chip_->setObjectName(QStringLiteral("holdingsBarDiscountChip"));
    discount_chip_->hide();
    root->addWidget(discount_chip_);

    feed_status_ = new QLabel(tr("○ IDLE"), this);
    feed_status_->setObjectName(QStringLiteral("holdingsBarFeedIdle"));
    root->addWidget(feed_status_);

    rpc_indicator_ = new QLabel(rpc_provider_label(), this);
    rpc_indicator_->setObjectName(QStringLiteral("holdingsBarRpcChip"));
    root->addWidget(rpc_indicator_);
}

void HoldingsBar::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QString("QWidget#holdingsBar { background:%1; border-bottom:1px solid %2; }"

                                      // Metric labels
                                      "QLabel#holdingsBarLabel { color:%3; font-family:%4; font-size:" + fincept::ui::fonts::px(-5) + "px;"
                                      "  font-weight:700; letter-spacing:1.5px; background:transparent; }"
                                      "QLabel#holdingsBarValue { color:%5; font-family:%4; font-size:" + fincept::ui::fonts::px(0) + "px;"
                                      "  font-weight:600; background:transparent; }"
                                      "QLabel#holdingsBarValueAccent { color:%6; font-family:%4; font-size:" + fincept::ui::fonts::px(0) + "px;"
                                      "  font-weight:700; background:transparent; }"
                                      "QLabel#holdingsBarValueDim { color:%7; font-family:%4; font-size:" + fincept::ui::fonts::px(-2) + "px;"
                                      "  background:transparent; }"

                                      // Discount chip — amber/neutral, not "money saved" green:
                                      // the discount is planned, not applied.
                                      "QLabel#holdingsBarDiscountChip { color:%6; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:" + ui::colors::alpha(AMBER, 0.08) + ";"
                                      "  border:1px solid %2; padding:2px 8px; }"

                                      // Feed status (5 variants)
                                      "QLabel#holdingsBarFeedIdle { color:%3; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent;"
                                      "  padding:2px 6px; }"
                                      "QLabel#holdingsBarFeedConnecting { color:%6; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent;"
                                      "  padding:2px 6px; }"
                                      "QLabel#holdingsBarFeedLive { color:%8; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent;"
                                      "  padding:2px 6px; }"
                                      "QLabel#holdingsBarFeedStale { color:%6; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent;"
                                      "  padding:2px 6px; }"
                                      "QLabel#holdingsBarFeedError { color:%9; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent;"
                                      "  padding:2px 6px; }"

                                      // RPC chip
                                      "QLabel#holdingsBarRpcChip { color:%3; font-family:%4; font-size:" + fincept::ui::fonts::px(-5) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:%10;"
                                      "  border:1px solid %2; padding:2px 6px; }")
                           .arg(BG_SURFACE(),     // %1
                                BORDER_DIM(),     // %2
                                TEXT_TERTIARY(),  // %3
                                font,             // %4
                                TEXT_PRIMARY(),   // %5
                                AMBER(),          // %6
                                TEXT_SECONDARY(), // %7
                                SUCCESS())        // %8
                           .arg(DANGER(),         // %9
                                BG_RAISED());     // %10

    setStyleSheet(ss);
}

void HoldingsBar::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    sol_value_->setText(QStringLiteral("—"));
    fncpt_value_->setText(QStringLiteral("—"));
    total_value_->setText(QStringLiteral("—"));
    fncpt_price_value_->setText(QStringLiteral("—"));
    updated_value_->setText(tr("waiting…"));
    last_balance_ts_ = 0;
    first_publish_received_ = false;
    update_rpc_indicator();
    set_feed_status(FeedStatus::Connecting);

    if (isVisible()) {
        refresh_subscription();
        if (!staleness_timer_->isActive())
            staleness_timer_->start();
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
    if (current_pubkey_.isEmpty())
        return;
    set_feed_status(FeedStatus::Connecting);
}

void HoldingsBar::refresh_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_balance_topic_.isEmpty()) {
        hub.unsubscribe(this, current_balance_topic_);
        current_balance_topic_.clear();
    }
    if (current_pubkey_.isEmpty())
        return;
    current_balance_topic_ = QStringLiteral("wallet:balance:%1").arg(current_pubkey_);
    // subscribe() delivers the cached value and cold-starts a fetch when it is
    // stale. force=false lets the hub's min_interval coalesce this bar with the
    // HOME / TRADE panels into one RPC round-trip instead of a forced refetch
    // per panel on every show. REFRESH stays a forced, user-driven fetch.
    hub.subscribe(this, current_balance_topic_, [this](const QVariant& v) { on_balance_update(v); });
    hub.request(current_balance_topic_, /*force=*/false);

    // Phase 2 §2C: subscribe to fee-discount eligibility for this pubkey.
    const auto discount_topic = QStringLiteral("billing:fncpt_discount:%1").arg(current_pubkey_);
    hub.subscribe(this, discount_topic, [this](const QVariant& v) { on_discount_update(v); });
    hub.request(discount_topic, /*force=*/false);
}

void HoldingsBar::on_discount_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::FncptDiscount>())
        return;
    const auto d = v.value<fincept::wallet::FncptDiscount>();
    if (!discount_chip_)
        return;
    if (d.eligible) {
        // The discount is planned only — nothing applies it — so the chip must
        // not read like an active "30% OFF" entitlement.
        discount_chip_->setText(tr("%1% OFF · PLANNED").arg(d.discount_pct));
        discount_chip_->setToolTip(tr("Holding ≥ %1 $FNCPT qualifies for the planned fee discount. "
                                      "Not active yet — no purchase in the terminal is discounted.")
                                       .arg(cc_fmt::quantity(d.threshold_raw / std::pow(10.0, d.threshold_decimals), 0)));
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
        if (!t.mint.isEmpty())
            wanted.insert(t.mint);
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
        if (price_topic_.contains(mint))
            continue;
        const auto topic = QStringLiteral("market:price:token:%1").arg(mint);
        price_topic_.insert(mint, topic);
        hub.subscribe(this, topic, [this, mint](const QVariant& v) { on_price_update(mint, v); });
        hub.request(topic, /*force=*/false);
    }
}

void HoldingsBar::on_balance_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::WalletBalance>())
        return;
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
    if (!v.canConvert<fincept::wallet::TokenPrice>())
        return;
    const auto p = v.value<fincept::wallet::TokenPrice>();
    if (!p.valid)
        return;
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
        if (amt <= 0.0)
            continue;
        const double px = price_usd_.value(t.mint, -1.0);
        if (px >= 0.0) {
            total += amt * px;
            any_known = true;
        } else {
            ++unpriced;
        }
    }

    QStringList tip;
    if (!any_known) {
        total_value_->setText(QStringLiteral("—"));
    } else {
        // The wallet total is an aggregate: shown in the display currency chosen in
        // Settings. Token prices — and so this sum — are USD; "≈" marks a stale rate,
        // "—" a missing one (never a 1:1 guess). The USD figure and the rate are in
        // the tooltip.
        const QString usd = QStringLiteral("USD");
        const cc_fmt::ConvertedMoney c = cc_fx::convert_for_display(total, usd);
        total_value_->setText(cc_fmt::converted_money(c));
        if (!c.ok()) {
            tip << tr("%1 in USD — no exchange rate to %2 yet.").arg(cc_fmt::money_precise(total, usd), c.currency);
        } else if (c.converted) {
            tip << tr("%1 in USD").arg(cc_fmt::money_precise(total, usd)) << cc_fmt::fx_note(c);
        }
    }
    if (unpriced > 0)
        tip << tr("%1 holding(s) excluded — no live price.").arg(unpriced);
    total_value_->setToolTip(tip.join(QLatin1Char('\n')));
}

void HoldingsBar::on_topic_error(const QString& topic, const QString& error) {
    if (topic == current_balance_topic_) {
        set_feed_status(FeedStatus::Error);
        LOG_DEBUG("HoldingsBar", "balance error: " + error);
    }
}

void HoldingsBar::set_feed_status(FeedStatus s) {
    if (s == feed_status_state_)
        return;
    feed_status_state_ = s;
    if (!feed_status_)
        return;
    QString text;
    QString object_name;
    switch (s) {
        case FeedStatus::Idle:
            text = tr("○ IDLE");
            object_name = QStringLiteral("holdingsBarFeedIdle");
            break;
        case FeedStatus::Connecting:
            text = tr("◌ CONNECTING");
            object_name = QStringLiteral("holdingsBarFeedConnecting");
            break;
        case FeedStatus::Live:
            text = tr("● LIVE");
            object_name = QStringLiteral("holdingsBarFeedLive");
            break;
        case FeedStatus::Stale:
            text = tr("◐ STALE");
            object_name = QStringLiteral("holdingsBarFeedStale");
            break;
        case FeedStatus::Error:
            text = tr("✕ ERROR");
            object_name = QStringLiteral("holdingsBarFeedError");
            break;
    }
    feed_status_->setText(text);
    feed_status_->setObjectName(object_name);
    feed_status_->style()->unpolish(feed_status_);
    feed_status_->style()->polish(feed_status_);
}

void HoldingsBar::update_rpc_indicator() {
    if (!rpc_indicator_)
        return;
    QString read_error;
    const auto label = rpc_provider_label(&read_error);
    if (!read_error.isEmpty()) {
        // Re-read on every show / connect / tab change, so it clears by itself.
        rpc_indicator_->setText(tr("RPC UNREADABLE"));
        rpc_indicator_->setToolTip(tr("Could not read the stored RPC setting: %1. No RPC request is sent until it "
                                      "can be read again.")
                                       .arg(read_error));
        return;
    }
    rpc_indicator_->setText(label);
    if (label == QStringLiteral("PUBLIC")) {
        rpc_indicator_->setToolTip(tr("Public Solana RPC. STREAM may degrade — add a Helius API key "
                                      "in Settings for reliable WebSocket subscriptions."));
    } else if (label == QStringLiteral("HELIUS")) {
        rpc_indicator_->setToolTip(tr("Helius RPC — STREAM fully supported."));
    } else {
        rpc_indicator_->setToolTip(tr("Custom RPC override active."));
    }
}

void HoldingsBar::update_staleness() {
    if (!first_publish_received_ || last_balance_ts_ == 0)
        return;
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
        if (!staleness_timer_->isActive())
            staleness_timer_->start();
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

void HoldingsBar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void HoldingsBar::retranslateUi() {
    // Metric captions (SOL / $FNCPT are kept as-is in the source; translators
    // typically leave ticker captions untranslated).
    if (sol_caption_)
        sol_caption_->setText(tr("SOL"));
    if (fncpt_caption_)
        fncpt_caption_->setText(tr("$FNCPT"));
    if (total_caption_)
        total_caption_->setText(tr("TOTAL"));
    if (fncpt_price_caption_)
        fncpt_price_caption_->setText(tr("$FNCPT PRICE"));
    if (updated_caption_)
        updated_caption_->setText(tr("UPDATED"));

    // Feed-status pill — re-render the current state's label in the new locale.
    if (feed_status_) {
        switch (feed_status_state_) {
            case FeedStatus::Idle:
                feed_status_->setText(tr("○ IDLE"));
                break;
            case FeedStatus::Connecting:
                feed_status_->setText(tr("◌ CONNECTING"));
                break;
            case FeedStatus::Live:
                feed_status_->setText(tr("● LIVE"));
                break;
            case FeedStatus::Stale:
                feed_status_->setText(tr("◐ STALE"));
                break;
            case FeedStatus::Error:
                feed_status_->setText(tr("✕ ERROR"));
                break;
        }
    }

    // RPC chip tooltips depend on the active provider — re-apply.
    update_rpc_indicator();
}

} // namespace fincept::screens
