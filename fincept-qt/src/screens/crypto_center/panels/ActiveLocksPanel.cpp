#include "screens/crypto_center/panels/ActiveLocksPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/StakingService.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QShowEvent>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
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

QString format_token(double v, int dp = 2) {
    if (v <= 0.0) return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

QString format_usdc(double v) {
    if (v <= 0.0) return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 0));
}

QString format_unlock_date(qint64 ts_ms) {
    if (ts_ms <= 0) return QStringLiteral("—");
    return QDateTime::fromMSecsSinceEpoch(ts_ms).toString(QStringLiteral("yyyy-MM-dd"));
}

QString format_duration(qint64 secs) {
    constexpr qint64 kMonth = 30LL * 24 * 60 * 60;
    constexpr qint64 kYear  = 365LL * 24 * 60 * 60;
    if (secs >= kYear * 4)  return QStringLiteral("4 yr");
    if (secs >= kYear * 2)  return QStringLiteral("2 yr");
    if (secs >= kYear)      return QStringLiteral("1 yr");
    if (secs >= kMonth * 6) return QStringLiteral("6 mo");
    if (secs >= kMonth * 3) return QStringLiteral("3 mo");
    // Off-grid (custom duration via on-chain extend) — show approx months.
    return QStringLiteral("%1 mo").arg(secs / kMonth);
}

bool position_is_expired(const fincept::wallet::LockPosition& p) {
    return p.unlock_ts > 0 && p.unlock_ts <= QDateTime::currentMSecsSinceEpoch();
}

} // namespace

ActiveLocksPanel::ActiveLocksPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("activeLocksPanel"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &ActiveLocksPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &ActiveLocksPanel::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &ActiveLocksPanel::on_topic_error);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    } else {
        on_wallet_disconnected();
    }
}

ActiveLocksPanel::~ActiveLocksPanel() = default;

void ActiveLocksPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("activeLocksHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("ACTIVE LOCKS"), head);
    title->setObjectName(QStringLiteral("activeLocksTitle"));
    summary_label_ = new QLabel(QStringLiteral("0 positions · 0 veFNCPT"), head);
    summary_label_->setObjectName(QStringLiteral("activeLocksHeadCaption"));
    status_pill_ = new QLabel(QStringLiteral("LIVE"), head);
    status_pill_->setObjectName(QStringLiteral("activeLocksPill"));
    hl->addWidget(title);
    hl->addWidget(summary_label_);
    hl->addStretch();
    hl->addWidget(status_pill_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("activeLocksBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    table_ = new QTableWidget(body);
    table_->setObjectName(QStringLiteral("activeLocksTable"));
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("LOCKED"),
        QStringLiteral("DURATION"),
        QStringLiteral("UNLOCKS"),
        QStringLiteral("WEIGHT"),
        QStringLiteral("YIELD (LIFETIME)"),
    });
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this,
            &ActiveLocksPanel::on_row_context_menu);
    bl->addWidget(table_, 1);

    empty_state_ = new QLabel(
        tr("No active locks. Lock $FNCPT above to start earning yield."), body);
    empty_state_->setObjectName(QStringLiteral("activeLocksEmpty"));
    empty_state_->setAlignment(Qt::AlignCenter);
    empty_state_->setMinimumHeight(120);
    bl->addWidget(empty_state_);

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("activeLocksErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("activeLocksErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("activeLocksErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    root->addWidget(body, 1);
}

void ActiveLocksPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#activeLocksPanel { background:%1; }"
        "QWidget#activeLocksHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#activeLocksTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#activeLocksHeadCaption { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:600; letter-spacing:0.8px; background:transparent; }"
        "QLabel#activeLocksPill { color:%7; background:%8; border:1px solid %3;"
        "  font-family:%5; font-size:9px; font-weight:700; letter-spacing:1.2px;"
        "  padding:2px 8px; }"
        "QLabel#activeLocksPillDemo { color:%4; background:rgba(217,119,6,0.10);"
        "  border:1px solid %12; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 8px; }"
        "QWidget#activeLocksBody { background:%1; }"
        "QTableWidget#activeLocksTable { background:%1; color:%7; gridline-color:%3;"
        "  border:none; selection-background-color:%10; selection-color:%7;"
        "  font-family:%5; font-size:11px; }"
        "QTableWidget#activeLocksTable::item { padding:6px 10px; border-bottom:1px solid %3; }"
        "QHeaderView::section { background:%8; color:%6; padding:6px 10px;"
        "  border:none; border-bottom:1px solid %3; font-family:%5;"
        "  font-size:9px; font-weight:700; letter-spacing:1.4px; }"
        "QLabel#activeLocksEmpty { color:%6; font-family:%5; font-size:11px;"
        "  background:transparent; }"
        "QFrame#activeLocksErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %9; }"
        "QLabel#activeLocksErrorIcon { color:%9; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#activeLocksErrorText { color:%9; font-family:%5; font-size:11px;"
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

void ActiveLocksPanel::on_wallet_connected(const QString& pubkey,
                                            const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("wallet:locks:%1").arg(pubkey);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_,
                      [this](const QVariant& v) { on_locks_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void ActiveLocksPanel::on_wallet_disconnected() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    latest_.clear();
    rebuild_table();
}

void ActiveLocksPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("wallet:locks:%1").arg(current_pubkey_);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_,
                      [this](const QVariant& v) { on_locks_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void ActiveLocksPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

// ── Updates ────────────────────────────────────────────────────────────────

void ActiveLocksPanel::on_locks_update(const QVariant& v) {
    if (!v.canConvert<QVector<fincept::wallet::LockPosition>>()) return;
    latest_ = v.value<QVector<fincept::wallet::LockPosition>>();
    rebuild_table();
}

void ActiveLocksPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != current_topic_) return;
    show_error_strip(tr("Locks feed error: %1").arg(error));
}

void ActiveLocksPanel::rebuild_table() {
    if (latest_.isEmpty()) {
        table_->setRowCount(0);
        table_->hide();
        empty_state_->show();
        summary_label_->setText(QStringLiteral("0 positions · 0 veFNCPT"));
        update_demo_chip(false);
        return;
    }
    empty_state_->hide();
    table_->show();

    bool any_mock = false;
    double total_weight_ui = 0.0;
    double total_yield_ui = 0.0;

    table_->setRowCount(latest_.size() + 1); // +1 for the TOTAL row
    for (int i = 0; i < latest_.size(); ++i) {
        const auto& p = latest_[i];
        any_mock = any_mock || p.is_mock;
        const double amount_ui = atomic_to_ui(p.amount_raw, p.decimals);
        const double weight_ui = atomic_to_ui(p.weight_raw, p.decimals);
        total_weight_ui += weight_ui;
        total_yield_ui += p.lifetime_yield_usdc;

        auto* locked = new QTableWidgetItem(
            QStringLiteral("%1 $FNCPT").arg(format_token(amount_ui, 0)));
        locked->setData(Qt::UserRole, p.position_id);
        table_->setItem(i, 0, locked);
        table_->setItem(i, 1, new QTableWidgetItem(format_duration(p.duration_secs)));
        table_->setItem(i, 2, new QTableWidgetItem(format_unlock_date(p.unlock_ts)));
        table_->setItem(i, 3, new QTableWidgetItem(format_token(weight_ui, 1)));
        table_->setItem(i, 4, new QTableWidgetItem(format_usdc(p.lifetime_yield_usdc)));
    }

    // TOTAL row — visually distinct via item-level styling.
    const int total_row = latest_.size();
    auto* tot_label = new QTableWidgetItem(QStringLiteral("TOTAL"));
    tot_label->setData(Qt::UserRole, QString()); // no position id
    table_->setItem(total_row, 0, tot_label);
    table_->setItem(total_row, 1, new QTableWidgetItem(QStringLiteral("—")));
    table_->setItem(total_row, 2, new QTableWidgetItem(QStringLiteral("—")));
    table_->setItem(total_row, 3, new QTableWidgetItem(format_token(total_weight_ui, 1)));
    table_->setItem(total_row, 4, new QTableWidgetItem(format_usdc(total_yield_ui)));

    summary_label_->setText(QStringLiteral("%1 position%2 · %3 veFNCPT")
        .arg(latest_.size())
        .arg(latest_.size() == 1 ? QString() : QStringLiteral("s"))
        .arg(format_token(total_weight_ui, 1)));
    update_demo_chip(any_mock);
    clear_error_strip();
}

void ActiveLocksPanel::on_row_context_menu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item) return;
    const int row = item->row();
    if (row < 0 || row >= latest_.size()) return; // ignore TOTAL row
    const auto& p = latest_[row];

    QMenu menu(this);
    auto* extend = menu.addAction(tr("Extend lock…"));
    auto* withdraw = menu.addAction(tr("Withdraw"));
    withdraw->setEnabled(position_is_expired(p));
    if (!withdraw->isEnabled()) {
        withdraw->setToolTip(tr("Available after %1")
                                 .arg(format_unlock_date(p.unlock_ts)));
    }

    // Mock-mode disable + tooltip for both. Real-mode lock txs come in
    // when the Anchor program ships.
    if (p.is_mock || !fincept::wallet::StakingService::instance().program_is_configured()) {
        extend->setEnabled(false);
        extend->setToolTip(tr("fincept_lock not deployed — Settings > Lock program ID"));
        withdraw->setEnabled(false);
        withdraw->setToolTip(tr("fincept_lock not deployed — Settings > Lock program ID"));
    }

    QAction* chosen = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == extend) {
        show_error_strip(tr("Extend flow lands with the Anchor program."));
    } else if (chosen == withdraw) {
        show_error_strip(tr("Withdraw flow lands with the Anchor program."));
    }
}

void ActiveLocksPanel::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
    error_text_->setText(msg);
    error_strip_->show();
}

void ActiveLocksPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void ActiveLocksPanel::update_demo_chip(bool is_mock) {
    if (is_mock) {
        status_pill_->setText(QStringLiteral("DEMO"));
        status_pill_->setObjectName(QStringLiteral("activeLocksPillDemo"));
    } else {
        status_pill_->setText(QStringLiteral("LIVE"));
        status_pill_->setObjectName(QStringLiteral("activeLocksPill"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
