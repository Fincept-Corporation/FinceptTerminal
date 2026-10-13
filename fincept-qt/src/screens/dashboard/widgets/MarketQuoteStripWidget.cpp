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
#include <QStyle>

namespace fincept::screens::widgets {

namespace {
const QStringList kDefaultSymbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA"};

// Colour of the change label comes from a "dir" property matched in the rows
// host's stylesheet; re-polish only when the sign flips instead of re-parsing
// CSS on every quote (CLAUDE.md P7).
void quote_strip_set_dir(QLabel* label, const char* dir) {
    if (label->property("dir").toString() == QLatin1String(dir))
        return;
    label->setProperty("dir", QString::fromLatin1(dir));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

MarketQuoteStripWidget::MarketQuoteStripWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("QUOTE STRIP"), parent) {
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
        // De-duplicate: rows_ is keyed by symbol, so a repeat produced a second
        // visible row that never updated (and a loading overlay that could not
        // reach 100%).
        if (!s.isEmpty() && !next.contains(s))
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

    // One host carries the stylesheet for every row label (objectName + "dir"
    // property selectors) — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName("quoteStripRows");
    auto* grid = new QGridLayout(rows_host_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(3);

    for (int i = 0; i < symbols_.size(); ++i) {
        const QString& sym = symbols_[i];
        Row r;
        r.symbol = new QLabel(sym, rows_host_);
        r.symbol->setObjectName("quoteStripSymbol");
        r.price = new QLabel("—", rows_host_);
        r.price->setObjectName("quoteStripPrice");
        r.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.change = new QLabel("—", rows_host_);
        r.change->setObjectName("quoteStripChange");
        r.change->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        grid->addWidget(r.symbol, i, 0);
        grid->addWidget(r.price, i, 1);
        grid->addWidget(r.change, i, 2);
        link_symbol(r.symbol, sym);
        link_symbol(r.price, sym);
        link_symbol(r.change, sym);
        rows_.insert(sym, r);
    }
    vl->addWidget(rows_host_);
    vl->addStretch(1);
    apply_styles();
}

void MarketQuoteStripWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    // Drop the cross-config received set so a re-subscribe after a config
    // change shows the loading overlay until each new symbol publishes once.
    received_.clear();
    set_loading_progress(received_.size(), symbols_.size());
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
    quote_strip_set_dir(r.change, q.change_pct >= 0 ? "up" : "down");
    received_.insert(q.symbol);
    set_loading_progress(received_.size(), symbols_.size());
}

QDialog* MarketQuoteStripWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle(tr("Configure — Quote Strip"));
    auto* form = new QFormLayout(dlg);

    auto* edit = new QLineEdit(dlg);
    edit->setText(symbols_.join(", "));
    edit->setPlaceholderText(tr("e.g. AAPL, MSFT, GOOGL"));
    form->addRow(tr("Symbols"), edit);

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
    if (!rows_host_)
        return; // ctor runs this before the first build_rows()
    // A change label with no "dir" yet (still "—") keeps the tertiary colour.
    rows_host_->setStyleSheet(
        QString("QLabel#quoteStripSymbol{color:%1;font-size:11px;font-weight:700;background:transparent;}"
                "QLabel#quoteStripPrice{color:%1;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#quoteStripChange{color:%2;font-size:11px;font-weight:600;background:transparent;}"
                "QLabel#quoteStripChange[dir=\"up\"]{color:%3;}"
                "QLabel#quoteStripChange[dir=\"down\"]{color:%4;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE()));
}

void MarketQuoteStripWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("QUOTE STRIP"));
    // Rows carry only symbols and numbers — nothing to translate. Rebuilding
    // them here (as this used to) blanked every price back to "—" until the
    // next hub publish, without re-delivering the cached quotes.
}

} // namespace fincept::screens::widgets
