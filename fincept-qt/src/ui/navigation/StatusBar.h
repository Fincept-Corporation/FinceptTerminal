#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept::ui {

/// Bottom status bar — version, feed indicators, ready status.
class StatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit StatusBar(QWidget* parent = nullptr);
    void set_ready(bool ready);

  private:
    QLabel* ready_label_ = nullptr;
};

} // namespace fincept::ui
