#pragma once
// ReportBuilderScreen — view onto ReportBuilderService.
//
// All document state (components, metadata, theme, undo, library row, file link, recent
// files) lives in fincept::services::ReportBuilderService; live data in
// ReportDataBinder; the library in ReportLibrary; inbound blocks from other tabs come
// through ReportInbox. The screen renders that state, forwards user gestures to the
// services and observes their signals, so changes made while the tab is closed appear
// when it opens.
//
// Pages: the report library / start page (never a blank tab) and the editor (left: report
// actions + structure, centre: the paginated page view, right: properties).
// Split across ReportBuilderScreen.cpp (lifecycle, toolbar, state),
// _Components.cpp (edits, service -> view, live data), _Dialogs.cpp (dialogs, files,
// exports) and _Tools.cpp (variables, schedules, stale-save conflicts, export formats, AI
// drafting).

#include "core/report/ReportDocument.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/report_builder/ComponentToolbar.h"
#include "screens/report_builder/DocumentCanvas.h"
#include "screens/report_builder/PropertiesPanel.h"
#include "screens/report_builder/ReportExportJob.h"

#include <QEvent>
#include <QPointer>
#include <QSet>
#include <QVariantMap>
#include <QWidget>

#include <functional>

class QDialog;
class QLabel;
class QProgressBar;
class QPropertyAnimation;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;

namespace fincept::services {
struct ApplyResult;
}

namespace fincept::screens {

using ReportComponent = ::fincept::report::ReportComponent;
using ReportMetadata = ::fincept::report::ReportMetadata;
using ReportTheme = ::fincept::report::ReportTheme;

class ReportLibraryPage;
class ReportAiPanel;

class ReportBuilderScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit ReportBuilderScreen(QWidget* parent = nullptr);
    ~ReportBuilderScreen() override;

