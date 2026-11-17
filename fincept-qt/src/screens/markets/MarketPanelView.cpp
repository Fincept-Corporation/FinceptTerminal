#include "screens/markets/MarketPanelView.h"

#include "core/instrument/VenueSymbols.h"
#include "core/symbol/SymbolDragSource.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketPanelModel.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsStyle.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/FlashCellDelegate.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace colors = fincept::ui::colors;
using markets_logic::Session;

namespace {

/// Tick flash wash (the Watchlist constant), as a fraction of the direction colour.
constexpr double kFlashAlpha = 0.32;
/// Linked row marker width in px.
constexpr int kLinkBarPx = 2;

/// NAME cell: session dot (shape first, colour second), the display name in the model's
/// colour, a "!" glyph when the last refresh failed, a ">" glyph when the resolver has a
/// note for the row, and the display ticker in tertiary when the cell is wide enough.
class MarketNameDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(ui::metrics::row_height(-2, 22, 6));
        return s;
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, idx);
        const QString name = opt.text;
        opt.text.clear();
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        const QFontMetrics fm(opt.font);
        QRect r = option.rect.adjusted(6, 0, -6, 0);

        // Session dot: filled = regular session, half = pre/post, ring = closed,
        // ring with a bar = holiday, nothing = unknown (space kept for alignment).
        const int d = std::max(6, fm.height() / 2 - 1);
        const QRectF dot(r.left() + 0.5, r.center().y() - d / 2.0 + 0.5, d - 1, d - 1);
        const auto session = static_cast<Session>(idx.data(MarketPanelModel::SessionRole).toInt());
        switch (session) {
            case Session::Regular:
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(colors::SUCCESS()));
                p->drawEllipse(dot);
                break;
            case Session::Extended: {
                const QColor c(colors::WARNING());
                p->setPen(QPen(c, 1.2));
                p->setBrush(Qt::NoBrush);
                p->drawEllipse(dot);
                p->setPen(Qt::NoPen);
                p->setBrush(c);
                p->drawPie(dot, 90 * 16, 180 * 16); // left half filled
                break;
            }
            case Session::Closed:
            case Session::Holiday: {
                const QColor c(colors::TEXT_TERTIARY());
                p->setPen(QPen(c, 1.2));
                p->setBrush(Qt::NoBrush);
                p->drawEllipse(dot);
                if (session == Session::Holiday)
                    p->drawLine(QPointF(dot.left(), dot.center().y()), QPointF(dot.right(), dot.center().y()));
                break;
            }
            case Session::Unknown:
                break;
        }
        r.setLeft(r.left() + d + 6);

        const QString ticker = idx.data(MarketPanelModel::TickerRole).toString();
        const bool failed = idx.data(MarketPanelModel::FailedRole).toBool();
        const bool noted = !idx.data(MarketPanelModel::NoteRole).toString().isEmpty();
        QString glyphs;
        if (noted)
            glyphs += QStringLiteral(" >");
        if (failed)
            glyphs += QStringLiteral(" !");
        const int glyph_w = glyphs.isEmpty() ? 0 : fm.horizontalAdvance(glyphs);
        const bool show_ticker = !ticker.isEmpty() && ticker.compare(name, Qt::CaseInsensitive) != 0 &&
                                 r.width() >= 22 * std::max(1, fm.averageCharWidth());
        const QString gap = QStringLiteral("  ");
        const int ticker_w = show_ticker ? fm.horizontalAdvance(gap + ticker) : 0;
        const QString shown = fm.elidedText(name, Qt::ElideRight, std::max(0, r.width() - glyph_w - ticker_w));
        p->setFont(opt.font);
        p->setPen(opt.palette.color(QPalette::Text));
        p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, shown);
        int x = r.left() + fm.horizontalAdvance(shown);
        if (!glyphs.isEmpty()) {
            p->setPen(QColor(colors::WARNING()));
            p->drawText(QRect(x, r.top(), glyph_w, r.height()), Qt::AlignLeft | Qt::AlignVCenter, glyphs);
            x += glyph_w;
        }
        if (show_ticker) {
            p->setPen(QColor(colors::TEXT_TERTIARY()));
            p->drawText(QRect(x, r.top(), std::max(0, r.right() - x), r.height()),
                        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        fm.elidedText(gap + ticker, Qt::ElideRight, std::max(0, r.right() - x)));
        }
        p->restore();
    }
};

