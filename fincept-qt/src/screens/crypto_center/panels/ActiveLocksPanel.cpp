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

QString format_token(double v, int dp = 2) {
    if (v <= 0.0)
        return QStringLiteral("0");
    return QLocale::system().toString(v, 'f', dp);
}

QString format_usdc(double v) {
    if (v <= 0.0)
        return QStringLiteral("$0");
    return QStringLiteral("$%1").arg(QLocale::system().toString(v, 'f', 0));
}

QString format_unlock_date(qint64 ts_ms) {
    if (ts_ms <= 0)
        return QStringLiteral("—");
    return QDateTime::fromMSecsSinceEpoch(ts_ms).toString(QStringLiteral("yyyy-MM-dd"));
}

QString format_duration(qint64 secs) {
    constexpr qint64 kMonth = 30LL * 24 * 60 * 60;
    constexpr qint64 kYear = 365LL * 24 * 60 * 60;
    if (secs >= kYear * 4)
        return QStringLiteral("4 yr");
    if (secs >= kYear * 2)
        return QStringLiteral("2 yr");
    if (secs >= kYear)
        return QStringLiteral("1 yr");
    if (secs >= kMonth * 6)
        return QStringLiteral("6 mo");
    if (secs >= kMonth * 3)
        return QStringLiteral("3 mo");
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
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &ActiveLocksPanel::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &ActiveLocksPanel::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this, &ActiveLocksPanel::on_topic_error);

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
    title_ = new QLabel(tr("ACTIVE LOCKS"), head);
    title_->setObjectName(QStringLiteral("activeLocksTitle"));
    summary_label_ = new QLabel(tr("0 positions · 0 veFNCPT"), head);
    summary_label_->setObjectName(QStringLiteral("activeLocksHeadCaption"));
    status_pill_ = new QLabel(tr("LIVE"), head);
    status_pill_->setObjectName(QStringLiteral("activeLocksPill"));
    hl->addWidget(title_);
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
        tr("LOCKED"),
        tr("DURATION"),
        tr("UNLOCKS"),
        tr("WEIGHT"),
        tr("YIELD (LIFETIME)"),
    });
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &ActiveLocksPanel::on_row_context_menu);
    bl->addWidget(table_, 1);

    empty_state_ = new QLabel(QStringLiteral("—"), body); // text set by rebuild_table()
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

    const QString ss =
        QStringLiteral("QWidget#activeLocksPanel { background:%1; }"
                       "QWidget#activeLocksHead { background:%2; border-bottom:1px solid %3; }"
                       "QLabel#activeLocksTitle { color:%4; font-family:%5; font-size:11px;"
                       "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
                       "QLabel#activeLocksHeadCaption { color:%6; font-family:%5; font-size:10px;"
                       "  font-weight:600; letter-spacing:0.8px; background:transparent; }"
                       "QLabel#activeLocksPill { color:%7; background:%8; border:1px solid %3;"
                       "  font-family:%5; font-size:9px; font-weight:700; letter-spacing:1.2px;"
                       "  padding:2px 8px; }"
                       "QLabel#activeLocksPillDemo { color:%4; background:rgba(217,119,6,0.10);"
                       "  border:1px solid %11; font-family:%5; font-size:9px; font-weight:700;"
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
                       "  background:transparent; }")
            // One arg per marker, in ascending marker order. QString::arg fills
            // the LOWEST marker present, so a gap in the numbering silently
            // shifts every later colour by one and warns "Argument missing" for
            // the overflow.
            .arg(BG_BASE(),                  // %1
                 BG_SURFACE(),               // %2
                 BORDER_DIM(),               // %3
                 AMBER(),                    // %4
                 font,                       // %5
                 TEXT_TERTIARY(),            // %6
                 TEXT_PRIMARY(),             // %7
                 BG_RAISED(),                // %8
                 NEGATIVE())                 // %9
            .arg(BG_HOVER(),                 // %10
                 QStringLiteral("#78350f")); // %11 darker amber
    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void ActiveLocksPanel::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    // Nothing to read while staking is not live: no subscription at all.
    if (!fincept::wallet::StakingService::instance().staking_live()) {
        rebuild_table();
        return;
    }
    if (isVisible() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("wallet:locks:%1").arg(pubkey);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_, [this](const QVariant& v) { on_locks_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void ActiveLocksPanel::on_wallet_disconnected() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    latest_.clear();
    // A previous wallet's feed error must not carry over to the next one.
    feed_error_ = false;
    clear_error_strip();
    rebuild_table();
}

void ActiveLocksPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!fincept::wallet::StakingService::instance().staking_live())
        return;
    if (!current_pubkey_.isEmpty() && current_topic_.isEmpty()) {
        current_topic_ = QStringLiteral("wallet:locks:%1").arg(current_pubkey_);
        auto& hub = fincept::datahub::DataHub::instance();
        hub.subscribe(this, current_topic_, [this](const QVariant& v) { on_locks_update(v); });
        hub.request(current_topic_, /*force=*/false);
    }
}

void ActiveLocksPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

void ActiveLocksPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ActiveLocksPanel::retranslateUi() {
    if (title_)
        title_->setText(tr("ACTIVE LOCKS"));
    if (table_) {
        table_->setHorizontalHeaderLabels({
            tr("LOCKED"),
            tr("DURATION"),
            tr("UNLOCKS"),
            tr("WEIGHT"),
            tr("YIELD (LIFETIME)"),
        });
    }
    // Re-render empty state, summary, TOTAL row and the pill in the new locale.
    rebuild_table();
}

// ── Updates ────────────────────────────────────────────────────────────────

