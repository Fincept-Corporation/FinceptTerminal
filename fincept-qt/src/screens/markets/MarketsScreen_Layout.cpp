// MarketsScreen_Layout.cpp — the panel grid of the Markets screen: building and
// re-flowing the splitter grid (3 / 2 / 1 columns by width), moving and maximising
// panels, horizontal / vertical splitter state, and panel / symbol add, edit and
// remove (with one level of undo). See MarketsScreen.h.

#include "screens/markets/MarketsScreen.h"

#include "screens/markets/MarketPanel.h"
#include "screens/markets/MarketPanelEditor.h"
#include "screens/markets/MarketPanelModel.h"
#include "screens/markets/MarketPanelStore.h"
#include "screens/markets/MarketPanelView.h"
#include "screens/markets/MarketsLogic.h"
#include "screens/markets/MarketsQuoteStore.h"
#include "screens/markets/MarketsStyle.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/widgets/ScreenStatusBar.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

namespace {
constexpr int kSavedColumns = 3; // the saved layout is always three columns
constexpr int kHandlePx = 3;

/// The grid as shown, for markets_logic::panel_drop_target_at: each column splitter's
/// horizontal extent and its panels top to bottom, in `host` coordinates. In the saved
/// 3-column grid an empty column (its placeholder only) carries its saved index.
QVector<markets_logic::PanelDropColumn> msl_shown_columns(const QVector<QSplitter*>& splitters, QWidget* host,
                                                          bool saved_layout) {
    QVector<markets_logic::PanelDropColumn> out;
    out.reserve(splitters.size());
    for (int c = 0; c < splitters.size(); ++c) {
        QSplitter* vs = splitters[c];
        markets_logic::PanelDropColumn col;
        const QPoint origin = vs->mapTo(host, QPoint(0, 0));
        col.left = origin.x();
        col.right = origin.x() + vs->width() - 1;
        for (int i = 0; i < vs->count(); ++i) {
            auto* p = qobject_cast<MarketPanel*>(vs->widget(i));
            if (!p || p->isHidden())
                continue;
            const int top = p->mapTo(host, QPoint(0, 0)).y();
            col.panels.append({p->panel_id(), top, top + p->height() - 1});
        }
        if (col.panels.isEmpty() && saved_layout)
            col.saved_column = c;
        out.append(col);
    }
    return out;
}
} // namespace

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------

MarketPanel* MarketsScreen::create_panel(const MarketPanelConfig& cfg, QWidget* parent) {
    auto* p = new MarketPanel(cfg, store_, parent);
    wire_panel(p);
    p->set_link_group(static_cast<int>(link_group_));
    if (!resolver_notes_.isEmpty() || !resolver_successors_.isEmpty())
        p->set_resolver_findings(resolver_notes_, resolver_successors_);
    panels_.insert(cfg.id, p);
    return p;
}

void MarketsScreen::create_panels() {
    for (const auto& cfg : std::as_const(configs_))
        if (!panels_.contains(cfg.id))
            create_panel(cfg, grid_host_);
}

