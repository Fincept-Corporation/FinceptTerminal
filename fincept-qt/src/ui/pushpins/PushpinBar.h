#pragma once
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QScrollArea;

namespace fincept::ui {

class SymbolChip;

/// Thin horizontal strip of symbol chips shown at the top of every
/// MainWindow. Rebuilt from PushpinService on every pins_changed signal.
/// Accepts drops of symbol MIME data (drag a symbol from any panel onto
/// the bar to pin it).
///
/// Empty state shows a muted hint label ("Drag any symbol here to pin")
/// so first-time users discover the feature without a tutorial.
class PushpinBar : public QWidget {
    Q_OBJECT
  public:
    explicit PushpinBar(QWidget* parent = nullptr);

  private:
    void rebuild();

    QScrollArea* scroll_ = nullptr;
    QWidget* strip_ = nullptr;
    QHBoxLayout* strip_layout_ = nullptr;
    QLabel* empty_hint_ = nullptr;
};

} // namespace fincept::ui
