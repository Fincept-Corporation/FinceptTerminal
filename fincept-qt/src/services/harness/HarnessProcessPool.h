#pragma once
// HarnessProcessPool.h — how many harness processes terminal-AI calls may run
// at once (AI Chat, Quick Chat, Quick Ask, the tool picker, Alpha Arena, Chat
// Mode tasks). Separate from the Agent Workbench's run slots: a long workbench
// run never blocks a chat answer and the other way round.
//
// Worker threads only: acquire() blocks (in 50 ms slices, so a cancel lands
// fast) until a slot is free. First come, first served. QtCore only.

#include <QMutex>
#include <QWaitCondition>

#include <deque>
#include <functional>

namespace fincept::harness {

class HarnessProcessPool {
  public:
    explicit HarnessProcessPool(int capacity);

    /// The terminal-AI pool: kChatProcesses at once, the rest queue.
    static HarnessProcessPool& chat();
    static constexpr int kChatProcesses = 2;

    /// Wait for a slot. False when `cancelled()` turned true or `wait_ms`
    /// passed first (nothing is held then). `cancelled` may be empty.
    bool acquire(const std::function<bool()>& cancelled, int wait_ms);
    void release();

    int capacity() const { return capacity_; }
    int in_use() const;
    int waiting() const;

    /// Holds one slot for its lifetime.
    class Slot {
      public:
        Slot(HarnessProcessPool& pool, const std::function<bool()>& cancelled, int wait_ms)
            : pool_(pool), held_(pool.acquire(cancelled, wait_ms)) {}
        ~Slot() {
            if (held_)
                pool_.release();
        }
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;
        bool held() const { return held_; }

      private:
        HarnessProcessPool& pool_;
        bool held_;
    };

  private:
    const int capacity_;
    mutable QMutex mutex_;
    QWaitCondition freed_;
    int in_use_ = 0;
    std::deque<quint64> queue_; // tickets of the waiting callers, oldest first
    quint64 next_ticket_ = 0;
};

} // namespace fincept::harness
