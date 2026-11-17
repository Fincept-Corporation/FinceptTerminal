// InstrumentRank.cpp — match tiers and the one sort order. See InstrumentRank.h.
// Qt Core only.

#include "core/instrument/InstrumentRank.h"

#include "core/instrument/VenueSymbols.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <vector>

namespace fincept::core::instrument {

namespace {
namespace instrank_detail {

constexpr qsizetype kIrMaxCompare = 64; // longest string fed to the edit distance

// Listing-code comparison form: upper case, '_' -> '-', digits without leading zeros.
QString ir_code_norm(const QString& s) {
    QString u = s.trimmed().toUpper();
    u.replace(QLatin1Char('_'), QLatin1Char('-'));
    bool digits = !u.isEmpty();
    for (const QChar c : u)
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            digits = false;
    if (digits) {
        qsizetype i = 0;
        while (i < u.size() - 1 && u.at(i) == QLatin1Char('0'))
            ++i;
        u = u.mid(i);
    }
    return u;
}

// Every code the instrument is known by: Yahoo code / base, the id's code part,
// a code-like display, and the broker symbols.
QStringList ir_codes(const Instrument& inst) {
    QStringList out;
    auto add = [&out](const QString& c) {
        const QString n = ir_code_norm(c);
        if (!n.isEmpty() && !out.contains(n))
            out.push_back(n);
    };
    const YahooParts p = from_yahoo(inst.symbol);
    if (p.valid()) {
        add(p.code);
        add(p.base);
    }
    const qsizetype colon = inst.id.indexOf(QLatin1Char(':'));
    if (colon > 0)
        add(inst.id.mid(colon + 1));
    if (!inst.display.contains(QLatin1Char(' ')))
        add(inst.display);
    for (auto it = inst.provider_symbols.constBegin(); it != inst.provider_symbols.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1String("broker:"))) {
            const QStringList parts = it.value().split(QLatin1Char('|'));
            if (parts.size() >= 2)
                add(parts.at(1));
        }
    }
    return out;
}

// The code a parsed text is matched against.
QString ir_query_code(const ParsedInput& p) {
    switch (p.kind) {
        case InputKind::Ticker:
        case InputKind::CryptoPair:
        case InputKind::YahooSymbol:
        case InputKind::Alias:
            return p.base;
        case InputKind::FxPair:
            return p.base + p.quote;
        case InputKind::Contract:
            return p.search_text.toUpper();
        case InputKind::Empty:
        case InputKind::Invalid:
        case InputKind::Isin:
        case InputKind::Name:
            break;
    }
    return {};
}

int ir_group(MatchKind m) {
    if (is_exact_match(m))
        return 0;
    switch (m) {
        case MatchKind::Prefix:
            return 1;
        case MatchKind::NameContains:
            return 2;
        case MatchKind::Fuzzy:
            return 3;
        default:
            return 4;
    }
}

int ir_tier(const RankedInstrument& r) {
    switch (r.match) {
        case MatchKind::ExactSymbol:
            return 0;
        case MatchKind::ExactListingCode:
            return 1;
        case MatchKind::ExactAlias:
            return 2;
        case MatchKind::ExactIsin:
            return 3;
        case MatchKind::Fuzzy:
            return r.distance;
        default:
            return 0;
    }
}

int ir_index_of(const QVector<AssetClass>& list, AssetClass c) {
    const qsizetype i = list.indexOf(c);
    return i < 0 ? static_cast<int>(list.size()) : static_cast<int>(i);
}

bool ir_keep(const Instrument& inst, const RankContext& ctx) {
    if (!ctx.allowed_classes.isEmpty() && !ctx.allowed_classes.contains(inst.asset_class))
        return false;
    if (!ctx.include_derivatives) {
        if (inst.asset_class == AssetClass::Option)
            return false;
        if (inst.asset_class == AssetClass::Future && !inst.symbol.endsWith(QLatin1String("=F")))
            return false;
    }
    if (!ctx.include_cfd && inst.quote_type.compare(QLatin1String("cfd"), Qt::CaseInsensitive) == 0)
        return false;
    if (!ctx.include_inactive && !inst.is_active())
        return false;
    return true;
}

} // namespace instrank_detail
} // namespace

using namespace instrank_detail;

