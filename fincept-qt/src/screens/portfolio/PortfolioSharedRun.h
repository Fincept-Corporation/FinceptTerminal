// src/screens/portfolio/PortfolioSharedRun.h
#pragma once
// PortfolioSharedRun — one script run whose result several ui::AutoLoadPanes
// show: the result tabs of OPTIMIZE (one optimiser run fills OPTIMIZE,
// FRONTIER, STRATEGIES, COMPARE, RISK and B-L), FFN (six tabs) and QUANTSTATS
// (METRICS, RETURNS, DRAWDOWN, ROLLING).
//
// Every pane keeps its own look (skeleton in the shape of its tab, error page
// with RETRY, "as of", remembered result — the panes of a group share one cache
// key, so the last result opens in every tab at once, also after a restart).
// Their StartFn / CancelFn are routed here:
//   * the first pane that needs a result launches the run; a pane shown while
//     it is in flight joins it (no second Python process);
//   * a pane whose result is older than the group's newest result for the same
//     view and inputs gets that one at once (no run);
//   * a pane that is left (tab switch) leaves the run; the run is cancelled only
//     when no pane re-joined it within kGraceMs — OPTIMIZE -> FRONTIER does not
//     kill and restart it, leaving the whole view does;
//   * a run that lands is delivered to every pane waiting for it, and the other
//     panes on the same view are invalidate()d so their next show takes the new
//     result from memory;
//   * a run that fails fails its waiters; a sibling shown within kFailureMemoryMs
//     shows the same error (with RETRY) instead of starting another run.
//
//   shared_ = new PortfolioSharedRun(this);
//   shared_->set_launch([this](quint64 run) { start the script; on its reply call
//                                             shared_->finish(run, json) or shared_->fail(run, message); });
//   shared_->set_abort([this](quint64 run) { requests_.take(run).cancel(); });
//   shared_->attach(pane);                                      // for each pane of the group
//   shared_->set_view(portfolio_run_key(summary), inputs_sig);  // every pane follows
//
// GUI thread only. Header-only (no Q_OBJECT: no signals of its own).

#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cmath>
#include <functional>

namespace fincept::screens {

/// Max age of a remembered result of the portfolio's price-history scripts
/// (optimiser, ffn, quantstats, Monte Carlo, backtest, historical replay): older
/// results are shown at once and refreshed quietly. Same as PortfolioService's
/// own market-analytics cache (kAnalyticsTtlSec).
constexpr int kPortfolioScriptMaxAgeS = 30 * 60;
/// Days a remembered result is kept in the persistent cache (CacheManager).
constexpr int kPortfolioResultTtlDays = 14;

/// View key of a portfolio's analytics results: the portfolio id plus a hash of
/// what the scripts are computed from (holdings and quantities, which of them can
/// be valued, benchmark, base currency). Prices are not part of it — a result is
/// refreshed by age, not on every tick; a trade gives a new key (a new default run).
inline QString portfolio_run_key(const portfolio::PortfolioSummary& s) {
    QStringList parts;
    parts.reserve(s.holdings.size());
    for (const auto& h : s.holdings)
        parts.append(h.symbol + QLatin1Char(':') + QString::number(h.quantity, 'g', 10) +
                     ((h.valued() && std::isfinite(h.weight)) ? QString() : QStringLiteral("!unvalued")));
    parts.sort();
    QJsonArray a = QJsonArray::fromStringList(parts);
    a.append(services::PortfolioService::benchmark_for(s.portfolio));
    a.append(s.base_currency.isEmpty() ? s.portfolio.currency : s.base_currency);
    return s.portfolio.id + QLatin1Char('|') + ui::autoload::signature_of(QJsonValue(a));
}

class PortfolioSharedRun : public QObject {
  public:
    /// Start the work for run `run_id` with the view's CURRENT inputs; reply with
    /// finish() / fail() (synchronously or later, GUI thread).
    using LaunchFn = std::function<void(quint64 run_id)>;
    /// Stop run `run_id` (no pane waits for it any more). Its finish()/fail() is ignored afterwards.
    using AbortFn = std::function<void(quint64 run_id)>;
    /// Called after a run landed (any pane, or none on screen).
    using ResultFn = std::function<void(const QString& view, const QJsonValue& payload, const QDateTime& as_of)>;

