#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QPixmap>
#include <QWidget>

namespace fincept::ui {

/// Geometric crosshatch background — diagonal lines forming X pattern.
/// Caches rendering as QPixmap — redraws only on resize or theme change.
class GeometricBackground : public QWidget {
    Q_OBJECT
  public:
    explicit GeometricBackground(QWidget* parent = nullptr);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void rebuild_cache();
    QPixmap     cache_;
    ThemeTokens tokens_{};
};

} // namespace fincept::ui
