#include "screens/dashboard/widgets/CryptoTickerWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "trading/TradingTypes.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>

namespace fincept::screens::widgets {

namespace {
const QStringList kDefaultPairs = {"BTC/USD", "ETH/USD", "SOL/USD"};
constexpr const char* kDefaultExchange = "kraken";
} // namespace

CryptoTickerWidget::CryptoTickerWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("CRYPTO TICKER", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject CryptoTickerWidget::config() const {
    QJsonObject o;
    o.insert("exchange", exchange_);
    QJsonArray arr;
    for (const auto& p : pairs_)
        arr.append(p);
    o.insert("pairs", arr);
    return o;
}

void CryptoTickerWidget::apply_config(const QJsonObject& cfg) {
    exchange_ = cfg.value("exchange").toString();
    if (exchange_.isEmpty())
        exchange_ = kDefaultExchange;

    QStringList next;
    const QJsonArray arr = cfg.value("pairs").toArray();
    for (const auto& v : arr) {
        const QString s = v.toString().trimmed().toUpper();
        if (!s.isEmpty())
            next.append(s);
    }
    if (next.isEmpty())
        next = kDefaultPairs;
    pairs_ = next;

    build_rows();
    if (isVisible())
        hub_resubscribe();
}

void CryptoTickerWidget::build_rows() {
    auto* vl = content_layout();
    while (QLayoutItem* it = vl->takeAt(0)) {
        if (auto* w = it->widget())
            w->deleteLater();
        else if (auto* sub = it->layout()) {
            while (QLayoutItem* ci = sub->takeAt(0)) {
                if (auto* cw = ci->widget())
                    cw->deleteLater();
                delete ci;
            }
            delete sub;
        }
        delete it;
    }
    rows_.clear();

    auto* hdr = new QLabel(exchange_.toUpper());
    hdr->setStyleSheet(
        QString("color:%1;font-size:9px;background:transparent;padding:2px 0;")
            .arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(hdr);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    for (int i = 0; i < pairs_.size(); ++i) {
        const QString& pair = pairs_[i];
        Row r;
        r.symbol = new QLabel(pair);
        r.price = new QLabel("—");
        r.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change = new QLabel("—");
        r.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.symbol, i, 0);
        grid->addWidget(r.price, i, 1);
        grid->addWidget(r.change, i, 2);
        rows_.insert(pair, r);
    }
    vl->addLayout(grid);
    vl->addStretch(1);
    apply_styles();
}

void CryptoTickerWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    for (const auto& pair : pairs_) {
        const QString topic =
            QStringLiteral("ws:") + exchange_ + QStringLiteral(":ticker:") + pair;
        const QString pair_copy = pair;
        hub.subscribe(this, topic, [this, pair_copy](const QVariant& v) {
            if (!v.canConvert<fincept::trading::TickerData>())
                return;
            on_ticker(pair_copy, v.value<fincept::trading::TickerData>());
        });
    }
    hub_active_ = true;
}

void CryptoTickerWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void CryptoTickerWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void CryptoTickerWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void CryptoTickerWidget::on_ticker(const QString& pair, const fincept::trading::TickerData& t) {
    auto it = rows_.find(pair);
    if (it == rows_.end())
        return;
    Row& r = it.value();
    r.price->setText(QString::number(t.last, 'f', t.last < 10 ? 4 : 2));
    const QString sign = t.percentage >= 0 ? "+" : "";
    r.change->setText(QString("%1%2%").arg(sign).arg(t.percentage, 0, 'f', 2));
    const QColor col = t.percentage >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    r.change->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;").arg(col.name()));
    set_loading(false);
}

QDialog* CryptoTickerWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Crypto Ticker");
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    combo->addItems({"kraken", "hyperliquid"});
    combo->setCurrentText(exchange_);
    form->addRow("Exchange", combo);

    auto* edit = new QLineEdit(dlg);
    edit->setText(pairs_.join(", "));
    edit->setPlaceholderText("e.g. BTC/USD, ETH/USD, SOL/USD");
    form->addRow("Pairs", edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, combo, edit]() {
        QJsonObject cfg;
        cfg.insert("exchange", combo->currentText());
        QJsonArray arr;
        const QStringList pieces = edit->text().split(',', Qt::SkipEmptyParts);
        for (const auto& p : pieces) {
            const QString s = p.trimmed().toUpper();
            if (!s.isEmpty())
                arr.append(s);
        }
        cfg.insert("pairs", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void CryptoTickerWidget::on_theme_changed() {
    apply_styles();
}

void CryptoTickerWidget::apply_styles() {
    const QString sym_css =
        QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    const QString price_css =
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY());
    const QString chg_css =
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY());
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        if (it->symbol)
            it->symbol->setStyleSheet(sym_css);
        if (it->price)
            it->price->setStyleSheet(price_css);
        if (it->change && it->change->text() == "—")
            it->change->setStyleSheet(chg_css);
    }
}

} // namespace fincept::screens::widgets
