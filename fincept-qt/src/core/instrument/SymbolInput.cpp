// SymbolInput.cpp — the symbol-text normaliser. See SymbolInput.h. Qt Core only.

#include "core/instrument/SymbolInput.h"

#include "core/instrument/VenueSymbols.h"

#include <initializer_list>

namespace fincept::core::instrument {

namespace venue_symbols_detail {
QString yahoo_spelling(const QString& upper_symbol);
} // namespace venue_symbols_detail

namespace {
namespace syminput_detail {

constexpr qsizetype kSiMaxText = 100;  // longer input is never a symbol or a name query
constexpr qsizetype kSiMaxToken = 32;  // longest single token considered (contracts reach ~25)
constexpr qsizetype kSiMaxTicker = 20; // longest listing code (NIFTY_FIN_SERVICE.NS is 20)

bool si_upper(QChar c) {
    return c >= QLatin1Char('A') && c <= QLatin1Char('Z');
}

bool si_digit(QChar c) {
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

bool si_all_digits(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!si_digit(c))
            return false;
    return true;
}

bool si_all_letters(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!si_upper(c))
            return false;
    return true;
}

bool si_alnum(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!si_upper(c) && !si_digit(c))
            return false;
    return true;
}

bool si_has_non_ascii(QStringView s) {
    for (const QChar c : s)
        if (c.unicode() > 0x7F)
            return true;
    return false;
}

bool si_is_wrapping_char(QChar c) {
    static const QString kQuotes = QStringLiteral("\"'`´‘’‚‛“”„‟"
                                                  "«»‹›「」『』");
    return kQuotes.contains(c);
}

// 1-6 of A-Z 0-9 '&' with at least one letter: could be a US listing code.
bool si_ticker_shaped(QStringView s) {
    if (s.isEmpty() || s.size() > 6)
        return false;
    bool letter = false;
    for (const QChar c : s) {
        if (si_upper(c))
            letter = true;
        else if (!si_digit(c) && c != QLatin1Char('&'))
            return false;
    }
    return letter;
}

bool si_is_metal(QStringView s) {
    return s == u"XAU" || s == u"XAG" || s == u"XPT" || s == u"XPD";
}

// 6 letters made of two currency / metal codes ("EURUSD", "XAUUSD").
bool si_pair_shaped(const QString& s) {
    if (s.size() != 6 || !si_all_letters(s))
        return false;
    const QString a = s.left(3);
    const QString b = s.right(3);
    return (is_iso_currency(a) || si_is_metal(a)) && (is_iso_currency(b) || si_is_metal(b));
}

// Listing code characters (after '_' -> '-').
bool si_code_chars_ok(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!si_upper(c) && !si_digit(c) && c != QLatin1Char('&') && c != QLatin1Char('-'))
            return false;
    return true;
}

bool si_isin_shaped(QStringView s) {
    if (s.size() != 12 || !si_upper(s.at(0)) || !si_upper(s.at(1)) || !si_digit(s.at(11)))
        return false;
    for (qsizetype i = 2; i < 11; ++i)
        if (!si_upper(s.at(i)) && !si_digit(s.at(i)))
            return false;
    // At least one digit among the 9 body characters: "ABCDEFGHIJK1" is not an ISIN shape.
    for (qsizetype i = 2; i < 11; ++i)
        if (si_digit(s.at(i)))
            return true;
    return false;
}

bool si_occ(QStringView s) {
    if (s.size() < 16 || s.size() > 21)
        return false;
    const qsizetype root = s.size() - 15;
    if (root < 1 || root > 6)
        return false;
    for (qsizetype i = 0; i < root; ++i)
        if (!si_upper(s.at(i)))
            return false;
    for (qsizetype i = root; i < root + 6; ++i)
        if (!si_digit(s.at(i)))
            return false;
    const QChar cp = s.at(root + 6);
    if (cp != QLatin1Char('C') && cp != QLatin1Char('P'))
        return false;
    for (qsizetype i = root + 7; i < s.size(); ++i)
        if (!si_digit(s.at(i)))
            return false;
    return true;
}

// Underlying of an Indian contract name: the leading non-digit run.
QString si_contract_underlying(const QString& s) {
    qsizetype i = 0;
    while (i < s.size() && !si_digit(s.at(i)))
        ++i;
    return s.left(i);
}

void si_add(ParsedInput& p, const QString& sym) {
    if (!sym.isEmpty() && !p.candidates.contains(sym))
        p.candidates.push_back(sym);
}

void si_note(ParsedInput& p, const char* note) {
    const QString n = QString::fromLatin1(note);
    if (!p.notes.contains(n))
        p.notes.push_back(n);
}

void si_prefer(ParsedInput& p, AssetClass c) {
    if (c != AssetClass::Unknown && !p.preferred_classes.contains(c))
        p.preferred_classes.push_back(c);
}

