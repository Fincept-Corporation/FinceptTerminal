#include "screens/dashboard/canvas/DashboardTemplates.h"

#include "core/logging/Logger.h"
#include "core/market/MarketProfile.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "storage/repositories/SettingsRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <initializer_list>
#include <utility>

namespace fincept::screens {

namespace {

constexpr const char* kDtTrContext = "fincept::screens::DashboardTemplates";
constexpr const char* kDtNotesKey = "dashboard.template_notes";

QString dt_tr(const char* english) {
    return QCoreApplication::translate(kDtTrContext, english);
}

/// One tile of a template: type, 12-column compact-row cell, preset config.
/// Minimum sizes come from the registry in build_template_layout().
GridItem dt_item(const char* type, int x, int y, int w, int h, const QJsonObject& cfg = {}) {
    GridItem item;
    item.id = QString::fromLatin1(type);
    item.cell = {x, y, w, h, 1, 1};
    item.config = cfg;
    return item;
}

QJsonObject dt_pin(const QString& region, QJsonObject cfg = {}) {
    cfg.insert(QStringLiteral("market_region"), region);
    return cfg;
}

/// A template's own symbol list for a quote / performance tile, named in the
/// tile title ("MARKET INDICES · ASIA-PACIFIC").
QJsonObject dt_list(std::initializer_list<std::pair<const char*, const char*>> syms, const QString& label) {
    QJsonArray arr;
    for (const auto& [s, l] : syms)
        arr.append(QJsonObject{{"s", QString::fromLatin1(s)}, {"l", QString::fromUtf8(l)}});
    return QJsonObject{{"symbols", arr}, {"list_label", label}};
}

/// US rates from live Yahoo quotes (always available, unlike the /data curve):
/// bill, 2Y / 5Y / 10Y / 30Y yields, bond volatility and the note / bond futures.
QJsonObject dt_us_rates() {
    QJsonObject o = dt_list({{"^IRX", "13-week T-bill"},
                             {"2YY=F", "2-year yield"},
                             {"^FVX", "5-year yield"},
                             {"^TNX", "10-year yield"},
                             {"^TYX", "30-year yield"},
                             {"^MOVE", "MOVE (bond vol)"},
                             {"ZN=F", "10Y T-note fut."},
                             {"ZB=F", "30Y T-bond fut."}},
                            dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "US rates")));
    o.insert("list_title", dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "US RATES")));
    return o;
}

QJsonObject dt_note(const char* template_id) {
    return QJsonObject{{"seed", QString::fromLatin1(template_id)}};
}

// ── Seed notes ───────────────────────────────────────────────────────────────
// Timeless, factual desk notes: what the desk watches, the readings on this
// layout that matter most and how to read them together, a short routine, and
// the kinds of events worth tracking. No prices, levels, dates or statistics.

