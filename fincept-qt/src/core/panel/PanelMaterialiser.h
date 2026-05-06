#pragma once

#include <QObject>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <functional>

namespace fincept {

/// Spreads panel-widget construction across UI ticks so layout loads don't freeze.
/// UI-thread only. Lower priority = runs sooner: 0=foreground/visible, 1=other tabs visible, 2=hidden frames.
class PanelMaterialiser : public QObject {
    Q_OBJECT
  public:
    static PanelMaterialiser& instance();

    void enqueue(const QString& id, std::function<void()> work, int priority = 1);

    /// Synchronous drain. Used by tests and tear-off paths; avoid during normal UI flow.
    void drain_all_now();

    int pending_count() const;

    void set_tick_interval_ms(int ms);

    void set_max_per_tick(int n);

    static constexpr int kDefaultMaxPerTick = 4;
    static constexpr int kDefaultTickIntervalMs = 16;

  signals:
    void queue_drained();

  private:
    PanelMaterialiser();

    void tick();

    struct Item {
        QString id;
        std::function<void()> work;
    };

    QQueue<Item> q_priority_0_;
    QQueue<Item> q_priority_1_;
    QQueue<Item> q_priority_2_;

    QTimer tick_timer_;
    int max_per_tick_ = kDefaultMaxPerTick;
};

} // namespace fincept