ParsedInput& si_done(ParsedInput& p) {
    p.asset_class = p.preferred_classes.value(0, AssetClass::Unknown);
    return p;
}

ParsedInput& si_invalid(ParsedInput& p, const char* note) {
    p.kind = InputKind::Invalid;
    p.candidates.clear();
    p.canonical.clear();
    p.search_text.clear();
    p.preferred_classes.clear();
    si_note(p, note);
    return si_done(p);
}

// Country of a candidate Yahoo symbol, for home-region ordering.
QString si_country_of(const QString& sym) {
    if (sym.startsWith(QLatin1Char('^')) || sym.contains(QLatin1Char('=')))
        return {};
    const qsizetype dot = sym.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        const VenueInfo* v = venue_info_for_suffix(sym.mid(dot + 1));
        return v ? v->country : QString();
    }
    return QStringLiteral("US");
}

// Primary listing market of a country (first covered, non-approximate listing entry).
const VenueInfo* si_home_market(const QString& country) {
    if (country.isEmpty())
        return nullptr;
    for (const VenueInfo& v : all_venue_infos()) {
        if (v.country == country && !v.approximate && v.yahoo_covered &&
            (v.role == VenueRole::Listing || v.role == VenueRole::UsListing))
            return &v;
    }
    return nullptr;
}

void si_home_first(ParsedInput& p, const QString& home) {
    if (home.isEmpty() || p.candidates.size() < 2)
        return;
    QStringList first, rest;
    for (const QString& c : p.candidates)
        (si_country_of(c) == home ? first : rest).push_back(c);
    p.candidates = first + rest;
}

// ── Builders ────────────────────────────────────────────────────────────────

// USD base -> Yahoo's own form (USDINR -> INR=X); otherwise BASEQUOTE=X.
ParsedInput& si_fx(ParsedInput& p, const QString& a, const QString& b) {
    p.kind = InputKind::FxPair;
    p.base = a;
    p.quote = b;
    p.pair = a + QLatin1Char('/') + b;
    p.venue = QStringLiteral("FX");
    const QString sym = (a == QLatin1String("USD") ? b : a + b) + QStringLiteral("=X");
    si_add(p, sym);
    p.canonical = sym;
    si_prefer(p, AssetClass::Fx);
    return si_done(p);
}

// typed_yahoo: the text was a Yahoo dash form ("TON-USD") — kept as a later candidate
// when the table maps the base elsewhere (Yahoo reuses short tickers for other coins).
ParsedInput& si_crypto(ParsedInput& p, const QString& base, const QString& quote, bool perp,
                       const QString& typed_yahoo = {}) {
    p.kind = InputKind::CryptoPair;
    const CryptoInfo* c = crypto_for_base(base);
    p.base = c ? c->base : base;
    p.quote = quote;
    if (!quote.isEmpty())
        p.pair = p.base + QLatin1Char('/') + quote;
    p.venue = QStringLiteral("CRYPTO");
    si_prefer(p, AssetClass::Crypto);
    if (perp)
        si_note(p, "perpetual");
    if (!c) {
        si_note(p, "unknown_crypto_base");
        return si_done(p);
    }
    QString q = quote.isEmpty() ? QStringLiteral("USD") : quote;
    if (is_stablecoin(q)) {
        si_note(p, "stablecoin_as_usd");
        q = QStringLiteral("USD");
    }
    const QString sym = to_yahoo(c->base + QLatin1Char('-') + q, QStringLiteral("CRYPTO"));
    if (sym.isEmpty()) {
        si_note(p, "no_yahoo_symbol");
        return si_done(p);
    }
    si_add(p, sym);
    if (!typed_yahoo.isEmpty())
        si_add(p, typed_yahoo);
    if (!quote.isEmpty())
        p.canonical = sym;
    return si_done(p);
}

