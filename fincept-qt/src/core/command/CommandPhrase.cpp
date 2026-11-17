// CommandPhrase.cpp — see CommandPhrase.h.
#include "core/command/CommandPhrase.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace fincept::command::phrase {

namespace {

const QSet<QString>& cph_currencies() {
    static const QSet<QString> s = {
        QStringLiteral("usd"), QStringLiteral("eur"), QStringLiteral("gbp"), QStringLiteral("jpy"), QStringLiteral("chf"),
        QStringLiteral("aud"), QStringLiteral("nzd"), QStringLiteral("cad"), QStringLiteral("cny"), QStringLiteral("cnh"),
        QStringLiteral("hkd"), QStringLiteral("sgd"), QStringLiteral("inr"), QStringLiteral("krw"), QStringLiteral("twd"),
        QStringLiteral("sek"), QStringLiteral("nok"), QStringLiteral("dkk"), QStringLiteral("pln"), QStringLiteral("czk"),
        QStringLiteral("huf"), QStringLiteral("try"), QStringLiteral("zar"), QStringLiteral("mxn"), QStringLiteral("brl"),
        QStringLiteral("rub"), QStringLiteral("ils"), QStringLiteral("aed"), QStringLiteral("sar"), QStringLiteral("thb"),
        QStringLiteral("idr"), QStringLiteral("myr"), QStringLiteral("php"), QStringLiteral("clp"), QStringLiteral("cop"),
        QStringLiteral("ars"), QStringLiteral("egp"), QStringLiteral("ngn"), QStringLiteral("kwd"), QStringLiteral("qar"),
        QStringLiteral("pkr"), QStringLiteral("vnd"),
    };
    return s;
}

const QSet<QString>& cph_cryptos() {
    static const QSet<QString> s = {
        QStringLiteral("btc"),  QStringLiteral("eth"),  QStringLiteral("sol"),  QStringLiteral("xrp"),
        QStringLiteral("bnb"),  QStringLiteral("ada"),  QStringLiteral("doge"), QStringLiteral("dot"),
        QStringLiteral("avax"), QStringLiteral("ltc"),  QStringLiteral("link"), QStringLiteral("matic"),
        QStringLiteral("trx"),  QStringLiteral("shib"), QStringLiteral("atom"), QStringLiteral("xlm"),
        QStringLiteral("bch"),  QStringLiteral("etc"),  QStringLiteral("uni"),  QStringLiteral("near"),
        QStringLiteral("apt"),  QStringLiteral("arb"),  QStringLiteral("op"),   QStringLiteral("sui"),
        QStringLiteral("ton"),  QStringLiteral("pepe"), QStringLiteral("xmr"),  QStringLiteral("fil"),
        QStringLiteral("hbar"), QStringLiteral("icp"),
    };
    return s;
}

const QStringList& cph_crypto_quotes() {
    // Longer first, so USDT / USDC / FDUSD are not read as USD.
    static const QStringList q = {QStringLiteral("fdusd"), QStringLiteral("usdt"), QStringLiteral("usdc"),
                                  QStringLiteral("busd"),  QStringLiteral("usd"),  QStringLiteral("eur"),
                                  QStringLiteral("gbp"),   QStringLiteral("btc"),  QStringLiteral("eth")};
    return q;
}

/// Words that are never a symbol inside a phrase.
bool cph_reserved(const QString& lower) {
    static const QSet<QString> s = {
        QStringLiteral("to"),     QStringLiteral("at"),        QStringLiteral("limit"),     QStringLiteral("lmt"),
        QStringLiteral("market"), QStringLiteral("mkt"),       QStringLiteral("shares"),    QStringLiteral("share"),
        QStringLiteral("units"),  QStringLiteral("unit"),      QStringLiteral("lots"),      QStringLiteral("lot"),
        QStringLiteral("x"),      QStringLiteral("watchlist"), QStringLiteral("and"),       QStringLiteral("vs"),
        QStringLiteral("versus"), QStringLiteral("with"),      QStringLiteral("for"),       QStringLiteral("of"),
        QStringLiteral("the"),    QStringLiteral("my"),        QStringLiteral("a"),         QStringLiteral("an"),
        QStringLiteral("buy"),    QStringLiteral("sell"),      QStringLiteral("in"),        QStringLiteral("on"),
    };
    return s.contains(lower);
}

QString cph_view_of(const QString& lower) {
    static const QHash<QString, QString> v = {
        {QStringLiteral("chart"), QStringLiteral("chart")},
        {QStringLiteral("charts"), QStringLiteral("chart")},
        {QStringLiteral("graph"), QStringLiteral("chart")},
        {QStringLiteral("price"), QStringLiteral("chart")},
        {QStringLiteral("news"), QStringLiteral("news")},
        {QStringLiteral("headlines"), QStringLiteral("news")},
        {QStringLiteral("research"), QStringLiteral("research")},
        {QStringLiteral("fundamentals"), QStringLiteral("research")},
        {QStringLiteral("financials"), QStringLiteral("research")},
        {QStringLiteral("profile"), QStringLiteral("research")},
        {QStringLiteral("analysis"), QStringLiteral("research")},
        {QStringLiteral("overview"), QStringLiteral("research")},
        {QStringLiteral("options"), QStringLiteral("options")},
        {QStringLiteral("option"), QStringLiteral("options")},
        {QStringLiteral("chain"), QStringLiteral("options")},
        {QStringLiteral("trade"), QStringLiteral("trade")},
        {QStringLiteral("ticket"), QStringLiteral("trade")},
        {QStringLiteral("order"), QStringLiteral("trade")},
        {QStringLiteral("backtest"), QStringLiteral("backtest")},
        {QStringLiteral("peers"), QStringLiteral("peers")},
        {QStringLiteral("comps"), QStringLiteral("peers")},
        {QStringLiteral("competitors"), QStringLiteral("peers")},
    };
    return v.value(lower);
}

bool cph_number(const QString& s, double* out) {
    QString t = s;
    t.remove(QLatin1Char(','));
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok || v < 0 || !std::isfinite(v))
        return false;
    if (out)
        *out = v;
    return true;
}

