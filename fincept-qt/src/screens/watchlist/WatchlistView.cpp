#include "screens/watchlist/WatchlistView.h"

#include "screens/watchlist/WatchlistColumns.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QHeaderView>
#include <QHelpEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace wcols = fincept::screens::watchlist_columns;

namespace {

/// Full heat colour at this many percent, per performance horizon.
double wlv_heat_clamp(const QString& id) {
    if (id == QLatin1String("perf_1w"))
        return 5.0;
    if (id == QLatin1String("perf_1m"))
        return 10.0;
    if (id == QLatin1String("perf_3m"))
        return 20.0;
    if (id == QLatin1String("perf_6m") || id == QLatin1String("perf_ytd"))
        return 30.0;
    return 40.0;
}

QPixmap wlv_drag_pixmap(const QString& text) {
    // Accent pill with the ticker(s), built per drag from the active theme.
    const QFont font = fincept::ui::fonts::font(-1, QFont::Bold);
    const QFontMetrics fm(font);
    const int w = fm.horizontalAdvance(text) + 20;
    const int h = fm.height() + 10;
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(static_cast<int>(w * dpr), static_cast<int>(h * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, w, h), h / 2.0, h / 2.0);
    p.fillPath(path, QColor(colors::AMBER()));
    p.setPen(QColor(colors::TEXT_ON_ACCENT()));
    p.setFont(font);
    p.drawText(QRectF(0, 0, w, h), Qt::AlignCenter, text);
    p.end();
    return pm;
}

} // namespace

// ── WatchlistCellDelegate ───────────────────────────────────────────────────
// ui::FlashCellDelegate on WatchlistModel::FlashRole / IsGroupRole (WatchlistView.h).
// A group's label (symbol column) is painted by WatchlistView::drawRow, which lets it
// run on over the empty cells beside it instead of eliding it to the ticker width.

namespace {

/// Range bar whose low / high labels use the row's own price decimals (EURUSD
/// 1.1004, not 1.10). RangeBarDelegate's formatter has no row context, so the
/// paint / tooltip are redone here with a per-row formatter.
class WatchlistRangeDelegate : public ui::RangeBarDelegate {
  public:
    using ui::RangeBarDelegate::RangeBarDelegate;

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& idx) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, idx);
        opt.text.clear(); // background / selection only
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, p, opt.widget);
        bool ok = false;
        const ui::RangeBarData d = data_for(idx, &ok);
        if (!ok)
            return;
        ui::RangeBar::paint_range(*p, QRectF(option.rect).adjusted(4, 1, -4, -1), d, formatter_for(idx), true);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& idx) override {
        if (event && event->type() == QEvent::ToolTip) {
            bool ok = false;
            const ui::RangeBarData d = data_for(idx, &ok);
            if (ok) {
                QToolTip::showText(event->globalPos(), ui::RangeBar::tooltip_for(d, formatter_for(idx)), view,
                                   option.rect);
                return true;
            }
        }
        return QStyledItemDelegate::helpEvent(event, view, option, idx);
    }

  private:
    static std::function<QString(double)> formatter_for(const QModelIndex& idx) {
        bool ok = false;
        const int dec = idx.data(WatchlistModel::PriceDecimalsRole).toInt(&ok);
        if (!ok || dec < 0)
            return [](double v) { return fmt::price(v); };
        fmt::InstrumentFmt inst;
        inst.decimals = dec;
        return [inst](double v) { return fmt::price(v, inst); };
    }
};

} // namespace

// ── WatchlistView ───────────────────────────────────────────────────────────

