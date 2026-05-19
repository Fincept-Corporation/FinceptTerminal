#pragma once
#include "screens/dashboard/canvas/GridLayout.h"

#include <QCoreApplication>
#include <QString>
#include <QVector>

namespace fincept::screens {

struct DashboardTemplate {
    QString id;
    QString display_name;
    QString description;
    QVector<GridItem> items; // instance_id is empty — filled on apply
};

/// Returns all 6 built-in dashboard templates (12-column grid).
QVector<DashboardTemplate> all_dashboard_templates();

/// Translated display_name / description for a template. Storage stays
/// English; QCoreApplication::translate resolves under the
/// "fincept::screens::DashboardTemplates" context populated by lupdate.
QString template_display_name_tr(const DashboardTemplate& t);
QString template_description_tr(const DashboardTemplate& t);

} // namespace fincept::screens
