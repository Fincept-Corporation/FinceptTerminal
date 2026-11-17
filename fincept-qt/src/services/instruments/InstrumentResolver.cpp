// InstrumentResolver.cpp — see InstrumentResolver.h. The request state machine, the
// remote-row mappers and the trading facts. Qt Core + core/instrument + core/market
// only (the production sources live in InstrumentResolverSources.cpp, the index
// adapter in InstrumentResolverIndex.cpp). File-scope names carry an isr_ / kIsr
// prefix (unity builds).

#include "services/instruments/InstrumentResolver.h"

#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "core/market/MarketSessions.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace fincept::services {

namespace ci = core::instrument;

namespace {

constexpr const char* kIsrTag = "InstrumentResolver";
constexpr int kIsrMaxLimit = 50;
constexpr int kIsrCacheMax = 256;
constexpr int kIsrMaxRecent = 12;
constexpr int kIsrManyParallel = 4;
constexpr int kIsrYahooGlobalMax = 2;
constexpr int kIsrDeadMax = 2000;

QString isr_tr(const char* text) {
    return QCoreApplication::translate("InstrumentResolver", text);
}

qint64 isr_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// “text” with typographic quotes.
QString isr_quoted(const QString& text) {
    return QString(QChar(0x201C)) + text + QChar(0x201D);
}

/// Factory registered by InstrumentResolverSources.cpp.
ResolverSources (*&isr_default_factory())() {
    static ResolverSources (*factory)() = nullptr;
    return factory;
}

/// Words that do not identify an issuer (legal forms, share-class and depositary words).
const QSet<QString>& isr_issuer_noise() {
    static const QSet<QString> k = {
        QStringLiteral("inc"),        QStringLiteral("incorporated"), QStringLiteral("corp"),
        QStringLiteral("corporation"),QStringLiteral("co"),           QStringLiteral("company"),
        QStringLiteral("ltd"),        QStringLiteral("limited"),      QStringLiteral("plc"),
        QStringLiteral("ag"),         QStringLiteral("se"),           QStringLiteral("sa"),
        QStringLiteral("nv"),         QStringLiteral("spa"),          QStringLiteral("ab"),
        QStringLiteral("asa"),        QStringLiteral("oyj"),          QStringLiteral("kgaa"),
        QStringLiteral("bhd"),        QStringLiteral("berhad"),       QStringLiteral("tbk"),
        QStringLiteral("pcl"),        QStringLiteral("the"),          QStringLiteral("of"),
        QStringLiteral("and"),        QStringLiteral("&"),            QStringLiteral("group"),
        QStringLiteral("holdings"),   QStringLiteral("holding"),      QStringLiteral("adr"),
        QStringLiteral("ads"),        QStringLiteral("gdr"),          QStringLiteral("sponsored"),
        QStringLiteral("unsponsored"),QStringLiteral("depositary"),   QStringLiteral("receipts"),
        QStringLiteral("receipt"),    QStringLiteral("class"),        QStringLiteral("cl"),
        QStringLiteral("shares"),     QStringLiteral("share"),        QStringLiteral("ordinary"),
        QStringLiteral("common"),     QStringLiteral("stock"),        QStringLiteral("reg"),
        QStringLiteral("registered"), QStringLiteral("new"),          QStringLiteral("pref"),
        QStringLiteral("preferred"),  QStringLiteral("shs"),          QStringLiteral("each"),
        QStringLiteral("rep"),        QStringLiteral("representing"), QStringLiteral("cert"),
        QStringLiteral("deposito"),   QStringLiteral("cedear"),
    };
    return k;
}

/// Issuer words of a name: normalised, noise words and single letters / digits removed.
QStringList isr_issuer_words(const QString& name) {
    QStringList out;
    const QStringList words = ci::normalize_name(name).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& w : words) {
        if (w.size() < 2 || isr_issuer_noise().contains(w))
            continue;
        out.push_back(w);
    }
    return out;
}

/// Two records of the same issuer (another listing / depositary receipt of one company):
/// the same ISIN, or the same issuer words (one may extend the other when both have 2+).
bool isr_same_issuer(const ci::Instrument& a, const ci::Instrument& b) {
    if (!a.isin.isEmpty() && a.isin.compare(b.isin, Qt::CaseInsensitive) == 0)
        return true;
    const QStringList wa = isr_issuer_words(a.name);
    const QStringList wb = isr_issuer_words(b.name);
    if (wa.isEmpty() || wb.isEmpty())
        return false;
    if (wa == wb)
        return true;
    const QStringList& shorter = wa.size() < wb.size() ? wa : wb;
    const QStringList& longer = wa.size() < wb.size() ? wb : wa;
    return shorter.size() >= 2 && longer.mid(0, shorter.size()) == shorter;
}

/// matches[0] is an unambiguous exact answer (owner rules G1 / G2, see the header).
bool isr_is_exact(const QVector<ci::RankedInstrument>& ranked, const ci::ParsedInput& p) {
    if (ranked.isEmpty())
        return false;
    const ci::RankedInstrument& top = ranked.first();
    if (!ci::is_exact_match(top.match) || !top.instrument.is_active())
        return false;
    if (top.match == ci::MatchKind::ExactSymbol && !p.canonical.isEmpty() && top.instrument.symbol == p.canonical)
        return true;
    const ci::AssetClass implied = p.preferred_classes.value(0, ci::AssetClass::Unknown);
    for (qsizetype i = 1; i < ranked.size(); ++i) {
        const ci::RankedInstrument& o = ranked.at(i);
        if (!ci::is_exact_match(o.match) || !o.instrument.is_active())
            continue; // a dead listing never competes with a live one
        if (top.match == ci::MatchKind::ExactSymbol && o.match != ci::MatchKind::ExactSymbol)
            continue; // the explicit symbol beats its siblings found by listing code
        if (isr_same_issuer(top.instrument, o.instrument))
            continue; // another listing of the same company
        if (implied != ci::AssetClass::Unknown && top.instrument.asset_class == implied &&
            o.instrument.asset_class != implied)
            continue; // "BTC" is the coin; the same-named ETF is listed second
        return false;
    }
    return true;
}

/// Weight 1..5 from a row's position in a remote reply (the source's own relevance order).
int isr_position_weight(int position) {
    if (position < 2)
        return 5;
    if (position < 5)
        return 4;
    if (position < 10)
        return 3;
    if (position < 20)
        return 2;
    return 1;
}

/// TradingView type / subtype (what /market/search sends) -> class.
ci::AssetClass isr_class_from_tv(const QString& type, const QString& subtype) {
    if (type == QLatin1String("stock") || type == QLatin1String("dr"))
        return ci::AssetClass::Equity;
    if (type == QLatin1String("fund"))
        return subtype == QLatin1String("etf") ? ci::AssetClass::Etf : ci::AssetClass::Fund;
    if (type == QLatin1String("index"))
        return ci::AssetClass::Index;
    if (type == QLatin1String("forex"))
        return ci::AssetClass::Fx;
    if (type == QLatin1String("crypto") || type == QLatin1String("spot") || type == QLatin1String("swap"))
        return ci::AssetClass::Crypto;
    if (type == QLatin1String("futures"))
        return ci::AssetClass::Future;
    if (type == QLatin1String("bond"))
        return ci::AssetClass::Bond;
    if (type == QLatin1String("option") || type == QLatin1String("options"))
        return ci::AssetClass::Option;
    return ci::AssetClass::Unknown;
}