QStringList cph_tokens(const QString& text) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return text.trimmed().split(ws, Qt::SkipEmptyParts);
}

QStringList cph_lower(const QStringList& in) {
    QStringList out;
    out.reserve(in.size());
    for (const QString& t : in)
        out.append(t.toLower());
    return out;
}

QString cph_symbol(const QString& token) {
    return token.toUpper();
}

bool cph_symbol_token(const QString& token) {
    double ignored = 0;
    return is_ticker_like(token) && !cph_reserved(token.toLower()) && !cph_number(token, &ignored);
}

/// "eurusd", "eur/usd" or ["eur", "usd"] -> "EURUSD"; "" when not a currency pair.
QString cph_fx(const QStringList& lower) {
    QString a;
    QString b;
    if (lower.size() == 1) {
        const QString t = lower[0];
        if (t.contains(QLatin1Char('/'))) {
            const QStringList parts = t.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            if (parts.size() != 2)
                return {};
            a = parts[0];
            b = parts[1];
        } else if (t.size() == 6) {
            a = t.left(3);
            b = t.mid(3);
        } else {
            return {};
        }
    } else if (lower.size() == 2) {
        a = lower[0];
        b = lower[1];
    } else {
        return {};
    }
    if (a == b || !cph_currencies().contains(a) || !cph_currencies().contains(b))
        return {};
    return (a + b).toUpper();
}

