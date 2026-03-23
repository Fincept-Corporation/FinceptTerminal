#pragma once
#include <QWidget>

namespace fincept::screens {

/// Semi-transparent orange drop-target overlay shown during drag/resize.
/// Matches react-grid-layout's orange placeholder.
class PlaceholderOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit PlaceholderOverlay(QWidget* parent = nullptr);

  protected:
    void paintEvent(QPaintEvent* e) override;
};

} // namespace fincept::screens