WatchlistView::WatchlistView(QWidget* parent) : QTreeView(parent) {
    setUniformRowHeights(true);
    setAlternatingRowColors(true);
    setRootIsDecorated(false);
    setItemsExpandable(true);
    setExpandsOnDoubleClick(false); // double-click opens the symbol; groups toggle on single click
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setAllColumnsShowFocus(true);
    setSortingEnabled(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDragDropOverwriteMode(false);
    setDefaultDropAction(Qt::CopyAction);
    setItemDelegate(new WatchlistCellDelegate(this));

    QHeaderView* h = header();
    h->setSectionsMovable(true);
    h->setStretchLastSection(false);
    h->setSortIndicatorClearable(true); // ascending -> descending -> manual order
    h->setSortIndicatorShown(true);
    h->setHighlightSections(false);
    h->setContextMenuPolicy(Qt::CustomContextMenu);
    h->setToolTip(tr("Click to sort (a third click returns to your order) · drag to reorder · "
                     "right-click to choose columns"));
    connect(h, &QWidget::customContextMenuRequested, this, &WatchlistView::show_header_menu);
    connect(h, &QHeaderView::sectionMoved, this, &WatchlistView::on_section_moved);
    connect(h, &QHeaderView::sectionResized, this, [this](int, int, int) {
        if (!applying_)
            emit layout_edited();
    });
    connect(h, &QHeaderView::sortIndicatorChanged, this, [this](int, Qt::SortOrder) {
        if (!applying_)
            emit layout_edited();
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex&) {
        if (!applying_)
            emit expansion_edited();
    });
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex&) {
        if (!applying_)
            emit expansion_edited();
    });
    // A click on a group row toggles it (group rows are not selectable).
    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) {
        if (idx.isValid() && idx.data(WatchlistModel::IsGroupRole).toBool()) {
            const QModelIndex first = idx.sibling(idx.row(), 0);
            setExpanded(first, !isExpanded(first));
        }
    });
}

void WatchlistView::set_models(WatchlistModel* source, WatchlistProxyModel* proxy) {
    source_ = source;
    proxy_ = proxy;
    setModel(proxy);
    install_delegates();
}

void WatchlistView::install_delegates() {
    for (int c = 0; c < wcols::count(); ++c) {
        const WatchlistColumnDef& d = wcols::def(c);
        const QString id = QString::fromLatin1(d.id);
        switch (d.kind) {
            case WatchlistColKind::RangeBar:
                setItemDelegateForColumn(c, new WatchlistRangeDelegate(this));
                break;
            case WatchlistColKind::Spark: {
                auto* sd = new ui::SparklineDelegate(this);
                sd->set_baseline_role(WatchlistModel::BaselineRole);
                setItemDelegateForColumn(c, sd);
                break;
            }
            case WatchlistColKind::HeatPct: {
                auto* hd = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, this);
                hd->set_heat_clamp(wlv_heat_clamp(id)); // fixed scale: colours compare across rows and groups
                setItemDelegateForColumn(c, hd);
                break;
            }
            case WatchlistColKind::RelVolume: {
                auto* bd = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, this);
                bd->set_range(0.0, 3.0); // 3x the average volume fills the cell
                setItemDelegateForColumn(c, bd);
                break;
            }
            default:
                break;
        }
    }
}

int WatchlistView::default_width(int column) const {
    const WatchlistColumnDef& d = wcols::def(column);
    const int ch = std::max(6, fontMetrics().averageCharWidth());
    int w = d.width_chars * ch + 16;
    if (d.kind == WatchlistColKind::Symbol)
        w += indentation(); // grouped rows are indented inside this column (RELIANCE.NS was elided)
    if (d.kind == WatchlistColKind::RangeBar)
        w = std::max(w, 150);
    else if (d.kind == WatchlistColKind::Spark)
        w = std::max(w, 96);
    // A header must fit its own title.
    w = std::max(w, header()->fontMetrics().horizontalAdvance(wcols::header(column)) + 24);
    return w;
}

