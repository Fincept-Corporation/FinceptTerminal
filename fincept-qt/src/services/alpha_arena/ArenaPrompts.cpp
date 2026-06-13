#include "services/alpha_arena/ArenaPrompts.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::arena {

QString decision_contract() {
    return QStringLiteral(
        "\n\nOUTPUT FORMAT — reply with ONLY this JSON object, no prose, no markdown:\n"
        "{\"actions\":[\n"
        "  {\"action\":\"open\",\"coin\":\"BTC\",\"side\":\"long|short\",\"size_usd\":2000,\"leverage\":3,\"reason\":\"...\"},\n"
        "  {\"action\":\"close\",\"coin\":\"ETH\",\"reason\":\"...\"},\n"
        "  {\"action\":\"hold\",\"reason\":\"...\"}\n"
        "]}\n"
        "Rules: every action needs a reason. size_usd is position notional in USD. "
        "close with no size_usd closes the whole position; with size_usd it reduces. "
        "If you want no changes this round, return a single hold action.");
}

QString system_prompt(const CompetitionRow& comp) {
    if (!comp.system_prompt_override.isEmpty())
        return comp.system_prompt_override + decision_contract();
    return QStringLiteral(
        "You are an autonomous crypto perpetual-futures trader competing against other AI models "
        "in the Fincept Alpha Arena. You each started with $%1 and trade the same market data. "
        "Your goal: the highest risk-adjusted return.\n\n"
        "Constraints:\n"
        "- Instruments: %2 (USD-margined perps, long or short)\n"
        "- Max leverage: %3x. Taker fee 0.045%. Funding accrues hourly.\n"
        "- One position per coin. Opening the opposite side flips the position.\n"
        "- A decision round happens every %4 seconds; between rounds your positions ride the market.\n"
        "- If your margin is exhausted you are liquidated — survival matters.\n"
        "Be decisive but size positions so a single move against you cannot end your run.")
        .arg(QString::number(comp.capital_per_agent, 'f', 0), comp.instruments.join(", "),
             QString::number(comp.max_leverage, 'f', 0), QString::number(comp.cadence_seconds))
        + decision_contract();
}

QString user_prompt(const MarketSnapshot& market, const AccountView& acct,
                    const QVector<DecisionRow>& recent, int round_seq, qint64 started_at) {
    QString s;
    // market.ts is the round's snapshot time — keeps the prompt deterministic
    // from inputs (no wall-clock read), so identical rounds hash identically.
    s += QStringLiteral("ROUND %1 — elapsed %2 min\n\nMARKET\n")
             .arg(round_seq).arg((market.ts - started_at) / 60000);
    for (const auto& c : market.coins) {
        // Indicators come from candles; with no candle data, zeroed EMA/ATR and
        // a neutral RSI would read as real signal to the model — say n/a instead.
        const QString ind = c.candles_1h.isEmpty()
            ? QStringLiteral("indicators n/a (no candle data)")
            : QStringLiteral("EMA20 %1 | EMA50 %2 | RSI14 %3 | ATR14 %4")
                  .arg(c.ema20, 0, 'g', 8).arg(c.ema50, 0, 'g', 8)
                  .arg(c.rsi14, 0, 'f', 1).arg(c.atr14, 0, 'g', 6);
        s += QStringLiteral("%1: mid %2 | 24h %3% | funding %4%/hr | OI %5 | %6\n")
                 .arg(c.coin).arg(c.mid, 0, 'g', 8).arg(c.day_change_pct, 0, 'f', 2)
                 .arg(c.funding_rate * 100, 0, 'f', 4).arg(c.open_interest, 0, 'g', 6)
                 .arg(ind);
        if (!c.candles_1h.isEmpty()) {
            s += QStringLiteral("  last 1h closes:");
            const int n = std::min<qsizetype>(12, c.candles_1h.size());
            for (int i = c.candles_1h.size() - n; i < c.candles_1h.size(); ++i)
                s += QStringLiteral(" %1").arg(c.candles_1h[i].c, 0, 'g', 8);
            s += '\n';
        }
    }
    s += QStringLiteral("\nYOUR ACCOUNT\nequity $%1 | free balance $%2 | margin used $%3\n")
             .arg(acct.equity, 0, 'f', 2).arg(acct.balance, 0, 'f', 2).arg(acct.margin_used, 0, 'f', 2);
    if (acct.positions.isEmpty()) s += QStringLiteral("no open positions\n");
    for (const auto& p : acct.positions)
        s += QStringLiteral("%1 %2 qty %3 @ entry %4 (%5x) | uPnL $%6\n")
                 .arg(p.side.toUpper(), p.coin).arg(p.qty, 0, 'g', 8)
                 .arg(p.entry_price, 0, 'g', 8).arg(p.leverage, 0, 'f', 1)
                 .arg(acct.upnl.value(p.coin), 0, 'f', 2);
    if (!recent.isEmpty()) {
        s += QStringLiteral("\nYOUR RECENT DECISIONS\n");
        for (const auto& d : recent)
            s += QStringLiteral("round %1: %2%3\n").arg(d.round_seq).arg(d.actions_json.left(220),
                     d.parse_error.isEmpty() ? QString() : QStringLiteral(" [parse_error]"));
    }
    s += QStringLiteral("\nDecide your actions for this round.");
    return s;
}

} // namespace fincept::arena
