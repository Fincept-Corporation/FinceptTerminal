#include "screens/crypto_center/panels/TreasuryPanel.h"
#include "screens/crypto_center/CryptoCenterText.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletTypes.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace cc_fmt = fincept::ui::formatting;

namespace fincept::screens::panels {

namespace {

constexpr const char* kTopicReserves = "treasury:reserves";
constexpr const char* kTopicRunway = "treasury:runway";

using fincept::screens::crypto_center_text::font_stack; // CryptoCenterText.h (one definition)

// The treasury endpoint publishes Fincept's reserves, total and opex in USD:
// shown in USD as published, never converted (USDC reserves keep their own code).
QString format_usd_compact(double v) {
    return cc_fmt::money(v > 0.0 ? v : 0.0, QStringLiteral("USD"), cc_fmt::Compact::FromSettings);
}

/// A SOL amount: a quantity of the token, never money.
QString format_sol(quint64 lamports) {
    const double sol = static_cast<double>(lamports) / 1e9;
    return cc_fmt::number(sol, 2);
}

} // namespace

TreasuryPanel::TreasuryPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("treasuryPanel"));
    build_ui();
    apply_theme();

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this, &TreasuryPanel::on_topic_error);
    // A number-format / currency change re-renders the figures on screen from the hub's last
    // values (no new request).
    cc_fmt::on_format_changed(this, [this]() {
        if (!has_data_)
            return;
        auto& h = fincept::datahub::DataHub::instance();
        if (const QVariant v = h.peek_raw(QString::fromLatin1(kTopicReserves)); v.isValid())
            on_reserves_update(v);
        if (const QVariant v = h.peek_raw(QString::fromLatin1(kTopicRunway)); v.isValid())
            on_runway_update(v);
    });
}

TreasuryPanel::~TreasuryPanel() = default;

void TreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("treasuryHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    title_ = new QLabel(tr("TREASURY"), head);
    title_->setObjectName(QStringLiteral("treasuryTitle"));
    status_pill_ = new QLabel(tr("LOADING"), head); // LIVE/DEMO once a value arrives
    status_pill_->setObjectName(QStringLiteral("treasuryPill"));
    hl->addWidget(title_);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("treasuryBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    auto add_kv = [&](const QString& cap, QLabel*& cap_out, QLabel*& v_out, const QString& obj_name = {}) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        cap_out = new QLabel(cap, body);
        cap_out->setObjectName(QStringLiteral("treasuryCaption"));
        v_out = new QLabel(QStringLiteral("—"), body);
        v_out->setObjectName(obj_name.isEmpty() ? QStringLiteral("treasuryValue") : obj_name);
        row->addWidget(cap_out);
        row->addStretch(1);
        row->addWidget(v_out);
        bl->addLayout(row);
    };

    add_kv(tr("USDC RESERVES"), usdc_caption_, usdc_value_);
    add_kv(tr("SOL RESERVES"), sol_caption_, sol_value_);
    add_kv(tr("TOTAL USD"), total_caption_, total_usd_value_, QStringLiteral("treasuryValueAmber"));
    add_kv(tr("RUNWAY @ CURRENT"), runway_caption_, runway_value_);

    // Multisig row — clickable button styled as a link.
    auto* ms_row = new QHBoxLayout;
    ms_row->setSpacing(8);
    multisig_caption_ = new QLabel(tr("MULTI-SIG"), body);
    multisig_caption_->setObjectName(QStringLiteral("treasuryCaption"));
    multisig_button_ = new QPushButton(QStringLiteral("—"), body);
    multisig_button_->setObjectName(QStringLiteral("treasuryMultisig"));
    multisig_button_->setCursor(Qt::PointingHandCursor);
    multisig_button_->setFlat(true);
    multisig_button_->setToolTip(tr("Open Squads vault in browser"));
    connect(multisig_button_, &QPushButton::clicked, this, &TreasuryPanel::on_multisig_clicked);
    ms_row->addWidget(multisig_caption_);
    ms_row->addStretch(1);
    ms_row->addWidget(multisig_button_);
    bl->addLayout(ms_row);

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("treasuryErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("treasuryErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("treasuryErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    bl->addStretch(1);
    root->addWidget(body, 1);
}

void TreasuryPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QString("QWidget#treasuryPanel { background:%1; }"
                                      "QWidget#treasuryHead { background:%2; border-bottom:1px solid %3; }"
                                      "QLabel#treasuryTitle { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                      "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                                      "QLabel#treasuryPill { color:%7; background:%8; border:1px solid %3;"
                                      "  font-family:%5; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1.2px;"
                                      "  padding:2px 8px; }"
                                      "QLabel#treasuryPillDemo { color:%4; background:" + ui::colors::alpha(AMBER, 0.10) + ";"
                                      "  border:1px solid %11; font-family:%5; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
                                      "  letter-spacing:1.2px; padding:2px 8px; }"
                                      "QWidget#treasuryBody { background:%1; }"
                                      "QLabel#treasuryCaption { color:%6; font-family:%5; font-size:" + fincept::ui::fonts::px(-5) + "px;"
                                      "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
                                      "QLabel#treasuryValue { color:%7; font-family:%5; font-size:" + fincept::ui::fonts::px(-2) + "px;"
                                      "  background:transparent; }"
                                      "QLabel#treasuryValueAmber { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-1) + "px;"
                                      "  font-weight:700; background:transparent; }"

                                      "QPushButton#treasuryMultisig { color:%4; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                      "  text-decoration:underline; background:transparent; border:none;"
                                      "  padding:0; text-align:right; }"
                                      "QPushButton#treasuryMultisig:hover { color:%10; }"

                                      "QFrame#treasuryErrorStrip { background:" + ui::colors::alpha(DANGER, 0.10) + ";"
                                      "  border:1px solid %9; }"
                                      "QLabel#treasuryErrorIcon { color:%9; font-family:%5; font-size:" + fincept::ui::fonts::px(-1) + "px;"
                                      "  font-weight:700; background:transparent; }"
                                      "QLabel#treasuryErrorText { color:%9; font-family:%5; font-size:" + fincept::ui::fonts::px(-3) + "px;"
                                      "  background:transparent; }")
                           .arg(BG_BASE(),                  // %1
                                BG_SURFACE(),               // %2
                                BORDER_DIM(),               // %3
                                AMBER(),                    // %4
                                font,                       // %5
                                TEXT_TERTIARY(),            // %6
                                TEXT_PRIMARY(),             // %7
                                BG_RAISED(),                // %8
                                DANGER())                   // %9
                           // One arg per marker, in ascending marker order —
                           // QString::arg fills the LOWEST marker present, so a
                           // spare arg shifts every later colour by one and
                           // warns "Argument missing" for the overflow.
                           .arg(BORDER_BRIGHT(),            // %10
                                AMBER_DIM());               // %11 darker amber

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void TreasuryPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kTopicReserves), [this](const QVariant& v) { on_reserves_update(v); });
    hub.subscribe(this, QString::fromLatin1(kTopicRunway), [this](const QVariant& v) { on_runway_update(v); });
    hub.request(QString::fromLatin1(kTopicReserves), /*force=*/false);
    hub.request(QString::fromLatin1(kTopicRunway), /*force=*/false);
}

void TreasuryPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void TreasuryPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void TreasuryPanel::retranslateUi() {
    if (title_)
        title_->setText(tr("TREASURY"));
    if (usdc_caption_)
        usdc_caption_->setText(tr("USDC RESERVES"));
    if (sol_caption_)
        sol_caption_->setText(tr("SOL RESERVES"));
    if (total_caption_)
        total_caption_->setText(tr("TOTAL USD"));
    if (runway_caption_)
        runway_caption_->setText(tr("RUNWAY @ CURRENT"));
    if (multisig_caption_)
        multisig_caption_->setText(tr("MULTI-SIG"));
    if (multisig_button_)
        multisig_button_->setToolTip(tr("Open Squads vault in browser"));
    // Re-render the LIVE/DEMO pill in the new locale.
    update_demo_chip();
}

// ── Updates ────────────────────────────────────────────────────────────────

