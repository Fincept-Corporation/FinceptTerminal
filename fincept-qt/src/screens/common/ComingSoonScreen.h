#pragma once
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

/// Placeholder screen for tabs not yet implemented.
class ComingSoonScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ComingSoonScreen(const QString& tab_name, QWidget* parent = nullptr);
};

} // namespace fincept::screens
