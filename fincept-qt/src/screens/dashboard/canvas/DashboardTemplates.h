#pragma once
#include "screens/dashboard/canvas/GridLayout.h"

#include <QCoreApplication>
#include <QString>
#include <QVector>

namespace fincept::screens {

/// A built-in dashboard layout. Designed on the 12-column grid in COMPACT rows
/// (kGridRowCompact): every row of tiles spans the full width and the tiles of a
/// row end on the same grid line, so the canvas is filled with no gaps at 12
/// columns and — through the proportional fold (fold_items_to_cols) — at 9 and 6.
/// Every item carries the config its widget needs to show data at once, and every
/// template places one "desk_note" seeded with template_note_seed(id).
struct DashboardTemplate {
    QString id;
    QString display_name; ///< English (QT_TRANSLATE_NOOP); show template_display_name_tr()
    QString description;  ///< one-line purpose, English; show template_description_tr()
    QString requirement;  ///< English; what some tiles need (a portfolio, Algo Trading), "" = none
    QString region;       ///< MarketProfile id the tiles are pinned to ("" = follow the region selector)
    QVector<GridItem> items; ///< instance_id empty — filled by build_template_layout()
    QString scope = {};   ///< English; what the tiles cover when it is neither "your region" nor `region`
};

/// The built-in templates, "market_overview" (the default for a new dashboard)
/// first. Region-aware: tiles that follow the region read the user's
/// MarketProfile; region templates (India, Europe, Asia-Pacific) pin theirs.
QVector<DashboardTemplate> all_dashboard_templates();
/// The template with `id`; false when there is none.
bool find_dashboard_template(const QString& id, DashboardTemplate* out);
/// A ready-to-load layout of `t`: compact rows, fresh instance ids, min sizes
/// from the widget registry, template_id set, pristine — and the desk note
/// carrying the user's own text when they edited this template's note before.
GridLayout build_template_layout(const DashboardTemplate& t);

/// Translated display_name / description / requirement. Storage stays English;
/// QCoreApplication::translate resolves under "fincept::screens::DashboardTemplates".
QString template_display_name_tr(const DashboardTemplate& t);
QString template_description_tr(const DashboardTemplate& t);
QString template_requirement_tr(const DashboardTemplate& t);
QString template_scope_tr(const DashboardTemplate& t);
/// Translated name of the template `id` ("" when unknown).
QString template_display_name_for(const QString& id);

// ── Template desk notes ───────────────────────────────────────────────────────
// Seed text (Markdown, translatable) shipped for each template, and the user's
// edited version per template id, kept in SettingsRepository under
// "dashboard.template_notes" = {"<template id>": {"text": "...", "saved_at": ISO}}.
// The tile's own config holds the text of THAT tile (DeskNoteWidget); this store
// is what a re-applied template starts from.

/// The template's seed note, translated; "" for an unknown id.
QString template_note_seed(const QString& template_id);
/// The user's saved text for the template's note; false when they never edited
/// it (or restored the seed).
bool template_note_saved(const QString& template_id, QString* text);
void template_note_store(const QString& template_id, const QString& text);
void template_note_forget(const QString& template_id);

} // namespace fincept::screens
