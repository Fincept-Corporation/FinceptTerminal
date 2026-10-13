#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/DashboardFormat.h"
#include "ui/theme/Theme.h"

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

QuoteTableWidget::QuoteTableWidget(const QString& title, const QStringList& symbols,
                                   const QMap<QString, QString>& label_map, int price_decimals,
                                   const QString& accent_color, QWidget* parent)
    : BaseWidget(title, parent, accent_color),
      symbols_(symbols),
      label_map_(label_map),
      price_decimals_(price_decimals) {
    table_ = new ui::DataTable;
    table_->set_headers({tr("SYMBOL"), tr("PRICE"), tr("CHG"), tr("CHG%")});
    table_->set_column_widths({130, 100, 80, 70});
    table_->setToolTip(tr("Double-click a row to open it in Equity Research · right-click for more"));
    content_layout()->addWidget(table_);

    // Row → symbol link. The raw symbol rides on the SYMBOL cell (the cell text
    // is the display label, e.g. "S&P 500" for ^GSPC).
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int /*col*/) {
        if (auto* it = table_->item(row, 0))
            open_symbol(it->data(Qt::UserRole).toString());
    });
    install_symbol_menu(table_, 0, Qt::UserRole);

    connect(this, &BaseWidget::refresh_requested, this, &QuoteTableWidget::refresh_data);

    apply_styles();
    set_loading(true);
}

void QuoteTableWidget::apply_styles() {
    // QuoteTableWidget delegates styling to DataTable; nothing extra needed.
}

void QuoteTableWidget::on_theme_changed() {
    apply_styles();
}

void QuoteTableWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    if (table_)
        table_->set_headers({tr("SYMBOL"), tr("PRICE"), tr("CHG"), tr("CHG%")});
}

void QuoteTableWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void QuoteTableWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void QuoteTableWidget::refresh_data() {
    // User-triggered refresh: force-bypass min_interval so a user tap on the
    // refresh button always kicks a fetch. Producer-side rate limit still
    // applies, so rage-clicking can't hammer upstream.
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(symbols_.size());
    for (const auto& sym : symbols_)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true);
}

void QuoteTableWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), symbols_.size());
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), symbols_.size());
            // One render per delivery burst, not one per symbol (P9/P10).
            schedule_render([this]() { render_from_cache(); });
        });
    }
    hub_active_ = true;
}

void QuoteTableWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
    // Keep row_cache_ so a quick hide/show doesn't flash an empty table —
    // the first hub delivery on re-subscribe will refresh it anyway.
}

void QuoteTableWidget::render_from_cache() {
    table_->clear_data();
    for (const auto& sym : symbols_) {
        auto it = row_cache_.constFind(sym);
        if (it == row_cache_.constEnd())
            continue;
        const auto& q = it.value();
        // Look the label up by the subscribed symbol, not the one echoed back
        // by the feed (which may differ in case / normalisation).
        const QString display_name = label_map_.value(sym, sym);
        // Grouped, and never fewer decimals than the preset asked for — but FX
        // keeps its pips and sub-unit prices their significant digits.
        const int decimals = std::max(price_decimals_, dash_fmt::price_decimals(q.price, sym));
        const QString price_str = q.price > 0 ? dash_fmt::grouped(q.price, decimals) : QStringLiteral("--");
        const QString chg_str = (q.change > 0 ? QStringLiteral("+") : QString()) + dash_fmt::grouped(q.change, decimals);
        const QString pct_str = dash_fmt::signed_pct(q.change_pct);

        table_->add_row({display_name, price_str, chg_str, pct_str});
        int row = table_->rowCount() - 1;
        if (auto* sym_item = table_->item(row, 0))
            sym_item->setData(Qt::UserRole, sym);
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
        table_->set_cell_color(row, 3, ui::change_color(q.change_pct));
    }
}

} // namespace fincept::screens::widgets