void WatchlistView::apply_layout(const WatchlistLayout& layout) {
    if (!proxy_)
        return;
    QHeaderView* h = header();
    applying_ = true;
    const QStringList visible = layout.is_valid() ? layout.visible : WatchlistColumnPrefs::builtin_default().visible;
    const QSet<QString> shown(visible.cbegin(), visible.cend());
    for (int c = 0; c < wcols::count(); ++c)
        h->setSectionHidden(c, !shown.contains(wcols::id_at(c)) && c != 0);
    // Order: the listed ids first, in their order; hidden columns keep whatever place they had.
    int target = 0;
    for (const QString& id : visible) {
        const int logical = wcols::index_of(id);
        if (logical < 0)
            continue;
        const int from = h->visualIndex(logical);
        if (from != target)
            h->moveSection(from, target);
        ++target;
    }
    for (int c = 0; c < wcols::count(); ++c) {
        const int w = layout.widths.value(wcols::id_at(c), 0);
        h->resizeSection(c, w > 0 ? w : default_width(c));
    }
    const int sort_col = layout.sort_id.isEmpty() ? -1 : wcols::index_of(layout.sort_id);
    if (sort_col >= 0) {
        sortByColumn(sort_col, layout.sort_order);
    } else {
        h->setSortIndicator(-1, Qt::AscendingOrder);
        proxy_->sort(-1);
    }
    applying_ = false;
    emit visible_columns_changed();
}

void WatchlistView::capture_layout(WatchlistLayout& layout) const {
    const QHeaderView* h = header();
    layout.visible.clear();
    layout.widths.clear();
    for (int v = 0; v < h->count(); ++v) {
        const int logical = h->logicalIndex(v);
        if (logical < 0 || h->isSectionHidden(logical))
            continue;
        const QString id = wcols::id_at(logical);
        layout.visible << id;
        layout.widths.insert(id, h->sectionSize(logical));
    }
    const int sort_col = h->sortIndicatorSection();
    if (sort_col >= 0 && sort_col < wcols::count() && h->isSortIndicatorShown()) {
        layout.sort_id = wcols::id_at(sort_col);
        layout.sort_order = h->sortIndicatorOrder();
    } else {
        layout.sort_id.clear();
        layout.sort_order = Qt::AscendingOrder;
    }
}

QStringList WatchlistView::visible_column_ids() const {
    WatchlistLayout l;
    capture_layout(l);
    return l.visible;
}

void WatchlistView::set_column_visible(const QString& id, bool visible) {
    const int c = wcols::index_of(id);
    if (c <= 0) // the symbol column always stays
        return;
    QHeaderView* h = header();
    if (h->isSectionHidden(c) == !visible)
        return;
    h->setSectionHidden(c, !visible);
    if (visible && h->sectionSize(c) < 20)
        h->resizeSection(c, default_width(c));
    emit layout_edited();
    emit visible_columns_changed();
}

void WatchlistView::clear_sort() {
    applying_ = true;
    header()->setSortIndicator(-1, Qt::AscendingOrder);
    if (proxy_)
        proxy_->sort(-1);
    applying_ = false;
    emit layout_edited();
}

bool WatchlistView::is_sorted() const {
    return header()->sortIndicatorSection() >= 0;
}

void WatchlistView::on_section_moved(int logical, int, int new_visual) {
    if (applying_)
        return;
    // The symbol column carries the group tree; keep it first.
    if (logical == 0 && new_visual != 0) {
        applying_ = true;
        header()->moveSection(new_visual, 0);
        applying_ = false;
    } else if (header()->visualIndex(0) != 0) {
        applying_ = true;
        header()->moveSection(header()->visualIndex(0), 0);
        applying_ = false;
    }
    emit layout_edited();
}