/// Index levels are quoted in the major unit even where the venue's shares trade in minor
/// units (^FTSE is GBP, not GBp).
QString isr_major_currency(const QString& ccy) {
    if (ccy == QLatin1String("GBp") || ccy == QLatin1String("GBX"))
        return QStringLiteral("GBP");
    if (ccy == QLatin1String("ZAc"))
        return QStringLiteral("ZAR");
    if (ccy == QLatin1String("ILA"))
        return QStringLiteral("ILS");
    if (ccy == QLatin1String("KWF"))
        return QStringLiteral("KWD");
    return ccy;
}

/// Fill empty fields of `dst` from `src` (the same instrument from another source).
void isr_fill(ci::Instrument& dst, const ci::Instrument& src) {
    auto fill = [](QString& d, const QString& s) {
        if (d.trimmed().isEmpty() && !s.trimmed().isEmpty())
            d = s;
    };
    fill(dst.id, src.id);
    fill(dst.symbol, src.symbol);
    fill(dst.display, src.display);
    fill(dst.name, src.name);
    fill(dst.venue, src.venue);
    fill(dst.mic, src.mic);
    fill(dst.country, src.country);
    fill(dst.currency, src.currency);
    fill(dst.isin, src.isin);
    fill(dst.figi, src.figi);
    if (dst.asset_class == ci::AssetClass::Unknown)
        dst.asset_class = src.asset_class;
    else if (dst.asset_class == ci::AssetClass::Equity && dst.quote_type.isEmpty() &&
             (src.asset_class == ci::AssetClass::Etf || src.asset_class == ci::AssetClass::Fund))
        dst.asset_class = src.asset_class; // a skeleton guess, the source knows it is a fund
    fill(dst.quote_type, src.quote_type);
    if (dst.rank_weight == 0)
        dst.rank_weight = src.rank_weight;
    for (auto it = src.provider_symbols.constBegin(); it != src.provider_symbols.constEnd(); ++it)
        if (!dst.provider_symbols.contains(it.key()))
            dst.provider_symbols.insert(it.key(), it.value());
    for (const QString& a : src.aliases)
        if (!dst.aliases.contains(a, Qt::CaseInsensitive))
            dst.aliases.push_back(a);
    for (const QString& f : src.former_symbols)
        if (!dst.former_symbols.contains(f, Qt::CaseInsensitive))
            dst.former_symbols.push_back(f);
}

/// The text the backend's /market/search understands for a parsed input (it matches raw
/// TradingView codes and names, so Yahoo forms must be taken apart). Empty = do not ask.
QString isr_backend_query(const ci::ParsedInput& p) {
    QString q;
    switch (p.kind) {
        case ci::InputKind::YahooSymbol:
            // Indices, futures and FX are table instruments (seeded locally); the backend
            // has them under unrelated codes.
            if (p.asset_class == ci::AssetClass::Index || p.asset_class == ci::AssetClass::Future ||
                p.asset_class == ci::AssetClass::Fx)
                return {};
            [[fallthrough]];
        case ci::InputKind::Ticker: {
            q = p.base;
            static const QRegularExpression kClassShare(QStringLiteral("^[A-Z]{1,5}-[A-Z]$"));
            if (kClassShare.match(q).hasMatch())
                q.replace(QLatin1Char('-'), QLatin1Char('.')); // NYSE:BRK.B
            else if (p.venue == QLatin1String("NSE") || p.venue == QLatin1String("BSE"))
                q.replace(QLatin1Char('-'), QLatin1Char('_')); // NSE:BAJAJ_AUTO
            if (p.venue == QLatin1String("HKEX")) {
                while (q.size() > 1 && q.startsWith(QLatin1Char('0')))
                    q.remove(0, 1); // HKEX:700
            }
            break;
        }
        case ci::InputKind::Alias:
        case ci::InputKind::Name:
            q = p.search_text;
            break;
        case ci::InputKind::FxPair:
            q = p.base + p.quote;
            break;
        case ci::InputKind::CryptoPair:
            q = p.base + p.quote;
            break;
        case ci::InputKind::Empty:
        case ci::InputKind::Invalid:
        case ci::InputKind::Isin:
        case ci::InputKind::Contract:
            return {};
    }
    return q.trimmed().left(50);
}

/// The text Yahoo search gets: Yahoo's own form where the parser knows it.
QString isr_yahoo_query(const ci::ParsedInput& p) {
    switch (p.kind) {
        case ci::InputKind::Ticker:
            return p.canonical.isEmpty() ? p.base : p.canonical;
        case ci::InputKind::YahooSymbol:
        case ci::InputKind::FxPair:
            return p.canonical.isEmpty() ? p.candidates.value(0) : p.canonical;
        case ci::InputKind::CryptoPair:
            return p.candidates.value(0, p.base);
        case ci::InputKind::Isin:
            return p.isin;
        case ci::InputKind::Alias:
        case ci::InputKind::Name:
            return p.search_text.left(60);
        case ci::InputKind::Empty:
        case ci::InputKind::Invalid:
        case ci::InputKind::Contract:
            break;
    }
    return {};
}

/// The text `resolve_symbol` validates: the fully specified symbol, else the code (the
/// script fans a bare code out over venues itself).
QString isr_validate_query(const ci::ParsedInput& p) {
    switch (p.kind) {
        case ci::InputKind::Ticker:
        case ci::InputKind::YahooSymbol:
        case ci::InputKind::FxPair:
        case ci::InputKind::CryptoPair:
            if (p.candidates.isEmpty())
                return {};
            return p.canonical.isEmpty() ? (p.kind == ci::InputKind::Ticker ? p.base : p.candidates.first())
                                         : p.canonical;
        default:
            break;
    }
    return {};
}

QString isr_invalid_message(const ci::ParsedInput& p) {
    const QString shown = p.cleaned.left(40);
    if (p.has_note("too_long"))
        return isr_tr("That text is too long to be a symbol or a company name.");
    if (p.has_note("isin_check_digit"))
        return isr_tr("%1 looks like an ISIN, but its check digit is wrong.").arg(isr_quoted(shown));
    if (p.has_note("bad_characters"))
        return isr_tr("%1 contains characters that no symbol uses.").arg(isr_quoted(shown));
    return isr_tr("%1 is not a symbol or a name the terminal can look up.").arg(isr_quoted(shown));
}

QString isr_offline_note() {
    return isr_tr("Online search is unavailable \xe2\x80\x94 showing instruments already known to the terminal.");
}

/// Code part of an id ("XNSE:RELIANCE" -> "RELIANCE").
QString isr_id_code(const QString& id) {
    const qsizetype c = id.indexOf(QLatin1Char(':'));
    return c >= 0 ? id.mid(c + 1) : id;
}

QString isr_recent_key(const QString& key) {
    QString k = key.trimmed();
    if (k.isEmpty())
        k = QStringLiteral("default");
    for (QChar& c : k)
        if (c == QLatin1Char('/') || c == QLatin1Char('\\'))
            c = QLatin1Char('_');
    return k;
}

} // namespace

// ── Free functions ──────────────────────────────────────────────────────────