void TreasuryPanel::on_reserves_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryReserves>())
        return;
    const auto r = v.value<fincept::wallet::TreasuryReserves>();
    reserves_is_mock_ = r.is_mock;
    has_data_ = true;
    update_demo_chip();

    // USDC is a stablecoin — its own currency, not "$".
    usdc_value_->setText(
        cc_fmt::money(r.usdc_amount > 0.0 ? r.usdc_amount : 0.0, QStringLiteral("USDC"), cc_fmt::Compact::FromSettings));
    // sol_usd_price is 0 when no SOL quote has reached the hub yet. Rendering
    // that as "(0 USD)" and a total that silently omits the SOL leg understates
    // reserves with full confidence — flag it instead.
    const bool sol_unpriced = r.sol_lamports > 0 && r.sol_usd_price <= 0.0;
    if (sol_unpriced) {
        sol_value_->setText(tr("%1 SOL  (USD price n/a)").arg(format_sol(r.sol_lamports)));
        total_usd_value_->setText(tr("%1 + SOL (unpriced)").arg(format_usd_compact(r.total_usd)));
        total_usd_value_->setToolTip(tr("No live SOL price yet — the total excludes the SOL balance."));
    } else {
        sol_value_->setText(QStringLiteral("%1 SOL  (%2)")
                                .arg(format_sol(r.sol_lamports))
                                .arg(format_usd_compact(static_cast<double>(r.sol_lamports) / 1e9 * r.sol_usd_price)));
        total_usd_value_->setText(format_usd_compact(r.total_usd));
        total_usd_value_->setToolTip(QString());
    }

    multisig_url_ = r.multisig_url;
    multisig_button_->setText(r.multisig_label.isEmpty() ? QStringLiteral("—") : r.multisig_label);
    clear_error_strip();
}

void TreasuryPanel::on_runway_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryRunway>())
        return;
    const auto w = v.value<fincept::wallet::TreasuryRunway>();
    runway_is_mock_ = w.is_mock;
    has_data_ = true;
    update_demo_chip();

    if (w.months <= 0.0) {
        // No configured burn rate → runway is unknown, not a number we invent.
        runway_value_->setText(w.monthly_opex_usd <= 0.0 ? tr("— (monthly opex not configured)")
                                                         : QStringLiteral("—"));
        return;
    }
    runway_value_->setText(
        tr("%1 months  (%2/mo)")
            .arg(cc_fmt::number(w.months, 0),
                 cc_fmt::money(w.monthly_opex_usd, QStringLiteral("USD"), cc_fmt::Compact::None, 0)));
}

void TreasuryPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != QLatin1String(kTopicReserves) && topic != QLatin1String(kTopicRunway)) {
        return;
    }
    show_error_strip(tr("Treasury feed error: %1").arg(error));
    feed_failed_ = true;
    update_demo_chip(); // OFFLINE when nothing has arrived yet
}

void TreasuryPanel::on_multisig_clicked() {
    if (multisig_url_.isEmpty())
        return;
    // `multisig_url_` arrives from the treasury endpoint — untrusted remote
    // data driving QDesktopServices::openUrl. Without a scheme check a
    // compromised or misconfigured worker could hand us `file:///…` (opens a
    // local file/executable path in the shell) or a custom-protocol URL.
    // Restrict to https, which is the only thing a Squads deeplink needs.
    const QUrl url(multisig_url_);
    if (!url.isValid() || url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        show_error_strip(tr("Refusing to open a non-https treasury link: %1").arg(multisig_url_.left(120)));
        LOG_WARN("TreasuryPanel", "blocked non-https multisig url");
        return;
    }
    QDesktopServices::openUrl(url);
}

void TreasuryPanel::show_error_strip(const QString& msg) {
    if (!error_strip_)
        return;
    error_text_->setText(msg);
    error_strip_->show();
}

void TreasuryPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void TreasuryPanel::update_demo_chip() {
    const bool any_mock = reserves_is_mock_ || runway_is_mock_;
    if (!has_data_) {
        // Nothing received yet: a LIVE badge over a column of dashes would vouch
        // for data that does not exist.
        status_pill_->setText(feed_failed_ ? tr("OFFLINE") : tr("LOADING"));
        status_pill_->setObjectName(QStringLiteral("treasuryPill"));
        if (title_)
            title_->setText(tr("TREASURY"));
    } else if (any_mock) {
        status_pill_->setText(tr("DEMO"));
        status_pill_->setObjectName(QStringLiteral("treasuryPillDemo"));
        // A fabricated reserve figure presented like a real one is the single
        // most misleading thing this panel can do, so the title says so too —
        // the pill alone is easy to miss next to a bold amber TOTAL USD.
        if (title_)
            title_->setText(tr("TREASURY · SAMPLE DATA"));
    } else {
        status_pill_->setText(tr("LIVE"));
        status_pill_->setObjectName(QStringLiteral("treasuryPill"));
        if (title_)
            title_->setText(tr("TREASURY"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