void WatchlistView::show_header_menu(const QPoint& pos) {
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QHash<QString, QMenu*> subs;
    for (const QString& cat : wcols::categories())
        subs.insert(cat, menu->addMenu(cat));
    QHeaderView* h = header();
    for (int c = 1; c < wcols::count(); ++c) {
        QMenu* sub = subs.value(wcols::category(c));
        if (!sub)
            continue;
        QAction* act = sub->addAction(source_ ? source_->headerData(c, Qt::Horizontal).toString() : wcols::header(c));
        act->setCheckable(true);
        act->setChecked(!h->isSectionHidden(c));
        act->setToolTip(wcols::tooltip(c));
        const QString id = wcols::id_at(c);
        connect(act, &QAction::toggled, this, [this, id](bool on) { set_column_visible(id, on); });
    }
    for (QMenu* sub : subs)
        sub->setToolTipsVisible(true);
    menu->addSeparator();
    const int at = h->logicalIndexAt(pos);
    if (at > 0) {
        connect(menu->addAction(tr("Hide \"%1\"").arg(wcols::header(at))), &QAction::triggered, this,
                [this, at]() { set_column_visible(wcols::id_at(at), false); });
    }
    connect(menu->addAction(tr("Fit columns to content")), &QAction::triggered, this, [this]() {
        for (int c = 0; c < wcols::count(); ++c) {
            if (header()->isSectionHidden(c))
                continue;
            const WatchlistColKind k = wcols::def(c).kind;
            if (k == WatchlistColKind::RangeBar || k == WatchlistColKind::Spark)
                continue;
            resizeColumnToContents(c);
            header()->resizeSection(c, std::max(header()->sectionSize(c), default_width(c) * 2 / 3));
        }
        emit layout_edited();
    });
    if (is_sorted())
        connect(menu->addAction(tr("Back to my order")), &QAction::triggered, this, &WatchlistView::clear_sort);
    connect(menu->addAction(tr("Reset columns")), &QAction::triggered, this, [this]() {
        WatchlistLayout cur;
        capture_layout(cur);
        WatchlistLayout def = WatchlistColumnPrefs::builtin_default();
        def.sort_id = cur.sort_id;
        def.sort_order = cur.sort_order;
        apply_layout(def);
        emit layout_edited();
    });
    menu->popup(h->mapToGlobal(pos));
}

QStringList WatchlistView::selected_symbols() const {
    QStringList out;
    if (!selectionModel())
        return out;
    const QModelIndexList rows = selectionModel()->selectedRows(0);
    // Display order: sort by visual position.
    QVector<QPair<int, QString>> keyed;
    for (const QModelIndex& idx : rows) {
        const QString sym = idx.data(WatchlistModel::SymbolRole).toString();
        if (!sym.isEmpty())
            keyed.append({visualRect(idx).top(), sym});
    }
    std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& k : keyed)
        if (!out.contains(k.second))
            out << k.second;
    return out;
}

QString WatchlistView::current_symbol() const {
    const QModelIndex cur = currentIndex();
    if (!cur.isValid())
        return {};
    const bool selected = selectionModel() && selectionModel()->isRowSelected(cur.row(), cur.parent());
    if (!selected) {
        const QStringList sel = selected_symbols();
        return sel.isEmpty() ? QString() : sel.first();
    }
    return cur.data(WatchlistModel::SymbolRole).toString();
}

void WatchlistView::select_symbols(const QStringList& symbols, const QString& current) {
    if (!source_ || !proxy_ || !selectionModel())
        return;
    QItemSelection sel;
    for (const QString& s : symbols) {
        const QModelIndex src = source_->index_of_symbol(s, 0);
        const QModelIndex idx = proxy_->mapFromSource(src);
        if (idx.isValid())
            sel.select(idx, idx);
    }
    QSignalBlocker block(selectionModel());
    selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (!current.isEmpty()) {
        const QModelIndex idx = proxy_->mapFromSource(source_->index_of_symbol(current, 0));
        if (idx.isValid())
            selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
    }
    viewport()->update();
}

QStringList WatchlistView::visible_symbols(int margin) const {
    QStringList out;
    QModelIndex idx = indexAt(QPoint(4, 2));
    if (!idx.isValid() && model() && model()->rowCount() > 0)
        idx = model()->index(0, 0);
    for (int i = 0; i < margin && idx.isValid(); ++i) {
        const QModelIndex up = indexAbove(idx);
        if (!up.isValid())
            break;
        idx = up;
    }
    int row_h = idx.isValid() ? rowHeight(idx) : 0;
    if (row_h <= 0)
        row_h = fontMetrics().height() + 6;
    const int limit = viewport()->height() / row_h + 2 + 2 * margin;
    for (int n = 0; idx.isValid() && n < limit; ++n) {
        const QString sym = idx.data(WatchlistModel::SymbolRole).toString();
        if (!sym.isEmpty())
            out << sym;
        idx = indexBelow(idx);
    }
    return out;
}

