#pragma once
// SettingsRowHelpers.h — shared row/separator builders for settings sections.

#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens::settings_helpers {

inline QWidget* make_row(const QString& label, QWidget* control, const QString& description = {}) {
    auto* row = new QWidget(nullptr);
    row->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(row);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(3);

    auto* hl = new QHBoxLayout;
    hl->setContentsMargins(0, 4, 0, 4);
    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(settings_styles::label_ss());
    hl->addWidget(lbl);
    hl->addStretch();
    hl->addWidget(control);
    vl->addLayout(hl);

    if (!description.isEmpty()) {
        auto* desc = new QLabel(description);
        desc->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        desc->setWordWrap(true);
        vl->addWidget(desc);
    }
    return row;
}

inline QFrame* make_sep() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

} // namespace fincept::screens::settings_helpers
