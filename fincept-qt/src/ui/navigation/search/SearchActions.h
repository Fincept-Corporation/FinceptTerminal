#pragma once
// SearchActions — what a search result DOES, in one place: open a screen (with a
// sub-tab), hand a symbol to a screen, open an order ticket PREFILLED (never sends),
// switch the theme, add to a watchlist, open AI Chat with the question typed in
// (never sends), run a registered action. GUI thread only. Nothing here logs what
// the user typed (counts / ids only).

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

namespace fincept::ui::search::actions {

struct Ctx {
    QPointer<QWidget> window; ///< the WindowFrame the box lives in
    bool new_panel = false;   ///< Ctrl+Enter: beside the current panel instead of replacing it
};

/// nav.switch_screen {screen_id, section, focus, new_panel, exclusive, + args}.
void open_screen(const Ctx& ctx, const QString& screen_id, const QString& section = QString(),
                 const QString& focus = QString(), const QVariantMap& args = QVariantMap());
/// nav.open_symbol {screen_id, symbol, asset_class, exchange, exclusive}.
void open_symbol(const Ctx& ctx, const QString& screen_id, const QString& symbol, const QString& asset_class,
                 const QString& exchange = QString());

struct Ticket {
    bool crypto = false;
    QString symbol;            ///< equity: broker form ("RELIANCE"); crypto: the pair ("BTC/USDT")
    QString exchange;          ///< equity: order exchange ("NSE", "" for US); crypto: exchange id ("" = default)
    QStringList match_exchanges;
    bool is_buy = true;
    double quantity = 0;       ///< 0 = leave the ticket's default
    double limit_price = 0;    ///< typed limit; 0 = none (NEVER sent as the reference price)
    QString order_type;        ///< "limit" | "market" | "" (ticket default)
};
/// equity.open_order_ticket / crypto.open_order_ticket — the ticket opens for the user to confirm; nothing is
/// sent. Crypto: when no handler acknowledges (crypto.open_order_ticket.ack {token}), Crypto Trading opens with
/// the pair instead.
void open_ticket(const Ctx& ctx, const Ticket& t);

/// Apply "Light" / "Dark" / "System" now and keep it (appearance.theme, as Settings > Appearance saves it).
void apply_theme(const QString& choice);

struct WatchlistOutcome {
    bool ok = false;
    QString message; ///< plain words for the status line
};
/// Adds `symbol` to the watchlist named `name` (case-insensitive; "" = the default / only list). A named list
/// that does not exist is created when `create_missing`. Publishes watchlist.updated like the other add paths.
WatchlistOutcome add_to_watchlist(const QString& symbol, const QString& display_name, const QString& name,
                                  bool create_missing);
/// Names of the user's watchlists (sync, small; GUI thread).
QStringList watchlist_names();

/// Opens AI Chat with `text` in its composer (not sent).
void ask_ai(const Ctx& ctx, const QString& text);

/// ActionRegistry::invoke(id) with this window as the focused frame. false + error on failure.
bool invoke_action(const Ctx& ctx, const QString& action_id, const QVariantMap& args, QString* error = nullptr);

/// Pin a symbol to the header strip (PushpinService).
void pin_symbol(const QString& symbol, const QString& asset_class, const QString& exchange);

} // namespace fincept::ui::search::actions
