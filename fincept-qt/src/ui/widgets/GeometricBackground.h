#pragma once
#include <QWidget>
#include <QPixmap>

namespace fincept::ui {

/// Geometric crosshatch background — diagonal lines forming X pattern.
/// Matches the Tauri BackgroundPattern.tsx design.
/// Caches rendering as QPixmap — redraws only on resize.
class GeometricBackground : public QWidget {
    Q_OBJECT
public:
    explicit GeometricBackground(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild_cache();
    QPixmap cache_;
};

} // namespace fincept::ui
