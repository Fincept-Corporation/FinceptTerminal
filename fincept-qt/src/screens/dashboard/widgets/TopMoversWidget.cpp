#include "screens/dashboard/widgets/TopMoversWidget.h"

#include "ui/theme/Theme.h"

namespace fincept::screens::widgets {

TopMoversWidget::TopMoversWidget(QWidget* parent) : BaseWidget("TOP MOVERS", parent) {
    // Tab toggle
    auto* tab_bar = new QWidget;
    tab_bar->setFixedHeight(26);
    auto* tl = new QHBoxLayout(tab_bar);
    tl->setContentsMargins(4, 2, 4, 2);
    tl->setSpacing(0);

    auto active_style = [](const QString& color) {
        return QString("QPushButton { background: %1; color: %2; border: none; "
                       "font-size: 9px; font-weight: bold; padding: 4px; }")
            .arg(color, ui::colors::BG_BASE);
    };
    auto inactive_style = QString("QPushButton { background: %1; color: %2; border: none; "
                                  "font-size: 9px; font-weight: bold; padding: 4px; }")
                              .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_TERTIARY);

    gainers_btn_ = new QPushButton(QString(QChar(0x25B2)) + " GAINERS");
    losers_btn_ = new QPushButton(QString(QChar(0x25BC)) + " LOSERS");

    gainers_btn_->setStyleSheet(active_style(ui::colors::POSITIVE));
    losers_btn_->setStyleSheet(inactive_style);

    connect(gainers_btn_, &QPushButton::clicked, this, [this]() { show_tab(true); });
    connect(losers_btn_, &QPushButton::clicked, this, [this]() { show_tab(false); });

    tl->addWidget(gainers_btn_, 1);
    tl->addWidget(losers_btn_, 1);

    content_layout()->addWidget(tab_bar);

    // Table
    table_ = new ui::DataTable;
    table_->set_headers({"SYMBOL", "PRICE", "CHG%"});
    table_->set_column_widths({100, 90, 80});
    content_layout()->addWidget(table_);

    connect(this, &BaseWidget::refresh_requested, this, &TopMoversWidget::refresh_data);

    set_loading(true);
    refresh_data();
}

void TopMoversWidget::refresh_data() {
    set_loading(true);

    services::MarketDataService::instance().fetch_quotes(
        services::MarketDataService::mover_symbols(), [this](bool ok, QVector<services::QuoteData> quotes) {
            set_loading(false);
            if (!ok || quotes.isEmpty())
                return;

            all_quotes_ = quotes;
            // Sort by change_pct descending
            std::sort(all_quotes_.begin(), all_quotes_.end(),
                      [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });

            show_tab(showing_gainers_);
        });
}

void TopMoversWidget::show_tab(bool gainers) {
    showing_gainers_ = gainers;

    auto active_g = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::POSITIVE, ui::colors::BG_BASE);
    auto active_l = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::NEGATIVE, ui::colors::BG_BASE);
    auto inactive = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_TERTIARY);

    gainers_btn_->setStyleSheet(gainers ? active_g : inactive);
    losers_btn_->setStyleSheet(gainers ? inactive : active_l);

    table_->clear_data();

    QVector<services::QuoteData> filtered;
    for (const auto& q : all_quotes_) {
        if (gainers && q.change_pct > 0)
            filtered.append(q);
        if (!gainers && q.change_pct < 0)
            filtered.append(q);
    }

    // Show top 6
    int count = std::min(filtered.size(), qsizetype(6));
    for (int i = 0; i < count; ++i) {
        const auto& q = gainers ? filtered[i] : filtered[filtered.size() - 1 - i];
        table_->add_row({q.symbol, QString("$%1").arg(q.price, 0, 'f', 2),
                         QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2)});
        int row = table_->rowCount() - 1;
        table_->set_cell_color(row, 2, ui::change_color(q.change_pct));
    }
}

} // namespace fincept::screens::widgets
