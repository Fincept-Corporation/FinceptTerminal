#include "screens/dashboard/widgets/OpenPositionsWidget.h"

#include "core/logging/Logger.h"
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

OpenPositionsWidget::OpenPositionsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("OPEN POSITIONS"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    header_hint_ = new QLabel("—");
    vl->addWidget(header_hint_);

    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg"), tr("LTP"), tr("P&L")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->setToolTip(tr("Double-click a position to open it in Equity Trading"));
    vl->addWidget(table_, 1);

    // Row → symbol link. The exchange rides on the SYMBOL cell; the "No open
    // positions" placeholder row carries none and is ignored.
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        auto* it = table_->item(row, 0);
        if (it && it->data(Qt::UserRole).isValid())
            open_symbol(it->text(), QStringLiteral("equity_trading"), QStringLiteral("equity"),
                        it->data(Qt::UserRole).toString());
    });

    // Title-bar refresh = "retry". It used to be wired to nothing, so the 20 s
    // "No data yet - click refresh to retry" prompt pointed at a dead button.
    // The account stream polls on its own cadence, so the only thing a retry can
    // do is (re)start that stream, show the loading state again, and ask the hub.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (broker_id_.isEmpty() || account_id_.isEmpty())
            return;
        ensure_stream_running();
        if (table_->rowCount() == 0)
            set_loading(true);
        datahub::DataHub::instance().request(
            trading::broker_topic(broker_id_, account_id_, QStringLiteral("positions")), /*force=*/true);
    });

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject OpenPositionsWidget::config() const {
    QJsonObject o;
    if (!account_id_.isEmpty())
        o.insert("account_id", account_id_);
    return o;
}

void OpenPositionsWidget::apply_config(const QJsonObject& cfg) {
    account_id_ = cfg.value("account_id").toString();
    if (account_id_.isEmpty())
        account_id_ = resolve_account_id();

    // Resolve broker_id from AccountManager for the topic key
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

QString OpenPositionsWidget::resolve_account_id() const {
    const auto active = trading::AccountManager::instance().active_accounts();
    return active.isEmpty() ? QString() : active.first().account_id;
}

void OpenPositionsWidget::ensure_stream_running() {
    auto& mgr = trading::DataStreamManager::instance();
    if (!mgr.has_stream(account_id_))
        mgr.start_stream(account_id_);
}

void OpenPositionsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    if (broker_id_.isEmpty() || account_id_.isEmpty())
        return;
    const QString topic = trading::broker_topic(broker_id_, account_id_, QStringLiteral("positions"));
    // Nothing on screen yet → show the loading state, so a disconnected account
    // ends in "No data yet" (BaseWidget watchdog) instead of a silent blank grid.
    if (table_->rowCount() == 0)
        set_loading(true);
    hub.subscribe(this, topic, [this](const QVariant& v) {
        if (!v.canConvert<QVector<trading::BrokerPosition>>())
            return;
        populate(v.value<QVector<trading::BrokerPosition>>());
    });
    hub_active_ = true;
}

void OpenPositionsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void OpenPositionsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!broker_id_.isEmpty() && !account_id_.isEmpty()) {
        ensure_stream_running();
        if (!hub_active_)
            hub_resubscribe();
    }
}

void OpenPositionsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void OpenPositionsWidget::populate(const QVector<trading::BrokerPosition>& rows) {
    // Cell items are reused between publishes (setText & co. skip unchanged
    // values) instead of allocating 5 per row per publish. Switching between the
    // empty-state message and data rows starts from clean cells, so no cell
    // keeps the other mode's alignment / colour / data.
    const bool want_empty = rows.isEmpty();
    if (want_empty != showing_empty_) {
        table_->clearContents();
        showing_empty_ = want_empty;
    }
    auto cell = [this](int row, int col) {
        QTableWidgetItem* item = table_->item(row, col);
        if (!item) {
            item = new QTableWidgetItem;
            table_->setItem(row, col, item);
        }
        return item;
    };

    // Empty state — a flat account previously rendered an empty grid with no
    // explanation, which reads the same as a failed load.
    table_->clearSpans();
    if (want_empty) {
        table_->setRowCount(1);
        auto* msg = cell(0, 0);
        msg->setText(tr("No open positions"));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        table_->setSpan(0, 0, 1, table_->columnCount());
        set_loading(false);
        return;
    }

    table_->setRowCount(static_cast<int>(rows.size()));
    const QColor pos_color(ui::colors::POSITIVE());
    const QColor neg_color(ui::colors::NEGATIVE());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& p = rows[i];
        auto* sym = cell(i, 0);
        sym->setText(p.symbol);
        sym->setData(Qt::UserRole, p.exchange);
        auto* qty = cell(i, 1);
        qty->setText(QString::number(p.quantity, 'f', 0));
        qty->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* avg = cell(i, 2);
        avg->setText(QString::number(p.avg_price, 'f', 2));
        avg->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* ltp = cell(i, 3);
        ltp->setText(QString::number(p.ltp, 'f', 2));
        ltp->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* pnl = cell(i, 4);
        pnl->setText(QString("%1%2").arg(p.pnl >= 0 ? "+" : "").arg(p.pnl, 0, 'f', 2));
        pnl->setForeground(p.pnl >= 0 ? pos_color : neg_color);
        pnl->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    set_loading(false);
}

QDialog* OpenPositionsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Open Positions"));
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

void OpenPositionsWidget::on_theme_changed() {
    apply_styles();
}

void OpenPositionsWidget::apply_styles() {
    header_hint_->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    table_->setStyleSheet(
        QString("QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:10px;border:none;}"
                "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
                "padding:2px 4px;font-size:9px;font-weight:bold;}"
                "QTableWidget::item{padding:2px 4px;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::TEXT_TERTIARY()));
}

void OpenPositionsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("OPEN POSITIONS"));
    // The old body called hub_resubscribe(), which rebuilds every hub
    // subscription and translates nothing. Re-apply the actual strings.
    if (table_)
        table_->setHorizontalHeaderLabels({tr("Symbol"), tr("Qty"), tr("Avg"), tr("LTP"), tr("P&L")});
    if (header_hint_ && account_id_.isEmpty())
        header_hint_->setText(tr("No active account — click gear to configure"));
}

} // namespace fincept::screens::widgets