/// ["btc"], ["eth", "usdt"], ["btcusdt"], ["btc/usdt"] -> {"BTC", "USDT"}; empty when not crypto.
QStringList cph_crypto(const QStringList& lower) {
    QString base;
    QString quote;
    if (lower.size() == 1) {
        const QString t = lower[0];
        if (t.contains(QLatin1Char('/')) || t.contains(QLatin1Char('-'))) {
            const QStringList parts = t.split(QRegularExpression(QStringLiteral("[/-]")), Qt::SkipEmptyParts);
            if (parts.size() != 2)
                return {};
            base = parts[0];
            quote = parts[1];
        } else if (cph_cryptos().contains(t)) {
            // A bare code alone only for unambiguous majors ("op", "link", "near", "dot" are words too).
            static const QSet<QString> majors = {
                QStringLiteral("btc"),  QStringLiteral("eth"),  QStringLiteral("sol"),  QStringLiteral("xrp"),
                QStringLiteral("bnb"),  QStringLiteral("ada"),  QStringLiteral("doge"), QStringLiteral("avax"),
                QStringLiteral("ltc"),  QStringLiteral("matic"), QStringLiteral("trx"), QStringLiteral("shib"),
                QStringLiteral("xlm"),  QStringLiteral("bch"),  QStringLiteral("xmr"),  QStringLiteral("hbar"),
                QStringLiteral("pepe"),
            };
            if (!majors.contains(t))
                return {};
            base = t;
        } else {
            for (const QString& q : cph_crypto_quotes()) {
                if (t.size() > q.size() + 1 && t.endsWith(q) && cph_cryptos().contains(t.left(t.size() - q.size()))) {
                    base = t.left(t.size() - q.size());
                    quote = q;
                    break;
                }
            }
        }
    } else if (lower.size() == 2) {
        base = lower[0];
        quote = lower[1];
    } else {
        return {};
    }
    if (!cph_cryptos().contains(base))
        return {};
    if (!quote.isEmpty() && !cph_crypto_quotes().contains(quote))
        return {};
    if (quote == base)
        return {};
    return {base.toUpper(), quote.isEmpty() ? QStringLiteral("USD") : quote.toUpper()};
}

bool cph_ticket(const QStringList& tokens, const QStringList& lower, Phrase* out) {
    if (lower.isEmpty() || (lower[0] != QLatin1String("buy") && lower[0] != QLatin1String("sell")))
        return false;
    Phrase p;
    p.kind = Kind::Ticket;
    p.side = lower[0];
    bool have_qty = false;
    bool have_price = false;
    for (int i = 1; i < tokens.size(); ++i) {
        const QString l = lower[i];
        double num = 0;
        if (l == QLatin1String("at") || l == QLatin1String("@") || l == QLatin1String("limit") ||
            l == QLatin1String("lmt") || l == QLatin1String("for")) {
            if (i + 1 < tokens.size() && cph_number(lower[i + 1], &num)) {
                if (have_price)
                    return false;
                p.price = num;
                have_price = true;
                p.order_type = QStringLiteral("limit");
                ++i;
                continue;
            }
            if (l == QLatin1String("limit")) {
                p.order_type = QStringLiteral("limit");
                continue;
            }
            return false;
        }
        if (l.startsWith(QLatin1Char('@')) && cph_number(l.mid(1), &num)) {
            if (have_price)
                return false;
            p.price = num;
            have_price = true;
            p.order_type = QStringLiteral("limit");
            continue;
        }
        if (l == QLatin1String("market") || l == QLatin1String("mkt")) {
            p.order_type = QStringLiteral("market");
            continue;
        }
        if (l == QLatin1String("shares") || l == QLatin1String("share") || l == QLatin1String("units") ||
            l == QLatin1String("unit") || l == QLatin1String("lots") || l == QLatin1String("lot") ||
            l == QLatin1String("x") || l == QLatin1String("of"))
            continue;
        if (cph_number(l, &num)) {
            if (have_qty)
                return false;
            p.quantity = num;
            have_qty = true;
            continue;
        }
        if (cph_symbol_token(tokens[i])) {
            if (!p.symbols.isEmpty())
                return false; // one instrument per ticket
            p.symbols.append(cph_symbol(tokens[i]));
            continue;
        }
        return false;
    }
    if (p.symbols.isEmpty())
        return false;
    if (p.order_type == QLatin1String("market"))
        p.price = 0;
    *out = p;
    return true;
}

bool cph_compare(const QStringList& tokens, const QStringList& lower, Phrase* out) {
    QStringList syms;
    int start = 0;
    bool keyword = false;
    if (!lower.isEmpty() && (lower[0] == QLatin1String("compare") || lower[0] == QLatin1String("cmp"))) {
        start = 1;
        keyword = true;
    }
    bool saw_vs = false;
    for (int i = start; i < tokens.size(); ++i) {
        QString t = tokens[i];
        QString l = lower[i];
        if (l == QLatin1String("vs") || l == QLatin1String("versus") || l == QLatin1String("v")) {
            saw_vs = true;
            continue;
        }
        if (l == QLatin1String("and") || l == QLatin1String("with") || l == QLatin1String("to") || l == QLatin1String(","))
            continue;
        if (t.endsWith(QLatin1Char(',')))
            t.chop(1);
        for (const QString& part : t.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (!cph_symbol_token(part))
                return false;
            syms.append(cph_symbol(part));
        }
    }
    if ((!keyword && !saw_vs) || syms.size() < 2)
        return false;
    out->kind = Kind::Compare;
    out->symbols = syms;
    return true;
}

