#include "screens/crypto_center/tabs/ActivityTab.h"
#include "screens/crypto_center/CryptoCenterText.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletService.h"
#include "services/wallet/WalletTypes.h"
#include "storage/secure/SecureStorage.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QButtonGroup>
#include <QColor>
#include <QCoreApplication>
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

namespace cc_fmt = fincept::ui::formatting;

namespace fincept::screens {

namespace {

using fincept::screens::crypto_center_text::font_stack; // CryptoCenterText.h (one definition)

QString format_time(qint64 ts_ms) {
    if (ts_ms <= 0)
        return QStringLiteral("—");
    return QDateTime::fromMSecsSinceEpoch(ts_ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString kind_label(fincept::wallet::ParsedActivity::Kind k) {
    using K = fincept::wallet::ParsedActivity::Kind;
    switch (k) {
        case K::Swap:
            return QCoreApplication::translate("ActivityTab", "SWAP");
        case K::Receive:
            return QCoreApplication::translate("ActivityTab", "RECEIVE");
        case K::Send:
            return QCoreApplication::translate("ActivityTab", "SEND");
        case K::Other:
            return QCoreApplication::translate("ActivityTab", "OTHER");
    }
    return QCoreApplication::translate("ActivityTab", "OTHER");
}

QString shorten_sig(const QString& sig) {
    if (sig.size() < 14)
        return sig;
    return sig.left(8) + QStringLiteral("…") + sig.right(4);
}

/// Solscan needs `?cluster=` for non-mainnet transactions; without it a devnet
/// / testnet signature opens a "transaction not found" page. Derived from the
/// same `solana.rpc_url` override the rest of the Crypto Center reads.
/// `unreadable` is set when the override is stored but cannot be read: the cluster
/// is then unknown (not mainnet) and no link is built from a guess.
QString activity_explorer_cluster_suffix(bool* unreadable) {
    const SecretRead r = SecureStorage::instance().read(QStringLiteral("solana.rpc_url"));
    *unreadable = r.failed();
    if (!r.found())
        return {};
    const QString url = r.value.toLower();
    if (url.contains(QStringLiteral("devnet")))
        return QStringLiteral("?cluster=devnet");
    if (url.contains(QStringLiteral("testnet")))
        return QStringLiteral("?cluster=testnet");
    return {};
}

} // namespace

ActivityTab::ActivityTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("activityTab"));
    build_ui();
    apply_theme();

    auto& svc = fincept::wallet::WalletService::instance();
    connect(&svc, &fincept::wallet::WalletService::wallet_connected, this, &ActivityTab::on_wallet_connected);
    connect(&svc, &fincept::wallet::WalletService::wallet_disconnected, this, &ActivityTab::on_wallet_disconnected);

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this, &ActivityTab::on_topic_error);

    if (svc.is_connected()) {
        on_wallet_connected(svc.current_pubkey(), svc.state().label);
    }

    // A number-format change re-renders the amounts from the list already received.
    cc_fmt::on_format_changed(this, [this]() {
        if (has_data_)
            rebuild_table();
    });
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
        b->setMinimumHeight(24);
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
        {tr("TIMESTAMP"), tr("EVENT"), tr("ASSET"), tr("AMOUNT"), tr("STATUS"), tr("SIGNATURE")});
    table_->verticalHeader()->setVisible(false);
    // Interactive, not ResizeToContents: rebuild_table() fills the rows and then
    // fits these columns once (a ResizeToContents header re-measures on every cell).
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
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

    const QString ss =
        QString("QWidget#activityTab { background:%1; }"

                       "QPushButton#activityTabChip { background:transparent; color:%2; border:1px solid %3;"
                       "  font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:1.2px;"
                       "  padding:0 12px; }"
                       "QPushButton#activityTabChip:hover { color:%5; border-color:%6; }"
                       "QPushButton#activityTabChip:checked { background:" + ui::colors::alpha(AMBER, 0.12) + "; color:%7;"
                       "  border-color:%7; }"

                       "QTableWidget#activityTabTable { background:%1; color:%5; gridline-color:%3;"
                       "  border:1px solid %3; selection-background-color:%8; selection-color:%5;"
                       "  font-family:%4; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                       "QTableWidget#activityTabTable::item { padding:6px 10px; border-bottom:1px solid %3; }"
                       "QHeaderView::section { background:%9; color:%2; padding:6px 10px;"
                       "  border:none; border-bottom:1px solid %3; font-family:%4;"
                       "  font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1.4px; }"

                       "QLabel#activityTabFooter { color:%2; font-family:%4; font-size:" + fincept::ui::fonts::px(-4) + "px;"
                       "  background:transparent; }")
            .arg(BG_BASE(),       // %1
                 TEXT_TERTIARY(), // %2
                 BORDER_DIM(),    // %3
                 font,            // %4
                 TEXT_PRIMARY(),  // %5
                 BORDER_BRIGHT(), // %6
                 AMBER(),         // %7
                 BG_HOVER(),      // %8
                 BG_RAISED());    // %9

    setStyleSheet(ss);
}

