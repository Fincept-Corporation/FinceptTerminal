#pragma once
#include "screens/dashboard/canvas/GridLayout.h"

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

} // namespace fincept::screens
