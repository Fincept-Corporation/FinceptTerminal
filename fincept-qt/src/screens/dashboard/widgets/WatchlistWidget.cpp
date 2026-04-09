#include "screens/dashboard/widgets/WatchlistWidget.h"

#include "ui/theme/Theme.h"

#include <QLabel>

namespace fincept::screens::widgets {

WatchlistWidget::WatchlistWidget(QWidget* parent)
    : BaseWidget("WATCHLIST", parent, ui::colors::INFO),
      symbols_({"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "TSLA", "META", "JPM"}) {

    auto* vl = content_layout();

    // Symbol input bar
    auto* input_row = new QWidget(this);
    auto* irl = new QHBoxLayout(input_row);
    irl->setContentsMargins(4, 4, 4, 4);
    irl->setSpacing(4);

    symbols_label_ = new QLabel("SYMBOLS:");
    irl->addWidget(symbols_label_);

    symbols_input_ = new QLineEdit(symbols_.join(", "));
    irl->addWidget(symbols_input_, 1);

    go_btn_ = new QPushButton("GO");
    go_btn_->setFixedWidth(32);
    connect(go_btn_, &QPushButton::clicked, this, [this]() {
        QString text = symbols_input_->text().trimmed().toUpper();
        symbols_.clear();
        for (auto& s : text.split(",")) {
            QString trimmed = s.trimmed();
            if (!trimmed.isEmpty())
                symbols_ << trimmed;
        }
        refresh_data();
    });
    irl->addWidget(go_btn_);

    vl->addWidget(input_row);

    // Table
    table_ = new ui::DataTable;
    table_->set_headers({"SYMBOL", "PRICE", "CHG", "CHG%"});
    table_->set_column_widths({100, 90, 80, 70});
    vl->addWidget(table_);

    connect(this, &BaseWidget::refresh_requested, this, &WatchlistWidget::refresh_data);

    apply_styles();
    set_loading(true);
    refresh_data();
}

void WatchlistWidget::apply_styles() {
    symbols_label_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                      .arg(ui::colors::TEXT_TERTIARY()));
    symbols_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "font-size: 10px; padding: 2px 6px; font-family: Consolas; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER()));
    go_btn_->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: none; "
                                   "font-size: 9px; font-weight: bold; padding: 3px; }"
                                   "QPushButton:hover { background: %1; }")
                               .arg(ui::colors::AMBER(), ui::colors::BG_BASE()));
}

void WatchlistWidget::on_theme_changed() {
    apply_styles();
}

void WatchlistWidget::refresh_data() {
    if (symbols_.isEmpty())
        return;
    set_loading(true);

    services::MarketDataService::instance().fetch_quotes(symbols_,
                                                         [this](bool ok, QVector<services::QuoteData> quotes) {
                                                             set_loading(false);
                                                             if (!ok || quotes.isEmpty()) {
                                                                 if (table_->rowCount() == 0)
                                                                     set_error("Failed to fetch watchlist data.");
                                                                 return;
                                                             }
                                                             populate(quotes);
                                                         });
}

void WatchlistWidget::populate(const QVector<services::QuoteData>& quotes) {
    table_->clear_data();

    for (const auto& q : quotes) {
        table_->add_row({q.symbol, QString("$%1").arg(q.price, 0, 'f', 2),
                         QString("%1%2").arg(q.change >= 0 ? "+" : "").arg(q.change, 0, 'f', 2),
                         QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2)});
        int row = table_->rowCount() - 1;
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
        table_->set_cell_color(row, 3, ui::change_color(q.change_pct));
    }
}

} // namespace fincept::screens::widgets
