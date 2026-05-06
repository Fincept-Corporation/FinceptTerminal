#include "services/alpha_arena/ContextBuilder.h"

#include <QChar>
#include <QDateTime>
#include <QString>
#include <QTextStream>
#include <QTimeZone>

#include <cmath>

namespace fincept::services::alpha_arena {

// ─── Stable float formatting ────────────────────────────────────────────────
//
// QString::number is locale-independent for 'f' format, but to belt-and-brace
// we route everything through these helpers so a future surprise (e.g. a
// reviewer adding setlocale somewhere) cannot drift the prompt bytes.

namespace {

QString fmt_price(double v) {
    if (std::isnan(v)) return QStringLiteral("null");
    return QString::number(v, 'f', 6);
}

QString fmt_indicator(double v) {
    if (std::isnan(v)) return QStringLiteral("null");
    return QString::number(v, 'f', 4);
}

QString fmt_qty(double v) {
    if (std::isnan(v)) return QStringLiteral("null");
    return QString::number(v, 'f', 8);
}

QString fmt_pct(double v) {
    if (std::isnan(v)) return QStringLiteral("null");
    return QString::number(v, 'f', 4);
}

QString fmt_int(int v) {
    return QString::number(v);
}

QString fmt_iso_utc(qint64 ms) {
    const auto dt = QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc());
    return dt.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"));
}

void append_bars(QTextStream& s, const QVector<Bar>& bars) {
    s << "[";
    for (int i = 0; i < bars.size(); ++i) {
        const auto& b = bars[i];
        if (i > 0) s << ",";
        s << "{\"t\":\"" << fmt_iso_utc(b.utc_ms) << "\","
          << "\"o\":" << fmt_price(b.open) << ","
          << "\"h\":" << fmt_price(b.high) << ","
          << "\"l\":" << fmt_price(b.low) << ","
          << "\"c\":" << fmt_price(b.close) << ","
          << "\"v\":" << fmt_qty(b.volume) << "}";
    }
    s << "]";
}

// Manual JSON string escape — keeps the prompt byte sequence under our
// exclusive control (no QJsonDocument round-trip surprises).
QString json_escape(const QString& in) {
    QString out;
    out.reserve(in.size() + 2);
    out.append(QChar('"'));
    for (const QChar c : in) {
        const ushort u = c.unicode();
        switch (u) {
        case '"':  out.append(QStringLiteral("\\\"")); break;
        case '\\': out.append(QStringLiteral("\\\\")); break;
        case '\b': out.append(QStringLiteral("\\b"));  break;
        case '\f': out.append(QStringLiteral("\\f"));  break;
        case '\n': out.append(QStringLiteral("\\n"));  break;
        case '\r': out.append(QStringLiteral("\\r"));  break;
        case '\t': out.append(QStringLiteral("\\t"));  break;
        default:
            if (u < 0x20) {
                out.append(QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0')));
            } else {
                out.append(c);
            }
        }
    }
    out.append(QChar('"'));
    return out;
}

void append_position(QTextStream& s, const Position& p) {
    s << "{"
      << "\"coin\":" << json_escape(p.coin) << ","
      << "\"qty\":" << fmt_qty(p.qty) << ","
      << "\"entry\":" << fmt_price(p.entry) << ","
      << "\"mark\":" << fmt_price(p.mark) << ","
      << "\"liq_price\":" << fmt_price(p.liq_price) << ","
      << "\"unrealized_pnl\":" << fmt_price(p.unrealized_pnl) << ","
      << "\"leverage\":" << fmt_int(p.leverage) << ","
      << "\"profit_target\":" << fmt_price(p.profit_target) << ","
      << "\"stop_loss\":" << fmt_price(p.stop_loss) << ","
      << "\"invalidation_condition\":" << json_escape(p.invalidation_condition)
      << "}";
}

} // namespace

// ─── System prompt ──────────────────────────────────────────────────────────

QString build_system_prompt(CompetitionMode mode) {
    QString s;
    QTextStream w(&s);
    w << "You are an autonomous trading agent competing in Alpha Arena. "
         "All competitors receive an identical context every tick. You compete "
         "on real or paper Hyperliquid perpetuals.\n"
         "\n"
         "## Action grammar\n"
         "\n"
         "On every tick you must emit a JSON object of the form:\n"
         "  {\"actions\": [<action>, ...]}\n"
         "where each <action> object has fields:\n"
         "  signal                  string, one of "
         "[\"buy_to_enter\", \"sell_to_enter\", \"hold\", \"close\"]\n"
         "  coin                    string, base symbol from the perp universe "
         "(BTC, ETH, SOL, BNB, DOGE, XRP)\n"
         "  quantity                number, base-asset units (>0 for entries)\n"
         "  leverage                integer in [1, 20]\n"
         "  profit_target           number, absolute price (>0 for entries)\n"
         "  stop_loss               number, absolute price (>0 for entries)\n"
         "  invalidation_condition  string up to 280 chars — the condition that "
         "would void this thesis\n"
         "  confidence              number in [0, 1]\n"
         "  risk_usd                number — your estimate of |entry-stop|*qty; "
         "the engine recomputes\n"
         "  justification           string up to 1000 chars\n"
         "\n"
         "## Hard rules (engine enforced — violations are silently rejected)\n"
         "\n"
         "1. One position per coin. You may not pyramid, hedge, or partially close.\n"
         "2. Risk per trade ≤ 2% of account equity (recomputed by the engine).\n"
         "3. Liquidation buffer ≥ 15% of mark price.\n"
         "4. Maximum leverage is 20×.\n"
         "5. No memory across ticks. Your only persistent state is the position "
         "table you see in the user prompt — including the invalidation_condition "
         "you set on entry. Treat that field as your externalised memory.\n"
         "6. Series in the user prompt are ordered OLDEST → NEWEST.\n"
         "7. There is no chat history. Each call is independent.\n";

    if (mode == CompetitionMode::Monk) {
        w << "\n"
             "## Monk-mode rules\n"
             "\n"
             "Additional caps apply:\n"
             "  • At most one entry per coin per 30 minutes.\n"
             "  • Notional position size capped at 25% of equity.\n";
    } else if (mode == CompetitionMode::Situational) {
        w << "\n"
             "## Situational-mode\n"
             "\n"
             "Your user prompt also contains a `peers` array showing the other "
             "agents' open positions (coin, qty, leverage). Equity and PnL of "
             "peers are NOT disclosed.\n";
    }

    w << "\n"
         "## Output\n"
         "\n"
         "Reply with the JSON object only. Prose around it is permitted but "
         "the engine extracts the outermost {...}. Do not retry on parse "
         "errors — a malformed tick is a no-op.\n";
    return s;
}

// ─── User prompt ────────────────────────────────────────────────────────────

QString build_user_prompt(const TickContext& ctx,
                          const AgentAccountState& account,
                          CompetitionMode mode,
                          const std::optional<SituationalContext>& situational) {
    QString s;
    QTextStream w(&s);

    // Top-level envelope. Stable field order is critical for the byte-equal
    // invariant: {tick, mode, markets, account, peers}.
    w << "{";

    // tick
    w << "\"tick\":{"
      << "\"utc\":\"" << fmt_iso_utc(ctx.tick.utc_ms) << "\","
      << "\"seq\":" << fmt_int(ctx.tick.seq)
      << "},";

    // mode
    w << "\"mode\":\"" << mode_to_wire(mode) << "\",";

    // markets — coin order: as supplied (engine sorts to canonical order
    // before passing in; we don't re-sort here).
    w << "\"markets\":[";
    for (int i = 0; i < ctx.markets.size(); ++i) {
        const auto& m = ctx.markets[i];
        if (i > 0) w << ",";
        w << "{"
          << "\"coin\":" << json_escape(m.coin) << ","
          << "\"mid\":"  << fmt_price(m.mid) << ","
          << "\"bid\":"  << fmt_price(m.bid) << ","
          << "\"ask\":"  << fmt_price(m.ask) << ","
          << "\"open_interest\":" << fmt_qty(m.open_interest) << ","
          << "\"funding_rate\":" << fmt_pct(m.funding_rate) << ","
          << "\"ema20\":" << fmt_indicator(m.ema20) << ","
          << "\"rsi7\":" << fmt_indicator(m.rsi7) << ","
          << "\"macd\":" << fmt_indicator(m.macd) << ","
          << "\"macd_signal\":" << fmt_indicator(m.macd_signal) << ","
          << "\"bars_3m_24h\":";
        append_bars(w, m.bars_3m_24h);
        w << ",\"bars_4h_30d\":";
        append_bars(w, m.bars_4h_30d);
        w << "}";
    }
    w << "],";

    // account
    w << "\"account\":{"
      << "\"equity\":" << fmt_price(account.equity) << ","
      << "\"cash\":" << fmt_price(account.cash) << ","
      << "\"return_pct\":" << fmt_pct(account.return_pct) << ","
      << "\"sharpe\":" << fmt_pct(account.sharpe) << ","
      << "\"max_drawdown\":" << fmt_pct(account.max_drawdown) << ","
      << "\"fees_paid\":" << fmt_price(account.fees_paid) << ","
      << "\"win_rate\":" << fmt_pct(account.win_rate) << ","
      << "\"trades_count\":" << fmt_int(account.trades_count) << ","
      << "\"positions\":[";
    for (int i = 0; i < account.positions.size(); ++i) {
        if (i > 0) w << ",";
        append_position(w, account.positions[i]);
    }
    w << "]},";

    // peers — only present in situational mode
    w << "\"peers\":";
    if (mode == CompetitionMode::Situational && situational.has_value()) {
        w << "[";
        for (int i = 0; i < situational->peers.size(); ++i) {
            const auto& p = situational->peers[i];
            if (i > 0) w << ",";
            w << "{"
              << "\"agent\":" << json_escape(p.agent_display_name) << ","
              << "\"coin\":" << json_escape(p.coin) << ","
              << "\"qty\":" << fmt_qty(p.qty) << ","
              << "\"leverage\":" << fmt_int(p.leverage)
              << "}";
        }
        w << "]";
    } else {
        w << "null";
    }

    w << "}";
    return s;
}

} // namespace fincept::services::alpha_arena
