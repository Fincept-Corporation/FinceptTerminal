// SearchController.cpp — see SearchController.h.
#include "ui/navigation/search/SearchController.h"

#include "core/command/CommandRank.h"
#include "core/command/CommandRegistry.h"
#include "core/command/CommandSeed.h"
#include "core/logging/Logger.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/navigation/search/SearchInstruments.h"
#include "ui/navigation/search/SearchSources.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonObject>

#include <algorithm>

namespace fincept::ui::search {

namespace {

namespace cmd = fincept::command;
namespace rk = fincept::command::rank;
namespace ph = fincept::command::phrase;
using fincept::core::instrument::AssetClass;
using fincept::core::instrument::Instrument;

constexpr const char* kTag = "CommandBar";
constexpr int kInstrumentDebounceMs = 90;
constexpr int kSlowProviderDebounceMs = 110;
constexpr int kEnterWaitMs = 1500;
constexpr int kInstrumentLimit = 12;
constexpr char kGroupPinned[] = "pinned";
constexpr char kGroupRecent[] = "recent";
constexpr char kGroupSuggested[] = "suggested";

QString sc_tr(const char* s) {
    return QCoreApplication::translate("CommandBar", s);
}

qint64 sc_now() {
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 sc_tier_score(rk::Tier tier, int quality = 999) {
    return static_cast<qint64>(tier) * rk::kTierWidth + quality + 2600; // above anything scored inside the tier
}

int sc_cap(const QString& group, bool scoped) {
    if (scoped)
        return 14;
    if (group == QLatin1String(cmd::groups::kInstrument) || group == QLatin1String(cmd::groups::kScreen))
        return 6;
    if (group == QLatin1String(cmd::groups::kAction) || group == QLatin1String(cmd::groups::kSaved))
        return 5;
    if (group == QLatin1String(cmd::groups::kSetting) || group == QLatin1String(cmd::groups::kData))
        return 4;
    return 3;
}

QString sc_chip(const cmd::CommandItem& item) {
    if (item.group == QLatin1String(cmd::groups::kScreen))
        return item.section.isEmpty() ? sc_tr("SCREEN") : sc_tr("TAB");
    if (item.group == QLatin1String(cmd::groups::kAction))
        return sc_tr("ACTION");
    if (item.group == QLatin1String(cmd::groups::kSetting))
        return sc_tr("SETTING");
    if (item.group == QLatin1String(cmd::groups::kSaved))
        return sc_tr("SAVED");
    if (item.group == QLatin1String(cmd::groups::kData))
        return sc_tr("DATA");
    if (item.group == QLatin1String(cmd::groups::kHelp))
        return sc_tr("HELP");
    return item.group.toUpper();
}

QString sc_subtitle(const cmd::CommandItem& item) {
    if (!item.subtitle.isEmpty() && item.group != QLatin1String(cmd::groups::kScreen))
        return item.subtitle;
    if (item.group == QLatin1String(cmd::groups::kScreen)) {
        if (item.section.isEmpty())
            return item.subtitle.isEmpty() ? sc_tr("Opens %1").arg(item.title)
                                           : sc_tr("Opens %1 — %2").arg(item.title, item.subtitle);
        return sc_tr("Opens %1 in %2").arg(item.title, item.context.isEmpty() ? cmd::seed::screen_title(item.screen_id)
                                                                              : item.context);
    }
    if (item.group == QLatin1String(cmd::groups::kSetting))
        return sc_tr("Opens Settings at %1").arg(item.title);
    if (item.group == QLatin1String(cmd::groups::kHelp) && item.screen_id == QLatin1String("docs"))
        return sc_tr("Opens this page in Docs");
    if (!item.screen_id.isEmpty())
        return sc_tr("Opens %1").arg(cmd::seed::screen_title(item.screen_id));
    return {};
}

/// The old /types searched through the resolver, with the classes each keeps (empty = MarketSearchService).
QVector<AssetClass> sc_classes_for_type(const QString& type) {
    if (type == QLatin1String("stock") || type == QLatin1String("dr"))
        return {AssetClass::Equity};
    if (type == QLatin1String("fund"))
        return {AssetClass::Etf, AssetClass::Fund};
    if (type == QLatin1String("index"))
        return {AssetClass::Index};
    if (type == QLatin1String("forex"))
        return {AssetClass::Fx};
    if (type == QLatin1String("crypto"))
        return {AssetClass::Crypto};
    if (type == QLatin1String("futures"))
        return {AssetClass::Future};
    return {};
}

QString sc_view_action(const QString& view) {
    namespace act = instruments::act;
    if (view == QLatin1String("chart"))
        return QString::fromLatin1(act::kChart);
    if (view == QLatin1String("news"))
        return QString::fromLatin1(act::kNews);
    if (view == QLatin1String("research"))
        return QString::fromLatin1(act::kResearch);
    if (view == QLatin1String("options"))
        return QString::fromLatin1(act::kOptions);
    if (view == QLatin1String("trade"))
        return QString::fromLatin1(act::kBuy);
    if (view == QLatin1String("backtest"))
        return QString::fromLatin1(act::kBacktest);
    if (view == QLatin1String("peers"))
        return QString::fromLatin1(act::kPeers);
    return {};
}

} // namespace

QStringList SearchController::default_group_order() {
    return {QString::fromLatin1(kGroupPinned),          QString::fromLatin1(kGroupRecent),
            QString::fromLatin1(cmd::groups::kInstrument), QString::fromLatin1(cmd::groups::kScreen),
            QString::fromLatin1(cmd::groups::kAction),  QString::fromLatin1(cmd::groups::kSetting),
            QString::fromLatin1(cmd::groups::kSaved),   QString::fromLatin1(cmd::groups::kData),
            QString::fromLatin1(cmd::groups::kHelp),    QString::fromLatin1(kGroupSuggested)};
}

QString SearchController::group_title(const QString& g) {
    if (g == QLatin1String(cmd::groups::kInstrument))
        return sc_tr("Instruments");
    if (g == QLatin1String(cmd::groups::kScreen))
        return sc_tr("Screens & tabs");
    if (g == QLatin1String(cmd::groups::kAction))
        return sc_tr("Actions");
    if (g == QLatin1String(cmd::groups::kSetting))
        return sc_tr("Settings");
    if (g == QLatin1String(cmd::groups::kSaved))
        return sc_tr("Your items");
    if (g == QLatin1String(cmd::groups::kData))
        return sc_tr("Data");
    if (g == QLatin1String(cmd::groups::kHelp))
        return sc_tr("Help");
    if (g == QLatin1String(kGroupPinned))
        return sc_tr("Pinned");
    if (g == QLatin1String(kGroupRecent))
        return sc_tr("Recent");
    if (g == QLatin1String(kGroupSuggested))
        return sc_tr("Try");
    return g;
}

SearchController::SearchController(QObject* parent) : QObject(parent) {
    sources::ensure_installed();

    slow_timer_ = new QTimer(this);
    slow_timer_->setSingleShot(true);
    slow_timer_->setInterval(kSlowProviderDebounceMs);
    connect(slow_timer_, &QTimer::timeout, this, [this]() { start_providers(true); });

    inst_timer_ = new QTimer(this);
    inst_timer_->setSingleShot(true);
    inst_timer_->setInterval(kInstrumentDebounceMs);
    connect(inst_timer_, &QTimer::timeout, this, &SearchController::fire_instruments);

    pending_timer_ = new QTimer(this);
    pending_timer_->setSingleShot(true);
    pending_timer_->setInterval(kEnterWaitMs);
    connect(pending_timer_, &QTimer::timeout, this, &SearchController::release_pending_enter);

    connect(&cmd::CommandRegistry::instance(), &cmd::CommandRegistry::changed, this, [this]() {
        if (!text_.trimmed().isEmpty())
            QTimer::singleShot(0, this, &SearchController::refresh); // coalesce a burst of registrations
    });
    connect(&sources::Notifier::instance(), &sources::Notifier::saved_changed, this, [this]() {
        if (!text_.trimmed().isEmpty())
            start_providers(false); // the snapshot provider answers again from the new snapshot
    });
}

SearchController::~SearchController() {
    cancel_all();
}

actions::Ctx SearchController::ctx(bool new_panel) const {
    actions::Ctx c;
    c.window = window_;
    c.new_panel = new_panel;
    return c;
}

void SearchController::cancel_all() {
    if (provider_request_)
        cmd::CommandRegistry::instance().cancel(provider_request_);
    provider_request_ = 0;
    provider_pending_.clear();
    slow_timer_->stop();
    inst_timer_->stop();
    ++inst_serial_; // a late resolver answer is dropped
    inst_waiting_ = false;
    pending_active_ = false;
    pending_timer_->stop();
}

// ── Query ─────────────────────────────────────────────────────────────────────

void SearchController::set_query(const QString& text) {
    const bool changed = text != text_;
    text_ = text;
    if (changed) {
        expanded_.clear();
        previous_ = {};
        same_query_ = false;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        cancel_all();
        provider_rows_.clear();
        inst_rows_.clear();
        legacy_rows_.clear();
        inst_wanted_ = false;
        build_empty_state();
        return;
    }
    if (!changed && !shown_.isEmpty()) {
        refresh();
        return;
    }

    ph::Vocabulary vocab;
    vocab.screen_for = [](const QString& token) { return sources::index().screen_for(token); };
    phrases_ = ph::parse(trimmed, vocab);
    scope_.clear();
    local_text_ = trimmed;
    for (const ph::Phrase& p : phrases_) {
        if (p.kind == ph::Kind::Scope) {
            scope_ = p.scope;
            local_text_ = p.text;
        } else if (p.kind == ph::Kind::Settings || p.kind == ph::Kind::Open) {
            local_text_ = p.text;
        }
        break; // the strongest reading decides the local text
    }
    query_ = rk::prepare_query(local_text_);

    plan_instruments();
    provider_rows_.clear();
    start_providers(false);
    slow_timer_->start();
    rebuild();
}

void SearchController::rerun() {
    const QString t = text_;
    text_ = QString(QChar(0x1)); // never typed: the next set_query() is a change
    set_query(t);
}

void SearchController::refresh() {
    if (text_.trimmed().isEmpty()) {
        build_empty_state();
        return;
    }
    same_query_ = true;
    rebuild();
}

void SearchController::start_providers(bool include_slow) {
    auto& reg = cmd::CommandRegistry::instance();
    if (!include_slow && provider_request_)
        reg.cancel(provider_request_);
    if (scope_ == QLatin1String("instrument") || local_text_.trimmed().isEmpty())
        return;
    QStringList asked;
    QPointer<SearchController> self(this);
    const QString answered_text = text_;
    const quint64 id = reg.query_providers(
        local_text_, this,
        [self, answered_text](quint64, const QString& pid, const QString& group, const QList<cmd::CommandItem>& rows,
                              bool final) {
            if (!self || self->text_ != answered_text)
                return;
            self->provider_rows_.insert(pid, qMakePair(group, rows));
            if (final)
                self->provider_pending_.remove(pid);
            self->same_query_ = true;
            self->rebuild();
        },
        include_slow, &asked);
    if (!include_slow)
        provider_request_ = id;
    for (const QString& pid : asked)
        provider_pending_.insert(pid);
}

void SearchController::plan_instruments() {
    inst_timer_->stop();
    ++inst_serial_;
    inst_rows_.clear();
    legacy_rows_.clear();
    inst_wanted_ = false;
    inst_waiting_ = false;
    inst_offline_ = false;
    inst_action_.clear();
    inst_args_.clear();
    inst_classes_.clear();
    legacy_type_.clear();
    inst_text_.clear();
    symbol_intent_ = false;

    const QString trimmed = text_.trimmed();
    if (scope_ == QLatin1String("action") || scope_ == QLatin1String("help"))
        return;
    const ph::Phrase* p = phrases_.isEmpty() ? nullptr : &phrases_.first();
    namespace act = instruments::act;
    if (p) {
        switch (p->kind) {
            case ph::Kind::Ticket:
                inst_text_ = p->symbols.value(0);
                inst_action_ = p->side == QLatin1String("sell") ? QString::fromLatin1(act::kSell) : QString::fromLatin1(act::kBuy);
                if (p->quantity > 0)
                    inst_args_.insert(QStringLiteral("quantity"), p->quantity);
                if (p->price > 0)
                    inst_args_.insert(QStringLiteral("limit_price"), p->price);
                if (!p->order_type.isEmpty())
                    inst_args_.insert(QStringLiteral("order_type"), p->order_type);
                break;
            case ph::Kind::SymbolView:
                inst_text_ = p->symbols.value(0);
                inst_action_ = sc_view_action(p->view);
                break;
            case ph::Kind::Compare:
                inst_text_ = p->symbols.value(0);
                inst_action_ = QString::fromLatin1(act::kPeers);
                inst_args_.insert(QStringLiteral("peers"), QStringList(p->symbols.mid(1)));
                break;
            case ph::Kind::FxPair:
                inst_text_ = p->symbols.value(0);
                inst_classes_ = {AssetClass::Fx};
                break;
            case ph::Kind::Crypto:
                inst_text_ = p->symbols.value(0);
                inst_classes_ = {AssetClass::Crypto};
                break;
            case ph::Kind::Backtest:
                inst_text_ = p->symbols.value(0);
                inst_action_ = QString::fromLatin1(act::kBacktest);
                if (!p->text.isEmpty())
                    inst_args_.insert(QStringLiteral("strategy"), p->text);
                break;
            case ph::Kind::Watchlist:
                inst_text_ = p->symbols.value(0);
                inst_action_ = QString::fromLatin1(act::kWatch);
                if (!p->text.isEmpty()) {
                    inst_args_.insert(QStringLiteral("watchlist"), p->text);
                    inst_args_.insert(QStringLiteral("create_watchlist"), true);
                }
                break;
            case ph::Kind::AssetType: {
                inst_text_ = p->text;
                inst_classes_ = sc_classes_for_type(p->target);
                if (inst_classes_.isEmpty() && (p->target == QLatin1String("bond") || p->target == QLatin1String("economic")))
                    legacy_type_ = p->target;
                break;
            }
            case ph::Kind::Scope:
                if (p->scope == QLatin1String("instrument"))
                    inst_text_ = p->text;
                break;
            case ph::Kind::Open:
                if (ph::is_ticker_like(p->text))
                    inst_text_ = p->text;
                break;
            case ph::Kind::Theme:
            case ph::Kind::Settings:
            case ph::Kind::Ask:
            case ph::Kind::Dock:
                return;
            case ph::Kind::None:
                break;
        }
    }
    if (!p) {
        // Plain text: one or two words search symbols and names too; a sentence does not.
        const int words = static_cast<int>(query_.words.size());
        if (trimmed.size() <= 40 && words >= 1 && words <= 3)
            inst_text_ = trimmed;
    }
    if (inst_text_.trimmed().isEmpty())
        return;
    symbol_intent_ = p && p->kind != ph::Kind::Open && p->kind != ph::Kind::None;
    inst_wanted_ = true;
    inst_waiting_ = true;
    inst_timer_->start();
}

void SearchController::fire_instruments() {
    if (!inst_wanted_)
        return;
    if (!legacy_type_.isEmpty()) {
        fire_legacy_market();
        return;
    }
    namespace sv = fincept::services;
    sv::ResolveOptions opts;
    opts.limit = kInstrumentLimit;
    opts.ctx.allowed_classes = inst_classes_;
    const quint64 serial = inst_serial_;
    const QString answered_text = text_;
    inst_clock_.start();
    QPointer<SearchController> self(this);
    sv::InstrumentResolver::instance().search(inst_text_, opts, this, [self, serial, answered_text](const sv::ResolveResult& res) {
        if (!self || serial != self->inst_serial_ || self->text_ != answered_text)
            return; // the user moved on
        self->inst_rows_.clear();
        for (const Instrument& inst : res.matches)
            if (!instruments::open_route(inst).symbol.isEmpty())
                self->inst_rows_.append(inst);
        self->inst_offline_ = res.offline;
        if (res.final || !self->inst_rows_.isEmpty())
            self->inst_waiting_ = false;
        if (res.final)
            LOG_DEBUG(kTag, logf("Symbol search answered", {{"rows", self->inst_rows_.size()},
                                                            {"ms", self->inst_clock_.elapsed()},
                                                            {"remote", res.from_remote}, {"offline", res.offline}}));
        self->same_query_ = true;
        self->rebuild();
        self->release_pending_enter();
    });
}

void SearchController::fire_legacy_market() {
    using services::MarketSearchService;
    auto& svc = MarketSearchService::instance();
    const QString request = QStringLiteral("commandbar/%1/%2").arg(reinterpret_cast<quintptr>(this), 0, 16).arg(inst_serial_);
    if (!legacy_wired_) {
        legacy_wired_ = true;
        connect(&svc, &MarketSearchService::results_ready, this,
                [this](const QString& request_id, const QString&, const QList<MarketSearchService::Item>& items) {
                    if (request_id != QStringLiteral("commandbar/%1/%2").arg(reinterpret_cast<quintptr>(this), 0, 16).arg(inst_serial_))
                        return;
                    legacy_rows_ = items;
                    inst_waiting_ = false;
                    same_query_ = true;
                    rebuild();
                    release_pending_enter();
                });
        connect(&svc, &MarketSearchService::search_failed, this,
                [this](const QString& request_id, const QString&, const QString&) {
                    if (request_id != QStringLiteral("commandbar/%1/%2").arg(reinterpret_cast<quintptr>(this), 0, 16).arg(inst_serial_))
                        return;
                    legacy_rows_.clear();
                    inst_waiting_ = false;
                    inst_offline_ = true;
                    same_query_ = true;
                    rebuild();
                    release_pending_enter();
                });
    }
    svc.search(inst_text_, legacy_type_, 10, request);
}

// ── Collecting candidates ─────────────────────────────────────────────────────────

SearchController::Cand SearchController::item_cand(const cmd::CommandItem& item, const rk::Match& m, qint64 score) const {
    Cand c;
    c.row.kind = SearchRow::Kind::Item;
    c.row.key = QStringLiteral("i:") + item.id;
    c.row.group = item.group;
    c.row.title = item.title;
    c.row.context = item.context;
    c.row.subtitle = sc_subtitle(item);
    c.row.chip = sc_chip(item);
    c.row.hint = item.shortcut;
    c.row.highlight = m.highlight;
    c.row.agent_risk = item.agent_risk;
    c.row.pinned = sources::usage().is_pinned(item.id);
    c.entry.kind = Entry::Kind::Item;
    c.entry.item = item;
    if (!item.screen_id.isEmpty() && !item.run) {
        c.row.actions << sc_tr("New panel");
        c.entry.actions << QStringLiteral("new_panel");
    }
    c.score = score;
    return c;
}

void SearchController::collect_local(const QString& text, const QSet<QString>& only_groups,
                                     QHash<QString, QList<Cand>>* out) const {
    const rk::Query q = rk::prepare_query(text);
    if (q.empty())
        return;
    const cmd::CommandIndex& idx = sources::index();
    const auto hits = idx.query(q, sources::usage(), sc_now(), only_groups, 120);
    for (const cmd::CommandIndex::Hit& h : hits) {
        const cmd::CommandItem& item = idx.entry(h.entry).item;
        (*out)[item.group].append(item_cand(item, h.match, h.score));
    }
}

void SearchController::collect_phrases(QHash<QString, QList<Cand>>* out) const {
    if (phrases_.isEmpty())
        return;
    const ph::Phrase& p = phrases_.first();
    const cmd::CommandIndex& idx = sources::index();
    switch (p.kind) {
        case ph::Kind::Theme: {
            const int i = idx.find(QStringLiteral("theme.") + p.target);
            if (i >= 0) {
                rk::Match m;
                m.tier = rk::Tier::Exact;
                (*out)[QString::fromLatin1(cmd::groups::kAction)].append(
                    item_cand(idx.entry(i).item, m, sc_tier_score(rk::Tier::Exact)));
            }
            break;
        }
        case ph::Kind::Dock: {
            Cand c;
            c.row.kind = SearchRow::Kind::Item;
            c.row.key = QStringLiteral("d:") + p.verb + QLatin1Char(':') + p.target + QLatin1Char(':') + p.text;
            c.row.group = QString::fromLatin1(cmd::groups::kAction);
            c.row.chip = sc_tr("LAYOUT");
            const QString a = cmd::seed::screen_title(p.target).isEmpty() ? p.target : cmd::seed::screen_title(p.target);
            const QString b = cmd::seed::screen_title(p.text).isEmpty() ? p.text : cmd::seed::screen_title(p.text);
            if (p.verb == QLatin1String("add")) {
                c.row.title = sc_tr("Open %1 beside %2").arg(b, a);
                c.row.subtitle = sc_tr("Adds %1 next to %2 in this window").arg(b, a);
            } else if (p.verb == QLatin1String("replace")) {
                c.row.title = sc_tr("Replace %1 with %2").arg(a, b);
                c.row.subtitle = sc_tr("Closes %1 and opens %2 in its place").arg(a, b);
            } else {
                c.row.title = sc_tr("Keep only %1").arg(a);
                c.row.subtitle = sc_tr("Closes every other panel in this window");
            }
            c.entry.kind = Entry::Kind::Dock;
            c.entry.text = p.verb;
            c.entry.a = p.target;
            c.entry.b = p.text;
            c.score = sc_tier_score(rk::Tier::Exact);
            (*out)[c.row.group].append(c);
            break;
        }
        case ph::Kind::Ask: {
            Cand c;
            c.row.kind = SearchRow::Kind::Ask;
            c.row.key = QStringLiteral("a:ask");
            c.row.group = QString::fromLatin1(cmd::groups::kHelp);
            c.row.chip = sc_tr("AI");
            c.row.lead = sc_tr("Ask AI:");
            c.row.title = p.text;
            c.row.subtitle = sc_tr("Opens AI Chat with this question typed in — you press Send");
            c.row.agent_risk = QString::fromLatin1(cmd::risk::kSpend);
            c.entry.kind = Entry::Kind::Ask;
            c.entry.text = p.text;
            c.score = sc_tier_score(rk::Tier::Exact);
            (*out)[c.row.group].append(c);
            break;
        }
        default:
            break;
    }
}

void SearchController::collect_providers(QHash<QString, QList<Cand>>* out) const {
    for (auto it = provider_rows_.constBegin(); it != provider_rows_.constEnd(); ++it) {
        for (const cmd::CommandItem& item : it.value().second) {
            rk::Fields f;
            f.title = item.title;
            f.codes = item.aliases;
            f.keywords = item.keywords;
            if (!item.context.isEmpty())
                f.keywords.append(item.context);
            f.description = item.subtitle;
            rk::Match m = rk::match(query_, rk::prepare(f));
            if (!m.matched()) {
                m.tier = rk::Tier::Description; // the provider found it its own way
                m.quality = 100;
            }
            const qint64 score = rk::score(m, rk::usage_boost(sources::usage().usage(item.id), sc_now()), item.priority);
            cmd::CommandItem copy = item;
            if (copy.group.isEmpty())
                copy.group = it.value().first;
            (*out)[copy.group].append(item_cand(copy, m, score));
        }
    }
}

void SearchController::collect_instruments(QHash<QString, QList<Cand>>* out, qint64* best) const {
    *best = 0;
    const QString group = QString::fromLatin1(cmd::groups::kInstrument);
    const rk::Query q = rk::prepare_query(inst_text_);
    QList<Cand> rows;
    qint64 top = 0;
    for (const Instrument& inst : inst_rows_) {
        rk::Fields f;
        f.title = inst.name.isEmpty() ? inst.display : inst.name;
        f.codes = {inst.display, inst.symbol, inst.isin};
        const int dot = static_cast<int>(inst.symbol.indexOf(QLatin1Char('.')));
        if (dot > 0)
            f.codes.append(inst.symbol.left(dot));
        rk::Match m = rk::match(q, rk::prepare(f));
        if (!m.matched()) {
            m.tier = rk::Tier::Prefix; // the resolver matched it (ISIN, former symbol, alias)
            m.quality = 0;
        }
        const QString usage_id = QStringLiteral("instrument:") + (inst.id.isEmpty() ? inst.symbol : inst.id);
        qint64 score = rk::score(m, rk::usage_boost(sources::usage().usage(usage_id), sc_now()), 0);
        // Typed in capitals and exactly a ticker: the symbol beats a screen alias of the same letters ("DASH", "MA").
        if (query_.all_caps && m.tier == rk::Tier::Exact)
            score += rk::kTierWidth / 2;
        top = std::max(top, score);

        Cand c;
        c.row.kind = SearchRow::Kind::Item;
        c.row.key = QStringLiteral("s:") + (inst.id.isEmpty() ? inst.symbol : inst.id);
        c.row.group = group;
        c.row.lead = inst.display.isEmpty() ? inst.symbol : inst.display;
        c.row.title = inst.name.isEmpty() ? QString() : inst.name;
        c.row.highlight = m.on_code ? QList<rk::Span>{} : m.highlight;
        c.row.chip = instruments::chip(inst);
        const QString facts = instruments::facts(inst);
        const QString what = instruments::sentence(inst, inst_action_, inst_args_);
        c.row.subtitle = facts.isEmpty() ? what : facts + QStringLiteral("  —  ") + what;
        c.row.agent_risk = instruments::risk(inst_action_);
        c.row.pinned = sources::usage().is_pinned(usage_id);
        const instruments::Route route = instruments::open_route(inst);
        c.row.symbol = route.symbol;
        c.row.symbol_class = route.asset_class;
        c.entry.kind = Entry::Kind::Instrument;
        c.entry.inst = inst;
        c.entry.default_action = inst_action_;
        c.entry.args = inst_args_;
        for (const QString& id : instruments::action_ids(inst)) {
            if (id == inst_action_)
                continue;
            c.entry.actions << id;
            c.row.actions << instruments::action_label(id);
        }
        rows.append(c);
    }
    // Inside the group the resolver's order stands (home market first, primary listing …).
    for (int i = 0; i < rows.size(); ++i)
        rows[i].score = top - i;
    if (!rows.isEmpty())
        (*out)[group] += rows;
    *best = top;

    // The old /bond and /economic searches (MarketSearchService).
    int i = 0;
    for (const auto& it : legacy_rows_) {
        if (it.symbol.isEmpty())
            continue;
        Cand c;
        c.row.kind = SearchRow::Kind::Item;
        c.row.key = QStringLiteral("x:") + it.symbol + QLatin1Char(':') + it.exchange;
        c.row.group = group;
        c.row.lead = it.symbol;
        c.row.title = it.name;
        c.row.chip = legacy_type_ == QLatin1String("economic") ? sc_tr("ECON") : sc_tr("BOND");
        c.row.subtitle = legacy_type_ == QLatin1String("economic") ? sc_tr("Opens Economics")
                                                                   : sc_tr("Opens %1 in Equity Research").arg(it.name);
        Instrument inst;
        inst.symbol = it.yf_symbol.isEmpty() ? it.symbol : it.yf_symbol;
        inst.display = it.symbol;
        inst.name = it.name;
        inst.venue = it.exchange;
        inst.asset_class = AssetClass::Bond;
        c.entry.kind = Entry::Kind::Instrument;
        c.entry.inst = inst;
        c.entry.text = legacy_type_; // economic rows open the Economics screen
        c.score = sc_tier_score(rk::Tier::Prefix, 0) - (i++);
        (*out)[group].append(c);
        *best = std::max(*best, c.score);
    }
}

// ── Layout ──────────────────────────────────────────────────────────────────────

QString SearchController::first_selectable_key(const QList<SearchRow>& rows) const {
    for (const SearchRow& r : rows)
        if (r.kind == SearchRow::Kind::Item || r.kind == SearchRow::Kind::Ask)
            return r.key;
    for (const SearchRow& r : rows)
        if (r.selectable())
            return r.key;
    return {};
}

void SearchController::rebuild() {
    QHash<QString, QList<Cand>> by_group;
    QSet<QString> only;
    if (scope_ == QLatin1String("action"))
        only = {QString::fromLatin1(cmd::groups::kAction)};
    else if (scope_ == QLatin1String("help"))
        only = {QString::fromLatin1(cmd::groups::kHelp)};
    const bool instruments_only = scope_ == QLatin1String("instrument");
    const ph::Kind kind = phrases_.isEmpty() ? ph::Kind::None : phrases_.first().kind;
    if (kind == ph::Kind::Settings)
        only = {QString::fromLatin1(cmd::groups::kSetting)};

    if (!instruments_only && kind != ph::Kind::Ticket && kind != ph::Kind::Ask)
        collect_local(local_text_, only, &by_group);
    collect_phrases(&by_group);
    if (!instruments_only && only.isEmpty())
        collect_providers(&by_group);
    qint64 inst_best = 0;
    collect_instruments(&by_group, &inst_best);

    // Local best (for the instrument slot while it loads).
    qint64 local_best = 0;
    for (auto it = by_group.constBegin(); it != by_group.constEnd(); ++it)
        if (it.key() != QLatin1String(cmd::groups::kInstrument))
            for (const Cand& c : it.value())
                local_best = std::max(local_best, c.score);

    local_exact_ = rk::tier_of(local_best) >= rk::Tier::Exact;

    // Dedupe keys across groups (a provider row with a seeded id) — keep the best score.
    QHash<QString, Cand> by_key;
    for (auto it = by_group.constBegin(); it != by_group.constEnd(); ++it) {
        for (const Cand& c : it.value()) {
            auto found = by_key.find(c.row.key);
            if (found == by_key.end() || found->score < c.score)
                by_key.insert(c.row.key, c);
        }
    }

    QList<cmd::layout::GroupInput> inputs;
    QHash<QString, QList<cmd::layout::Candidate>> cands;
    for (auto it = by_key.constBegin(); it != by_key.constEnd(); ++it)
        cands[it->row.group].append({it.key(), it->score});
    QStringList groups = default_group_order();
    for (const QString& g : cands.keys())
        if (!groups.contains(g))
            groups.append(g);
    const bool scoped = !scope_.isEmpty() || !only.isEmpty();
    for (const QString& g : groups) {
        cmd::layout::GroupInput in;
        in.group = g;
        in.rows = cands.value(g);
        in.cap = sc_cap(g, scoped);
        in.expanded = expanded_.contains(g);
        if (g == QLatin1String(cmd::groups::kInstrument)) {
            // A loading row only where symbols are likely the answer: an exact local hit ("backtest", "bt")
            // keeps the panel calm — symbols that do turn up are added after the rows already shown.
            in.loading = inst_wanted_ && inst_waiting_ && in.rows.isEmpty() && (symbol_intent_ || !local_exact_);
            const bool ticker = query_.words.size() == 1 && ph::is_ticker_like(text_.trimmed());
            if (ticker && rk::tier_of(local_best) < rk::Tier::Exact)
                in.predicted = sc_tier_score(rk::Tier::Exact);
            else if (ticker)
                in.predicted = local_best - 1;
            else
                in.predicted = static_cast<qint64>(rk::Tier::WordsAnyOrder) * rk::kTierWidth;
        } else if (g == QLatin1String(cmd::groups::kSaved) && in.rows.isEmpty()) {
            in.loading = false; // saved items never show a loading row (they are local and quick)
        }
        if (!in.rows.isEmpty() || in.loading)
            inputs.append(in);
    }
    previous_.same_query = same_query_;
    const QList<cmd::layout::Section> sections = cmd::layout::arrange(inputs, default_group_order(), previous_);

    QList<SearchRow> rows;
    entries_.clear();
    command::layout::Previous next;
    next.same_query = true;
    qint64 best_shown = 0;
    for (const cmd::layout::Section& s : sections) {
        SearchRow h;
        h.kind = SearchRow::Kind::Heading;
        h.key = QStringLiteral("h:") + s.group;
        h.group = s.group;
        h.title = group_title(s.group);
        h.count = s.total;
        rows.append(h);
        next.group_order.append(s.group);
        for (const QString& key : s.keys) {
            const Cand& c = by_key[key];
            rows.append(c.row);
            entries_.insert(key, c.entry);
            best_shown = std::max(best_shown, c.score);
            next.keys[s.group].append(key);
        }
        if (s.loading && s.keys.isEmpty()) {
            SearchRow l;
            l.kind = SearchRow::Kind::Loading;
            l.key = QStringLiteral("l:") + s.group;
            l.group = s.group;
            l.title = s.group == QLatin1String(cmd::groups::kInstrument) ? sc_tr("Searching symbols…") : sc_tr("Searching…");
            rows.append(l);
        }
        if (s.more) {
            SearchRow m;
            m.kind = SearchRow::Kind::More;
            m.key = QStringLiteral("m:") + s.group;
            m.group = s.group;
            m.title = sc_tr("Show all %1 %2").arg(s.total).arg(group_title(s.group).toLower());
            rows.append(m);
            Entry e;
            e.kind = Entry::Kind::More;
            e.text = s.group;
            entries_.insert(m.key, e);
        }
    }

    // Nothing strong: an explicit last row offers AI Chat (never automatic; nothing is sent).
    const bool asked = kind == ph::Kind::Ask;
    const bool weak = rk::tier_of(best_shown) < rk::Tier::WordsInOrder && !(inst_wanted_ && inst_waiting_);
    if (!asked && (weak || scope_ == QLatin1String("help")) && local_text_.trimmed().size() >= 3 &&
        kind != ph::Kind::Ticket) {
        SearchRow a;
        a.kind = SearchRow::Kind::Ask;
        a.key = QStringLiteral("a:ask");
        a.group = QString::fromLatin1(cmd::groups::kHelp);
        a.chip = sc_tr("AI");
        a.lead = sc_tr("Ask AI:");
        a.title = local_text_.trimmed();
        a.subtitle = sc_tr("Opens AI Chat with this question typed in — you press Send");
        a.agent_risk = QString::fromLatin1(cmd::risk::kSpend);
        rows.append(a);
        Entry e;
        e.kind = Entry::Kind::Ask;
        e.text = local_text_.trimmed();
        entries_.insert(a.key, e);
    }
    if (rows.isEmpty()) {
        SearchRow n;
        n.kind = SearchRow::Kind::Hint;
        n.key = QStringLiteral("n:none");
        n.title = inst_offline_ ? sc_tr("No matches — symbol search is offline right now")
                                : sc_tr("No matches. Try a ticker (AAPL), a screen (portfolio) or what to do (dark theme)");
        rows.append(n);
    }
    previous_ = next;
    shown_ = rows;
    emit rows_changed(rows, first_selectable_key(rows));
}

void SearchController::build_empty_state() {
    QList<SearchRow> rows;
    entries_.clear();
    auto& usage = sources::usage();
    const cmd::CommandIndex& idx = sources::index();
    auto add_heading = [&rows](const QString& group, int count) {
        SearchRow h;
        h.kind = SearchRow::Kind::Heading;
        h.key = QStringLiteral("h:") + group;
        h.group = group;
        h.title = group_title(group);
        h.count = count;
        rows.append(h);
    };
    auto add_usage_row = [&](const cmd::CommandUsage::Entry& e, const QString& group) {
        SearchRow r;
        r.kind = SearchRow::Kind::Item;
        r.key = QStringLiteral("r:") + group + QLatin1Char(':') + e.id;
        r.group = group;
        r.title = e.title;
        r.subtitle = e.subtitle;
        r.pinned = e.pinned;
        r.lead = e.payload.value(QStringLiteral("lead")).toString();
        r.context = e.payload.value(QStringLiteral("context")).toString();
        if (e.group == QLatin1String(cmd::groups::kInstrument)) {
            const Instrument inst = Instrument::from_json(QJsonObject::fromVariantMap(e.payload.value(QStringLiteral("inst")).toMap()));
            r.chip = instruments::chip(inst);
        } else {
            const int i = idx.find(e.id);
            r.chip = i >= 0 ? sc_chip(idx.entry(i).item) : e.group.toUpper();
            if (i >= 0)
                r.agent_risk = idx.entry(i).item.agent_risk;
        }
        Entry en;
        en.kind = Entry::Kind::Recent;
        en.recent_id = e.id;
        en.text = e.group;
        rows.append(r);
        entries_.insert(r.key, en);
    };
    const auto pinned = usage.pinned();
    if (!pinned.isEmpty()) {
        add_heading(QString::fromLatin1(kGroupPinned), static_cast<int>(pinned.size()));
        for (const auto& e : pinned)
            add_usage_row(e, QString::fromLatin1(kGroupPinned));
    }
    const auto recent = usage.recent(6);
    if (!recent.isEmpty()) {
        add_heading(QString::fromLatin1(kGroupRecent), static_cast<int>(recent.size()));
        for (const auto& e : recent)
            add_usage_row(e, QString::fromLatin1(kGroupRecent));
    }
    if (recent.size() < 3) {
        struct Suggestion {
            const char* text;
            const char* what;
        };
        static const Suggestion kSuggestions[] = {
            {"AAPL", QT_TRANSLATE_NOOP("CommandBar", "See a stock: quote, chart, news")},
            {"backtest", QT_TRANSLATE_NOOP("CommandBar", "Test a trading idea on history")},
            {"dark theme", QT_TRANSLATE_NOOP("CommandBar", "Change how the terminal looks")},
            {"connect broker", QT_TRANSLATE_NOOP("CommandBar", "Trade with your own account")},
            {"buy 10 msft", QT_TRANSLATE_NOOP("CommandBar", "Opens an order ticket to check — nothing is sent")},
        };
        add_heading(QString::fromLatin1(kGroupSuggested), 0);
        for (const Suggestion& s : kSuggestions) {
            SearchRow r;
            r.kind = SearchRow::Kind::Item;
            r.key = QStringLiteral("t:") + QString::fromLatin1(s.text);
            r.group = QString::fromLatin1(kGroupSuggested);
            r.lead = QString::fromLatin1(s.text);
            r.subtitle = QCoreApplication::translate("CommandBar", s.what);
            r.chip = sc_tr("TRY");
            r.hint = sc_tr("Enter types it");
            rows.append(r);
            Entry e;
            e.kind = Entry::Kind::Suggestion;
            e.text = QString::fromLatin1(s.text);
            entries_.insert(r.key, e);
        }
    }
    shown_ = rows;
    previous_ = {};
    emit rows_changed(rows, first_selectable_key(rows));
}

// ── Activation ──────────────────────────────────────────────────────────────────

void SearchController::accept(const QString& current_key, Qt::KeyboardModifiers modifiers, bool user_moved) {
    // Enter beat the symbol lookup on a ticker-shaped query: wait for it rather than open a weaker local match.
    const bool ticker = query_.words.size() == 1 && ph::is_ticker_like(text_.trimmed());
    if (!user_moved && inst_wanted_ && inst_waiting_ && (ticker || symbol_intent_)) {
        // An exact local hit ("bt", "markets", "ai") opens at once; a symbol phrase ("buy 10 msft") always waits.
        if (!local_exact_ || symbol_intent_) {
            pending_active_ = true;
            pending_mods_ = modifiers;
            pending_timer_->start();
            if (inst_timer_->isActive()) {
                inst_timer_->stop();
                fire_instruments(); // no need to wait for the debounce
            }
            emit status(sc_tr("Looking up %1…").arg(inst_text_.trimmed().toUpper()));
            return;
        }
    }
    if (current_key.isEmpty())
        return;
    activate(current_key, -1, modifiers);
}

void SearchController::release_pending_enter() {
    if (!pending_active_)
        return;
    if (inst_waiting_ && pending_timer_->isActive())
        return; // still nothing: keep waiting until the timeout
    pending_active_ = false;
    pending_timer_->stop();
    const QString key = first_selectable_key(shown_);
    if (!key.isEmpty() && !key.startsWith(QLatin1String("n:")))
        activate(key, -1, pending_mods_);
    else
        emit status(sc_tr("No symbol found for %1").arg(inst_text_.trimmed().toUpper()));
}

QStringList SearchController::menu_actions(const QString& key) const {
    const auto it = entries_.constFind(key);
    if (it == entries_.constEnd())
        return {};
    QStringList labels;
    const Entry& e = *it;
    if (e.kind == Entry::Kind::Instrument) {
        labels << instruments::action_label(e.default_action.isEmpty() ? QString::fromLatin1(instruments::act::kOpen)
                                                                        : e.default_action);
        for (const QString& id : e.actions)
            labels << instruments::action_label(id);
    } else if (e.kind == Entry::Kind::Item) {
        labels << sc_tr("Open");
        if (!e.item.screen_id.isEmpty() && !e.item.run)
            labels << sc_tr("Open in new panel");
    } else if (e.kind == Entry::Kind::Recent) {
        labels << sc_tr("Open");
    } else {
        return {};
    }
    QString usage_id;
    if (e.kind == Entry::Kind::Instrument)
        usage_id = QStringLiteral("instrument:") + (e.inst.id.isEmpty() ? e.inst.symbol : e.inst.id);
    else if (e.kind == Entry::Kind::Item)
        usage_id = e.item.id;
    else
        usage_id = e.recent_id;
    labels << (sources::usage().is_pinned(usage_id) ? sc_tr("Unpin") : sc_tr("Pin to the top of the empty box"));
    if (e.kind == Entry::Kind::Recent && !sources::usage().is_pinned(usage_id))
        labels << sc_tr("Remove from recent");
    return labels;
}

QList<SearchController::MenuAction> SearchController::menu_entries(const QString& key) const {
    QList<MenuAction> out;
    const QStringList labels = menu_actions(key);
    const auto it = entries_.constFind(key);
    for (int i = 0; i < labels.size(); ++i) {
        MenuAction m;
        m.label = labels[i];
        if (it != entries_.constEnd() && it->kind == Entry::Kind::Instrument) {
            const QString id = i == 0 ? it->default_action : (i - 1 < it->actions.size() ? it->actions[i - 1] : QString());
            m.agent_risk = instruments::risk(id);
        } else if (it != entries_.constEnd() && it->kind == Entry::Kind::Item && i == 0) {
            m.agent_risk = it->item.agent_risk;
        }
        out.append(m);
    }
    return out;
}

void SearchController::activate_menu(const QString& key, int index, Qt::KeyboardModifiers modifiers) {
    const auto it = entries_.constFind(key);
    if (it == entries_.constEnd() || index < 0)
        return;
    const Entry e = *it;
    const QStringList labels = menu_actions(key);
    if (index >= labels.size())
        return;
    QString usage_id;
    cmd::CommandUsage::Entry ue;
    if (e.kind == Entry::Kind::Instrument) {
        usage_id = QStringLiteral("instrument:") + (e.inst.id.isEmpty() ? e.inst.symbol : e.inst.id);
        ue.title = e.inst.name.isEmpty() ? e.inst.display : e.inst.name;
        ue.group = QString::fromLatin1(cmd::groups::kInstrument);
        ue.subtitle = instruments::sentence(e.inst, QString(), {});
        ue.payload = {{QStringLiteral("inst"), e.inst.to_json().toVariantMap()},
                      {QStringLiteral("lead"), e.inst.display.isEmpty() ? e.inst.symbol : e.inst.display}};
    } else if (e.kind == Entry::Kind::Item) {
        usage_id = e.item.id;
        ue.title = e.item.title;
        ue.group = e.item.group;
        ue.subtitle = sc_subtitle(e.item);
        ue.payload = {{QStringLiteral("context"), e.item.context}};
    } else {
        usage_id = e.recent_id;
    }
    ue.id = usage_id;
    const QString label = labels[index];
    if (label == sc_tr("Unpin") || label == sc_tr("Pin to the top of the empty box")) {
        const bool pin = label != sc_tr("Unpin");
        if (!sources::usage().set_pinned(ue, pin, sc_now()))
            emit status(sc_tr("Up to %1 pins — unpin one first").arg(cmd::CommandUsage::kMaxPins));
        else
            emit status(pin ? sc_tr("Pinned — it shows when the box is empty") : sc_tr("Unpinned"));
        sources::usage_changed();
        refresh();
        return;
    }
    if (label == sc_tr("Remove from recent")) {
        sources::usage().forget(usage_id);
        sources::usage_changed();
        refresh();
        return;
    }
    if (e.kind == Entry::Kind::Instrument) {
        if (index == 0) {
            activate(key, -1, modifiers);
            return;
        }
        activate(key, index - 1, modifiers);
        return;
    }
    if (label == sc_tr("Open in new panel")) {
        activate(key, -1, modifiers | Qt::ControlModifier);
        return;
    }
    activate(key, -1, modifiers);
}

void SearchController::activate(const QString& key, int action, Qt::KeyboardModifiers modifiers) {
    const auto it = entries_.constFind(key);
    if (it == entries_.constEnd())
        return;
    const Entry e = *it; // a copy: running it may rebuild the rows
    const bool new_panel = modifiers.testFlag(Qt::ControlModifier);
    QString action_id;
    if (action >= 0 && action < e.actions.size())
        action_id = e.actions[action];
    run_entry(e, action_id, new_panel);
}

void SearchController::run_entry(const Entry& e, const QString& action_id, bool new_panel) {
    pending_active_ = false;
    pending_timer_->stop();
    const QString group = e.kind == Entry::Kind::Instrument ? QString::fromLatin1(cmd::groups::kInstrument) : e.item.group;
    switch (e.kind) {
        case Entry::Kind::Suggestion:
            emit fill_requested(e.text);
            return;
        case Entry::Kind::More:
            expanded_.insert(e.text);
            refresh();
            return;
        case Entry::Kind::Dock:
            emit dock_requested(e.text, e.a, e.b);
            LOG_INFO(kTag, logf("Search pick", {{"group", "layout"}, {"verb", e.text}}));
            emit finished();
            return;
        case Entry::Kind::Ask:
            actions::ask_ai(ctx(false), e.text);
            LOG_INFO(kTag, logf("Search pick", {{"group", "help"}, {"kind", "ask_ai"}}));
            emit finished();
            return;
        case Entry::Kind::Recent: {
            const auto& usage = sources::usage();
            cmd::CommandUsage::Entry found;
            bool ok = false;
            for (const auto& u : usage.pinned())
                if (u.id == e.recent_id) {
                    found = u;
                    ok = true;
                }
            if (!ok)
                for (const auto& u : usage.recent(cmd::CommandUsage::kMaxEntries))
                    if (u.id == e.recent_id) {
                        found = u;
                        ok = true;
                    }
            if (!ok)
                return;
            Entry real;
            if (found.group == QLatin1String(cmd::groups::kInstrument)) {
                real.kind = Entry::Kind::Instrument;
                real.inst = Instrument::from_json(QJsonObject::fromVariantMap(found.payload.value(QStringLiteral("inst")).toMap()));
                if (!real.inst.is_valid())
                    return;
            } else {
                const cmd::CommandIndex& idx = sources::index();
                const int i = idx.find(found.id);
                if (i < 0) {
                    emit status(sc_tr("“%1” is no longer available").arg(found.title));
                    return;
                }
                real.kind = Entry::Kind::Item;
                real.item = idx.entry(i).item;
            }
            run_entry(real, action_id, new_panel);
            return;
        }
        case Entry::Kind::Instrument: {
            const QString act = action_id.isEmpty() ? e.default_action : action_id;
            if (e.text == QLatin1String("economic")) {
                emit navigate_requested(QStringLiteral("economics"));
                emit finished();
                return;
            }
            const QString note = instruments::run(ctx(new_panel), e.inst, act, e.args);
            const bool ticket = act == QLatin1String(instruments::act::kBuy) || act == QLatin1String(instruments::act::kSell);
            if (!ticket)
                record(e);
            LOG_INFO(kTag, logf("Search pick", {{"group", group}, {"action", act.isEmpty() ? QStringLiteral("open") : act}}));
            const bool stays = act == QLatin1String(instruments::act::kWatch) || act == QLatin1String(instruments::act::kCopy) ||
                               act == QLatin1String(instruments::act::kPinHeader);
            if (!note.isEmpty())
                emit status(note);
            if (!stays && note.isEmpty())
                emit finished();
            return;
        }
        case Entry::Kind::Item: {
            const cmd::CommandItem& item = e.item;
            record(e);
            LOG_INFO(kTag, logf("Search pick", {{"group", group}, {"kind", item.run ? "run" : "navigate"}}));
            if (action_id == QLatin1String("new_panel"))
                new_panel = true;
            if (item.run) {
                cmd::CommandContext c;
                c.window = window_.data();
                c.item_id = item.id;
                c.query = text_;
                c.args = item.args;
                c.new_panel = new_panel;
                emit finished();
                item.run(c);
                return;
            }
            const QString action_ref = item.args.value(QStringLiteral("action_id")).toString();
            if (!action_ref.isEmpty()) {
                emit finished();
                QString error;
                if (!actions::invoke_action(ctx(new_panel), action_ref, item.args, &error))
                    LOG_DEBUG(kTag, logf("Search action failed", {{"action", action_ref}}));
                return;
            }
            const QString toolbar = item.args.value(QStringLiteral("toolbar_action")).toString();
            if (!toolbar.isEmpty()) {
                emit finished();
                emit toolbar_action_requested(toolbar);
                return;
            }
            const QString theme = item.args.value(QStringLiteral("theme")).toString();
            if (!theme.isEmpty()) {
                actions::apply_theme(theme);
                emit finished();
                return;
            }
            if (item.screen_id.isEmpty())
                return;
            emit finished();
            QVariantMap args = item.args;
            if (item.section.isEmpty() && item.focus.isEmpty() && args.isEmpty() && !new_panel)
                emit navigate_requested(item.screen_id); // the old bar's path: this window, replace the view
            else
                actions::open_screen(ctx(new_panel), item.screen_id, item.section, item.focus, args);
            return;
        }
    }
}

void SearchController::record(const Entry& e) const {
    cmd::CommandUsage::Entry u;
    if (e.kind == Entry::Kind::Instrument) {
        if (e.text == QLatin1String("economic") || e.text == QLatin1String("bond"))
            return;
        u.id = QStringLiteral("instrument:") + (e.inst.id.isEmpty() ? e.inst.symbol : e.inst.id);
        u.title = e.inst.name.isEmpty() ? e.inst.display : e.inst.name;
        u.group = QString::fromLatin1(cmd::groups::kInstrument);
        u.subtitle = instruments::sentence(e.inst, QString(), {});
        u.payload = {{QStringLiteral("inst"), e.inst.to_json().toVariantMap()},
                     {QStringLiteral("lead"), e.inst.display.isEmpty() ? e.inst.symbol : e.inst.display}};
        // The shared resolver learns the pick too (ranking everywhere).
        services::InstrumentResolver::instance().remember_pick(QStringLiteral("commandbar"), e.inst);
    } else if (e.kind == Entry::Kind::Item) {
        u.id = e.item.id;
        u.title = e.item.title;
        u.group = e.item.group;
        u.subtitle = sc_subtitle(e.item);
        if (!e.item.context.isEmpty())
            u.payload.insert(QStringLiteral("context"), e.item.context);
    } else {
        return;
    }
    sources::usage().record(u, sc_now());
    sources::usage_changed();
}

} // namespace fincept::ui::search
