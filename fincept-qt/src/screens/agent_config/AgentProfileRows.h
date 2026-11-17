#pragma once
// AgentProfileRows.h — the LLM-profile rows of Agent Studio's profile pickers
// (AGENTS, CREATE, TEAMS, WORKFLOWS, PLANNER).
//
// Agents, teams, workflows and the planner run on the Python agent runtime,
// which cannot use an agent harness (provider "harness:<id>") yet, so
// LlmProfileRepository::list_profiles() leaves harness profiles out. Hiding
// them silently made a harness profile the user had just created look lost;
// here they are listed after a separator as DISABLED rows saying why. Such a
// row carries no profile id (Qt::UserRole is empty), so it can never be
// picked, restored by id or saved as an assignment, and the combo's keyboard
// and wheel navigation skip it (no Qt::ItemIsEnabled).
//
// Header-only (no CMake entry needed). GUI thread (repository reads).

#include "services/llm/ModelRouter.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QVector>

namespace fincept::screens::agent_profiles {

/// Label of a harness profile row in an agent-runtime picker.
inline QString harness_row_text(const QString& profile_name) {
    return QCoreApplication::translate("AgentProfileRows", "%1 — not available for tool-using agents yet")
        .arg(profile_name);
}

/// Tooltip of a harness row: why this feature cannot use it (the feature
/// table's own reason, ModelRouter.cpp feature_specs()) and where it does work.
inline QString harness_row_tooltip(const QString& feature_key) {
    QString why;
    if (const auto* spec = fincept::ai_chat::feature_spec(feature_key); spec && spec->not_routed_reason)
        why = QString::fromUtf8(spec->not_routed_reason);
    if (why.isEmpty())
        why = QCoreApplication::translate("AgentProfileRows",
                                          "Tool-using agents run on the Python agent runtime: not available for "
                                          "harnesses yet.");
    return why + QLatin1Char(' ') +
           QCoreApplication::translate("AgentProfileRows",
                                       "This harness profile answers AI Chat, Quick Chat and Quick Ask where you "
                                       "choose it (the model badge in those headers).");
}

/// Appends the profile rows to `combo` (after whatever the caller added, e.g.
/// "Default (Global)"): every profile the agent runtime can use as a normal
/// row (data = profile id; `default_marker` appended to the global default),
/// then — after a separator — every harness profile as a disabled row.
/// `feature_key` ("agent", "team", "workflow") picks the tooltip's reason.
/// Returns the usable profiles in row order.
inline QVector<LlmProfile> add_profile_rows(QComboBox* combo, const QString& default_marker,
                                            const QString& feature_key) {
    QVector<LlmProfile> usable;
    if (!combo)
        return usable;
    const auto r = LlmProfileRepository::instance().list_all_profiles();
    if (r.is_err())
        return usable;
    QVector<LlmProfile> harness;
    for (const LlmProfile& p : r.value())
        (p.is_harness() ? harness : usable).append(p);
    for (const LlmProfile& p : usable)
        combo->addItem(p.is_default ? p.name + default_marker : p.name, p.id);
    if (harness.isEmpty())
        return usable;

    combo->insertSeparator(combo->count());
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    const QString tip = harness_row_tooltip(feature_key);
    for (const LlmProfile& p : harness) {
        combo->addItem(harness_row_text(p.name), QString{}); // no id: never chosen, restored or saved
        const int row = combo->count() - 1;
        combo->setItemData(row, tip, Qt::ToolTipRole);
        combo->setItemData(row, QBrush(QColor(fincept::ui::colors::TEXT_DIM())), Qt::ForegroundRole);
        if (model) {
            if (QStandardItem* it = model->item(row))
                it->setFlags(it->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        }
    }
    return usable;
}

/// True when `row` of `combo` can be chosen (not a separator or harness row).
inline bool is_selectable_row(const QComboBox* combo, int row) {
    if (!combo || row < 0 || row >= combo->count())
        return false;
    const QModelIndex idx = combo->model()->index(row, combo->modelColumn(), combo->rootModelIndex());
    return (combo->model()->flags(idx) & Qt::ItemIsEnabled) != 0;
}

} // namespace fincept::screens::agent_profiles
