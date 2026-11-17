#include "screens/markets/MarketsOverviewViews.h"

#include "core/instrument/VenueSymbols.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketPanelModel.h"
#include "screens/markets/MarketPanelView.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "ui/charts/Treemap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/ChartStyle.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/PanelHeader.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace colors = fincept::ui::colors;
namespace fmt = fincept::ui::formatting;

namespace {

constexpr int kMoversPerList = 12;

QString mov_sheet() {
    return QStringLiteral("#movRoot{background:%1;}"
                          "QLabel#movNote{color:%2;background:transparent;font-size:%3px;}"
                          "QLabel#movCaption{color:%2;background:transparent;font-size:%3px;padding:4px 12px;}"
                          "#movList{background:%4;border:1px solid %5;}")
        .arg(colors::BG_BASE(), colors::TEXT_TERTIARY())
        .arg(ui::fonts::px(-3))
        .arg(colors::BG_BASE(), colors::BORDER_DIM());
}

/// Unique panel symbols in reading order, with the first panel that lists each.
QHash<QString, QString> ov_first_panel(const QVector<MarketPanelConfig>& panels, QStringList* order) {
    QHash<QString, QString> first;
    for (const auto& p : panels)
        for (const QString& s : p.symbols)
            if (!first.contains(s)) {
                first.insert(s, p.title);
                if (order)
                    order->append(s);
            }
    return first;
}

} // namespace

// ── MarketsHeatmapView ──────────────────────────────────────────────────────

