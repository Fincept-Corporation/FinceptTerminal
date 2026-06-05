#pragma once
#include "services/feeds/FeedTypes.h"

#include <QWidget>

class QCloseEvent;

namespace fincept::feeds {

class FeedView;

/// Top-level window hosting one FeedView (per-feed pop-out). Emits closed() so the
/// host panel can re-dock the feed. Self-managed; not tied to the ADS dock system.
class FloatingFeedWindow : public QWidget {
    Q_OBJECT
  public:
    explicit FloatingFeedWindow(const FeedSubscription& sub, QWidget* parent = nullptr);
    QString feed_id() const { return feed_id_; }
    FeedView* view() const { return view_; }

  signals:
    void closed(const QString& feed_id);

  protected:
    void closeEvent(QCloseEvent* e) override;

  private:
    QString feed_id_;
    FeedView* view_ = nullptr;
};

} // namespace fincept::feeds