/// CHG %: heat cell (diverging scale, +/-3 % = full colour) with the tick flash on top.
class HeatFlashDelegate : public ui::BarCellDelegate {
  public:
    explicit HeatFlashDelegate(QObject* parent) : ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, parent) {
        set_heat_clamp(3.0);
    }
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const override {
        ui::BarCellDelegate::paint(p, option, idx);
        const double f = idx.data(MarketPanelModel::FlashRole).toDouble();
        if (f != 0.0 && std::isfinite(f)) {
            QColor c(f > 0 ? colors::POSITIVE() : colors::NEGATIVE());
            c.setAlphaF(static_cast<float>(std::clamp(std::abs(f), 0.0, 1.0) * kFlashAlpha));
            p->fillRect(option.rect, c);
        }
    }
};

/// DAY RANGE / 52W RANGE: the shared range bar, with its hover text in the row's own
/// currency and decimals ("Low $182.10 / High $189.84 / Last $185.02"; an index level or
/// FX rate without a unit) instead of the bar's generic compact numbers.
class MarketRangeDelegate : public ui::RangeBarDelegate {
  public:
    using ui::RangeBarDelegate::RangeBarDelegate;

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override {
        namespace fmt = fincept::ui::formatting;
        if (!event || event->type() != QEvent::ToolTip)
            return ui::RangeBarDelegate::helpEvent(event, view, option, index);
        bool ok_lo = false, ok_hi = false, ok_v = false;
        const double lo = index.data(LowRole).toDouble(&ok_lo);
        const double hi = index.data(HighRole).toDouble(&ok_hi);
        const double v = index.data(ValueRole).toDouble(&ok_v);
        if (!ok_lo || !ok_hi)
            return ui::RangeBarDelegate::helpEvent(event, view, option, index);
        const QString ccy = index.data(MarketPanelModel::CurrencyRole).toString(); // empty = not money
        fmt::InstrumentFmt inst = fmt::instrument_for_symbol(index.data(MarketPanelModel::SymbolRole).toString(), ccy);
        const int digits = index.data(MarketPanelModel::PriceDecimalsRole).toInt();
        if (digits >= 0)
            inst.decimals = digits;
        auto px = [&](double x) { return ccy.isEmpty() ? fmt::price(x, inst) : fmt::price_with_symbol(x, inst); };
        QStringList lines = {QCoreApplication::translate("MarketPanelView", "Low %1").arg(px(lo)),
                             QCoreApplication::translate("MarketPanelView", "High %1").arg(px(hi))};
        if (ok_v && std::isfinite(v))
            lines << QCoreApplication::translate("MarketPanelView", "Last %1").arg(px(v));
        QToolTip::showText(event->globalPos(), lines.join(QLatin1Char('\n')), view, option.rect);
        return true;
    }
};

QString mpv_sheet() {
    const QString table =
        QStringLiteral("QTreeView{background:%1;alternate-background-color:%2;border:none;outline:0;color:%3;"
                       "font-family:%4;font-size:%5px;}"
                       "QTreeView::item{border:none;padding:0px;}"
                       "QTreeView::item:hover{background:%6;}"
                       "QTreeView::item:selected{background:%7;color:%3;}"
                       "QHeaderView::section{background:%8;color:%9;border:none;border-bottom:1px solid %10;"
                       "padding:0 6px;font-family:%4;font-size:%11px;font-weight:600;}")
            .arg(colors::BG_BASE(), colors::ROW_ALT(), colors::TEXT_PRIMARY(), QString(ui::fonts::DATA_FAMILY))
            .arg(ui::fonts::px(-2))
            .arg(colors::BG_HOVER(), markets_selection_rgba(), colors::BG_SURFACE(), colors::TEXT_TERTIARY(),
                 colors::BORDER_DIM())
            .arg(ui::fonts::px(-4));
    return table + markets_scrollbar_css();
}

} // namespace

