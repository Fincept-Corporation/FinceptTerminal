#pragma once
#include "screens/dashboard/canvas/GridLayout.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept {

// ── Workspace screen state ────────────────────────────────────────────────────

/// Opaque per-screen state blob saved/restored by IWorkspaceParticipant.
struct WorkspaceScreenState {
    QString screen_id;
    QJsonObject state;
};

// ── Workspace metadata ────────────────────────────────────────────────────────

struct WorkspaceMetadata {
    QString id; // UUID
    QString name;
    QString description;
    QString template_id; // empty = user-created
    QString created_at;  // ISO-8601
    QString updated_at;  // ISO-8601
    int version = 1;
};

// ── Full workspace definition ─────────────────────────────────────────────────

struct WorkspaceDef {
    WorkspaceMetadata metadata;

    // Navigation
    QString active_screen;
    QStringList open_screens; // screens visited, in order

    // Dashboard widget grid — self-contained, not a DB reference
    screens::GridLayout dashboard_layout;

    // References to existing DB entities (by ID)
    QStringList watchlist_ids;
    QStringList portfolio_ids;
    QStringList workflow_ids;

    // Per-screen in-memory state
    QVector<WorkspaceScreenState> screen_states;

    // Window geometry (base64 of QWidget::saveGeometry())
    QString window_geometry_base64;

    // SymbolContext snapshot — per-group active security assignments.
    // Opaque blob authored by SymbolContext::to_json(), consumed by
    // SymbolContext::from_json() on workspace load. Empty on first run.
    QJsonObject symbol_context;
};

// ── Lightweight summary for listing dialogs ───────────────────────────────────

struct WorkspaceSummary {
    QString id;
    QString name;
    QString description;
    QString updated_at;
    QString template_id;
    QString file_path;
};

} // namespace fincept
