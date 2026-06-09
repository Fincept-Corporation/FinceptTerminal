#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

namespace fincept::screens::widgets {

namespace {
// Build a MARKET SELL / CNC order that exits the full quantity of one holding.
trading::UnifiedOrder make_square_off_order(const trading::BrokerHolding& h) {
    trading::UnifiedOrder o;
    o.symbol = h.symbol;
    o.exchange = h.exchange;
    o.side = trading::OrderSide::Sell;
    o.order_type = trading::OrderType::Market;
    o.quantity = h.quantity;
    // Market sells need a reference price for the paper engine to fill; the live
    // market path ignores price. Fall back to avg cost if no LTP is cached yet.
    o.price = h.ltp > 0 ? h.ltp : h.avg_price;
    o.product_type = trading::ProductType::Delivery; // CNC / delivery
    o.validity = QStringLiteral("DAY");
    return o;
}
} // namespace

BrokerHoldingsWidget::BrokerHoldingsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("HOLDINGS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    header_row->setSpacing(6);
    header_hint_ = new QLabel("—");
    header_row->addWidget(header_hint_, 1);
    square_off_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"));
    square_off_all_btn_->setCursor(Qt::PointingHandCursor);
    square_off_all_btn_->setEnabled(false);
    connect(square_off_all_btn_, &QPushButton::clicked, this, &BrokerHoldingsWidget::square_off_all);
    header_row->addWidget(square_off_all_btn_, 0);
    vl->addLayout(header_row);

    // Columns: Symbol, Qty, Avg, LTP, P&L %, and a trailing per-row exit button.
    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels(
        {tr("Symbol"), tr("Qty"), tr("Avg"), tr("LTP"), tr("P&L %"), QString()});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    vl->addWidget(table_, 1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject BrokerHoldingsWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void BrokerHoldingsWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty()) {
        const auto acct = trading::AccountManager::instance().get_account(account_id_);
        broker_id_ = acct.broker_id;
        header_hint_->setText(acct.display_name.isEmpty() ? account_id_ : acct.display_name);
    } else {
        header_hint_->setText(tr("No active account — click gear to configure"));
    }

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
}

QString BrokerHoldingsWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void BrokerHoldingsWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void BrokerHoldingsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = trading::broker_topic(broker_id_, account_id_, QStringLiteral("holdings"));
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerHolding>>())
            return;
        populate(v.value<QVector<trading::BrokerHolding>>());
    });
    hub_active_ = true;
}

void BrokerHoldingsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void BrokerHoldingsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void BrokerHoldingsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void BrokerHoldingsWidget::populate(const QVector<trading::BrokerHolding>& rows) {
    holdings_ = rows;
    table_->setRowCount(rows.size());
    int sellable = 0;
    for (int i = 0; i < rows.size(); ++i) {
        const auto& h = rows[i];
        auto* sym = new QTableWidgetItem(h.symbol);
        auto* qty = new QTableWidgetItem(QString::number(h.quantity, 'f', 0));
        auto* avg = new QTableWidgetItem(QString::number(h.avg_price, 'f', 2));
        auto* ltp = new QTableWidgetItem(QString::number(h.ltp, 'f', 2));
        auto* pnl_pct = new QTableWidgetItem(
            QString("%1%2%").arg(h.pnl_pct >= 0 ? "+" : "").arg(h.pnl_pct, 0, 'f', 2));
        const QColor col(h.pnl_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        pnl_pct->setForeground(col);
        qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        avg->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ltp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pnl_pct->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 0, sym);
        table_->setItem(i, 1, qty);
        table_->setItem(i, 2, avg);
        table_->setItem(i, 3, ltp);
        table_->setItem(i, 4, pnl_pct);

        // Per-row exit button — squares off this single holding. Capture the
        // holding by value so a click always acts on the row it was drawn for.
        auto* exit_btn = new QPushButton(tr("Exit"), table_);
        exit_btn->setCursor(Qt::PointingHandCursor);
        exit_btn->setProperty("holdingsExit", true);
        const bool can_sell = h.quantity > 0;
        exit_btn->setEnabled(can_sell);
        connect(exit_btn, &QPushButton::clicked, this,
                [this, h]() { square_off_holding(h); });
        table_->setCellWidget(i, 5, exit_btn);
        if (can_sell)
            ++sellable;
    }
    square_off_all_btn_->setEnabled(sellable > 0);
    set_loading(false);
}

