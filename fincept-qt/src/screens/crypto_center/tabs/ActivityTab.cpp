#include "screens/crypto_center/tabs/ActivityTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

QString format_time(qint64 ts_ms) {
    if (ts_ms <= 0) return QStringLiteral("—");
    return QDateTime::fromMSecsSinceEpoch(ts_ms)
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString kind_label(fincept::wallet::ParsedActivity::Kind k) {
    using K = fincept::wallet::ParsedActivity::Kind;
    switch (k) {
        case K::Swap:    return QStringLiteral("SWAP");
        case K::Receive: return QStringLiteral("RECEIVE");
        case K::Send:    return QStringLiteral("SEND");
        case K::Other:   return QStringLiteral("OTHER");
    }
    return QStringLiteral("OTHER");
}

QString shorten_sig(const QString& sig) {
    if (sig.size() < 14) return sig;
    return sig.left(8) + QStringLiteral("…") + sig.right(4);
}

} // namespace

ActivityTab::ActivityTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("activityTab"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this,
            &ActivityTab::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this,
            &ActivityTab::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &ActivityTab::on_topic_error);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    }
}

ActivityTab::~ActivityTab() = default;

void ActivityTab::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // Filter chips row
    auto* chips_row = new QHBoxLayout;
    chips_row->setSpacing(6);
    auto make_chip = [this](const QString& text, bool checked = false) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QStringLiteral("activityTabChip"));
        b->setCheckable(true);
        b->setChecked(checked);
        b->setFixedHeight(24);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    filter_all_ = make_chip(tr("ALL"), true);
    filter_swap_ = make_chip(tr("SWAP"));
    filter_send_ = make_chip(tr("SEND"));
    filter_receive_ = make_chip(tr("RECEIVE"));
    filter_other_ = make_chip(tr("OTHER"));

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(filter_all_);
    group->addButton(filter_swap_);
    group->addButton(filter_send_);
    group->addButton(filter_receive_);
    group->addButton(filter_other_);

    chips_row->addWidget(filter_all_);
    chips_row->addWidget(filter_swap_);
    chips_row->addWidget(filter_send_);
    chips_row->addWidget(filter_receive_);
    chips_row->addWidget(filter_other_);
    chips_row->addStretch(1);
    root->addLayout(chips_row);

    // Table
    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("activityTabTable"));
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels(
        {QStringLiteral("TIMESTAMP"), QStringLiteral("EVENT"),
         QStringLiteral("ASSET"), QStringLiteral("AMOUNT"),
         QStringLiteral("STATUS"), QStringLiteral("SIGNATURE")});
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setSortingEnabled(false);
    root->addWidget(table_, 1);

    // Footer hint
    footer_ = new QLabel(tr("No transactions yet."), this);
    footer_->setObjectName(QStringLiteral("activityTabFooter"));
    footer_->setWordWrap(true);
    root->addWidget(footer_);

    connect(filter_all_, &QPushButton::clicked, this, &ActivityTab::on_filter_changed);
    connect(filter_swap_, &QPushButton::clicked, this, &ActivityTab::on_filter_changed);
    connect(filter_send_, &QPushButton::clicked, this, &ActivityTab::on_filter_changed);
    connect(filter_receive_, &QPushButton::clicked, this, &ActivityTab::on_filter_changed);
    connect(filter_other_, &QPushButton::clicked, this, &ActivityTab::on_filter_changed);
    connect(table_, &QTableWidget::cellClicked, this, &ActivityTab::on_row_clicked);
}

void ActivityTab::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#activityTab { background:%1; }"

        "QPushButton#activityTabChip { background:transparent; color:%2; border:1px solid %3;"
        "  font-family:%4; font-size:10px; font-weight:700; letter-spacing:1.2px;"
        "  padding:0 12px; }"
        "QPushButton#activityTabChip:hover { color:%5; border-color:%6; }"
        "QPushButton#activityTabChip:checked { background:rgba(217,119,6,0.12); color:%7;"
        "  border-color:%7; }"

        "QTableWidget#activityTabTable { background:%1; color:%5; gridline-color:%3;"
        "  border:1px solid %3; selection-background-color:%8; selection-color:%5;"
        "  font-family:%4; font-size:11px; }"
        "QTableWidget#activityTabTable::item { padding:6px 10px; border-bottom:1px solid %3; }"
        "QHeaderView::section { background:%9; color:%2; padding:6px 10px;"
        "  border:none; border-bottom:1px solid %3; font-family:%4;"
        "  font-size:9px; font-weight:700; letter-spacing:1.4px; }"

        "QLabel#activityTabFooter { color:%2; font-family:%4; font-size:10px;"
        "  background:transparent; }"
    )
        .arg(BG_BASE(),         // %1
             TEXT_TERTIARY(),   // %2
             BORDER_DIM(),      // %3
             font,              // %4
             TEXT_PRIMARY(),    // %5
             BORDER_BRIGHT(),   // %6
             AMBER(),           // %7
             BG_HOVER(),        // %8
             BG_RAISED());      // %9

    setStyleSheet(ss);
}

