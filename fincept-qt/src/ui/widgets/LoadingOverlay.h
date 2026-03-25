// src/ui/widgets/LoadingOverlay.h
#pragma once
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

/// Bloomberg-style loading overlay with pulsing bar animation.
/// Usage:
///   1. Create as child of the content widget: new LoadingOverlay(content_widget)
///   2. Call show_loading("message") to display, hide_loading() to dismiss.
class LoadingOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void show_loading(const QString& message = "LOADING\xe2\x80\xa6");
    void hide_loading();

  protected:
    void paintEvent(QPaintEvent* e) override;

  private:
    QTimer* anim_timer_ = nullptr;
    QString message_;
    int tick_ = 0;

    static constexpr int BAR_COUNT = 5;
    static constexpr int ANIM_INTERVAL_MS = 100;
};

} // namespace fincept::ui
