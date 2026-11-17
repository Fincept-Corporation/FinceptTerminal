#pragma once
// PropertiesPanel — the Report Builder's right panel.
//
// Nothing selected: the document's properties (title, author, paper, theme, blocks,
// pages, where it is stored) with EDIT DOCUMENT INFO / PAGE SETUP / THEME — never a
// blank panel. A block selected: its editor, plus a DATA SOURCE group for blocks that can
// show live data (table, chart, stats, sparkline, market data, list): the recipe (source,
// symbols via the shared symbol search, range / statement / count ...), REFRESH, its state
// ("Updated 14:02" / "Refreshing…" / "Could not load: …") and "Unbind (keep numbers)".
//
// Edits are addressed by the component's stable id (never an index) and typing is
// debounced (one service update per pause, not per keystroke).

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QTimer;
class QVBoxLayout;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

using ReportComponent = ::fincept::report::ReportComponent;

class PropertiesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    /// Edit `component` (rebuilds the editor; pending typing is flushed first).
    void show_component(const ReportComponent& component);
    /// Document properties (nothing selected).
    void show_document(const report::ReportMetadata& meta, const report::ReportTheme& theme, int blocks, int pages,
                       qint64 library_id, const QString& file_path);
    int current_id() const { return current_id_; }
    /// Send pending (debounced) edits now.
    void flush_pending();
    /// Put the keyboard focus in the editor's first field (double-click on the page).
    void focus_first_editor();
    /// Width that fits the editors at the current UI font.
    int preferred_width() const;

  signals:
    void content_changed(int id, const QString& content);
    void config_changed(int id, const QString& key, const QString& value);
    /// New data recipe for block `id` (empty = unbind, keep the numbers).
    void binding_changed(int id, const QString& binding_json);
    void refresh_requested(int id);
    void duplicate_requested(int id);
    void delete_requested(int id);
    void document_info_requested();
    void page_setup_requested();
    void theme_requested();
    void variables_requested();
    /// An AI drafting action on block `id` ("draft" | "rewrite" | "shorten" | "expand" |
    /// "grammar" | "tone"), on the selected text [sel_start, sel_end) or the whole text (-1).
    /// Nothing is sent from here: the screen opens the proposal panel, where the user sends.
    void ai_requested(int id, const QString& action, const QString& tone, int sel_start, int sel_end);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_editor(const ReportComponent& c);
    void build_data_source(const ReportComponent& c);
    void build_table_formats(const ReportComponent& c);
    void build_chart_options(const ReportComponent& c, bool bound);
    void build_cover_editor(const ReportComponent& c);
    void add_ai_actions(QTextEdit* editor, int id);
    void add_picture_buttons(int id, QLabel* path_lbl);
    void update_data_status();
    void queue_edit(const QString& key, const QString& value); ///< key "@content" = the text
    QLabel* add_label(const QString& text, const char* role = nullptr);
    QPushButton* add_button(const QString& text, const char* role = nullptr);
    void retranslateUi();

    QStackedWidget* stack_ = nullptr;
    ui::EmptyState* doc_state_ = nullptr;
    QPushButton* theme_btn_ = nullptr;
    QWidget* editor_widget_ = nullptr;
    QVBoxLayout* editor_layout_ = nullptr;
    QWidget* first_editor_ = nullptr;

    int current_id_ = 0;
    ReportComponent last_component_;
    bool has_component_ = false;

    // Debounced edits of the shown component.
    QTimer* edit_timer_ = nullptr;
    QHash<QString, QString> pending_;
    int pending_id_ = 0;

    // Data source group.
    QLabel* data_status_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* vars_btn_ = nullptr;
    /// Table formats: the column being edited (kept across rebuilds of the same block).
    int format_column_ = 0;
    int format_column_block_ = 0;

    // Document page (for retranslate).
    report::ReportMetadata doc_meta_;
    report::ReportTheme doc_theme_;
    int doc_blocks_ = 0;
    int doc_pages_ = 1;
    qint64 doc_library_id_ = 0;
    QString doc_file_;
};

} // namespace fincept::screens
