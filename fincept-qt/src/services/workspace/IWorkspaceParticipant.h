#pragma once
#include <QJsonObject>

namespace fincept {

/// Opt-in interface for screens that have in-memory state worth saving
/// into a workspace snapshot.
///
/// Usage:
///   1. Inherit alongside QWidget: class MyScreen : public QWidget, public IWorkspaceParticipant
///   2. In constructor: WorkspaceManager::instance().register_participant("my_screen", this);
///   3. In destructor:  WorkspaceManager::instance().unregister_participant("my_screen");
///   4. Implement save_state() and restore_state().
///
/// restore_state() may be called before showEvent() if the screen is already
/// constructed, or immediately upon registration if a pending state exists.
/// Implementations must be safe to call before setup_ui() completes — guard
/// with an initialised_ flag if needed.
class IWorkspaceParticipant {
  public:
    virtual ~IWorkspaceParticipant() = default;

    /// Capture current in-memory state into a JSON object.
    /// Called from the UI thread only.
    virtual QJsonObject save_state() const = 0;

    /// Restore state from a previously saved JSON object.
    /// Called from the UI thread only.
    /// Implementations should defer heavy work (broker init, data fetch)
    /// to showEvent() by storing values in member variables here.
    virtual void restore_state(const QJsonObject& state) = 0;
};

} // namespace fincept
