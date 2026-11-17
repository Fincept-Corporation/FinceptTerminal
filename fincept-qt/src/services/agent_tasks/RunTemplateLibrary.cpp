// RunTemplateLibrary.cpp — built-in + user task templates and the preparation
// of a run request before it starts (WP6). See the header.

#include "services/agent_tasks/RunTemplateLibrary.h"

#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/agent_tasks/ScheduleLogic.h"
#include "services/agent_tasks/TaskRunStore.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QPointer>
#include <QThreadPool>
#include <QUuid>

#include <algorithm>

namespace fincept::agent_tasks {

namespace run_template_library_detail {

constexpr const char* kRtlTag = "RunTemplates";
constexpr int kRtlMaxHoldings = 150;
constexpr int kRtlMaxWatchlistSymbols = 300;

QString rtl_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::agent_tasks::RunTemplateLibrary", text, disambiguation, n);
}

void rtl_post(QObject* ctx, std::function<void()> fn) {
    QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [guard, has_ctx, fn = std::move(fn)]() {
            if (has_ctx && !guard)
                return;
            fn();
        },
        Qt::QueuedConnection);
}

QString rtl_number(double v) {
    QString s = QLocale(QLocale::English).toString(v, 'f', 4);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s;
}

} // namespace run_template_library_detail

using namespace run_template_library_detail;

struct RunTemplateLibrary::Impl {
    RunTemplateLibrary* q = nullptr;
    QThreadPool* worker = nullptr;
    bool loading = false;
    bool loaded = false;
    QVector<RunTemplate> user;
};

RunTemplateLibrary& RunTemplateLibrary::instance() {
    static RunTemplateLibrary s;
    return s;
}

RunTemplateLibrary::RunTemplateLibrary() : QObject(nullptr), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->worker = new QThreadPool(this);
    d_->worker->setMaxThreadCount(1);
    if (auto* app = QCoreApplication::instance()) {
        if (thread() != app->thread())
            moveToThread(app->thread());
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() { d_->worker->waitForDone(3000); });
    }
}

RunTemplateLibrary::~RunTemplateLibrary() = default;

void RunTemplateLibrary::ensure_loaded() {
    if (d_->loaded || d_->loading)
        return;
    d_->loading = true;
    QPointer<RunTemplateLibrary> self(this);
    d_->worker->start([self]() {
        auto r = TaskRunStore::instance().load_templates();
        QVector<RunTemplate> list;
        if (r.is_ok()) {
            for (const StoredRunTemplate& s : r.value()) {
                RunTemplate t = run_template_from_json(s.json);
                t.id = s.id;
                if (t.title.isEmpty())
                    t.title = s.title;
                t.created_at_ms = s.created_at_ms;
                t.updated_at_ms = s.updated_at_ms;
                t.user_defined = true;
                t.category = rtl_tr("My templates");
                list.append(t);
            }
        } else {
            LOG_WARN(kRtlTag, QStringLiteral("Loading user templates failed: %1").arg(QString::fromStdString(r.error())));
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, list]() {
                if (!self)
                    return;
                self->d_->user = list;
                self->d_->loaded = true;
                self->d_->loading = false;
                emit self->templates_changed();
            },
            Qt::QueuedConnection);
    });
}

bool RunTemplateLibrary::is_loaded() const {
    return d_->loaded;
}

QVector<RunTemplate> RunTemplateLibrary::templates() const {
    QVector<RunTemplate> out = run_templates();
    out += user_templates();
    return out;
}

QVector<RunTemplate> RunTemplateLibrary::user_templates() const {
    QVector<RunTemplate> out = d_->user;
    std::sort(out.begin(), out.end(), [](const RunTemplate& a, const RunTemplate& b) {
        return QString::compare(a.title, b.title, Qt::CaseInsensitive) < 0;
    });
    return out;
}

std::optional<RunTemplate> RunTemplateLibrary::find(const QString& id) const {
    if (id.isEmpty())
        return std::nullopt;
    for (const RunTemplate& t : d_->user)
        if (t.id == id)
            return t;
    return find_run_template(id);
}