bool ActivityTab::helius_key_present() const {
    // A key that is stored but cannot be read is not "no key": no "add a key" hint
    // then — the activity topic already reports the read error (WalletActivityProducer).
    const SecretRead r = SecureStorage::instance().read(QStringLiteral("solana.helius_api_key"));
    return r.failed() || (r.found() && !r.value.isEmpty());
}

void ActivityTab::on_wallet_connected(const QString& pubkey, const QString& /*label*/) {
    if (pubkey != current_pubkey_) {
        // Another wallet: nothing of the previous one's list or error applies.
        latest_.clear();
        has_data_ = false;
        fetch_error_.clear();
    }
    current_pubkey_ = pubkey;
    if (isVisible()) {
        refresh_subscription();
        rebuild_table();
    }
}

void ActivityTab::on_wallet_disconnected() {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.unsubscribe(this);
    current_topic_.clear();
    current_pubkey_.clear();
    latest_.clear();
    has_data_ = false;
    fetch_error_.clear();
    rebuild_table();
}

void ActivityTab::refresh_subscription() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!current_topic_.isEmpty()) {
        hub.unsubscribe(this, current_topic_);
        current_topic_.clear();
    }
    if (current_pubkey_.isEmpty())
        return;
    current_topic_ = QStringLiteral("wallet:activity:%1").arg(current_pubkey_);
    hub.subscribe(this, current_topic_, [this](const QVariant& v) { on_activity_update(v); });
    // force=false: a tab switch must not re-hit Helius/RPC while the cached feed is fresh.
    hub.request(current_topic_, /*force=*/false);
}

void ActivityTab::on_activity_update(const QVariant& v) {
    if (v.canConvert<QVector<fincept::wallet::ParsedActivity>>()) {
        latest_ = v.value<QVector<fincept::wallet::ParsedActivity>>();
        has_data_ = true;
        fetch_error_.clear();
        rebuild_table();
    }
}

void ActivityTab::on_topic_error(const QString& topic, const QString& error) {
    if (topic != current_topic_)
        return;
    if (!has_data_) {
        // Nothing on screen yet: the error page (with RETRY) instead of a bare header.
        fetch_error_ = error;
        rebuild_table();
        return;
    }
    // Rows on screen stay; the footer says the refresh failed.
    footer_->setText(tr("Activity fetch failed: %1").arg(error));
}

void ActivityTab::retry_fetch() {
    if (current_pubkey_.isEmpty())
        return;
    fetch_error_.clear();
    rebuild_table(); // skeleton until the answer (or the next error) arrives
    if (current_topic_.isEmpty())
        refresh_subscription();
    if (!current_topic_.isEmpty())
        fincept::datahub::DataHub::instance().request(current_topic_, /*force=*/true);
}

fincept::ui::EmptyState* ActivityTab::ensure_state_page() {
    if (state_page_)
        return state_page_;
    state_page_ = new ui::EmptyState(this);
    if (auto* root = qobject_cast<QVBoxLayout*>(layout()))
        root->insertWidget(root->indexOf(table_) + 1, state_page_, 1);
    connect(state_page_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (page_kind_ == PageKind::Error)
            retry_fetch();
        else if (page_kind_ == PageKind::FilteredOut && filter_all_)
            filter_all_->click(); // exclusive group: ALL checked, on_filter_changed rebuilds
    });
    return state_page_;
}

