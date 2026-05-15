// src/screens/code_editor/CodeEditorScreen.h
// Python Colab — Jupyter notebook with Obsidian design system.
// Responsive cells, markdown rendering, keyboard shortcuts, collapsible output.
#pragma once

#include "screens/common/IStatefulScreen.h"

#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>
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

  private:
    void build_ui();
    void update_gutter();
    void adjust_editor_height();
    void render_markdown();

    QString cell_id_;
    QString cell_type_;
    int execution_count_ = 0;
    int index_ = 0;
    bool running_ = false;
    bool selected_ = false;
    bool hovered_ = false;
    bool output_collapsed_ = false;
    bool md_editing_ = false;
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

  private:
    QListWidget* list_ = nullptr;
};

// -- Main screen --------------------------------------------------------------

class CodeEditorScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit CodeEditorScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "code_editor"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

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

  private:
    void build_ui();
    QWidget* build_toolbar();
    QWidget* build_status_bar();
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
};

} // namespace fincept::screens