QStringList WatchlistView::display_order() const {
    QStringList out;
    if (!model())
        return out;
    const int top = model()->rowCount();
    for (int r = 0; r < top; ++r) {
        const QModelIndex idx = model()->index(r, 0);
        if (idx.data(WatchlistModel::IsGroupRole).toBool()) {
            const int n = model()->rowCount(idx);
            for (int k = 0; k < n; ++k)
                out << model()->index(k, 0, idx).data(WatchlistModel::SymbolRole).toString();
        } else {
            out << idx.data(WatchlistModel::SymbolRole).toString();
        }
    }
    return out;
}

QStringList WatchlistView::collapsed_groups() const {
    QStringList out;
    if (!model())
        return out;
    for (int r = 0; r < model()->rowCount(); ++r) {
        const QModelIndex idx = model()->index(r, 0);
        if (idx.data(WatchlistModel::IsGroupRole).toBool() && !isExpanded(idx))
            out << idx.data(WatchlistModel::GroupKeyRole).toString();
    }
    return out;
}

void WatchlistView::restore_expansion(const QStringList& collapsed) {
    if (!model())
        return;
    applying_ = true;
    const QSet<QString> closed(collapsed.cbegin(), collapsed.cend());
    for (int r = 0; r < model()->rowCount(); ++r) {
        const QModelIndex idx = model()->index(r, 0);
        if (!idx.data(WatchlistModel::IsGroupRole).toBool())
            continue;
        setExpanded(idx, !closed.contains(idx.data(WatchlistModel::GroupKeyRole).toString()));
    }
    applying_ = false;
}

void WatchlistView::startDrag(Qt::DropActions) {
    if (!source_ || !proxy_ || !selectionModel())
        return;
    QModelIndexList src;
    for (const QModelIndex& idx : selectionModel()->selectedRows(0))
        src << proxy_->mapToSource(idx);
    if (src.isEmpty())
        return;
    QMimeData* mime = source_->mimeData(src);
    if (!mime)
        return;
    const QStringList syms = selected_symbols();
    QString label = syms.isEmpty() ? QString() : syms.first();
    if (syms.size() > 1)
        label += QStringLiteral(" +%1").arg(syms.size() - 1);
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    const QPixmap pm = wlv_drag_pixmap(label);
    drag->setPixmap(pm);
    const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
    drag->setHotSpot(QPoint(static_cast<int>(pm.width() / (2 * dpr)), static_cast<int>(pm.height() / (2 * dpr))));
    // Rows are never removed by a drag (no clearOrRemove): a drop inside the
    // table reorders through the model's move handler, a drop on another panel
    // only reads the symbol.
    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void WatchlistView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QTreeView::drawRow(painter, option, index);
    if (!index.data(WatchlistModel::IsGroupRole).toBool())
        return;
    const QModelIndex first = index.sibling(index.row(), 0);
    const QString text = first.data(Qt::DisplayRole).toString();
    if (text.isEmpty())
        return;
    // From the symbol cell (after the branch indicator) through every following
    // visible cell that has nothing to show on a group row.
    // Geometry from the header (group rows are top level: one indentation step for
    // the branch indicator when the root is decorated).
    const QHeaderView* h = header();
    const int left = h->sectionViewportPosition(0) + (rootIsDecorated() ? indentation() : 0);
    int right = h->sectionViewportPosition(0) + h->sectionSize(0) - 1;
    for (int v = h->visualIndex(0) + 1; v < h->count(); ++v) {
        const int logical = h->logicalIndex(v);
        if (h->isSectionHidden(logical))
            continue;
        const QModelIndex sib = index.sibling(index.row(), logical);
        if (!sib.data(Qt::DisplayRole).toString().isEmpty() || itemDelegateForColumn(logical) != nullptr)
            break;
        right = h->sectionViewportPosition(logical) + h->sectionSize(logical) - 1;
    }
    const QRect r = QRect(QPoint(left, option.rect.top()), QPoint(right, option.rect.bottom())).adjusted(6, 0, -6, 0);
    if (r.width() <= 0)
        return;
    painter->save();
    QFont font = option.font;
    font.setBold(true);
    painter->setFont(font);
    const QColor fg = first.data(Qt::ForegroundRole).value<QColor>();
    painter->setPen(fg.isValid() ? fg : QColor(colors::AMBER()));
    painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      QFontMetrics(font).elidedText(text, Qt::ElideRight, r.width()));
    painter->restore();
}