// Ticker candidates for `code` (A-Z 0-9 & -, already normalised) with a venue, or the fan-out.
ParsedInput& si_ticker(ParsedInput& p, QString code, const VenueInfo* venue, bool venue_unknown, bool class_share,
                       const QString& home, const CryptoInfo* coin_last) {
    p.kind = InputKind::Ticker;
    if (code.size() > kSiMaxTicker)
        return si_invalid(p, "too_long");
    if (!si_code_chars_ok(code))
        return si_invalid(p, "bad_characters");
    p.base = code;
    if (venue_unknown) {
        si_note(p, "unknown_venue");
        return si_done(p);
    }
    if (venue) {
        p.venue = venue->venue;
        if (!venue->broker_exchange.isEmpty() && venue->role == VenueRole::Listing) {
            const QString stripped = strip_series_suffix(code);
            if (stripped != code) {
                si_note(p, "series_stripped");
                p.base = stripped;
            }
        }
        const QString sym = to_yahoo(code, venue->key);
        if (sym.isEmpty()) {
            if (venue->role == VenueRole::Euronext)
                si_note(p, "euronext_needs_market"); // Paris, Amsterdam, ...? The code alone does not say
            si_note(p, venue->yahoo_covered ? "no_yahoo_symbol" : "no_yahoo_venue");
            return si_done(p);
        }
        si_add(p, sym);
        // "KRX" (TradingView, MIC XKRX, country KR) does not tell KOSPI from KOSDAQ: offer
        // the KOSDAQ listing too. An explicit KOSPI / KSC hint does not.
        const QString hint_up = p.venue_hint.trimmed().toUpper();
        if (venue->key == QLatin1String("KOSPI") && si_all_digits(code) &&
            (hint_up == QLatin1String("KRX") || hint_up == QLatin1String("XKRX") || hint_up == QLatin1String("KR")))
            si_add(p, to_yahoo(code, QStringLiteral("KOSDAQ")));
        if (p.candidates.size() == 1)
            p.canonical = sym;
        if (venue->role == VenueRole::IndexNamespace || sym.startsWith(QLatin1Char('^')))
            si_prefer(p, AssetClass::Index);
        return si_done(p);
    }

    QStringList keys;
    if (class_share) {
        keys = {QStringLiteral("NYSE"), QStringLiteral("TSX"), QStringLiteral("LSE")};
    } else if (si_all_digits(code)) {
        const qsizetype n = code.size();
        if (n <= 3 || n == 5) {
            keys = {QStringLiteral("HKEX")};
        } else if (n == 4) {
            if (code.startsWith(QLatin1Char('0')))
                keys = {QStringLiteral("HKEX"), QStringLiteral("TWSE"), QStringLiteral("MYX")};
            else
                keys = {QStringLiteral("TSE"), QStringLiteral("HKEX"), QStringLiteral("TWSE"),
                        QStringLiteral("TADAWUL"), QStringLiteral("MYX"), QStringLiteral("TPEX")};
        } else if (n == 6) {
            const QChar d = code.at(0);
            if (code.startsWith(QLatin1String("92")) || d == QLatin1Char('4') || d == QLatin1Char('8'))
                keys = {QStringLiteral("BJSE")};
            else if (d == QLatin1Char('6') || d == QLatin1Char('5') || d == QLatin1Char('9'))
                keys = {QStringLiteral("SSE")};
            else
                keys = {QStringLiteral("SZSE")};
            if (d == QLatin1Char('5'))
                keys << QStringLiteral("BSE");
            keys << QStringLiteral("KOSPI") << QStringLiteral("KOSDAQ");
        }
    } else if (code.size() == 4 && si_all_digits(QStringView(code).left(3)) && si_upper(code.at(3))) {
        keys = {QStringLiteral("TSE")}; // 130A: TSE alphanumeric code
    } else {
        keys = {QStringLiteral("NYSE"),  QStringLiteral("NSE"), QStringLiteral("BSE"),   QStringLiteral("LSE"),
                QStringLiteral("TSX"),   QStringLiteral("ASX"), QStringLiteral("XETRA"), QStringLiteral("PARIS")};
        if (const VenueInfo* hv = si_home_market(home); hv && !keys.contains(hv->key))
            keys.prepend(hv->key);
    }
    for (const QString& k : keys)
        si_add(p, to_yahoo(code, k));
    si_home_first(p, home);
    if (coin_last) {
        si_add(p, coin_last->yahoo_base + QStringLiteral("-USD"));
    }
    if (p.candidates.isEmpty())
        si_note(p, "no_yahoo_symbol");
    return si_done(p);
}

ParsedInput& si_yahoo(ParsedInput& p, const QString& sym_in) {
    const QString sym = venue_symbols_detail::yahoo_spelling(sym_in);
    const YahooParts parts = from_yahoo(sym);
    if (!parts.valid())
        return si_invalid(p, "bad_characters");
    p.kind = InputKind::YahooSymbol;
    p.base = parts.code;
    p.quote = parts.quote;
    p.venue = parts.venue;
    if (parts.cls != AssetClass::Equity)
        si_prefer(p, parts.cls);
    si_add(p, parts.symbol);
    p.canonical = parts.symbol;
    if (parts.venue_from_suffix) {
        if (const VenueInfo* v = venue_info_for_suffix(parts.suffix); v && !v->yahoo_covered)
            si_note(p, "no_yahoo_venue");
    }
    return si_done(p);
}

