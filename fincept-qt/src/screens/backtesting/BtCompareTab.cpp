// src/screens/backtesting/BtCompareTab.cpp
//
// COMPARE tab (BtWorkbenchTabs.h): keeps up to 6 saved-run ids (and the baseline) remembered with
// ui::InputMemory("backtesting", "compare"), reads each run through BtRunController::load_saved
// (worker; cached bundles are reused) and shows them in BtCompareView. "+ ADD RUN" offers the 20
// newest saved backtests (read on a worker); prune() drops ids whose run no longer exists.
#include "screens/backtesting/BtWorkbenchTabs.h"

#include "screens/backtesting/BtAnalysisViews.h"
#include "screens/backtesting/BtRunController.h"
#include "services/backtesting/BacktestRunStore.h"
#include "ui/formatting/DateFormat.h"
#include "ui/widgets/InputMemory.h"

#include <QCoreApplication>
#include <QCursor>
#include <QHash>
#include <QJsonArray>
#include <QMenu>
#include <QPointer>
#include <QSet>
#include <QVBoxLayout>
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace fincept::screens::bt {

using services::backtest::BacktestRunStore;
using services::backtest::SavedRun;

namespace {

constexpr int kCmpTabMaxRuns = 6;
constexpr int kCmpTabAddChoices = 20;

QString cmp_tab_engine_name(const QString& slug) {
    for (const services::backtest::Provider& p : services::backtest::all_providers())
        if (p.slug == slug)
            return p.display_name;
    return slug;
}

/// "SMA Crossover · SPY" when the run has no label.
QString cmp_tab_label(const BtResultBundle& b, qint64 id) {
    if (!b.label.isEmpty())
        return b.label;
    QStringList parts;
    const QJsonObject strategy = b.request.value(QStringLiteral("strategy")).toObject();
    QString name = strategy.value(QStringLiteral("name")).toString();
    if (name.isEmpty())
        name = strategy.value(QStringLiteral("type")).toString();
    if (name.isEmpty() && b.run)
        name = b.run->provenance.strategy_name.isEmpty() ? b.run->provenance.strategy_type
                                                         : b.run->provenance.strategy_name;
    if (!name.isEmpty())
        parts << name;
    QStringList syms;
    for (const QJsonValue& v : b.request.value(QStringLiteral("symbols")).toArray())
        syms << v.toString();
    if (syms.isEmpty() && b.run)
        syms = b.run->symbols();
    if (!syms.isEmpty())
        parts << syms.join(QLatin1Char(','));
    return parts.isEmpty() ? BtCompareTab::tr("Run %1").arg(id) : parts.join(QStringLiteral(" · "));
}

/// A choice of the ADD RUN menu (built on the worker).
struct CmpTabChoice {
    qint64 id = 0;
    QString text;
    bool pinned = false;
};

} // namespace

struct BtCompareTab::Impl {
    BtCompareTab* q = nullptr;
    BtHost* host = nullptr;
    BtCompareView* view = nullptr;
    ui::InputMemory* memory = nullptr;
    QVector<qint64> ids;
    qint64 baseline = 0;
    QHash<qint64, BtBundlePtr> bundles; ///< loaded (null = could not be read)
    QHash<qint64, QString> errors;
    QSet<qint64> loading;
    quint64 menu_generation = 0;

    void save();
    void load_missing();
    void rebuild();
    void set_ids(const QVector<qint64>& next, bool emit_change);
    void show_add_menu(const QPoint& at);
};

BtCompareTab::BtCompareTab(BtHost* host, QWidget* parent) : QWidget(parent), d(std::make_unique<Impl>()) {
    d->q = this;
    d->host = host;
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    d->view = new BtCompareView(this);
    root->addWidget(d->view);
    d->memory = new ui::InputMemory(QStringLiteral("backtesting"), QStringLiteral("compare"), this);

    connect(d->view, &BtCompareView::remove_requested, this, [this](qint64 id) {
        QVector<qint64> next = d->ids;
        next.removeAll(id);
        d->set_ids(next, true);
    });
    connect(d->view, &BtCompareView::baseline_requested, this, [this](qint64 id) {
        if (!d->ids.contains(id) || d->baseline == id)
            return;
        d->baseline = id;
        d->save();
        d->rebuild();
    });
    connect(d->view, &BtCompareView::open_requested, this, [this](qint64 id) {
        if (d->host)
            d->host->open_saved_run(id);
    });
    connect(d->view, &BtCompareView::add_requested, this, [this]() { d->show_add_menu(QCursor::pos()); });

    // The remembered selection.
    QVector<qint64> remembered;
    for (const QVariant& v : d->memory->value(QStringLiteral("ids")).toList()) {
        const qint64 id = v.toLongLong();
        if (id > 0 && !remembered.contains(id) && remembered.size() < kCmpTabMaxRuns)
            remembered.append(id);
    }
    d->baseline = d->memory->value(QStringLiteral("baseline"), 0).toLongLong();
    d->ids = remembered;
    if (!d->ids.contains(d->baseline))
        d->baseline = 0;
    d->rebuild();
    d->load_missing();
    if (!d->ids.isEmpty())
        prune();
}

BtCompareTab::~BtCompareTab() = default;

void BtCompareTab::Impl::save() {
    QVariantList list;
    for (qint64 id : ids)
        list << id;
    memory->set_value(QStringLiteral("ids"), list);
    memory->set_value(QStringLiteral("baseline"), baseline);
}

void BtCompareTab::Impl::set_ids(const QVector<qint64>& next, bool emit_change) {
    QVector<qint64> clean;
    for (qint64 id : next)
        if (id > 0 && !clean.contains(id) && clean.size() < kCmpTabMaxRuns)
            clean.append(id);
    const bool changed = clean != ids;
    ids = clean;
    if (!ids.contains(baseline))
        baseline = 0;
    for (auto it = bundles.begin(); it != bundles.end();) {
        if (!ids.contains(it.key())) {
            errors.remove(it.key());
            it = bundles.erase(it);
        } else {
            ++it;
        }
    }
    save();
    rebuild();
    load_missing();
    if (changed && emit_change)
        emit q->ids_changed();
}

void BtCompareTab::Impl::load_missing() {
    if (!host || !host->controller())
        return;
    QPointer<BtCompareTab> self(q);
    for (qint64 id : ids) {
        if (bundles.contains(id) || loading.contains(id))
            continue;
        loading.insert(id);
        host->controller()->load_saved(id, [self, id](BtBundlePtr b, QString err) {
            if (!self)
                return;
            Impl* d = self->d.get();
            d->loading.remove(id);
            if (!d->ids.contains(id))
                return; // removed while it loaded
            d->bundles.insert(id, b);
            if (!b)
                d->errors.insert(id, err);
            bool all = true;
            for (qint64 k : d->ids)
                if (!d->bundles.contains(k))
                    all = false;
            if (all)
                d->rebuild();
        });
    }
}

void BtCompareTab::Impl::rebuild() {
    QVector<BtCompareEntry> entries;
    for (qint64 id : ids) {
        BtCompareEntry e;
        e.id = id;
        e.baseline = baseline != 0 && id == baseline;
        const BtBundlePtr b = bundles.value(id);
        if (b) {
            e.label = cmp_tab_label(*b, id);
            e.as_of = b->as_of.isValid() ? ui::formatting::date_time(b->as_of, ui::formatting::DateStyle::Short)
                                         : QString();
            e.engine = cmp_tab_engine_name(b->engine);
            e.run = b->run;
            e.inputs = b->request;
            if (!b->run)
                e.label += QStringLiteral(" (") + BtCompareTab::tr("not a backtest") + QLatin1Char(')');
        } else if (bundles.contains(id)) {
            e.label = BtCompareTab::tr("Run %1 (%2)").arg(id).arg(
                errors.value(id).isEmpty() ? BtCompareTab::tr("could not be read") : errors.value(id));
        } else {
            e.label = BtCompareTab::tr("Run %1 (loading…)").arg(id);
        }
        entries.append(e);
    }
    view->show_runs(entries);
}

void BtCompareTab::Impl::show_add_menu(const QPoint& at) {
    const quint64 gen = ++menu_generation;
    const QVector<qint64> exclude = ids;
    QPointer<BtCompareTab> self(q);
    (void)QtConcurrent::run([self, gen, exclude, at]() {
        QVector<CmpTabChoice> choices;
        QString error;
        auto& store = BacktestRunStore::instance();
        if (!store.available()) {
            error = BtCompareTab::tr("The run history is not available yet.");
        } else {
            auto listed = store.list(200);
            if (listed.is_err()) {
                error = QString::fromStdString(listed.error());
            } else {
                for (const SavedRun& s : listed.value()) {
                    if (s.command != QLatin1String("backtest") || exclude.contains(s.id))
                        continue;
                    CmpTabChoice c;
                    c.id = s.id;
                    c.pinned = s.pinned();
                    QStringList parts;
                    if (!s.label.isEmpty())
                        parts << s.label;
                    if (!s.strategy.isEmpty())
                        parts << s.strategy;
                    if (!s.symbols.isEmpty())
                        parts << s.symbols;
                    parts << cmp_tab_engine_name(s.provider) << s.created_at.left(16).replace(QLatin1Char('T'),
                                                                                               QLatin1Char(' '));
                    c.text = (c.pinned ? QStringLiteral("★ ") : QString()) + parts.join(QStringLiteral(" · "));
                    choices.append(c);
                    if (choices.size() >= kCmpTabAddChoices)
                        break;
                }
            }
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, gen, choices, error, at]() {
                if (!self || self->d->menu_generation != gen)
                    return;
                Impl* d = self->d.get();
                if (!error.isEmpty()) {
                    if (d->host)
                        d->host->notify(BtCompareTab::tr("Could not list the saved runs: %1").arg(error));
                    return;
                }
                if (choices.isEmpty()) {
                    if (d->host)
                        d->host->notify(BtCompareTab::tr("No other saved backtest to add: run one first."));
                    return;
                }
                auto* menu = new QMenu(self);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                menu->setToolTipsVisible(true);
                const bool full = d->ids.size() >= kCmpTabMaxRuns;
                for (const CmpTabChoice& c : choices) {
                    QString text = c.text;
                    text.replace(QLatin1Char('&'), QStringLiteral("&&"));
                    QAction* a = menu->addAction(text);
                    a->setEnabled(!full);
                    const qint64 id = c.id;
                    QObject::connect(a, &QAction::triggered, self, [self, id]() {
                        if (self)
                            self->add_run(id);
                    });
                }
                if (full)
                    menu->addSection(BtCompareTab::tr("%1 runs is the maximum: remove one first").arg(kCmpTabMaxRuns));
                menu->popup(at);
            },
            Qt::QueuedConnection);
    });
}

