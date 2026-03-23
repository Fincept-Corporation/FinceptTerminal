#include "screens/dashboard/widgets/QuoteTableWidget.h"

#include "ui/theme/Theme.h"

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

    set_loading(true);
    refresh_data();
}

void QuoteTableWidget::refresh_data() {
    set_loading(true);

    services::MarketDataService::instance().fetch_quotes(
        symbols_, [this](bool ok, QVector<services::QuoteData> quotes) {
            set_loading(false);
            if (!ok || quotes.isEmpty()) {
                // If fetch failed, leave table empty with a message
                if (table_->rowCount() == 0) {
                    set_error("Failed to fetch data. Check Python/yfinance.");
                }
                return;
            }
            populate(quotes);
        });
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
