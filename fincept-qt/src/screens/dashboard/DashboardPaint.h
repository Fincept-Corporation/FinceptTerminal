#pragma once
// DashboardPaint.h — the opaque window background the dashboard's bars paint behind
// themselves (status bar, toolbar). One definition: unity builds merge the
// directory's .cpp files, and each bar used to carry an identical file-local copy.

#include <QColor>
#include <QPalette>
#include <QWidget>

namespace fincept::screens::dashboard_paint {

/// Styled, auto-filled background of `color` (the palette's Window role). Null-safe.
inline void apply_solid_window_background(QWidget* widget, const QColor& color) {
    if (!widget)
        return;

    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setAutoFillBackground(true);

    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, color);
    widget->setPalette(pal);
}

} // namespace fincept::screens::dashboard_paint