// Whole-text alias (index / commodity / crypto name), filtered by the venue.
bool si_alias(ParsedInput& p, const QString& up, const VenueInfo* venue) {
    const QVector<AliasTarget> all = lookup_alias(up);
    QVector<AliasTarget> hits;
    for (const AliasTarget& t : all) {
        bool ok = false;
        if (!venue) {
            ok = true;
        } else {
            switch (venue->role) {
                case VenueRole::IndexNamespace:
                    ok = (t.asset_class == AssetClass::Index &&
                          (venue->country.isEmpty() || t.country == venue->country)) ||
                         (t.asset_class == AssetClass::Future && venue->country.isEmpty());
                    break;
                case VenueRole::Futures:
                    ok = t.asset_class == AssetClass::Future;
                    break;
                case VenueRole::Fx:
                    ok = t.asset_class == AssetClass::Future; // XAUUSD on OANDA -> gold
                    break;
                case VenueRole::Crypto:
                    ok = t.asset_class == AssetClass::Crypto;
                    break;
                case VenueRole::Listing:
                case VenueRole::UsListing:
                    ok = t.asset_class == AssetClass::Index && t.country == venue->country && !t.text_only;
                    break;
                case VenueRole::Euronext:
                case VenueRole::Derivatives:
                    break;
            }
        }
        if (ok)
            hits.push_back(t);
    }
    if (hits.isEmpty())
        return false;
    p.kind = InputKind::Alias;
    p.base = up;
    p.venue = venue && (venue->role == VenueRole::Listing || venue->role == VenueRole::UsListing) ? venue->venue
                                                                                                   : hits.first().venue;
    for (const AliasTarget& t : hits) {
        si_add(p, t.yahoo);
        si_prefer(p, t.asset_class);
    }
    if (p.candidates.isEmpty())
        si_note(p, "no_yahoo_symbol");
    // The same-named US listing last ("gold" -> GOLD, "DOW" -> Dow Inc).
    if (!venue && si_ticker_shaped(up) && !si_pair_shaped(up))
        si_add(p, up);
    si_done(p);
    return true;
}

// Pair syntax. Returns true when `up` was handled (as a pair, or rejected).
bool si_pair(ParsedInput& p, const QString& up, const VenueInfo* venue) {
    const bool fx_venue = venue && venue->role == VenueRole::Fx;
    const bool crypto_venue = venue && venue->role == VenueRole::Crypto;

    const qsizetype slash = up.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        const QString a = up.left(slash);
        QString b = up.mid(slash + 1);
        const qsizetype colon = b.indexOf(QLatin1Char(':'));
        if (colon > 0)
            b.truncate(colon); // BTC/USDT:USDT (ccxt settle)
        if (b.size() == 1 && si_upper(b.at(0)) && !fx_venue && !crypto_venue)
            return false; // BRK/B: class share
        if (si_is_metal(a)) {
            p.kind = InputKind::Alias;
            p.base = a;
            p.quote = b;
            si_prefer(p, AssetClass::Future);
            const QString sym = b == QLatin1String("USD") ? to_yahoo(a + b, QStringLiteral("FX")) : QString();
            if (sym.isEmpty())
                si_note(p, "no_yahoo_symbol");
            si_add(p, sym);
            si_done(p);
            return true;
        }
        if (!crypto_venue && is_iso_currency(a) && is_iso_currency(b) && a != b) {
            si_fx(p, a, b);
            return true;
        }
        if (si_alnum(a) && a.size() >= 2 && a.size() <= 12 && crypto_quote_currencies().contains(b)) {
            si_crypto(p, a, b, false);
            return true;
        }
        si_invalid(p, "bad_characters");
        return true;
    }

    const qsizetype dash = up.indexOf(QLatin1Char('-'));
    if (dash > 0 && up.indexOf(QLatin1Char('-'), dash + 1) < 0) {
        const QString a = up.left(dash);
        QString b = up.mid(dash + 1);
        bool perp = false;
        if (b.endsWith(QLatin1String(".P"))) {
            b.chop(2);
            perp = true;
        }
        if (!crypto_venue && is_iso_currency(a) && is_iso_currency(b) && a != b && !crypto_for_base(a)) {
            si_fx(p, a, b);
            return true;
        }
        if (si_alnum(a) && a.size() >= 2 && crypto_quote_currencies().contains(b)) {
            const CryptoInfo* c = crypto_for_base(a);
            // The typed Yahoo form is a second candidate when it is a different Yahoo symbol.
            const QString typed = (c && c->yahoo_base != a && is_iso_currency(b)) ? up : QString();
            si_crypto(p, a, b, perp, typed);
            return true;
        }
        if (fx_venue || crypto_venue) {
            si_invalid(p, "bad_characters");
            return true;
        }
        return false;
    }

    // Concatenated forms.
    QString code = up;
    bool perp = false;
    if (code.endsWith(QLatin1String(".P"))) {
        code.chop(2);
        perp = true;
    }
    if (!si_alnum(code))
        return false;
    if (!crypto_venue && code.size() == 6 && is_iso_currency(code.left(3)) && is_iso_currency(code.right(3)) &&
        code.left(3) != code.right(3) && !crypto_for_base(code.left(3))) {
        si_fx(p, code.left(3), code.right(3));
        return true;
    }
    if (fx_venue) {
        if (code.size() == 3 && is_iso_currency(code) && code != QLatin1String("USD")) {
            si_fx(p, QStringLiteral("USD"), code);
            return true;
        }
        si_invalid(p, "bad_characters");
        return true;
    }
    for (const QString& q : crypto_quote_currencies()) {
        if (code.size() > q.size() + 1 && code.endsWith(q)) {
            const QString b = code.left(code.size() - q.size());
            if (crypto_for_base(b) || crypto_venue) {
                si_crypto(p, b, q, perp);
                return true;
            }
        }
    }
    if (crypto_venue) {
        si_crypto(p, code, QString(), perp);
        return true;
    }
    return false;
}

} // namespace syminput_detail
} // namespace