MarketPanelView::MarketPanelView(QWidget* parent) : QTreeView(parent) {
    markets_set_sheet(this, mpv_sheet()); // before the header / viewport are styled
    setUniformRowHeights(true);
    setAlternatingRowColors(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setExpandsOnDoubleClick(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // columns auto-hide instead
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAllColumnsShowFocus(true);
    setFocusPolicy(Qt::StrongFocus);
    setSortingEnabled(true);
    setDragEnabled(false); // the symbol drag below carries the shared MIME instead
    setContextMenuPolicy(Qt::CustomContextMenu);
    setItemDelegate(new ui::FlashCellDelegate(MarketPanelModel::FlashRole, this));
    setAccessibleName(tr("Market quotes"));

    QHeaderView* h = header();
    h->setSectionsMovable(true);
    h->setStretchLastSection(false);
    h->setSortIndicatorClearable(true); // ascending -> descending -> the panel's order
    h->setSortIndicatorShown(true);
    h->setHighlightSections(false);
    h->setMinimumSectionSize(20);
    h->setFixedHeight(ui::metrics::height(22));
    h->setContextMenuPolicy(Qt::CustomContextMenu);
    h->setToolTip(tr("Click to sort (a third click returns to the panel's order) · drag to reorder · "
                     "right-click to choose columns"));
    connect(h, &QWidget::customContextMenuRequested, this, &MarketPanelView::show_header_menu);
    connect(h, &QHeaderView::sectionMoved, this, &MarketPanelView::on_section_moved);
    connect(h, &QHeaderView::sectionResized, this, [this](int logical, int, int size) {
        if (applying_ || logical == mcols::Name)
            return;
        saved_widths_.insert(mcols::id_at(logical), size);
        emit layout_edited();
    });
    connect(h, &QHeaderView::sortIndicatorChanged, this, [this](int, Qt::SortOrder) {
        if (!applying_)
            emit layout_edited();
    });

    connect(this, &QAbstractItemView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString s = idx.data(MarketPanelModel::SymbolRole).toString();
        if (!s.isEmpty())
            emit symbol_activated(s);
    });
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex idx = indexAt(pos);
        const QString s = idx.data(MarketPanelModel::SymbolRole).toString();
        if (s.isEmpty())
            return;
        // The symbol is captured here, when the menu opens: a later tick or re-sort can
        // move rows under the pointer, so nothing reads the row again afterwards.
        setCurrentIndex(idx);
        emit context_menu_requested(s, viewport()->mapToGlobal(pos));
    });

    // A row drags out as a symbol (any panel / screen that takes the shared MIME).
    symbol_dnd::installDragSource(viewport(), [this]() -> SymbolRef {
        SymbolRef ref;
        const QModelIndex cur = currentIndex();
        ref.symbol = cur.data(MarketPanelModel::SymbolRole).toString();
        if (!ref.symbol.isEmpty())
            ref.asset_class = core::instrument::asset_class_slug(core::instrument::asset_class_of(ref.symbol));
        return ref;
    });
    // A symbol dropped from another panel or screen is added to this panel.
    symbol_dnd::installDropFilter(viewport(), [this](const SymbolRef& ref, SymbolGroup) {
        if (!ref.symbol.isEmpty())
            emit symbol_dropped(ref.symbol);
    });
}

void MarketPanelView::set_models(MarketPanelModel* model, MarketPanelProxy* proxy) {
    model_ = model;
    proxy_ = proxy;
    setModel(proxy);
    install_delegates();
    header()->setSectionResizeMode(QHeaderView::Interactive);
    header()->setSectionResizeMode(mcols::Name, QHeaderView::Stretch); // the one stretch section
}

