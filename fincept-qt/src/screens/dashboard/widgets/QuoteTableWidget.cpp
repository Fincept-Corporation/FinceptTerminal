#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

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
    table_->set_headers({"SYMBOL", "PRICE", "CHG", "CHG%"});
    table_->set_column_widths({130, 100, 80, 70});
    content_layout()->addWidget(table_);

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
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading(false);
            render_from_cache();
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
        QString display_name = label_map_.value(q.symbol, q.symbol);
        QString price_str = QString::number(q.price, 'f', price_decimals_);
        double chg_abs = q.change;
        QString chg_str = QString("%1%2").arg(chg_abs >= 0 ? "+" : "").arg(chg_abs, 0, 'f', price_decimals_);
        QString pct_str = QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2);

        table_->add_row({display_name, price_str, chg_str, pct_str});
        int row = table_->rowCount() - 1;
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
        table_->set_cell_color(row, 3, ui::change_color(q.change_pct));
    }
}


void QuoteTableWidget::populate(const QVector<services::QuoteData>& quotes) {
    table_->clear_data();

    for (const auto& q : quotes) {
        QString display_name = label_map_.value(q.symbol, q.symbol);
        QString price_str = QString::number(q.price, 'f', price_decimals_);
        double chg_abs = q.change;
        QString chg_str = QString("%1%2").arg(chg_abs >= 0 ? "+" : "").arg(chg_abs, 0, 'f', price_decimals_);
        QString pct_str = QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2);

        table_->add_row({display_name, price_str, chg_str, pct_str});
        int row = table_->rowCount() - 1;
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
        table_->set_cell_color(row, 3, ui::change_color(q.change_pct));
    }
}

} // namespace fincept::screens::widgets