QString dt_seed(const QString& id) {
    if (id == QLatin1String("market_overview"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "The day's tone in your region: do indices, sectors and cross-asset risk tell the same story, and what "
            "on the calendar could change it?\n\n"
            "**Read together**\n\n"
            "- **Indices + sector map** — a rally led by defensives (staples, utilities, health care) is weaker than "
            "one led by cyclicals and financials.\n"
            "- **Risk-on / risk-off + breadth** — an index making highs while the score falls or breadth narrows is a "
            "divergence to respect.\n"
            "- **Top movers + news** — a big move with a headline behind it is information; one without is often "
            "flow.\n"
            "- **FX + commodities** — a firm dollar with soft copper usually leans against risk appetite.\n\n"
            "**Routine**\n\n"
            "1. Before the open: today's high-importance releases and their times.\n"
            "2. At the open: is sector leadership the same as yesterday?\n"
            "3. Midday: breadth and the risk score against the index.\n"
            "4. Close: write down what moved and why.\n\n"
            "**Track**\n\n"
            "Central-bank decisions, inflation and jobs data, large-cap earnings in your region, index rebalances."));
    if (id == QLatin1String("global_macro"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "How growth, inflation and policy expectations move through rates, currencies, commodities and equities "
            "— and where regions disagree.\n\n"
            "**Read together**\n\n"
            "- **Yield curve + US rates** — steepening because short yields fall usually prices rate cuts; "
            "steepening because long yields rise points to inflation or term-premium worries.\n"
            "- **FX cross matrix** — one currency moving against all others is that currency's story; a broad dollar "
            "bid while equities fall is a stress signal.\n"
            "- **Commodities** — copper against gold reads growth against fear; oil feeds inflation expectations.\n"
            "- **Volatility term structure** — front months above later months mark acute stress.\n"
            "- **Global indices by session** — does a move carry from Asia to Europe to the US?\n\n"
            "**Routine**\n\n"
            "1. Europe open: what moved overnight, and why.\n"
            "2. Calendar: central banks and tier-one data today.\n"
            "3. After each release: curve, dollar and gold first — equities follow.\n"
            "4. Weekly: compare 1M and YTD across regions for a change of regime.\n\n"
            "**Track**\n\n"
            "Policy-rate decisions and minutes, CPI, payrolls, PMIs, GDP, central-bank speeches, OPEC+ meetings, "
            "government bond auctions."));
    if (id == QLatin1String("equity_trader"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Single names in play today — why they move, and whether volume, the sector and the tape support the "
            "move.\n\n"
            "**Read together**\n\n"
            "- **Top movers, REL VOL column** — a move on high relative volume is real participation; on low "
            "volume it fades more easily.\n"
            "- **Quote + candles** — where is price against the prior day's range and the 52-week range?\n"
            "- **Sector map** — moving with the sector is beta; against it is stock-specific: read the news.\n"
            "- **Breadth** — buying into narrowing breadth is fighting the tape.\n\n"
            "**Routine**\n\n"
            "1. Pre-market: build the list from movers, news and today's earnings.\n"
            "2. Mark levels on each name: prior high and low, gaps, 52-week extremes.\n"
            "3. First hour: let the open settle; watch relative volume.\n"
            "4. After the close: log entries, exits and what you would repeat.\n\n"
            "**Track**\n\n"
            "Earnings dates of your names, index rebalances, options expiry days, ex-dividend dates, lock-up "
            "expiries.\n\n"
            "*Tip: the tiles share link group A — click a name in movers, the watchlist or the screener and the "
            "quote and the chart follow it. Change a tile's group badge to take it out of the link.*"));
    if (id == QLatin1String("portfolio_manager"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "The portfolio first — value, P&L, risk and what drives it — then the market context that explains it.\n\n"
            "**Read together**\n\n"
            "- **Performance vs benchmark** — time-weighted returns separate your decisions from deposits and "
            "withdrawals.\n"
            "- **Portfolio risk** — compare each holding's share of risk with its weight: a small position "
            "carrying a large share of risk deserves a look.\n"
            "- **Beta + market regime** — high beta into a rising volatility regime deepens drawdowns.\n"
            "- **Earnings calendar (your symbols) + news** — know the event risk before it lands.\n\n"
            "**Routine**\n\n"
            "1. Daily: which holdings explain most of today's P&L?\n"
            "2. Weekly: risk shares against target weights; note the drift.\n"
            "3. Before a busy earnings week: review the size of reporting positions.\n"
            "4. Monthly: return against the benchmark, and why.\n\n"
            "**Track**\n\n"
            "Earnings and dividend dates of holdings, rebalancing dates, cash flows, benchmark changes.\n\n"
            "*No portfolio yet? Create or import one in the Portfolio screen — the portfolio tiles fill in by "
            "themselves.*"));
    if (id == QLatin1String("crypto_trader"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "A market that never closes: bitcoin's lead, rotation across major coins, liquidity and risk "
            "appetite.\n\n"
            "**Read together**\n\n"
            "- **BTC dominance (crypto overview)** — rising dominance with falling prices is risk-off inside "
            "crypto; falling dominance with rising prices is rotation into altcoins.\n"
            "- **Returns by period** — a coin leading over 1W and 1M but lagging today is a pullback in a trend; "
            "the reverse is often a squeeze.\n"
            "- **Crypto stocks & ETFs** — spot ETFs and listed crypto firms show how traditional money is "
            "positioned; they only trade in exchange hours, coins never stop.\n"
            "- **Risk-on / risk-off** — crypto often trades with broad risk appetite; a divergence is "
            "information.\n\n"
            "**Routine**\n\n"
            "1. Start of day: the overnight range of BTC and ETH on the chart.\n"
            "2. Dominance and trending coins: is money rotating?\n"
            "3. Before a trade: does volume confirm the move?\n"
            "4. Set alerts at your levels — the market keeps moving while you are away.\n\n"
            "**Track**\n\n"
            "Token unlocks, exchange listings and delistings, network upgrades, spot ETF flows, US inflation data and "
            "Fed decisions, regulatory announcements."));
    if (id == QLatin1String("fx_rates"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "The price of money: rate differentials drive currencies, and central banks drive both.\n\n"
            "**Read together**\n\n"
            "- **US rates + yield curve** — the 2-year yield tracks policy expectations, the 10-year adds growth and "
            "term premium; MOVE is the bond market's volatility gauge.\n"
            "- **FX cross matrix** — a currency moving against all others colours its whole row; when that row is "
            "the dollar's, it is a dollar story, not a story about the other currencies.\n"
            "- **Currency performance by period** — separates today's noise from the 1M and YTD trend.\n"
            "- **Candles** (EUR/USD by default — change it with the gear) — mark the range before a release and "
            "compare after.\n\n"
            "**Routine**\n\n"
            "1. Calendar: decisions and data for the currencies you trade.\n"
            "2. Before a release: note the 2-year yield and the dollar; compare after.\n"
            "3. On risk-off days: watch the yen and the franc crosses for haven demand.\n"
            "4. Week end: which central-bank expectations shifted?\n\n"
            "**Track**\n\n"
            "Rate decisions and statements, CPI, employment, PMIs, retail sales, minutes, month-end and quarter-end "
            "fixings."));
    if (id == QLatin1String("volatility_risk"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "The price of protection and the shape of the market underneath the index.\n\n"
            "**Read together**\n\n"
            "- **VIX futures curve** — later months above spot (contango) is the calm norm; spot above later "
            "months (backwardation) shows near-term stress.\n"
            "- **Implied vs realised (market risk)** — implied well above realised means protection is expensive; "
            "below it, complacency.\n"
            "- **VVIX** — volatility of volatility rising ahead of the VIX can flag a regime change.\n"
            "- **Breadth + sector map** — participation shrinking under a steady index often comes before "
            "volatility.\n"
            "- **Risk-on / risk-off** — note which of its inputs turns first.\n\n"
            "**Routine**\n\n"
            "1. Morning: curve shape and the VIX / VIX3M ratio.\n"
            "2. Implied against realised volatility for your region.\n"
            "3. Around macro events: implied volatility before, and after.\n"
            "4. Weekly: each index's distance from its 52-week high.\n\n"
            "**Track**\n\n"
            "Options expiries, VIX settlement days, central-bank and CPI days, index rebalances, earnings of index "
            "heavyweights."));
    if (id == QLatin1String("news_geopolitics"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Events before they become prices — conflicts, elections, sanctions, supply shocks — and how markets "
            "price them.\n\n"
            "**Read together**\n\n"
            "- **Headlines with impact tags** — one high-impact headline can outweigh many routine ones.\n"
            "- **Geopolitics and energy headlines + commodities** — supply risks show up first in oil, gas, metals "
            "and grains.\n"
            "- **Prediction markets** — the crowd's probability of an outcome; watch the change after news.\n"
            "- **Risk-on / risk-off + global indices** — is the event treated as local or as systemic?\n\n"
            "**Routine**\n\n"
            "1. Morning: high-impact headlines across regions.\n"
            "2. For each developing story: which asset would be hit first? Check it.\n"
            "3. Compare prediction-market odds before and after major headlines.\n"
            "4. Keep a short log below: event, date, market reaction.\n\n"
            "**Track**\n\n"
            "Elections and referendums, sanctions decisions, OPEC+ meetings, summits, emergency central-bank "
            "actions, shipping-lane disruptions."));
    if (id == QLatin1String("research_desk"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Ideas on their way from a screen to conviction: valuation, quality, trend and catalysts.\n\n"
            "**Read together**\n\n"
            "- **Screener** (quality by default — switch to value, dividend, growth or defensive) — a starting "
            "list, not a conclusion.\n"
            "- **Quote card + candles** — valuation context, range position and trend.\n"
            "- **Earnings calendar** — the next catalyst for each name.\n"
            "- **Sector map (1M)** — is the idea swimming with or against its sector?\n\n"
            "**Routine**\n\n"
            "1. Run one screen; pick a few names worth a deeper look.\n"
            "2. For each: trend, valuation, next earnings date.\n"
            "3. Read the recent news; write the thesis and what would prove it wrong.\n"
            "4. Keep the survivors on a watchlist; revisit after their results.\n\n"
            "**Track**\n\n"
            "Earnings and guidance, investor days, index inclusions, rating changes, insider transactions."));
    if (id == QLatin1String("india_markets"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Indian equities and the flows behind them — foreign against domestic institutions, the banks, the rupee "
            "and India VIX.\n\n"
            "**Read together**\n\n"
            "- **FII / DII flows** — foreign selling absorbed by domestic buying tends to keep the index in a "
            "range; both selling is a stronger negative.\n"
            "- **Bank Nifty against Nifty 50** — banks are the heaviest sector, so their leadership sets the "
            "index's tone.\n"
            "- **India VIX (market risk)** — rising into expiry or events signals hedging demand.\n"
            "- **USD/INR + sector map** — a weaker rupee tends to help IT and pharma exporters.\n\n"
            "**Routine**\n\n"
            "1. Pre-open: global cues (US close, Asia), the previous day's FII / DII, overnight news.\n"
            "2. Open: sector leadership; Bank Nifty against Nifty.\n"
            "3. Into the close: India VIX and the movers.\n"
            "4. Evening: the day's provisional flow figures.\n\n"
            "**Track**\n\n"
            "RBI policy meetings, F&O expiry days, the Union Budget, CPI and IIP releases, results season, US Fed "
            "decisions."));
    if (id == QLatin1String("asia_pacific"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "The first sessions of the trading day — Tokyo, Hong Kong, Shanghai, Seoul, Taipei, Mumbai, Sydney — "
            "and how they set up Europe and the US.\n\n"
            "**Read together**\n\n"
            "- **Session clock** — which markets are open, at lunch or closed; Tokyo, Hong Kong and Shanghai pause "
            "at midday.\n"
            "- **Indices + performance by period** — today's move against each market's trend.\n"
            "- **Yen and yuan** — yen strength often comes with risk-off; yuan moves feed into Hong Kong and the "
            "commodity currencies.\n"
            "- **Commodities** — copper and gold matter for Australia; oil for importers such as Japan, Korea and "
            "India.\n\n"
            "**Routine**\n\n"
            "1. Before Tokyo opens: the US close, futures, data from China and Japan.\n"
            "2. Hong Kong and Shanghai open: China-sensitive names and the yuan.\n"
            "3. Midday: chip-heavy Taiwan and Korea.\n"
            "4. Before Europe opens: summarise the session.\n\n"
            "**Track**\n\n"
            "BoJ, PBoC and RBA decisions, China activity data, Japanese wage and inflation data, regional "
            "holidays such as Lunar New Year and Golden Week."));
    if (id == QLatin1String("europe"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Eurozone equities, the euro, the euro-area curve and energy prices.\n\n"
            "**Read together**\n\n"
            "- **Sector map (STOXX 600 sectors)** — banks tend to lead when yields rise; autos and luxury are "
            "sensitive to demand from China.\n"
            "- **Euro-area curve + EUR crosses** — rate expectations against the euro's moves.\n"
            "- **EUR/CHF** — a falling rate is a haven signal.\n"
            "- **Energy (Dutch TTF gas, Brent)** — Europe's input costs and inflation expectations.\n\n"
            "**Routine**\n\n"
            "1. Before the open: the Asia close, US futures, overnight news.\n"
            "2. Early session: national data from Germany and France, and sector leadership.\n"
            "3. ECB speakers and euro-area data: watch the curve and the euro react.\n"
            "4. US open: does European leadership hold?\n\n"
            "**Track**\n\n"
            "ECB meetings and accounts, flash PMIs, HICP inflation, German Ifo and ZEW surveys, national elections, "
            "EU summits, gas-storage reports."));
    if (id == QLatin1String("quant_desk"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Market internals and regime — trend, breadth, volatility and dispersion — the inputs systematic rules "
            "depend on.\n\n"
            "**Read together**\n\n"
            "- **Sparklines** — many trends on one screen; divergences stand out.\n"
            "- **Volatility curve + market risk** — the regime filter for position sizing.\n"
            "- **Screener (low beta) + most active** — which factor or names carry the tape.\n"
            "- **Scan alerts + automation runs** — your strategies' signals and whether every run succeeded.\n\n"
            "**Routine**\n\n"
            "1. Before the open: overnight runs and any failures.\n"
            "2. Does today's regime match what each strategy assumes?\n"
            "3. Act on scan alerts by the rules, not by the story.\n"
            "4. Weekly: live behaviour against the backtest.\n\n"
            "**Track**\n\n"
            "Data-provider outages, index reconstitutions, futures rolls and expiries, exchange holidays, corporate "
            "actions on traded symbols.\n\n"
            "*Scan alerts and automation runs fill in once you run strategies from Algo Trading.*"));
    if (id == QLatin1String("focus"))
        return dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
            "**What this desk watches**\n\n"
            "Only what you need: the headline indices, your own list, one chart and the news.\n\n"
            "**Read together**\n\n"
            "- **Indices** — the market's mood at a glance.\n"
            "- **Watchlist** — your saved list; double-click any name to open it in Equity Research.\n"
            "- **Chart** — follows the name you click in the indices or the watchlist (link group A); set it with "
            "the gear.\n\n"
            "**Routine**\n\n"
            "1. Glance: does anything need a closer look?\n"
            "2. If it does, open a fuller desk from RESET → templates.\n\n"
            "**Track**\n\n"
            "Write here the two or three things you are watching this week."));
    return {};
}

