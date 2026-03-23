// src/ui/widgets/LoadingOverlay.h
#pragma once
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

/// Semi-transparent overlay with an animated spinner.
/// Usage:
///   1. Create as child of the content widget: new LoadingOverlay(content_widget)
///   2. Call show_loading("message") to display, hide_loading() to dismiss.
///   3. Call resize_to_parent() in the parent's resizeEvent to keep it full-size.
class LoadingOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void show_loading(const QString& message = "LOADING…");
    void hide_loading();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

  private:
    QLabel* spinner_label_  = nullptr;
    QLabel* message_label_  = nullptr;
    QTimer* spin_timer_     = nullptr;
    int     spin_angle_     = 0;

    static constexpr int SPIN_CHARS_COUNT = 8;
    static constexpr const char* SPIN_CHARS[8] = {
        "◜", "◠", "◝", "◞", "◡", "◟", "◜", "◠"
    };
    int spin_idx_ = 0;
};

} // namespace fincept::ui