void MarketsScreen::wire_panel(MarketPanel* p) {
    connect(p, &MarketPanel::edit_requested, this, &MarketsScreen::open_editor);
    connect(p, &MarketPanel::delete_requested, this, &MarketsScreen::on_panel_delete);
    connect(p, &MarketPanel::config_changed, this, &MarketsScreen::on_panel_config_changed);
    connect(p, &MarketPanel::symbol_selected, this, &MarketsScreen::on_symbol_selected);
    connect(p, &MarketPanel::symbol_activated, this, &MarketsScreen::on_symbol_activated);
    connect(p, &MarketPanel::maximise_requested, this, &MarketsScreen::toggle_maximise);
    connect(p, &MarketPanel::move_requested, this, &MarketsScreen::move_panel);
    connect(p, &MarketPanel::header_drag_moved, this, &MarketsScreen::on_panel_drag_moved);
    connect(p, &MarketPanel::header_drag_finished, this, &MarketsScreen::on_panel_drag_finished);
    connect(p, &MarketPanel::detail_requested, this, [this](const QString& s) { on_detail_requested(s, false); });
    connect(p, &MarketPanel::detail_toggle_requested, this, [this](const QString& s) { on_detail_requested(s, true); });
    connect(p, &MarketPanel::remove_symbol_requested, this, &MarketsScreen::remove_symbol);
    connect(p, &MarketPanel::add_symbol_requested, this, [this](const QString& id, const QString& s) {
        add_symbol(id, markets_logic::canonical_or_kept(s), true);
    });
    connect(p, &MarketPanel::focus_panel_requested, this, &MarketsScreen::focus_panel);
    connect(p, &MarketPanel::escape_pressed, this, [this]() {
        if (!maximised_id_.isEmpty())
            restore_maximised();
    });
    connect(p, &MarketPanel::viewport_rows_changed, this, &MarketsScreen::schedule_sparklines);
    connect(p, &MarketPanel::replace_symbol_requested, this, &MarketsScreen::replace_symbol);
    connect(p, &MarketPanel::unresolved_entry, this, [this](const QString& message) {
        status_bar_->show_notice(message, {}, {}, 7000, ui::ScreenStatusBar::Tone::Warn);
    });
}

MarketPanel* MarketsScreen::panel_by_id(const QString& id) const {
    return id.isEmpty() ? nullptr : panels_.value(id, nullptr);
}

MarketPanelConfig* MarketsScreen::config_by_id(const QString& id) {
    for (auto& c : configs_)
        if (c.id == id)
            return &c;
    return nullptr;
}

QVector<QStringList> MarketsScreen::saved_columns() const {
    // The saved three-column layout: per column, panel ids by splitter_index.
    QVector<QVector<QPair<int, QString>>> keyed(kSavedColumns);
    for (const auto& c : configs_)
        keyed[std::clamp(c.column_index, 0, kSavedColumns - 1)].append({c.splitter_index, c.id});
    QVector<QStringList> cols(kSavedColumns);
    for (int i = 0; i < kSavedColumns; ++i) {
        std::stable_sort(keyed[i].begin(), keyed[i].end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& k : keyed[i])
            cols[i].append(k.second);
    }
    return cols;
}

QVector<MarketPanel*> MarketsScreen::panels_in_reading_order() const {
    QVector<MarketPanel*> out;
    for (const QString& id : markets_logic::reading_order(saved_columns()))
        if (MarketPanel* p = panel_by_id(id))
            out.append(p);
    return out;
}

QStringList MarketsScreen::all_panel_symbols(const QString& except_panel) const {
    QStringList out;
    QSet<QString> seen;
    for (const auto& c : configs_) {
        if (c.id == except_panel)
            continue;
        for (const QString& s : c.symbols)
            if (!seen.contains(s)) {
                seen.insert(s);
                out << s;
            }
    }
    return out;
}

