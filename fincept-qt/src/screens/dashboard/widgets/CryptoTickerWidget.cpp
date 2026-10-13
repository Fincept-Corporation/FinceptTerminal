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
#include <QStyle>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {
const QStringList kDefaultPairs = {"BTC/USD", "ETH/USD", "SOL/USD"};
constexpr const char* kDefaultExchange = "kraken";

// The change label's colour is driven by a "dir" property matched in the rows
// host's stylesheet. Ticks arrive at up to 20 Hz per pair; a per-tick
// setStyleSheet() re-parsed CSS every time, while the colour only changes when
// the sign flips — so re-polish only then (CLAUDE.md P7).
void crypto_ticker_set_dir(QLabel* label, const char* dir) {
    if (label->property("dir").toString() == QLatin1String(dir))
        return;
    label->setProperty("dir", QString::fromLatin1(dir));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

CryptoTickerWidget::CryptoTickerWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("CRYPTO TICKER"), parent) {
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
        if (!s.isEmpty() && !next.contains(s)) // rows_ is keyed by pair
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

    // One host carries the stylesheet for every label below (objectName +
    // "dir" property selectors) — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName("cryptoTickerRows");
    auto* host_layout = new QVBoxLayout(rows_host_);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->setSpacing(vl->spacing());

    auto* hdr = new QLabel(exchange_.toUpper(), rows_host_);
    hdr->setObjectName("cryptoTickerHdr");
    host_layout->addWidget(hdr);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);
    for (int i = 0; i < pairs_.size(); ++i) {
        const QString& pair = pairs_[i];
        Row r;
        r.symbol = new QLabel(pair, rows_host_);
        r.symbol->setObjectName("cryptoTickerSym");
        r.price = new QLabel("—", rows_host_);
        r.price->setObjectName("cryptoTickerPrice");
        r.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change = new QLabel("—", rows_host_);
        r.change->setObjectName("cryptoTickerChg");
        r.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(r.symbol, i, 0);
        grid->addWidget(r.price, i, 1);
        grid->addWidget(r.change, i, 2);
        rows_.insert(pair, r);
    }
    host_layout->addLayout(grid);
    vl->addWidget(rows_host_);
    vl->addStretch(1);
    apply_styles();
}

void CryptoTickerWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    received_.clear();
    set_loading_progress(received_.size(), pairs_.size());
    for (const auto& pair : pairs_) {
        const QString topic = QStringLiteral("ws:") + exchange_ + QStringLiteral(":ticker:") + pair;
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
    crypto_ticker_set_dir(r.change, t.percentage >= 0 ? "up" : "down");
    received_.insert(pair);
    set_loading_progress(received_.size(), pairs_.size());
}

QDialog* CryptoTickerWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Crypto Ticker"));
    auto* form = new QFormLayout(dlg);

    auto* combo = new QComboBox(dlg);
    combo->addItems({"kraken", "hyperliquid"});
    combo->setCurrentText(exchange_);
    form->addRow(tr("Exchange"), combo);

    auto* edit = new QLineEdit(dlg);
    edit->setText(pairs_.join(", "));
    edit->setPlaceholderText(tr("e.g. BTC/USD, ETH/USD, SOL/USD"));
    form->addRow(tr("Pairs"), edit);

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
    if (!rows_host_)
        return; // ctor runs this before the first build_rows()
    // A change label with no "dir" yet (still "—") keeps the tertiary colour.
    rows_host_->setStyleSheet(
        QString("QLabel#cryptoTickerHdr{color:%1;font-size:9px;background:transparent;padding:2px 0;}"
                "QLabel#cryptoTickerSym{color:%2;font-size:11px;font-weight:700;background:transparent;}"
                "QLabel#cryptoTickerPrice{color:%2;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#cryptoTickerChg{color:%1;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#cryptoTickerChg[dir=\"up\"]{color:%3;}"
                "QLabel#cryptoTickerChg[dir=\"down\"]{color:%4;}")
            .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE()));
}

void CryptoTickerWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("CRYPTO TICKER"));
    // No translatable text in the rows; rebuilding them here blanked every
    // price until the next websocket tick.
}

} // namespace fincept::screens::widgets
