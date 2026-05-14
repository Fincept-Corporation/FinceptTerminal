#pragma once
// ReportBuilderScreen — view onto ReportBuilderService.
//
// All document state (components, metadata, theme, undo stack, autosave,
// recent files) lives in fincept::services::ReportBuilderService. This
// screen renders that state and forwards user gestures to the service.
// LLM/MCP tools mutate the same service on a different thread; the screen
// observes service signals and re-renders live.

#include "core/report/ReportDocument.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/report_builder/ComponentToolbar.h"
#include "screens/report_builder/DocumentCanvas.h"
#include "screens/report_builder/PropertiesPanel.h"

#include <QPropertyAnimation>
#include <QPushButton>
#include <QSplitter>
#include <QUndoStack>
#include <QWidget>

namespace fincept::screens {

// Type aliases — ComponentToolbar / DocumentCanvas / PropertiesPanel were
// originally written against types declared inside fincept::screens. After
// the model moved into core/report we keep those aliases so the existing
// callers (and forward declarations like `struct ReportComponent` in
// PropertiesPanel.h) keep resolving.
using ReportComponent = ::fincept::report::ReportComponent;
using ReportMetadata = ::fincept::report::ReportMetadata;
using ReportTheme = ::fincept::report::ReportTheme;

class ReportBuilderScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit ReportBuilderScreen(QWidget* parent = nullptr);
    ~ReportBuilderScreen() override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "report_builder"; }
    // v2 adds: left_collapsed, right_collapsed.
    // v3 = ServiceMigration — selected_id (stable id, not index).
    int state_version() const override { return 3; }

  private:
    QWidget* build_toolbar();

    // Dialogs (UI shells; service does the actual work)
    void show_recent_dialog();
    void show_template_dialog();
    void show_theme_dialog();
    void show_metadata_dialog();

    // User-driven operations — all go through the service
    void add_component(const QString& type);
    void select_component(int index);
    void remove_component_at(int index);
    void duplicate_at(int index);
    void move_up_at(int index);
    void move_down_at(int index);

    // Service → screen
    void rebind_from_service();
    void on_component_added(int id, int index);
    void on_component_updated(int id);
    void on_component_removed(int id, int prior_index);
    void on_component_moved(int id, int from, int to);
    void on_metadata_changed();
    void on_theme_changed();
    void on_document_changed();
    void on_document_loaded(const QString& path);

    void refresh_canvas();
    void refresh_structure();

    // Children
    ComponentToolbar* comp_toolbar_ = nullptr;
    DocumentCanvas* canvas_ = nullptr;
    PropertiesPanel* properties_ = nullptr;
    QSplitter* splitter_ = nullptr;

    // Selection — tracked by stable id so it survives mutations from the
    // LLM that shift indices. -1 / 0 = nothing selected.
    int selected_id_ = 0;

    // ── Side-panel collapse state ───────────────────────────────────────
    QPushButton* left_toggle_btn_ = nullptr;
    QPushButton* right_toggle_btn_ = nullptr;
    QPropertyAnimation* left_anim_ = nullptr;
    QPropertyAnimation* right_anim_ = nullptr;
    bool left_collapsed_ = false;
    bool right_collapsed_ = false;
    static constexpr int kLeftPanelWidth = 240;
    static constexpr int kRightPanelWidth = 280;
    void apply_left_collapsed(bool collapsed, bool animate);
    void apply_right_collapsed(bool collapsed, bool animate);

  private slots:
    void on_toggle_left();
    void on_toggle_right();
    void on_save();
    void on_open();
    void on_new();
    void on_export_pdf();
    void on_preview();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
};

} // namespace fincept::screens
