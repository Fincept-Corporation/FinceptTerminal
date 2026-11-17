// src/screens/code_editor/CodeEditorScreen.h
// The Code tab — a research-notebook workstation in the terminal's house style:
//
//   COMMAND BAR   notebook name, file / run / kernel commands, find, kernel badge
//   RAIL          LIBRARY | OUTLINE | VARIABLES (collapsible, width remembered)
//   EDITOR        the cells (or, with no notebook open, the start view)
//   INSPECTOR     HELP | OUTPUT | HISTORY (optional, built on first use)
//   STATUS BAR    kernel · environment · last run · cells · cursor · saved · path
//
// Cells run in one persistent kernel per notebook (services::NotebookKernel): the
// variables of one cell are there for the next, as in Jupyter. Opening a notebook
// shows its saved outputs and never runs anything; running is always a user action.
//
// Pieces: NotebookDocument (model, .ipynb, exports), CodeEditorScreen_Cells (cell
// and editor widgets), NotebookOutputs (tables / figures / errors), NotebookStyle
// (the one stylesheet, icons, palettes), NotebookCommandBar, the shared
// ui::ScreenStatusBar (bottom bar), CodeEditorScreen_Library (rail library + start view), CodeEditorScreen_Navigator
// (outline + variables), NotebookInspector.
#pragma once

#include "screens/code_editor/CodeEditorScreen_Cells.h"
#include "screens/code_editor/CodeEditorScreen_Library.h"
#include "screens/code_editor/NotebookCommandBar.h"
#include "screens/code_editor/NotebookDocument.h"
#include "screens/code_editor/NotebookInspector.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

class QKeyEvent;
class QPushButton;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace fincept::services {
class NotebookKernel;
}

namespace fincept::ui {
class ScreenStatusBar;
}

namespace fincept::screens {

class NotebookOutlinePanel;
class NotebookVariablesPanel;

class CodeEditorScreen : public QWidget, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
  public:
    explicit CodeEditorScreen(QWidget* parent = nullptr);
    ~CodeEditorScreen() override;

    /// Open a notebook (.ipynb) — the File Manager's and the Library's entry
    /// point. Asks about unsaved changes first. True if the file was loaded.
    bool open_notebook_path(const QString& path);