MarketsHeatmapView::MarketsHeatmapView(MarketsQuoteStore* store, QWidget* parent) : QWidget(parent), store_(store) {
    setObjectName(QStringLiteral("movRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mov_sheet()); // before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    const ui::PanelHeaderParts hdr = ui::make_panel_header(QString(), this);
    title_ = hdr.title_label;
    note_ = new QLabel(hdr.controls_slot);
    note_->setObjectName(QStringLiteral("movNote"));
    hdr.controls_slot->layout()->addWidget(note_);
    root->addWidget(hdr.header);

    map_ = new ui::Treemap(this);
    map_->set_grouped(true);
    map_->set_color_clamp(3.0);
    map_->set_show_legend(true);
    map_->set_tooltip_provider([this](const ui::TreemapItem& it) {
        const MarketRow* r = store_ ? store_->row(it.id) : nullptr;
        QVector<QPair<QString, QString>> rows;
        if (r && r->has_quote) {
            fmt::InstrumentFmt inst = fmt::instrument_for_symbol(it.id, r->quote.currency);
            if (r->quote.price_digits >= 0)
                inst.decimals = r->quote.price_digits;
            // In the instrument's own currency; an index level or FX rate has no unit.
            using core::instrument::AssetClass;
            const bool money = r->cls != AssetClass::Index && r->cls != AssetClass::Fx &&
                               !r->quote.currency.isEmpty() && fmt::is_money_quote(r->quote.quote_type, it.id);
            rows.append({tr("LAST"), money ? fmt::price_with_symbol(r->quote.price, inst)
                                           : fmt::price(r->quote.price, inst)});
            rows.append({tr("CHG %"), fmt::pct(r->quote.change_pct)});
        } else {
            rows.append({tr("LAST"), r && !r->error.isEmpty() ? r->error : tr("Waiting for the first quote")});
        }
        rows.append({tr("PANEL"), it.group});
        const QString name = store_ ? store_->display_name(it.id) : it.id;
        return ui::chart_style::tooltip_html(name == it.label ? it.label : QStringLiteral("%1  (%2)").arg(name, it.label),
                                             rows);
    });
    connect(map_, &ui::Treemap::item_clicked, this, &MarketsHeatmapView::symbol_selected);
    connect(map_, &ui::Treemap::item_double_clicked, this, &MarketsHeatmapView::symbol_activated);
    connect(map_, &ui::Treemap::group_clicked, this, [this](const QString& group) {
        for (const auto& p : std::as_const(panels_))
            if (p.title == group) {
                emit panel_requested(p.id);
                return;
            }
    });
    root->addWidget(map_, 1);

    rebuild_timer_ = new QTimer(this);
    rebuild_timer_->setSingleShot(true);
    rebuild_timer_->setInterval(500); // coalesces a delivery burst into one relayout
    connect(rebuild_timer_, &QTimer::timeout, this, &MarketsHeatmapView::rebuild);
    if (store_)
        connect(store_, &MarketsQuoteStore::rows_changed, this, &MarketsHeatmapView::schedule_rebuild);
    // Tile sub-labels hold formatted percentages: re-render from the store (no fetch).
    fmt::on_format_changed(this, [this]() { rebuild(); });
    retranslate();
}

void MarketsHeatmapView::set_panels(const QVector<MarketPanelConfig>& panels) {
    panels_ = panels;
    schedule_rebuild();
}

void MarketsHeatmapView::schedule_rebuild() {
    if (isVisible() && !rebuild_timer_->isActive())
        rebuild_timer_->start();
}

void MarketsHeatmapView::rebuild() {
    QVector<ui::TreemapItem> items;
    for (const auto& p : std::as_const(panels_)) {
        for (const QString& s : p.symbols) {
            ui::TreemapItem it;
            it.id = s;
            const QString d = core::instrument::display_symbol(s);
            it.label = d.isEmpty() ? s : d;
            it.group = p.title;
            it.weight = 1.0; // equal tiles: no size source exists for indices, FX and futures
            const MarketRow* r = store_ ? store_->row(s) : nullptr;
            if (r && r->has_quote) {
                it.value = r->quote.change_pct;
                it.sublabel = fmt::pct(r->quote.change_pct);
            } else {
                it.value = std::numeric_limits<double>::quiet_NaN(); // neutral tile
                it.sublabel = r && !r->error.isEmpty() ? QStringLiteral("-") : QStringLiteral("...");
            }
            items.append(it);
        }
    }
    map_->set_items(items);
}

void MarketsHeatmapView::refresh_theme() {
    map_->update(); // painted from the theme
}

void MarketsHeatmapView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    rebuild();
}

void MarketsHeatmapView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void MarketsHeatmapView::retranslate() {
    title_->setText(tr("HEATMAP"));
    note_->setText(tr("Equal-size tiles, colour = today's change, +/-3 % = full colour"));
}

// ── MarketsMoversView ───────────────────────────────────────────────────────

MarketsMoversView::MarketsMoversView(MarketsQuoteStore* store, QWidget* parent) : QWidget(parent), store_(store) {
    setObjectName(QStringLiteral("movRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    markets_set_sheet(this, mov_sheet()); // before any child exists
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    caption_ = new QLabel(this);
    caption_->setObjectName(QStringLiteral("movCaption"));
    root->addWidget(caption_);
    auto* row = new QHBoxLayout;
    row->setContentsMargins(4, 0, 4, 4);
    row->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        auto* box = new QWidget(this);
        box->setObjectName(QStringLiteral("movList"));
        box->setAttribute(Qt::WA_StyledBackground, true);
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(1, 1, 1, 1);
        bl->setSpacing(0);
        const ui::PanelHeaderParts hdr = ui::make_panel_header(QString(), box);
        bl->addWidget(hdr.header);
        List l;
        l.title = hdr.title_label;
        l.model = new MarketPanelModel(store_, this);
        l.proxy = new MarketPanelProxy(this);
        l.proxy->setSourceModel(l.model);
        l.stack = new QStackedWidget(box); // the list, or the shared empty state
        l.view = new MarketPanelView(l.stack);
        l.view->set_models(l.model, l.proxy);
        MarketPanelConfig cfg;
        cfg.columns = {QStringLiteral("name"), QStringLiteral("last"), QStringLiteral("change_pct"),
                       QStringLiteral("spark")};
        l.view->apply_config(cfg);
        l.view->header()->setSectionHidden(mcols::Panel, false); // internal PANEL column
        l.view->header()->resizeSection(mcols::Panel, l.view->default_width(mcols::Panel));
        l.stack->addWidget(l.view);
        l.empty = new ui::EmptyState(l.stack);
        l.empty->set_compact(true);
        l.stack->addWidget(l.empty);
        l.stack->setCurrentWidget(l.empty); // never an empty table: filled by recompute()
        bl->addWidget(l.stack, 1);
        connect(l.view, &MarketPanelView::symbol_selected, this, &MarketsMoversView::symbol_selected);
        connect(l.view, &MarketPanelView::symbol_activated, this, &MarketsMoversView::symbol_activated);
        connect(l.view, &MarketPanelView::viewport_rows_changed, this, &MarketsMoversView::viewport_rows_changed);
        row->addWidget(box, 1);
        lists_.append(l);
    }
    root->addLayout(row, 1);
    retranslate();
}

void MarketsMoversView::set_panels(const QVector<MarketPanelConfig>& panels) {
    panels_ = panels;
    if (isVisible())
        recompute();
}

void MarketsMoversView::recompute() {
    if (!store_)
        return;
    QStringList order;
    const QHash<QString, QString> first = ov_first_panel(panels_, &order);
    struct Entry {
        QString symbol;
        double pct;
        double extension;
    };
    QVector<Entry> fresh;
    for (const QString& s : std::as_const(order)) {
        const MarketRow* r = store_->row(s);
        if (!r || !r->has_quote || !r->error.isEmpty() || store_->is_stale(*r) || !std::isfinite(r->quote.change_pct))
            continue;
        const double ma = r->quote.fifty_day_average;
        const double ext = std::isfinite(ma) && ma > 0 ? std::abs(r->quote.price / ma - 1.0) : -1.0;
        fresh.append({s, r->quote.change_pct, ext});
    }
    QVector<Entry> by_pct = fresh;
    std::stable_sort(by_pct.begin(), by_pct.end(), [](const Entry& a, const Entry& b) { return a.pct > b.pct; });
    QStringList gainers, losers, extended;
    for (int i = 0; i < by_pct.size() && gainers.size() < kMoversPerList; ++i)
        gainers << by_pct.at(i).symbol;
    for (int i = static_cast<int>(by_pct.size()) - 1; i >= 0 && losers.size() < kMoversPerList; --i)
        losers << by_pct.at(i).symbol;
    QVector<Entry> by_ext;
    for (const auto& e : std::as_const(fresh))
        if (e.extension >= 0)
            by_ext.append(e);
    std::stable_sort(by_ext.begin(), by_ext.end(), [](const Entry& a, const Entry& b) { return a.extension > b.extension; });
    for (int i = 0; i < by_ext.size() && extended.size() < kMoversPerList; ++i)
        extended << by_ext.at(i).symbol;

    const QStringList lists[] = {gainers, losers, extended};
    for (int i = 0; i < lists_.size() && i < 3; ++i) {
        // Never an empty table: the shared empty state says why the list has nothing yet.
        List& l = lists_[i];
        if (lists[i].isEmpty()) {
            if (fresh.isEmpty()) {
                l.empty->set_title(tr("Waiting for quotes"));
                l.empty->set_message(tr("The lists fill as the quotes of your panel symbols arrive."));
            } else {
                l.empty->set_title(tr("Nothing to list"));
                l.empty->set_message(i == 2 ? tr("No 50-day average is reported for these symbols yet.")
                                            : tr("No symbol has a fresh quote."));
            }
        }
        QWidget* page = lists[i].isEmpty() ? static_cast<QWidget*>(l.empty) : static_cast<QWidget*>(l.view);
        if (l.stack->currentWidget() != page)
            l.stack->setCurrentWidget(page);
        lists_[i].model->set_panel_names(first);
        if (lists_[i].model->symbols() != lists[i]) {
            const QString cur = lists_[i].view->current_symbol();
            lists_[i].model->set_symbols(lists[i]);
            if (!cur.isEmpty())
                lists_[i].view->select_symbol(cur, false);
        }
    }
    caption_->setText(tr("From the %n symbol(s) in your panels - not the whole market.", nullptr,
                         static_cast<int>(order.size())));
    emit viewport_rows_changed();
}

QStringList MarketsMoversView::visible_symbols(int margin) const {
    QStringList out;
    for (const auto& l : lists_)
        for (const QString& s : l.view->visible_symbols(margin))
            if (!out.contains(s))
                out << s;
    return out;
}

void MarketsMoversView::refresh_theme() {
    for (const auto& l : lists_) {
        l.model->refresh_all();
        l.view->viewport()->update();
    }
}

void MarketsMoversView::refresh_metrics() {
    for (const auto& l : lists_)
        l.view->refresh_metrics();
}

void MarketsMoversView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    recompute();
}

void MarketsMoversView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void MarketsMoversView::retranslate() {
    const char* titles[] = {QT_TR_NOOP("TOP GAINERS"), QT_TR_NOOP("TOP LOSERS"), QT_TR_NOOP("MOST EXTENDED")};
    for (int i = 0; i < lists_.size() && i < 3; ++i)
        lists_[i].title->setText(tr(titles[i]));
    if (lists_.size() > 2)
        lists_[2].title->setToolTip(tr("Furthest from the 50-day average, either side"));
    caption_->setText(tr("From the %n symbol(s) in your panels - not the whole market.", nullptr,
                         static_cast<int>(ov_first_panel(panels_, nullptr).size())));
}

} // namespace fincept::screens
