#pragma once
#include <QWidget>
#include <QStackedWidget>

namespace fincept::screens {

/// Settings screen — Appearance, Notifications, Storage, Credentials.
class SettingsScreen : public QWidget {
    Q_OBJECT
public:
    explicit SettingsScreen(QWidget* parent = nullptr);

private:
    QStackedWidget* sections_ = nullptr;

    QWidget* build_appearance();
    QWidget* build_notifications();
    QWidget* build_storage();
};

} // namespace fincept::screens