QDialog* BrokerHoldingsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Holdings"));
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow(tr("Broker account"), combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo]() {
        const QString picked = combo->currentData().toString();
        if (!picked.isEmpty() && picked != account_id_) {
            QJsonObject cfg;
            cfg.insert("account_id", picked);
            apply_config(cfg);
            emit config_changed(cfg);
        }
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void BrokerHoldingsWidget::square_off_holding(const trading::BrokerHolding& h) {
    if (account_id_.isEmpty() || h.quantity <= 0)
        return;

    const auto answer = QMessageBox::warning(
        this, tr("Square Off Holding"),
        tr("Place a MARKET SELL order for %1 %2 (CNC)?")
            .arg(QString::number(h.quantity, 'f', 0), h.symbol),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const auto resp = trading::UnifiedTrading::instance().place_order(account_id_, make_square_off_order(h));
    if (resp.success) {
        QMessageBox::information(this, tr("Square Off Holding"),
                                 tr("Sell order placed for %1 (order %2).").arg(h.symbol, resp.order_id));
    } else {
        QMessageBox::critical(this, tr("Square Off Holding"),
                              tr("Failed to square off %1: %2").arg(h.symbol, resp.message));
    }
}

void BrokerHoldingsWidget::square_off_all() {
    if (account_id_.isEmpty())
        return;

    // Snapshot sellable holdings now — the holdings subscription may refresh
    // (and rebuild the table) while the confirm dialog / orders are in flight.
    QVector<trading::BrokerHolding> targets;
    for (const auto& h : holdings_)
        if (h.quantity > 0)
            targets.append(h);
    if (targets.isEmpty()) {
        QMessageBox::information(this, tr("Square Off All Holdings"), tr("No holdings to square off."));
        return;
    }

    const auto answer = QMessageBox::warning(
        this, tr("Square Off All Holdings"),
        tr("This will place MARKET SELL orders to exit ALL %1 holding(s) in this account.\n\n"
           "Positions are NOT affected. Continue?")
            .arg(targets.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    int placed = 0;
    QStringList failures;
    for (const auto& h : targets) {
        const auto resp = trading::UnifiedTrading::instance().place_order(account_id_, make_square_off_order(h));
        if (resp.success)
            ++placed;
        else
            failures.append(QStringLiteral("%1 (%2)").arg(h.symbol, resp.message));
    }

    if (failures.isEmpty()) {
        QMessageBox::information(this, tr("Square Off All Holdings"),
                                 tr("Placed %1 sell order(s).").arg(placed));
    } else {
        QMessageBox::warning(this, tr("Square Off All Holdings"),
                             tr("Placed %1 order(s). %2 failed:\n%3")
                                 .arg(placed)
                                 .arg(failures.size())
                                 .arg(failures.join(QStringLiteral("\n"))));
    }
}

void BrokerHoldingsWidget::on_theme_changed() {
    apply_styles();
}

void BrokerHoldingsWidget::apply_styles() {
    header_hint_->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;")
            .arg(ui::colors::TEXT_TERTIARY()));
    table_->setStyleSheet(QString(
        "QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:10px;border:none;}"
        "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
        "padding:2px 4px;font-size:9px;font-weight:bold;}"
        "QTableWidget::item{padding:2px 4px;}"
        "QPushButton[holdingsExit=\"true\"]{color:%5;background:transparent;border:1px solid %5;"
        "border-radius:2px;padding:0 6px;font-size:9px;font-weight:bold;}"
        "QPushButton[holdingsExit=\"true\"]:hover{color:%6;background:%5;}"
        "QPushButton[holdingsExit=\"true\"]:disabled{color:%2;border-color:%2;}")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_TERTIARY(), ui::colors::NEGATIVE(), ui::colors::BG_BASE()));

    if (square_off_all_btn_)
        square_off_all_btn_->setStyleSheet(
            QString("QPushButton{color:%1;background:transparent;border:1px solid %1;border-radius:2px;"
                    "padding:2px 8px;font-size:9px;font-weight:bold;}"
                    "QPushButton:hover{color:%2;background:%1;}"
                    "QPushButton:disabled{color:%3;border-color:%3;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
}

void BrokerHoldingsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("HOLDINGS"));
    hub_resubscribe(); // re-renders header hint + table from cached subscription
}

} // namespace fincept::screens::widgets