void RunTemplateLibrary::save_user_template(RunTemplate t, QObject* ctx, SaveCallback cb) {
    t.title = t.title.trimmed();
    if (t.title.isEmpty() || t.prompt.trimmed().isEmpty()) {
        if (cb)
            rtl_post(ctx, [cb]() { cb(false, {}, rtl_tr("A template needs a name and a task text.")); });
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool is_user = t.id.startsWith(QLatin1String("user:"));
    if (!is_user) {
        if (!t.id.isEmpty())
            t.based_on = t.id;
        t.id = QStringLiteral("user:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        t.created_at_ms = now;
    }
    t.updated_at_ms = now;
    t.user_defined = true;
    t.category = rtl_tr("My templates");
    StoredRunTemplate row;
    row.id = t.id;
    row.title = t.title;
    row.json = run_template_to_json(t);
    row.created_at_ms = t.created_at_ms > 0 ? t.created_at_ms : now;
    row.updated_at_ms = now;
    QPointer<RunTemplateLibrary> self(this);
    d_->worker->start([self, row, t, ctx, cb]() {
        auto r = TaskRunStore::instance().upsert_template(row);
        const bool ok = r.is_ok();
        const QString err = ok ? QString() : QString::fromStdString(r.error());
        rtl_post(ctx, [self, ok, err, t, cb]() {
            if (self && ok) {
                auto& list = self->d_->user;
                auto it = std::find_if(list.begin(), list.end(), [&t](const RunTemplate& x) { return x.id == t.id; });
                if (it != list.end())
                    *it = t;
                else
                    list.append(t);
                emit self->templates_changed();
            }
            if (cb)
                cb(ok, ok ? t.id : QString(), ok ? QString() : rtl_tr("The template could not be saved: %1").arg(err));
        });
    });
}

void RunTemplateLibrary::delete_user_template(const QString& id, QObject* ctx, ResultCallback cb) {
    if (!id.startsWith(QLatin1String("user:"))) {
        if (cb)
            rtl_post(ctx, [cb]() { cb(false, rtl_tr("Built-in templates cannot be deleted.")); });
        return;
    }
    QPointer<RunTemplateLibrary> self(this);
    d_->worker->start([self, id, ctx, cb]() {
        auto r = TaskRunStore::instance().delete_template(id);
        const bool ok = r.is_ok();
        const QString err = ok ? QString() : QString::fromStdString(r.error());
        rtl_post(ctx, [self, ok, err, id, cb]() {
            if (self && ok) {
                self->d_->user.erase(std::remove_if(self->d_->user.begin(), self->d_->user.end(),
                                                    [&id](const RunTemplate& t) { return t.id == id; }),
                                     self->d_->user.end());
                emit self->templates_changed();
            }
            if (cb)
                cb(ok, ok ? QString() : rtl_tr("The template could not be deleted: %1").arg(err));
        });
    });
}

// ── Preparing a run ──────────────────────────────────────────────────────────

void RunTemplateLibrary::prepare_request(const RunRequest& request, const QString& refill_template_id, QObject* ctx,
                                         PrepareCallback cb) {
    Prepared p;
    p.request = request;
    const QJsonObject context = schedule_logic::expand_relative_dates(request.context, QDate::currentDate());
    if (const auto t = find(refill_template_id)) {
        const RunRequest filled = request_from_template(*t, context);
        p.request.prompt = filled.prompt;
        if (request.title.trimmed().isEmpty() || request.title == t->title)
            p.request.title = filled.title;
        p.request.template_id = t->id;
    } else {
        p.request.prompt = fill_run_template(request.prompt, context);
    }
    p.request.context = context;

    const QString portfolio_id = context.value(QStringLiteral("portfolio_id")).toVariant().toString();
    const QString portfolio_name = context.value(QStringLiteral("portfolio_name")).toString();
    const QString watchlist_id = context.value(QStringLiteral("watchlist_id")).toVariant().toString();
    const QString watchlist_name = context.value(QStringLiteral("watchlist_name")).toString();
    if (portfolio_id.isEmpty() && watchlist_id.isEmpty()) {
        p.ok = true;
        rtl_post(ctx, [cb, p]() {
            if (cb)
                cb(p);
        });
        return;
    }
    // The cloud agent cannot read the terminal's own portfolios and watchlists:
    // they go into the task text (read off the GUI thread).
    QThreadPool::globalInstance()->start([p, portfolio_id, portfolio_name, watchlist_id, watchlist_name, ctx, cb]() mutable {
        const QString today = QLocale().toString(QDate::currentDate(), QLocale::ShortFormat);
        if (!portfolio_id.isEmpty()) {
            auto assets = PortfolioRepository::instance().get_assets(portfolio_id);
            if (assets.is_err()) {
                p.error = rtl_tr("The holdings of “%1” could not be read: %2")
                              .arg(portfolio_name, QString::fromStdString(assets.error()));
            } else if (assets.value().isEmpty()) {
                p.error = rtl_tr("The portfolio “%1” has no holdings. Add some in the Portfolio screen first.")
                              .arg(portfolio_name.isEmpty() ? portfolio_id : portfolio_name);
            } else {
                QStringList lines;
                lines << QStringLiteral("| %1 | %2 | %3 | %4 |")
                             .arg(rtl_tr("Symbol"), rtl_tr("Quantity"), rtl_tr("Average cost"), rtl_tr("Sector"));
                lines << QStringLiteral("|---|---|---|---|");
                int n = 0;
                for (const portfolio::PortfolioAsset& a : assets.value()) {
                    if (a.quantity == 0)
                        continue;
                    if (++n > kRtlMaxHoldings)
                        break;
                    lines << QStringLiteral("| %1 | %2 | %3 | %4 |")
                                 .arg(a.symbol, rtl_number(a.quantity), rtl_number(a.avg_buy_price),
                                      a.sector.isEmpty() ? QStringLiteral("—") : a.sector);
                }
                const int shown = std::min(n, kRtlMaxHoldings);
                p.request.prompt += QStringLiteral("\n\n") +
                                    rtl_tr("Holdings of the portfolio “%1” (from the terminal, %2; quantities and average "
                                           "costs in each instrument's quoted currency):")
                                        .arg(portfolio_name, today) +
                                    QLatin1Char('\n') + lines.join(QLatin1Char('\n'));
                if (n > kRtlMaxHoldings)
                    p.request.prompt += QLatin1Char('\n') + rtl_tr("(%1 smaller holdings not listed)").arg(n - kRtlMaxHoldings);
                p.notes << rtl_tr("%n holding(s) of “%1” were added to the task.", nullptr, shown).arg(portfolio_name);
            }
        }
        if (p.error.isEmpty() && !watchlist_id.isEmpty()) {
            auto stocks = WatchlistRepository::instance().get_stocks(watchlist_id);
            if (stocks.is_err()) {
                p.error = rtl_tr("The watchlist “%1” could not be read: %2")
                              .arg(watchlist_name, QString::fromStdString(stocks.error()));
            } else if (stocks.value().isEmpty()) {
                p.error = rtl_tr("The watchlist “%1” is empty. Add symbols in the Watchlist screen first.")
                              .arg(watchlist_name.isEmpty() ? watchlist_id : watchlist_name);
            } else {
                QStringList symbols;
                for (const WatchlistStock& s : stocks.value()) {
                    if (symbols.size() >= kRtlMaxWatchlistSymbols)
                        break;
                    if (!s.symbol.isEmpty() && !symbols.contains(s.symbol))
                        symbols << s.symbol;
                }
                p.request.prompt += QStringLiteral("\n\n") +
                                    rtl_tr("Symbols of the watchlist “%1” (from the terminal, %2): %3")
                                        .arg(watchlist_name, today, symbols.join(QStringLiteral(", ")));
                p.notes << rtl_tr("%n symbol(s) of “%1” were added to the task.", nullptr,
                                  static_cast<int>(symbols.size()))
                               .arg(watchlist_name);
            }
        }
        if (p.request.prompt.size() > kRunPromptMaxChars) {
            p.request.prompt = p.request.prompt.left(kRunPromptMaxChars - 80) + QStringLiteral("\n") +
                               rtl_tr("(the rest was cut to fit the task limit)");
        }
        p.ok = p.error.isEmpty();
        rtl_post(ctx, [cb, p]() {
            if (cb)
                cb(p);
        });
    });
}

} // namespace fincept::agent_tasks