ci::Instrument complete_instrument(const ci::Instrument& in) {
    ci::Instrument inst = in;
    inst.symbol = inst.symbol.trimmed();
    if (!inst.symbol.isEmpty()) {
        const QString canon = ci::canonical_symbol(inst.symbol);
        if (!canon.isEmpty())
            inst.symbol = canon;
        const ci::Instrument sk = ci::instrument_skeleton(inst.symbol);
        if (sk.is_valid()) {
            auto fill = [](QString& d, const QString& v) {
                if (d.trimmed().isEmpty())
                    d = v;
            };
            fill(inst.id, sk.id);
            fill(inst.display, sk.display);
            fill(inst.venue, sk.venue);
            fill(inst.mic, sk.mic);
            fill(inst.country, sk.country);
            fill(inst.currency, sk.currency);
            fill(inst.name, sk.name);
            if (inst.asset_class == ci::AssetClass::Unknown)
                inst.asset_class = sk.asset_class;
            if (inst.aliases.isEmpty())
                inst.aliases = sk.aliases;
        }
    }
    if (inst.display.trimmed().isEmpty())
        inst.display = inst.symbol.isEmpty() ? isr_id_code(inst.id) : inst.symbol;
    if (inst.currency.isEmpty() && inst.asset_class == ci::AssetClass::Index && !inst.venue.isEmpty()) {
        if (const ci::VenueInfo* v = ci::venue_info_for_code(inst.venue))
            inst.currency = isr_major_currency(v->currency);
    }
    return inst;
}

TradingMeta trading_meta_for(const ci::Instrument& inst) {
    TradingMeta m;
    const core::market::Venue* v = nullptr;
    if (!inst.symbol.isEmpty())
        v = core::market::venue_for_symbol(inst.symbol, inst.venue);
    if (!v && !inst.venue.isEmpty())
        v = core::market::venue_by_code(inst.venue);
    if (!v && !inst.mic.isEmpty())
        v = core::market::venue_by_code(inst.mic);
    if (v) {
        m.venue = v->id;
        m.time_zone = v->tz_id;
    }
    if (ci::is_equity_like(inst.asset_class) && inst.asset_class != ci::AssetClass::Fund) {
        QString country = inst.country.trimmed().toUpper();
        if (country.isEmpty() && v)
            country = v->country.toUpper();
        static const QSet<QString> kUnitOne = {
            QStringLiteral("US"), QStringLiteral("IN"), QStringLiteral("GB"), QStringLiteral("IE"),
            QStringLiteral("DE"), QStringLiteral("FR"), QStringLiteral("NL"), QStringLiteral("BE"),
            QStringLiteral("PT"), QStringLiteral("IT"), QStringLiteral("ES"), QStringLiteral("CH"),
            QStringLiteral("AT"), QStringLiteral("SE"), QStringLiteral("NO"), QStringLiteral("DK"),
            QStringLiteral("FI"), QStringLiteral("CA"), QStringLiteral("AU"), QStringLiteral("NZ"),
            QStringLiteral("KR"),
        };
        static const QSet<QString> kUnitHundred = {
            QStringLiteral("JP"), QStringLiteral("CN"), QStringLiteral("SG"),
            QStringLiteral("MY"), QStringLiteral("TH"), QStringLiteral("ID"),
        };
        if (kUnitOne.contains(country)) {
            m.lot_size = 1.0;
            m.lot_known = true;
        } else if (kUnitHundred.contains(country)) {
            m.lot_size = 100.0;
            m.lot_known = true;
        } else if (country == QLatin1String("TW")) {
            m.lot_size = 1000.0;
            m.lot_known = true;
        }
    }
    return m;
}

ci::Instrument instrument_from_market_search_row(const QJsonObject& row, int position) {
    const QString code = row.value(QStringLiteral("symbol")).toString().trimmed();
    const QString exch = row.value(QStringLiteral("exchange")).toString().trimmed().toUpper();
    QString country = row.value(QStringLiteral("country")).toString().trimmed().toUpper();
    const QString type = row.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QString subtype = row.value(QStringLiteral("subtype")).toString().trimmed().toLower();
    if (code.isEmpty() || type == QLatin1String("economic"))
        return {};
    if (country == QLatin1String("GLOBAL"))
        country.clear();
    const ci::AssetClass cls = isr_class_from_tv(type, subtype);

    QString yahoo = ci::to_yahoo_tv(code, exch, country, cls);
    if (yahoo.isEmpty()) {
        // A newer backend sends its own Yahoo form; trust it only when it is self-describing
        // (a bare code for a non-US venue would quote a different, US company).
        const QString yf = row.value(QStringLiteral("yf_symbol")).toString().trimmed();
        if (!yf.isEmpty() && yf.compare(code, Qt::CaseInsensitive) != 0)
            yahoo = ci::canonical_symbol(yf);
    }

    const ci::VenueInfo* vinfo = exch.isEmpty() ? nullptr : ci::venue_info_for_code(exch, ci::CodeSpace::TradingView, country);
    ci::Instrument inst;
    if (!yahoo.isEmpty())
        inst = ci::instrument_skeleton(yahoo);
    if (!inst.is_valid()) {
        // No Yahoo symbol: kept (no quote data), with an id of its own.
        inst = ci::Instrument{};
        const QString prefix = (vinfo && !vinfo->mic.isEmpty()) ? vinfo->mic : QStringLiteral("TV.") + exch;
        inst.id = prefix + QLatin1Char(':') + code.toUpper();
        inst.display = code.toUpper();
        inst.venue = (vinfo && !vinfo->venue.isEmpty()) ? vinfo->venue : exch;
        inst.mic = vinfo ? vinfo->mic : QString();
        inst.asset_class = cls;
    }
    inst.name = row.value(QStringLiteral("name")).toString().trimmed();
    if (cls != ci::AssetClass::Unknown &&
        (inst.asset_class == ci::AssetClass::Unknown ||
         (inst.asset_class == ci::AssetClass::Equity && (cls == ci::AssetClass::Etf || cls == ci::AssetClass::Fund))))
        inst.asset_class = cls;
    inst.quote_type = subtype == QLatin1String("cfd") ? QStringLiteral("cfd") : type;
    if (inst.currency.isEmpty())
        inst.currency = row.value(QStringLiteral("currency")).toString().trimmed();
    if (inst.venue.isEmpty() && vinfo) {
        inst.venue = vinfo->venue;
        if (inst.mic.isEmpty())
            inst.mic = vinfo->mic;
    }
    if (inst.country.isEmpty() && inst.asset_class != ci::AssetClass::Fx && inst.asset_class != ci::AssetClass::Crypto)
        inst.country = country;
    if (!exch.isEmpty())
        inst.provider_symbols.insert(QStringLiteral("tv"), exch + QLatin1Char(':') + code);
    inst.rank_weight = isr_position_weight(position);
    return inst;
}

QVector<ci::Instrument> instruments_from_market_search(const QJsonDocument& body) {
    QJsonArray rows;
    if (body.isArray()) {
        rows = body.array();
    } else if (body.isObject()) {
        const QJsonObject o = body.object();
        rows = o.contains(QStringLiteral("results")) ? o.value(QStringLiteral("results")).toArray()
                                                     : o.value(QStringLiteral("data")).toArray();
    }
    QVector<ci::Instrument> out;
    out.reserve(rows.size());
    int position = 0;
    for (const auto& v : rows) {
        ci::Instrument inst = instrument_from_market_search_row(v.toObject(), position++);
        if (inst.is_valid())
            out.push_back(std::move(inst));
    }
    return out;
}