    /// ISectionLinked — "new" (NEW button) | "template:<catalog id>" (the template's OPEN) |
    /// "notebook:<absolute .ipynb path or File Manager id>" (opened, nothing runs) |
    /// "library" | "outline" | "variables" (rail tabs). Opening asks about unsaved changes first.
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "code_editor"; }
    int state_version() const override { return 1; }
    void flush_pending_state() override;

  protected:
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    enum class Mode { Command, Edit };
    enum class AfterRun { Stay, Advance, Insert };

    // ── Building ─────────────────────────────────────────────────────────────
    void build_ui();
    QWidget* build_rail();
    void show_rail_tab(int tab);
    void ensure_inspector();
    void set_rail_visible(bool on);
    void set_inspector_visible(bool on);
    void apply_splitter_sizes();
    void bind_shortcuts();

    // ── Notebook lifecycle ───────────────────────────────────────────────────
    bool load_path(const QString& path, QString* error);
    void set_document(const notebook::NotebookDoc& doc, const QString& path);
    void new_notebook();
    void close_notebook();
    void open_dialog();
    void open_template(int catalog_index);
    void recover_unsaved();
    bool save();
    bool save_as();
    bool write_to(const QString& path, bool quiet);
    void export_python();
    void export_html();
    bool confirm_discard(const QString& action);
    QString default_save_dir() const;
    QString suggested_base_name() const;
    QString working_dir() const;

    // ── Cells ────────────────────────────────────────────────────────────────
    CellWidget* create_cell_widget(const notebook::NotebookCell& cell);
    void build_cell_widgets();
    void build_more_cells(quint64 generation);
    void ensure_all_built();
    void clear_cell_widgets();
    int index_of(const QString& id) const;
    CellWidget* widget_at(int index) const;
    void sync_sources();
    void mark_changed();
    void update_dirty();
    void select_cell(int index, Mode mode, bool scroll = true, bool take_focus = true);
    void scroll_to(CellWidget* w);
    void insert_cell(int index, notebook::NotebookCell::Type type, const QString& source, Mode mode);
    void delete_cell(int index);
    void undo_delete();
    void move_cell(int index, int delta);
    void set_cell_type(int index, notebook::NotebookCell::Type type);
    void copy_cell(int index, bool cut);
    void paste_cells(int index);
    void rename_cell(int index);
    void clear_outputs(int index); ///< -1 = every cell
    void toggle_output(int index);
    void toggle_input(int index);
    void show_cell_menu(const QString& id, const QPoint& global_pos);
    void on_cell_action(const QString& id, CellWidget::Action action);
    void on_cell_activated(const QString& id, bool edit);
    void on_run_key(const QString& id, CodeTextEdit::RunKey key);
    bool handle_command_key(QKeyEvent* e);
    void set_line_numbers(bool on);
    void insert_snippet(NotebookCommandBar::Cmd cmd);
    QString ask_symbol();

    // ── Running ──────────────────────────────────────────────────────────────
    void run_cells(const QStringList& ids);
    void run_selected(AfterRun after);
    void run_all();
    void run_above();
    void run_below();
    void start_next();
    void interrupt();
    void restart_kernel(bool confirm, bool run_all_after);
    void abort_queue(const QString& why);
    void on_exec_stream(const QString& token, const QString& text, bool is_stderr);
    void on_exec_event(const QString& token, const QJsonObject& event);
    void on_exec_finished(const QString& token, const QString& status, qint64 elapsed_ms, bool truncated,
                          bool kernel_lost);
    void on_kernel_reply(const QJsonObject& event);
    void set_environment(const QString& env);

    // ── Panels, bars, find, help ─────────────────────────────────────────────
    void on_command(NotebookCommandBar::Cmd cmd);
    void refresh_kernel_badge();
    void refresh_status();
    void refresh_title();
    void refresh_outline();
    void refresh_save_state();
    void refresh_start_view();
    void request_help(const QString& symbol);
    void on_completion_requested(const QString& id, const QString& prefix);
    void run_find(bool restart, bool backwards);
    void clear_find();
    void add_recent(const QString& path);
    void autosave_tick();
    QString recovery_path() const;
    bool write_recovery();
    void on_theme_refresh();
    void on_appearance_settled();
    void retranslate();
    void persist();

    // ── Data ─────────────────────────────────────────────────────────────────
    notebook::NotebookDoc doc_;
    QString path_;
    QString clean_fp_;
    bool open_ = false;
    bool dirty_ = false;
    bool outputs_dirty_ = false; ///< outputs changed since the last save (not a prompt, but autosaved)
    QVector<CellWidget*> cells_; ///< widgets of doc_.cells[0 .. cells_.size()) (built in batches)
    quint64 build_gen_ = 0;
    int selected_ = -1;
    Mode mode_ = Mode::Command;

    services::NotebookKernel* kernel_ = nullptr;
    QStringList queue_;
    QString running_id_;
    QString running_token_;
    QVector<notebook::CellOutput> running_outputs_;
    quint64 run_seq_ = 0;
    int exec_counter_ = 0;
    QString last_run_text_;
    QString last_run_tone_;
    QVector<NotebookRunRecord> runs_;
    QJsonArray variables_;
    int variables_total_ = 0;

    struct Deleted {
        notebook::NotebookCell cell;
        int index = 0;
    };
    QVector<Deleted> deleted_;
    QVector<notebook::NotebookCell> clipboard_;
    int pending_key_ = 0;
    qint64 pending_key_at_ = 0;
    quint64 req_seq_ = 0;
    QHash<QString, QPair<QString, QString>> completion_requests_; ///< request id -> (cell id, prefix)
    QString help_request_;
    QString last_help_symbol_;
    QString preview_request_;

    QString find_text_;
    QVector<QPair<int, int>> find_hits_; ///< (cell index, position)
    int find_pos_ = -1;

    bool rail_visible_ = true;
    int rail_tab_ = 0;
    bool inspector_visible_ = false;
    int inspector_tab_ = 0;
    int rail_width_ = 0;
    int inspector_width_ = 0;
    bool autosave_ = true;
    bool line_numbers_ = true;
    int timeout_s_ = 1800;
    QVector<RecentNotebook> recent_;
    QDateTime saved_at_;
    QDateTime autosaved_at_;
    QString save_error_;
    bool theme_refresh_pending_ = false;

    // ── Widgets ──────────────────────────────────────────────────────────────
    NotebookCommandBar* cmd_ = nullptr;
    fincept::ui::ScreenStatusBar* status_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QWidget* rail_ = nullptr;
    QPushButton* rail_tabs_[3] = {nullptr, nullptr, nullptr};
    QStackedWidget* rail_stack_ = nullptr;
    QWidget* rail_pages_[3] = {nullptr, nullptr, nullptr};
    NotebookLibraryPanel* library_ = nullptr;
    NotebookOutlinePanel* outline_ = nullptr;
    NotebookVariablesPanel* variables_panel_ = nullptr;
    QStackedWidget* center_ = nullptr;
    NotebookStartView* start_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* canvas_ = nullptr;
    QVBoxLayout* cells_layout_ = nullptr;
    QWidget* footer_ = nullptr;
    QPushButton* add_code_ = nullptr;
    QPushButton* add_text_ = nullptr;
    NotebookInspector* inspector_ = nullptr;
    QTimer* tick_timer_ = nullptr;
    QTimer* autosave_timer_ = nullptr;
    QTimer* dirty_timer_ = nullptr;
    QTimer* help_timer_ = nullptr;
    QTimer* find_timer_ = nullptr;
};

} // namespace fincept::screens
