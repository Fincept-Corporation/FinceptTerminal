#pragma once
// VersionLabel.h — the "Fincept Terminal vX.Y.Z" tag in the top-left corner of
// the screens a user meets before the terminal opens: sign-in, sign-up,
// password reset, the PIN lock and first-time setup. It is what support asks
// for first, and these screens are exactly where a user is stuck when they ask.
//
// The text comes from QCoreApplication::applicationVersion(), which main.cpp
// sets from CMake's project(FinceptTerminal VERSION …), so it cannot drift from
// the binary. Styled by the global #dimLabel rule (ThemeManager), not inline.
// Header-only on purpose: no moc and no source-list change.

#include <QCoreApplication>
#include <QLabel>
#include <QString>

namespace fincept::ui {

inline QLabel* make_version_label(QWidget* parent) {
    const QString version = QCoreApplication::applicationVersion();
    auto* label = new QLabel(version.isEmpty() ? QStringLiteral("Fincept Terminal")
                                               : QStringLiteral("Fincept Terminal v%1").arg(version),
                             parent);
    label->setObjectName(QStringLiteral("dimLabel"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse); // copyable into a support request
    return label;
}

} // namespace fincept::ui
