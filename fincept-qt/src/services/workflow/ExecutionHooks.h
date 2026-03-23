#pragma once

#include "screens/node_editor/NodeEditorTypes.h"

#include <QObject>
#include <QMap>
#include <QVector>

#include <functional>

namespace fincept::workflow {

/// Hook event types for workflow execution lifecycle.
enum class HookEvent {
    WorkflowStart,
    WorkflowEnd,
    WorkflowError,
    NodeStart,
    NodeEnd,
    NodeError,
    PerformanceMetric,
};

/// Data passed to hook callbacks.
struct HookEventData {
    HookEvent event;
    QString workflow_id;
    QString node_id;
    QString node_type;
    bool success = true;
    QString error;
    int duration_ms = 0;
    QJsonValue data;
};

/// Callback type for hooks.
using HookCallback = std::function<void(const HookEventData&)>;

/// Manages lifecycle hooks for workflow execution.
/// Allows registering callbacks for start/end/error events.
class ExecutionHooks : public QObject {
    Q_OBJECT
  public:
    static ExecutionHooks& instance();

    /// Register a hook. Returns a handle for removal.
    int on(HookEvent event, HookCallback callback, int priority = 0);

    /// Register a one-time hook (auto-removed after first call).
    int once(HookEvent event, HookCallback callback, int priority = 0);

    /// Remove a hook by handle.
    void remove(int handle);

    /// Remove all hooks for an event.
    void clear(HookEvent event);

    /// Remove all hooks.
    void clear_all();

    /// Emit a hook event to all registered callbacks.
    void emit_event(const HookEventData& data);

    // Convenience emitters
    void emit_workflow_start(const QString& workflow_id);
    void emit_workflow_end(const QString& workflow_id, bool success, int duration_ms);
    void emit_workflow_error(const QString& workflow_id, const QString& error);
    void emit_node_start(const QString& workflow_id, const QString& node_id, const QString& node_type);
    void emit_node_end(const QString& workflow_id, const QString& node_id, bool success, int duration_ms);
    void emit_node_error(const QString& workflow_id, const QString& node_id, const QString& error);

  signals:
    void hook_fired(const HookEventData& data);

  private:
    ExecutionHooks();

    struct HookEntry {
        int handle;
        HookEvent event;
        HookCallback callback;
        int priority;
        bool one_shot;
    };

    QVector<HookEntry> hooks_;
    int next_handle_ = 1;
};

} // namespace fincept::workflow
