#pragma once
#include "services/feeds/FeedTypes.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QStackedWidget;
class QToolButton;

namespace fincept::feeds {

class FeedCardView;
class FeedTableView;

/// One feed: header (name, status dot, ⟳ refresh, ⧉ pop-out/⤓ dock-back, ⚙ config) +
/// body (cards or table per sub.display_mode). Subscribes to FeedMonitor filtered by id.
class FeedView : public QWidget {
    Q_OBJECT
  public:
    explicit FeedView(FeedSubscription sub, QWidget* parent = nullptr);
    QString feed_id() const { return sub_.id; }
    void set_subscription(const FeedSubscription& sub); // re-render on display-mode/name change
    // Floating mode flips the ⧉ pop-out button into a ⤓ "dock back" button
    // (emits pop_in_requested instead of pop_out_requested).
    void set_floating(bool floating);

  signals:
    void pop_out_requested(const QString& feed_id);
    void pop_in_requested(const QString& feed_id);
    void config_requested(const QString& feed_id);

  private:
    void on_updated(const QString& id, const QVector<FeedItem>& items);
    void on_status(const QString& id, FeedStatus st, const QString& msg);
    void rebuild_body();

    FeedSubscription sub_;
    QLabel* title_ = nullptr;
    QLabel* status_dot_ = nullptr;
    QToolButton* popout_btn_ = nullptr;
    QStackedWidget* body_ = nullptr;
    FeedCardView* cards_ = nullptr;
    FeedTableView* table_ = nullptr;
    bool floating_mode_ = false;
};

} // namespace fincept::feeds