void MarketPanelView::install_delegates() {
    setItemDelegateForColumn(mcols::Name, new MarketNameDelegate(this));
    setItemDelegateForColumn(mcols::ChangePct, new HeatFlashDelegate(this));
    auto* spark = new ui::SparklineDelegate(this);
    spark->set_baseline_role(MarketPanelModel::BaselineRole);
    setItemDelegateForColumn(mcols::Spark, spark);
    for (int c : {static_cast<int>(mcols::DayRange), static_cast<int>(mcols::Range52w)}) {
        auto* rd = new MarketRangeDelegate(this);
        rd->set_show_labels(false);
        setItemDelegateForColumn(c, rd);
    }
    auto* rel = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, this);
    rel->set_range(0.0, 3.0); // 3x the average volume fills the cell
    setItemDelegateForColumn(mcols::RelVolume, rel);
}

// ── Columns ─────────────────────────────────────────────────────────────────

int MarketPanelView::default_width(int column) const {
    const mcols::ColumnDef& d = mcols::def(column);
    const QFontMetrics fm(ui::fonts::font(-2));
    int w = d.fixed_px > 0 ? ui::metrics::width(d.fixed_px)
                           : std::max(4, d.width_chars) * std::max(6, fm.averageCharWidth()) + 16;
    // A header must fit its own title (+ sort arrow).
    const QFontMetrics hfm(ui::fonts::font(-4, QFont::DemiBold));
    w = std::max(w, hfm.horizontalAdvance(mcols::header(column)) + 28);
    return w;
}

void MarketPanelView::apply_config(const MarketPanelConfig& cfg) {
    if (!proxy_)
        return;
    QHeaderView* h = header();
    applying_ = true;
    const QStringList ids = mcols::sanitize_ids(cfg.columns.isEmpty() ? default_market_columns() : cfg.columns);
    chosen_.clear();
    for (const QString& id : ids)
        chosen_.insert(mcols::index_of(id));
    auto_hidden_.clear();
    for (int c = 0; c < mcols::count(); ++c)
        h->setSectionHidden(c, !chosen_.contains(c));
    int target = 0;
    for (const QString& id : ids) {
        const int logical = mcols::index_of(id);
        const int from = h->visualIndex(logical);
        if (from != target)
            h->moveSection(from, target);
        ++target;
    }
    saved_widths_.clear();
    for (auto it = cfg.widths.cbegin(); it != cfg.widths.cend(); ++it)
        if (mcols::index_of(it.key()) > 0 && it.value() >= 20 && it.value() <= 2000)
            saved_widths_.insert(it.key(), it.value());
    for (int c = 1; c < mcols::count(); ++c)
        h->resizeSection(c, saved_widths_.value(mcols::id_at(c), default_width(c)));
    const int sort_col = cfg.sort_id.isEmpty() ? -1 : mcols::index_of(cfg.sort_id);
    if (sort_col >= 0) {
        sortByColumn(sort_col, cfg.sort_desc ? Qt::DescendingOrder : Qt::AscendingOrder);
    } else {
        h->setSortIndicator(-1, Qt::AscendingOrder);
        proxy_->sort(-1);
    }
    applying_ = false;
    update_auto_hide();
}

QStringList MarketPanelView::chosen_column_ids() const {
    QStringList out;
    const QHeaderView* h = header();
    for (int v = 0; v < h->count(); ++v) {
        const int logical = h->logicalIndex(v);
        if (chosen_.contains(logical))
            out << mcols::id_at(logical);
    }
    return out;
}

void MarketPanelView::capture_config(MarketPanelConfig& cfg) const {
    cfg.columns = chosen_column_ids();
    cfg.widths = saved_widths_;
    const int sort_col = header()->sortIndicatorSection();
    if (sort_col >= 0 && sort_col < mcols::count()) {
        cfg.sort_id = mcols::id_at(sort_col);
        cfg.sort_desc = header()->sortIndicatorOrder() == Qt::DescendingOrder;
    } else {
        cfg.sort_id.clear();
        cfg.sort_desc = false;
    }
}