    // ISectionLinked — "library" (the start page), "editor" (the open report), "new" (what NEW
    // does), "report:<library id>". `focus` = a library report id: opens that report (as the
    // library's OPEN does; its own unsaved-file question first).
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "report_builder"; }
    // v3 = selected_id; v4 adds page ("library" | "editor").
    int state_version() const override { return 4; }

    /// The currently constructed Report Builder screen, or nullptr.
    static ReportBuilderScreen* current();

    /// Open a .fincept file in the Report Builder (Files screen "Open in Report Builder").
    /// Parks the request in the service; the screen takes it when it shows.
    static void open_when_ready(const QString& path);

    using ExportDone = std::function<void(bool ok, const QString& path, const QString& error)>;
    /// Exports that never block: written in slices with progress and cancel. `done` runs
    /// when the file is complete (or failed / was cancelled).
    void export_pdf_to(const QString& path, ExportDone done = {});
    void export_html_to(const QString& path, ExportDone done = {});

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    // ── Construction (ReportBuilderScreen.cpp) ──────────────────────────────
    QWidget* build_toolbar();
    QWidget* build_editor_page();
    void wire_services();
    void retranslateUi();
    void update_title();
    void show_library();
    void show_editor();
    void refit_panels();
    void fit_toolbar();

    // ── Edits / view sync (_Components.cpp) ─────────────────────────────────
    void add_component(const QString& type);
    void select_component(int id, bool scroll_into_view);
    void remove_component(int id);
    void duplicate_component(int id);
    void move_component_by(int id, int delta);
    void schedule_refresh();
    void do_refresh();
    void refresh_structure();
    void refresh_properties(bool force);
    void on_document_loaded();
    void apply_binding(int id, const QString& binding_json);
    void refresh_all_data();
    void update_refresh_button();
    void check_inbox_notice();
    void check_stale_notice();
    void apply_left_collapsed(bool collapsed, bool animate);
    void apply_right_collapsed(bool collapsed, bool animate);

    // ── Notices ────────────────────────────────────────────────────────────
    enum class NoticeKind { None, Inbox, Stale, Info, Error, Export };
    void show_notice(NoticeKind kind, const QString& text, const QString& action_text, const char* tone);
    void clear_notice(NoticeKind only = NoticeKind::None);
    void on_notice_action();

    // ── Dialogs, files, exports (_Dialogs.cpp) ──────────────────────────────
    void show_recent_dialog();
    void show_theme_dialog();
    void show_metadata_dialog();
    void show_page_setup_dialog();
    void show_save_template_dialog();
    void on_new();
    void on_open();
    void on_import();
    void on_save();
    void on_save_as();
    void on_export_pdf();
    void on_export_html();
    void on_export(ReportExportJob::Format format);
    void open_file_async(const QString& path);
    void open_library_report(qint64 id, int select_block_id);
    void apply_template(const QString& template_id, const QVariantMap& params);
    void handle_open_request();
    /// The open report is linked to a file with edits not written to it: ask.
    /// Returns false when the user cancels.
    bool confirm_leave_file(const QString& title);
    void start_export(ReportExportJob::Format format, const QString& path, ExportDone done);
    bool confirm_unfilled(const QString& title);
    /// Variables with no value: ask before printing them as typed. False = cancelled.
    bool confirm_unresolved(const QString& title);

    // ── Variables, schedules, conflicts, AI drafting (_Tools.cpp) ────────────
    void show_variables_dialog();
    void show_schedule_dialog();
    void show_conflict_dialog();
    void open_ai_panel(int block_id, const QString& action, const QString& tone, int sel_start, int sel_end);
    void apply_ai_result(const services::ApplyResult& result);

    // Children
    QStackedWidget* pages_ = nullptr;
    ReportLibraryPage* library_page_ = nullptr;
    QWidget* editor_page_ = nullptr;
    ComponentToolbar* comp_toolbar_ = nullptr;
    DocumentCanvas* canvas_ = nullptr;
    PropertiesPanel* properties_ = nullptr;
    QSplitter* splitter_ = nullptr;

    QWidget* notice_bar_ = nullptr;
    QLabel* notice_lbl_ = nullptr;
    QPushButton* notice_action_ = nullptr;
    QPushButton* notice_close_ = nullptr;
    NoticeKind notice_kind_ = NoticeKind::None;
    QString notice_path_;

    QWidget* export_bar_ = nullptr;
    QLabel* export_lbl_ = nullptr;
    QProgressBar* export_progress_ = nullptr;
    QPushButton* export_cancel_ = nullptr;
    QPointer<ReportExportJob> export_job_;

    // Toolbar
    QWidget* toolbar_ = nullptr;
    QPushButton* left_toggle_btn_ = nullptr;
    QPushButton* right_toggle_btn_ = nullptr;
    QLabel* toolbar_title_ = nullptr;
    QLabel* doc_title_ = nullptr;
    QPushButton* library_btn_ = nullptr;
    QPushButton* undo_btn_ = nullptr;
    QPushButton* redo_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QToolButton* save_btn_ = nullptr;
    QPushButton* refresh_data_btn_ = nullptr;
    QPushButton* preview_btn_ = nullptr;
    QToolButton* export_btn_ = nullptr;

    // Non-modal dialogs (one of each at a time).
    QPointer<QDialog> recent_dlg_;
    QPointer<QDialog> theme_dlg_;
    QPointer<QDialog> meta_dlg_;
    QPointer<QDialog> page_dlg_;
    QPointer<QDialog> template_dlg_;
    QPointer<QDialog> vars_dlg_;
    QPointer<QDialog> schedule_dlg_;
    QPointer<QDialog> conflict_dlg_;

    QSplitter* right_split_ = nullptr; ///< properties above, the AI proposal panel below
    ReportAiPanel* ai_panel_ = nullptr;

    QTimer* refresh_timer_ = nullptr;
    QSet<int> panel_echo_ids_; ///< updates that came FROM the properties panel
    ReportComponent shown_component_;
    int selected_id_ = 0;
    bool started_ = false;
    bool hidden_dirty_ = false; ///< the document changed while the tab was hidden
    bool left_collapsed_ = false;
    bool right_collapsed_ = false;
    bool library_saving_ = false;
    QPropertyAnimation* left_anim_ = nullptr;
    QPropertyAnimation* right_anim_ = nullptr;
    QString saved_state_text_;
};

} // namespace fincept::screens
