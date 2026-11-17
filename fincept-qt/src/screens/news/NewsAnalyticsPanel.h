#pragma once
// NewsAnalyticsPanel — the News screen's analytics strip (toggled by CHARTS):
//   * timeline: articles per bucket (bars) and mean tone of the scored ones
//     (baseline area) for the current filter; zoom / drag picks the time window
//     the feed shows, double-click resets it,
//   * a price chart with news markers when the feed is filtered to one symbol,
//   * trending tickers / entities: mentions vs the previous window, tone, price
//     change, a mention sparkline,
//   * tone heatmap by category, region or sector.
// Everything shown is computed by the screen's filter worker
// (NewsAnalyticsSnapshot); the panel only subscribes to market quotes / price
// history for what it displays, and only while visible.

#include "services/news/NewsAnalytics.h"

#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

#include <limits>

class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableWidget;
class QTimer;

namespace fincept::ui {
class TimeSeriesChart;
class HeatmapGrid;
} // namespace fincept::ui

namespace fincept::screens {

/// What the analytics panel shows, computed off the GUI thread.
struct NewsAnalyticsSnapshot {
    qint64 from = 0;   ///< unix seconds — span of the time filter (not the selected window)
    qint64 to = 0;
    qint64 bucket = 3600;
    QVector<services::news_analytics::SeriesPoint> series; ///< volume + tone per bucket
    QVector<services::news_analytics::TrendRow> tickers;   ///< current vs previous half of the span
    QVector<services::news_analytics::TrendRow> categories; ///< whole span (current = count)
    QVector<services::news_analytics::TrendRow> regions;
    int total = 0;  ///< articles in the span
    int scored = 0; ///< of which tone-scored
    /// Active symbol filter (Yahoo form) and its articles (ts, headline, tone).
    QString symbol;
    struct Mark {
        qint64 ts = 0;
        QString headline;
        double tone = std::numeric_limits<double>::quiet_NaN();
    };
    QVector<Mark> symbol_news;
};

class NewsAnalyticsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit NewsAnalyticsPanel(QWidget* parent = nullptr);

    void set_snapshot(const NewsAnalyticsSnapshot& snap);
    /// Trending entities (people, organisations, places) from the NER pass.
    void set_entities(const QVector<services::news_analytics::TrendRow>& rows);
    /// Sector of each ticker (from market:info), for the sector heatmap.
    void set_sectors(const QHash<QString, QString>& sector_by_symbol);
    /// Show the selected window (from the screen's state) without re-emitting it.
    void set_window(qint64 from, qint64 to);

  signals:
    /// The user zoomed / dragged the timeline: show only [from, to] (unix seconds).
    void window_selected(qint64 from, qint64 to);
    /// Back to the whole time range.
    void window_cleared();
    void ticker_clicked(const QString& symbol);
    void category_clicked(const QString& category);
    void region_clicked(const QString& region);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;
    /// Trending table resized: re-fit its columns (fit_trend_columns).
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    /// The charts, table and heatmap are built on first show (CHARTS is off by
    /// default), so opening the News screen does not pay for a hidden panel;
    /// the setters only store state until then.
    void ensure_built();
    void build_ui();
    /// Every view from the stored snapshot (after build, and on each snapshot).
    void refresh_all();
    void retranslate();
    void rebuild_timeline();
    void rebuild_price_chart();
    void rebuild_trending();
    /// The trending table at its current width: the name column stretches and
    /// elides, the numbers keep their width, and the least important columns
    /// (price change, vs-previous, sparkline) give way on a narrow panel (the
    /// article detail open) instead of being cut off at the edge.
    void fit_trend_columns();
    void rebuild_heatmap();
    void subscribe_market();
    void on_quote(const QString& symbol, double change_pct);

    NewsAnalyticsSnapshot snap_;
    QVector<services::news_analytics::TrendRow> entities_;
    QHash<QString, QString> sectors_;
    QHash<QString, double> quote_change_; // symbol -> % change
    QSet<QString> subscribed_;            // quote / history topics held
    bool built_ = false;
    bool visible_ = false;
    bool applying_range_ = false;
    qint64 window_from_ = 0;
    qint64 window_to_ = 0;
    QString heat_mode_ = QStringLiteral("CATEGORY");
    QString trend_mode_ = QStringLiteral("TICKERS");

    QLabel* title_ = nullptr;
    QLabel* window_label_ = nullptr;
    QPushButton* window_clear_ = nullptr;
    QLabel* scored_label_ = nullptr;
    QVector<QPushButton*> heat_btns_;
    QVector<QPushButton*> trend_btns_;
    QSplitter* body_ = nullptr;
    ui::TimeSeriesChart* price_chart_ = nullptr;
    ui::TimeSeriesChart* timeline_ = nullptr;
    QTableWidget* trend_table_ = nullptr;
    QLabel* trend_empty_ = nullptr;
    ui::HeatmapGrid* heatmap_ = nullptr;
    QTimer* range_debounce_ = nullptr;
    double pending_from_ms_ = 0;
    double pending_to_ms_ = 0;
};

} // namespace fincept::screens