QString match_kind_slug(MatchKind kind) {
    switch (kind) {
        case MatchKind::None:
            return QStringLiteral("none");
        case MatchKind::ExactSymbol:
            return QStringLiteral("exact_symbol");
        case MatchKind::ExactListingCode:
            return QStringLiteral("exact_listing_code");
        case MatchKind::ExactAlias:
            return QStringLiteral("exact_alias");
        case MatchKind::ExactIsin:
            return QStringLiteral("exact_isin");
        case MatchKind::Prefix:
            return QStringLiteral("prefix");
        case MatchKind::NameContains:
            return QStringLiteral("name_contains");
        case MatchKind::Fuzzy:
            return QStringLiteral("fuzzy");
    }
    return {};
}

int edit_distance(QStringView a_in, QStringView b_in, int max_distance) {
    const QStringView a = a_in.left(kIrMaxCompare);
    const QStringView b = b_in.left(kIrMaxCompare);
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    const int cap = max_distance + 1;
    if (std::abs(n - m) > max_distance)
        return cap;
    std::vector<int> prev2(static_cast<size_t>(m) + 1, 0);
    std::vector<int> prev(static_cast<size_t>(m) + 1, 0);
    std::vector<int> cur(static_cast<size_t>(m) + 1, 0);
    for (int j = 0; j <= m; ++j)
        prev[static_cast<size_t>(j)] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = a.at(i - 1) == b.at(j - 1) ? 0 : 1;
            int v = std::min({prev[static_cast<size_t>(j)] + 1, cur[static_cast<size_t>(j) - 1] + 1,
                              prev[static_cast<size_t>(j) - 1] + cost});
            if (i > 1 && j > 1 && a.at(i - 1) == b.at(j - 2) && a.at(i - 2) == b.at(j - 1))
                v = std::min(v, prev2[static_cast<size_t>(j) - 2] + 1);
            cur[static_cast<size_t>(j)] = v;
        }
        std::swap(prev2, prev);
        std::swap(prev, cur);
    }
    const int d = prev[static_cast<size_t>(m)];
    return d > max_distance ? cap : d;
}

RankedInstrument match_instrument(const Instrument& inst, const ParsedInput& p) {
    RankedInstrument r;
    r.instrument = inst;
    if (p.kind == InputKind::Empty || p.kind == InputKind::Invalid)
        return r;
    auto hit = [&r](MatchKind m, const QString& text) {
        r.match = m;
        r.matched_text = text;
        return r;
    };

    // 1. The canonical symbol of a fully specified input.
    const bool explicit_form = p.kind == InputKind::YahooSymbol || p.kind == InputKind::FxPair ||
                               p.kind == InputKind::Contract ||
                               (p.kind == InputKind::CryptoPair && !p.quote.isEmpty());
    if (!inst.symbol.isEmpty()) {
        if (!p.canonical.isEmpty() && inst.symbol == p.canonical)
            return hit(MatchKind::ExactSymbol, inst.symbol);
        if (explicit_form && p.candidates.contains(inst.symbol))
            return hit(MatchKind::ExactSymbol, inst.symbol);
    }

    // 2. A listing code.
    const QStringList codes = ir_codes(inst);
    const QString qcode = ir_code_norm(ir_query_code(p));
    if (!qcode.isEmpty() && codes.contains(qcode))
        return hit(MatchKind::ExactListingCode, qcode);
    if (p.kind == InputKind::Ticker && !inst.symbol.isEmpty() && p.candidates.contains(inst.symbol))
        return hit(MatchKind::ExactListingCode, inst.symbol);

    // 3. A former symbol / alias, or the normaliser's alias mapping.
    const QString qkey = alias_key(p.search_text);
    if (!qkey.isEmpty()) {
        for (const QString& f : inst.former_symbols) {
            if (alias_key(f) == qkey) {
                r.via_former_symbol = true;
                return hit(MatchKind::ExactAlias, f);
            }
        }
        for (const QString& a : inst.aliases)
            if (alias_key(a) == qkey)
                return hit(MatchKind::ExactAlias, a);
    }
    if ((p.kind == InputKind::Alias || p.kind == InputKind::CryptoPair) && !inst.symbol.isEmpty() &&
        p.candidates.contains(inst.symbol))
        return hit(MatchKind::ExactAlias, p.cleaned);

    // 4. ISIN.
    if (p.kind == InputKind::Isin) {
        if (!inst.isin.isEmpty() && inst.isin.trimmed().toUpper() == p.isin)
            return hit(MatchKind::ExactIsin, p.isin);
        return r; // an ISIN never matches by prefix or name
    }

    // 5. Prefix: a code, the name or an alias.
    if (!qcode.isEmpty()) {
        for (const QString& c : codes) {
            if (c.startsWith(qcode)) {
                r.code_prefix = true;
                return hit(MatchKind::Prefix, c);
            }
        }
    }
    const QString qname = normalize_name(p.search_text);
    const QString name = normalize_name(inst.name);
    if (qname.size() >= 2 && name.startsWith(qname))
        return hit(MatchKind::Prefix, inst.name);
    if (qkey.size() >= 2) {
        for (const QString& a : inst.aliases)
            if (alias_key(a).startsWith(qkey))
                return hit(MatchKind::Prefix, a);
    }

    // 6. Name contains.
    if (qname.size() >= 3 && name.contains(qname))
        return hit(MatchKind::NameContains, inst.name);

    // 7. Fuzzy.
    if (qkey.size() >= 4) {
        const int max = qkey.size() >= 6 ? 2 : 1;
        int best = max + 1;
        QString best_text;
        auto consider = [&](const QString& text, const QString& key) {
            if (key.isEmpty())
                return;
            const int d = edit_distance(qkey, key, max);
            if (d < best) {
                best = d;
                best_text = text;
            }
        };
        for (const QString& c : codes)
            consider(c, alias_key(c));
        for (const QString& a : inst.aliases)
            consider(a, alias_key(a));
        for (const QString& f : inst.former_symbols)
            consider(f, alias_key(f));
        if (!name.isEmpty()) {
            const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            const qsizetype n = std::max<qsizetype>(1, qname.split(QLatin1Char(' '), Qt::SkipEmptyParts).size());
            consider(inst.name, alias_key(words.mid(0, n).join(QString())));
        }
        if (best <= max) {
            r.distance = best;
            return hit(MatchKind::Fuzzy, best_text);
        }
    }
    return r;
}

