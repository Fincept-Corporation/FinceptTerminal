#pragma once
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class QFrame;

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
    QWidget* build_stat_row(const QString& label, const QString& value, const QString& change, const QString& color);
    QWidget* build_breadth_bar(const QString& label, int advancing, int declining);

    /// Re-apply all token-based styles so a theme switch updates every child widget.
    void refresh_theme();

    static QString market_status(const QString& region);

    // ── Header ──
    QWidget* header_bar_ = nullptr;
    QLabel* header_icon_ = nullptr;
    QLabel* header_title_ = nullptr;
    QLabel* header_live_dot_ = nullptr;

    // ── Scroll area ──
    QScrollArea* scroll_area_ = nullptr;

    // ── Section headers (5 total) ──
    struct SectionHeader {
        QWidget* container = nullptr;
        QLabel* icon = nullptr;
        QLabel* title = nullptr;
    };
    SectionHeader sh_breadth_;
    SectionHeader sh_gainers_;
    SectionHeader sh_losers_;
    SectionHeader sh_snapshot_;
    SectionHeader sh_hours_;

    // ── Fear & Greed ──
    QLabel* fg_header_label_ = nullptr;
    QLabel* fg_gauge_icon_ = nullptr;
    QLabel* fg_score_val_ = nullptr;
    QLabel* fg_score_max_ = nullptr;
    QLabel* fg_sentiment_ = nullptr;
    QFrame* fg_gradient_bar_ = nullptr;

    // ── Market Breadth ──
    // per-exchange: {name_label, adv_label, slash_label, dec_label, green_bar, red_bar}
    struct BreadthRow {
        QLabel* name = nullptr;
        QLabel* adv = nullptr;
        QLabel* slash = nullptr;
        QLabel* dec = nullptr;
        QWidget* green = nullptr;
        QWidget* red = nullptr;
    };
    BreadthRow nyse_row_;
    BreadthRow nasdaq_row_;
    BreadthRow sp500_row_;

    // ── Top Movers ──
    QVBoxLayout* gainers_layout_ = nullptr;
    QVBoxLayout* losers_layout_ = nullptr;

    // ── Global Snapshot ──
    struct StatRow {
        QWidget* container = nullptr;
        QLabel* name_lbl = nullptr;
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
        QWidget* container = nullptr;
        QLabel* name_lbl = nullptr;
        QLabel* dot = nullptr;
        QLabel* status = nullptr;
        QString region;
    };
    QVector<HoursRow> hours_rows_;

    QTimer* refresh_timer_ = nullptr;
    QTimer* hours_timer_ = nullptr;
};

} // namespace fincept::screens
