#pragma once

#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QDialog>
#include <QList>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace fincept {

class CrashRecovery;
class WorkspaceSnapshotRing;

} // namespace fincept

namespace fincept::screens {

/// Phase 6 final: modal dialog shown at startup when the previous session
/// ended uncleanly and at least one auto/crash_recovery snapshot is available.
///
/// Behaviour:
///   - Lists the last N snapshots (default 5) newest-first, with timestamp +
///     kind label ("auto" / "crash_recovery" / "named_save"). Selecting a
///     row enables the Restore button.
///   - "Restore" loads the snapshot payload, parses it as a Workspace, and
///     applies it via `WorkspaceShell::apply` after the dialog accepts.
///   - "Skip" dismisses the dialog with reject(); main.cpp continues with
///     the normal startup path (primary frame is constructed afterwards).
///   - The dialog is intentionally **not** modal-to-a-window, because it
///     runs *before* any WindowFrame exists. parent==nullptr is normal.
///
/// Threading: UI thread only. SQL reads go through WorkspaceDb's mutex via
/// the snapshot ring, so the list-load is safe even if SessionManager is
/// already running an autosave on its own thread.
class CrashRecoveryDialog : public QDialog {
    Q_OBJECT
  public:
    /// Construct with the live CrashRecovery + ring pointers from
    /// TerminalShell. Both must outlive the dialog (they're owned by the
    /// shell and only released in shutdown()).
    explicit CrashRecoveryDialog(fincept::CrashRecovery* recovery,
                                 fincept::WorkspaceSnapshotRing* ring,
                                 QWidget* parent = nullptr);

    /// True iff the user picked "Restore" and the snapshot was successfully
    /// decoded + applied. False when the user picked "Skip" or no snapshot
    /// could be parsed (parse failures fall back to skip with a warning).
    bool was_restored() const { return restored_; }

  private slots:
    void on_selection_changed();
    void on_restore_clicked();
    void on_skip_clicked();

  private:
    void build_ui();
    void populate_snapshots();
    QString format_timestamp(qint64 unix_ms) const;
    QString format_kind(const QString& kind) const;

    fincept::CrashRecovery* recovery_ = nullptr;
    fincept::WorkspaceSnapshotRing* ring_ = nullptr;

    QLabel* heading_ = nullptr;
    QLabel* explainer_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* restore_button_ = nullptr;
    QPushButton* skip_button_ = nullptr;

    QList<fincept::WorkspaceSnapshotRing::Entry> entries_;
    bool restored_ = false;
};

} // namespace fincept::screens
