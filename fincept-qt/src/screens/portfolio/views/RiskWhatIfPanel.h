// src/screens/portfolio/views/RiskWhatIfPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAnalyticsService.h"

#include <QJsonObject>
#include <QWidget>

class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace fincept::ui {
class BarChart;
class EmptyState;
class LoadingOverlay;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// What-if trade: edit target weights (or add a symbol), then see the ex-ante
/// risk before → after (volatility, beta, VaR / CVaR, tracking error,
/// diversification ratio) and the per-holding risk contribution, computed by
/// PortfolioService::what_if on trailing daily returns in the base currency.
/// Nothing is traded or recorded.
///
/// Never blank: the NOW column and the "Risk share now" bars come at once from
/// the screen's market-analytics run (set_analytics), TARGET / CHANGE read "—"
/// until the user RUNs (a run with target = current would only repeat NOW, so
/// nothing runs by itself). A RUN dims the result pane, can be superseded, is
/// cancelled when the tab is left, and a failure keeps NOW with a RETRY notice.
class RiskWhatIfPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RiskWhatIfPanel(QWidget* parent = nullptr);
    ~RiskWhatIfPanel() override;

    void set_data(const portfolio::PortfolioSummary& summary);
    /// Ex-ante risk of the book as it is now (the screen's market-analytics run).
    void set_analytics(const portfolio::MarketAnalytics& analytics);
    /// Confidence / horizon used for the VaR rows.
    void set_options(const portfolio::RiskOptions& opts) { opts_ = opts; }

  signals:
    /// The empty-book state's BUY action.
    void add_position_requested();

  protected:
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void reset_weights();
    void add_symbol();
    void normalise();
    void run();
    /// Stop the run in flight (its late reply is dropped by request id).
    void cancel_run();
    void update_total();
    /// NOW from the analytics, or the last what-if result when there is one.
    void render();
    void render_now();
    void render_result();
    void recolor_targets();
    QString targets_signature() const;

    QStackedWidget* pages_ = nullptr; ///< 0 = editor + results, 1 = empty book
    ui::EmptyState* empty_ = nullptr;
    QTableWidget* weights_ = nullptr;
    QLineEdit* add_edit_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* reset_btn_ = nullptr;
    QPushButton* norm_btn_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QLabel* total_label_ = nullptr;
    QWidget* right_ = nullptr;
    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QLabel* status_ = nullptr;
    QTableWidget* results_ = nullptr;
    ui::BarChart* contrib_chart_ = nullptr;
    ui::LoadingOverlay* overlay_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::MarketAnalytics analytics_;
    portfolio::RiskOptions opts_;
    QString loaded_for_;
    QJsonObject result_;      ///< last what-if answer for loaded_for_ (empty = none yet)
    QString result_targets_;  ///< targets_signature() the result was computed for
    QString run_error_;       ///< last RUN failure ("" = none)
    bool run_stopped_ = false; ///< the last RUN was cancelled before it answered (offers RETRY)
    services::AnalyticsRequest req_; ///< handle of the RUN in flight (its cancel token)
    quint64 run_seq_ = 0;            ///< RUN in flight (0 = none); a reply for another one is dropped
    quint64 next_seq_ = 0;
};

} // namespace fincept::screens
