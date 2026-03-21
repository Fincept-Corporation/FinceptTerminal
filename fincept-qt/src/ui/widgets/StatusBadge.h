#pragma once
#include <QLabel>

namespace fincept::ui {

/// Small colored status label (e.g., "CONNECTED" in green, "OFFLINE" in red).
class StatusBadge : public QLabel {
    Q_OBJECT
public:
    enum Status { Connected, Disconnected, Loading, Idle };

    explicit StatusBadge(QWidget* parent = nullptr);
    void set_status(Status s);
    void set_custom(const QString& text, const QString& color);
};

} // namespace fincept::ui
