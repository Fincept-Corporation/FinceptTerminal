#pragma once
// MarketPanelView — the table of one Markets panel: a flat QTreeView over
// MarketPanelProxy (QTreeView for the same reasons Watchlist uses it: cheap uniform
// rows, per-pixel scrolling). Installs the delegates (session-dot name cell, tick
// flash, heat CHG %, sparkline, range bars, relative-volume bar), computes column
// widths arithmetically (never from content), owns the header column chooser and the
// view-only column auto-hide, the keyboard of spec 3.1, the symbol drag source / drop
// target and the row context-menu request.

#include "core/symbol/SymbolGroup.h"
#include "screens/markets/MarketPanelConfig.h"

#include <QSet>
#include <QStringList>
#include <QTreeView>

namespace fincept::screens {

class MarketPanelModel;
class MarketPanelProxy;

class MarketPanelView : public QTreeView {
    Q_OBJECT
  public:
    explicit MarketPanelView(QWidget* parent = nullptr);

    void set_models(MarketPanelModel* model, MarketPanelProxy* proxy);
    MarketPanelModel* panel_model() const { return model_; }
    MarketPanelProxy* panel_proxy() const { return proxy_; }

    /// Visible columns, order, widths and sort from `cfg` (columns empty = defaults).
    void apply_config(const MarketPanelConfig& cfg);
    /// Write the view's columns / widths / sort into `cfg` (auto-hidden columns count as visible).
    void capture_config(MarketPanelConfig& cfg) const;
    /// Ids the user has chosen to show, in display order (auto-hidden ones included).
    QStringList chosen_column_ids() const;
    void set_column_visible(const QString& id, bool visible);
    /// Default width of a column at the current font (arithmetic, never measured).
    int default_width(int column) const;
    void reset_widths();
    bool is_sorted() const;
    void clear_sort();
    /// Re-sort a sorted panel (the proxy never re-sorts on ticks).
    void resort();

    QString current_symbol() const;
    void select_symbol(const QString& symbol, bool center);
    void clear_selection_silently();
    /// Symbols of the rows in or near the viewport (`margin` rows above / below).
    QStringList visible_symbols(int margin) const;
    /// Symbols in display (sorted) order.
    QStringList display_order() const;
    void set_link_group(fincept::SymbolGroup group) { link_group_ = group; }

    /// Font-derived metrics (row height, header height, default widths) after an
    /// appearance change — call on ThemeManager::appearance_settled, not theme_changed.
    void refresh_metrics();
    /// Re-run the column auto-hide for the current width.
    void update_auto_hide();

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_activated(const QString& symbol);
    void layout_edited();
    void context_menu_requested(const QString& symbol, const QPoint& global_pos);
    void remove_requested(const QString& symbol);
    void move_requested(const QString& symbol, int delta);
    void viewport_rows_changed();
    void add_row_requested();
    void detail_toggle_requested(const QString& symbol);
    void maximise_requested();
    void panel_move_requested(Qt::ArrowType direction);
    void focus_panel_requested(int delta);
    void escape_pressed();
    void symbol_dropped(const QString& symbol);

  protected:
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

  private:
    void install_delegates();
    void show_header_menu(const QPoint& pos);
    void on_section_moved(int logical, int old_visual, int new_visual);
    QString symbol_at_proxy_row(int row) const;

    MarketPanelModel* model_ = nullptr;
    MarketPanelProxy* proxy_ = nullptr;
    QSet<int> chosen_;        ///< columns the user wants visible
    QSet<int> auto_hidden_;   ///< chosen columns hidden for width (view-only)
    QHash<QString, int> saved_widths_; ///< id -> px the user set (others follow the font)
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    bool applying_ = false;
    bool suppress_selection_signal_ = false;
};

} // namespace fincept::screens
