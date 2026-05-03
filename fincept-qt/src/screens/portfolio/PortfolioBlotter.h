// src/screens/portfolio/PortfolioBlotter.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class PortfolioSparkline;

/// 11-column sortable positions table — blotter style.
class PortfolioBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioBlotter(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_selected_symbol(const QString& symbol);
    void set_filter(const QString& text);
    /// Show only rows whose symbol is in @p symbols. Empty list = show all.
    void set_sector_filter(const QStringList& symbols);
    void refresh_theme();

  signals:
    void symbol_selected(QString symbol);
    void sort_changed(portfolio::SortColumn col, portfolio::SortDirection dir);
    void edit_transaction_requested(QString symbol);
    void delete_position_requested(QString symbol);

  private:
    void build_ui();
    void build_pagination_footer();
    void populate_table();
    void apply_filter();
    void on_header_clicked(int section);
    void on_row_clicked(int row, int col);
    void on_context_menu(const QPoint& pos);
    QString format_value(double v, int dp = 2) const;

    // ── Pagination ──────────────────────────────────────────────────────────
    /// Filtered + sorted view (after filter/sector trim) used as the source
    /// of truth for pagination math. Recomputed every populate_table().
    QVector<portfolio::HoldingWithQuote> visible_view() const;
    /// Rows for the current page only — what actually goes into table_.
    QVector<portfolio::HoldingWithQuote> paged_view() const;
    /// Update footer labels + button enabled-states from current state.
    void update_pagination_controls();
    /// Clamp current_page_ to a valid range given the latest filtered count.
    void clamp_current_page();
    /// Total pages for current filtered view + page size.
    int total_pages() const;

    QTableWidget* table_ = nullptr;

    // Pagination footer widgets
    QWidget* footer_ = nullptr;
    QLabel* footer_status_ = nullptr;        // "Showing 1-10 of 47"
    QPushButton* btn_first_ = nullptr;
    QPushButton* btn_prev_ = nullptr;
    QLabel* footer_page_label_ = nullptr;    // "Page 1 of 5"
    QPushButton* btn_next_ = nullptr;
    QPushButton* btn_last_ = nullptr;
    QComboBox* page_size_combo_ = nullptr;

    void fetch_sparklines();

    void hub_resubscribe_sparklines();
    void hub_unsubscribe_all();
    void repaint_sparkline_cells();
    bool hub_active_ = false;

    QVector<portfolio::HoldingWithQuote> holdings_;
    QVector<portfolio::HoldingWithQuote> sorted_;
    QString selected_symbol_;
    QString filter_text_;
    QStringList sector_symbols_; // empty = no sector filter
    portfolio::SortColumn sort_col_ = portfolio::SortColumn::Weight;
    portfolio::SortDirection sort_dir_ = portfolio::SortDirection::Desc;

    // Pagination state. page_size_ persists via SettingsRepository
    // ("portfolio.blotter.page_size"). current_page_ is 1-indexed.
    int page_size_ = 10;
    int current_page_ = 1;

    // Real sparkline data keyed by symbol — populated async via fetch_sparklines()
    QHash<QString, QVector<double>> sparkline_cache_;

    // Sparkline fetch state per symbol
    enum class SparklineState { Pending, Loaded, Failed };
    QHash<QString, SparklineState> sparkline_state_;
};

} // namespace fincept::screens