    /// A run nobody waits for is cancelled after this long (a tab switch re-joins within it).
    static constexpr int kGraceMs = 1500;
    /// A failure is shown to sibling tabs opened within this long instead of re-running.
    static constexpr int kFailureMemoryMs = 60 * 1000;

    explicit PortfolioSharedRun(QObject* parent) : QObject(parent) {}

    void set_launch(LaunchFn f) { launch_ = std::move(f); }
    void set_abort(AbortFn f) { abort_ = std::move(f); }
    void set_on_result(ResultFn f) { on_result_ = std::move(f); }

    /// Route `pane`'s StartFn / CancelFn here. The pane keeps its own cache key,
    /// render function, look and policy.
    void attach(ui::AutoLoadPane* pane) {
        if (!pane)
            return;
        panes_.append(pane);
        QPointer<PortfolioSharedRun> self(this);
        QPointer<ui::AutoLoadPane> p(pane);
        pane->set_start([self, p](quint64 id) {
            if (self && p)
                self->request(p.data(), id);
        });
        pane->set_cancel([self, p](quint64 id) {
            if (self)
                self->leave(p.data(), id);
        });
        if (!view_.isNull()) {
            pane->set_view(view_);
            pane->set_signature(signature_);
        }
    }

    /// The view key (portfolio_run_key) and the inputs signature the next run uses;
    /// every attached pane follows. Call before the panes' first show and whenever
    /// the portfolio, its holdings or the user's inputs change.
    void set_view(const QString& view, const QString& signature) {
        view_ = view;
        signature_ = signature;
        for (const auto& p : panes_) {
            if (!p)
                continue;
            p->set_view(view);
            p->set_signature(signature);
        }
    }
    QString view() const { return view_; }
    QString signature() const { return signature_; }

    /// A run is in flight for the current view.
    bool running() const {
        for (auto it = runs_.cbegin(); it != runs_.cend(); ++it)
            if (it->view == view_)
                return true;
        return false;
    }

    /// Completion of run `run_id` (stale / aborted ids are ignored).
    void finish(quint64 run_id, const QJsonValue& payload) {
        const auto it = runs_.find(run_id);
        if (it == runs_.end())
            return;
        const Run run = it.value();
        runs_.erase(it);
        failures_.remove(run.view);
        const QDateTime at = QDateTime::currentDateTimeUtc();
        remember(run.view, Result{payload, at, run.signature});
        QSet<ui::AutoLoadPane*> served;
        for (const auto& w : run.waiters) {
            if (!w.pane)
                continue;
            served.insert(w.pane.data());
            w.pane->deliver(w.id, payload, at);
        }
        for (const auto& p : panes_) {
            if (p && !served.contains(p.data()) && p->view() == run.view && p->signature() == run.signature)
                p->invalidate(); // next show takes the new result from memory
        }
        if (on_result_)
            on_result_(run.view, payload, at);
    }

    /// Failure of run `run_id`: every waiting pane shows `message` with RETRY.
    void fail(quint64 run_id, const QString& message) {
        const auto it = runs_.find(run_id);
        if (it == runs_.end())
            return;
        const Run run = it.value();
        runs_.erase(it);
        Failure f;
        f.signature = run.signature;
        f.message = message;
        f.at_ms = QDateTime::currentMSecsSinceEpoch();
        for (const auto& w : run.waiters) {
            if (!w.pane)
                continue;
            f.told.insert(w.pane.data());
        }
        failures_.insert(run.view, f);
        for (const auto& w : run.waiters)
            if (w.pane)
                w.pane->fail(w.id, message);
    }

