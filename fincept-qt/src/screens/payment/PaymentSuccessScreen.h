#pragma once
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Shown after successful payment — confirmation, plan details, auto-redirect.
class PaymentSuccessScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PaymentSuccessScreen(QWidget* parent = nullptr);

    /// Show success details for a completed payment.
    void show_success(const QString& plan_name, double amount, int days);

  signals:
    void navigate_dashboard();

  private:
    void build_ui();

    QLabel* title_label_ = nullptr;
    QLabel* plan_label_ = nullptr;
    QLabel* amount_label_ = nullptr;
    QLabel* validity_label_ = nullptr;
    QLabel* countdown_label_ = nullptr;
    QPushButton* dashboard_btn_ = nullptr;

    QTimer* redirect_timer_ = nullptr;
    int countdown_ = 5;
};

} // namespace fincept::screens