void ActivityTab::update_view_state(int filtered_rows) {
    PageKind kind = PageKind::None;
    bool loading = false;
    if (current_pubkey_.isEmpty())
        kind = PageKind::NoWallet;
    else if (!has_data_ && fetch_error_.isEmpty())
        loading = true;
    else if (!has_data_)
        kind = PageKind::Error;
    else if (latest_.isEmpty())
        kind = PageKind::NoTransactions;
    else if (filtered_rows == 0)
        kind = PageKind::FilteredOut;

    const bool show_table = !loading && kind == PageKind::None;
    table_->setVisible(show_table);
    footer_->setVisible(show_table);
    // The filter chips only mean something over a list of transactions.
    const bool chips = has_data_ && !latest_.isEmpty();
    for (QPushButton* b : {filter_all_, filter_swap_, filter_send_, filter_receive_, filter_other_}) {
        if (b)
            b->setVisible(chips);
    }

    if (loading) {
        if (!skeleton_) {
            skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, this);
            skeleton_->set_row_count(8);
            if (auto* root = qobject_cast<QVBoxLayout*>(layout()))
                root->insertWidget(root->indexOf(table_) + 1, skeleton_, 1);
        }
        skeleton_->set_message(tr("Loading wallet activity…"));
        skeleton_->show();
    } else if (skeleton_) {
        skeleton_->hide();
    }

    page_kind_ = kind;
    if (kind == PageKind::None) {
        if (state_page_)
            state_page_->hide();
        return;
    }
    ui::EmptyState* page = ensure_state_page();
    switch (kind) {
        case PageKind::NoWallet:
            page->apply(ui::EmptyState::no_data_spec(tr("No wallet connected"), tr("Connect a wallet to view activity.")));
            break;
        case PageKind::Error:
            // The producer's sentence (RPC failure, Helius key that could not be read …).
            page->apply(ui::EmptyState::error_spec(fetch_error_, tr("Could not load activity")));
            break;
        case PageKind::NoTransactions:
            page->apply(ui::EmptyState::no_data_spec(
                tr("No transactions yet"), tr("Swaps, sends and receives of this wallet appear here, newest first.")));
            break;
        case PageKind::FilteredOut: {
            QString filter_name;
            for (QPushButton* b : {filter_swap_, filter_send_, filter_receive_, filter_other_}) {
                if (b && b->isChecked())
                    filter_name = b->text();
            }
            page->apply(ui::EmptyState::no_data_spec(
                tr("No %1 transactions").arg(filter_name),
                tr("None of the last %n transaction(s) matches this filter.", "", int(latest_.size())), tr("SHOW ALL")));
            break;
        }
        case PageKind::None:
            break;
    }
    page->show();
}

void ActivityTab::on_filter_changed() {
    rebuild_table();
}

void ActivityTab::on_row_clicked(int row, int /*column*/) {
    auto* it = table_->item(row, 0);
    if (!it)
        return;
    const auto sig = it->data(Qt::UserRole).toString();
    if (sig.isEmpty())
        return;
    bool cluster_unreadable = false;
    const QString suffix = activity_explorer_cluster_suffix(&cluster_unreadable);
    if (cluster_unreadable) {
        footer_->setText(tr("Could not read the stored RPC setting, so the explorer cluster is unknown — the link "
                            "was not opened. Click the row again in a moment."));
        return;
    }
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://solscan.io/tx/%1%2").arg(sig, suffix)));
}