ci::Instrument instrument_from_yahoo_row(const QJsonObject& row, int position) {
    const QString raw = row.value(QStringLiteral("symbol")).toString().trimmed();
    if (raw.isEmpty() || ci::is_valid_isin(raw.toUpper()))
        return {};
    const QString type = row.value(QStringLiteral("type")).toString().trimmed();
    QString symbol = ci::canonical_symbol(raw);
    if (symbol.isEmpty())
        symbol = raw.toUpper();
    ci::Instrument inst = ci::instrument_skeleton(symbol);
    if (!inst.is_valid()) {
        // A Yahoo suffix the venue table does not know: still quotable on Yahoo.
        inst = ci::Instrument{};
        inst.symbol = symbol;
        inst.id = QStringLiteral("YAHOO:") + symbol;
        inst.display = symbol;
    }
    const ci::AssetClass cls = ci::asset_class_of(symbol, type);
    if (inst.asset_class == ci::AssetClass::Unknown ||
        (inst.asset_class == ci::AssetClass::Equity && cls != ci::AssetClass::Unknown && cls != ci::AssetClass::Equity))
        inst.asset_class = cls;
    inst.quote_type = type;
    inst.name = row.value(QStringLiteral("name")).toString().trimmed();
    if (inst.currency.isEmpty())
        inst.currency = row.value(QStringLiteral("currency")).toString().trimmed();
    if (inst.venue.isEmpty()) {
        const QString code = row.value(QStringLiteral("exchange")).toString().trimmed();
        if (!code.isEmpty()) {
            if (const ci::VenueInfo* v = ci::venue_info_for_code(code, ci::CodeSpace::Yahoo)) {
                inst.venue = v->venue;
                if (inst.mic.isEmpty())
                    inst.mic = v->mic;
            }
        }
    }
    inst.rank_weight = isr_position_weight(position);
    return inst;
}

QVector<ci::Instrument> instruments_from_yahoo_search(const QJsonObject& reply, const QString& isin_query) {
    QVector<ci::Instrument> out;
    const QJsonArray rows = reply.value(QStringLiteral("results")).toArray();
    const QString isin = isin_query.trimmed().toUpper();
    int position = 0;
    for (const auto& v : rows) {
        ci::Instrument inst = instrument_from_yahoo_row(v.toObject(), position++);
        if (!inst.is_valid())
            continue;
        if (!isin.isEmpty() && ci::is_valid_isin(isin))
            inst.isin = isin; // Yahoo matched every row by this ISIN
        out.push_back(std::move(inst));
    }
    return out;
}

// ── Request state ───────────────────────────────────────────────────────────

struct InstrumentResolver::Request {
    quint64 id = 0;
    Mode mode = Mode::Search;
    QString text;
    QString stored_hint;
    ResolveOptions opts;
    ci::ParsedInput parsed;
    ci::RankContext ctx;
    int limit = 12;
    QPointer<QObject> context;
    QObject* context_key = nullptr;
    bool has_context = false;
    Callback cb;

    QVector<Instrument> pool; ///< every candidate so far, one per instrument
    quint64 local_id = 0;
    bool local_done = false;
    QString local_error;
    QString backend_query;
    QString yahoo_query;
    bool remote_started = false;
    bool backend_done = false;
    bool backend_ok = false;
    bool yahoo_done = false;
    bool validate_done = false;
    bool from_remote = false;
    bool offline = false;
    QString last_signature;
};

struct InstrumentResolver::ContextState {
    quint64 search_id = 0;
    QTimer* timer = nullptr;
    bool backend_busy = false;
    bool yahoo_busy = false;
    quint64 backend_waiting = 0;
    quint64 yahoo_waiting = 0;
    QMetaObject::Connection destroyed;
};

struct InstrumentResolver::Batch {
    quint64 id = 0;
    QStringList texts;
    ResolveOptions opts;
    QVector<ResolveResult> results;
    QVector<quint64> sub_ids;
    int next = 0;
    int done = 0;
    int running = 0;
    QPointer<QObject> context;
    bool has_context = false;
    ListCallback cb;
};

// ── Construction ────────────────────────────────────────────────────────────

bool InstrumentResolver::register_default_sources(ResolverSources (*factory)()) {
    isr_default_factory() = factory;
    return true;
}

InstrumentResolver& InstrumentResolver::instance() {
    // Deliberately leaked: it must outlive every widget and the services its sources use.
    static InstrumentResolver* s = [] {
        auto factory = isr_default_factory();
        auto* r = new InstrumentResolver(factory ? factory() : ResolverSources{});
        if (QCoreApplication* app = QCoreApplication::instance(); app && r->thread() != app->thread())
            r->moveToThread(app->thread());
        return r;
    }();
    return *s;
}

InstrumentResolver::InstrumentResolver(ResolverSources sources, QObject* parent)
    : QObject(parent), sources_(std::move(sources)) {}

InstrumentResolver::~InstrumentResolver() {
    for (auto it = contexts_.begin(); it != contexts_.end(); ++it)
        QObject::disconnect((*it)->destroyed);
}

// ── Public entry points ─────────────────────────────────────────────────────

quint64 InstrumentResolver::search(const QString& text, const ResolveOptions& opts, QObject* context, Callback cb) {
    return start_request(Mode::Search, text, opts, context, std::move(cb));
}

quint64 InstrumentResolver::resolve(const QString& text, const ResolveOptions& opts, QObject* context, Callback cb) {
    return start_request(Mode::Resolve, text, opts, context, std::move(cb));
}

quint64 InstrumentResolver::resolve_stored(const QString& stored_symbol, const QString& exchange_hint, QObject* context,
                                           Callback cb) {
    ResolveOptions opts;
    opts.venue_hint = exchange_hint;
    opts.allow_remote = false;
    return start_request(Mode::Stored, stored_symbol, opts, context, std::move(cb));
}

quint64 InstrumentResolver::start_request(Mode mode, const QString& text, const ResolveOptions& opts, QObject* context,
                                          Callback cb) {
    auto r = std::make_shared<Request>();
    r->id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    r->mode = mode;
    r->text = text;
    r->opts = opts;
    if (mode == Mode::Stored)
        r->stored_hint = opts.venue_hint;
    r->context = context;
    r->context_key = context;
    r->has_context = context != nullptr;
    r->cb = std::move(cb);
    if (QThread::currentThread() != thread())
        QMetaObject::invokeMethod(this, [this, r]() { begin(r); }, Qt::QueuedConnection);
    else
        begin(r);
    return r->id;
}

void InstrumentResolver::cancel(quint64 id) {
    if (id == 0)
        return;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, id]() { cancel(id); }, Qt::QueuedConnection);
        return;
    }
    if (dead_.size() > kIsrDeadMax)
        dead_.clear();
    dead_.insert(id);
    if (auto b = batches_.take(id)) {
        for (quint64 sub : std::as_const(b->sub_ids))
            cancel(sub);
        return;
    }
    if (RequestPtr r = requests_.take(id)) {
        if (r->local_id && !r->local_done && sources_.local_cancel)
            sources_.local_cancel(r->local_id);
    }
}

void InstrumentResolver::learn(const Instrument& picked) {
    if (!sources_.learn)
        return;
    const Instrument inst = complete_instrument(picked);
    // The index is keyed by market-data symbols: records without one are not learned.
    if (!is_quotable(inst) || ci::instrument_id_for(inst.symbol).isEmpty())
        return;
    sources_.learn(inst);
}

void InstrumentResolver::remember_pick(const QString& key, const Instrument& picked) {
    learn(picked);
    if (!sources_.load_recent || !sources_.save_recent || !picked.is_valid())
        return;
    const QString k = isr_recent_key(key);
    const Instrument inst = complete_instrument(picked);
    QStringList out;
    out.push_back(QString::fromUtf8(QJsonDocument(inst.to_json()).toJson(QJsonDocument::Compact)));
    for (const QString& js : sources_.load_recent(k)) {
        const Instrument old = Instrument::from_json(QJsonDocument::fromJson(js.toUtf8()).object());
        if (!old.is_valid() || old.same_instrument(inst))
            continue;
        out.push_back(js);
        if (out.size() >= kIsrMaxRecent)
            break;
    }
    sources_.save_recent(k, out);
}

