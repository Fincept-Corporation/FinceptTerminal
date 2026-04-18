#include "screens/dashboard/widgets/TradeTapeWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/TradingTypes.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>

namespace fincept::screens::widgets {

TradeTapeWidget::TradeTapeWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("TRADES", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"Time", "Price", "Size"});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    vl->addWidget(table_, 1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject TradeTapeWidget::config() const {
    QJsonObject o;
    o.insert("exchange", exchange_);
    o.insert("pair", pair_);
    o.insert("max_rows", max_rows_);
    return o;
}

void TradeTapeWidget::apply_config(const QJsonObject& cfg) {
    exchange_ = cfg.value("exchange").toString("kraken").trimmed().toLower();
    if (exchange_ != "kraken" && exchange_ != "hyperliquid")
        exchange_ = "kraken";
    pair_ = cfg.value("pair").toString("BTC/USD").trimmed();
    if (pair_.isEmpty())
        pair_ = "BTC/USD";
    max_rows_ = qBound(5, cfg.value("max_rows").toInt(20), 200);

    trades_.clear();
    render();

    if (isVisible())
        hub_resubscribe();
}

void TradeTapeWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic =
        QStringLiteral("ws:") + exchange_ + QStringLiteral(":trades:") + pair_;
    hub.subscribe(this, topic, [this](const QVariant& v) { on_trade(v); });
    hub_active_ = true;
}

void TradeTapeWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void TradeTapeWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void TradeTapeWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void TradeTapeWidget::on_trade(const QVariant& v) {
    if (!v.canConvert<fincept::trading::TradeData>())
        return;
    const auto td = v.value<fincept::trading::TradeData>();
    Trade t;
    t.when = (td.timestamp > 0)
                 ? QDateTime::fromMSecsSinceEpoch(td.timestamp)
                 : QDateTime::currentDateTime();
    t.side = td.side;
    t.price = td.price;
    t.amount = td.amount;
    trades_.prepend(t);
    while (trades_.size() > max_rows_)
        trades_.removeLast();
    render();
}

void TradeTapeWidget::render() {
    table_->setRowCount(trades_.size());
    const QColor buy = ui::colors::POSITIVE();
    const QColor sell = ui::colors::NEGATIVE();
    for (int i = 0; i < trades_.size(); ++i) {
        const auto& t = trades_[i];
        auto* tm = new QTableWidgetItem(t.when.toString("HH:mm:ss"));
        auto* px = new QTableWidgetItem(QString::number(t.price, 'f', 2));
        auto* sz = new QTableWidgetItem(QString::number(t.amount, 'f', 4));
        const QColor col = (t.side.compare("buy", Qt::CaseInsensitive) == 0) ? buy : sell;
        px->setForeground(col);
        sz->setForeground(col);
        px->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, 0, tm);
        table_->setItem(i, 1, px);
        table_->setItem(i, 2, sz);
    }
    set_loading(false);
}

QDialog* TradeTapeWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Trades");
    auto* form = new QFormLayout(dlg);

    auto* ex = new QComboBox(dlg);
    ex->addItems({"kraken", "hyperliquid"});
    ex->setCurrentText(exchange_);
    form->addRow("Exchange", ex);

    auto* pair = new QLineEdit(dlg);
    pair->setText(pair_);
    pair->setPlaceholderText("e.g. BTC/USD");
    form->addRow("Pair", pair);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(5, 200);
    spin->setValue(max_rows_);
    form->addRow("Max rows", spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, ex, pair, spin]() {
        QJsonObject cfg;
        cfg.insert("exchange", ex->currentText());
        cfg.insert("pair", pair->text().trimmed());
        cfg.insert("max_rows", spin->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void TradeTapeWidget::on_theme_changed() {
    apply_styles();
}

void TradeTapeWidget::apply_styles() {
    table_->setStyleSheet(
        QString("QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:10px;border:none;}"
                "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
                "padding:2px 4px;font-size:9px;font-weight:bold;}"
                "QTableWidget::item{padding:2px 4px;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
                 ui::colors::TEXT_TERTIARY()));
}

} // namespace fincept::screens::widgets