void WatchlistView::scrollContentsBy(int dx, int dy) {
    QTreeView::scrollContentsBy(dx, dy);
    if (dy != 0)
        emit viewport_rows_changed();
}

void WatchlistView::resizeEvent(QResizeEvent* e) {
    QTreeView::resizeEvent(e);
    emit viewport_rows_changed();
}

// ── WatchlistFooter ─────────────────────────────────────────────────────────

WatchlistFooter::WatchlistFooter(QTreeView* view, QWidget* parent) : QWidget(parent), view_(view) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMouseTracking(true);
    if (view) {
        QHeaderView* h = view->header();
        const auto repaint = [this]() { update(); };
        connect(h, &QHeaderView::sectionResized, this, repaint);
        connect(h, &QHeaderView::sectionMoved, this, repaint);
        connect(h, &QHeaderView::geometriesChanged, this, repaint);
        connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, repaint);
    }
}

void WatchlistFooter::set_cells(const QHash<int, Cell>& cells) {
    cells_ = cells;
    update();
}

QSize WatchlistFooter::sizeHint() const {
    const QFontMetrics fm(fincept::ui::fonts::font(-1, QFont::DemiBold));
    return {200, fm.height() + 10};
}

int WatchlistFooter::column_at(int x) const {
    if (!view_)
        return -1;
    const int vx = view_->viewport()->geometry().x();
    return view_->header()->logicalIndexAt(x - vx);
}

bool WatchlistFooter::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        const int c = column_at(he->pos().x());
        const QString tip = cells_.value(c).tooltip;
        if (!tip.isEmpty())
            QToolTip::showText(he->globalPos(), tip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

void WatchlistFooter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(colors::BG_RAISED()));
    p.setPen(QColor(colors::BORDER_DIM()));
    p.drawLine(rect().topLeft(), rect().topRight());
    if (!view_)
        return;
    const QHeaderView* h = view_->header();
    const int vx = view_->viewport()->geometry().x();
    const int vw = view_->viewport()->width();
    const QFont font = fincept::ui::fonts::font(-1, QFont::DemiBold);
    p.setFont(font);
    const QFontMetrics fm(font);
    p.setClipRect(QRect(vx, 0, vw, height()));
    for (auto it = cells_.constBegin(); it != cells_.constEnd(); ++it) {
        const int c = it.key();
        if (c < 0 || c >= h->count() || h->isSectionHidden(c) || it->text.isEmpty())
            continue;
        const int x = vx + h->sectionViewportPosition(c);
        const int w = h->sectionSize(c);
        const QRect r(x + 6, 0, w - 12, height());
        Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter;
        const WatchlistColKind k = wcols::def(c).kind;
        if (k == WatchlistColKind::Symbol || k == WatchlistColKind::Text || k == WatchlistColKind::State)
            align = Qt::AlignLeft | Qt::AlignVCenter;
        p.setPen(it->color.isValid() ? it->color : QColor(colors::TEXT_SECONDARY()));
        p.drawText(r, static_cast<int>(align) | Qt::TextSingleLine, fm.elidedText(it->text, Qt::ElideRight, r.width()));
    }
}

} // namespace fincept::screens
