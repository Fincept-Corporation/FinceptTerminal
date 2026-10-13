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
#include <QTimer>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {
// A busy pair delivers a trade batch every ~100 ms; never redraw faster.
constexpr qint64 kTradeTapeMinRenderMs = 100;
} // namespace

TradeTapeWidget::TradeTapeWidget(const QJsonObject& cfg, QWidget* parent) : BaseWidget(tr("TRADES"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({tr("Time"), tr("Price"), tr("Size")});
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
    render_all();

    if (isVisible())
        hub_resubscribe();
}

void TradeTapeWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QString topic = QStringLiteral("ws:") + exchange_ + QStringLiteral(":trades:") + pair_;
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
    using fincept::trading::TradeData;
    if (v.metaType() == QMetaType::fromType<QVector<TradeData>>()) {
        // Batch in arrival order: prepending each leaves the newest on top.
        const auto batch = v.value<QVector<TradeData>>();
        for (const auto& td : batch)
            prepend_trade(td);
    } else if (v.canConvert<TradeData>()) {
        prepend_trade(v.value<TradeData>());
    } else {
        return;
    }
    request_render();
}

void TradeTapeWidget::prepend_trade(const fincept::trading::TradeData& td) {
    Trade t;
    t.when = (td.timestamp > 0) ? QDateTime::fromMSecsSinceEpoch(td.timestamp) : QDateTime::currentDateTime();
    t.side = td.side;
    t.price = td.price;
    t.amount = td.amount;
    trades_.prepend(t);
    while (trades_.size() > max_rows_)
        trades_.removeLast();
    unrendered_ = std::min(unrendered_ + 1, max_rows_);
}

void TradeTapeWidget::request_render() {
    if (render_armed_)
        return;
    render_armed_ = true;
    const qint64 since = last_render_.isValid() ? last_render_.elapsed() : kTradeTapeMinRenderMs;
    const int delay = since >= kTradeTapeMinRenderMs ? 0 : static_cast<int>(kTradeTapeMinRenderMs - since);
    // `this` as context: the pending render is dropped if the widget dies first.
    QTimer::singleShot(delay, this, [this]() {
        render_armed_ = false;
        render();
    });
}

void TradeTapeWidget::fill_row(int row, const Trade& t, const QColor& buy, const QColor& sell) {
    auto cell = [this, row](int column) {
        QTableWidgetItem* item = table_->item(row, column);
        if (!item) {
            item = new QTableWidgetItem;
            table_->setItem(row, column, item);
        }
        return item;
    };
    const QColor& col = (t.side.compare("buy", Qt::CaseInsensitive) == 0) ? buy : sell;
    cell(0)->setText(t.when.toString("HH:mm:ss"));
    QTableWidgetItem* px = cell(1);
    px->setText(QString::number(t.price, 'f', 2));
    px->setForeground(col);
    px->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QTableWidgetItem* sz = cell(2);
    sz->setText(QString::number(t.amount, 'f', 4));
    sz->setForeground(col);
    sz->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void TradeTapeWidget::render() {
    // The table mirrors trades_ as of the last render; since then `fresh` trades
    // were prepended and the tail trimmed to max_rows_. Shift the existing rows
    // down instead of re-creating every cell (up to 600 items per tick).
    const int n = static_cast<int>(trades_.size());
    const int fresh = std::min(unrendered_, n);
    if (fresh >= max_rows_ || fresh >= n) {
        render_all(); // every visible row is new anyway
        return;
    }
    unrendered_ = 0;
    last_render_.start();
    if (fresh > 0) {
        auto* model = table_->model();
        model->insertRows(0, fresh);
        const int overflow = table_->rowCount() - n;
        if (overflow > 0)
            model->removeRows(n, overflow);
        const QColor buy = ui::colors::POSITIVE();
        const QColor sell = ui::colors::NEGATIVE();
        for (int i = 0; i < fresh; ++i)
            fill_row(i, trades_[i], buy, sell);
    }
    set_loading(false);
}

void TradeTapeWidget::render_all() {
    unrendered_ = 0;
    last_render_.start();
    const int n = static_cast<int>(trades_.size());
    table_->setRowCount(n);
    const QColor buy = ui::colors::POSITIVE();
    const QColor sell = ui::colors::NEGATIVE();
    for (int i = 0; i < n; ++i)
        fill_row(i, trades_[i], buy, sell);
    set_loading(false);
}

QDialog* TradeTapeWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Trades"));
    auto* form = new QFormLayout(dlg);

    auto* ex = new QComboBox(dlg);
    ex->addItems({"kraken", "hyperliquid"});
    ex->setCurrentText(exchange_);
    form->addRow(tr("Exchange"), ex);

    auto* pair = new QLineEdit(dlg);
    pair->setText(pair_);
    pair->setPlaceholderText(tr("e.g. BTC/USD"));
    form->addRow(tr("Pair"), pair);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(5, 200);
    spin->setValue(max_rows_);
    form->addRow(tr("Max rows"), spin);

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
    render_all(); // existing rows keep their buy/sell colours otherwise
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

void TradeTapeWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("TRADES"));
    render_all(); // re-renders header + side labels in the new language
}

} // namespace fincept::screens::widgets
