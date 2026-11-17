#include "screens/dashboard/widgets/MarketQuoteStripWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
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
// A fresh strip shows the five largest names of the user's market region.
QStringList quote_strip_default_symbols() {
    return core::market::MarketProfileStore::instance().current().movers.mid(0, 5);
}

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

QString quote_strip_topic(const QString& sym) {
    return QStringLiteral("market:quote:") + sym;
}
} // namespace

MarketQuoteStripWidget::MarketQuoteStripWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("QUOTE STRIP"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);
    enable_symbol_link(/*follows=*/false); // clicking a row feeds the link group

    // Title-bar refresh (the retry the error overlay points at) was not wired
    // here. Force past min_interval; the producer's rate limit still applies.
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        QStringList topics;
        topics.reserve(symbols_.size());
        for (const auto& sym : symbols_)
            topics.append(quote_strip_topic(sym));
        if (!topics.isEmpty())
            datahub::DataHub::instance().request(topics, /*force=*/true);
    });

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
        next = quote_strip_default_symbols();

    symbols_ = next;
    build_rows();
    untrack_topics(); // fresh "—" rows: forget the old rows' data / error state

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
    QStringList topics;
    topics.reserve(symbols_.size());
    for (const auto& sym : symbols_)
        topics.append(quote_strip_topic(sym));
    // BaseWidget drives the overlay: it covers the strip only until the first
    // quote lands (rows already showing prices are never covered again on a
    // re-show), and a symbol whose quote errored counts as resolved — its row
    // shows "—" with the reason.
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& sym : symbols_) {
        const QString topic = quote_strip_topic(sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<fincept::services::QuoteData>())
                return;
            on_quote(sym, v.value<fincept::services::QuoteData>());
        });
    }
    hub_active_ = true;
}

void MarketQuoteStripWidget::on_topic_errors_changed() {
    for (const auto& sym : symbols_)
        apply_row_error(sym);
}

void MarketQuoteStripWidget::apply_row_error(const QString& sym) {
    auto it = rows_.find(sym);
    if (it == rows_.end())
        return;
    Row& r = it.value();
    const QString topic = quote_strip_topic(sym);
    const QString tip = topic_error_tooltip(topic);
    // No error: back to the row's own link tooltip (set by link_symbol()).
    const QString row_tip = tip.isEmpty() ? r.symbol->toolTip() : tip;
    r.price->setToolTip(row_tip);
    r.change->setToolTip(row_tip);
    if (!tip.isEmpty() && !topic_has_data(topic)) {
        // Failed with nothing to show: mark it (the pending placeholder is the same dash).
        r.price->setText(failed_value_text());
        r.change->setText(failed_value_text());
        quote_strip_set_dir(r.change, "fail");
    }
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

void MarketQuoteStripWidget::on_quote(const QString& sym, const fincept::services::QuoteData& q) {
    // Keyed by the subscribed symbol: the feed's echoed q.symbol may differ in
    // case / normalisation, which left such a row on "—" forever.
    auto it = rows_.find(sym);
    if (it == rows_.end())
        return;
    Row& r = it.value();
    // Shared dashboard price format (pips kept, 4-6 decimals below 1, grouping).
    r.price->setText(dash_fmt::price(q.price, sym));
    const QString sign = q.change_pct >= 0 ? "+" : "";
    r.change->setText(QString("%1%2%").arg(sign).arg(q.change_pct, 0, 'f', 2));
    quote_strip_set_dir(r.change, q.change_pct >= 0 ? "up" : "down");
    const bool had_error = !topic_error(quote_strip_topic(sym)).isEmpty();
    note_topic_value(quote_strip_topic(sym));
    if (had_error)
        apply_row_error(sym); // drop the failure tooltip
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
    set_css_cached(rows_host_, 
        QString("QLabel#quoteStripSymbol{color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;background:transparent;}"
                "QLabel#quoteStripPrice{color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;}"
                "QLabel#quoteStripChange{color:%2;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;}"
                "QLabel#quoteStripChange[dir=\"up\"]{color:%3;}"
                "QLabel#quoteStripChange[dir=\"down\"]{color:%4;}"
                "QLabel#quoteStripChange[dir=\"fail\"]{color:%5;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(), ui::colors::POSITIVE(),
                 ui::colors::NEGATIVE(), ui::colors::WARNING()));
}

void MarketQuoteStripWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("QUOTE STRIP"));
    // Rows carry only symbols and numbers — nothing to translate. Rebuilding
    // them here (as this used to) blanked every price back to "—" until the
    // next hub publish, without re-delivering the cached quotes.
}

} // namespace fincept::screens::widgets
