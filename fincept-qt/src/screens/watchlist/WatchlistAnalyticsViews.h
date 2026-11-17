#pragma once
// WatchlistAnalyticsViews — the Watchlist screen's alternative views of a list:
//   * WatchlistMapView: market map — a treemap sized by market cap (or an
//     equal-size heat grid), coloured by a selectable horizon (1D .. 1Y);
//   * WatchlistCorrelationView: correlation matrix of the members' daily log
//     returns over a selectable window, optionally clustered.
// Both are fed by the screen from the model / shared history topics.

#include "services/markets/MarketDataTypes.h"
#include "ui/charts/Treemap.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QStackedWidget;

namespace fincept::ui {
class HeatmapGrid;
class MatrixHeatmap;
} // namespace fincept::ui

namespace fincept::screens {

/// Pearson correlations of daily log returns. `series` maps symbol -> daily
/// bars; only bars within `window_days` calendar days of the latest bar of all
/// series are used; returns are paired by (UTC) bar date. A pair with fewer than
/// `min_overlap` common return dates is NaN. Diagonal = 1 where the symbol has data.
QVector<QVector<double>> watchlist_correlation(const QStringList& symbols,
                                               const QHash<QString, QVector<services::HistoryPoint>>& series,
                                               int window_days, int min_overlap = 15);

class WatchlistMapView : public QWidget {
    Q_OBJECT
  public:
    explicit WatchlistMapView(QWidget* parent = nullptr);

    /// "cap" | "equal" and "1d" | "1w" | "1m" | "3m" | "ytd" | "1y".
    void set_mode(const QString& size, const QString& horizon);
    QString size_mode() const;
    QString horizon() const;
    /// Items: id = symbol, value = percent change over horizon() (NaN = none),
    /// weight = market cap (USD where convertible) or 1. `grouped` shows group headers.
    void set_items(const QVector<ui::TreemapItem>& items, bool grouped, const QString& note);
    void set_loading(const QString& message);
    void refresh_theme();
    void retranslate();

    /// Percent value range for full colour at a horizon.
    static double clamp_for(const QString& horizon);

  signals:
    void mode_changed(const QString& size, const QString& horizon);
    void symbol_clicked(const QString& symbol);
    void symbol_activated(const QString& symbol);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    QComboBox* size_box_ = nullptr;
    QComboBox* horizon_box_ = nullptr;
    QLabel* size_label_ = nullptr;
    QLabel* horizon_label_ = nullptr;
    QLabel* note_ = nullptr;
    QWidget* bar_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    ui::Treemap* treemap_ = nullptr;
    ui::HeatmapGrid* grid_ = nullptr;
};

class WatchlistCorrelationView : public QWidget {
    Q_OBJECT
  public:
    explicit WatchlistCorrelationView(QWidget* parent = nullptr);

    /// Recompute from the members' daily series. `pending` = symbols whose
    /// history is still loading; `failed` = without history.
    void set_data(const QStringList& symbols, const QHash<QString, QVector<services::HistoryPoint>>& series,
                  int pending, const QStringList& failed);
    int window_days() const;
    void refresh_theme();
    void retranslate();

  signals:
    void symbol_pair_clicked(const QString& a, const QString& b);
    void window_changed();

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void recompute();

    QStringList symbols_;
    QHash<QString, QVector<services::HistoryPoint>> series_;
    int pending_ = 0;
    QStringList failed_;
    QStringList shown_;

    QWidget* bar_ = nullptr;
    QLabel* window_label_ = nullptr;
    QComboBox* window_box_ = nullptr;
    QCheckBox* cluster_box_ = nullptr;
    QLabel* status_ = nullptr;
    ui::MatrixHeatmap* matrix_ = nullptr;
};

} // namespace fincept::screens
