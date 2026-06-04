// src/screens/code_editor/CodeEditorScreen.h
// Fincept Notebook — two-view workspace (Library + Editor) on the Obsidian
// design system. Library lists prebuilt finance notebooks as cards; the editor
// runs responsive notebook cells with markdown rendering and collapsible output.
#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QEvent>
#include <QGridLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

// -- Cell data ----------------------------------------------------------------

struct CellOutput {
    QString type;       // "stream", "error", "execute_result"
    QString text;       // stdout/stderr/result text
    QString name;       // "stdout" or "stderr"
    QString error_name; // for errors
    QString error_value;
    QStringList traceback;
};

struct NotebookCell {
    QString id;
    QString cell_type; // "code" or "markdown"
    QString title;     // optional custom name for sidebar navigator
    QString source;
    QVector<CellOutput> outputs;
    int execution_count = 0;
    bool running = false;
};

// -- Code editor with line numbers --------------------------------------------

class CodeTextEdit : public QTextEdit {
    Q_OBJECT
  public:
    explicit CodeTextEdit(QWidget* parent = nullptr);

  signals:
    void run_shortcut(); // Ctrl+Enter
    void run_and_next(); // Shift+Enter

  protected:
    void keyPressEvent(QKeyEvent* event) override;
};

// -- Cell widget --------------------------------------------------------------

class CellWidget : public QWidget {
    Q_OBJECT
  public:
    explicit CellWidget(const NotebookCell& cell, int index, QWidget* parent = nullptr);

    void set_cell_data(const NotebookCell& cell);
    NotebookCell cell_data() const;
    void set_running(bool running);
    void set_outputs(const QVector<CellOutput>& outputs, int exec_count);
    void set_selected(bool selected);
    void set_index(int index);
    QString cell_id() const { return cell_id_; }

  signals:
    void run_requested(const QString& cell_id);
    void run_and_advance(const QString& cell_id);
    void delete_requested(const QString& cell_id);
    void move_up_requested(const QString& cell_id);
    void move_down_requested(const QString& cell_id);
    void toggle_type_requested(const QString& cell_id);
    void cell_clicked(const QString& cell_id);
    void insert_below_requested(const QString& cell_id);

  protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    void build_ui();
    void update_gutter();
    void adjust_editor_height();
    void render_markdown();
    void retranslateUi();

    QString cell_id_;
    QString cell_type_;
    int execution_count_ = 0;
    int index_ = 0;
    bool running_ = false;
    bool selected_ = false;
    bool hovered_ = false;
    bool output_collapsed_ = false;
    bool md_editing_ = false;
    bool adjusting_height_ = false; // guards adjust_editor_height re-entrancy
    QString title_;

    // Gutter
    QWidget* gutter_ = nullptr;
    QLabel* gutter_number_ = nullptr;
    QLabel* gutter_type_ = nullptr;

    // Editor area
    QWidget* editor_container_ = nullptr;
    CodeTextEdit* editor_ = nullptr;
    QTextBrowser* md_preview_ = nullptr; // rendered markdown view

    // Hover toolbar
    QWidget* toolbar_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QPushButton* type_btn_ = nullptr;
    QPushButton* up_btn_ = nullptr;
    QPushButton* dn_btn_ = nullptr;
    QPushButton* del_btn_ = nullptr;

    // Output area
    QWidget* output_area_ = nullptr;
    QVBoxLayout* output_layout_ = nullptr;
    QPushButton* output_toggle_ = nullptr;
    QWidget* output_content_ = nullptr;
    QVBoxLayout* output_content_layout_ = nullptr;

    // Insert hint
    QWidget* insert_hint_ = nullptr;
};

// -- Sidebar cell navigator ---------------------------------------------------

class CellNavigator : public QWidget {
    Q_OBJECT
  public:
    explicit CellNavigator(QWidget* parent = nullptr);
    void rebuild(const QVector<NotebookCell>& cells, const QString& selected_id);

  signals:
    void cell_selected(const QString& cell_id);
    void rename_requested(const QString& cell_id);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QListWidget* list_ = nullptr;
    QLabel* header_title_ = nullptr;
    // Cached so rebuild() can re-render after a language change.
    QVector<NotebookCell> last_cells_;
    QString last_selected_id_;
};

