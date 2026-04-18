#include "screens/dashboard/widgets/OrderBookMiniWidget.h"

#include "core/logging/Logger.h"
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
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QtConcurrent>

namespace fincept::screens::widgets {

OrderBookMiniWidget::OrderBookMiniWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("WORKING ORDERS", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    table_ = new QTableWidget(0, 6, this);
    table_->setHorizontalHeaderLabels({"Symbol", "Side", "Qty", "Price", "Status", ""});
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
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    table_->setColumnWidth(5, 52);
    vl->addWidget(table_, 1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject OrderBookMiniWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void OrderBookMiniWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    broker_id_.clear();
    if (!account_id_.isEmpty()) {
        const auto acct = trading::AccountManager::instance().get_account(account_id_);
        broker_id_ = acct.broker_id;
        header_hint_->setText(acct.display_name.isEmpty() ? account_id_ : acct.display_name);
    } else {
        header_hint_->setText("No active account — click gear to configure");
    }

    if (isVisible() && !broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        hub_resubscribe();
    }
}

QString OrderBookMiniWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void OrderBookMiniWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

bool OrderBookMiniWidget::is_working(const QString& status) {
    const QString s = status.toUpper();
    return s != "COMPLETE" && s != "COMPLETED" && s != "FILLED" && s != "CANCELLED" &&
           s != "CANCELED" && s != "REJECTED";
}

void OrderBookMiniWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = trading::broker_topic(broker_id_, account_id_, QStringLiteral("orders"));
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerOrderInfo>>())
            return;
        populate(v.value<QVector<trading::BrokerOrderInfo>>());
    });
    hub_active_ = true;
}

void OrderBookMiniWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void OrderBookMiniWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void OrderBookMiniWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void OrderBookMiniWidget::populate(const QVector<trading::BrokerOrderInfo>& rows) {
    QVector<trading::BrokerOrderInfo> working;
    working.reserve(rows.size());
    for (const auto& o : rows) {
        if (is_working(o.status))
            working.append(o);
    }
    table_->setRowCount(working.size());
    for (int i = 0; i < working.size(); ++i) {
        const auto& o = working[i];
        auto* sym = new QTableWidgetItem(o.symbol);
        auto* side = new QTableWidgetItem(o.side);
        side->setForeground(QColor(o.side.compare("BUY", Qt::CaseInsensitive) == 0
                                       ? ui::colors::POSITIVE()
                                       : ui::colors::NEGATIVE()));
        auto* qty = new QTableWidgetItem(QString::number(o.quantity, 'f', 0));
        auto* price = new QTableWidgetItem(o.price > 0 ? QString::number(o.price, 'f', 2) : "MKT");
        auto* status = new QTableWidgetItem(o.status);
        qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 0, sym);
        table_->setItem(i, 1, side);
        table_->setItem(i, 2, qty);
        table_->setItem(i, 3, price);
        table_->setItem(i, 4, status);

        auto* cancel_btn = new QPushButton("×", table_);
        cancel_btn->setToolTip("Cancel order " + o.order_id);
        cancel_btn->setCursor(Qt::PointingHandCursor);
        cancel_btn->setFixedHeight(18);
        cancel_btn->setStyleSheet(QString("QPushButton{color:%1;background:transparent;border:1px solid %2;"
                                          "border-radius:2px;font-size:11px;font-weight:bold;}"
                                          "QPushButton:hover{color:%3;border-color:%3;}")
                                      .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(),
                                           ui::colors::NEGATIVE()));
        const QString oid = o.order_id;
        connect(cancel_btn, &QPushButton::clicked, this, [this, oid]() { cancel_order(oid); });
        table_->setCellWidget(i, 5, cancel_btn);
    }
    set_loading(false);
}

void OrderBookMiniWidget::cancel_order(const QString& order_id) {
    if (account_id_.isEmpty() || order_id.isEmpty())
        return;
    if (QMessageBox::question(this, "Cancel Order",
                              QString("Cancel order %1?").arg(order_id),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // BrokerHttp blocking trap (P1) — always wrap live broker calls in QtConcurrent::run.
    const QString acct_id = account_id_;
    QPointer<OrderBookMiniWidget> self = this;
    QtConcurrent::run([self, acct_id, order_id]() {
        if (!self)
            return;
        trading::UnifiedTrading::instance().cancel_order(acct_id, order_id);
        // Order stream will publish the updated order state via the hub;
        // populate() repaints automatically.
    });
}

QDialog* OrderBookMiniWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Working Orders");
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    const auto accts = trading::AccountManager::instance().list_accounts();
    for (const auto& a : accts) {
        const QString label = a.display_name.isEmpty() ? (a.broker_id + " — " + a.account_id) : a.display_name;
        combo->addItem(label, a.account_id);
        if (a.account_id == account_id_)
            combo->setCurrentIndex(combo->count() - 1);
    }
    form->addRow("Broker account", combo);

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

void OrderBookMiniWidget::on_theme_changed() {
    apply_styles();
}

void OrderBookMiniWidget::apply_styles() {
    header_hint_->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;")
            .arg(ui::colors::TEXT_TERTIARY()));
    table_->setStyleSheet(QString(
        "QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:10px;border:none;}"
        "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
        "padding:2px 4px;font-size:9px;font-weight:bold;}"
        "QTableWidget::item{padding:2px 4px;}")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_TERTIARY()));
}

} // namespace fincept::screens::widgets