void MarketsScreen::renumber_configs() {
    const QVector<QStringList> cols = saved_columns();
    for (int c = 0; c < cols.size(); ++c)
        for (int i = 0; i < cols[c].size(); ++i)
            if (MarketPanelConfig* cfg = config_by_id(cols[c][i])) {
                cfg->column_index = c;
                cfg->splitter_index = i;
            }
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------

int MarketsScreen::effective_columns() const {
    // Before the first show the geometry is a default, not the real width: build the
    // saved three-column grid; the first resize re-flows once if the window is narrow.
    if (!isVisible())
        return kSavedColumns;
    int w = grid_scroll_->viewport()->width();
    if (w <= 0)
        w = view_stack_->width() > 0 ? view_stack_->width() : width();
    return markets_logic::grid_columns_for_width(w, kSavedColumns);
}

void MarketsScreen::schedule_reflow() {
    reflow_timer_->start();
}

void MarketsScreen::apply_reflow() {
    apply_detail_visibility(); // may change the grid's width first
    if (effective_columns() != grid_columns_)
        build_grid();
}

void MarketsScreen::build_grid() {
    if (!maximised_id_.isEmpty())
        restore_maximised();
    if (h_splitter_ && grid_columns_ == kSavedColumns) {
        save_vsplit_state();
        save_splitter_state();
    }
    const int n = effective_columns();
    QVector<QStringList> cols = saved_columns();
    if (n < kSavedColumns)
        cols = markets_logic::reflow(markets_logic::reading_order(cols), n);

    // Panels persist across rebuilds: move them out of the old splitters first.
    if (h_splitter_) {
        for (MarketPanel* p : std::as_const(panels_))
            if (h_splitter_->isAncestorOf(p))
                p->setParent(grid_host_);
        h_splitter_->hide();
        h_splitter_->deleteLater();
        h_splitter_ = nullptr;
    }
    col_splitters_.clear();

    h_splitter_ = new QSplitter(Qt::Horizontal, grid_host_);
    h_splitter_->setHandleWidth(kHandlePx);
    h_splitter_->setChildrenCollapsible(false);
    grid_host_->layout()->addWidget(h_splitter_);
    for (int c = 0; c < n; ++c) {
        auto* vs = new QSplitter(Qt::Vertical, h_splitter_);
        vs->setHandleWidth(kHandlePx);
        vs->setChildrenCollapsible(false);
        h_splitter_->addWidget(vs);
        col_splitters_.append(vs);
        for (const QString& id : cols.value(c)) {
            if (MarketPanel* p = panel_by_id(id)) {
                vs->addWidget(p);
                p->show();
            }
        }
        if (vs->count() == 0) { // an empty saved column keeps its width
            auto* placeholder = new QWidget(vs);
            placeholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            vs->addWidget(placeholder);
        }
        connect(vs, &QSplitter::splitterMoved, this, [this]() { vsplit_save_timer_->start(); });
    }
    connect(h_splitter_, &QSplitter::splitterMoved, this, [this]() { vsplit_save_timer_->start(); });
    grid_columns_ = n;

    // Sizes: the user's (persisted) when the column still holds the same panels,
    // else first-run proportional heights; widths restored for the 3-column layout.
    if (!(n == kSavedColumns && restore_vsplit_state())) {
        for (QSplitter* vs : std::as_const(col_splitters_)) {
            QVector<int> counts;
            for (int i = 0; i < vs->count(); ++i) {
                auto* p = qobject_cast<MarketPanel*>(vs->widget(i));
                counts.append(p ? static_cast<int>(p->config().symbols.size()) : 0);
            }
            vs->setSizes(markets_logic::proportional_sizes(counts));
        }
    }
    if (n == kSavedColumns)
        restore_splitter_state();
    else
        h_splitter_->setSizes(QList<int>(n, 1000));
    apply_compact_heights();

    const bool empty = configs_.isEmpty();
    no_panels_->setVisible(empty);
    h_splitter_->setVisible(!empty);
    schedule_sparklines();
}

void MarketsScreen::apply_compact_heights() {
    // 2- / 1-column mode: the grid scrolls and each panel keeps header + column header +
    // min(rows, 8) rows, so nothing is clipped off-screen. 3 columns: the panels share the
    // height (two rows minimum each).
    const int row_h = ui::metrics::row_height(-2, 22, 6);
    const int chrome = ui::metrics::height(30) + ui::metrics::height(22) + 4;
    for (MarketPanel* p : std::as_const(panels_)) {
        const int rows = static_cast<int>(p->config().symbols.size());
        const int h = grid_columns_ < kSavedColumns ? chrome + std::clamp(rows, 2, 8) * row_h : chrome + 2 * row_h;
        if (p->minimumHeight() != h)
            p->setMinimumHeight(h);
    }
}

void MarketsScreen::equalise_panels() {
    for (QSplitter* vs : std::as_const(col_splitters_))
        vs->setSizes(QList<int>(vs->count(), 1000));
    vsplit_save_timer_->start();
}

// ---------------------------------------------------------------------------
// Splitter state
// ---------------------------------------------------------------------------

void MarketsScreen::save_splitter_state() {
    if (!h_splitter_ || grid_columns_ != kSavedColumns || h_splitter_->count() != kSavedColumns)
        return;
    const QString state = QString::fromLatin1(h_splitter_->saveState().toBase64());
    (void)QtConcurrent::run([state]() { SettingsRepository::instance().set("markets_splitter_state", state); });
}

void MarketsScreen::restore_splitter_state() {
    if (!h_splitter_ || h_splitter_->count() != kSavedColumns)
        return;
    const auto res = SettingsRepository::instance().get("markets_splitter_state");
    bool restored = false;
    if (res.is_ok() && !res.value().isEmpty()) {
        const QString saved = res.value();
        const int sep = static_cast<int>(saved.indexOf(QLatin1Char('|'))); // legacy "h|v" form
        restored = h_splitter_->restoreState(QByteArray::fromBase64((sep < 0 ? saved : saved.left(sep)).toLatin1()));
    }
    if (!restored)
        h_splitter_->setSizes(QList<int>(kSavedColumns, 1000));
}

void MarketsScreen::save_vsplit_state() {
    if (grid_columns_ != kSavedColumns || col_splitters_.isEmpty() || !maximised_id_.isEmpty())
        return;
    QJsonArray cols;
    for (QSplitter* vs : std::as_const(col_splitters_)) {
        QJsonArray ids;
        for (int i = 0; i < vs->count(); ++i)
            if (auto* p = qobject_cast<MarketPanel*>(vs->widget(i)))
                ids.append(p->panel_id());
        QJsonObject col;
        col["ids"] = ids;
        col["state"] = QString::fromLatin1(vs->saveState().toBase64());
        cols.append(col);
    }
    QJsonObject root;
    root["cols"] = cols;
    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    (void)QtConcurrent::run(
        [json]() { SettingsRepository::instance().set("markets_vsplit_state", json, "market_data"); });
}

bool MarketsScreen::restore_vsplit_state() {
    const auto res = SettingsRepository::instance().get("markets_vsplit_state");
    if (!res.is_ok() || res.value().isEmpty())
        return false;
    const QJsonArray cols = QJsonDocument::fromJson(res.value().toUtf8()).object().value("cols").toArray();
    if (cols.size() != col_splitters_.size())
        return false;
    bool all = true;
    for (int c = 0; c < col_splitters_.size(); ++c) {
        QSplitter* vs = col_splitters_[c];
        QStringList now;
        for (int i = 0; i < vs->count(); ++i)
            if (auto* p = qobject_cast<MarketPanel*>(vs->widget(i)))
                now << p->panel_id();
        QStringList saved;
        const QJsonObject col = cols.at(c).toObject();
        for (const auto& v : col.value("ids").toArray())
            saved << v.toString();
        // A column's sizes come back only when it holds exactly the panels it held.
        if (saved != now || !vs->restoreState(QByteArray::fromBase64(col.value("state").toString().toLatin1())))
            all = false;
    }
    return all;
}

// ---------------------------------------------------------------------------
// Move / maximise / focus
// ---------------------------------------------------------------------------

void MarketsScreen::move_panel(const QString& panel_id, Qt::ArrowType direction) {
    MarketPanelConfig* cfg = config_by_id(panel_id);
    if (!cfg || !maximised_id_.isEmpty())
        return;
    QVector<QStringList> cols = saved_columns();
    const int c = std::clamp(cfg->column_index, 0, kSavedColumns - 1);
    const int i = static_cast<int>(cols[c].indexOf(panel_id));
    if (i < 0)
        return;
    switch (direction) {
        case Qt::UpArrow:
        case Qt::DownArrow: {
            const int j = i + (direction == Qt::UpArrow ? -1 : 1);
            if (j < 0 || j >= cols[c].size())
                return;
            cols[c].swapItemsAt(i, j);
            break;
        }
        case Qt::LeftArrow:
        case Qt::RightArrow: {
            const int d = c + (direction == Qt::LeftArrow ? -1 : 1);
            if (d < 0 || d >= kSavedColumns)
                return;
            cols[c].removeAt(i);
            cols[d].insert(std::min(i, static_cast<int>(cols[d].size())), panel_id);
            break;
        }
        default:
            return;
    }
    apply_saved_columns(cols, panel_id);
}

void MarketsScreen::apply_saved_columns(const QVector<QStringList>& cols, const QString& focus_id) {
    for (int col = 0; col < cols.size(); ++col)
        for (int k = 0; k < cols[col].size(); ++k)
            if (MarketPanelConfig* x = config_by_id(cols[col][k])) {
                x->column_index = col;
                x->splitter_index = k;
            }
    schedule_config_save();
    build_grid();
    push_panel_symbols();
    if (MarketPanel* p = panel_by_id(focus_id))
        p->view()->setFocus(Qt::OtherFocusReason);
}

// ---------------------------------------------------------------------------
// Drag-to-reorder by a panel header (the Move menu / Ctrl+Shift+Arrow are the keyboard way)
// ---------------------------------------------------------------------------

void MarketsScreen::on_panel_drag_moved(const QString& panel_id, const QPoint& global_pos) {
    // A scrolling grid (2 / 1 columns) scrolls when the pointer nears its top / bottom.
    if (grid_scroll_ && view_stack_->currentWidget() == grid_stack_ && grid_stack_->currentWidget() == grid_scroll_) {
        QWidget* vp = grid_scroll_->viewport();
        const QPoint in_vp = vp->mapFromGlobal(global_pos);
        QScrollBar* sb = grid_scroll_->verticalScrollBar();
        if (sb && sb->maximum() > 0 && in_vp.x() >= 0 && in_vp.x() < vp->width()) {
            const int edge = ui::metrics::height(24);
            if (in_vp.y() < edge && in_vp.y() >= -edge)
                sb->setValue(sb->value() - edge / 2);
            else if (in_vp.y() > vp->height() - edge && in_vp.y() <= vp->height() + edge)
                sb->setValue(sb->value() + edge / 2);
        }
    }
    int column = -1;
    int index = -1;
    update_drop_bar(panel_id, global_pos, &column, &index);
}

void MarketsScreen::on_panel_drag_finished(const QString& panel_id, const QPoint& global_pos, bool drop) {
    int column = -1;
    int index = -1;
    // A release outside the grid, or where the panel already is, changes nothing.
    const bool moves = drop && update_drop_bar(panel_id, global_pos, &column, &index);
    if (drop_bar_)
        drop_bar_->hide();
    if (!moves)
        return;
    bool changed = false;
    const QVector<QStringList> cols =
        markets_logic::move_panel_to_slot(saved_columns(), panel_id, column, index, &changed);
    if (changed)
        apply_saved_columns(cols, panel_id); // the Move menu's path: configs, save, rebuild
}

bool MarketsScreen::update_drop_bar(const QString& panel_id, const QPoint& global_pos, int* column, int* index) {
    const auto hide_bar = [this]() {
        if (drop_bar_ && drop_bar_->isVisible())
            drop_bar_->hide();
    };
    // The grid page of PANELS only, never with a maximised panel.
    if (!maximised_id_.isEmpty() || view_stack_->currentWidget() != grid_stack_ ||
        grid_stack_->currentWidget() != grid_scroll_ || col_splitters_.isEmpty() || !config_by_id(panel_id)) {
        hide_bar();
        return false;
    }
    QWidget* vp = grid_scroll_->viewport();
    if (!vp->rect().contains(vp->mapFromGlobal(global_pos))) { // released / moved off the grid
        hide_bar();
        return false;
    }
    const QVector<markets_logic::PanelDropColumn> shown =
        msl_shown_columns(col_splitters_, grid_host_, grid_columns_ == kSavedColumns);
    const QPoint at = grid_host_->mapFromGlobal(global_pos);
    const markets_logic::PanelDropTarget target = markets_logic::panel_drop_target_at(shown, at.x(), at.y());
    const QVector<QStringList> saved = saved_columns();
    int c = -1;
    int i = -1;
    if (!target.valid || !markets_logic::saved_drop_slot(saved, target, &c, &i)) {
        hide_bar();
        return false;
    }
    bool changed = false;
    (void)markets_logic::move_panel_to_slot(saved, panel_id, c, i, &changed);
    if (!changed) {
        hide_bar(); // where it already is: no bar, nothing to drop
        return false;
    }

    if (!drop_bar_) {
        drop_bar_ = new QWidget(grid_host_); // styled by the root sheet (#msDropBar)
        drop_bar_->setObjectName(QStringLiteral("msDropBar"));
        drop_bar_->setAttribute(Qt::WA_StyledBackground, true);
        drop_bar_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        drop_bar_->setFocusPolicy(Qt::NoFocus);
        drop_bar_->hide();
    }
    const markets_logic::PanelDropColumn& col = shown[target.column];
    const bool frame = target.anchor.isEmpty(); // an empty saved column takes the panel
    QRect r;
    if (frame) {
        QSplitter* vs = col_splitters_[target.column];
        r = QRect(vs->mapTo(grid_host_, QPoint(0, 0)), vs->size());
    } else {
        // An accent bar across the column, centred on the gap above / below the anchor.
        const int thick = std::max(3, QFontMetrics(font()).height() / 5);
        int y = 0;
        for (const auto& p : col.panels)
            if (p.id == target.anchor) {
                y = target.after ? p.bottom + 1 + kHandlePx / 2 : p.top - 1 - kHandlePx / 2;
                break;
            }
        const int top = std::clamp(y - thick / 2, 0, std::max(0, grid_host_->height() - thick));
        r = QRect(col.left, top, col.right - col.left + 1, thick);
    }
    const QString kind = frame ? QStringLiteral("column") : QStringLiteral("bar");
    if (drop_bar_->property("kind").toString() != kind) {
        drop_bar_->setProperty("kind", kind);
        drop_bar_->style()->unpolish(drop_bar_);
        drop_bar_->style()->polish(drop_bar_);
    }
    if (drop_bar_->geometry() != r)
        drop_bar_->setGeometry(r);
    drop_bar_->raise();
    if (!drop_bar_->isVisible())
        drop_bar_->show();
    *column = c;
    *index = i;
    return true;
}

void MarketsScreen::toggle_maximise(const QString& panel_id) {
    if (maximised_id_ == panel_id) {
        restore_maximised();
        return;
    }
    if (!maximised_id_.isEmpty())
        restore_maximised();
    MarketPanel* p = panel_by_id(panel_id);
    if (!p)
        return;
    save_vsplit_state();
    max_split_ = qobject_cast<QSplitter*>(p->parentWidget());
    max_index_ = max_split_ ? max_split_->indexOf(p) : -1;
    max_split_state_ = max_split_ ? max_split_->saveState() : QByteArray();
    max_page_->layout()->addWidget(p);
    p->show();
    p->set_maximised(true);
    maximised_id_ = panel_id;
    set_view(0, false);
    grid_stack_->setCurrentWidget(max_page_);
    p->view()->update_auto_hide(); // every optional column that fits
    p->view()->setFocus(Qt::OtherFocusReason);
    push_panel_symbols(); // the maximised panel loads first
}

void MarketsScreen::restore_maximised() {
    MarketPanel* p = panel_by_id(maximised_id_);
    const QString id = maximised_id_;
    maximised_id_.clear();
    grid_stack_->setCurrentWidget(grid_scroll_);
    if (!p)
        return;
    p->set_maximised(false);
    if (max_split_ && max_index_ >= 0) {
        max_split_->insertWidget(std::min(max_index_, max_split_->count()), p);
        p->show();
        if (!max_split_state_.isEmpty())
            max_split_->restoreState(max_split_state_);
    } else {
        build_grid();
    }
    max_split_ = nullptr;
    max_index_ = -1;
    p->view()->setFocus(Qt::OtherFocusReason);
    Q_UNUSED(id);
}

void MarketsScreen::focus_panel(const QString& panel_id, int delta) {
    const QVector<MarketPanel*> order = panels_in_reading_order();
    if (order.isEmpty())
        return;
    int i = 0;
    for (int k = 0; k < order.size(); ++k)
        if (order[k]->panel_id() == panel_id)
            i = k;
    const int n = static_cast<int>(order.size());
    MarketPanel* next = order[((i + delta) % n + n) % n];
    MarketPanelView* v = next->view();
    v->setFocus(Qt::OtherFocusReason);
    if (v->current_symbol().isEmpty() && v->model() && v->model()->rowCount() > 0)
        v->setCurrentIndex(v->model()->index(0, 0)); // a current row (the first) to continue from
    last_table_ = v;
}

// ---------------------------------------------------------------------------
// Panel add / edit / remove
// ---------------------------------------------------------------------------

void MarketsScreen::open_editor(const QString& panel_id) {
    MarketPanelConfig* existing = config_by_id(panel_id);
    if (!existing)
        return;
    auto* dlg = new MarketPanelEditor(*existing, this);
    dlg->set_other_symbols(all_panel_symbols(panel_id));
    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }
    MarketPanelConfig updated = dlg->result_config();
    dlg->deleteLater();
    existing = config_by_id(panel_id); // the list may have changed while the dialog was open
    if (!existing)
        return;
    updated.column_index = existing->column_index;
    updated.splitter_index = existing->splitter_index;
    updated.columns = existing->columns;
    updated.sort_id = existing->sort_id;
    updated.sort_desc = existing->sort_desc;
    updated.widths = existing->widths;
    *existing = updated;
    if (MarketPanel* p = panel_by_id(panel_id))
        p->update_config(updated);
    flush_config_save();
    push_panel_symbols(); // the store loads what is new
}