void MarketPanelView::set_column_visible(const QString& id, bool visible) {
    const int c = mcols::index_of(id);
    if (c < 0 || mcols::locked(c) || mcols::def(c).internal)
        return;
    if (chosen_.contains(c) == visible)
        return;
    if (visible) {
        chosen_.insert(c);
        header()->setSectionHidden(c, false);
        if (header()->sectionSize(c) < 20)
            header()->resizeSection(c, default_width(c));
    } else {
        chosen_.remove(c);
        auto_hidden_.remove(c);
        header()->setSectionHidden(c, true);
    }
    update_auto_hide();
    emit layout_edited();
    emit viewport_rows_changed(); // the sparkline column may have appeared
}

void MarketPanelView::reset_widths() {
    applying_ = true;
    saved_widths_.clear();
    for (int c = 1; c < mcols::count(); ++c)
        header()->resizeSection(c, default_width(c));
    applying_ = false;
    update_auto_hide();
    emit layout_edited();
}

bool MarketPanelView::is_sorted() const {
    return header()->sortIndicatorSection() >= 0;
}

void MarketPanelView::clear_sort() {
    applying_ = true;
    header()->setSortIndicator(-1, Qt::AscendingOrder);
    if (proxy_)
        proxy_->sort(-1);
    applying_ = false;
    emit layout_edited();
}

void MarketPanelView::resort() {
    if (!proxy_ || !is_sorted())
        return;
    const QString cur = current_symbol();
    proxy_->sort(header()->sortIndicatorSection(), header()->sortIndicatorOrder());
    if (!cur.isEmpty())
        select_symbol(cur, false);
}

void MarketPanelView::on_section_moved(int logical, int, int new_visual) {
    if (applying_)
        return;
    // NAME stays first (the Watchlist rule).
    applying_ = true;
    if (logical == mcols::Name && new_visual != 0)
        header()->moveSection(new_visual, 0);
    else if (header()->visualIndex(mcols::Name) != 0)
        header()->moveSection(header()->visualIndex(mcols::Name), 0);
    applying_ = false;
    emit layout_edited();
}

void MarketPanelView::update_auto_hide() {
    if (!proxy_)
        return;
    QHeaderView* h = header();
    QVector<QPair<int, int>> visible; // chosen columns except the stretch NAME, in visual order
    QHash<int, int> rank;
    for (int v = 0; v < h->count(); ++v) {
        const int logical = h->logicalIndex(v);
        if (!chosen_.contains(logical) || logical == mcols::Name)
            continue;
        const int w = h->sectionSize(logical) > 0 ? h->sectionSize(logical)
                                                   : saved_widths_.value(mcols::id_at(logical), default_width(logical));
        visible.append({logical, w});
        rank.insert(logical, mcols::def(logical).auto_hide_rank);
    }
    const int available = viewport()->width() > 0 ? viewport()->width() : width();
    const QSet<int> hide = markets_logic::auto_hidden_columns(visible, rank, available, 96);
    if (hide == auto_hidden_)
        return;
    applying_ = true;
    for (int c : chosen_) {
        const bool hidden = hide.contains(c);
        if (h->isSectionHidden(c) != hidden) {
            h->setSectionHidden(c, hidden);
            if (!hidden && h->sectionSize(c) < 20)
                h->resizeSection(c, saved_widths_.value(mcols::id_at(c), default_width(c)));
        }
    }
    applying_ = false;
    const bool spark_changed = hide.contains(mcols::Spark) != auto_hidden_.contains(mcols::Spark);
    auto_hidden_ = hide;
    if (spark_changed)
        emit viewport_rows_changed();
}

