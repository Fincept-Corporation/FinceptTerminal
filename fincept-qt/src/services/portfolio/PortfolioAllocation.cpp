// src/services/portfolio/PortfolioAllocation.cpp — see the header.
#include "services/portfolio/PortfolioAllocation.h"

#include "core/market/InstrumentCurrency.h"
#include "core/market/MarketSessions.h"
#include "services/portfolio/PortfolioClassify.h"

#include <QMap>

#include <algorithm>
#include <cmath>

namespace fincept::portfolio {

QString allocation_dimension_key(AllocationDimension d) {
    switch (d) {
        case AllocationDimension::Sector:
            return QStringLiteral("sector");
        case AllocationDimension::Industry:
            return QStringLiteral("industry");
        case AllocationDimension::AssetClass:
            return QStringLiteral("asset_class");
        case AllocationDimension::Country:
            return QStringLiteral("country");
        case AllocationDimension::Region:
            return QStringLiteral("region");
        case AllocationDimension::Currency:
            return QStringLiteral("currency");
        case AllocationDimension::Exchange:
            return QStringLiteral("exchange");
        case AllocationDimension::MarketCap:
            break;
    }
    return QStringLiteral("market_cap");
}

QVector<AllocationDimension> all_allocation_dimensions() {
    return {AllocationDimension::Sector,   AllocationDimension::Industry, AllocationDimension::AssetClass,
            AllocationDimension::Country,  AllocationDimension::Region,   AllocationDimension::Currency,
            AllocationDimension::Exchange, AllocationDimension::MarketCap};
}

QString market_cap_bucket_usd(double cap) {
    if (!std::isfinite(cap) || cap <= 0)
        return {};
    if (cap >= 200e9)
        return QStringLiteral("mega");
    if (cap >= 10e9)
        return QStringLiteral("large");
    if (cap >= 2e9)
        return QStringLiteral("mid");
    if (cap >= 300e6)
        return QStringLiteral("small");
    return QStringLiteral("micro");
}

ConcentrationStats concentration_of(const QVector<QPair<QString, double>>& weights_pct) {
    ConcentrationStats c;
    QVector<QPair<QString, double>> w;
    for (const auto& p : weights_pct)
        if (std::isfinite(p.second) && p.second > 0)
            w.append(p);
    c.positions = static_cast<int>(w.size());
    if (w.isEmpty())
        return c;
    std::sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    double hhi = 0, sum = 0;
    for (const auto& p : w) {
        hhi += (p.second / 100.0) * (p.second / 100.0);
        sum += p.second;
    }
    // Renormalise when the weights do not add up to 100 (e.g. a subset).
    if (sum > 0 && std::abs(sum - 100.0) > 1e-6)
        hhi = hhi * (100.0 / sum) * (100.0 / sum);
    c.hhi = hhi;
    c.effective_n = hhi > 1e-12 ? 1.0 / hhi : kAbsent;
    auto top = [&w](int k) {
        double s = 0;
        for (int i = 0; i < std::min(k, static_cast<int>(w.size())); ++i)
            s += w[i].second;
        return s;
    };
    c.top1 = top(1);
    c.top3 = top(3);
    c.top5 = top(5);
    c.top10 = top(10);
    c.largest_symbol = w.first().first;
    return c;
}

AllocationReport compute_allocation(const PortfolioSummary& summary, const QHash<QString, services::InfoData>& info,
                                    const std::function<std::optional<double>(const QString& ccy)>& usd_per) {
    AllocationReport r;
    r.portfolio_id = summary.portfolio.id;
    r.base_currency = summary.base_currency;

    struct Item {
        const HoldingWithQuote* h = nullptr;
        const services::InfoData* info = nullptr;
    };
    QVector<Item> items;
    double total = 0;
    for (const auto& h : summary.holdings) {
        if (!h.valued() || !std::isfinite(h.market_value)) {
            r.unvalued.append(h.symbol);
            continue;
        }
        Item it;
        it.h = &h;
        const auto ii = info.constFind(h.symbol);
        if (ii != info.constEnd())
            it.info = &*ii;
        else
            r.info_missing.append(h.symbol);
        items.append(it);
        total += h.market_value;
    }
    r.valued_total = total;

    auto attribute = [&](AllocationDimension d, const Item& it) -> QString {
        const HoldingWithQuote& h = *it.h;
        const services::InfoData* inf = it.info;
        const QString qt = !h.quote_type.isEmpty() ? h.quote_type : (inf ? inf->quote_type : QString());
        switch (d) {
            case AllocationDimension::Sector:
                if (inf && !inf->sector.isEmpty())
                    return inf->sector;
                return h.sector;
            case AllocationDimension::Industry:
                return inf ? inf->industry : QString();
            case AllocationDimension::AssetClass: {
                const auto ac = classify_asset(h.symbol, qt, !h.name.isEmpty() ? h.name : (inf ? inf->name : QString()));
                if (ac.guessed && !r.class_guessed.contains(h.symbol))
                    r.class_guessed.append(h.symbol);
                return asset_class_key(ac.cls);
            }
            case AllocationDimension::Country: {
                if (inf && !inf->country.isEmpty())
                    return inf->country;
                return {};
            }
            case AllocationDimension::Region: {
                QString country = inf ? inf->country : QString();
                if (country.isEmpty()) {
                    // Listing venue's country stands in for an unknown domicile.
                    if (const auto* v = core::market::venue_for_symbol(h.symbol, h.exchange))
                        country = v->country;
                }
                return region_of_country(country);
            }
            case AllocationDimension::Currency:
                return h.currency;
            case AllocationDimension::Exchange: {
                if (const auto* v = core::market::venue_for_symbol(h.symbol, h.exchange))
                    return v->id;
                return h.exchange;
            }
            case AllocationDimension::MarketCap: {
                if (!inf || !std::isfinite(inf->market_cap) || inf->market_cap <= 0)
                    return {};
                const auto ac = classify_asset(h.symbol, qt, h.name);
                if (ac.fund)
                    return {};
                // Yahoo reports the cap in the listing's major currency. Deliberate USD: the
                // buckets are defined in US dollars (market_cap_bucket_usd).
                const QString ccy = core::market::major_currency(!inf->currency.isEmpty() ? inf->currency : h.currency);
                double usd = kAbsent;
                if (ccy == QLatin1String("USD"))
                    usd = inf->market_cap;
                else if (usd_per) {
                    if (auto f = usd_per(ccy); f && std::isfinite(*f) && *f > 0)
                        usd = inf->market_cap * *f;
                }
                return market_cap_bucket_usd(usd);
            }
        }
        return {};
    };

    for (AllocationDimension d : all_allocation_dimensions()) {
        AllocationBreakdown b;
        b.dimension = d;
        b.key = allocation_dimension_key(d);
        QMap<QString, AllocationSlice> slices;
        for (const auto& it : items) {
            const QString k = attribute(d, it).trimmed();
            AllocationSlice& s = slices[k];
            s.key = k;
            s.value += it.h->market_value;
            ++s.count;
            s.symbols.append(it.h->symbol);
        }
        for (auto& s : slices) {
            s.weight = total > 0 ? s.value / total * 100.0 : 0.0;
            if (s.key.isEmpty())
                b.unknown_weight = s.weight;
            b.slices.append(s);
        }
        std::sort(b.slices.begin(), b.slices.end(), [](const AllocationSlice& a, const AllocationSlice& c) {
            if (a.key.isEmpty() != c.key.isEmpty())
                return c.key.isEmpty(); // unknown last
            return a.value > c.value;
        });
        r.breakdowns.append(b);
    }

    QVector<QPair<QString, double>> w;
    for (const auto& it : items)
        w.append({it.h->symbol, total > 0 ? it.h->market_value / total * 100.0 : kAbsent});
    r.concentration = concentration_of(w);
    r.complete = r.unvalued.isEmpty() && r.info_missing.isEmpty();
    return r;
}

} // namespace fincept::portfolio
