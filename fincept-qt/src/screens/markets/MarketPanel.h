#pragma once
#include "screens/markets/MarketPanelConfig.h"

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

class QLabel;
class QToolButton;
class QStackedWidget;
class QEvent;

namespace fincept::screens {

class MarketsQuoteStore;
class MarketPanelModel;
class MarketPanelProxy;
class MarketPanelView;

/// One Markets panel: the shared panel header (3x14 accent tick, title, count badge,
/// add / menu icons) over a MarketPanelView of the panel's symbols. Every configured
/// symbol is a row and carries its own state (spec 3.6); there is no per-panel loading
/// or error page. Data comes from the screen's MarketsQuoteStore; the panel never talks
/// to the DataHub.
class MarketPanel : public QWidget {
    Q_OBJECT
  public:
    MarketPanel(const MarketPanelConfig& config, MarketsQuoteStore* store, QWidget* parent = nullptr);

    const QString& panel_id() const { return config_.id; }
    const MarketPanelConfig& config() const { return config_; }
    void update_config(const MarketPanelConfig& cfg);
    MarketPanelView* view() const { return view_; }
    MarketPanelModel* model() const { return model_; }

    void set_maximised(bool on);
    bool is_maximised() const { return maximised_; }
    /// Colours are read live; repaint every cell (theme_changed / format settings).
    void refresh_theme();
    /// Font-derived sizes (ThemeManager::appearance_settled).
    void refresh_metrics();
    void set_link_group(int group);

    /// Add / remove / move a symbol in this panel (keeps the model, the view's
    /// selection and config_ in step); each emits config_changed.
    bool insert_symbol(const QString& symbol, int at = -1);
    bool remove_symbol(const QString& symbol);
    void move_symbol(const QString& symbol, int delta);
    void move_symbol_to_end(const QString& symbol, bool top);

    /// Visible columns in display order and rows in display order, for CSV export.
    void export_table(bool with_panel_column, QStringList* headers, QVector<QStringList>* rows) const;
    void export_csv();
    /// Header columns menu entries (shared with "Columns for all panels").
    void set_columns(const QStringList& ids);

    /// Show / focus the inline add row (the shared symbol box, multi entry).
    void open_add_row();
    void close_add_row();
    /// Resolver findings for saved symbols: notes ("TATAMOTORS is now TMPV") shown on the
    /// row with a ">" marker, and successors offered as "Replace with ..." in the row menu.
    void set_resolver_findings(const QHash<QString, QString>& notes, const QHash<QString, QString>& successors);

  signals:
    void edit_requested(const QString& panel_id);
    void delete_requested(const QString& panel_id);
    void config_changed(const fincept::screens::MarketPanelConfig& cfg);
    void symbol_selected(const QString& panel_id, const QString& symbol);
    void symbol_activated(const QString& symbol);
    void maximise_requested(const QString& panel_id);
    void move_requested(const QString& panel_id, Qt::ArrowType direction);
    void export_requested(const QString& panel_id);
    void detail_requested(const QString& symbol);
    void detail_toggle_requested(const QString& symbol);
    void remove_symbol_requested(const QString& panel_id, const QString& symbol);
    void add_symbol_requested(const QString& panel_id, const QString& symbol);
    void focus_panel_requested(const QString& panel_id, int delta);
    void escape_pressed();
    void viewport_rows_changed();
    void replace_symbol_requested(const QString& panel_id, const QString& from, const QString& to);
    /// Text the add row could not resolve (status-bar notice).
    void unresolved_entry(const QString& message);
    /// Drag-to-reorder by the header (left button held past the start-drag distance;
    /// never while maximised): the pointer moved to `global_pos` …
    void header_drag_moved(const QString& panel_id, const QPoint& global_pos);
    /// … and the drag ended: released at `global_pos` (`drop` true) or cancelled (Esc,
    /// another button, the button lost; `drop` false). The screen picks the slot.
    void header_drag_finished(const QString& panel_id, const QPoint& global_pos, bool drop);

  protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void update_badge();
    void update_empty_state();
    void show_panel_menu(const QPoint& global_pos);
    void show_row_menu(const QString& symbol, const QPoint& global_pos);
    void capture_and_emit();
    /// Header mouse / key events of a drag-to-reorder; true when consumed.
    bool handle_header_drag(QEvent* event);
    /// End a header drag in progress (cursor, keyboard grab) and report it.
    void end_header_drag(const QPoint& global_pos, bool drop);

    MarketPanelConfig config_;
    QPointer<MarketsQuoteStore> store_;
    MarketPanelModel* model_ = nullptr;
    MarketPanelProxy* proxy_ = nullptr;
    MarketPanelView* view_ = nullptr;

    QWidget* header_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* badge_ = nullptr;
    QToolButton* add_btn_ = nullptr;
    QToolButton* menu_btn_ = nullptr;
    QWidget* add_row_ = nullptr;
    QWidget* add_entry_ = nullptr;
    QStackedWidget* body_ = nullptr;
    QLabel* empty_label_ = nullptr;
    QHash<QString, QString> successors_;
    QString badge_text_;
    QString badge_tip_;
    bool badge_warn_ = false;
    bool maximised_ = false;
    // Header drag-to-reorder: armed by a left press on the header, dragging once the
    // pointer moved past QApplication::startDragDistance().
    QPoint drag_press_global_;
    bool drag_armed_ = false;
    bool dragging_ = false;
    QObject* drag_key_filter_ = nullptr; ///< app-wide while dragging: Esc cancels
};

} // namespace fincept::screens
