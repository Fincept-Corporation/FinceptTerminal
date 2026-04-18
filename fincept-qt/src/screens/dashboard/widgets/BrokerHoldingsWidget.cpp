#include "screens/dashboard/widgets/BrokerHoldingsWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens::widgets {

BrokerHoldingsWidget::BrokerHoldingsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("HOLDINGS", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({"Symbol", "Qty", "Avg", "LTP", "P&L %"});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
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
        header_hint_->setText("No active account — click gear to configure");
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
    table_->setRowCount(rows.size());
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
    }
    set_loading(false);
}

QDialog* BrokerHoldingsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Holdings");
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
        "QTableWidget::item{padding:2px 4px;}")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_TERTIARY()));
}

} // namespace fincept::screens::widgets