QVector<ci::Instrument> InstrumentResolver::recent_picks(const QString& key, int limit) const {
    QVector<Instrument> out;
    if (!sources_.load_recent)
        return out;
    for (const QString& js : sources_.load_recent(isr_recent_key(key))) {
        const Instrument inst = Instrument::from_json(QJsonDocument::fromJson(js.toUtf8()).object());
        if (!inst.is_valid())
            continue;
        out.push_back(complete_instrument(inst));
        if (limit > 0 && out.size() >= limit)
            break;
    }
    return out;
}

void InstrumentResolver::clear_cache() {
    cache_.clear();
    backend_down_until_ms_ = 0;
    yahoo_down_until_ms_ = 0;
}

quint64 InstrumentResolver::resolve_many(const QStringList& texts, const ResolveOptions& opts, QObject* context,
                                         ListCallback cb) {
    auto b = std::make_shared<Batch>();
    b->id = next_id_.fetch_add(1, std::memory_order_relaxed) + 1;
    b->texts = texts;
    b->opts = opts;
    b->results.resize(texts.size());
    b->context = context;
    b->has_context = context != nullptr;
    b->cb = std::move(cb);
    auto start = [this, b]() {
        batches_.insert(b->id, b);
        if (b->texts.isEmpty()) {
            batches_.remove(b->id);
            QObject* target = b->has_context ? b->context.data() : this;
            if (!target)
                return;
            QMetaObject::invokeMethod(
                target, [b]() { if (b->cb) b->cb(b->results); }, Qt::QueuedConnection);
            return;
        }
        pump_batch(b);
    };
    if (QThread::currentThread() != thread())
        QMetaObject::invokeMethod(this, start, Qt::QueuedConnection);
    else
        start();
    return b->id;
}

void InstrumentResolver::pump_batch(const std::shared_ptr<Batch>& b) {
    while (b->running < kIsrManyParallel && b->next < b->texts.size()) {
        const int index = b->next++;
        ++b->running;
        const quint64 sub = resolve(b->texts.at(index), b->opts, this, [this, b, index](const ResolveResult& res) {
            if (!batches_.contains(b->id))
                return; // cancelled
            b->results[index] = res;
            --b->running;
            ++b->done;
            if (b->done >= b->texts.size()) {
                batches_.remove(b->id);
                if (b->has_context && !b->context)
                    return;
                if (b->cb)
                    b->cb(b->results);
                return;
            }
            pump_batch(b);
        });
        b->sub_ids.push_back(sub);
    }
}

// ── Request machinery ───────────────────────────────────────────────────────

InstrumentResolver::RequestPtr InstrumentResolver::live(quint64 id) const {
    return requests_.value(id);
}

QString InstrumentResolver::effective_home(const ResolveOptions& opts) const {
    QString home = opts.home_country.trimmed().toUpper();
    if (home.isEmpty())
        home = opts.ctx.home_country.trimmed().toUpper();
    if (home.isEmpty() && sources_.default_home_country)
        home = sources_.default_home_country().trimmed().toUpper();
    return home;
}

InstrumentResolver::ContextState& InstrumentResolver::context_state(QObject* context) {
    auto it = contexts_.find(context);
    if (it != contexts_.end())
        return **it;
    auto st = std::make_shared<ContextState>();
    st->timer = new QTimer(this);
    st->timer->setSingleShot(true);
    connect(st->timer, &QTimer::timeout, this, [this, context]() {
        auto cit = contexts_.find(context);
        if (cit == contexts_.end())
            return;
        RequestPtr r = live((*cit)->search_id);
        if (r && !r->remote_started) {
            r->remote_started = true;
            run_remote_chain(r);
        }
    });
    st->destroyed = connect(context, &QObject::destroyed, this, [this, context]() { forget_context(context); });
    contexts_.insert(context, st);
    return *st;
}

void InstrumentResolver::forget_context(QObject* context) {
    auto st = contexts_.take(context);
    if (!st)
        return;
    QObject::disconnect(st->destroyed);
    if (st->timer)
        st->timer->deleteLater();
    if (st->search_id)
        cancel(st->search_id);
}

void InstrumentResolver::begin(const RequestPtr& r) {
    if (r->has_context && !r->context)
        return; // the asker is gone
    if (dead_.contains(r->id))
        return; // cancelled before it started
    r->limit = std::clamp(r->opts.limit, 1, kIsrMaxLimit);
    r->ctx = r->opts.ctx;
    r->ctx.home_country = effective_home(r->opts);
    r->ctx.limit = r->limit;
    requests_.insert(r->id, r);

    if (r->mode == Mode::Search && r->has_context) {
        ContextState& st = context_state(r->context_key);
        if (st.search_id && st.search_id != r->id)
            cancel(st.search_id); // the newer text wins
        st.search_id = r->id;
        st.timer->stop();
    }
    if (r->mode == Mode::Stored) {
        run_stored(r);
        return;
    }
    r->parsed = ci::parse_symbol_input(r->text, r->opts.venue_hint, r->ctx.home_country);
    if (r->parsed.kind == ci::InputKind::Empty) {
        ResolveResult res = build_result(*r, true);
        if (r->mode == Mode::Resolve)
            res.error = isr_tr("Type a symbol or a company name.");
        deliver_result(r, std::move(res), true);
        return;
    }
    if (r->parsed.kind == ci::InputKind::Invalid) {
        ResolveResult res = build_result(*r, true);
        res.error = isr_invalid_message(r->parsed);
        deliver_result(r, std::move(res), true);
        return;
    }
    r->backend_query = isr_backend_query(r->parsed);
    r->yahoo_query = isr_yahoo_query(r->parsed);
    run_local(r);
}

void InstrumentResolver::run_local(const RequestPtr& r) {
    const quint64 id = r->id;
    if (!sources_.local_search) {
        QMetaObject::invokeMethod(
            this, [this, id]() { on_local(id, {}, QString()); }, Qt::QueuedConnection);
        return;
    }
    ResolverSources::LocalQuery q;
    q.parsed = r->parsed;
    q.venue_hint = r->opts.venue_hint;
    q.ctx = r->ctx;
    q.ctx.limit = 0;
    // The class filter is applied when the merged list is ranked; the pool keeps the other
    // classes so the answer can say "not one of the types this field accepts".
    q.ctx.allowed_classes.clear();
    q.limit = std::min(kIsrMaxLimit, std::max(r->limit * 2, r->mode == Mode::Resolve ? 20 : r->limit));
    if (r->mode == Mode::Search && r->has_context)
        q.channel = QStringLiteral("isr:%1").arg(reinterpret_cast<quintptr>(r->context_key), 0, 16);
    r->local_id = sources_.local_search(
        q, this, [this, id](const QVector<ci::RankedInstrument>& hits, const QString& error) { on_local(id, hits, error); });
    if (r->mode == Mode::Resolve && sources_.local_timeout_ms > 0) {
        QTimer::singleShot(sources_.local_timeout_ms, this, [this, id]() {
            RequestPtr x = live(id);
            if (x && !x->local_done)
                on_local(id, {}, isr_tr("The symbol index did not answer in time."));
        });
    }
}

bool InstrumentResolver::remote_possible(const Request& r) const {
    if (!r.opts.allow_remote)
        return false;
    if (r.mode == Mode::Search && r.parsed.cleaned.size() < 2)
        return false; // one character: local only while typing
    const bool backend = !r.backend_query.isEmpty() && static_cast<bool>(sources_.backend_search);
    const bool yahoo = !r.yahoo_query.isEmpty() && static_cast<bool>(sources_.yahoo_search);
    const bool validate = r.mode == Mode::Resolve && static_cast<bool>(sources_.yahoo_validate) &&
                          !isr_validate_query(r.parsed).isEmpty();
    return backend || yahoo || validate;
}