using namespace syminput_detail;

// ── Public API ──────────────────────────────────────────────────────────────

QString input_kind_slug(InputKind kind) {
    switch (kind) {
        case InputKind::Empty:
            return QStringLiteral("empty");
        case InputKind::Invalid:
            return QStringLiteral("invalid");
        case InputKind::Isin:
            return QStringLiteral("isin");
        case InputKind::Contract:
            return QStringLiteral("contract");
        case InputKind::YahooSymbol:
            return QStringLiteral("yahoo_symbol");
        case InputKind::FxPair:
            return QStringLiteral("fx_pair");
        case InputKind::CryptoPair:
            return QStringLiteral("crypto_pair");
        case InputKind::Alias:
            return QStringLiteral("alias");
        case InputKind::Ticker:
            return QStringLiteral("ticker");
        case InputKind::Name:
            return QStringLiteral("name");
    }
    return {};
}

QString clean_symbol_text(const QString& text) {
    const QString nfkc = text.normalized(QString::NormalizationForm_KC);
    QString out;
    out.reserve(nfkc.size());
    for (const QChar c : nfkc) {
        if (c.isSpace() || c == QLatin1Char('\t') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            out.append(QLatin1Char(' '));
            continue;
        }
        const QChar::Category cat = c.category();
        if (cat == QChar::Other_Format || cat == QChar::Other_Control)
            continue; // zero-width space, BOM, soft hyphen, controls
        out.append(c);
    }
    out = out.simplified();
    // Wrapping quotes and a cashtag / spreadsheet-text prefix, repeatedly ("'$AAPL'").
    for (bool changed = true; changed && !out.isEmpty();) {
        changed = false;
        if (si_is_wrapping_char(out.front()) || out.front() == QLatin1Char('$')) {
            out.remove(0, 1);
            changed = true;
        }
        if (!out.isEmpty() && si_is_wrapping_char(out.back())) {
            out.chop(1);
            changed = true;
        }
        if (changed)
            out = out.trimmed();
    }
    return out;
}

QStringList split_symbol_list(const QString& text) {
    QStringList out;
    QString cur;
    auto flush = [&out, &cur]() {
        const QString c = clean_symbol_text(cur);
        if (!c.isEmpty())
            out.push_back(c);
        cur.clear();
    };
    for (const QChar c : text) {
        if (c == QLatin1Char(',') || c == QLatin1Char(';') || c == QLatin1Char('\t') || c == QLatin1Char('\n') ||
            c == QLatin1Char('\r'))
            flush();
        else
            cur.append(c);
    }
    flush();
    return out;
}

QString normalize_name(const QString& text) {
    const QString d = text.normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d) {
        const QChar::Category cat = c.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining || cat == QChar::Mark_Enclosing)
            continue;
        if (c.isLetterOrNumber() || c == QLatin1Char('&') || c.isSurrogate())
            out.append(c);
        else
            out.append(QLatin1Char(' '));
    }
    return out.toCaseFolded().simplified();
}

bool is_valid_isin(const QString& isin) {
    if (!si_isin_shaped(isin))
        return false;
    // Letters -> 10..35, then Luhn over the resulting digit string.
    QString digits;
    digits.reserve(24);
    for (const QChar c : isin) {
        if (si_digit(c))
            digits.append(c);
        else
            digits.append(QString::number(c.unicode() - 'A' + 10));
    }
    int sum = 0;
    bool dbl = false;
    for (qsizetype i = digits.size() - 1; i >= 0; --i) {
        int v = digits.at(i).unicode() - '0';
        if (dbl) {
            v *= 2;
            if (v > 9)
                v -= 9;
        }
        sum += v;
        dbl = !dbl;
    }
    return sum % 10 == 0;
}

