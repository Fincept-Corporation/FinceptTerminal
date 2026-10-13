#include "screens/dashboard/widgets/ScreenerWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QStyle>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {
// The CHG% label's colour is a "dir" property matched by the list stylesheet;
// re-polish only when it flips.
void screener_set_dir(QLabel* label, const char* dir) {
    if (label->property("dir").toString() == QLatin1String(dir))
        return;
    label->setProperty("dir", QString::fromLatin1(dir));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

// Broad basket: large-caps across sectors for screening
static const QStringList kScreenerSymbols = {
    "AAPL", "MSFT",  "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX", "AMD",  "INTC", "JPM",  "GS",   "BAC",  "WFC",
    "MS",   "BRK-B", "V",     "MA",   "WMT",  "COST", "TGT",  "HD",   "AMGN", "PFE",  "JNJ",  "MRK",  "XOM",  "CVX",
    "SLB",  "NEE",   "DUK",   "SO",   "CAT",  "GE",   "HON",  "RTX",  "PLTR", "COIN", "SOFI", "PYPL", "SNAP", "UBER"};

ScreenerWidget::ScreenerWidget(QWidget* parent) : BaseWidget(tr("STOCK SCREENER"), parent, ui::colors::INFO()) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Filter bar
    filter_bar_ = new QWidget(this);
    auto* fl = new QHBoxLayout(filter_bar_);
    fl->setContentsMargins(8, 6, 8, 6);
    fl->setSpacing(8);

    filter_lbl_ = new QLabel(tr("SORT BY"));
    fl->addWidget(filter_lbl_);

    filter_combo_ = new QComboBox;
    filter_combo_->addItems({tr("% CHANGE ↑"), tr("% CHANGE ↓"), tr("VOLUME ↓"), tr("PRICE ↓"), tr("PRICE ↑")});
    fl->addWidget(filter_combo_);
    fl->addStretch();

    count_lbl_ = new QLabel(tr("%1 symbols").arg(kScreenerSymbols.size()));
    fl->addWidget(count_lbl_);

    vl->addWidget(filter_bar_);

    // Column headers
    header_ = new QWidget(this);
    header_->setObjectName("scHeader");
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(8, 3, 8, 3);

    auto make_hdr = [&](const QString& t, int s, Qt::Alignment a = Qt::AlignLeft) {
        auto* l = new QLabel(t);
        l->setAlignment(a);
        header_labels_.append(l);
        hl->addWidget(l, s);
    };
    make_hdr(tr("SYMBOL"), 2);
    make_hdr(tr("PRICE"), 2, Qt::AlignRight);
    make_hdr(tr("CHG%"), 1, Qt::AlignRight);
    make_hdr(tr("VOLUME"), 2, Qt::AlignRight);
    vl->addWidget(header_);

    // Scrollable list
    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);

    list_widget_ = new QWidget(this);
    list_widget_->setObjectName("screenerList");
    list_layout_ = new QVBoxLayout(list_widget_);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);

    // Empty / loading placeholder — the screener used to render nothing at all
    // until the first quote landed, which reads as a broken tile.
    empty_lbl_ = new QLabel(tr("Waiting for quotes..."));
    empty_lbl_->setObjectName("scEmpty");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    list_layout_->addWidget(empty_lbl_);
    list_layout_->addStretch();

    scroll_->setWidget(list_widget_);
    vl->addWidget(scroll_, 1);

    filter_combo_->setAccessibleName(tr("Screener sort order"));
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

    // The header used to carry a selector-less sheet, which Qt also applies to its
    // labels (so they drew the bottom border too) — the label rule keeps that.
    header_->setStyleSheet(
        QString("QWidget#scHeader{background:%1;border-bottom:1px solid %2;}"
                "QWidget#scHeader QLabel{border-bottom:1px solid %2;color:%3;font-size:9px;font-weight:bold;"
                "background:transparent;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY()));

    scroll_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }" +
        QString("QScrollBar::handle:vertical { background: %1; border-radius: 2px; min-height: 20px; }")
            .arg(ui::colors::BORDER_MED()) +
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    // ── Row styling, hoisted (P7) ──
    // render_rows() draws up to 20 rows × 5 widgets. Styling each of those
    // inline meant ~100 full CSS reparses per render, and render ran once per
    // arriving symbol (42 of them). All of it is now one stylesheet, set here
    // and only re-set on a theme change; rows just carry object names.
    if (list_widget_) {
        list_widget_->setStyleSheet(
            QString("#screenerList{background:transparent;}"
                    "#screenerList QWidget#scRow{background:transparent;}"
                    "#screenerList QWidget#scRowAlt{background:%1;}"
                    "#screenerList QLabel{font-size:10px;background:transparent;}"
                    "#screenerList QLabel#scSym{color:%2;font-weight:bold;}"
                    "#screenerList QLabel#scPrice{color:%3;}"
                    "#screenerList QLabel#scChg{color:%3;font-weight:bold;}"
                    "#screenerList QLabel#scChg[dir=\"pos\"]{color:%4;}"
                    "#screenerList QLabel#scChg[dir=\"neg\"]{color:%5;}"
                    "#screenerList QLabel#scVol{color:%6;}"
                    "#screenerList QLabel#scEmpty{color:%7;font-size:11px;padding:20px;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::INFO(), ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(),
                     ui::colors::NEGATIVE(), ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_TERTIARY()));
    }

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
    hub.request(topics, /*force=*/true); // user-triggered: bypass min_interval
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
            // One sort + 20-row redraw per delivery burst, not one per symbol.
            schedule_render([this]() { rebuild_all_quotes(); });
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
    if (all_quotes_.isEmpty()) {
        render_rows({}); // keeps the empty-state label on screen
        return;
    }

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

ScreenerWidget::ScreenerRow ScreenerWidget::make_row(int rank) {
    // No setStyleSheet here — every colour/size comes from the single
    // stylesheet on list_widget_ (see apply_styles) via these object names.
    ScreenerRow r;
    r.row = new QWidget(list_widget_);
    r.row->setObjectName(rank % 2 ? QStringLiteral("scRowAlt") : QStringLiteral("scRow"));
    auto* rl = new QHBoxLayout(r.row);
    rl->setContentsMargins(8, 4, 8, 4);

    r.sym = new QLabel(r.row);
    r.sym->setObjectName("scSym");
    rl->addWidget(r.sym, 2);

    r.price = new QLabel(r.row);
    r.price->setObjectName("scPrice");
    r.price->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rl->addWidget(r.price, 2);

    r.chg = new QLabel(r.row);
    r.chg->setObjectName("scChg");
    r.chg->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rl->addWidget(r.chg, 1);

    r.vol = new QLabel(r.row);
    r.vol->setObjectName("scVol");
    r.vol->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rl->addWidget(r.vol, 2);

    // Rows sit between the empty-state label and the trailing stretch.
    list_layout_->insertWidget(list_layout_->count() - 1, r.row);
    return r;
}

void ScreenerWidget::render_rows(const QVector<services::QuoteData>& rows) {
    // empty_lbl_ stays in the layout for the widget's lifetime; it is shown
    // only while there is nothing to list.
    if (empty_lbl_)
        empty_lbl_->setVisible(rows.isEmpty());

    // Refill the pooled rank rows in place (QLabel::setText is a no-op for
    // unchanged text); rows beyond this render's count are hidden.
    for (int i = 0; i < rows.size(); ++i) {
        if (i >= rows_.size())
            rows_.append(make_row(i));
        const auto& q = rows[i];
        const ScreenerRow& r = rows_[i];
        link_symbol(r.row, q.symbol);
        r.sym->setText(q.symbol);
        r.price->setText(QString("$%1").arg(q.price, 0, 'f', 2));
        r.chg->setText(QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2));
        screener_set_dir(r.chg, q.change_pct > 0 ? "pos" : (q.change_pct < 0 ? "neg" : "flat"));

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
        r.vol->setText(vol_str);

        r.row->setVisible(true);
    }
    for (int i = static_cast<int>(rows.size()); i < rows_.size(); ++i)
        rows_[i].row->setVisible(false);
}

void ScreenerWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("STOCK SCREENER"));
    // Must match the source string used in the constructor — this said
    // "FILTER:" while the ctor used "SORT BY", so a language switch silently
    // relabelled the control (and looked up a key that isn't in the catalogue).
    if (filter_lbl_)
        filter_lbl_->setText(tr("SORT BY"));
    if (count_lbl_)
        count_lbl_->setText(tr("%1 symbols").arg(kScreenerSymbols.size()));
    if (empty_lbl_)
        empty_lbl_->setText(tr("Waiting for quotes..."));
    // Column headers + sort-order items are set once in the ctor — the old
    // rebuild_all_quotes() call re-rendered data rows but never these.
    const QStringList hdrs = {tr("SYMBOL"), tr("PRICE"), tr("CHG%"), tr("VOLUME")};
    for (int i = 0; i < header_labels_.size() && i < hdrs.size(); ++i)
        header_labels_[i]->setText(hdrs[i]);
    if (filter_combo_) {
        const int cur = filter_combo_->currentIndex();
        const QStringList items = {tr("% CHANGE ↑"), tr("% CHANGE ↓"), tr("VOLUME ↓"), tr("PRICE ↓"), tr("PRICE ↑")};
        for (int i = 0; i < filter_combo_->count() && i < items.size(); ++i)
            filter_combo_->setItemText(i, items[i]);
        filter_combo_->setCurrentIndex(cur);
        filter_combo_->setAccessibleName(tr("Screener sort order"));
    }
    rebuild_all_quotes();
}

} // namespace fincept::screens::widgets