QJsonObject dt_read_note_store() {
    const auto r = SettingsRepository::instance().get(QString::fromLatin1(kDtNotesKey));
    if (r.is_err() || r.value().isEmpty())
        return {};
    return QJsonDocument::fromJson(r.value().toUtf8()).object();
}

void dt_write_note_store(const QJsonObject& all) {
    const auto w = SettingsRepository::instance().set(
        QString::fromLatin1(kDtNotesKey), QString::fromUtf8(QJsonDocument(all).toJson(QJsonDocument::Compact)),
        "dashboard");
    if (w.is_err())
        LOG_WARN("Dashboard", QString("could not save a template note: %1").arg(QString::fromStdString(w.error())));
}

} // namespace

// ── Templates ─────────────────────────────────────────────────────────────────
// Compact rows (40 px + 4 px gap). Each band of tiles spans all 12 columns and
// its tiles (or stacks of tiles) end on the same row. Widths are multiples of 4
// or 6 where the fold to 9 columns must keep them readable (4 -> 3, 8 -> 6).

QVector<DashboardTemplate> all_dashboard_templates() {
    const auto& prof = core::market::MarketProfileStore::instance().current();
    const QString region = prof.id;
    const QString lead = prof.movers.isEmpty() ? QStringLiteral("AAPL") : prof.movers.first();
    // Quant desk sparklines: the region's headline indices, then its sectors.
    QJsonArray sector_syms;
    for (const auto& s : prof.indices.mid(0, 5))
        sector_syms.append(s.symbol);
    for (const auto& s : prof.sectors)
        if (sector_syms.size() < 16)
            sector_syms.append(s.symbol);

    QVector<DashboardTemplate> out;

    // ── Market Overview (the default) ────────────────────────────────────────
    {
        DashboardTemplate t{"market_overview",
                            QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Market Overview"),
                            QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                              "Your region at a glance: indices, sectors, risk regime, movers, news "
                                              "and the calendar"),
                            {},
                            {},
                            {
                                dt_item("indices", 0, 0, 4, 9),
                                dt_item("sector_heatmap", 4, 0, 4, 9),
                                dt_item("sentiment", 8, 0, 4, 9),
                                dt_item("top_movers", 0, 9, 4, 8),
                                dt_item("news", 4, 9, 4, 8),
                                dt_item("econ_calendar", 8, 9, 4, 8, {{"country", "@region"}, {"importance", 2}}),
                                dt_item("forex", 0, 17, 4, 10),
                                dt_item("commodities", 4, 17, 4, 10),
                                dt_item("desk_note", 8, 17, 4, 10, dt_note("market_overview")),
                            }};
        // India adds the FII / DII flows its users check first.
        if (region == QLatin1String("IN")) {
            t.items.append(dt_item("world_clock", 0, 27, 4, 8));
            t.items.append(dt_item("fii_dii_flows", 4, 27, 4, 8));
            t.items.append(dt_item("market_breadth", 8, 27, 4, 8));
        } else {
            t.items.append(dt_item("world_clock", 0, 27, 8, 8));
            t.items.append(dt_item("market_breadth", 8, 27, 4, 8));
        }
        out.append(t);
    }

    // ── Global Macro ─────────────────────────────────────────────────────────
    out.append(
        DashboardTemplate{"global_macro",
         QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Global Macro"),
         QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                           "Cross-asset view: world indices by session, rates, FX, commodities, volatility and the "
                           "macro calendar"),
         {},
         {},
         {
             dt_item("global_indices", 0, 0, 4, 17),
             dt_item("yield_curve", 4, 0, 4, 9),
             dt_item("sentiment", 8, 0, 4, 9),
             dt_item("fx_matrix", 4, 9, 4, 8),
             dt_item("indices", 8, 9, 4, 8, dt_us_rates()),
             dt_item("commodities", 0, 17, 4, 10),
             dt_item("volatility", 4, 17, 4, 10),
             dt_item("desk_note", 8, 17, 4, 10, dt_note("global_macro")),
             dt_item("econ_calendar", 0, 27, 4, 8, {{"country", ""}, {"importance", 3}}),
             dt_item("market_performance", 4, 27, 4, 8,
                     dt_list({{"^GSPC", "S&P 500"},
                              {"^IXIC", "Nasdaq Comp."},
                              {"^STOXX", "STOXX 600"},
                              {"^FTSE", "FTSE 100"},
                              {"^N225", "Nikkei 225"},
                              {"^HSI", "Hang Seng"},
                              {"000001.SS", "SSE Composite"},
                              {"^NSEI", "Nifty 50"},
                              {"^AXJO", "S&P/ASX 200"},
                              {"^BVSP", "Ibovespa"}},
                             dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "World")))),
             dt_item("news", 8, 27, 4, 8),
         }});

    // ── Equities Trader ──────────────────────────────────────────────────────
    out.append(DashboardTemplate{"equity_trader",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Equities Trader"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Single names in play: quote and candles, movers, watchlist, screener, sectors and "
                                  "breadth"),
                {},
                {},
                {
                    dt_item("stock_quote", 0, 0, 4, 10, {{"symbol", lead}}),
                    dt_item("candle_chart", 4, 0, 8, 10, {{"symbol", lead}, {"tf", "6M"}}),
                    dt_item("top_movers", 0, 10, 4, 8, {{"universe", "region"}, {"tab", 0}}),
                    dt_item("watchlist", 4, 10, 4, 8),
                    dt_item("screener", 8, 10, 4, 8, {{"screen", "largest"}}),
                    dt_item("sector_heatmap", 0, 18, 4, 10),
                    dt_item("news", 4, 18, 4, 10),
                    dt_item("desk_note", 8, 18, 4, 10, dt_note("equity_trader")),
                    dt_item("market_breadth", 0, 28, 4, 8),
                    dt_item("world_clock", 4, 28, 8, 8),
                }});

    // ── Portfolio Manager ────────────────────────────────────────────────────
    out.append(DashboardTemplate{"portfolio_manager",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Portfolio Manager"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Your portfolio's value, performance, allocation and risk, with its earnings, news "
                                  "and market context"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Portfolio tiles need a portfolio — they show how to create one"),
                {},
                {
                    dt_item("portfolio_summary", 0, 0, 8, 9),
                    dt_item("performance", 8, 0, 4, 9),
                    dt_item("risk_metrics", 0, 9, 4, 9),
                    dt_item("watchlist", 4, 9, 4, 9),
                    dt_item("earnings_calendar", 8, 9, 4, 9, {{"scope", "mine"}}),
                    dt_item("news", 0, 18, 4, 10),
                    dt_item("market_performance", 4, 18, 4, 10),
                    dt_item("desk_note", 8, 18, 4, 10, dt_note("portfolio_manager")),
                }});

    // ── Crypto Desk ──────────────────────────────────────────────────────────
    out.append(DashboardTemplate{"crypto_trader",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Crypto Desk"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Top coins, BTC candles, returns by period, market cap and dominance, crypto "
                                  "stocks and ETFs, crypto news and the risk regime"),
                {},
                {},
                {
                    // Yahoo pairs only: the live exchange ticker / trade tape need the
                    // optional Python libraries (ccxt), which a first-run install lacks.
                    dt_item("crypto", 0, 0, 4, 10,
                            dt_list({{"BTC-USD", "Bitcoin"},
                                     {"ETH-USD", "Ethereum"},
                                     {"SOL-USD", "Solana"},
                                     {"XRP-USD", "XRP"},
                                     {"BNB-USD", "BNB"},
                                     {"DOGE-USD", "Dogecoin"},
                                     {"ADA-USD", "Cardano"},
                                     {"TRX-USD", "TRON"},
                                     {"AVAX-USD", "Avalanche"},
                                     {"LINK-USD", "Chainlink"},
                                     {"LTC-USD", "Litecoin"},
                                     {"DOT-USD", "Polkadot"}},
                                    dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Top coins")))),
                    dt_item("candle_chart", 4, 0, 8, 10, {{"symbol", "BTC-USD"}, {"tf", "1M"}}),
                    dt_item("market_performance", 0, 10, 4, 9,
                            dt_list({{"BTC-USD", "Bitcoin"},
                                     {"ETH-USD", "Ethereum"},
                                     {"SOL-USD", "Solana"},
                                     {"XRP-USD", "XRP"},
                                     {"BNB-USD", "BNB"},
                                     {"DOGE-USD", "Dogecoin"},
                                     {"ADA-USD", "Cardano"},
                                     {"LINK-USD", "Chainlink"}},
                                    dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Coins")))),
                    dt_item("crypto_overview", 4, 10, 4, 9),
                    dt_item("indices", 8, 10, 4, 9,
                            [] {
                                QJsonObject o = dt_list({{"IBIT", "iShares Bitcoin Trust"},
                                                         {"FBTC", "Fidelity Wise Origin BTC"},
                                                         {"ETHA", "iShares Ethereum Trust"},
                                                         {"COIN", "Coinbase"},
                                                         {"MSTR", "Strategy"},
                                                         {"HOOD", "Robinhood"},
                                                         {"MARA", "MARA Holdings"}},
                                                        dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Crypto stocks & ETFs")));
                                o.insert("list_title", dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "CRYPTO STOCKS & ETFS")));
                                return o;
                            }()),
                    dt_item("news_category", 0, 19, 4, 10, {{"category", "CRYPTO"}}),
                    dt_item("sentiment", 4, 19, 4, 10),
                    dt_item("desk_note", 8, 19, 4, 10, dt_note("crypto_trader")),
                }});

    // ── FX & Rates ───────────────────────────────────────────────────────────
    out.append(DashboardTemplate{"fx_rates",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "FX & Rates"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Currencies against rates: FX and the cross matrix, US yields and the curve, "
                                  "EUR/USD candles, currency performance and the data calendar"),
                {},
                {},
                {
                    dt_item("forex", 0, 0, 4, 9),
                    dt_item("fx_matrix", 4, 0, 4, 9),
                    dt_item("indices", 8, 0, 4, 9, dt_us_rates()),
                    dt_item("candle_chart", 0, 9, 8, 10, {{"symbol", "EURUSD=X"}, {"tf", "6M"}}),
                    dt_item("econ_calendar", 8, 9, 4, 10, {{"country", ""}, {"importance", 2}}),
                    dt_item("yield_curve", 0, 19, 4, 10),
                    dt_item("market_performance", 4, 19, 4, 10,
                            dt_list({{"DX-Y.NYB", "US Dollar Index"},
                                     {"EURUSD=X", "EUR/USD"},
                                     {"USDJPY=X", "USD/JPY"},
                                     {"GBPUSD=X", "GBP/USD"},
                                     {"AUDUSD=X", "AUD/USD"},
                                     {"USDCAD=X", "USD/CAD"},
                                     {"USDCHF=X", "USD/CHF"},
                                     {"USDCNY=X", "USD/CNY"},
                                     {"USDINR=X", "USD/INR"},
                                     {"USDMXN=X", "USD/MXN"},
                                     {"GC=F", "Gold"}},
                                    dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Currencies")))),
                    dt_item("desk_note", 8, 19, 4, 10, dt_note("fx_rates")),
                }});

    // ── Volatility & Risk ────────────────────────────────────────────────────
    out.append(DashboardTemplate{"volatility_risk",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Volatility & Risk"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "The price of protection: VIX family and futures curve, market risk, risk regime, "
                                  "breadth and sector rotation"),
                {},
                {},
                {
                    dt_item("volatility", 0, 0, 4, 10),
                    dt_item("market_risk", 4, 0, 4, 10),
                    dt_item("sentiment", 8, 0, 4, 10),
                    dt_item("candle_chart", 0, 10, 6, 9, {{"symbol", "^VIX"}, {"tf", "1Y"}}),
                    dt_item("sector_heatmap", 6, 10, 6, 9, {{"period", "1W"}}),
                    dt_item("market_breadth", 0, 19, 4, 10),
                    dt_item("news", 4, 19, 4, 10),
                    dt_item("desk_note", 8, 19, 4, 10, dt_note("volatility_risk")),
                }});

    // ── News & Geopolitics ───────────────────────────────────────────────────
    out.append(DashboardTemplate{"news_geopolitics",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "News & Geopolitics"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Headlines with impact, geopolitics and energy news, commodities, prediction "
                                  "markets and the world's indices"),
                {},
                {},
                {
                    dt_item("news", 0, 0, 8, 10),
                    dt_item("sentiment", 8, 0, 4, 10),
                    dt_item("news_category", 0, 10, 4, 10, {{"category", "GEOPOLITICS"}}),
                    dt_item("news_category", 4, 10, 4, 10, {{"category", "ENERGY"}}),
                    dt_item("commodities", 8, 10, 4, 10),
                    dt_item("kalshi_prices", 0, 20, 4, 10),
                    dt_item("global_indices", 4, 20, 4, 10),
                    dt_item("desk_note", 8, 20, 4, 10, dt_note("news_geopolitics")),
                }});

    // ── Research Desk ────────────────────────────────────────────────────────
    out.append(DashboardTemplate{"research_desk",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Research Desk"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "From screen to conviction: quality screen, quote and one-year chart, earnings "
                                  "calendar, sector trends and news"),
                {},
                {},
                {
                    dt_item("stock_quote", 0, 0, 4, 10, {{"symbol", lead}}),
                    dt_item("candle_chart", 4, 0, 8, 10, {{"symbol", lead}, {"tf", "1Y"}}),
                    dt_item("screener", 0, 10, 6, 9, {{"screen", "quality"}}),
                    dt_item("earnings_calendar", 6, 10, 6, 9, {{"scope", "region"}}),
                    dt_item("news", 0, 19, 4, 10),
                    dt_item("sector_heatmap", 4, 19, 4, 10, {{"period", "1M"}}),
                    dt_item("desk_note", 8, 19, 4, 10, dt_note("research_desk")),
                }});

    // ── India Markets (pinned to the India profile) ──────────────────────────
    {
        const QString in = QStringLiteral("IN");
        out.append(
            DashboardTemplate{"india_markets",
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "India Markets"),
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                               "Nifty, Sensex and Bank Nifty, NSE sectors, FII / DII flows, India VIX, rupee and "
                               "Indian news"),
             {},
             in,
             {
                 dt_item("indices", 0, 0, 4, 9, dt_pin(in)),
                 dt_item("sector_heatmap", 4, 0, 4, 9, dt_pin(in)),
                 dt_item("fii_dii_flows", 8, 0, 4, 9),
                 dt_item("top_movers", 0, 9, 4, 8, dt_pin(in, {{"universe", "region"}, {"tab", 0}})),
                 dt_item("market_risk", 4, 9, 4, 8, dt_pin(in)),
                 dt_item("news", 8, 9, 4, 8, {{"region", "INDIA"}}),
                 dt_item("forex", 0, 17, 4, 6, dt_pin(in)),
                 dt_item("world_clock", 0, 23, 4, 4,
                         {{"venues", QJsonArray{"NSE", "SGX", "HKEX", "LSE", "NYSE"}}}),
                 dt_item("candle_chart", 4, 17, 4, 10, {{"symbol", "^NSEBANK"}, {"tf", "6M"}}),
                 dt_item("desk_note", 8, 17, 4, 10, dt_note("india_markets")),
                 dt_item("market_performance", 0, 27, 6, 8, dt_pin(in)),
                 dt_item("econ_calendar", 6, 27, 6, 8, dt_pin(in, {{"country", "IN"}, {"importance", 0}})),
             }});
    }

    // ── Asia-Pacific ─────────────────────────────────────────────────────────
    {
        const QString jp = QStringLiteral("JP");
        out.append(
            DashboardTemplate{"asia_pacific",
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Asia-Pacific"),
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                               "The Asian session: regional indices and their trends, exchange clocks, Asian FX, "
                               "Japan's sectors and movers, Asian news"),
             {},
             {},
             {
                 dt_item("indices", 0, 0, 4, 9,
                         dt_list({{"^N225", "Nikkei 225"},
                                  {"^HSI", "Hang Seng"},
                                  {"000001.SS", "SSE Composite"},
                                  {"399001.SZ", "SZSE Component"},
                                  {"^KS11", "KOSPI"},
                                  {"^TWII", "TAIEX"},
                                  {"^NSEI", "Nifty 50"},
                                  {"^AXJO", "S&P/ASX 200"},
                                  {"^STI", "Straits Times"},
                                  {"^NZ50", "NZX 50"}},
                                 dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Asia-Pacific")))),
                 dt_item("market_performance", 4, 0, 4, 9,
                         dt_list({{"^N225", "Nikkei 225"},
                                  {"^HSI", "Hang Seng"},
                                  {"000001.SS", "SSE Composite"},
                                  {"^KS11", "KOSPI"},
                                  {"^TWII", "TAIEX"},
                                  {"^NSEI", "Nifty 50"},
                                  {"^AXJO", "S&P/ASX 200"},
                                  {"^STI", "Straits Times"}},
                                 dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Asia-Pacific")))),
                 dt_item("world_clock", 8, 0, 4, 9,
                         {{"venues", QJsonArray{"TSE", "HKEX", "SSE", "KRX", "TWSE", "NSE", "SGX", "ASX", "NZX"}}}),
                 dt_item("sector_heatmap", 0, 9, 4, 9, dt_pin(jp)),
                 dt_item("top_movers", 4, 9, 4, 9, dt_pin(jp, {{"universe", "region"}, {"tab", 0}})),
                 dt_item("forex", 8, 9, 4, 9,
                         dt_list({{"USDJPY=X", "USD/JPY"},
                                  {"USDCNY=X", "USD/CNY"},
                                  {"USDHKD=X", "USD/HKD"},
                                  {"USDKRW=X", "USD/KRW"},
                                  {"USDTWD=X", "USD/TWD"},
                                  {"USDINR=X", "USD/INR"},
                                  {"USDSGD=X", "USD/SGD"},
                                  {"AUDUSD=X", "AUD/USD"},
                                  {"NZDUSD=X", "NZD/USD"}},
                                 dt_tr(QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Asia-Pacific")))),
                 dt_item("news", 0, 18, 4, 10, {{"region", "ASIA"}}),
                 dt_item("commodities", 4, 18, 4, 10, dt_pin(jp)),
                 dt_item("desk_note", 8, 18, 4, 10, dt_note("asia_pacific")),
             },
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                               "Its own Asia-Pacific lists of indices and currencies; the sector map, movers and "
                               "commodities use the Japan profile")});
    }

    // ── Europe (pinned to the Eurozone profile) ──────────────────────────────
    {
        const QString eu = QStringLiteral("EU");
        out.append(
            DashboardTemplate{"europe",
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Europe"),
             QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                               "Euro Stoxx, DAX and CAC, STOXX 600 sectors, euro crosses, the euro-area curve, "
                               "energy and the ECB calendar"),
             {},
             eu,
             {
                 dt_item("indices", 0, 0, 4, 9, dt_pin(eu)),
                 dt_item("sector_heatmap", 4, 0, 4, 9, dt_pin(eu)),
                 dt_item("forex", 8, 0, 4, 9, dt_pin(eu)),
                 dt_item("top_movers", 0, 9, 4, 8, dt_pin(eu, {{"universe", "region"}, {"tab", 0}})),
                 dt_item("screener", 4, 9, 4, 8, dt_pin(eu, {{"screen", "largest"}})),
                 dt_item("news", 8, 9, 4, 8, {{"region", "EU"}}),
                 dt_item("yield_curve", 0, 17, 4, 10, {{"curve", "euro"}}),
                 dt_item("commodities", 4, 17, 4, 10, dt_pin(eu)),
                 dt_item("desk_note", 8, 17, 4, 10, dt_note("europe")),
                 dt_item("econ_calendar", 0, 27, 6, 8, dt_pin(eu, {{"country", "@region"}, {"importance", 2}})),
                 dt_item("market_performance", 6, 27, 6, 8, dt_pin(eu)),
             }});
    }

    // ── Quant / Systematic ───────────────────────────────────────────────────
    out.append(DashboardTemplate{"quant_desk",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Quant / Systematic"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Regime and internals: sector trends, volatility, market risk, factor screen, most "
                                  "active, breadth, your scans and runs"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Scan alerts and automation runs fill in once you run strategies from Algo Trading"),
                {},
                {
                    dt_item("sparklines", 0, 0, 4, 10,
                            sector_syms.isEmpty() ? QJsonObject{} : QJsonObject{{"symbols", sector_syms}}),
                    dt_item("volatility", 4, 0, 4, 10),
                    dt_item("market_risk", 8, 0, 4, 10),
                    dt_item("screener", 0, 10, 4, 9, {{"screen", "low_beta"}}),
                    dt_item("top_movers", 4, 10, 4, 9, {{"universe", "region"}, {"tab", 2}}),
                    dt_item("market_breadth", 8, 10, 4, 9, {{"universe", "sectors"}}),
                    dt_item("scan_alerts", 0, 19, 4, 10),
                    dt_item("automation_runs", 4, 19, 4, 10),
                    dt_item("desk_note", 8, 19, 4, 10, dt_note("quant_desk")),
                }});

    // ── Focus (minimal) ──────────────────────────────────────────────────────
    out.append(DashboardTemplate{"focus",
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates", "Focus"),
                QT_TRANSLATE_NOOP("fincept::screens::DashboardTemplates",
                                  "Minimal: headline indices, one large chart, your watchlist, the news and a note — "
                                  "one screen"),
                {},
                {},
                {
                    dt_item("indices", 0, 0, 4, 10),
                    dt_item("candle_chart", 4, 0, 8, 10, {{"symbol", prof.benchmark}, {"tf", "6M"}}),
                    dt_item("watchlist", 0, 10, 4, 9),
                    dt_item("news", 4, 10, 4, 9),
                    dt_item("desk_note", 8, 10, 4, 9, dt_note("focus")),
                }});

    return out;
}

bool find_dashboard_template(const QString& id, DashboardTemplate* out) {
    for (const auto& t : all_dashboard_templates()) {
        if (t.id == id) {
            if (out)
                *out = t;
            return true;
        }
    }
    return false;
}

GridLayout build_template_layout(const DashboardTemplate& t) {
    GridLayout layout;
    layout.cols = 12;
    layout.row_h = kGridRowCompact;
    layout.margin = 4;
    layout.template_id = t.id;
    layout.pristine = true;
    const auto& reg = WidgetRegistry::instance();
    for (auto item : t.items) {
        item.instance_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Registry minimums are in comfortable rows; never above the template's size.
        if (const auto* meta = reg.find(item.id)) {
            item.cell.min_w = std::clamp(meta->min_w, 1, item.cell.w);
            item.cell.min_h = std::clamp(meta->min_h, 1, item.cell.h);
        }
        if (item.id == QLatin1String("desk_note")) {
            QString text;
            const QString seed = item.config.value("seed").toString();
            if (!seed.isEmpty() && template_note_saved(seed, &text))
                item.config.insert("text", text); // the user's own note for this template wins
        }
        layout.items.append(item);
    }
    layout.items = compact_vertical(layout.items);
    return layout;
}

QString template_display_name_tr(const DashboardTemplate& t) {
    return QCoreApplication::translate(kDtTrContext, t.display_name.toUtf8().constData());
}

QString template_description_tr(const DashboardTemplate& t) {
    return QCoreApplication::translate(kDtTrContext, t.description.toUtf8().constData());
}

QString template_requirement_tr(const DashboardTemplate& t) {
    return t.requirement.isEmpty() ? QString()
                                   : QCoreApplication::translate(kDtTrContext, t.requirement.toUtf8().constData());
}

QString template_scope_tr(const DashboardTemplate& t) {
    return t.scope.isEmpty() ? QString() : QCoreApplication::translate(kDtTrContext, t.scope.toUtf8().constData());
}

QString template_display_name_for(const QString& id) {
    DashboardTemplate t;
    return find_dashboard_template(id, &t) ? template_display_name_tr(t) : QString();
}

QString template_note_seed(const QString& template_id) {
    return dt_seed(template_id);
}

bool template_note_saved(const QString& template_id, QString* text) {
    if (template_id.isEmpty())
        return false;
    const QJsonObject entry = dt_read_note_store().value(template_id).toObject();
    if (!entry.contains(QStringLiteral("text")))
        return false;
    if (text)
        *text = entry.value("text").toString();
    return true;
}

void template_note_store(const QString& template_id, const QString& text) {
    if (template_id.isEmpty())
        return;
    QJsonObject all = dt_read_note_store();
    all.insert(template_id, QJsonObject{{"text", text},
                                        {"saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    dt_write_note_store(all);
}

void template_note_forget(const QString& template_id) {
    if (template_id.isEmpty())
        return;
    QJsonObject all = dt_read_note_store();
    if (!all.contains(template_id))
        return;
    all.remove(template_id);
    dt_write_note_store(all);
}

} // namespace fincept::screens
