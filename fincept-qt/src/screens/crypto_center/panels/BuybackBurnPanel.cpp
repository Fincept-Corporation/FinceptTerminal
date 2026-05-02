#include "screens/crypto_center/panels/BuybackBurnPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens::panels {

namespace {

constexpr const char* kTopicEpoch = "treasury:buyback_epoch";
constexpr const char* kTopicBurnTotal = "treasury:burn_total";

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_usd(double v) {
    if (v <= 0.0) return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 0));
}

QString format_usd_precise(double v) {
    if (v <= 0.0) return QStringLiteral("$0.00");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 2));
}

QString format_usd_micro(double v) {
    // Per-unit price for $FNCPT — needs many decimals.
    if (v <= 0.0) return QStringLiteral("—");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 7));
}

/// Convert an integer-string atomic amount into a human-friendly count
/// like "82.4M" / "1.42B" given the token decimals.
QString format_token_compact(const QString& raw, int decimals) {
    if (raw.isEmpty()) return QStringLiteral("—");
    bool ok = false;
    const auto units = raw.toULongLong(&ok);
    if (!ok) return QStringLiteral("—");
    const double ui = static_cast<double>(units) /
                      std::pow(10.0, std::max(0, decimals));
    if (ui <= 0.0) return QStringLiteral("0");
    if (ui >= 1e9) return QStringLiteral("%1 B")
        .arg(QLocale::system().toString(ui / 1e9, 'f', 2));
    if (ui >= 1e6) return QStringLiteral("%1 M")
        .arg(QLocale::system().toString(ui / 1e6, 'f', 1));
    if (ui >= 1e3) return QStringLiteral("%1 K")
        .arg(QLocale::system().toString(ui / 1e3, 'f', 1));
    return QLocale::system().toString(ui, 'f', 0);
}

QString format_window(qint64 start_ms, qint64 end_ms) {
    if (start_ms <= 0 || end_ms <= 0) return QStringLiteral("—");
    const auto fmt = QStringLiteral("yyyy-MM-dd");
    return QStringLiteral("%1 → %2")
        .arg(QDateTime::fromMSecsSinceEpoch(start_ms).toString(fmt))
        .arg(QDateTime::fromMSecsSinceEpoch(end_ms).toString(fmt));
}

QString truncate_signature(const QString& sig) {
    if (sig.size() <= 14) return sig;
    return sig.left(8) + QStringLiteral("…") + sig.right(4);
}

} // namespace

BuybackBurnPanel::BuybackBurnPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("buybackBurnPanel"));
    build_ui();
    apply_theme();

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &BuybackBurnPanel::on_topic_error);
}

BuybackBurnPanel::~BuybackBurnPanel() = default;

void BuybackBurnPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("buybackBurnHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    title_ = new QLabel(QStringLiteral("BUYBACK & BURN"), head);
    title_->setObjectName(QStringLiteral("buybackBurnTitle"));
    epoch_window_ = new QLabel(QStringLiteral("epoch — · — → —"), head);
    epoch_window_->setObjectName(QStringLiteral("buybackBurnHeadCaption"));
    status_pill_ = new QLabel(QStringLiteral("LIVE"), head);
    status_pill_->setObjectName(QStringLiteral("buybackBurnPill"));
    hl->addWidget(title_);
    hl->addWidget(epoch_window_);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("buybackBurnBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(12);

    auto add_kv = [&](QVBoxLayout* parent, const QString& cap, QLabel*& v_out,
                      const QString& obj_name = {}) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* k = new QLabel(cap, body);
        k->setObjectName(QStringLiteral("buybackBurnCaption"));
        v_out = new QLabel(QStringLiteral("—"), body);
        v_out->setObjectName(obj_name.isEmpty()
            ? QStringLiteral("buybackBurnValue") : obj_name);
        row->addWidget(k);
        row->addStretch(1);
        row->addWidget(v_out);
        parent->addLayout(row);
    };

    // THIS EPOCH section
    {
        auto* section_title = new QLabel(QStringLiteral("THIS EPOCH"), body);
        section_title->setObjectName(QStringLiteral("buybackBurnSectionTitle"));
        bl->addWidget(section_title);

        add_kv(bl, QStringLiteral("REVENUE"), revenue_total_,
               QStringLiteral("buybackBurnValueAmber"));
        revenue_split_ = new QLabel(QString(), body);
        revenue_split_->setObjectName(QStringLiteral("buybackBurnSubMeta"));
        bl->addWidget(revenue_split_);

        // The 50/25/25 split (plan §5.4) — show all three so the value loop
        // is visible end-to-end. The numbers come from the worker; the
        // panel just renders.
        add_kv(bl, QStringLiteral("BUYBACK (50%)"), buyback_usd_);
        add_kv(bl, QStringLiteral("STAKER YIELD (25%)"), staker_yield_usd_);
        add_kv(bl, QStringLiteral("TREASURY TOPUP (25%)"), treasury_topup_usd_);
        add_kv(bl, QStringLiteral("$FNCPT BOUGHT"), fncpt_bought_);
        add_kv(bl, QStringLiteral("$FNCPT BURNED"), fncpt_burned_,
               QStringLiteral("buybackBurnValueAmber"));

        // Burn signature row — clickable. Use a flat QPushButton so the
        // click is a real button activation (keyboard-accessible), not a
        // mouse-only event filter on a QLabel.
        auto* sig_row = new QHBoxLayout;
        sig_row->setSpacing(8);
        auto* sig_cap = new QLabel(QStringLiteral("BURN TX"), body);
        sig_cap->setObjectName(QStringLiteral("buybackBurnCaption"));
        burn_signature_link_ = new QPushButton(QStringLiteral("—"), body);
        burn_signature_link_->setObjectName(QStringLiteral("buybackBurnSig"));
        burn_signature_link_->setCursor(Qt::PointingHandCursor);
        burn_signature_link_->setFlat(true);
        burn_signature_link_->setToolTip(tr("Open burn transaction in Solscan"));
        connect(burn_signature_link_, &QPushButton::clicked, this,
                &BuybackBurnPanel::on_burn_signature_clicked);
        sig_row->addWidget(sig_cap);
        sig_row->addStretch(1);
        sig_row->addWidget(burn_signature_link_);
        bl->addLayout(sig_row);
    }

    // Separator
    auto* sep = new QFrame(body);
    sep->setObjectName(QStringLiteral("buybackBurnSeparator"));
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    bl->addWidget(sep);

    // ALL-TIME section
    {
        auto* section_title = new QLabel(QStringLiteral("ALL-TIME"), body);
        section_title->setObjectName(QStringLiteral("buybackBurnSectionTitle"));
        bl->addWidget(section_title);

        add_kv(bl, QStringLiteral("BURNED"), total_burned_,
               QStringLiteral("buybackBurnValueAmber"));
        add_kv(bl, QStringLiteral("SUPPLY REMAINING"), supply_remaining_);
        add_kv(bl, QStringLiteral("SPENT ON BUYBACK"), spent_on_buyback_);
    }

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("buybackBurnErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("buybackBurnErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("buybackBurnErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    bl->addStretch(1);
    root->addWidget(body, 1);
}

void BuybackBurnPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#buybackBurnPanel { background:%1; }"
        "QWidget#buybackBurnHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#buybackBurnTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#buybackBurnHeadCaption { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:600; letter-spacing:0.8px; background:transparent; }"
        "QLabel#buybackBurnPill { color:%7; background:%8; border:1px solid %3;"
        "  font-family:%5; font-size:9px; font-weight:700; letter-spacing:1.2px;"
        "  padding:2px 8px; }"
        "QLabel#buybackBurnPillDemo { color:%4; background:rgba(217,119,6,0.10);"
        "  border:1px solid %12; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 8px; }"

        "QWidget#buybackBurnBody { background:%1; }"
        "QLabel#buybackBurnSectionTitle { color:%4; font-family:%5; font-size:10px;"
        "  font-weight:700; letter-spacing:1.5px; background:transparent;"
        "  padding:0; margin-top:4px; }"
        "QLabel#buybackBurnCaption { color:%6; font-family:%5; font-size:9px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#buybackBurnValue { color:%7; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#buybackBurnValueAmber { color:%4; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#buybackBurnSubMeta { color:%6; font-family:%5; font-size:10px;"
        "  background:transparent; }"
        "QPushButton#buybackBurnSig { color:%4; font-family:%5; font-size:11px;"
        "  text-decoration:underline; background:transparent; border:none;"
        "  padding:0; text-align:right; }"
        "QPushButton#buybackBurnSig:hover { color:%11; }"

        "QFrame#buybackBurnSeparator { color:%3; background:%3; }"

        "QFrame#buybackBurnErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %9; }"
        "QLabel#buybackBurnErrorIcon { color:%9; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#buybackBurnErrorText { color:%9; font-family:%5; font-size:11px;"
        "  background:transparent; }"
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
        .arg(BG_HOVER(),                       // %10 (unused but reserved)
             BORDER_BRIGHT(),                  // %11
             QStringLiteral("#78350f"));       // %12 darker amber

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void BuybackBurnPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kTopicEpoch),
                  [this](const QVariant& v) { on_epoch_update(v); });
    hub.subscribe(this, QString::fromLatin1(kTopicBurnTotal),
                  [this](const QVariant& v) { on_burn_total_update(v); });
    hub.request(QString::fromLatin1(kTopicEpoch), /*force=*/false);
    hub.request(QString::fromLatin1(kTopicBurnTotal), /*force=*/false);
}

void BuybackBurnPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

// ── Updates ────────────────────────────────────────────────────────────────

void BuybackBurnPanel::on_epoch_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::BuybackEpoch>()) return;
    const auto e = v.value<fincept::wallet::BuybackEpoch>();
    epoch_is_mock_ = e.is_mock;
    update_demo_chip(epoch_is_mock_ || burn_total_is_mock_);

    epoch_window_->setText(QStringLiteral("epoch %1 · %2")
        .arg(e.epoch_no)
        .arg(format_window(e.start_ts_ms, e.end_ts_ms)));

    revenue_total_->setText(format_usd(e.revenue_total_usd));
    revenue_split_->setText(tr("subs %1 · pred-mkt %2 · misc %3")
        .arg(format_usd(e.revenue_subs_usd))
        .arg(format_usd(e.revenue_predmkt_usd))
        .arg(format_usd(e.revenue_misc_usd)));

    auto pct_of_revenue = [&](double v) {
        return (e.revenue_total_usd > 0.0)
            ? (100.0 * v / e.revenue_total_usd) : 0.0;
    };
    auto fmt_pct = [&](double v) {
        return QStringLiteral("%1 (%2%)")
            .arg(format_usd(v))
            .arg(QLocale::system().toString(pct_of_revenue(v), 'f', 0));
    };
    buyback_usd_->setText(fmt_pct(e.buyback_usd));
    staker_yield_usd_->setText(fmt_pct(e.staker_yield_usd));
    treasury_topup_usd_->setText(fmt_pct(e.treasury_topup_usd));

    fncpt_bought_->setText(QStringLiteral("%1 $FNCPT  (avg %2)")
        .arg(format_token_compact(e.fncpt_bought_raw, e.fncpt_decimals))
        .arg(format_usd_micro(e.avg_buy_price_usd)));
    fncpt_burned_->setText(QStringLiteral("%1 $FNCPT")
        .arg(format_token_compact(e.fncpt_burned_raw, e.fncpt_decimals)));

    current_burn_signature_ = e.burn_signature;
    burn_signature_link_->setText(e.burn_signature.isEmpty()
        ? QStringLiteral("—")
        : truncate_signature(e.burn_signature));

    clear_error_strip();
}

void BuybackBurnPanel::on_burn_total_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::BurnTotal>()) return;
    const auto t = v.value<fincept::wallet::BurnTotal>();
    burn_total_is_mock_ = t.is_mock;
    update_demo_chip(epoch_is_mock_ || burn_total_is_mock_);

    total_burned_->setText(QStringLiteral("%1 $FNCPT")
        .arg(format_token_compact(t.total_burned_raw, t.decimals)));
    supply_remaining_->setText(QStringLiteral("%1 $FNCPT")
        .arg(format_token_compact(t.supply_remaining_raw, t.decimals)));
    spent_on_buyback_->setText(format_usd_precise(t.spent_on_buyback_usd));
}

void BuybackBurnPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != QLatin1String(kTopicEpoch)
        && topic != QLatin1String(kTopicBurnTotal)) {
        return;
    }
    show_error_strip(tr("Treasury feed error: %1").arg(error));
}

void BuybackBurnPanel::on_burn_signature_clicked() {
    if (current_burn_signature_.isEmpty()) return;
    if (current_burn_signature_.startsWith(QStringLiteral("FinceptTreasury"))
        || current_burn_signature_.contains(QStringLiteral("Mock"))) {
        // Mock signature — Solscan would 404. Skip the navigation and
        // give a tooltip cue.
        burn_signature_link_->setToolTip(
            tr("Demo signature — connect a treasury endpoint for a real burn tx."));
        return;
    }
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://solscan.io/tx/") +
                                   current_burn_signature_));
}

void BuybackBurnPanel::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
    error_text_->setText(msg);
    error_strip_->show();
}

void BuybackBurnPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void BuybackBurnPanel::update_demo_chip(bool any_mock) {
    if (any_mock) {
        status_pill_->setText(QStringLiteral("DEMO"));
        status_pill_->setObjectName(QStringLiteral("buybackBurnPillDemo"));
    } else {
        status_pill_->setText(QStringLiteral("LIVE"));
        status_pill_->setObjectName(QStringLiteral("buybackBurnPill"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
