#include "services/workspace/WorkspaceManager.h"

#include "app/ScreenRouter.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "screens/dashboard/canvas/GridLayout.h"
#include "services/workspace/WorkspaceSerializer.h"
#include "services/workspace/WorkspaceTemplates.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QUuid>

namespace fincept {

// ── Singleton ─────────────────────────────────────────────────────────────────

WorkspaceManager& WorkspaceManager::instance() {
    static WorkspaceManager inst;
    return inst;
}

WorkspaceManager::WorkspaceManager() {
    autosave_timer_ = new QTimer(this);
    autosave_timer_->setInterval(5 * 60 * 1000); // 5 minutes
    connect(autosave_timer_, &QTimer::timeout, this, [this]() {
        if (current_workspace_.has_value())
            save_workspace();
    });
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void WorkspaceManager::set_main_window(QMainWindow* w) {
    if (w && !windows_.contains(w))
        windows_.append(w);
}

void WorkspaceManager::set_router(ScreenRouter* r) {
    if (r && !routers_.contains(r))
        routers_.append(r);
}

void WorkspaceManager::remove_window(QMainWindow* w) {
    windows_.removeAll(w);
}

void WorkspaceManager::remove_router(ScreenRouter* r) {
    routers_.removeAll(r);
}

// ── Participant registration ──────────────────────────────────────────────────

void WorkspaceManager::register_participant(const QString& screen_id, IWorkspaceParticipant* p) {
    participants_[screen_id] = p;

    // If a pending state exists, deliver it immediately
    if (pending_states_.contains(screen_id)) {
        p->restore_state(pending_states_.take(screen_id));
        LOG_DEBUG("WorkspaceManager", QString("Delivered pending state to: %1").arg(screen_id));
    }
}

void WorkspaceManager::unregister_participant(const QString& screen_id) {
    participants_.remove(screen_id);
}

// ── Workspace operations ──────────────────────────────────────────────────────

void WorkspaceManager::new_workspace(const QString& name, const QString& description, const QString& template_id) {
    ensure_workspaces_dir();

    WorkspaceDef ws = WorkspaceTemplates::make(template_id);
    ws.metadata.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ws.metadata.name = name;
    ws.metadata.description = description;
    ws.metadata.created_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    ws.metadata.updated_at = ws.metadata.created_at;

    current_workspace_ = std::move(ws);
    pending_states_.clear();

    apply_to_ui();

    emit workspace_closed();
    emit workspace_loaded(*current_workspace_);

    autosave_timer_->start();
    LOG_INFO("WorkspaceManager", QString("New workspace: %1").arg(name));
}

void WorkspaceManager::open_workspace(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit workspace_error(QString("Cannot open file: %1").arg(path));
        return;
    }
    QByteArray data = f.readAll();
    f.close();

    auto result = WorkspaceSerializer::from_json(QJsonDocument::fromJson(data));
    if (result.is_err()) {
        emit workspace_error(QString::fromStdString(result.error()));
        return;
    }

    // Save current workspace before switching
    if (current_workspace_.has_value())
        save_workspace();

    current_workspace_ = std::move(result.value());
    pending_states_.clear();

    apply_to_ui();

    save_last_id(current_workspace_->metadata.id);
    update_mru(current_workspace_->metadata.id);

    emit workspace_loaded(*current_workspace_);
    autosave_timer_->start();

    LOG_INFO("WorkspaceManager", QString("Opened workspace: %1").arg(current_workspace_->metadata.name));
}

void WorkspaceManager::save_workspace() {
    if (!current_workspace_.has_value()) {
        new_workspace("Default");
        return;
    }

    ensure_workspaces_dir();
    capture_from_ui();

    current_workspace_->metadata.updated_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonDocument doc = WorkspaceSerializer::to_json(*current_workspace_);
    QString path = workspace_file_path(current_workspace_->metadata.id);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit workspace_error(QString("Cannot write workspace: %1").arg(path));
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    save_last_id(current_workspace_->metadata.id);
    emit workspace_saved(current_workspace_->metadata.id);
    LOG_INFO("WorkspaceManager", QString("Saved workspace: %1").arg(current_workspace_->metadata.name));
}

void WorkspaceManager::save_workspace_as(const QString& new_name, const QString& path) {
    if (!current_workspace_.has_value())
        return;

    capture_from_ui();

    WorkspaceDef ws = *current_workspace_;
    ws.metadata.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ws.metadata.name = new_name;
    ws.metadata.updated_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonDocument doc = WorkspaceSerializer::to_json(ws);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit workspace_error(QString("Cannot write workspace: %1").arg(path));
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();

    // Switch to the new workspace
    current_workspace_ = std::move(ws);
    save_last_id(current_workspace_->metadata.id);
    emit workspace_saved(current_workspace_->metadata.id);
    emit workspace_loaded(*current_workspace_);
    LOG_INFO("WorkspaceManager", QString("Saved workspace as: %1").arg(new_name));
}

void WorkspaceManager::import_workspace(const QString& path) {
    auto summary = WorkspaceSerializer::summary_from_file(path);
    if (summary.is_err()) {
        emit workspace_error(QString::fromStdString(summary.error()));
        return;
    }

    ensure_workspaces_dir();

    // Assign new UUID to avoid collision
    QString new_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString dest = workspace_file_path(new_id);

    QFile::copy(path, dest);

    // Patch the id in the copied file
    QFile f(dest);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray data = f.readAll();
        f.close();
        auto result = WorkspaceSerializer::from_json(QJsonDocument::fromJson(data));
        if (result.is_ok()) {
            WorkspaceDef ws = result.value();
            ws.metadata.id = new_id;
            QFile fw(dest);
            if (fw.open(QIODevice::WriteOnly | QIODevice::Truncate))
                fw.write(WorkspaceSerializer::to_json(ws).toJson(QJsonDocument::Indented));
        }
    }