bool cph_watchlist(const QStringList& tokens, const QStringList& lower, Phrase* out) {
    if (lower.size() >= 2 && lower[0] == QLatin1String("watch") && lower.size() == 2 && cph_symbol_token(tokens[1])) {
        out->kind = Kind::Watchlist;
        out->symbols = {cph_symbol(tokens[1])};
        return true;
    }
    if (lower.size() < 3 || lower[0] != QLatin1String("add"))
        return false;
    const int to = static_cast<int>(lower.indexOf(QStringLiteral("to")));
    if (to < 2)
        return false;
    QStringList syms;
    for (int i = 1; i < to; ++i) {
        QString t = tokens[i];
        if (lower[i] == QLatin1String("and"))
            continue;
        for (const QString& part : t.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (!cph_symbol_token(part))
                return false;
            syms.append(cph_symbol(part));
        }
    }
    if (syms.isEmpty())
        return false;
    QStringList name;
    bool said_watchlist = false;
    for (int i = to + 1; i < tokens.size(); ++i) {
        if (lower[i] == QLatin1String("watchlist") || lower[i] == QLatin1String("wl") ||
            lower[i] == QLatin1String("list")) {
            said_watchlist = true;
            continue;
        }
        if (lower[i] == QLatin1String("my") || lower[i] == QLatin1String("the"))
            continue;
        name.append(tokens[i]);
    }
    if (!said_watchlist && name.isEmpty())
        return false;
    out->kind = Kind::Watchlist;
    out->symbols = syms;
    out->text = name.join(QLatin1Char(' '));
    return true;
}

bool cph_theme(const QStringList& lower, Phrase* out) {
    auto choice = [](const QString& w) -> QString {
        if (w == QLatin1String("light") || w == QLatin1String("day") || w == QLatin1String("white"))
            return QStringLiteral("light");
        if (w == QLatin1String("dark") || w == QLatin1String("night") || w == QLatin1String("black"))
            return QStringLiteral("dark");
        if (w == QLatin1String("system") || w == QLatin1String("auto") || w == QLatin1String("os"))
            return QStringLiteral("system");
        return {};
    };
    QStringList t = lower;
    if (t.size() >= 2 && (t[0] == QLatin1String("switch") || t[0] == QLatin1String("use") ||
                          t[0] == QLatin1String("set") || t[0] == QLatin1String("change"))) {
        t.removeFirst();
        if (!t.isEmpty() && t[0] == QLatin1String("to"))
            t.removeFirst();
    }
    QString target;
    if (t.size() == 2 && (t[0] == QLatin1String("theme") || t[0] == QLatin1String("mode")))
        target = choice(t[1]);
    else if (t.size() == 2 && (t[1] == QLatin1String("theme") || t[1] == QLatin1String("mode")))
        target = choice(t[0]);
    else if (t.size() == 3 && t[0] == QLatin1String("theme") && t[1] == QLatin1String("to"))
        target = choice(t[2]);
    else if (t.size() == 1 && lower.size() > 1)
        target = choice(t[0]); // "switch to dark"
    if (target.isEmpty())
        return false;
    out->kind = Kind::Theme;
    out->target = target;
    return true;
}

QString cph_rest(const QString& text, int skip_tokens) {
    QString t = text.trimmed();
    for (int i = 0; i < skip_tokens; ++i) {
        const int sp = static_cast<int>(t.indexOf(QRegularExpression(QStringLiteral("\\s"))));
        if (sp < 0)
            return {};
        t = t.mid(sp).trimmed();
    }
    return t;
}

