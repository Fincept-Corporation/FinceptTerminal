// SearchActions.cpp — see SearchActions.h.
#include "ui/navigation/search/SearchActions.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionRegistry.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/symbol/SymbolRef.h"
#include "screens/common/IStatefulScreen.h"
#include "services/pushpins/PushpinService.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QTimer>

namespace fincept::ui::search::actions {

namespace {

constexpr const char* kTag = "CommandBar";

/// The AI Chat screen of `window` once nav.switch_screen materialised it (found by class, no header needed).
QWidget* sa_find_screen(QWidget* window, const char* class_name) {
    if (!window)
        return nullptr;
    const auto widgets = window->findChildren<QWidget*>();
    for (QWidget* w : widgets)
        if (w->inherits(class_name))
            return w;
    return nullptr;
}

void sa_prefill_ai(QPointer<QWidget> window, const QString& text, int attempts_left) {
    if (!window)
        return;
    if (QWidget* chat = sa_find_screen(window.data(), "fincept::screens::AiChatScreen")) {
        if (auto* stateful = dynamic_cast<fincept::screens::IStatefulScreen*>(chat)) {
            // restore_state({"draft"}) only fills the composer (AiChatScreen.cpp): nothing is sent.
            stateful->restore_state(QVariantMap{{QStringLiteral("draft"), text}});
            return;
        }
    }
    if (attempts_left > 0)
        QTimer::singleShot(60, window.data(), [window, text, attempts_left]() { sa_prefill_ai(window, text, attempts_left - 1); });
}

} // namespace

void open_screen(const Ctx& ctx, const QString& screen_id, const QString& section, const QString& focus,
                 const QVariantMap& args) {
    if (screen_id.isEmpty())
        return;
    QVariantMap payload = args;
    payload.insert(QStringLiteral("screen_id"), screen_id);
    if (!section.isEmpty())
        payload.insert(QStringLiteral("section"), section);
    if (!focus.isEmpty())
        payload.insert(QStringLiteral("focus"), focus);
    payload.insert(QStringLiteral("new_panel"), ctx.new_panel);
    payload.insert(QStringLiteral("exclusive"), !ctx.new_panel);
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"), payload);
}

void open_symbol(const Ctx& ctx, const QString& screen_id, const QString& symbol, const QString& asset_class,
                 const QString& exchange) {
    if (screen_id.isEmpty() || symbol.trimmed().isEmpty())
        return;
    QVariantMap payload{{QStringLiteral("screen_id"), screen_id},
                        {QStringLiteral("symbol"), symbol.trimmed()},
                        {QStringLiteral("asset_class"), asset_class.isEmpty() ? QStringLiteral("equity") : asset_class},
                        {QStringLiteral("exclusive"), !ctx.new_panel},
                        {QStringLiteral("new_panel"), ctx.new_panel}};
    if (!exchange.isEmpty())
        payload.insert(QStringLiteral("exchange"), exchange);
    EventBus::instance().publish(QStringLiteral("nav.open_symbol"), payload);
}

void open_ticket(const Ctx& ctx, const Ticket& t) {
    if (t.symbol.isEmpty())
        return;
    // `price` is the ticket's REFERENCE price (also a paper MARKET fill price): never the typed limit.
    QVariantMap payload{{QStringLiteral("symbol"), t.symbol},
                        {QStringLiteral("exchange"), t.exchange},
                        {QStringLiteral("is_buy"), t.is_buy},
                        {QStringLiteral("price"), 0.0},
                        {QStringLiteral("quantity"), t.quantity},
                        {QStringLiteral("source"), QStringLiteral("search")}};
    if (!t.order_type.isEmpty())
        payload.insert(QStringLiteral("order_type"), t.order_type);
    if (t.limit_price > 0)
        payload.insert(QStringLiteral("limit_price"), t.limit_price);
    if (!t.crypto) {
        payload.insert(QStringLiteral("match_exchanges"), t.match_exchanges);
        EventBus::instance().publish(QStringLiteral("equity.open_order_ticket"), payload);
        LOG_INFO(kTag, logf("Order ticket opened from search", {{"kind", "equity"}, {"side", t.is_buy ? "buy" : "sell"}}));
        return;
    }
    // Crypto: a handler acknowledges synchronously; without one, open Crypto Trading on the pair.
    static int serial = 0;
    const QString token = QStringLiteral("search-%1").arg(++serial);
    payload.insert(QStringLiteral("ack_token"), token);
    // The window the box lives in: the handler acts only for its own window, also when
    // no window is active (an agent driving the box).
    payload.insert(QStringLiteral("window"), QVariant::fromValue<QObject*>(ctx.window.data()));
    bool acked = false;
    {
        QObject guard; // the subscription ends with this scope
        EventBus::instance().subscribe(&guard, QStringLiteral("crypto.open_order_ticket.ack"),
                                       [&acked, token](const QVariantMap& d) {
                                           if (d.value(QStringLiteral("token")).toString() == token)
                                               acked = true;
                                       });
        EventBus::instance().publish(QStringLiteral("crypto.open_order_ticket"), payload);
    }
    LOG_INFO(kTag, logf("Order ticket opened from search",
                        {{"kind", "crypto"}, {"side", t.is_buy ? "buy" : "sell"}, {"handled", acked}}));
    if (!acked)
        open_symbol(ctx, QStringLiteral("crypto_trading"), t.symbol, QStringLiteral("crypto"));
}

void apply_theme(const QString& choice) {
    const QString c = choice.trimmed();
    if (c.isEmpty())
        return;
    // Queued: never restyle the application inside the popup's own key / mouse event chain.
    QTimer::singleShot(0, &ThemeManager::instance(), [c]() {
        ThemeManager::instance().apply_theme(c);
        auto r = SettingsRepository::instance().set(QStringLiteral("appearance.theme"), c, QStringLiteral("appearance"));
        if (r.is_err())
            LOG_WARN(kTag, logf("Theme applied but not saved", {{"error", QString::fromStdString(r.error())}}));
        EventBus::instance().publish(QStringLiteral("settings.changed"),
                                     {{QStringLiteral("key"), QStringLiteral("appearance.theme")}, {QStringLiteral("value"), c}});
        LOG_INFO(kTag, logf("Theme switched from search", {{"theme", c}}));
    });
}

QStringList watchlist_names() {
    QStringList out;
    const auto lists = WatchlistRepository::instance().list_all();
    if (lists.is_ok())
        for (const auto& wl : lists.value())
            out.append(wl.name);
    return out;
}

WatchlistOutcome add_to_watchlist(const QString& symbol, const QString& display_name, const QString& name,
                                  bool create_missing) {
    WatchlistOutcome out;
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty())
        return out;
    auto& repo = WatchlistRepository::instance();
    const auto lists = repo.list_all();
    if (lists.is_err()) {
        out.message = QCoreApplication::translate("CommandBar", "Could not read your watchlists");
        return out;
    }
    QString list_id;
    QString list_name;
    const QString wanted = name.trimmed();
    for (const auto& wl : lists.value()) {
        if (wanted.isEmpty() ? wl.is_default : wl.name.compare(wanted, Qt::CaseInsensitive) == 0) {
            list_id = wl.id;
            list_name = wl.name;
            break;
        }
    }
    if (list_id.isEmpty() && wanted.isEmpty() && !lists.value().isEmpty()) {
        list_id = lists.value().first().id; // no default flag set: the first list
        list_name = lists.value().first().name;
    }
    if (list_id.isEmpty()) {
        if (!create_missing) {
            out.message = QCoreApplication::translate("CommandBar", "No watchlist named “%1”").arg(wanted);
            return out;
        }
        const auto created = repo.create(wanted.isEmpty() ? QCoreApplication::translate("CommandBar", "My watchlist") : wanted);
        if (created.is_err()) {
            out.message = QCoreApplication::translate("CommandBar", "Could not create the watchlist");
            return out;
        }
        list_id = created.value().id;
        list_name = created.value().name;
        EventBus::instance().publish(QStringLiteral("watchlist.created"), {{QStringLiteral("id"), list_id}});
    }
    const auto stocks = repo.get_stocks(list_id);
    if (stocks.is_ok()) {
        for (const auto& s : stocks.value()) {
            if (s.symbol.compare(sym, Qt::CaseInsensitive) == 0) {
                out.ok = true;
                out.message = QCoreApplication::translate("CommandBar", "%1 is already in %2").arg(sym, list_name);
                return out;
            }
        }
    }
    const auto r = repo.add_stock(list_id, sym, display_name);
    if (r.is_err()) {
        out.message = QCoreApplication::translate("CommandBar", "Could not add %1 to %2").arg(sym, list_name);
        return out;
    }
    EventBus::instance().publish(QStringLiteral("watchlist.updated"),
                                 {{QStringLiteral("action"), QStringLiteral("add")}, {QStringLiteral("symbol"), sym}});
    out.ok = true;
    out.message = QCoreApplication::translate("CommandBar", "Added %1 to %2").arg(sym, list_name);
    return out;
}

