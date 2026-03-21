#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>

namespace fincept::screens {

/// Top toolbar for dashboard — terminal identity, clock, connection status, widget count, actions.
class DashboardToolBar : public QWidget {
    Q_OBJECT
public:
    explicit DashboardToolBar(QWidget* parent = nullptr);

    void set_widget_count(int count);
    void set_connected(bool connected);

signals:
    void add_widget_clicked();
    void save_layout_clicked();
    void reset_layout_clicked();
    void toggle_pulse_clicked();
    void toggle_compact_clicked();

private:
    void update_clock();

    QLabel* clock_label_      = nullptr;
    QLabel* status_dot_       = nullptr;
    QLabel* status_text_      = nullptr;
    QLabel* widget_count_     = nullptr;
    QPushButton* pulse_btn_   = nullptr;
    QPushButton* compact_btn_ = nullptr;
    QTimer  clock_timer_;
    bool    connected_ = true;
    bool    pulse_visible_ = true;
};

} // namespace fincept::screens