bool cph_dock(const QStringList& tokens, const QStringList& lower, const Vocabulary& vocab, Phrase* out) {
    if (!vocab.screen_for || tokens.size() < 2)
        return false;
    for (int i = 1; i < lower.size(); ++i) {
        const QString v = lower[i];
        if (v != QLatin1String("add") && v != QLatin1String("replace") && v != QLatin1String("remove"))
            continue;
        const QString primary = vocab.screen_for(tokens.mid(0, i).join(QLatin1Char(' ')));
        if (primary.isEmpty())
            return false;
        if (v == QLatin1String("remove")) {
            if (i != lower.size() - 1)
                return false;
            out->kind = Kind::Dock;
            out->verb = v;
            out->target = primary;
            return true;
        }
        if (i == lower.size() - 1)
            return false;
        const QString secondary = vocab.screen_for(tokens.mid(i + 1).join(QLatin1Char(' ')));
        if (secondary.isEmpty())
            return false;
        out->kind = Kind::Dock;
        out->verb = v;
        out->target = primary;
        out->text = secondary;
        return true;
    }
    return false;
}

} // namespace

bool is_ticker_like(const QString& token) {
    static const QRegularExpression rx(QStringLiteral("^\\^?[A-Za-z0-9][A-Za-z0-9.\\-=&]{0,14}$"));
    if (!rx.match(token).hasMatch())
        return false;
    bool letter = false;
    for (const QChar c : token)
        letter = letter || c.isLetter();
    return letter;
}

bool is_currency(const QString& code) {
    return cph_currencies().contains(code.toLower());
}

bool is_crypto(const QString& code) {
    return cph_cryptos().contains(code.toLower());
}

bool is_crypto_quote(const QString& code) {
    return cph_crypto_quotes().contains(code.toLower());
}

QStringList legacy_asset_types() {
    return {QStringLiteral("stock"),   QStringLiteral("fund"),  QStringLiteral("dr"),
            QStringLiteral("index"),   QStringLiteral("forex"), QStringLiteral("crypto"),
            QStringLiteral("futures"), QStringLiteral("bond"),  QStringLiteral("economic")};
}

