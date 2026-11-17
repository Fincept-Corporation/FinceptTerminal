#pragma once
// CodeEditorScreen_Cells — one notebook cell on screen:
//
//   CodeTextEdit  the source editor: line numbers, Python highlighting (token
//                 colours from the theme), bracket matching, auto-indent, Tab /
//                 Shift+Tab indent, Ctrl+/ comment, completion popup, and the
//                 run / navigation keys reported as signals.
//   CellWidget    gutter (execution count + run state with elapsed time) |
//                 editor or rendered markdown | outputs (NotebookOutputs.h), a
//                 small tool bar over the top-right corner, folded input /
//                 output placeholders, selection (command / edit mode) through
//                 the "sel" property of the screen's stylesheet.
//
// Neither class owns notebook data beyond what is on screen: the screen keeps
// the NotebookDoc and pulls source() when it needs it.

#include "screens/code_editor/NotebookDocument.h"

#include <QFrame>
#include <QPlainTextEdit>
#include <QPointer>
#include <QStringList>
#include <QTextEdit>
#include <QVector>

class QCompleter;
class QLabel;
class QPushButton;
class QStringListModel;
class QTextBrowser;
class QToolButton;
class QVBoxLayout;

namespace fincept::screens {

class NbHighlighter;
class NbLineNumberArea;

class CodeTextEdit : public QPlainTextEdit {
    Q_OBJECT
  public:
    enum class RunKey { Run, RunAdvance, RunInsert };

    explicit CodeTextEdit(QWidget* parent = nullptr);

    void set_markdown_mode(bool markdown);
    void set_line_numbers_visible(bool on);
    bool line_numbers_visible() const { return line_numbers_; }
    /// Fit the height to every line (the notebook scrolls, not the cell).
    void fit_height();
    /// Re-read the theme: highlighter formats, gutter colours.
    void refresh_theme();

    /// Line (1-based) to wash as the line the cell failed on; 0 clears it.
    void set_error_line(int line);
    /// Find-in-notebook highlights (cleared with an empty list).
    void set_search_ranges(const QVector<QPair<int, int>>& ranges);
    void go_to_line(int line);

    /// Dotted name under / before the cursor ("pd.DataFrame.merge"), for help.
    QString symbol_at_cursor() const;
    /// Text before the cursor that a completion would extend ("df.ro"), or empty.
    QString completion_prefix() const;
    /// Show `matches` for `prefix` (no-op when the cursor moved on). One match is inserted directly.
    void show_completions(const QString& prefix, const QStringList& matches);
    bool completion_visible() const;

    int line_number_area_width() const;
    void paint_line_numbers(QPaintEvent* event);

  signals:
    void run_key(fincept::screens::CodeTextEdit::RunKey key);
    void escape_pressed();
    /// Up on the first line / Down on the last line: move to the neighbouring cell.
    void leave_edge(int delta);
    void completion_requested(const QString& prefix);
    void inspect_requested(const QString& symbol);
    void focused();

  protected:
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void update_gutter_width();
    void update_gutter_area(const QRect& rect, int dy);
    void update_extra_selections();
    void indent_selection(bool dedent);
    void toggle_comment();
    void insert_newline_with_indent();
    bool smart_backspace();
    void insert_completion(const QString& completion);

    NbLineNumberArea* line_area_ = nullptr;
    NbHighlighter* highlighter_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel* completion_model_ = nullptr;
    QString completion_prefix_;
    bool line_numbers_ = true;
    bool markdown_ = false;
    int error_line_ = 0;
    QVector<QPair<int, int>> search_ranges_;
};

class CellWidget : public QFrame {
    Q_OBJECT
  public:
    enum class Selection { None, Command, Edit };
    enum class RunState { Idle, Queued, Running, Done, Error, Cancelled };
    enum class Action { Run, MoveUp, MoveDown, ToggleType, Delete, Menu, ToggleInput, ToggleOutput };

    explicit CellWidget(const notebook::NotebookCell& cell, QWidget* parent = nullptr);

