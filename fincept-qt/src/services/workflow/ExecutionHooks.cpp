#include "services/workflow/ExecutionHooks.h"

#include "core/logging/Logger.h"

#include <algorithm>

namespace fincept::workflow {

ExecutionHooks& ExecutionHooks::instance() {
    static ExecutionHooks s;
    return s;
}

ExecutionHooks::ExecutionHooks() : QObject(nullptr) {}

int ExecutionHooks::on(HookEvent event, HookCallback callback, int priority) {
    int handle = next_handle_++;
    hooks_.append({handle, event, std::move(callback), priority, false});
    // Sort by priority (higher first)
    std::stable_sort(hooks_.begin(), hooks_.end(),
                     [](const HookEntry& a, const HookEntry& b) { return a.priority > b.priority; });
    return handle;
}

int ExecutionHooks::once(HookEvent event, HookCallback callback, int priority) {
    int handle = next_handle_++;
    hooks_.append({handle, event, std::move(callback), priority, true});
    std::stable_sort(hooks_.begin(), hooks_.end(),
                     [](const HookEntry& a, const HookEntry& b) { return a.priority > b.priority; });
    return handle;
}

void ExecutionHooks::remove(int handle) {
    hooks_.erase(
        std::remove_if(hooks_.begin(), hooks_.end(), [handle](const HookEntry& e) { return e.handle == handle; }),
        hooks_.end());
}

void ExecutionHooks::clear(HookEvent event) {
    hooks_.erase(std::remove_if(hooks_.begin(), hooks_.end(), [event](const HookEntry& e) { return e.event == event; }),
                 hooks_.end());
}

void ExecutionHooks::clear_all() {
    hooks_.clear();
}

void ExecutionHooks::emit_event(const HookEventData& data) {
    QVector<int> to_remove;
    for (auto& entry : hooks_) {
        if (entry.event == data.event) {
            entry.callback(data);
            if (entry.one_shot)
                to_remove.append(entry.handle);
        }
    }
    for (int h : to_remove)
        remove(h);

    emit hook_fired(data);
}

void ExecutionHooks::emit_workflow_start(const QString& workflow_id) {
    emit_event({HookEvent::WorkflowStart, workflow_id, {}, {}, true, {}, 0, {}});
}

void ExecutionHooks::emit_workflow_end(const QString& workflow_id, bool success, int duration_ms) {
    emit_event({HookEvent::WorkflowEnd, workflow_id, {}, {}, success, {}, duration_ms, {}});
}

void ExecutionHooks::emit_workflow_error(const QString& workflow_id, const QString& error) {
    emit_event({HookEvent::WorkflowError, workflow_id, {}, {}, false, error, 0, {}});
}

void ExecutionHooks::emit_node_start(const QString& workflow_id, const QString& node_id, const QString& node_type) {
    emit_event({HookEvent::NodeStart, workflow_id, node_id, node_type, true, {}, 0, {}});
}

void ExecutionHooks::emit_node_end(const QString& workflow_id, const QString& node_id, bool success, int duration_ms) {
    emit_event({HookEvent::NodeEnd, workflow_id, node_id, {}, success, {}, duration_ms, {}});
}

void ExecutionHooks::emit_node_error(const QString& workflow_id, const QString& node_id, const QString& error) {
    emit_event({HookEvent::NodeError, workflow_id, node_id, {}, false, error, 0, {}});
}

} // namespace fincept::workflow
