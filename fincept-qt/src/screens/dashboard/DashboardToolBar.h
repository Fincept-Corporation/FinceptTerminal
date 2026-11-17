#pragma once
#include <QComboBox>
#include <QDateTime>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens {

/// Top toolbar for dashboard — terminal identity, clock, connection status, widget count, actions.
class DashboardToolBar : public QWidget {
    Q_OBJECT
  public:
    explicit DashboardToolBar(QWidget* parent = nullptr);

    void set_widget_count(int count);
    void set_connected(bool connected);
    /// Show the layout's density on the COMPACT button (checked = compact).
    void set_compact(bool compact);

  signals:
    void add_widget_clicked();
    void save_layout_clicked();
    void reset_layout_clicked();
    void refresh_clicked();
    void toggle_pulse_clicked();
    void toggle_compact_clicked();
    /// LAYOUTS button: the screen shows its named-layout / export / import menu below `anchor`.
    void layouts_menu_requested(QWidget* anchor);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    void update_clock();
    void refresh_theme();
    void retranslateUi();
    /// Sets accessible names on every control and wires an explicit tab order.
    void apply_accessibility();
    /// Fill the region combo from MarketProfile and select the current region.
    void fill_regions();
    /// Hide the left-hand identity / clock / count groups, least important
    /// first, until the bar fits beside its action buttons.
    void fit_left_items();
    QVector<QVector<QWidget*>> fold_groups_;

    QWidget* left_container_ = nullptr;
    QWidget* right_container_ = nullptr;
    QPushButton* clock_btn_ = nullptr;
    bool clock_is_utc_ = true;
    bool connected_ = true;
    bool compact_ = true;
    int widget_count_value_ = 0;
    QLabel* status_text_ = nullptr;
    QLabel* widget_count_ = nullptr;
    QPushButton* pulse_btn_ = nullptr;
    QPushButton* compact_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QPushButton* layouts_btn_ = nullptr;
    QComboBox* region_combo_ = nullptr;
    bool filling_regions_ = false;
    QTimer clock_timer_;
};

} // namespace fincept::screens