void InstrumentResolver::on_local(quint64 id, const QVector<ci::RankedInstrument>& hits, const QString& error) {
    RequestPtr r = live(id);
    if (!r || r->local_done)
        return;
    r->local_done = true;
    r->local_error = error;
    QVector<Instrument> rows;
    rows.reserve(hits.size());
    for (const ci::RankedInstrument& h : hits)
        rows.push_back(h.instrument);
    merge_into_pool(*r, rows, false);

    const bool remote = remote_possible(*r);
    if (r->mode == Mode::Search) {
        deliver(r, !remote);
        if (!remote)
            return;
        if (r->has_context) {
            ContextState& st = context_state(r->context_key);
            st.timer->start(std::max(0, sources_.debounce_ms));
        } else {
            QTimer::singleShot(std::max(0, sources_.debounce_ms), this, [this, id]() {
                RequestPtr x = live(id);
                if (x && !x->remote_started) {
                    x->remote_started = true;
                    run_remote_chain(x);
                }
            });
        }
        return;
    }
    // Resolve: stop at an unambiguous exact local answer — for a bare code only once the
    // backend has been asked too (the local index cannot know a same-code company elsewhere:
    // TCS is also TECSYS on the TSX).
    const bool bare = r->parsed.canonical.isEmpty() && !r->backend_query.isEmpty() &&
                      static_cast<bool>(sources_.backend_search) && isr_now_ms() >= backend_down_until_ms_;
    if (!remote || (!bare && build_result(*r, false).exact)) {
        deliver(r, true);
        return;
    }
    r->remote_started = true;
    run_remote_chain(r);
}

void InstrumentResolver::run_remote_chain(const RequestPtr& r) {
    const bool backend = !r->backend_query.isEmpty() && static_cast<bool>(sources_.backend_search) &&
                         r->opts.allow_remote;
    if (backend && !r->backend_done) {
        if (isr_now_ms() < backend_down_until_ms_) {
            r->backend_done = true;
            r->offline = true; // skipped: unreachable a moment ago
        } else {
            run_backend(r);
            return;
        }
    }
    continue_after_backend(r);
}

void InstrumentResolver::continue_after_backend(const RequestPtr& r) {
    if (wants_yahoo(*r)) {
        if (r->mode == Mode::Search && r->backend_done)
            deliver(r, false); // show the backend's rows while Yahoo is asked
        run_yahoo(r);
        return;
    }
    if (r->mode == Mode::Resolve && wants_validate(*r)) {
        run_validate(r);
        return;
    }
    deliver(r, true);
}

bool InstrumentResolver::wants_yahoo(const Request& r) const {
    if (!r.opts.allow_remote || r.yahoo_done || r.yahoo_query.isEmpty() || !sources_.yahoo_search)
        return false;
    if (isr_now_ms() < yahoo_down_until_ms_)
        return false;
    if (r.parsed.kind == ci::InputKind::Isin)
        return true; // the backend has no ISINs
    const ResolveResult cur = build_result(r, false);
    for (const ci::RankedInstrument& h : cur.ranked)
        if (ci::is_exact_match(h.match))
            return false; // exact hits are listed already (several of them: the list is the answer)
    if (r.mode == Mode::Search) {
        if (r.parsed.cleaned.size() < 3 || yahoo_in_flight_ >= kIsrYahooGlobalMax)
            return false;
        return !r.backend_ok || cur.matches.size() < 3;
    }
    return true;
}

bool InstrumentResolver::wants_validate(const Request& r) const {
    if (r.mode != Mode::Resolve || r.validate_done || !r.opts.allow_remote || !sources_.yahoo_validate)
        return false;
    if (isr_now_ms() < yahoo_down_until_ms_)
        return false;
    if (isr_validate_query(r.parsed).isEmpty())
        return false;
    // Only when nothing matched exactly: confirming one candidate is slow (up to 20 s).
    for (const ci::RankedInstrument& h : build_result(r, false).ranked)
        if (ci::is_exact_match(h.match))
            return false;
    return true;
}

void InstrumentResolver::run_backend(const RequestPtr& r) {
    const int bl = std::clamp(r->limit * 2, 20, kIsrMaxLimit);
    const QString key = QStringLiteral("b|%1|%2").arg(r->backend_query.toUpper()).arg(bl);
    ResolverRemoteReply cached;
    if (cache_get(key, &cached)) {
        on_backend(r->id, cached);
        return;
    }
    std::weak_ptr<ContextState> wst;
    if (r->mode == Mode::Search && r->has_context) {
        auto it = contexts_.find(r->context_key);
        if (it != contexts_.end()) {
            if ((*it)->backend_busy) {
                (*it)->backend_waiting = r->id; // the newest text goes next
                return;
            }
            (*it)->backend_busy = true;
            wst = *it;
        }
    }
    const quint64 id = r->id;
    sources_.backend_search(r->backend_query, bl, this, [this, id, key, wst](const ResolverRemoteReply& rep) {
        if (rep.ok)
            cache_put(key, rep);
        if (rep.unreachable) {
            backend_down_until_ms_ = isr_now_ms() + sources_.backend_retry_ms;
            LOG_WARN(kIsrTag, QStringLiteral("backend symbol search unreachable: %1").arg(rep.error.left(200)));
        }
        on_backend(id, rep);
        if (auto st = wst.lock()) {
            st->backend_busy = false;
            const quint64 waiting = std::exchange(st->backend_waiting, 0);
            if (RequestPtr w = live(waiting); w && !w->backend_done)
                run_backend(w);
        }
    });
}

void InstrumentResolver::on_backend(quint64 id, const ResolverRemoteReply& rep) {
    RequestPtr r = live(id);
    if (!r || r->backend_done)
        return;
    r->backend_done = true;
    if (rep.ok) {
        r->backend_ok = true;
        if (!rep.rows.isEmpty())
            r->from_remote = true;
        merge_into_pool(*r, rep.rows, true);
    } else if (rep.unreachable) {
        r->offline = true;
    }
    if (r->mode == Mode::Resolve && build_result(*r, false).exact) {
        deliver(r, true);
        return;
    }
    continue_after_backend(r);
}

void InstrumentResolver::run_yahoo(const RequestPtr& r) {
    const int yl = std::clamp(r->limit, 8, 20);
    const QString key = QStringLiteral("y|%1|%2").arg(r->yahoo_query.toUpper()).arg(yl);
    ResolverRemoteReply cached;
    if (cache_get(key, &cached)) {
        on_yahoo(r->id, cached);
        return;
    }
    std::weak_ptr<ContextState> wst;
    if (r->mode == Mode::Search && r->has_context) {
        auto it = contexts_.find(r->context_key);
        if (it != contexts_.end()) {
            if ((*it)->yahoo_busy) {
                (*it)->yahoo_waiting = r->id;
                return;
            }
            (*it)->yahoo_busy = true;
            wst = *it;
        }
    }
    const quint64 id = r->id;
    ++yahoo_in_flight_;
    sources_.yahoo_search(r->yahoo_query, yl, this, [this, id, key, wst](const ResolverRemoteReply& rep) {
        yahoo_in_flight_ = std::max(0, yahoo_in_flight_ - 1);
        if (rep.ok)
            cache_put(key, rep);
        if (rep.unreachable) {
            yahoo_down_until_ms_ = isr_now_ms() + sources_.yahoo_retry_ms;
            LOG_WARN(kIsrTag, QStringLiteral("Yahoo symbol search unavailable: %1").arg(rep.error.left(200)));
        }
        if (rep.ok) {
            // Yahoo's own symbols: learned so the next search finds them locally.
            for (const Instrument& inst : rep.rows)
                learn(inst);
        }
        on_yahoo(id, rep);
        if (auto st = wst.lock()) {
            st->yahoo_busy = false;
            const quint64 waiting = std::exchange(st->yahoo_waiting, 0);
            if (RequestPtr w = live(waiting); w && !w->yahoo_done && wants_yahoo(*w))
                run_yahoo(w);
        }
    });
}

