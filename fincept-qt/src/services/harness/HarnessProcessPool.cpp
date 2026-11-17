// HarnessProcessPool.cpp — slot pool for terminal-AI harness processes (see the header).

#include "services/harness/HarnessProcessPool.h"

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QMutexLocker>

#include <algorithm>

namespace fincept::harness {

namespace harness_pool_detail {
constexpr int kHarnessPoolSliceMs = 50;
} // namespace harness_pool_detail

HarnessProcessPool::HarnessProcessPool(int capacity) : capacity_(std::max(1, capacity)) {}

HarnessProcessPool& HarnessProcessPool::chat() {
    static HarnessProcessPool pool(kChatProcesses);
    return pool;
}

bool HarnessProcessPool::acquire(const std::function<bool()>& cancelled, int wait_ms) {
    using namespace harness_pool_detail;
    QElapsedTimer waited;
    waited.start();
    QMutexLocker lock(&mutex_);
    if (in_use_ < capacity_ && queue_.empty()) {
        ++in_use_;
        return true;
    }
    // FIFO: a slot that frees goes to the oldest waiter still waiting.
    const quint64 ticket = next_ticket_++;
    queue_.push_back(ticket);
    bool got = false;
    for (;;) {
        if (queue_.front() == ticket && in_use_ < capacity_) {
            ++in_use_;
            got = true;
            break;
        }
        if ((cancelled && cancelled()) || waited.elapsed() >= wait_ms)
            break;
        freed_.wait(&mutex_, QDeadlineTimer(kHarnessPoolSliceMs));
    }
    queue_.erase(std::find(queue_.begin(), queue_.end(), ticket));
    lock.unlock();
    freed_.wakeAll(); // the next in line may go now (also when this one gave up)
    return got;
}

void HarnessProcessPool::release() {
    {
        QMutexLocker lock(&mutex_);
        if (in_use_ > 0)
            --in_use_;
    }
    freed_.wakeAll();
}

int HarnessProcessPool::in_use() const {
    QMutexLocker lock(&mutex_);
    return in_use_;
}

int HarnessProcessPool::waiting() const {
    QMutexLocker lock(&mutex_);
    return static_cast<int>(queue_.size());
}

} // namespace fincept::harness