void MarketsScreen::add_panel(const QStringList& symbols) {
    MarketPanelConfig cfg;
    cfg.symbols = symbols;
    auto* dlg = new MarketPanelEditor(cfg, this);
    dlg->set_other_symbols(all_panel_symbols());
    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }
    MarketPanelConfig created = dlg->result_config();
    dlg->deleteLater();
    if (created.columns.isEmpty())
        created.columns = default_market_columns();
    // Into the column with the fewest panels, at its end.
    const QVector<QStringList> cols = saved_columns();
    int best = 0;
    for (int c = 1; c < cols.size(); ++c)
        if (cols[c].size() < cols[best].size())
            best = c;
    created.column_index = best;
    created.splitter_index = static_cast<int>(cols[best].size());
    configs_.append(created);
    create_panel(created, grid_host_);
    flush_config_save();
    build_grid();
    push_panel_symbols();
    if (MarketPanel* p = panel_by_id(created.id))
        p->view()->setFocus(Qt::OtherFocusReason);
}

void MarketsScreen::on_panel_delete(const QString& panel_id) {
    // Immediate and undoable (one level): no confirmation dialog.
    int pos = -1;
    for (int i = 0; i < configs_.size(); ++i)
        if (configs_[i].id == panel_id)
            pos = i;
    if (pos < 0)
        return;
    if (maximised_id_ == panel_id)
        restore_maximised();
    undo_ = Undo{};
    undo_.kind = Undo::Kind::Panel;
    undo_.panel = configs_.at(pos);
    undo_.panel_pos = pos;
    configs_.removeAt(pos);
    if (MarketPanel* p = panels_.take(panel_id)) {
        p->hide();
        p->deleteLater();
    }
    renumber_configs();
    flush_config_save();
    build_grid();
    push_panel_symbols();
    status_bar_->show_notice(tr("Removed the panel \"%1\".").arg(undo_.panel.title), tr("UNDO"), [this]() {
        if (undo_.kind != Undo::Kind::Panel || config_by_id(undo_.panel.id))
            return;
        configs_.insert(std::clamp(undo_.panel_pos, 0, static_cast<int>(configs_.size())), undo_.panel);
        create_panel(undo_.panel, grid_host_);
        undo_ = Undo{};
        flush_config_save();
        build_grid();
        push_panel_symbols();
    });
}

