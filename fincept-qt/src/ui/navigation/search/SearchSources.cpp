// SearchSources.cpp — see SearchSources.h.
#include "ui/navigation/search/SearchSources.h"

#include "core/actions/ActionRegistry.h"
#include "core/command/CommandRank.h"
#include "core/command/CommandRegistry.h"
#include "core/command/CommandSeed.h"
#include "core/layout/LayoutCatalog.h"
#include "core/logging/Logger.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/backtesting/BacktestRunStore.h"
#include "services/connectors/ConnectorCatalog.h"
#include "storage/repositories/NotesRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/navigation/search/SearchActions.h"
#include "ui/notifications/NotificationService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>

namespace fincept::ui::search::sources {

namespace {

namespace cmd = fincept::command;

constexpr const char* kTag = "CommandBar";
constexpr const char* kUsageKey = "search.usage";
constexpr const char* kBridgeOwner = "actions";
constexpr const char* kSavedOwner = "search.saved";
constexpr const char* kDataOwner = "search.data";
constexpr qint64 kSavedFreshMs = 20000;

QString ss_tr(const char* s) {
    return QCoreApplication::translate("CommandBar", s);
}

// ── ActionRegistry bridge ───────────────────────────────────────────────────────

QString ss_action_subtitle(const ActionDef& def) {
    static const QHash<QString, const char*> kSubtitles = {
        {"frame.toggle_chat_mode", QT_TRANSLATE_NOOP("CommandBar", "Switches between the terminal and Chat Mode")},
        {"frame.toggle_fullscreen", QT_TRANSLATE_NOOP("CommandBar", "Fills the screen with this window")},
        {"frame.toggle_focus_mode", QT_TRANSLATE_NOOP("CommandBar", "Hides the bars for a distraction-free view")},
        {"frame.toggle_always_on_top", QT_TRANSLATE_NOOP("CommandBar", "Keeps this window above the others")},
        {"frame.screenshot", QT_TRANSLATE_NOOP("CommandBar", "Saves a picture of this window")},
        {"frame.component_browser", QT_TRANSLATE_NOOP("CommandBar", "Opens the list of every screen, by category")},
        {"panel.refresh", QT_TRANSLATE_NOOP("CommandBar", "Reloads the focused panel")},
        {"terminal.lock_now", QT_TRANSLATE_NOOP("CommandBar", "Locks the terminal; your PIN unlocks it")},
        {"help.replay_tour", QT_TRANSLATE_NOOP("CommandBar", "Shows the short tour of the terminal again")},
        {"billing.plans", QT_TRANSLATE_NOOP("CommandBar", "Opens Plans & Pricing")},
        {"help.enterprise", QT_TRANSLATE_NOOP("CommandBar", "About Fincept Terminal Enterprise")},
        {"cmdbar.toggle", QT_TRANSLATE_NOOP("CommandBar", "Shows the command line at the bottom of the window")},
        {"palette.open", QT_TRANSLATE_NOOP("CommandBar", "Opens the command palette")},
        {"layout.tile_2x2", QT_TRANSLATE_NOOP("CommandBar", "Arranges the panels in a 2×2 grid")},
        {"panel.fullscreen_toggle", QT_TRANSLATE_NOOP("CommandBar", "Lets the focused panel fill the window")},
        {"panel.tear_off", QT_TRANSLATE_NOOP("CommandBar", "Moves the focused panel into its own window")},
        {"link.clear_group", QT_TRANSLATE_NOOP("CommandBar", "Stops linking the focused panel's symbol")},
        {"window.new_on_next_monitor", QT_TRANSLATE_NOOP("CommandBar", "Opens a new window on the next monitor")},
        {"layout.new", QT_TRANSLATE_NOOP("CommandBar", "Starts a new saved layout")},
    };
    const auto it = kSubtitles.constFind(def.id);
    if (it != kSubtitles.constEnd())
        return QCoreApplication::translate("CommandBar", it.value());
    if (def.id.startsWith(QLatin1String("debug.")))
        return ss_tr("Developer tool");
    return ss_tr("Runs “%1”").arg(def.display);
}

QString ss_action_risk(const ActionDef& def) {
    static const QRegularExpression account(QStringLiteral("lock|logout|log_out|sign_out"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression destructive(QStringLiteral("delete|remove|reset|clear"),
                                                QRegularExpression::CaseInsensitiveOption);
    const QString words = def.id + QLatin1Char(' ') + def.display;
    if (account.match(words).hasMatch())
        return QString::fromLatin1(cmd::risk::kAccount);
    if (destructive.match(words).hasMatch() && def.id != QLatin1String("link.clear_group"))
        return QString::fromLatin1(cmd::risk::kDelete);
    return {};
}

/// Actions that need a value the box cannot ask for (a name, a path) stay out; their dialogs are rows
/// of their own (Save layout as…, Open layout…). Ctrl+K's own action is the box itself.
bool ss_action_listed(const ActionDef& def) {
    if (def.id == QLatin1String("frame.browse_components"))
        return false;
    for (const ParameterSlot& p : def.parameter_slots)
        if (p.required && !p.default_value.isValid())
            return false;
    return true;
}

void ss_sync_action(const QString& id) {
    auto& reg = cmd::CommandRegistry::instance();
    const QString row_id = QStringLiteral("action:") + id;
    const ActionDef* def = ActionRegistry::instance().find(id);
    if (!def || !ss_action_listed(*def)) {
        reg.unregister_item(row_id);
        return;
    }
    cmd::CommandItem it;
    it.id = row_id;
    it.title = def->display;
    it.keywords = def->aliases;
    it.keywords.append(def->category);
    it.group = QString::fromLatin1(cmd::groups::kAction);
    it.subtitle = ss_action_subtitle(*def);
    it.agent_risk = ss_action_risk(*def);
    it.owner = QString::fromLatin1(kBridgeOwner);
    it.shortcut = def->default_hotkey.toString(QKeySequence::NativeText);
    if (id.startsWith(QLatin1String("debug.")))
        it.priority = -100;
    else if (id.startsWith(QLatin1String("window.focus_")) || id.startsWith(QLatin1String("window.move_to_monitor_")))
        it.priority = -40;
    it.args.insert(QStringLiteral("action_id"), id);
    it.run = [id](const cmd::CommandContext& ctx) {
        actions::Ctx a;
        a.window = qobject_cast<QWidget*>(ctx.window.data());
        a.new_panel = ctx.new_panel;
        QString error;
        if (!actions::invoke_action(a, id, ctx.args, &error))
            ToastService::instance().post(ToastService::Severity::Warning, error, QStringLiteral("search"));
    };
    reg.register_item(it);
}

void ss_install_bridge() {
    auto& actions_reg = ActionRegistry::instance();
    for (const QString& id : actions_reg.all_ids())
        ss_sync_action(id);
    QObject::connect(&actions_reg, &ActionRegistry::action_registered, &cmd::CommandRegistry::instance(),
                     [](const QString& id) { ss_sync_action(id); });
    QObject::connect(&actions_reg, &ActionRegistry::action_replaced, &cmd::CommandRegistry::instance(),
                     [](const QString& id) { ss_sync_action(id); });
    QObject::connect(&actions_reg, &ActionRegistry::action_unregistered, &cmd::CommandRegistry::instance(),
                     [](const QString& id) {
                         cmd::CommandRegistry::instance().unregister_item(QStringLiteral("action:") + id);
                     });
}

// ── Saved items: snapshots ─────────────────────────────────────────────────────

struct SavedState {
    QList<cmd::CommandItem> worker_rows; // watchlists, portfolios, backtest runs (read off the GUI thread)
    QList<cmd::CommandItem> gui_rows;    // layouts, agent runs (GUI-thread APIs, in memory / tiny)
    qint64 loaded_ms = 0;
    bool loading = false;
};

SavedState& ss_saved() {
    static SavedState s;
    return s;
}

cmd::CommandItem ss_saved_row(const QString& id, const QString& title, const QString& context, const QString& subtitle,
                              const QString& screen, const QString& section, const QStringList& keywords) {
    cmd::CommandItem it;
    it.id = id;
    it.title = title;
    it.context = context;
    it.subtitle = subtitle;
    it.screen_id = screen;
    it.section = section;
    it.keywords = keywords;
    it.group = QString::fromLatin1(cmd::groups::kSaved);
    it.owner = QString::fromLatin1(kSavedOwner);
    return it;
}

QList<cmd::CommandItem> ss_load_worker_rows() {
    QList<cmd::CommandItem> rows;
    const auto lists = WatchlistRepository::instance().list_all();
    if (lists.is_ok())
        for (const auto& wl : lists.value())
            rows.append(ss_saved_row(QStringLiteral("saved.watchlist.") + wl.id, wl.name, ss_tr("Watchlist"),
                                     ss_tr("Opens the %1 watchlist").arg(wl.name), QStringLiteral("watchlist"),
                                     QStringLiteral("list:") + wl.id, {QStringLiteral("watchlist"), QStringLiteral("list")}));
    const auto ports = PortfolioRepository::instance().list_portfolios();
    if (ports.is_ok())
        for (const auto& p : ports.value())
            rows.append(ss_saved_row(QStringLiteral("saved.portfolio.") + p.id, p.name, ss_tr("Portfolio"),
                                     ss_tr("Opens the %1 portfolio").arg(p.name), QStringLiteral("portfolio"),
                                     QStringLiteral("portfolio:") + p.id, {QStringLiteral("portfolio")}));
    const auto runs = services::backtest::BacktestRunStore::instance().list(60);
    if (runs.is_ok()) {
        for (const auto& r : runs.value()) {
            QString title = r.label.trimmed();
            if (title.isEmpty())
                title = QStringList{r.strategy, r.symbols}.join(QStringLiteral(" · "));
            rows.append(ss_saved_row(QStringLiteral("saved.btrun.") + QString::number(r.id), title, ss_tr("Backtest run"),
                                     ss_tr("Opens this saved run in Backtesting"), QStringLiteral("backtesting"),
                                     QStringLiteral("run:") + QString::number(r.id),
                                     {QStringLiteral("backtest"), QStringLiteral("run"), r.provider, r.symbols}));
        }
    }
    return rows;
}

QList<cmd::CommandItem> ss_load_gui_rows() {
    QList<cmd::CommandItem> rows;
    const auto layouts = LayoutCatalog::instance().list_layouts();
    if (layouts.is_ok()) {
        for (const auto& e : layouts.value()) {
            if (e.kind == QLatin1String("auto") || e.kind == QLatin1String("crash_recovery"))
                continue;
            cmd::CommandItem it = ss_saved_row(QStringLiteral("saved.layout.") + e.id.to_string(), e.name, ss_tr("Layout"),
                                               ss_tr("Restores the panels saved as %1").arg(e.name), QString(), QString(),
                                               {QStringLiteral("layout"), QStringLiteral("workspace")});
            const QString name = e.name;
            it.run = [name](const cmd::CommandContext& ctx) {
                actions::Ctx a;
                a.window = qobject_cast<QWidget*>(ctx.window.data());
                QString error;
                if (!actions::invoke_action(a, QStringLiteral("layout.switch"), {{QStringLiteral("name"), name}}, &error))
                    ToastService::instance().post(ToastService::Severity::Warning, error, QStringLiteral("search"));
            };
            rows.append(it);
        }
    }
    auto& runs = agent_tasks::TaskRunService::instance();
    if (runs.is_loaded()) {
        int n = 0;
        for (const QString& id : runs.run_ids()) {
            if (++n > 40)
                break;
            const agent_tasks::TaskRun* r = runs.find_run(id);
            if (!r || r->title.trimmed().isEmpty())
                continue;
            cmd::CommandItem it = ss_saved_row(QStringLiteral("saved.agentrun.") + id, r->title.trimmed(), ss_tr("Agent run"),
                                               ss_tr("Opens this run in Chat Mode"), QStringLiteral("chat_mode"), QString(),
                                               {QStringLiteral("run"), QStringLiteral("agent"), QStringLiteral("chat")});
            it.args.insert(QStringLiteral("run_id"), id);
            rows.append(it);
        }
    }
    return rows;
}

void ss_register_saved_provider() {
    cmd::ProviderSpec p;
    p.id = QStringLiteral("search.saved");
    p.group = QString::fromLatin1(cmd::groups::kSaved);
    p.owner = QString::fromLatin1(kSavedOwner);
    p.cap = 12;
    p.min_chars = 1;
    p.query = [](const QString& query, quint64, cmd::ResultSink sink) {
        const cmd::rank::Query q = cmd::rank::prepare_query(query);
        struct Scored {
            qint64 score;
            const cmd::CommandItem* row;
        };
        QList<Scored> hits;
        const SavedState& s = ss_saved();
        auto consider = [&](const cmd::CommandItem& row) {
            cmd::rank::Fields f;
            f.title = row.title;
            f.keywords = row.keywords;
            f.keywords.append(row.context);
            const cmd::rank::Match m = cmd::rank::match(q, cmd::rank::prepare(f));
            if (m.tier >= cmd::rank::Tier::Fuzzy)
                hits.append({cmd::rank::score(m, 0, 0), &row});
        };
        for (const cmd::CommandItem& r : s.gui_rows)
            consider(r);
        for (const cmd::CommandItem& r : s.worker_rows)
            consider(r);
        std::stable_sort(hits.begin(), hits.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
        QList<cmd::CommandItem> rows;
        for (const Scored& h : hits) {
            if (rows.size() >= 12)
                break;
            rows.append(*h.row);
        }
        sink(rows, true);
    };
    cmd::CommandRegistry::instance().register_provider(p);
}

void ss_register_notes_provider() {
    cmd::ProviderSpec p;
    p.id = QStringLiteral("search.notes");
    p.group = QString::fromLatin1(cmd::groups::kSaved);
    p.owner = QString::fromLatin1(kSavedOwner);
    p.cap = 5;
    p.min_chars = 2;
    p.slow = true; // SQLite on a worker
    p.query = [](const QString& query, quint64, cmd::ResultSink sink) {
        const QString q = query.trimmed();
        QThreadPool::globalInstance()->start([q, sink]() {
            QList<cmd::CommandItem> rows;
            const auto found = NotesRepository::instance().search(q);
            if (found.is_ok()) {
                for (const FinancialNote& n : found.value()) {
                    if (n.is_archived || n.title.trimmed().isEmpty())
                        continue;
                    rows.append(ss_saved_row(QStringLiteral("saved.note.") + QString::number(n.id), n.title.trimmed(),
                                             ss_tr("Note"), ss_tr("Opens this note in Notes"), QStringLiteral("notes"),
                                             QStringLiteral("note:") + QString::number(n.id),
                                             {QStringLiteral("note"), n.tickers, n.tags}));
                    if (rows.size() >= 5)
                        break;
                }
            }
            sink(rows, true);
        });
    };
    cmd::CommandRegistry::instance().register_provider(p);
}

// ── Data: connectors (data_connectors.json, read once off the GUI thread) ─────────

void ss_load_connectors() {
    QPointer<cmd::CommandRegistry> reg(&cmd::CommandRegistry::instance());
    QThreadPool::globalInstance()->start([reg]() {
        auto& catalog = services::connectors::ConnectorCatalog::instance();
        if (!catalog.ensure_loaded())
            return;
        QList<cmd::CommandItem> rows;
        for (const auto& c : catalog.connectors()) {
            if (c.name.isEmpty() || c.title.isEmpty())
                continue;
            cmd::CommandItem it;
            it.id = QStringLiteral("data.connector.") + c.name;
            it.title = c.title;
            it.context = ss_tr("Data source");
            it.keywords = {c.category, c.region, c.name, QStringLiteral("connector"), QStringLiteral("data")};
            it.group = QString::fromLatin1(cmd::groups::kData);
            it.subtitle = c.key_required ? ss_tr("Opens %1 in Data Sources (needs an API key)").arg(c.title)
                                         : ss_tr("Opens %1 in Data Sources").arg(c.title);
            it.screen_id = QStringLiteral("data_sources");
            it.section = QStringLiteral("connector:") + c.name;
            it.owner = QString::fromLatin1(kDataOwner);
            rows.append(it);
        }
        // register_items re-posts to the registry's (GUI) thread.
        if (reg)
            reg->register_items(rows);
    });
}

// ── Usage persistence ───────────────────────────────────────────────────────────

struct UsageState {
    cmd::CommandUsage usage;
    bool loaded = false;
    QTimer* save_timer = nullptr;
};

UsageState& ss_usage() {
    static UsageState s;
    return s;
}

void ss_save_usage_now() {
    const QByteArray json = QJsonDocument(ss_usage().usage.to_json()).toJson(QJsonDocument::Compact);
    auto r = SettingsRepository::instance().set(QString::fromLatin1(kUsageKey), QString::fromUtf8(json),
                                                QStringLiteral("search"));
    if (r.is_err())
        LOG_WARN(kTag, logf("Search history not saved", {{"error", QString::fromStdString(r.error())}}));
}

} // namespace

Notifier& Notifier::instance() {
    static Notifier* n = new Notifier();
    return *n;
}

void ensure_installed() {
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    QElapsedTimer t;
    t.start();
    auto& reg = cmd::CommandRegistry::instance();
    // Built-in rows never replace a row a screen registered first (same id = that screen's own row).
    QList<cmd::CommandItem> fresh;
    for (const cmd::CommandItem& it : cmd::seed::all())
        if (!reg.contains(it.id))
            fresh.append(it);
    reg.register_items(fresh);
    ss_install_bridge();
    ss_register_saved_provider();
    ss_register_notes_provider();
    ss_load_connectors();
    LOG_INFO(kTag, logf("Search sources installed", {{"rows", reg.size()}, {"ms", t.elapsed()}}));
}

void refresh_saved() {
    SavedState& s = ss_saved();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (s.loading || (s.loaded_ms > 0 && now - s.loaded_ms < kSavedFreshMs))
        return;
    s.loading = true;
    s.gui_rows = ss_load_gui_rows();
    QPointer<Notifier> notifier(&Notifier::instance());
    QThreadPool::globalInstance()->start([notifier]() {
        QList<cmd::CommandItem> rows = ss_load_worker_rows();
        if (!notifier)
            return;
        QMetaObject::invokeMethod(
            notifier.data(),
            [notifier, rows]() {
                SavedState& st = ss_saved();
                st.worker_rows = rows;
                st.loading = false;
                st.loaded_ms = QDateTime::currentMSecsSinceEpoch();
                if (notifier)
                    emit notifier->saved_changed();
            },
            Qt::QueuedConnection);
    });
}

const cmd::CommandIndex& index() {
    static cmd::CommandIndex idx;
    static quint64 rev = 0;
    auto& reg = cmd::CommandRegistry::instance();
    if (reg.revision() != rev) {
        QElapsedTimer t;
        t.start();
        idx.rebuild(reg.items());
        rev = reg.revision();
        LOG_DEBUG(kTag, logf("Search index rebuilt", {{"rows", idx.size()}, {"ms", t.elapsed()}}));
    }
    return idx;
}

cmd::CommandUsage& usage() {
    UsageState& s = ss_usage();
    if (!s.loaded) {
        s.loaded = true;
        const auto r = SettingsRepository::instance().get(QString::fromLatin1(kUsageKey));
        if (r.is_ok() && !r.value().isEmpty())
            s.usage = cmd::CommandUsage::from_json(QJsonDocument::fromJson(r.value().toUtf8()).object());
    }
    return s.usage;
}

void usage_changed() {
    UsageState& s = ss_usage();
    if (!s.save_timer) {
        s.save_timer = new QTimer(QCoreApplication::instance());
        s.save_timer->setSingleShot(true);
        s.save_timer->setInterval(1500);
        QObject::connect(s.save_timer, &QTimer::timeout, s.save_timer, []() { ss_save_usage_now(); });
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, s.save_timer, []() {
            UsageState& st = ss_usage();
            if (st.save_timer && st.save_timer->isActive()) {
                st.save_timer->stop();
                ss_save_usage_now();
            }
        });
    }
    s.save_timer->start();
}

} // namespace fincept::ui::search::sources