void MarketPanelView::refresh_metrics() {
    header()->setFixedHeight(ui::metrics::height(22));
    applying_ = true;
    for (int c = 1; c < mcols::count(); ++c)
        if (!saved_widths_.contains(mcols::id_at(c)))
            header()->resizeSection(c, default_width(c));
    applying_ = false;
    doItemsLayout(); // uniform row height follows the new font
    update_auto_hide();
}

void MarketPanelView::show_header_menu(const QPoint& pos) {
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    markets_set_sheet(menu, markets_menu_css());
    menu->setToolTipsVisible(true);
    for (int c = 0; c < mcols::count(); ++c) {
        if (mcols::def(c).internal)
            continue;
        QAction* act = menu->addAction(mcols::header(c));
        act->setCheckable(true);
        act->setChecked(chosen_.contains(c));
        act->setToolTip(mcols::tooltip(c));
        if (mcols::locked(c)) {
            act->setEnabled(false);
            continue;
        }
        const QString id = mcols::id_at(c);
        connect(act, &QAction::toggled, this, [this, id](bool on) { set_column_visible(id, on); });
    }
    menu->addSeparator();
    const int at = header()->logicalIndexAt(pos);
    if (at > 0 && !mcols::locked(at)) {
        const QString id = mcols::id_at(at);
        connect(menu->addAction(tr("Hide \"%1\"").arg(mcols::header(at))), &QAction::triggered, this,
                [this, id]() { set_column_visible(id, false); });
    }
    connect(menu->addAction(tr("Fit columns")), &QAction::triggered, this, &MarketPanelView::reset_widths);
    if (is_sorted())
        connect(menu->addAction(tr("Back to panel order")), &QAction::triggered, this, &MarketPanelView::clear_sort);
    connect(menu->addAction(tr("Reset columns")), &QAction::triggered, this, [this]() {
        MarketPanelConfig def;
        capture_config(def);
        def.columns = default_market_columns();
        def.widths.clear();
        apply_config(def);
        emit layout_edited();
        emit viewport_rows_changed();
    });
    menu->popup(header()->mapToGlobal(pos));
}

// ── Rows ────────────────────────────────────────────────────────────────────

QString MarketPanelView::symbol_at_proxy_row(int row) const {
    if (!proxy_ || row < 0 || row >= proxy_->rowCount())
        return {};
    return proxy_->index(row, 0).data(MarketPanelModel::SymbolRole).toString();
}

QString MarketPanelView::current_symbol() const {
    const QModelIndex cur = currentIndex();
    if (!cur.isValid() || !selectionModel() || !selectionModel()->isRowSelected(cur.row(), cur.parent()))
        return {};
    return cur.data(MarketPanelModel::SymbolRole).toString();
}

void MarketPanelView::select_symbol(const QString& symbol, bool center) {
    if (!model_ || !proxy_ || !selectionModel())
        return;
    const int src = model_->row_of(symbol);
    if (src < 0)
        return;
    const QModelIndex idx = proxy_->mapFromSource(model_->index(src, 0));
    if (!idx.isValid())
        return;
    suppress_selection_signal_ = true;
    selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    suppress_selection_signal_ = false;
    scrollTo(idx, center ? QAbstractItemView::PositionAtCenter : QAbstractItemView::EnsureVisible);
}