namespace {
namespace syminput_detail {

// The classification itself; parse_symbol_input() applies the unknown-venue rule on top.
ParsedInput si_parse_body(const QString& text, const QString& venue_hint, const QString& home_country) {
    ParsedInput p;
    p.raw = text;
    p.cleaned = clean_symbol_text(text);
    if (p.cleaned.isEmpty())
        return si_done(p);
    if (p.cleaned.size() > kSiMaxText)
        return si_invalid(p, "too_long");
    p.search_text = p.cleaned;
    QString up = p.cleaned.toUpper();
    const QString home = home_country.trimmed().toUpper();

    // Venue from the argument.
    const VenueInfo* venue = nullptr;
    bool venue_unknown = false;
    const QString hint = venue_hint.trimmed();
    if (!hint.isEmpty()) {
        venue = venue_info_for_code(hint);
        p.venue_hint = hint;
        venue_unknown = venue == nullptr;
    }

    // ISIN: a lookup key, never a symbol.
    if (si_isin_shaped(up)) {
        if (!is_valid_isin(up))
            return si_invalid(p, "isin_check_digit");
        p.kind = InputKind::Isin;
        p.isin = up;
        p.base = up;
        p.search_text.clear();
        return si_done(p);
    }

    // "EXCH:REST" — the prefix names the venue (TradingView codes first). An explicit hint wins.
    const qsizetype colon = up.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        const QString prefix = up.left(colon);
        bool code_like = true;
        for (const QChar c : prefix)
            if (!si_upper(c) && !si_digit(c) && c != QLatin1Char('_'))
                code_like = false;
        if (code_like) {
            if (!venue && !venue_unknown) {
                venue = venue_info_for_code(prefix, CodeSpace::TradingView);
                p.venue_hint = prefix;
                venue_unknown = venue == nullptr;
            }
            up = up.mid(colon + 1).trimmed();
            p.search_text = p.cleaned.mid(colon + 1).trimmed();
            if (up.isEmpty()) {
                if (venue)
                    p.venue = venue->venue;
                return si_done(p);
            }
        }
    }
    if (venue_unknown)
        si_note(p, "unknown_venue");

    const bool single = !up.contains(QLatin1Char(' '));
    if (single && up.size() > kSiMaxToken)
        return si_invalid(p, "too_long");

    // Contracts.
    if (venue && venue->role == VenueRole::Derivatives) {
        p.kind = InputKind::Contract;
        p.venue = venue->venue;
        p.base = si_contract_underlying(up);
        if (p.base.isEmpty())
            p.base = up;
        if (is_exchange_contract_name(up)) // RELIANCE alone is not an option although it ends in "CE"
            si_prefer(p, up.endsWith(QLatin1String("FUT")) ? AssetClass::Future : AssetClass::Option);
        si_note(p, "no_yahoo_venue");
        return si_done(p);
    }
    if (single && (up.endsWith(QLatin1String("1!")) || up.endsWith(QLatin1String("2!"))) && up.size() > 2) {
        p.kind = InputKind::Contract;
        p.base = up.left(up.size() - 2);
        si_prefer(p, AssetClass::Future);
        const FutureInfo* f = future_for_root(p.base);
        const bool us_futures = !venue || venue->role == VenueRole::Futures ||
                                (venue->role == VenueRole::IndexNamespace && venue->country.isEmpty());
        if (f && us_futures) {
            si_add(p, f->yahoo);
            p.canonical = f->yahoo;
            p.venue = QStringLiteral("CME");
        } else {
            if (venue)
                p.venue = venue->venue;
            si_note(p, "no_yahoo_symbol");
        }
        return si_done(p);
    }
    if (single && si_occ(up) && (!venue || venue->role == VenueRole::UsListing)) {
        p.kind = InputKind::Contract;
        p.base = up.left(up.size() - 15);
        p.venue = QStringLiteral("NYSE");
        si_prefer(p, AssetClass::Option);
        si_add(p, up);
        p.canonical = up;
        return si_done(p);
    }
    if (single && is_exchange_contract_name(up)) {
        p.kind = InputKind::Contract;
        p.base = si_contract_underlying(up);
        if (venue)
            p.venue = venue->venue;
        si_prefer(p, up.endsWith(QLatin1String("FUT")) ? AssetClass::Future : AssetClass::Option);
        si_note(p, "no_yahoo_symbol");
        return si_done(p);
    }

    // Non-ASCII text is a name ("Nestlé", "腾讯").
    if (si_has_non_ascii(up)) {
        p.kind = InputKind::Name;
        if (venue)
            p.venue = venue->venue;
        return si_done(p);
    }