bool ActivityTab::helius_key_present() const {
    auto r = SecureStorage::instance().retrieve(
        QStringLiteral("solana.helius_api_key"));
    return r.is_ok() && !r.value().isEmpty();
}

void ActivityTab::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    current_pubkey_ = pubkey;
    if (isVisible()) refresh_subscription();
}

void ActivityTab::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    latest_.clear();
    rebuild_table();
}

void ActivityTab::refresh_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_topic_.isEmpty()) {
        hub.unsubscribe(this, current_topic_);
        current_topic_.clear();
    }
    if (current_pubkey_.isEmpty()) return;
    current_topic_ = QStringLiteral("wallet:activity:%1").arg(current_pubkey_);
    hub.subscribe(this, current_topic_,
                  [this](const QVariant& v) { on_activity_update(v); });
    hub.request(current_topic_, /*force=*/true);
}

void ActivityTab::on_activity_update(const QVariant& v) {
    if (v.canConvert<QVector<fincept::wallet::ParsedActivity>>()) {
        latest_ = v.value<QVector<fincept::wallet::ParsedActivity>>();
        rebuild_table();
    }
}

void ActivityTab::on_topic_error(const QString& topic, const QString& error) {
    if (topic == current_topic_) {
        footer_->setText(tr("Activity fetch failed: %1").arg(error));
    }
}

void ActivityTab::on_filter_changed() {
    rebuild_table();
}

void ActivityTab::on_row_clicked(int row, int /*column*/) {
    auto* it = table_->item(row, 0);
    if (!it) return;
    const auto sig = it->data(Qt::UserRole).toString();
    if (sig.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(
        QStringLiteral("https://solscan.io/tx/%1").arg(sig)));
}

void ActivityTab::rebuild_table() {
    // Determine the active filter from which chip is checked.
    auto filter_ok = [this](Kind k) {
        if (filter_all_ && filter_all_->isChecked()) return true;
        if (k == Kind::Swap && filter_swap_->isChecked()) return true;
        if (k == Kind::Send && filter_send_->isChecked()) return true;
        if (k == Kind::Receive && filter_receive_->isChecked()) return true;
        if (k == Kind::Other && filter_other_->isChecked()) return true;
        return false;
    };

    QVector<Activity> filtered;
    filtered.reserve(latest_.size());
    for (const auto& a : latest_) {
        if (filter_ok(a.kind)) filtered.append(a);
    }

    table_->setRowCount(filtered.size());
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& a = filtered[i];
        auto* ts = new QTableWidgetItem(format_time(a.ts_ms));
        ts->setData(Qt::UserRole, a.signature);
        table_->setItem(i, 0, ts);

        table_->setItem(i, 1, new QTableWidgetItem(kind_label(a.kind)));
        table_->setItem(i, 2, new QTableWidgetItem(
            a.asset.isEmpty() ? QStringLiteral("—") : a.asset));
        table_->setItem(i, 3, new QTableWidgetItem(
            a.amount_ui.isEmpty() ? QStringLiteral("—") : a.amount_ui));
        auto* status = new QTableWidgetItem(
            a.status.isEmpty() ? QStringLiteral("—") : a.status);
        table_->setItem(i, 4, status);
        auto* sig = new QTableWidgetItem(shorten_sig(a.signature));
        sig->setToolTip(a.signature);
        table_->setItem(i, 5, sig);
    }

    // Footer status — count + Helius hint when fallback path is in use.
    if (current_pubkey_.isEmpty()) {
        footer_->setText(tr("Connect a wallet to view activity."));
    } else if (latest_.isEmpty()) {
        footer_->setText(tr("No transactions yet."));
    } else {
        QString text = tr("%1 of %2 events").arg(filtered.size()).arg(latest_.size());
        if (!helius_key_present()) {
            text += tr("  ·  Add a Helius API key in Settings for parsed swap "
                       "and transfer details.");
        }
        footer_->setText(text);
    }
}

void ActivityTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!current_pubkey_.isEmpty()) refresh_subscription();
}

void ActivityTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

} // namespace fincept::screens