void MarketPanelView::clear_selection_silently() {
    if (!selectionModel())
        return;
    suppress_selection_signal_ = true;
    {
        QSignalBlocker block(selectionModel());
        selectionModel()->clearSelection();
        selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
    suppress_selection_signal_ = false;
    viewport()->update();
}

void MarketPanelView::currentChanged(const QModelIndex& current, const QModelIndex& previous) {
    QTreeView::currentChanged(current, previous);
    if (suppress_selection_signal_ || !current.isValid())
        return;
    if (current.row() == previous.row() && previous.isValid())
        return;
    const QString s = current.data(MarketPanelModel::SymbolRole).toString();
    if (!s.isEmpty())
        emit symbol_selected(s);
}

QStringList MarketPanelView::visible_symbols(int margin) const {
    QStringList out;
    if (!proxy_ || proxy_->rowCount() == 0)
        return out;
    QModelIndex top = indexAt(QPoint(4, 2));
    int first = top.isValid() ? top.row() : 0;
    QModelIndex bottom = indexAt(QPoint(4, std::max(0, viewport()->height() - 2)));
    int last = bottom.isValid() ? bottom.row() : proxy_->rowCount() - 1;
    first = std::max(0, first - margin);
    last = std::min(proxy_->rowCount() - 1, last + margin);
    for (int r = first; r <= last; ++r) {
        const QString s = symbol_at_proxy_row(r);
        if (!s.isEmpty())
            out << s;
    }
    return out;
}

QStringList MarketPanelView::display_order() const {
    QStringList out;
    if (!proxy_)
        return out;
    for (int r = 0; r < proxy_->rowCount(); ++r)
        out << symbol_at_proxy_row(r);
    return out;
}

// ── Events ──────────────────────────────────────────────────────────────────

void MarketPanelView::keyPressEvent(QKeyEvent* e) {
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
    const QString sym = current_symbol();
    const int key = e->key();
    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
        const Qt::ArrowType dir = key == Qt::Key_Left    ? Qt::LeftArrow
                                  : key == Qt::Key_Right ? Qt::RightArrow
                                  : key == Qt::Key_Up    ? Qt::UpArrow
                                  : key == Qt::Key_Down  ? Qt::DownArrow
                                                         : Qt::NoArrow;
        if (dir != Qt::NoArrow) {
            emit panel_move_requested(dir);
            return;
        }
    }
    if (mods == Qt::ControlModifier) {
        switch (key) {
            case Qt::Key_Up:
            case Qt::Key_Down:
                if (!sym.isEmpty() && !is_sorted())
                    emit move_requested(sym, key == Qt::Key_Up ? -1 : 1);
                return;
            case Qt::Key_Left:
            case Qt::Key_Right:
                emit focus_panel_requested(key == Qt::Key_Left ? -1 : 1);
                return;
            case Qt::Key_M:
                emit maximise_requested();
                return;
            case Qt::Key_N:
                emit add_row_requested();
                return;
            case Qt::Key_C:
                if (!sym.isEmpty()) {
                    const QString d = core::instrument::display_symbol(sym);
                    QApplication::clipboard()->setText(d.isEmpty() ? sym : d);
                }
                return;
            default:
                break;
        }
    }
    if (mods == Qt::NoModifier) {
        switch (key) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (!sym.isEmpty())
                    emit symbol_activated(sym);
                return;
            case Qt::Key_Space:
                if (!sym.isEmpty())
                    emit detail_toggle_requested(sym);
                return;
            case Qt::Key_Insert:
                emit add_row_requested();
                return;
            case Qt::Key_Delete:
                if (!sym.isEmpty())
                    emit remove_requested(sym);
                return;
            case Qt::Key_Escape:
                emit escape_pressed();
                return;
            default:
                break;
        }
    }
    QTreeView::keyPressEvent(e);
}

void MarketPanelView::resizeEvent(QResizeEvent* e) {
    QTreeView::resizeEvent(e);
    update_auto_hide(); // view-only: the saved column list does not change
    emit viewport_rows_changed();
}

void MarketPanelView::scrollContentsBy(int dx, int dy) {
    QTreeView::scrollContentsBy(dx, dy);
    if (dy != 0)
        emit viewport_rows_changed();
}

void MarketPanelView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QTreeView::drawRow(painter, option, index);
    if (!index.data(MarketPanelModel::LinkedRole).toBool())
        return;
    // The symbol-group's current symbol: a 2 px bar in the group colour (the wash is
    // the model's BackgroundRole).
    const QColor bar = link_group_ != SymbolGroup::None ? SymbolGroupRegistry::instance().color(link_group_)
                                                        : QColor(colors::AMBER());
    painter->fillRect(QRect(option.rect.left(), option.rect.top(), kLinkBarPx, option.rect.height()), bar);
}

} // namespace fincept::screens
