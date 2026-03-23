// src/screens/code_editor/CodeEditorScreen.h
// Python Colab notebook — Jupyter-style cell-based code execution.
// Supports code/markdown cells, run individual/all, stdout/stderr output,
// .ipynb file I/O, and package management via the bundled Python interpreter.
#pragma once

#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// ── Cell data ────────────────────────────────────────────────────────────────

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
    QString cell_type;  // "code" or "markdown"
    QString source;
    QVector<CellOutput> outputs;
    int execution_count = 0;
    bool running = false;
};

// ── Cell widget ──────────────────────────────────────────────────────────────

class CellWidget : public QWidget {
    Q_OBJECT
  public:
    explicit CellWidget(const NotebookCell& cell, QWidget* parent = nullptr);

    void set_cell_data(const NotebookCell& cell);
    NotebookCell cell_data() const;
    void set_running(bool running);
    void set_outputs(const QVector<CellOutput>& outputs, int exec_count);
    void set_selected(bool selected);
    QString cell_id() const { return cell_id_; }

  signals:
    void run_requested(const QString& cell_id);
    void delete_requested(const QString& cell_id);
    void move_up_requested(const QString& cell_id);
    void move_down_requested(const QString& cell_id);
    void toggle_type_requested(const QString& cell_id);
    void cell_selected(const QString& cell_id);

  private:
    void build_ui();
    void update_header();
    void render_outputs();

    QString cell_id_;
    QString cell_type_;
    int execution_count_ = 0;
    bool running_ = false;

    QLabel* header_label_ = nullptr;
    QLabel* type_badge_ = nullptr;
    QTextEdit* editor_ = nullptr;
    QWidget* output_area_ = nullptr;
    QVBoxLayout* output_layout_ = nullptr;
    QWidget* action_bar_ = nullptr;
};

// ── Main screen ──────────────────────────────────────────────────────────────

class CodeEditorScreen : public QWidget {
    Q_OBJECT
  public:
    explicit CodeEditorScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_add_cell();
    void on_run_cell(const QString& cell_id);
    void on_run_all();
    void on_delete_cell(const QString& cell_id);
    void on_move_cell_up(const QString& cell_id);
    void on_move_cell_down(const QString& cell_id);
    void on_toggle_cell_type(const QString& cell_id);
    void on_new_notebook();
    void on_open_notebook();
    void on_save_notebook();

  private:
    void build_ui();
    QWidget* build_toolbar();
    void rebuild_cells();
    void add_cell_after(const QString& after_id, const QString& type);
    int find_cell_index(const QString& cell_id) const;
    CellWidget* find_cell_widget(const QString& cell_id) const;
    QString new_cell_id() const;
    void update_status();

    // Notebook data
    QVector<NotebookCell> cells_;
    int execution_counter_ = 0;
    QString notebook_path_;

    // UI
    QScrollArea* scroll_area_ = nullptr;
    QWidget* cells_container_ = nullptr;
    QVBoxLayout* cells_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QString selected_cell_id_;
};

} // namespace fincept::screens
