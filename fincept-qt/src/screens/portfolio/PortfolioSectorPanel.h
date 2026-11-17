// src/screens/portfolio/PortfolioSectorPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioAllocation.h"

#include <QHash>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QPushButton;

namespace fincept::ui {
class DonutChart;
class MatrixHeatmap;
} // namespace fincept::ui

namespace fincept::screens {

/// Main-view allocation panel: a donut by sector / asset class / region / currency
/// (weights of the valued holdings, base currency) and the clustered correlation
/// matrix of every holding. Clicking a sector slice filters the POSITIONS table.
class PortfolioSectorPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioSectorPanel(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_currency(const QString& currency);
    /// Allocation report for the non-sector dimensions.
    void set_allocation(const portfolio::AllocationReport& report);
    /// Pairwise Pearson correlation, key = "SYM1|SYM2". A delivered matrix ends
    /// the "pending" state and clears a previous failure.
    void set_correlation(const QHash<QString, double>& matrix);
    /// A market-analytics run is in flight for the book: with no matrix on screen
    /// the correlation shows Loading - only then (never "loads..." for a book that
    /// has nothing to download).
    void set_correlation_pending(bool pending);
    /// The last analytics run failed and no matrix is on screen: Error state with
    /// the plain reason and RETRY. Empty `message` clears it.
    void set_correlation_error(const QString& message);
    /// What RETRY does (the screen re-requests the analytics).
    void set_correlation_retry(std::function<void()> retry) { corr_retry_ = std::move(retry); }
    /// Make `sector` the active filter (empty = clear) and emit sector_selected.
    void select_sector(const QString& sector);

    static QColor sector_color(int index);

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    /// A sector slice was clicked. Empty string = clear filter.
    void sector_selected(QString sector);

  private:
    void build_ui();
    void update_donut();
    void update_correlation();
    void apply_styles();
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QComboBox* dim_cb_ = nullptr;
    ui::DonutChart* donut_ = nullptr;
    QLabel* corr_title_ = nullptr;
    ui::MatrixHeatmap* corr_ = nullptr;
    QLabel* corr_note_ = nullptr;
    QPushButton* corr_retry_btn_ = nullptr;

    QVector<portfolio::HoldingWithQuote> holdings_;
    portfolio::AllocationReport report_;
    bool have_report_ = false;
    QString currency_; ///< portfolio base currency; empty (no unit printed) until set_currency()
    QString selected_sector_;
    QHash<QString, double> corr_matrix_;
    QString corr_signature_;
    bool corr_pending_ = false;
    QString corr_error_;
    std::function<void()> corr_retry_;
};

} // namespace fincept::screens