    QString cell_id() const { return id_; }
    notebook::NotebookCell::Type type() const { return type_; }
    QString source() const;
    void set_source(const QString& source);
    void set_type(notebook::NotebookCell::Type type);
    void set_title(const QString& title);
    CodeTextEdit* editor() const { return editor_; }

    void set_selection(Selection s);
    Selection selection() const { return selection_; }
    void focus_editor(bool to_end = false);

    void set_exec_count(int count);
    void set_run_state(RunState state, qint64 elapsed_ms = -1);
    RunState run_state() const { return run_state_; }
    /// While running: the elapsed time in the gutter.
    void tick(qint64 elapsed_ms);

    /// Replace every output (open, clear, undo).
    void set_outputs(const QVector<notebook::CellOutput>& outputs);
    /// Output `index` changed or was appended (live run).
    void apply_output(int index, const notebook::CellOutput& output);
    /// Exec count shown on the output header.
    void set_output_collapsed(bool collapsed);
    bool output_collapsed() const { return output_collapsed_; }
    void set_input_collapsed(bool collapsed);
    bool input_collapsed() const { return input_collapsed_; }

    /// Markdown cells: rendered view (false) or source editor (true).
    void set_markdown_editing(bool editing);
    bool markdown_editing() const { return md_editing_; }
    void render_markdown();

    void set_line_numbers_visible(bool on);
    /// Theme / font change: highlighter, markdown HTML, output colours, gutter width.
    void refresh_theme();
    void refit();

  signals:
    /// The user clicked into the cell; `edit` when into its editor.
    void activated(const QString& id, bool edit);
    void action(const QString& id, fincept::screens::CellWidget::Action action);
    void run_key(const QString& id, fincept::screens::CodeTextEdit::RunKey key);
    void escape_pressed(const QString& id);
    void leave_edge(const QString& id, int delta);
    void edited(const QString& id);
    void completion_requested(const QString& id, const QString& prefix);
    void inspect_requested(const QString& id, const QString& symbol);
    void cursor_moved(const QString& id, int line, int column);
    /// An output's EXPAND: show it large in the inspector.
    void output_expand_requested(const QString& id, int output_index);
    void menu_requested(const QString& id, const QPoint& global_pos);

  protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* e) override;

  private:
    void build();
    void retranslate();
    void update_gutter();
    void update_folds();
    void update_bar_visibility();
    void place_bar();
    void apply_metrics();
    QWidget* make_output(const notebook::CellOutput& out, int index);

    QString id_;
    notebook::NotebookCell::Type type_ = notebook::NotebookCell::Type::Code;
    QString title_;
    Selection selection_ = Selection::None;
    RunState run_state_ = RunState::Idle;
    qint64 last_elapsed_ms_ = -1;
    int exec_count_ = 0;
    bool hovered_ = false;
    bool output_collapsed_ = false;
    bool input_collapsed_ = false;
    bool md_editing_ = false;
    int output_count_ = 0;

    QWidget* gutter_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* state_ = nullptr;
    QWidget* body_ = nullptr;
    QVBoxLayout* body_layout_ = nullptr;
    CodeTextEdit* editor_ = nullptr;
    QTextBrowser* markdown_ = nullptr;
    QPushButton* input_fold_ = nullptr;
    QWidget* outputs_ = nullptr;
    QVBoxLayout* outputs_layout_ = nullptr;
    QVector<QWidget*> output_widgets_;
    QVector<notebook::CellOutput::Kind> output_kinds_;
    QPushButton* output_fold_ = nullptr;
    QFrame* bar_ = nullptr;
    QToolButton* bar_run_ = nullptr;
    QToolButton* bar_up_ = nullptr;
    QToolButton* bar_down_ = nullptr;
    QToolButton* bar_type_ = nullptr;
    QToolButton* bar_delete_ = nullptr;
    QToolButton* bar_more_ = nullptr;
};

} // namespace fincept::screens
