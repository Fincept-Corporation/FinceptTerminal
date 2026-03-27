#pragma once
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Right-side market pulse panel — Fear/Greed, breadth, top movers, global snapshot, market hours.
/// All data is live via MarketDataService. Refreshes every 60s when visible.
class MarketPulsePanel : public QWidget {
    Q_OBJECT
  public:
    explicit MarketPulsePanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void refresh_data();
    void refresh_market_hours();

  private:
    // ── Build helpers ──
    QWidget* build_header();
    QWidget* build_section_header(const QString& title, const QString& icon_char, const QString& color);
    QWidget* build_fear_greed_section();
    QWidget* build_breadth_section();
    QWidget* build_gainers_section();
    QWidget* build_losers_section();
    QWidget* build_global_snapshot_section();
    QWidget* build_market_hours_section();
    QWidget* build_mover_row(const QString& symbol, double change, const QString& volume);
    QWidget* build_stat_row(const QString& label, const QString& value, const QString& change,
                            const QString& color);
    QWidget* build_breadth_bar(const QString& label, int advancing, int declining);

    static QString market_status(const QString& region);

    // ── Fear & Greed ──
    QLabel* fg_score_val_   = nullptr;
    QLabel* fg_score_max_   = nullptr;
    QLabel* fg_sentiment_   = nullptr;
    QFrame* fg_gradient_bar_= nullptr;

    // ── Market Breadth ──
    // per-exchange: {adv_label, dec_label, green_bar, red_bar}
    struct BreadthRow {
        QLabel* adv   = nullptr;
        QLabel* dec   = nullptr;
        QWidget* green= nullptr;
        QWidget* red  = nullptr;
    };
    BreadthRow nyse_row_;
    BreadthRow nasdaq_row_;
    BreadthRow sp500_row_;

    // ── Top Movers ──
    QVBoxLayout* gainers_layout_ = nullptr;
    QVBoxLayout* losers_layout_  = nullptr;

    // ── Global Snapshot ──
    struct StatRow {
        QLabel* val = nullptr;
        QLabel* chg = nullptr;
    };
    StatRow vix_row_;
    StatRow us10y_row_;
    StatRow dxy_row_;
    StatRow gold_row_;
    StatRow oil_row_;
    StatRow btc_row_;

    // ── Market Hours ──
    struct HoursRow {
        QLabel* dot    = nullptr;
        QLabel* status = nullptr;
        QString region;
    };
    QVector<HoursRow> hours_rows_;

    QTimer* refresh_timer_       = nullptr;
    QTimer* hours_timer_         = nullptr;
};

} // namespace fincept::screens
