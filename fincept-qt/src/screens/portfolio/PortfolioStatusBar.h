// src/screens/portfolio/PortfolioStatusBar.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QTimer>
#include <QWidget>

#include <optional>

namespace fincept::screens {

class PortfolioStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioStatusBar(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_portfolio_name(const QString& name);

    void start_clock();
    void stop_clock();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void update_clock();
    void retranslateUi();

    QLabel* brand_label_ = nullptr;
    QLabel* version_label_ = nullptr;
    QLabel* portfolio_label_ = nullptr;
    QLabel* live_label_ = nullptr;
    QLabel* positions_label_ = nullptr;
    QLabel* nav_label_ = nullptr;
    QLabel* pnl_label_ = nullptr;
    QLabel* time_label_ = nullptr;
    QLabel* tz_label_ = nullptr;

    QTimer* clock_timer_ = nullptr;

    std::optional<portfolio::PortfolioSummary> last_summary_;
};

} // namespace fincept::screens
