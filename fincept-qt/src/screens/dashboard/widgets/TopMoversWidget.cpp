#include "screens/dashboard/widgets/TopMoversWidget.h"

#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

namespace fincept::screens::widgets {

TopMoversWidget::TopMoversWidget(QWidget* parent) : BaseWidget("TOP MOVERS", parent) {
    // Tab toggle
    auto* tab_bar = new QWidget(this);
    tab_bar->setFixedHeight(26);
    auto* tl = new QHBoxLayout(tab_bar);
    tl->setContentsMargins(4, 2, 4, 2);
    tl->setSpacing(0);

    gainers_btn_ = new QPushButton(QString(QChar(0x25B2)) + " GAINERS");
    losers_btn_ = new QPushButton(QString(QChar(0x25BC)) + " LOSERS");

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

    apply_styles();
    set_loading(true);

    symbols_ = services::MarketDataService::mover_symbols();
}

void TopMoversWidget::apply_styles() {
    // Apply styles based on current tab state
    show_tab(showing_gainers_);
}

void TopMoversWidget::on_theme_changed() {
    apply_styles();
}

void TopMoversWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void TopMoversWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void TopMoversWidget::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(symbols_.size());
    for (const auto& sym : symbols_)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics);
}


void TopMoversWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    for (const auto& sym : symbols_) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading(false);
            rebuild_from_cache();
        });
    }
    hub_active_ = true;
}

void TopMoversWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void TopMoversWidget::rebuild_from_cache() {
    all_quotes_.clear();
    all_quotes_.reserve(row_cache_.size());
    for (const auto& sym : symbols_) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            all_quotes_.append(it.value());
    }
    std::sort(all_quotes_.begin(), all_quotes_.end(),
              [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });
    show_tab(showing_gainers_);
}


void TopMoversWidget::show_tab(bool gainers) {
    showing_gainers_ = gainers;

    auto active_g = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE());
    auto active_l = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::NEGATIVE(), ui::colors::BG_BASE());
    auto inactive = QString("QPushButton { background: %1; color: %2; border: none; "
                            "font-size: 9px; font-weight: bold; padding: 4px; }")
                        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY());

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