  private:
    struct Waiter {
        QPointer<ui::AutoLoadPane> pane;
        quint64 id = 0;
    };
    struct Run {
        QString view;
        QString signature;
        QList<Waiter> waiters;
    };
    struct Result {
        QJsonValue payload;
        QDateTime as_of;
        QString signature;
    };
    struct Failure {
        QString signature;
        QString message;
        qint64 at_ms = 0;
        QSet<ui::AutoLoadPane*> told; ///< panes that already showed it (their next request re-runs)
    };

    void request(ui::AutoLoadPane* pane, quint64 id) {
        const QString view = pane->view();
        const QString sig = pane->signature();
        // 1. The group already has a newer, still fresh result for these inputs.
        const auto r = results_.constFind(view);
        if (r != results_.constEnd() && r->signature == sig &&
            (!pane->has_result() || r->as_of > pane->as_of()) &&
            !ui::autoload::is_stale(r->as_of, QDateTime::currentDateTimeUtc(), pane->max_age())) {
            pane->deliver(id, r->payload, r->as_of);
            return;
        }
        // 2. A sibling just failed on these inputs: show that instead of re-running.
        //    (This pane's own next request — RETRY — runs again.)
        auto f = failures_.find(view);
        if (f != failures_.end() && f->signature == sig && !f->told.contains(pane) &&
            QDateTime::currentMSecsSinceEpoch() - f->at_ms < kFailureMemoryMs) {
            f->told.insert(pane);
            pane->fail(id, f->message);
            return;
        }
        // 3. Join the run in flight for these inputs.
        for (auto it = runs_.begin(); it != runs_.end(); ++it) {
            if (it->view == view && it->signature == sig) {
                it->waiters.append(Waiter{QPointer<ui::AutoLoadPane>(pane), id});
                return;
            }
        }
        // 4. Launch.
        const quint64 run_id = ++next_run_;
        Run run;
        run.view = view;
        run.signature = sig;
        run.waiters.append(Waiter{QPointer<ui::AutoLoadPane>(pane), id});
        runs_.insert(run_id, run);
        if (launch_)
            launch_(run_id); // may finish() / fail() synchronously
        else
            fail(run_id, QStringLiteral("Nothing to run."));
    }

    void leave(ui::AutoLoadPane* pane, quint64 id) {
        QList<quint64> orphaned;
        for (auto it = runs_.begin(); it != runs_.end(); ++it) {
            auto& ws = it->waiters;
            for (int i = ws.size() - 1; i >= 0; --i) {
                if (!ws[i].pane || (ws[i].pane.data() == pane && ws[i].id == id))
                    ws.removeAt(i);
            }
            if (ws.isEmpty())
                orphaned.append(it.key());
        }
        for (quint64 run_id : orphaned) {
            QPointer<PortfolioSharedRun> self(this);
            QTimer::singleShot(kGraceMs, this, [self, run_id]() {
                if (!self)
                    return;
                const auto it = self->runs_.find(run_id);
                if (it == self->runs_.end() || !it->waiters.isEmpty())
                    return; // finished meanwhile, or a pane re-joined it
                self->runs_.erase(it);
                if (self->abort_)
                    self->abort_(run_id);
            });
        }
    }

    void remember(const QString& view, const Result& r) {
        results_.insert(view, r);
        result_order_.removeOne(view);
        result_order_.append(view);
        while (result_order_.size() > kKeptViews)
            results_.remove(result_order_.takeFirst());
    }

    static constexpr int kKeptViews = 6;

    LaunchFn launch_;
    AbortFn abort_;
    ResultFn on_result_;
    QList<QPointer<ui::AutoLoadPane>> panes_;
    QString view_;
    QString signature_;
    QHash<quint64, Run> runs_;
    QHash<QString, Result> results_;
    QStringList result_order_;
    QHash<QString, Failure> failures_;
    quint64 next_run_ = 0;
};

} // namespace fincept::screens