    // Yahoo special forms.
    if (single) {
        const bool futures_suffix = [&up]() {
            const qsizetype dot = up.lastIndexOf(QLatin1Char('.'));
            if (dot <= 0)
                return false;
            const VenueInfo* v = venue_info_for_suffix(up.mid(dot + 1));
            return v && v->role == VenueRole::Futures;
        }();
        if (up.startsWith(QLatin1Char('^')) || up.endsWith(QLatin1String("=F")) || futures_suffix ||
            index_for_symbol(up))
            return si_yahoo(p, up);
        if (up.endsWith(QLatin1String("=X"))) {
            const QString pair = up.chopped(2);
            if (pair.size() == 6 && is_iso_currency(pair.left(3)) && is_iso_currency(pair.right(3)) &&
                pair.left(3) != pair.right(3))
                return si_fx(p, pair.left(3), pair.right(3));
            if (pair.size() == 3 && is_iso_currency(pair) && pair != QLatin1String("USD"))
                return si_fx(p, QStringLiteral("USD"), pair);
            if (si_alias(p, pair, nullptr))
                return p; // XAUUSD=X -> gold
            return si_invalid(p, "bad_characters");
        }
    }

    // Fyers index spelling "NIFTY50-INDEX": only an index may match.
    if (single && up.size() > 6 && up.endsWith(QLatin1String("-INDEX"))) {
        up.chop(6);
        const VenueInfo* idx_venue = venue;
        if (venue && (venue->role == VenueRole::Listing || venue->role == VenueRole::UsListing))
            idx_venue = venue_info_for_code(venue->country == QLatin1String("IN") ? QStringLiteral("NSE_INDEX")
                                                                                 : QStringLiteral("INDEX"));
        if (si_alias(p, up, idx_venue) && p.preferred_classes.value(0) == AssetClass::Index)
            return p;
        ParsedInput fresh;
        fresh.raw = p.raw;
        fresh.cleaned = p.cleaned;
        fresh.venue_hint = p.venue_hint;
        p = fresh;
        p.kind = InputKind::Ticker;
        p.base = up;
        p.search_text = up;
        if (venue)
            p.venue = venue->venue;
        si_prefer(p, AssetClass::Index);
        si_note(p, "no_yahoo_symbol");
        if (venue_unknown)
            si_note(p, "unknown_venue");
        return si_done(p);
    }

    // A bare crypto base or short crypto code ("BTC", "XBT", "MATIC").
    const CryptoInfo* coin_last = nullptr;
    if (single && (!venue || venue->role == VenueRole::Crypto)) {
        if (const CryptoInfo* c = crypto_for_base(up);
            c && (up == c->base || (up.size() <= 5 && si_all_letters(up)))) {
            if (venue) {
                si_crypto(p, up, QString(), false);
                return p;
            }
            if (c->crypto_first) {
                si_crypto(p, up, QString(), false);
                si_add(p, up); // the same-named US listing second (BTC: the ETF)
                return p;
            }
            coin_last = c; // STX, SUI, SAND, BCH: the listed company first, the coin last
        }
    }

    // Whole-text alias.
    if (!coin_last && si_alias(p, up, venue))
        return p;

    // Pair syntax.
    if (single && !coin_last && (!venue || venue->role == VenueRole::Fx || venue->role == VenueRole::Crypto)) {
        if (si_pair(p, up, venue))
            return p;
    }

    if (venue && (venue->role == VenueRole::Fx || venue->role == VenueRole::Crypto)) {
        // Nothing pair-like on an FX / crypto venue.
        return si_invalid(p, "bad_characters");
    }

    // Two tokens: "BRK B" (class share), "VOD L" (Yahoo suffix), "RELIANCE NSE" (venue word).
    if (!single) {
        const QStringList parts = up.split(QLatin1Char(' '));
        if (parts.size() == 2) {
            const QString& a = parts.at(0);
            const QString& b = parts.at(1);
            if (b.size() == 1 && si_upper(b.at(0)) && si_all_letters(a) && a.size() <= 5) {
                if (venue_info_for_suffix(b))
                    up = a + QLatin1Char('.') + b; // "VOD L" -> VOD.L
                else
                    return si_ticker(p, a + QLatin1Char('-') + b, venue, venue_unknown, true, home, nullptr);
            } else if (si_code_chars_ok(QString(a).replace(QLatin1Char('_'), QLatin1Char('-'))) && b.size() >= 2) {
                const VenueInfo* vb = venue_info_for_code(b);
                if (vb && !venue && !venue_unknown) {
                    p.venue_hint = b;
                    return si_ticker(p, QString(a).replace(QLatin1Char('_'), QLatin1Char('-')), vb, false, false,
                                     home, nullptr);
                }
            }
        }
        if (up.contains(QLatin1Char(' '))) {
            p.kind = InputKind::Name;
            if (venue)
                p.venue = venue->venue;
            return si_done(p);
        }
    }