    open_workspace(dest);
    LOG_INFO("WorkspaceManager", QString("Imported workspace from: %1").arg(path));
}

void WorkspaceManager::export_workspace(const QString& destination_path) {
    if (!current_workspace_.has_value())
        return;
    capture_from_ui();

    QJsonDocument doc = WorkspaceSerializer::to_json(*current_workspace_);
    QFile f(destination_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit workspace_error(QString("Cannot export to: %1").arg(destination_path));
        return;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    LOG_INFO("WorkspaceManager", QString("Exported workspace to: %1").arg(destination_path));
}

void WorkspaceManager::load_last_workspace() {
    auto id_result = SettingsRepository::instance().get("workspace.last_id");
    if (id_result.is_ok() && !id_result.value().isEmpty()) {
        QString path = workspace_file_path(id_result.value());
        if (QFile::exists(path)) {
            open_workspace(path);
            return;
        }
    }
    // No saved workspace — create default
    new_workspace("Default");
}

// ── Queries ───────────────────────────────────────────────────────────────────

Result<QVector<WorkspaceSummary>> WorkspaceManager::list_workspaces() const {
    ensure_workspaces_dir();
    QDir dir(AppPaths::workspaces());
    QVector<WorkspaceSummary> result;

    const auto entries = dir.entryInfoList({"*.fwsp"}, QDir::Files, QDir::Time);
    for (const auto& fi : entries) {
        auto s = WorkspaceSerializer::summary_from_file(fi.absoluteFilePath());
        if (s.is_ok())
            result.append(s.value());
    }
    return Result<QVector<WorkspaceSummary>>::ok(std::move(result));
}

QString WorkspaceManager::current_workspace_name() const {
    return current_workspace_.has_value() ? current_workspace_->metadata.name : QString{};
}

QString WorkspaceManager::current_workspace_path() const {
    return current_workspace_.has_value() ? workspace_file_path(current_workspace_->metadata.id) : QString{};
}

// ── Screen changed slot ───────────────────────────────────────────────────────

void WorkspaceManager::on_screen_changed(const QString& screen_id) {
    if (!current_workspace_.has_value())
        return;
    current_workspace_->active_screen = screen_id;

    // Track open screens
    if (!current_workspace_->open_screens.contains(screen_id))
        current_workspace_->open_screens.append(screen_id);

    // Deliver pending state if participant is now registered
    if (pending_states_.contains(screen_id) && participants_.contains(screen_id)) {
        participants_[screen_id]->restore_state(pending_states_.take(screen_id));
        LOG_DEBUG("WorkspaceManager", QString("Delivered pending state on navigate: %1").arg(screen_id));
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void WorkspaceManager::capture_from_ui() {
    if (!current_workspace_.has_value())
        return;

    // Window geometry — use primary window (first registered)
    if (!windows_.isEmpty())
        current_workspace_->window_geometry_base64 = windows_.first()->saveGeometry().toBase64();

    // Active screen — use primary router (first registered)
    if (!routers_.isEmpty())
        current_workspace_->active_screen = routers_.first()->current_screen_id();

    // Per-screen states from participants
    current_workspace_->screen_states.clear();
    for (auto it = participants_.cbegin(); it != participants_.cend(); ++it) {
        WorkspaceScreenState ss;
        ss.screen_id = it.key();
        ss.state = it.value()->save_state();
        current_workspace_->screen_states.append(ss);
    }
}

void WorkspaceManager::apply_to_ui() {
    if (!current_workspace_.has_value())
        return;
    const WorkspaceDef& ws = *current_workspace_;

    // Store dashboard profile key so DashboardScreen picks it up
    SettingsRepository::instance().set("workspace.active_dashboard_profile", ws.metadata.id, "workspace");

    // Store dashboard layout as base64 in the settings key DashboardScreen reads
    // We serialize the layout and write it so restore_layout() loads the right grid
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << ws.dashboard_layout.cols << ws.dashboard_layout.row_h << ws.dashboard_layout.margin;
        stream << static_cast<int>(ws.dashboard_layout.items.size());
        for (const auto& item : ws.dashboard_layout.items) {
            stream << item.id << item.instance_id;
            stream << item.cell.x << item.cell.y << item.cell.w << item.cell.h;
            stream << item.cell.min_w << item.cell.min_h;
        }
        if (!ws.dashboard_layout.items.isEmpty()) {
            SettingsRepository::instance().set("dashboard_canvas_layout", QString::fromLatin1(data.toBase64()),
                                               "dashboard");
        }
    }

    // Queue pending screen states for lazy-constructed screens
    for (const auto& ss : ws.screen_states) {
        if (participants_.contains(ss.screen_id)) {
            // Screen already constructed — deliver immediately
            participants_[ss.screen_id]->restore_state(ss.state);
        } else {
            // Screen not yet constructed — queue for when it registers
            pending_states_[ss.screen_id] = ss.state;
        }
    }

    // Restore window geometry — apply to primary window
    if (!windows_.isEmpty() && !ws.window_geometry_base64.isEmpty())
        windows_.first()->restoreGeometry(QByteArray::fromBase64(ws.window_geometry_base64.toLatin1()));

    // Navigate to active screen — apply to primary router
    if (!routers_.isEmpty() && !ws.active_screen.isEmpty())
        routers_.first()->navigate(ws.active_screen);
}

void WorkspaceManager::ensure_workspaces_dir() const {
    QDir().mkpath(AppPaths::workspaces());
}

QString WorkspaceManager::workspace_file_path(const QString& id) const {
    return AppPaths::workspaces() + "/" + id + ".fwsp";
}

void WorkspaceManager::save_last_id(const QString& id) {
    SettingsRepository::instance().set("workspace.last_id", id, "workspace");
}

void WorkspaceManager::update_mru(const QString& id) {
    auto r = SettingsRepository::instance().get("workspace.mru_list");
    QStringList mru;
    if (r.is_ok() && !r.value().isEmpty())
        mru = r.value().split(',', Qt::SkipEmptyParts);

    mru.removeAll(id);
    mru.prepend(id);
    if (mru.size() > 5)
        mru = mru.mid(0, 5);

    SettingsRepository::instance().set("workspace.mru_list", mru.join(','), "workspace");
}

} // namespace fincept
