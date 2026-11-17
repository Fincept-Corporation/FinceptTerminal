#pragma once
// NotebookCommandBar — the Code tab's one-row command strip (Watchlist / Markets
// top-bar look: BG_SURFACE, 1 px BORDER_DIM rule):
//
//   NAME ● | NEW OPEN SAVE▾ | ▶ RUN  ▶▶ RUN ALL  ■ STOP  ↻ RESTART | + INSERT▾ ...... [find] [● PY 3.11 · NUMPY2 · IDLE ▾] [rail] [inspector] [⋯]
//
// Icon + label buttons with tooltips naming their keys; secondary labels fold
// to icons below 1360 px and the run cluster's below 1080 px (resizeEvent only).
// Holds no notebook data; everything it does is a command() / signal.

#include <QWidget>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QToolButton;

namespace fincept::screens {

class NotebookCommandBar : public QWidget {
    Q_OBJECT
  public:
    enum class Cmd {
        New, Open, Save, SaveAs, ExportPy, ExportHtml, Close,
        RunCell, RunAll, RunAbove, RunBelow, Interrupt, Restart, RestartRunAll, Shutdown,
        InsertCode, InsertText, InsertPrices, InsertPlot, InsertReturns, InsertConnector, InsertCsv,
        ToggleRail, ToggleInspector, ClearOutputs, ToggleLineNumbers, ToggleAutosave, Shortcuts,
    };
    Q_ENUM(Cmd)

    explicit NotebookCommandBar(QWidget* parent = nullptr);

    /// `open` false = start page (notebook commands disabled).
    void set_notebook(const QString& name, const QString& tooltip, bool dirty, bool open);
    /// Kernel badge: text, tone ("ok" | "warn" | "danger" | "dim"), tooltip.
    void set_kernel(const QString& text, const QString& short_text, const QString& tone, const QString& tooltip);
    void set_busy(bool busy);
    void set_environment(const QString& env);
    void set_timeout(int seconds);
    void set_rail_checked(bool on);
    void set_inspector_checked(bool on);
    void set_line_numbers_checked(bool on) { line_numbers_ = on; }
    void set_autosave_checked(bool on) { autosave_ = on; }
    void set_find_status(const QString& text);
    QLineEdit* find_box() const { return find_; }
    void retranslate();
    QSize minimumSizeHint() const override;

  signals:
    void command(fincept::screens::NotebookCommandBar::Cmd cmd);
    void environment_selected(const QString& env);
    void timeout_selected(int seconds);
    void find_changed(const QString& text);
    void find_next(bool backwards);
    void find_closed();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    QToolButton* make(const char* object_name, int glyph, int tone, Cmd cmd);
    void show_save_menu();
    void show_insert_menu();
    void show_kernel_menu();
    void show_more_menu();
    void apply_collapse();

    QHBoxLayout* row_ = nullptr;
    QLabel* name_ = nullptr;
    QLabel* dirty_ = nullptr;
    QToolButton* new_ = nullptr;
    QToolButton* open_btn_ = nullptr;
    QToolButton* save_ = nullptr;
    QToolButton* run_ = nullptr;
    QToolButton* run_all_ = nullptr;
    QToolButton* stop_ = nullptr;
    QToolButton* restart_ = nullptr;
    QToolButton* insert_ = nullptr;
    QLineEdit* find_ = nullptr;
    QLabel* find_status_ = nullptr;
    QToolButton* kernel_ = nullptr;
    QToolButton* rail_ = nullptr;
    QToolButton* inspector_ = nullptr;
    QToolButton* more_ = nullptr;
    QString kernel_text_;
    QString kernel_short_;
    QString env_ = QStringLiteral("numpy2");
    int timeout_s_ = 1800;
    bool is_open_ = false;
    bool busy_ = false;
    bool line_numbers_ = true;
    bool autosave_ = true;
    int collapse_level_ = -1;
};

} // namespace fincept::screens
