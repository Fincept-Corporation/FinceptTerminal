#pragma once
// CodeEditorScreen_Navigator — the rail's OUTLINE and VARIABLES pages.
//
//   NotebookOutlinePanel    markdown headings as a tree with the code cells under
//                           them, each with its run state as a glyph (✓ ✕ ▶ … ·)
//                           and its execution count; click jumps to the cell,
//                           F2 / context menu renames it.
//   NotebookVariablesPanel  the live kernel's variables (name, type, shape,
//                           one-line value), filterable; double-click shows the
//                           value (a DataFrame as a table) in the inspector.
//                           Empty states say why (kernel off / no variables).

#include <QJsonArray>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class NotebookOutlinePanel : public QWidget {
    Q_OBJECT
  public:
    struct Entry {
        QString cell_id;
        int level = 0;        ///< 1-6 heading, 0 code cell
        QString text;
        int exec_count = 0;
        int run_state = 0;    ///< CellWidget::RunState as int
    };

    explicit NotebookOutlinePanel(QWidget* parent = nullptr);
    void set_entries(const QVector<Entry>& entries, const QString& selected_id);
    void set_selected(const QString& cell_id);

  signals:
    void cell_activated(const QString& cell_id);
    void rename_requested(const QString& cell_id);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void rebuild();
    QTreeWidget* tree_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    QVector<Entry> entries_;
    QString selected_;
    bool updating_ = false;
};

class NotebookVariablesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NotebookVariablesPanel(QWidget* parent = nullptr);
    void set_variables(const QJsonArray& items, int total);
    /// `alive` false: the kernel is not running (`why` says why, e.g. not started yet).
    void set_kernel_alive(bool alive, const QString& why = QString());

  signals:
    void preview_requested(const QString& name);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void apply_filter();
    void update_state();
    void retranslate();

    QLineEdit* filter_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* footer_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* empty_ = nullptr;
    QJsonArray items_;
    int total_ = 0;
    bool alive_ = false;
    QString why_;
};

} // namespace fincept::screens
