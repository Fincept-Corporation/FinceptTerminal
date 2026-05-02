#include "screens/crypto_center/panels/TreasuryPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

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

constexpr const char* kTopicReserves = "treasury:reserves";
constexpr const char* kTopicRunway   = "treasury:runway";

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_usd(double v, int dp = 0) {
    if (v <= 0.0) return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', dp));
}

QString format_usd_compact(double v) {
    if (v <= 0.0) return QStringLiteral("$0");
    if (v >= 1e9) return QStringLiteral("$%1B")
        .arg(QLocale::system().toString(v / 1e9, 'f', 2));
    if (v >= 1e6) return QStringLiteral("$%1M")
        .arg(QLocale::system().toString(v / 1e6, 'f', 2));
    if (v >= 1e3) return QStringLiteral("$%1k")
        .arg(QLocale::system().toString(v / 1e3, 'f', 1));
    return format_usd(v, 0);
}

QString format_sol(quint64 lamports) {
    const double sol = static_cast<double>(lamports) / 1e9;
    return QLocale::system().toString(sol, 'f', 2);
}

} // namespace

TreasuryPanel::TreasuryPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("treasuryPanel"));
    build_ui();
    apply_theme();

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &TreasuryPanel::on_topic_error);
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
    auto* title = new QLabel(QStringLiteral("TREASURY"), head);
    title->setObjectName(QStringLiteral("treasuryTitle"));
    status_pill_ = new QLabel(QStringLiteral("LIVE"), head);
    status_pill_->setObjectName(QStringLiteral("treasuryPill"));
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("treasuryBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(10);

    auto add_kv = [&](const QString& cap, QLabel*& v_out,
                      const QString& obj_name = {}) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* k = new QLabel(cap, body);
        k->setObjectName(QStringLiteral("treasuryCaption"));
        v_out = new QLabel(QStringLiteral("—"), body);
        v_out->setObjectName(obj_name.isEmpty()
            ? QStringLiteral("treasuryValue") : obj_name);
        row->addWidget(k);
        row->addStretch(1);
        row->addWidget(v_out);
        bl->addLayout(row);
    };

    add_kv(QStringLiteral("USDC RESERVES"), usdc_value_);
    add_kv(QStringLiteral("SOL RESERVES"), sol_value_);
    add_kv(QStringLiteral("TOTAL USD"), total_usd_value_,
           QStringLiteral("treasuryValueAmber"));
    add_kv(QStringLiteral("RUNWAY @ CURRENT"), runway_value_);

    // Multisig row — clickable button styled as a link.
    auto* ms_row = new QHBoxLayout;
    ms_row->setSpacing(8);
    auto* ms_cap = new QLabel(QStringLiteral("MULTI-SIG"), body);
    ms_cap->setObjectName(QStringLiteral("treasuryCaption"));
    multisig_button_ = new QPushButton(QStringLiteral("—"), body);
    multisig_button_->setObjectName(QStringLiteral("treasuryMultisig"));
    multisig_button_->setCursor(Qt::PointingHandCursor);
    multisig_button_->setFlat(true);
    multisig_button_->setToolTip(tr("Open Squads vault in browser"));
    connect(multisig_button_, &QPushButton::clicked, this,
            &TreasuryPanel::on_multisig_clicked);
    ms_row->addWidget(ms_cap);
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

    const QString ss = QStringLiteral(
        "QWidget#treasuryPanel { background:%1; }"
        "QWidget#treasuryHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#treasuryTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#treasuryPill { color:%7; background:%8; border:1px solid %3;"
        "  font-family:%5; font-size:9px; font-weight:700; letter-spacing:1.2px;"
        "  padding:2px 8px; }"
        "QLabel#treasuryPillDemo { color:%4; background:rgba(217,119,6,0.10);"
        "  border:1px solid %12; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 8px; }"
        "QWidget#treasuryBody { background:%1; }"
        "QLabel#treasuryCaption { color:%6; font-family:%5; font-size:9px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#treasuryValue { color:%7; font-family:%5; font-size:12px;"
        "  background:transparent; }"
        "QLabel#treasuryValueAmber { color:%4; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"

        "QPushButton#treasuryMultisig { color:%4; font-family:%5; font-size:11px;"
        "  text-decoration:underline; background:transparent; border:none;"
        "  padding:0; text-align:right; }"
        "QPushButton#treasuryMultisig:hover { color:%11; }"

        "QFrame#treasuryErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %9; }"
        "QLabel#treasuryErrorIcon { color:%9; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#treasuryErrorText { color:%9; font-family:%5; font-size:11px;"
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
        .arg(BG_HOVER(),                       // %10
             BORDER_BRIGHT(),                  // %11
             QStringLiteral("#78350f"));       // %12 darker amber

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void TreasuryPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kTopicReserves),
                  [this](const QVariant& v) { on_reserves_update(v); });
    hub.subscribe(this, QString::fromLatin1(kTopicRunway),
                  [this](const QVariant& v) { on_runway_update(v); });
    hub.request(QString::fromLatin1(kTopicReserves), /*force=*/false);
    hub.request(QString::fromLatin1(kTopicRunway), /*force=*/false);
}

void TreasuryPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

// ── Updates ────────────────────────────────────────────────────────────────

void TreasuryPanel::on_reserves_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryReserves>()) return;
    const auto r = v.value<fincept::wallet::TreasuryReserves>();
    reserves_is_mock_ = r.is_mock;
    update_demo_chip();

    usdc_value_->setText(format_usd_compact(r.usdc_amount));
    sol_value_->setText(QStringLiteral("%1 SOL  (%2)")
        .arg(format_sol(r.sol_lamports))
        .arg(format_usd_compact(static_cast<double>(r.sol_lamports) / 1e9 *
                                 r.sol_usd_price)));
    total_usd_value_->setText(format_usd_compact(r.total_usd));

    multisig_url_ = r.multisig_url;
    multisig_button_->setText(r.multisig_label.isEmpty()
        ? QStringLiteral("—") : r.multisig_label);
    clear_error_strip();
}

void TreasuryPanel::on_runway_update(const QVariant& v) {
    if (!v.canConvert<fincept::wallet::TreasuryRunway>()) return;
    const auto w = v.value<fincept::wallet::TreasuryRunway>();
    runway_is_mock_ = w.is_mock;
    update_demo_chip();

    if (w.months <= 0.0) {
        runway_value_->setText(QStringLiteral("—"));
        return;
    }
    runway_value_->setText(QStringLiteral("%1 months  ($%2/mo)")
        .arg(QLocale::system().toString(w.months, 'f', 0))
        .arg(QLocale::system().toString(w.monthly_opex_usd, 'f', 0)));
}

void TreasuryPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != QLatin1String(kTopicReserves)
        && topic != QLatin1String(kTopicRunway)) {
        return;
    }
    show_error_strip(tr("Treasury feed error: %1").arg(error));
}

void TreasuryPanel::on_multisig_clicked() {
    if (multisig_url_.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(multisig_url_));
}

void TreasuryPanel::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
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
    if (any_mock) {
        status_pill_->setText(QStringLiteral("DEMO"));
        status_pill_->setObjectName(QStringLiteral("treasuryPillDemo"));
    } else {
        status_pill_->setText(QStringLiteral("LIVE"));
        status_pill_->setObjectName(QStringLiteral("treasuryPill"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