void ActiveLocksPanel::on_locks_update(const QVariant& v) {
    if (!v.canConvert<QVector<fincept::wallet::LockPosition>>())
        return;
    latest_ = v.value<QVector<fincept::wallet::LockPosition>>();
    feed_error_ = false;
    rebuild_table();
}

void ActiveLocksPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != current_topic_)
        return;
    show_error_strip(tr("Locks feed error: %1").arg(error));
    // Without cached positions the panel would otherwise read "LIVE · No active
    // locks" next to an error — a claim nothing established. Say "unavailable".
    if (latest_.isEmpty()) {
        feed_error_ = true;
        rebuild_table();
    }
}

void ActiveLocksPanel::rebuild_table() {
    if (!fincept::wallet::StakingService::instance().staking_live()) {
        // Not live: there are no positions to read, and none were ever shown
        // as this wallet's. Say what the user can expect instead.
        latest_.clear();
        table_->setRowCount(0);
        table_->hide();
        empty_state_->setText(tr("No lock positions — on-chain staking is not live yet.\n"
                                 "Your locks will appear here once the fincept_lock program launches."));
        empty_state_->show();
        summary_label_->setText(QStringLiteral("—"));
        set_pill(tr("COMING SOON"), true);
        return;
    }
    if (latest_.isEmpty()) {
        table_->setRowCount(0);
        table_->hide();
        empty_state_->setText(feed_error_ ? tr("Locks unavailable — the locks feed returned an error.")
                                          : tr("No active locks."));
        empty_state_->show();
        summary_label_->setText(tr("0 positions · 0 veFNCPT"));
        // Without cached positions an error must not sit next to a LIVE pill.
        set_pill(feed_error_ ? tr("UNAVAILABLE") : tr("LIVE"), feed_error_);
        return;
    }
    empty_state_->hide();
    table_->show();

    double total_weight_ui = 0.0;
    double total_yield_ui = 0.0;

    table_->setRowCount(latest_.size() + 1); // +1 for the TOTAL row
    for (int i = 0; i < latest_.size(); ++i) {
        const auto& p = latest_[i];
        const double amount_ui = atomic_to_ui(p.amount_raw, p.decimals);
        const double weight_ui = atomic_to_ui(p.weight_raw, p.decimals);
        total_weight_ui += weight_ui;
        total_yield_ui += p.lifetime_yield_usdc;

        auto* locked = new QTableWidgetItem(QStringLiteral("%1 $FNCPT").arg(format_token(amount_ui, 0)));
        locked->setData(Qt::UserRole, p.position_id);
        table_->setItem(i, 0, locked);
        table_->setItem(i, 1, new QTableWidgetItem(format_duration(p.duration_secs)));
        table_->setItem(i, 2, new QTableWidgetItem(format_unlock_date(p.unlock_ts)));
        table_->setItem(i, 3, new QTableWidgetItem(format_token(weight_ui, 1)));
        table_->setItem(i, 4, new QTableWidgetItem(format_usdc(p.lifetime_yield_usdc)));
    }

    // TOTAL row — visually distinct via item-level styling.
    const int total_row = latest_.size();
    auto* tot_label = new QTableWidgetItem(tr("TOTAL"));
    tot_label->setData(Qt::UserRole, QString()); // no position id
    table_->setItem(total_row, 0, tot_label);
    table_->setItem(total_row, 1, new QTableWidgetItem(QStringLiteral("—")));
    table_->setItem(total_row, 2, new QTableWidgetItem(QStringLiteral("—")));
    table_->setItem(total_row, 3, new QTableWidgetItem(format_token(total_weight_ui, 1)));
    table_->setItem(total_row, 4, new QTableWidgetItem(format_usdc(total_yield_ui)));

    // Plural form is a translator's decision, not an English "s" suffix.
    summary_label_->setText(
        tr("%n position(s) · %1 veFNCPT", "", latest_.size()).arg(format_token(total_weight_ui, 1)));
    set_pill(tr("LIVE"), false);
    clear_error_strip();
}

void ActiveLocksPanel::on_row_context_menu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item)
        return;
    const int row = item->row();
    if (row < 0 || row >= latest_.size())
        return; // ignore TOTAL row
    const auto& p = latest_[row];

    // Both actions are shown so the user can see what will exist, but neither
    // transaction is implemented — they stay disabled with an honest reason.
    // (The menu used to point at a "Settings > Lock program ID" field that
    // does not exist, and configuring the key would not have enabled them.)
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    auto* extend = menu.addAction(tr("Extend lock… (coming soon)"));
    auto* withdraw = menu.addAction(tr("Withdraw (coming soon)"));
    extend->setEnabled(false);
    withdraw->setEnabled(false);
    extend->setToolTip(tr("Extending a lock is not implemented in this build."));
    withdraw->setToolTip(position_is_expired(p)
                             ? tr("Withdrawing is not implemented in this build.")
                             : tr("Unlocks %1. Withdrawing is not implemented in this build.")
                                   .arg(format_unlock_date(p.unlock_ts)));
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void ActiveLocksPanel::show_error_strip(const QString& msg) {
    if (!error_strip_)
        return;
    error_text_->setText(msg);
    error_strip_->show();
}

void ActiveLocksPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void ActiveLocksPanel::set_pill(const QString& text, bool highlighted) {
    status_pill_->setText(text);
    // The amber variant marks every not-live / not-healthy state.
    status_pill_->setObjectName(highlighted ? QStringLiteral("activeLocksPillDemo")
                                            : QStringLiteral("activeLocksPill"));
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
