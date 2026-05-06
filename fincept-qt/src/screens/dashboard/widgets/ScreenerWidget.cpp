#include "screens/dashboard/widgets/ScreenerWidget.h"

#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QFrame>
#include <QHBoxLayout>

#include <algorithm>

namespace fincept::screens::widgets {

// Broad basket: large-caps across sectors for screening
static const QStringList kScreenerSymbols = {
    "AAPL", "MSFT",  "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX", "AMD",  "INTC", "JPM",  "GS",   "BAC",  "WFC",
    "MS",   "BRK-B", "V",     "MA",   "WMT",  "COST", "TGT",  "HD",   "AMGN", "PFE",  "JNJ",  "MRK",  "XOM",  "CVX",
    "SLB",  "NEE",   "DUK",   "SO",   "CAT",  "GE",   "HON",  "RTX",  "PLTR", "COIN", "SOFI", "PYPL", "SNAP", "UBER"};

ScreenerWidget::ScreenerWidget(QWidget* parent) : BaseWidget("STOCK SCREENER", parent, ui::colors::INFO()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Filter bar
    filter_bar_ = new QWidget(this);
    auto* fl = new QHBoxLayout(filter_bar_);
    fl->setContentsMargins(8, 6, 8, 6);
    fl->setSpacing(8);

    filter_lbl_ = new QLabel("SORT BY");
    fl->addWidget(filter_lbl_);

    filter_combo_ = new QComboBox;
    filter_combo_->addItems({"% CHANGE ↑", "% CHANGE ↓", "VOLUME ↓", "PRICE ↓", "PRICE ↑"});
    fl->addWidget(filter_combo_);
    fl->addStretch();

    count_lbl_ = new QLabel(QString("%1 symbols").arg(kScreenerSymbols.size()));
    fl->addWidget(count_lbl_);

    vl->addWidget(filter_bar_);

    // Column headers
    header_ = new QWidget(this);
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setAlignment(a);
        header_labels_.append(l);
        hl->addWidget(l, s);
    };
    make_hdr("SYMBOL", 2);
    make_hdr("PRICE", 2, Qt::AlignRight);
    make_hdr("CHG%", 1, Qt::AlignRight);
    make_hdr("VOLUME", 2, Qt::AlignRight);
    vl->addWidget(header_);

    // Scrollable list
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);

    auto* list_widget = new QWidget(this);
    list_widget->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(list_widget);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);
    list_layout_->addStretch();

    scroll_->setWidget(list_widget);
    vl->addWidget(scroll_, 1);

    connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScreenerWidget::apply_filter);
    connect(this, &BaseWidget::refresh_requested, this, &ScreenerWidget::refresh_data);

    apply_styles();
    set_loading(true);

}

void ScreenerWidget::apply_styles() {
    filter_bar_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    filter_lbl_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    filter_combo_->setStyleSheet(
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3; font-size: 10px; padding: 2px 6px; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
            .arg(ui::colors::BG_BASE())
            .arg(ui::colors::TEXT_PRIMARY())
            .arg(ui::colors::BORDER_MED()));
    count_lbl_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));

    header_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                               .arg(ui::colors::BG_RAISED())
                               .arg(ui::colors::BORDER_DIM()));
    for (auto* lbl : header_labels_) {
        lbl->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                               .arg(ui::colors::TEXT_TERTIARY()));
    }

    scroll_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }" +
        QString("QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }")
            .arg(ui::colors::BORDER_MED()) +
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    // Re-render data rows with current tokens if data exists
    if (!all_quotes_.isEmpty())
        apply_filter();
}

void ScreenerWidget::on_theme_changed() {
    apply_styles();
}

void ScreenerWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void ScreenerWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void ScreenerWidget::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(kScreenerSymbols.size());
    for (const auto& sym : kScreenerSymbols)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true);  // user-triggered: bypass min_interval
}


void ScreenerWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), kScreenerSymbols.size());
    for (const auto& sym : kScreenerSymbols) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), kScreenerSymbols.size());
            rebuild_all_quotes();
        });
    }
    hub_active_ = true;
}

void ScreenerWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void ScreenerWidget::rebuild_all_quotes() {
    all_quotes_.clear();
    all_quotes_.reserve(row_cache_.size());
    for (const auto& sym : kScreenerSymbols) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            all_quotes_.append(it.value());
    }
    apply_filter();
}


void ScreenerWidget::apply_filter() {
    if (all_quotes_.isEmpty())
        return;

    QVector<services::QuoteData> sorted = all_quotes_;
    int idx = filter_combo_ ? filter_combo_->currentIndex() : 0;

    switch (idx) {
        case 0: // % change asc (top gainers first)
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });
            break;
        case 1: // % change desc (top losers first)
            std::sort(sorted.begin(), sorted.end(),
                      [](const auto& a, const auto& b) { return a.change_pct < b.change_pct; });
            break;
        case 2: // volume desc
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.volume > b.volume; });
            break;
        case 3: // price desc
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.price > b.price; });
            break;
        case 4: // price asc
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.price < b.price; });
            break;
    }

    // Show top 20
    if (sorted.size() > 20)
        sorted.resize(20);
    render_rows(sorted);
}

void ScreenerWidget::render_rows(const QVector<services::QuoteData>& rows) {
    while (list_layout_->count() > 0) {
        auto* item = list_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    bool alt = false;
    for (const auto& q : rows) {
        auto* row = new QWidget(this);
        row->setStyleSheet(QString("background: %1;").arg(alt ? ui::colors::BG_RAISED() : "transparent"));
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        auto* sym = new QLabel(q.symbol);
        sym->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(ui::colors::INFO()));
        rl->addWidget(sym, 2);

        auto* price = new QLabel(QString("$%1").arg(q.price, 0, 'f', 2));
        price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        price->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_PRIMARY()));
        rl->addWidget(price, 2);

        QString chg_str = QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2);
        QString chg_col = q.change_pct > 0   ? ui::colors::POSITIVE()
                          : q.change_pct < 0 ? ui::colors::NEGATIVE()
                                             : ui::colors::TEXT_PRIMARY();
        auto* chg = new QLabel(chg_str);
        chg->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        chg->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(chg_col));
        rl->addWidget(chg, 1);

        // Format volume: e.g. 45.2M
        QString vol_str;
        if (q.volume >= 1e9)
            vol_str = QString("%1B").arg(q.volume / 1e9, 0, 'f', 1);
        else if (q.volume >= 1e6)
            vol_str = QString("%1M").arg(q.volume / 1e6, 0, 'f', 1);
        else if (q.volume >= 1e3)
            vol_str = QString("%1K").arg(q.volume / 1e3, 0, 'f', 0);
        else
            vol_str = QString::number(static_cast<long long>(q.volume));

        auto* vol = new QLabel(vol_str);
        vol->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vol->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        rl->addWidget(vol, 2);

        list_layout_->addWidget(row);
        alt = !alt;
    }

    list_layout_->addStretch();
}

} // namespace fincept::screens::widgets