void InstrumentResolver::on_yahoo(quint64 id, const ResolverRemoteReply& rep) {
    RequestPtr r = live(id);
    if (!r || r->yahoo_done)
        return;
    r->yahoo_done = true;
    if (rep.ok) {
        if (!rep.rows.isEmpty())
            r->from_remote = true;
        merge_into_pool(*r, rep.rows, true);
    } else if (rep.unreachable && !r->backend_ok) {
        r->offline = true;
    }
    if (r->mode == Mode::Resolve && wants_validate(*r)) {
        run_validate(r);
        return;
    }
    deliver(r, true);
}

void InstrumentResolver::run_validate(const RequestPtr& r) {
    r->validate_done = true;
    const QString q = isr_validate_query(r->parsed);
    const QString key = QStringLiteral("v|%1").arg(q.toUpper());
    ResolverRemoteReply cached;
    if (cache_get(key, &cached)) {
        on_validate(r->id, cached);
        return;
    }
    const quint64 id = r->id;
    sources_.yahoo_validate(q, this, [this, id, key](const ResolverRemoteReply& rep) {
        if (rep.ok)
            cache_put(key, rep);
        if (rep.unreachable)
            yahoo_down_until_ms_ = isr_now_ms() + sources_.yahoo_retry_ms;
        on_validate(id, rep);
    });
}

void InstrumentResolver::on_validate(quint64 id, const ResolverRemoteReply& rep) {
    RequestPtr r = live(id);
    if (!r)
        return;
    if (rep.ok && !rep.rows.isEmpty()) {
        const Instrument confirmed = complete_instrument(rep.rows.first());
        if (rep.first_confirmed && is_quotable(confirmed))
            learn(confirmed); // Yahoo returned data for it
        r->from_remote = true;
        merge_into_pool(*r, rep.rows, true);
    } else if (rep.unreachable && !r->backend_ok) {
        r->offline = true;
    }
    deliver(r, true);
}

void InstrumentResolver::run_stored(const RequestPtr& r) {
    const QString raw = ci::clean_symbol_text(r->text);
    const quint64 id = r->id;
    if (raw.isEmpty()) {
        ResolveResult res;
        res.query = r->text;
        res.request_id = id;
        res.error = isr_tr("No symbol is stored.");
        deliver_result(r, std::move(res), true);
        return;
    }
    const QString upper = raw.toUpper();
    if (ci::is_valid_isin(upper)) {
        if (!sources_.local_lookup) {
            ResolveResult res;
            res.query = r->text;
            res.request_id = id;
            res.note = isr_tr("%1 is an ISIN; the symbol index is not available to look it up.").arg(upper);
            deliver_result(r, std::move(res), true);
            return;
        }
        sources_.local_lookup(upper, ResolverSources::LookupMode::Isin, this,
                              [this, id, upper](const QVector<Instrument>& found, const QString& error) {
                                  RequestPtr x = live(id);
                                  if (!x)
                                      return;
                                  ResolveResult res;
                                  res.query = x->text;
                                  res.request_id = id;
                                  for (const Instrument& f : found) {
                                      const Instrument c = complete_instrument(f);
                                      res.matches.push_back(c);
                                      ci::RankedInstrument ri;
                                      ri.instrument = c;
                                      ri.match = ci::MatchKind::ExactIsin;
                                      ri.matched_text = upper;
                                      res.ranked.push_back(ri);
                                  }
                                  res.exact = res.matches.size() == 1;
                                  if (res.matches.isEmpty())
                                      res.note = isr_tr("No instrument with ISIN %1 is known to the terminal yet.").arg(upper);
                                  else if (res.matches.size() > 1)
                                      res.note = isr_tr("ISIN %1 is listed on more than one exchange \xe2\x80\x94 pick the listing.")
                                                     .arg(upper);
                                  if (res.matches.isEmpty() && !error.isEmpty())
                                      res.error = error;
                                  deliver_result(x, std::move(res), true);
                              });
        return;
    }

    const QString canon = ci::canonical_symbol(raw, r->stored_hint);
    if (canon.isEmpty()) {
        ResolveResult res;
        res.query = r->text;
        res.request_id = id;
        res.note = r->stored_hint.trimmed().isEmpty()
                       ? isr_tr("%1 is not a symbol the terminal can price.").arg(isr_quoted(raw))
                       : isr_tr("%1 on %2 is not a symbol the terminal can price.")
                             .arg(isr_quoted(raw), r->stored_hint.trimmed().toUpper());
        deliver_result(r, std::move(res), true);
        return;
    }

    auto answer = [this, id, canon](const QVector<Instrument>& found, const QString& error) {
        RequestPtr x = live(id);
        if (!x)
            return;
        ResolveResult res;
        res.query = x->text;
        res.request_id = id;
        if (!found.isEmpty()) {
            for (qsizetype i = 0; i < found.size(); ++i) {
                const Instrument c = complete_instrument(found.at(i));
                res.matches.push_back(c);
                ci::RankedInstrument ri;
                ri.instrument = c;
                ri.match = i == 0 ? ci::MatchKind::ExactSymbol : ci::MatchKind::ExactAlias;
                ri.matched_text = i == 0 ? canon : res.matches.first().display;
                ri.via_former_symbol = i > 0;
                res.ranked.push_back(ri);
            }
            res.exact = true;
            const Instrument& rec = res.matches.first();
            if (rec.status == ci::InstrumentStatus::Renamed) {
                res.note = res.matches.size() > 1
                               ? isr_tr("%1 is now %2.").arg(rec.display, res.matches.at(1).display)
                               : isr_tr("%1 was renamed; its new symbol is not known yet.").arg(rec.display);
            } else if (rec.status == ci::InstrumentStatus::Delisted) {
                res.note = isr_tr("%1 is delisted \xe2\x80\x94 it no longer trades.").arg(rec.display);
            }
        } else {
            const Instrument sk = ci::instrument_skeleton(canon);
            if (sk.is_valid()) {
                res.matches.push_back(complete_instrument(sk));
                ci::RankedInstrument ri;
                ri.instrument = res.matches.first();
                ri.match = ci::MatchKind::ExactSymbol;
                ri.matched_text = canon;
                res.ranked.push_back(ri);
                const ci::YahooParts parts = ci::from_yahoo(canon);
                const bool bare = !parts.venue_from_suffix && parts.cls == ci::AssetClass::Equity &&
                                  x->stored_hint.trimmed().isEmpty();
                res.exact = !bare;
                if (bare)
                    res.note = isr_tr("%1 has no exchange, so it is read as a US symbol. If you meant another "
                                      "listing, pick it again.")
                                   .arg(canon);
            } else {
                res.note = isr_tr("%1 is not a symbol the terminal can price.").arg(isr_quoted(canon));
            }
            if (!error.isEmpty())
                LOG_WARN(kIsrTag, QStringLiteral("stored symbol lookup: %1").arg(error));
        }
        deliver_result(x, std::move(res), true);
    };
    if (!sources_.local_lookup) {
        QMetaObject::invokeMethod(this, [answer]() { answer({}, QString()); }, Qt::QueuedConnection);
        return;
    }
    sources_.local_lookup(canon, ResolverSources::LookupMode::Current, this, answer);
}

