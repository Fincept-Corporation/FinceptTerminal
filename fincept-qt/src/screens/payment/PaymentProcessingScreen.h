#pragma once
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Polls the payment gateway for order confirmation after checkout URL is opened.
class PaymentProcessingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PaymentProcessingScreen(QWidget* parent = nullptr);

    /// Start polling for the given order/transaction ID.
    void start_polling(const QString& order_id, const QString& plan_name);
    void stop_polling();

  signals:
    void payment_completed();
    void navigate_back();

  private:
    void build_ui();
    void poll_status();
    void set_status(const QString& status, const QString& color);

    QLabel* title_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* detail_label_ = nullptr;
    QLabel* elapsed_label_ = nullptr;
    QLabel* check_count_label_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    QTimer* poll_timer_ = nullptr;
    QString order_id_;
    QString plan_name_;
    int check_count_ = 0;
    int elapsed_seconds_ = 0;
    static constexpr int MAX_CHECKS = 60;            // 5 minutes at 5s intervals
    static constexpr int POLL_INTERVAL_MS = 5000;     // 5 seconds
    static constexpr int INITIAL_DELAY_MS = 3000;     // 3 seconds before first check
};

} // namespace fincept::screens