void ask_ai(const Ctx& ctx, const QString& text) {
    open_screen(ctx, QStringLiteral("ai_chat"));
    const QPointer<QWidget> window = ctx.window;
    const QString draft = text.trimmed();
    if (!window || draft.isEmpty())
        return;
    // After the queued navigation has materialised the screen.
    QMetaObject::invokeMethod(window.data(), [window, draft]() { sa_prefill_ai(window, draft, 8); }, Qt::QueuedConnection);
    LOG_INFO(kTag, logf("AI Chat opened from search with a draft", {{"chars", draft.size()}}));
}

bool invoke_action(const Ctx& ctx, const QString& action_id, const QVariantMap& args, QString* error) {
    CommandContext c;
    c.shell = &TerminalShell::instance();
    c.focused_frame = qobject_cast<WindowFrame*>(ctx.window.data());
    c.args = args;
    const auto r = ActionRegistry::instance().invoke(action_id, c);
    if (r.is_err()) {
        if (error)
            *error = QString::fromStdString(r.error());
        return false;
    }
    return true;
}

void pin_symbol(const QString& symbol, const QString& asset_class, const QString& exchange) {
    SymbolRef ref;
    ref.symbol = symbol;
    ref.asset_class = asset_class;
    ref.exchange = exchange;
    if (ref.is_valid())
        PushpinService::instance().pin(ref);
}

} // namespace fincept::ui::search::actions
