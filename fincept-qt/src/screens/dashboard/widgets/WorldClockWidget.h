#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QStringList>

class QTimer;

namespace fincept::screens::widgets {

class SessionTimeline;

/// World market clock (type id "world_clock").
///
/// One row per exchange (the user's MarketProfile venues first, then the
/// world's main venues, or the tile's own list): local time, today's sessions
/// on a 24-hour track of the user's day (pre-market, regular session with the
/// lunch break, post-market) with a "now" line, the session state and the time
/// to the next open / close — all from core/market/MarketSessions, holidays
/// and early closes included where the calendar covers them.
/// Config: {"venues": ["NYSE", "LSE", ...]} (empty = region defaults).
class WorldClockWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit WorldClockWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void on_theme_changed() override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    QStringList effective_venues() const;
    void update_rows();

    SessionTimeline* timeline_ = nullptr;
    QTimer* tick_ = nullptr;
    QStringList venues_; ///< user's list; empty = region defaults
};

/// Painted session timeline used by WorldClockWidget (theme read at paint time).
class SessionTimeline : public QWidget {
    Q_OBJECT
  public:
    explicit SessionTimeline(QWidget* parent = nullptr);
    void set_venues(const QStringList& venue_ids);
    QSize sizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* e) override;
    bool event(QEvent* e) override;

  private:
    int row_height() const;
    QStringList venues_;
};

} // namespace fincept::screens::widgets