void ActivityTab::rebuild_table() {
    // Determine the active filter from which chip is checked.
    auto filter_ok = [this](Kind k) {
        if (filter_all_ && filter_all_->isChecked())
            return true;
        if (k == Kind::Swap && filter_swap_->isChecked())
            return true;
        if (k == Kind::Send && filter_send_->isChecked())
            return true;
        if (k == Kind::Receive && filter_receive_->isChecked())
            return true;
        if (k == Kind::Other && filter_other_->isChecked())
            return true;
        return false;
    };

    QVector<Activity> filtered;
    filtered.reserve(latest_.size());
    for (const auto& a : latest_) {
        if (filter_ok(a.kind))
            filtered.append(a);
    }

    table_->setUpdatesEnabled(false);
    table_->setRowCount(filtered.size());
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& a = filtered[i];
        auto* ts = new QTableWidgetItem(format_time(a.ts_ms));
        ts->setData(Qt::UserRole, a.signature);
        table_->setItem(i, 0, ts);

        auto* event_item = new QTableWidgetItem(kind_label(a.kind));
        // The parsed counterparty was collected by the producer but never shown.
        // Full address in the tooltip so a transfer can be checked against the
        // wallet / explorer without leaving the row.
        if (!a.counterparty.isEmpty()) {
            const QString who = a.kind == Kind::Send      ? tr("To: %1")
                                : a.kind == Kind::Receive ? tr("From: %1")
                                                          : tr("Counterparty: %1"); // swap / other
            event_item->setToolTip(who.arg(a.counterparty));
        }
        table_->setItem(i, 1, event_item);
        table_->setItem(i, 2, new QTableWidgetItem(a.asset.isEmpty() ? QStringLiteral("—") : a.asset));
        // Amount: right-aligned like every numeric column; outflow / inflow coloured.
        // The producer's plain decimal string ("1234.5678") — a token quantity —
        // with the user's grouping and separator, its own decimals kept (up to the
        // formatter's 10); anything else as sent.
        QString amount_text = a.amount_ui.isEmpty() ? QStringLiteral("—") : a.amount_ui;
        {
            const QString raw = a.amount_ui.trimmed();
            bool ok = false;
            const double v = raw.toDouble(&ok);
            if (ok && !raw.contains(QLatin1Char('e'), Qt::CaseInsensitive)) {
                const int dot = raw.indexOf(QLatin1Char('.'));
                const int dp = dot >= 0 ? int(raw.size()) - dot - 1 : 0;
                if (dp <= 10)
                    amount_text = cc_fmt::number(v, dp);
            }
        }
        auto* amount = new QTableWidgetItem(amount_text);
        amount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!a.amount_ui.isEmpty()) {
            if (a.kind == Kind::Send)
                amount->setForeground(QColor(ui::colors::NEGATIVE()));
            else if (a.kind == Kind::Receive)
                amount->setForeground(QColor(ui::colors::POSITIVE()));
        }
        table_->setItem(i, 3, amount);
        auto* status = new QTableWidgetItem(a.status.isEmpty() ? QStringLiteral("—") : a.status);
        if (a.status.compare(QLatin1String("failed"), Qt::CaseInsensitive) == 0)
            status->setForeground(QColor(ui::colors::DANGER()));
        else if (a.status.compare(QLatin1String("confirmed"), Qt::CaseInsensitive) == 0)
            status->setForeground(QColor(ui::colors::SUCCESS()));
        table_->setItem(i, 4, status);
        auto* sig = new QTableWidgetItem(shorten_sig(a.signature));
        sig->setToolTip(a.signature);
        table_->setItem(i, 5, sig);
    }
    // Filled: fit the content columns once (2 and 3 stretch).
    if (!filtered.isEmpty()) {
        for (int c : {0, 1, 4, 5})
            table_->resizeColumnToContents(c);
    }
    table_->setUpdatesEnabled(true);
    update_view_state(static_cast<int>(filtered.size()));

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
    if (!current_pubkey_.isEmpty())
        refresh_subscription(); // a cached list arrives synchronously and rebuilds
    if (!has_data_)
        rebuild_table(); // skeleton / error / no-wallet page instead of a bare header
}

void ActivityTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
    current_topic_.clear();
}

void ActivityTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ActivityTab::retranslateUi() {
    if (filter_all_)
        filter_all_->setText(tr("ALL"));
    if (filter_swap_)
        filter_swap_->setText(tr("SWAP"));
    if (filter_send_)
        filter_send_->setText(tr("SEND"));
    if (filter_receive_)
        filter_receive_->setText(tr("RECEIVE"));
    if (filter_other_)
        filter_other_->setText(tr("OTHER"));
    if (table_) {
        table_->setHorizontalHeaderLabels(
            {tr("TIMESTAMP"), tr("EVENT"), tr("ASSET"), tr("AMOUNT"), tr("STATUS"), tr("SIGNATURE")});
    }
    // Re-render rows (kind labels) + footer in the new locale.
    rebuild_table();
}

} // namespace fincept::screens