    // One token: CODE.SUFFIX, class shares, CODE.VENUE, plain codes.
    QString code = up;
    while (code.endsWith(QLatin1Char('.')))
        code.chop(1); // "BP." (LSE TIDM padding)
    if (code.isEmpty())
        return si_invalid(p, "bad_characters");
    const qsizetype dot = code.lastIndexOf(QLatin1Char('.'));
    bool class_share = false;
    if (dot > 0 && dot < code.size() - 1) {
        const QString left = code.left(dot);
        const QString right = code.mid(dot + 1);
        if (venue_info_for_suffix(right))
            return si_yahoo(p, code); // the suffix wins over any hint
        if (right.size() == 1 && si_upper(right.at(0)) && si_all_letters(left) && left.size() <= 5) {
            code = left + QLatin1Char('-') + right;
            class_share = true;
        } else if (right.size() >= 2) {
            const VenueInfo* vr = venue_info_for_code(right);
            if (vr && !venue && !venue_unknown) {
                p.venue_hint = right;
                venue = vr;
                code = left;
            } else if (!vr) {
                si_note(p, "unknown_suffix");
                code.replace(QLatin1Char('.'), QLatin1Char('-'));
            } else {
                code = left;
            }
        } else {
            return si_invalid(p, "bad_characters");
        }
    }
    code.replace(QLatin1Char('_'), QLatin1Char('-'));
    const qsizetype slash = code.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        const QString left = code.left(slash);
        const QString right = code.mid(slash + 1);
        if (right.size() == 1 && si_upper(right.at(0)) && si_all_letters(left) && left.size() <= 5) {
            code = left + QLatin1Char('-') + right;
            class_share = true;
        } else {
            return si_invalid(p, "bad_characters");
        }
    }
    if (!class_share) {
        const qsizetype dash = code.lastIndexOf(QLatin1Char('-'));
        if (dash > 0 && dash == code.size() - 2 && si_upper(code.back()) && si_all_letters(code.left(dash)) &&
            dash <= 5 && code.indexOf(QLatin1Char('-')) == dash)
            class_share = true; // BRK-B, BF-B (not BAJAJ-AUTO, not MCDOWELL-N)
    }
    if (single && code.size() > kSiMaxTicker)
        return si_invalid(p, "too_long");
    return si_ticker(p, code, venue, venue_unknown, class_share, home, coin_last);
}

} // namespace syminput_detail
} // namespace

ParsedInput parse_symbol_input(const QString& text, const QString& venue_hint, const QString& home_country) {
    ParsedInput p = si_parse_body(text, venue_hint, home_country);
    // An exchange we do not know never yields a guess (it could be any market).
    if (p.has_note("unknown_venue")) {
        p.candidates.clear();
        p.canonical.clear();
    }
    return p;
}

QString canonical_symbol(const QString& stored, const QString& exchange_hint) {
    const ParsedInput p = parse_symbol_input(stored, exchange_hint);
    QString up = p.cleaned.toUpper();
    // Drop an "EXCH:" prefix (not the ccxt settle of "BTC/USDT:USDT").
    const qsizetype colon = up.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        bool code_like = true;
        for (const QChar c : QStringView(up).left(colon))
            if (!si_upper(c) && !si_digit(c) && c != QLatin1Char('_'))
                code_like = false;
        if (code_like)
            up = up.mid(colon + 1).trimmed();
    }
    const bool has_venue = !p.venue_hint.isEmpty(); // argument, prefix or venue word
    const bool pair_syntax = up.contains(QLatin1Char('/')) || up.contains(QLatin1Char('-')) ||
                             up.endsWith(QLatin1String("=X"));
    switch (p.kind) {
        case InputKind::YahooSymbol:
        case InputKind::Contract:
            return p.canonical;
        case InputKind::FxPair:
            return (has_venue || pair_syntax) ? p.canonical : up;
        case InputKind::CryptoPair:
            if (!has_venue && up.contains(QLatin1Char('-')) && !up.contains(QLatin1Char('/')))
                return up; // a Yahoo crypto symbol is kept verbatim (TON-USD is a real, different coin)
            if (has_venue || up.contains(QLatin1Char('/')))
                return p.canonical.isEmpty() ? p.candidates.value(0) : p.canonical;
            return up; // bare "BTC" / "BTCUSDT" stays bare
        case InputKind::Ticker:
            if (has_venue)
                return p.canonical;
            if (p.has_note("unknown_venue"))
                return {};
            return venue_symbols_detail::yahoo_spelling(p.base); // spelling only: bare stays bare
        case InputKind::Alias:
            if (has_venue)
                return p.candidates.value(0);
            if (si_code_chars_ok(up))
                return venue_symbols_detail::yahoo_spelling(up); // a bare code stays bare ("SPX", "BANKNIFTY")
            return {};
        case InputKind::Empty:
        case InputKind::Invalid:
        case InputKind::Isin:
        case InputKind::Name:
            break;
    }
    return {};
}

} // namespace fincept::core::instrument
