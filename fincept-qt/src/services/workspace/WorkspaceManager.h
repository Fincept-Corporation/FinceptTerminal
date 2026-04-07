#pragma once
#include "services/workspace/IWorkspaceParticipant.h"
#include "services/workspace/WorkspaceTypes.h"
#include "core/result/Result.h"

#include <QHash>
#include <QMainWindow>
#include <QObject>
#include <QTimer>
#include <optional>

namespace fincept {

class ScreenRouter;

/// Central workspace service — singleton QObject on the UI thread.
///
/// Lifecycle:
///   MainWindow creates it after setup_app_screens().
///   load_last_workspace() is called after auth resolves.
///   save_workspace() is called in closeEvent().
class WorkspaceManager : public QObject {
    Q_OBJECT
  public:
    static WorkspaceManager& instance();

    /// Register a window/router. Multiple windows supported (multi-window mode).
    void set_main_window(QMainWindow* w);
    void set_router(ScreenRouter* r);

    /// Unregister a closing window/router to prevent dangling pointers.
    void remove_window(QMainWindow* w);
    void remove_router(ScreenRouter* r);

    // ── Screen participation ──────────────────────────────────────────────────

    /// Called by screens in their constructor.
    void register_participant(const QString& screen_id, IWorkspaceParticipant* p);

    /// Called by screens in their destructor.
    void unregister_participant(const QString& screen_id);

    // ── Workspace operations ──────────────────────────────────────────────────

    void new_workspace(const QString& name, const QString& description = {},
                       const QString& template_id = {});

    void open_workspace(const QString& path);

    void save_workspace();

    void save_workspace_as(const QString& new_name, const QString& path);

    void import_workspace(const QString& path);

    void export_workspace(const QString& destination_path);

    /// Load the last-used workspace on startup.
    void load_last_workspace();

    // ── Queries ───────────────────────────────────────────────────────────────

    Result<QVector<WorkspaceSummary>> list_workspaces() const;

    bool has_current_workspace() const { return current_workspace_.has_value(); }

    QString current_workspace_name() const;

    QString current_workspace_path() const;

  public slots:
    /// Connected to ScreenRouter::screen_changed — delivers pending states.
    void on_screen_changed(const QString& screen_id);

  signals:
    void workspace_loaded(const WorkspaceDef& ws);
    void workspace_saved(const QString& id);
    void workspace_closed();
    void workspace_error(const QString& message);

  private:
    WorkspaceManager();

    void capture_from_ui();
    void apply_to_ui();
    void ensure_workspaces_dir() const;
    QString workspace_file_path(const QString& id) const;
    void save_last_id(const QString& id);
    void update_mru(const QString& id);

    std::optional<WorkspaceDef> current_workspace_;
    QHash<QString, IWorkspaceParticipant*> participants_;
    QHash<QString, QJsonObject> pending_states_;  // states waiting for lazy screens

    // Multi-window aware: stores all registered windows and routers.
    // The first registered window is treated as primary for workspace operations.
    QList<QMainWindow*>  windows_;
    QList<ScreenRouter*> routers_;
    QTimer*       autosave_timer_ = nullptr;
};

} // namespace fincept