void BtCompareTab::set_runs(const QVector<qint64>& store_ids) {
    d->set_ids(store_ids, true);
}

void BtCompareTab::add_run(qint64 store_id) {
    if (store_id <= 0 || d->ids.contains(store_id))
        return;
    if (d->ids.size() >= kCmpTabMaxRuns) {
        if (d->host)
            d->host->notify(tr("COMPARE holds at most %1 runs: remove one first.").arg(kCmpTabMaxRuns));
        return;
    }
    QVector<qint64> next = d->ids;
    next.append(store_id);
    d->set_ids(next, true);
}

QVector<qint64> BtCompareTab::run_ids() const {
    return d->ids;
}

void BtCompareTab::prune() {
    const QVector<qint64> ids = d->ids;
    if (ids.isEmpty())
        return;
    QPointer<BtCompareTab> self(this);
    (void)QtConcurrent::run([self, ids]() {
        QVector<qint64> gone;
        auto& store = BacktestRunStore::instance();
        if (!store.available())
            return;
        for (qint64 id : ids) {
            auto r = store.row(id);
            if (r.is_ok() && r.value().id == 0)
                gone.append(id);
        }
        if (gone.isEmpty())
            return;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, gone]() {
                if (!self)
                    return;
                QVector<qint64> next = self->d->ids;
                for (qint64 id : gone)
                    next.removeAll(id);
                if (next != self->d->ids)
                    self->d->set_ids(next, true);
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens::bt
