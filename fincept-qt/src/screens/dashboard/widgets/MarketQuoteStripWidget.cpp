#include "screens/dashboard/widgets/MarketQuoteStripWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/theme/Theme.h"

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
const QStringList kDefaultSymbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA"};
} // namespace

MarketQuoteStripWidget::MarketQuoteStripWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("QUOTE STRIP", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject MarketQuoteStripWidget::config() const {
    QJsonObject o;
    QJsonArray arr;
    for (const auto& s : symbols_)
        arr.append(s);
    o.insert("symbols", arr);
    return o;
}

void MarketQuoteStripWidget::apply_config(const QJsonObject& cfg) {
    QStringList next;
    const QJsonArray arr = cfg.value("symbols").toArray();
    for (const auto& v : arr) {
        const QString s = v.toString().trimmed().toUpper();
        if (!s.isEmpty())
            next.append(s);
    }
    if (next.isEmpty())
        next = kDefaultSymbols;

    symbols_ = next;
    build_rows();

    if (isVisible())
        hub_resubscribe();
}

void MarketQuoteStripWidget::build_rows() {
    auto* vl = content_layout();

    // Clear any existing row widgets
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

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);

    for (int i = 0; i < symbols_.size(); ++i) {
        const QString& sym = symbols_[i];
        Row r;
        r.symbol = new QLabel(sym);
        r.symbol->setObjectName("quoteStripSymbol");
        r.price = new QLabel("—");
        r.price->setObjectName("quoteStripPrice");
        r.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change = new QLabel("—");
        r.change->setObjectName("quoteStripChange");
        r.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        grid->addWidget(r.symbol, i, 0);
        grid->addWidget(r.price, i, 1);
        grid->addWidget(r.change, i, 2);
        rows_.insert(sym, r);
    }
    vl->addLayout(grid);
    vl->addStretch(1);
    apply_styles();
}

void MarketQuoteStripWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this](const QVariant& v) {
            if (!v.canConvert<fincept::services::QuoteData>())
                return;
            on_quote(v.value<fincept::services::QuoteData>());
        });
    }
    hub_active_ = true;
}

void MarketQuoteStripWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void MarketQuoteStripWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void MarketQuoteStripWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void MarketQuoteStripWidget::on_quote(const fincept::services::QuoteData& q) {
    auto it = rows_.find(q.symbol);
    if (it == rows_.end())
        return;
    Row& r = it.value();
    r.price->setText(QString::number(q.price, 'f', 2));
    const QString sign = q.change_pct >= 0 ? "+" : "";
    r.change->setText(QString("%1%2%").arg(sign).arg(q.change_pct, 0, 'f', 2));
    const QColor col = q.change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
    r.change->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:600;background:transparent;").arg(col.name()));
    set_loading(false);
}

QDialog* MarketQuoteStripWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Quote Strip");
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(symbols_.join(", "));
    edit->setPlaceholderText("e.g. AAPL, MSFT, GOOGL");
    form->addRow("Symbols", edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, edit]() {
        QJsonArray arr;
        const QStringList pieces = edit->text().split(',', Qt::SkipEmptyParts);
        for (const auto& p : pieces) {
            const QString s = p.trimmed().toUpper();
            if (!s.isEmpty())
                arr.append(s);
        }
        QJsonObject cfg;
        cfg.insert("symbols", arr);
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void MarketQuoteStripWidget::on_theme_changed() {
    apply_styles();
}

void MarketQuoteStripWidget::apply_styles() {
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
