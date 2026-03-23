#pragma once
#include <QWidget>

namespace fincept::screens {

/// About & legal information — version, license, contact, resources.
class AboutScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AboutScreen(QWidget* parent = nullptr);
};

} // namespace fincept::screens