QList<Phrase> parse(const QString& input, const Vocabulary& vocab) {
    QList<Phrase> out;
    const QString text = input.trimmed();
    if (text.isEmpty())
        return out;

    // ── Optional accelerators ──
    const QChar first = text.at(0);
    if (first == QLatin1Char('>') || first == QLatin1Char('$') || first == QLatin1Char('@') ||
        first == QLatin1Char('?')) {
        Phrase p;
        p.kind = Kind::Scope;
        p.scope = first == QLatin1Char('>')   ? QStringLiteral("action")
                  : first == QLatin1Char('?') ? QStringLiteral("help")
                                              : QStringLiteral("instrument");
        p.text = text.mid(1).trimmed();
        out.append(p);
        return out;
    }

    // ── The old bar's /types ──
    if (first == QLatin1Char('/')) {
        const QStringList toks = cph_tokens(text.mid(1));
        const QString head = toks.isEmpty() ? QString() : toks[0].toLower();
        QString type;
        for (const QString& t : legacy_asset_types()) {
            if (t == head) {
                type = t;
                break;
            }
        }
        if (type.isEmpty() && !head.isEmpty() && toks.size() == 1) {
            for (const QString& t : legacy_asset_types()) {
                if (t.startsWith(head)) {
                    type = t;
                    break;
                }
            }
        }
        if (!type.isEmpty()) {
            Phrase p;
            p.kind = Kind::AssetType;
            p.target = type;
            p.text = cph_rest(text.mid(1), 1);
            out.append(p);
            return out;
        }
        return parse(text.mid(1), vocab); // "/" carries nothing any more
    }

    const QStringList tokens = cph_tokens(text);
    const QStringList lower = cph_lower(tokens);

    Phrase p;
    if (lower[0] == QLatin1String("ask") && tokens.size() > 1) {
        p.kind = Kind::Ask;
        int skip = 1;
        if (lower.size() > 2 && (lower[1] == QLatin1String("ai") || lower[1] == QLatin1String("assistant")))
            skip = 2;
        p.text = cph_rest(text, skip);
        out.append(p);
        return out;
    }
    if (cph_theme(lower, &p)) {
        out.append(p);
        return out;
    }
    if ((lower[0] == QLatin1String("settings") || lower[0] == QLatin1String("setting") ||
         lower[0] == QLatin1String("preferences") || lower[0] == QLatin1String("prefs") ||
         lower[0] == QLatin1String("preference")) &&
        tokens.size() > 1) {
        p.kind = Kind::Settings;
        p.text = cph_rest(text, 1);
        out.append(p);
        return out;
    }
    if (cph_ticket(tokens, lower, &p)) {
        out.append(p);
        return out;
    }
    p = Phrase{};
    if (cph_compare(tokens, lower, &p)) {
        out.append(p);
        return out;
    }
    p = Phrase{};
    if (cph_watchlist(tokens, lower, &p)) {
        out.append(p);
        return out;
    }
    p = Phrase{};
    if (lower[0] == QLatin1String("backtest") || lower[0] == QLatin1String("backtesting")) {
        p.kind = Kind::Backtest;
        int next = 1;
        if (tokens.size() > 1 && cph_symbol_token(tokens[1]) && !cph_view_of(lower[1]).size()) {
            p.symbols = {cph_symbol(tokens[1])};
            next = 2;
        }
        p.text = cph_rest(text, next);
        if (tokens.size() > 1) {
            out.append(p);
            return out;
        }
        p = Phrase{};
    }
    if (lower[0] == QLatin1String("open") || lower[0] == QLatin1String("show") || lower[0] == QLatin1String("launch") ||
        lower[0] == QLatin1String("goto") ||
        (lower[0] == QLatin1String("go") && lower.size() > 2 && lower[1] == QLatin1String("to"))) {
        const int skip = lower[0] == QLatin1String("go") ? 2 : 1;
        const QString rest = cph_rest(text, skip);
        if (!rest.isEmpty()) {
            p.kind = Kind::Open;
            p.text = rest;
            out.append(p);
            // "open aapl chart": the rest may itself be a phrase — offer that reading too.
            for (const Phrase& inner : parse(rest, vocab))
                if (inner.kind != Kind::Open)
                    out.append(inner);
            return out;
        }
    }
    if (cph_dock(tokens, lower, vocab, &p)) {
        out.append(p);
        return out;
    }
    p = Phrase{};

    // ── "<symbol> <view>" / "<view> <symbol>" ──
    if (tokens.size() == 2 || (tokens.size() == 3 && lower[1] == QLatin1String("option") &&
                               lower[2] == QLatin1String("chain"))) {
        const QString v_last = tokens.size() == 3 ? QStringLiteral("options") : cph_view_of(lower[1]);
        const QString v_first = tokens.size() == 2 ? cph_view_of(lower[0]) : QString();
        if (!v_last.isEmpty() && cph_symbol_token(tokens[0]) && cph_view_of(lower[0]).isEmpty()) {
            p.kind = Kind::SymbolView;
            p.view = v_last;
            p.symbols = {cph_symbol(tokens[0])};
            out.append(p);
            return out;
        }
        if (!v_first.isEmpty() && cph_symbol_token(tokens[1]) && cph_view_of(lower[1]).isEmpty()) {
            p.kind = Kind::SymbolView;
            p.view = v_first;
            p.symbols = {cph_symbol(tokens[1])};
            out.append(p);
            return out;
        }
    }

    // ── FX / crypto ──
    QStringList fx_tokens = lower;
    if (fx_tokens.size() == 3 && fx_tokens[1] == QLatin1String("/"))
        fx_tokens = {fx_tokens[0], fx_tokens[2]};
    const QString fx = cph_fx(fx_tokens);
    if (!fx.isEmpty()) {
        p.kind = Kind::FxPair;
        p.pair = fx;
        p.symbols = {fx + QStringLiteral("=X")};
        out.append(p);
        return out;
    }
    const QStringList crypto = cph_crypto(fx_tokens);
    if (!crypto.isEmpty()) {
        p.kind = Kind::Crypto;
        p.pair = crypto[0] + QLatin1Char('/') + crypto[1];
        const QString yq = (crypto[1] == QLatin1String("USDT") || crypto[1] == QLatin1String("USDC") ||
                            crypto[1] == QLatin1String("FDUSD") || crypto[1] == QLatin1String("BUSD"))
                               ? QStringLiteral("USD")
                               : crypto[1];
        p.symbols = {crypto[0] + QLatin1Char('-') + yq};
        out.append(p);
        return out;
    }
    return out;
}

} // namespace fincept::command::phrase