void InstrumentResolver::merge_into_pool(Request& r, const QVector<Instrument>& rows, bool remote) {
    Q_UNUSED(remote);
    for (const Instrument& in : rows) {
        Instrument inst = complete_instrument(in);
        if (!inst.is_valid())
            continue;
        bool merged = false;
        for (Instrument& p : r.pool) {
            if (p.same_instrument(inst)) {
                isr_fill(p, inst);
                merged = true;
                break;
            }
        }
        if (!merged)
            r.pool.push_back(std::move(inst));
    }
}

ResolveResult InstrumentResolver::build_result(const Request& r, bool final) const {
    ResolveResult res;
    res.query = r.text;
    res.request_id = r.id;
    res.final = final;
    res.from_remote = r.from_remote;
    res.offline = r.offline;
    if (r.parsed.kind == ci::InputKind::Empty || r.parsed.kind == ci::InputKind::Invalid)
        return res;

    res.ranked = ci::rank_instruments(r.pool, r.parsed, r.ctx);
    res.matches.reserve(res.ranked.size());
    for (const ci::RankedInstrument& h : res.ranked)
        res.matches.push_back(h.instrument);
    res.exact = isr_is_exact(res.ranked, r.parsed);

    const QString shown = r.parsed.cleaned.left(40);
    QStringList notes;
    if (!res.ranked.isEmpty()) {
        const ci::RankedInstrument& top = res.ranked.first();
        if (top.via_former_symbol) {
            notes << isr_tr("%1 is now %2.").arg(top.matched_text.toUpper(), top.instrument.display);
        } else if (top.instrument.status == ci::InstrumentStatus::Renamed) {
            QString successor;
            for (const ci::RankedInstrument& h : res.ranked)
                if (!top.instrument.successor_id.isEmpty() && h.instrument.id == top.instrument.successor_id)
                    successor = h.instrument.display;
            notes << (successor.isEmpty() ? isr_tr("%1 was renamed.").arg(top.instrument.display)
                                          : isr_tr("%1 is now %2.").arg(top.instrument.display, successor));
        } else if (top.instrument.status == ci::InstrumentStatus::Delisted) {
            notes << isr_tr("%1 is delisted \xe2\x80\x94 it no longer trades.").arg(top.instrument.display);
        } else if (top.match == ci::MatchKind::Fuzzy) {
            notes << isr_tr("No exact match for %1 \xe2\x80\x94 did you mean %2?").arg(isr_quoted(shown), top.instrument.display);
        } else if (!res.exact && r.mode == Mode::Resolve && final) {
            notes << (ci::is_exact_match(top.match)
                          ? isr_tr("%1 matches more than one instrument \xe2\x80\x94 pick one.").arg(isr_quoted(shown))
                          : isr_tr("No exact match for %1 \xe2\x80\x94 the closest matches are listed.").arg(isr_quoted(shown)));
        }
    } else if (final) {
        if (r.parsed.kind == ci::InputKind::Contract) {
            notes << isr_tr("%1 is a futures or options contract; contracts are picked in the F&O and trading "
                            "screens.")
                         .arg(isr_quoted(shown));
        } else if (r.parsed.has_note("unknown_venue")) {
            notes << isr_tr("The exchange %1 is not known to the terminal.").arg(isr_quoted(r.parsed.venue_hint));
        } else if (r.parsed.has_note("no_yahoo_venue")) {
            notes << isr_tr("The terminal has no market data for instruments on that exchange.");
        } else if (!r.ctx.allowed_classes.isEmpty() && !r.pool.isEmpty()) {
            ci::RankContext any = r.ctx;
            any.allowed_classes.clear();
            if (!ci::rank_instruments(r.pool, r.parsed, any).isEmpty())
                notes << isr_tr("%1 is not one of the instrument types this field accepts.").arg(isr_quoted(shown));
        }
        if (notes.isEmpty())
            notes << isr_tr("Nothing matches %1.").arg(isr_quoted(shown));
    }
    if (r.offline)
        notes << isr_offline_note();
    res.note = notes.join(QLatin1Char(' '));
    if (res.ranked.isEmpty() && !r.local_error.isEmpty() && !r.backend_ok && !r.from_remote)
        res.error = r.local_error;
    return res;
}

void InstrumentResolver::deliver(const RequestPtr& r, bool final) {
    ResolveResult res = build_result(*r, final);
    if (!final) {
        // Skip an interim call that would show exactly what the caller already has.
        QString sig = res.note + QLatin1Char('|') + (res.offline ? QLatin1Char('o') : QLatin1Char('-'));
        for (const Instrument& m : std::as_const(res.matches))
            sig += QLatin1Char('|') + m.id;
        if (sig == r->last_signature)
            return;
        r->last_signature = sig;
    }
    if (final && res.exact && r->mode == Mode::Resolve && !res.matches.isEmpty())
        learn(res.matches.first());
    deliver_result(r, std::move(res), final);
}

void InstrumentResolver::deliver_result(const RequestPtr& r, ResolveResult res, bool final) {
    res.final = final;
    res.request_id = r->id;
    if (final)
        requests_.remove(r->id);
    if (!r->cb)
        return;
    const quint64 id = r->id;
    const Mode mode = r->mode;
    QObject* key = r->context_key;
    Callback cb = r->cb;
    // Runs on this thread: is the delivery still wanted?
    QPointer<InstrumentResolver> self(this);
    auto wanted = [self, id, mode, key]() {
        if (!self || self->dead_.contains(id))
            return false;
        if (mode == Mode::Search && key) {
            auto it = self->contexts_.constFind(key);
            if (it == self->contexts_.cend() || (*it)->search_id != id)
                return false; // superseded by a newer search of the same context
        }
        return true;
    };
    if (!r->has_context) {
        QMetaObject::invokeMethod(
            this, [wanted, cb, res]() { if (wanted()) cb(res); }, Qt::QueuedConnection);
        return;
    }
    QPointer<QObject> ctx = r->context;
    if (!ctx)
        return;
    if (ctx->thread() == thread()) {
        QMetaObject::invokeMethod(
            ctx.data(), [wanted, cb, res]() { if (wanted()) cb(res); }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(
            this,
            [wanted, cb, res, ctx]() {
                if (!wanted() || !ctx)
                    return;
                QMetaObject::invokeMethod(ctx.data(), [cb, res]() { cb(res); }, Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    }
}

bool InstrumentResolver::cache_get(const QString& key, ResolverRemoteReply* out) {
    auto it = cache_.find(key);
    if (it == cache_.end())
        return false;
    if (isr_now_ms() - it->at_ms > sources_.cache_ttl_ms) {
        cache_.erase(it);
        return false;
    }
    if (out)
        *out = it->reply;
    return true;
}

void InstrumentResolver::cache_put(const QString& key, const ResolverRemoteReply& reply) {
    if (sources_.cache_ttl_ms <= 0)
        return;
    const qint64 now = isr_now_ms();
    if (cache_.size() >= kIsrCacheMax) {
        for (auto it = cache_.begin(); it != cache_.end();) {
            if (now - it->at_ms > sources_.cache_ttl_ms)
                it = cache_.erase(it);
            else
                ++it;
        }
        while (cache_.size() >= kIsrCacheMax) {
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it)
                if (it->at_ms < oldest->at_ms)
                    oldest = it;
            cache_.erase(oldest);
        }
    }
    cache_.insert(key, CacheEntry{now, reply});
}

} // namespace fincept::services
