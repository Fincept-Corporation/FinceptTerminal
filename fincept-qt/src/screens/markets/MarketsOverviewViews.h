#pragma once
// MarketsOverviewViews — the two overview pages of the Markets screen (spec 2.3 e).
// Both read MarketsQuoteStore only: zero additional requests.
//   MarketsHeatmapView  equal-size tiles of every panel symbol, grouped by panel,
//                       coloured by today's change (+/-3 % = full colour).
//   MarketsMoversView   top gainers / top losers / most extended among the panel
//                       symbols (honestly labelled: not the whole market).

#include "screens/markets/MarketPanelConfig.h"

#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QStackedWidget;
class QTimer;
class QEvent;

namespace fincept::ui {
class EmptyState;
class Treemap;
}

namespace fincept::screens {

class MarketsQuoteStore;
class MarketPanelModel;
class MarketPanelProxy;
class MarketPanelView;

class MarketsHeatmapView : public QWidget {
    Q_OBJECT
  public:
    MarketsHeatmapView(MarketsQuoteStore* store, QWidget* parent = nullptr);
    void set_panels(const QVector<MarketPanelConfig>& panels);
    void refresh_theme();

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_activated(const QString& symbol);
    void panel_requested(const QString& panel_id);

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void schedule_rebuild();
    void rebuild();
    void retranslate();

    QPointer<MarketsQuoteStore> store_;
    QVector<MarketPanelConfig> panels_;
    ui::Treemap* map_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* note_ = nullptr;
    QTimer* rebuild_timer_ = nullptr;
};

class MarketsMoversView : public QWidget {
    Q_OBJECT
  public:
    MarketsMoversView(MarketsQuoteStore* store, QWidget* parent = nullptr);
    void set_panels(const QVector<MarketPanelConfig>& panels);
    void refresh_theme();
    void refresh_metrics();
    /// Rows on screen (+ margin) of the three lists, for sparklines.
    QStringList visible_symbols(int margin) const;
    /// Recompute the three lists from the store (cycle finished / LIVE every 5 s).
    void recompute();

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_activated(const QString& symbol);
    void panel_requested(const QString& panel_id);
    void viewport_rows_changed();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    struct List {
        QLabel* title = nullptr;
        MarketPanelModel* model = nullptr;
        MarketPanelProxy* proxy = nullptr;
        MarketPanelView* view = nullptr;
        QStackedWidget* stack = nullptr; // the list, or the shared empty state
        ui::EmptyState* empty = nullptr;
    };
    void retranslate();

    QPointer<MarketsQuoteStore> store_;
    QVector<MarketPanelConfig> panels_;
    QVector<List> lists_;
    QLabel* caption_ = nullptr;
};

} // namespace fincept::screens
