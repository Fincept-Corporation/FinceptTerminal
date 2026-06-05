#pragma once
#include <QHash>
#include <QWidget>

class QVBoxLayout;
class QShowEvent;

namespace fincept::feeds {

class FeedView;
class FloatingFeedWindow;

/// Reusable feed section: add-feed toolbar + a vertical stack of FeedViews (one per
/// enabled subscription). Drop into any screen's layout. Calls
/// FeedMonitor::ensure_loaded() on first show. Owns floating windows for popped-out
/// feeds and skips them in the docked stack while floating.
class FeedPanel : public QWidget {
    Q_OBJECT
  public:
    explicit FeedPanel(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void rebuild();
    void open_config(const QString& feed_id); // empty id = add new
    void pop_out(const QString& feed_id);

    QVBoxLayout* stack_ = nullptr;
    QHash<QString, FeedView*> docked_;
    QHash<QString, FloatingFeedWindow*> floating_;
    bool first_shown_ = false;
};

} // namespace fincept::feeds