bool rank_before(const RankedInstrument& a, const RankedInstrument& b, const ParsedInput& p, const RankContext& ctx) {
    const QString home = ctx.home_country.trimmed().toUpper();
    auto key = [&p, &ctx, &home](const RankedInstrument& r) {
        const Instrument& i = r.instrument;
        const int group = ir_group(r.match);
        const int inactive = i.is_active() ? 0 : 1;
        const int pref = ir_index_of(p.preferred_classes, i.asset_class);
        const int tier = ir_tier(r);
        const int not_home = (!home.isEmpty() && i.country.compare(home, Qt::CaseInsensitive) == 0) ? 0 : 1;
        const int not_primary = i.primary_listing ? 0 : 1;
        const int cls = ir_index_of(ctx.class_priority, i.asset_class);
        const int weight = -i.rank_weight;
        const int name_prefix = (r.match == MatchKind::Prefix && !r.code_prefix) ? 1 : 0;
        const qsizetype ci = p.candidates.indexOf(i.symbol);
        const qsizetype cand = ci < 0 ? std::numeric_limits<qsizetype>::max() : ci;
        return std::make_tuple(group, inactive, pref, tier, not_home, not_primary, cls, weight, name_prefix, cand);
    };
    const auto ka = key(a);
    const auto kb = key(b);
    if (ka != kb)
        return ka < kb;
    if (a.instrument.symbol != b.instrument.symbol)
        return a.instrument.symbol < b.instrument.symbol;
    return a.instrument.id < b.instrument.id;
}

QVector<RankedInstrument> rank_instruments(const QVector<Instrument>& candidates, const ParsedInput& parsed,
                                           const RankContext& ctx) {
    QVector<RankedInstrument> hits;
    hits.reserve(candidates.size());
    for (const Instrument& inst : candidates) {
        if (!ir_keep(inst, ctx))
            continue;
        RankedInstrument r = match_instrument(inst, parsed);
        if (r.match != MatchKind::None)
            hits.push_back(std::move(r));
    }
    std::stable_sort(hits.begin(), hits.end(), [&parsed, &ctx](const RankedInstrument& a, const RankedInstrument& b) {
        return rank_before(a, b, parsed, ctx);
    });
    QVector<RankedInstrument> out;
    out.reserve(hits.size());
    for (RankedInstrument& h : hits) {
        bool dup = false;
        for (const RankedInstrument& o : out) {
            if (o.instrument.same_instrument(h.instrument)) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        out.push_back(std::move(h));
        if (ctx.limit > 0 && out.size() >= ctx.limit)
            break;
    }
    return out;
}

QVector<RankedInstrument> rank_instruments(const QVector<Instrument>& candidates, const QString& text,
                                           const RankContext& ctx) {
    return rank_instruments(candidates, parse_symbol_input(text, {}, ctx.home_country), ctx);
}

} // namespace fincept::core::instrument
