#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace fincept::screens {

class PhaseOneBootstrapScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PhaseOneBootstrapScreen(QWidget* parent = nullptr);

  signals:
    void bootstrap_succeeded();
    void navigate_login();

  private:
    void on_submit();
    void show_error(const QString& message);

    QLineEdit* username_input_ = nullptr;
    QLineEdit* password_input_ = nullptr;
    QLabel* error_label_ = nullptr;
    QPushButton* submit_button_ = nullptr;
};

} // namespace fincept::screens