// -- Main screen --------------------------------------------------------------

class CodeEditorScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit CodeEditorScreen(QWidget* parent = nullptr);

    /// Load a notebook (.ipynb) from disk into the editor and switch to the
    /// editor view. Public entry point used by the File Manager and the
    /// Library "OPEN" action. Returns true if the file was loaded.
    bool open_notebook_path(const QString& path);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "code_editor"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private slots:
    void on_add_cell();
    void on_run_cell(const QString& cell_id);
    void on_run_and_advance(const QString& cell_id);
    void on_run_all();
    void on_delete_cell(const QString& cell_id);
    void on_move_cell_up(const QString& cell_id);
    void on_move_cell_down(const QString& cell_id);
    void on_toggle_cell_type(const QString& cell_id);
    void on_new_notebook();
    void on_open_notebook();
    void on_save_notebook();
    void on_select_cell(const QString& cell_id);
    void on_insert_below(const QString& cell_id);
    void on_clear_outputs();
    void on_toggle_sidebar();
    void on_rename_cell(const QString& cell_id);

    // Header / Library
    void on_view_changed(int index);          // 0 = Library, 1 = Editor
    void on_library_search(const QString& text);
    void on_open_library_entry(int catalog_index);

  private:
    void build_ui();
    QWidget* build_header();
    QWidget* build_editor_page();
    QWidget* build_library_page();            // CodeEditorScreen_Library.cpp
    void populate_library();                  // CodeEditorScreen_Library.cpp
    void relayout_library_cards();            // CodeEditorScreen_Library.cpp
    void set_view(int index);
    bool load_notebook_from_path(const QString& path);
    QWidget* build_toolbar();
    QWidget* build_status_bar();
    void retranslateUi();
    void rebuild_cells();
    void add_cell_after(const QString& after_id, const QString& type);
    int find_cell_index(const QString& cell_id) const;
    CellWidget* find_cell_widget(const QString& cell_id) const;
    QString new_cell_id() const;
    void update_status();
    void update_navigator();
    void advance_to_next(const QString& current_id);

    // Notebook data
    QVector<NotebookCell> cells_;
    int execution_counter_ = 0;
    QString notebook_path_;

    // UI
    QSplitter* splitter_ = nullptr;
    CellNavigator* navigator_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    QWidget* cells_container_ = nullptr;
    QVBoxLayout* cells_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* kernel_label_ = nullptr;
    QString selected_cell_id_;
    bool sidebar_visible_ = true;

    // Toolbar / status-bar text widgets (cached for retranslateUi)
    QPushButton* btn_new_ = nullptr;
    QPushButton* btn_open_ = nullptr;
    QPushButton* btn_save_ = nullptr;
    QPushButton* btn_add_cell_ = nullptr;
    QPushButton* btn_clear_out_ = nullptr;
    QPushButton* btn_run_all_ = nullptr;
    QPushButton* btn_sidebar_ = nullptr;
    QLabel* py_label_ = nullptr;
    QLabel* shortcuts_label_ = nullptr;

    // Tracks whether the kernel is busy so retranslateUi can re-render the badge.
    bool kernel_busy_ = false;
    void refresh_kernel_label();

    // ── Header + two-view stack ──────────────────────────────────────────────
    QStackedWidget* view_stack_ = nullptr;
    int active_view_ = 0; // 0 = Library, 1 = Editor
    QLabel* header_title_ = nullptr;
    QVector<QPushButton*> view_btns_;          // [LIBRARY, EDITOR]
    QLineEdit* lib_search_input_ = nullptr;
    QPushButton* header_new_btn_ = nullptr;

    // ── Library view ─────────────────────────────────────────────────────────
    QWidget* library_page_ = nullptr;
    QLabel* lib_toolbar_lbl_ = nullptr;
    QLabel* lib_count_lbl_ = nullptr;
    QScrollArea* lib_scroll_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QGridLayout* cards_layout_ = nullptr;
    QVector<QPushButton*> cat_chips_;
    QVector<QPushButton*> diff_chips_;
    QString lib_category_filter_ = "All";
    QString lib_difficulty_filter_ = "All";
    QString lib_search_text_;
    int lib_columns_ = 0; // last computed column count (re-flow guard)
};

} // namespace fincept::screens
