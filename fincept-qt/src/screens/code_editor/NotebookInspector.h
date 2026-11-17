#pragma once
// NotebookInspector — the Code tab's optional right pane (built on first use):
//
//   HELP     the signature and docstring of the name under the cursor (Ctrl+I,
//            or automatically when the cursor rests on a name while the kernel is
//            idle), from the live kernel; before that, the keyboard reference.
//   OUTPUT   one output large: a table with every row the cell returned, a figure
//            with FIT / 100%, long text; or a variable's value (double-click in
//            VARIABLES).
//   HISTORY  every run of this session: time, cell, duration, outcome; click to
//            jump to the cell.

#include "screens/code_editor/NotebookDocument.h"

#include <QDateTime>
#include <QJsonObject>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTreeWidget;
class QVBoxLayout;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class NbOutputView;

struct NotebookRunRecord {
    QString cell_id;
    QString label;
    QDateTime when;
    qint64 ms = 0;
    QString status; ///< ok | error | cancelled | timeout
    int exec_count = 0;
};

class NotebookInspector : public QWidget {
    Q_OBJECT
  public:
    enum class Tab { Help = 0, Output = 1, History = 2 };

    explicit NotebookInspector(QWidget* parent = nullptr);

    void show_tab(Tab tab);
    Tab current_tab() const { return tab_; }

    void show_shortcuts();
    void set_help_pending(const QString& symbol);
    void set_help(const QJsonObject& inspect_reply);
    void set_help_unavailable(const QString& symbol, const QString& why);
    void show_output(const notebook::CellOutput& out, const QString& caption);
    /// A kernel "preview" reply (a variable's value).
    void show_preview(const QJsonObject& preview_reply);
    void add_run(const NotebookRunRecord& rec);
    void clear_runs();
    /// Rich text colours (help HTML, history colours) after a theme change.
    void refresh_theme();

  signals:
    void run_activated(const QString& cell_id);
    void close_requested();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void render_help();
    void rebuild_history();
    void set_output_view(NbOutputView* view, const QString& caption);
    void retranslate();

    Tab tab_ = Tab::Help;
    QPushButton* tabs_[3] = {nullptr, nullptr, nullptr};
    QStackedWidget* stack_ = nullptr;
    QTextBrowser* help_ = nullptr;
    QWidget* output_page_ = nullptr;
    QVBoxLayout* output_layout_ = nullptr;
    QLabel* output_caption_ = nullptr;
    ui::EmptyState* output_empty_ = nullptr;
    NbOutputView* output_view_ = nullptr;
    QStackedWidget* history_stack_ = nullptr;
    QTreeWidget* history_ = nullptr;
    ui::EmptyState* history_empty_ = nullptr;
    QVector<NotebookRunRecord> runs_;

    enum class HelpMode { Shortcuts, Pending, Reply, Unavailable };
    HelpMode help_mode_ = HelpMode::Shortcuts;
    QString help_symbol_;
    QString help_why_;
    QJsonObject help_reply_;
};

} // namespace fincept::screens