void MarketsScreen::reset_panels() {
    // Not undoable in one step: confirmed, with the application's own dialog styling.
    if (QMessageBox::question(this, tr("Reset Panels"), tr("Reset all panels to defaults?"),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    if (!maximised_id_.isEmpty())
        restore_maximised();
    MarketPanelStore::instance().reset_to_defaults();
    configs_ = MarketPanelStore::instance().load();
    for (MarketPanel* p : std::as_const(panels_)) {
        p->hide();
        p->deleteLater();
    }
    panels_.clear();
    undo_ = Undo{};
    create_panels();
    (void)QtConcurrent::run([]() { SettingsRepository::instance().set("markets_vsplit_state", QString(), "market_data"); });
    build_grid();
    push_panel_symbols();
    update_banner();
}

void MarketsScreen::on_panel_config_changed(const MarketPanelConfig& cfg) {
    MarketPanelConfig* c = config_by_id(cfg.id);
    if (!c)
        return;
    const bool symbols_changed = c->symbols != cfg.symbols;
    // The panel's copy may carry an old position: the screen owns column / splitter index.
    const int ci = c->column_index;
    const int si = c->splitter_index;
    *c = cfg;
    c->column_index = ci;
    c->splitter_index = si;
    if (symbols_changed)
        push_panel_symbols();
    schedule_config_save(); // column drags emit per pixel: debounced 700 ms
}

void MarketsScreen::remove_symbol(const QString& panel_id, const QString& symbol) {
    MarketPanel* p = panel_by_id(panel_id);
    if (!p)
        return;
    const int at = static_cast<int>(p->config().symbols.indexOf(symbol));
    if (at < 0 || !p->remove_symbol(symbol))
        return;
    undo_ = Undo{};
    undo_.kind = Undo::Kind::Symbol;
    undo_.panel_id = panel_id;
    undo_.symbol = symbol;
    undo_.index = at;
    status_bar_->show_notice(tr("Removed %1 from %2.").arg(store_->display_name(symbol), p->config().title), tr("UNDO"),
                             [this]() {
                                 if (undo_.kind != Undo::Kind::Symbol)
                                     return;
                                 const Undo u = undo_;
                                 undo_ = Undo{};
                                 add_symbol(u.panel_id, u.symbol, true, u.index);
                             });
}

bool MarketsScreen::add_symbol(const QString& panel_id, const QString& symbol, bool select, int at) {
    MarketPanel* p = panel_by_id(panel_id);
    if (!p || symbol.isEmpty())
        return false;
    if (p->config().symbols.contains(symbol)) {
        if (select)
            p->view()->select_symbol(symbol, true);
        return true;
    }
    if (p->config().symbols.size() >= markets_logic::kMaxSymbolsPerPanel) {
        status_bar_->show_notice(tr("A panel can hold up to %1 symbols.").arg(markets_logic::kMaxSymbolsPerPanel), {},
                                 {}, 7000, ui::ScreenStatusBar::Tone::Warn);
        return false;
    }
    const QStringList all = all_panel_symbols();
    if (!all.contains(symbol) && all.size() >= markets_logic::kMaxSymbolsOnScreen) {
        status_bar_->show_notice(tr("This layout already tracks %1 symbols. Remove some before adding more.")
                                     .arg(markets_logic::kMaxSymbolsOnScreen),
                                 {}, {}, 7000, ui::ScreenStatusBar::Tone::Warn);
        return false;
    }
    if (!p->insert_symbol(symbol, at)) // emits config_changed -> configs_ + store
        return false;
    if (select) {
        p->view()->select_symbol(symbol, true);
        p->view()->setFocus(Qt::OtherFocusReason);
    }
    return true;
}

} // namespace fincept::screens
