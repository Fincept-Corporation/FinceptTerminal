// src/services/equity/EquityResearchModels.h
#pragma once
#include <QDate>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services::equity {

// ── Real-time quote ───────────────────────────────────────────────────────────
struct QuoteData {
    QString symbol;
    double price = 0.0;
    double change = 0.0;
    double change_pct = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double prev_close = 0.0;
    double volume = 0.0;
    QString exchange;
    qint64 timestamp = 0;
};

// ── Company fundamentals ──────────────────────────────────────────────────────
struct StockInfo {
    QString symbol;
    QString company_name;
    QString sector;
    QString industry;
    QString description;
    QString website;
    QString country;
    QString currency; ///< trading currency of the quote
    /// Currency the financial statements are reported in (yfinance `financialCurrency`). Differs
    /// from `currency` for ADRs and many dual listings (TSM: price USD, statements TWD) — any
    /// per-share value derived from statements must not be compared with the price then.
    /// Empty when the source did not say.
    QString financial_currency;
    QString exchange;
    int employees = 0;

    // Valuation
    double market_cap = 0.0;
    double enterprise_value = 0.0;
    double pe_ratio = 0.0;
    double forward_pe = 0.0;
    double peg_ratio = 0.0;
    double price_to_book = 0.0;
    double ev_to_revenue = 0.0;
    double ev_to_ebitda = 0.0;

    // Profitability
    double gross_margins = 0.0;
    double operating_margins = 0.0;
    double ebitda_margins = 0.0;
    double profit_margins = 0.0;
    double roe = 0.0;
    double roa = 0.0;
    double gross_profits = 0.0;

    // Per share / cash
    double book_value = 0.0;
    double revenue_per_share = 0.0;
    double free_cashflow = 0.0;
    double operating_cashflow = 0.0;
    double total_cash = 0.0;
    double total_debt = 0.0;
    double total_revenue = 0.0;

    // Growth
    double earnings_growth = 0.0;
    double revenue_growth = 0.0;

    // Share data
    double shares_outstanding = 0.0;
    double float_shares = 0.0;
    double held_insiders_pct = 0.0;
    double held_institutions_pct = 0.0;
    double short_ratio = 0.0;
    double short_pct_of_float = 0.0;

    // Price range / risk
    double week52_high = 0.0;
    double week52_low = 0.0;
    double avg_volume = 0.0;
    double beta = 0.0;
    double dividend_yield = 0.0;
    double current_price = 0.0;

    // Analyst targets
    double target_high = 0.0;
    double target_low = 0.0;
    double target_mean = 0.0;
    double recommendation_mean = 0.0;
    QString recommendation_key;
    int analyst_count = 0;
};

// ── Historical OHLCV candle ───────────────────────────────────────────────────
struct Candle {
    qint64 timestamp = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    qint64 volume = 0;
};

// ── Financial statements ──────────────────────────────────────────────────────
// period → line_item → value; stored as raw JSON since yfinance returns
// hundreds of heterogeneous line-item names that vary by company.
struct FinancialsData {
    QString symbol;
    // Each entry: (period_string, QJsonObject of line items) — ANNUAL statements (fiscal
    // years), newest first.
    QVector<QPair<QString, QJsonObject>> income_statement;
    QVector<QPair<QString, QJsonObject>> balance_sheet;
    QVector<QPair<QString, QJsonObject>> cash_flow;
    // Line items in statement order (Total Revenue, Cost Of Revenue, Gross Profit, …) as the
    // source delivered them — QJsonObject sorts its keys alphabetically. Empty for a payload
    // cached before the script sent "row_order" (the tables then fall back to A–Z).
    QStringList income_order;
    QStringList balance_order;
    QStringList cash_flow_order;
    /// Quarterly statements (fiscal quarters, newest first). Empty for half-yearly reporters
    /// (most London listings) and for payloads cached before the script sent them.
    QVector<QPair<QString, QJsonObject>> q_income;
    QVector<QPair<QString, QJsonObject>> q_balance;
    QVector<QPair<QString, QJsonObject>> q_cash_flow;
    QStringList q_income_order;
    QStringList q_balance_order;
    QStringList q_cash_flow_order;
    /// Yahoo's trailing-twelve-month columns (one column, labelled with the latest quarter end).
    QVector<QPair<QString, QJsonObject>> ttm_income;
    QVector<QPair<QString, QJsonObject>> ttm_cash_flow;
    qint64 fetched_at = 0; ///< epoch seconds the statements were pulled from the source (0 = unknown)
    /// True when this is the last-known-good copy replayed from cache while a refresh runs
    /// (or after the refresh failed). Statements only change quarterly, so it is shown
    /// immediately — labelled as such — instead of a blank tab.
    bool stale = false;
};

// ── Technical indicator signal ────────────────────────────────────────────────
enum class TechSignal { StrongBuy, Buy, Neutral, Sell, StrongSell };

struct TechIndicator {
    QString name;
    QString key; ///< Technicals frame column ("rsi", "macd_diff", "bb_wband", …; see TechnicalIndicators.h)
    double value = 0.0;
    TechSignal signal = TechSignal::Neutral;
    QString category; // "trend" | "momentum" | "volatility" | "volume"
};

/// One vote in a technical-rating group (EquityTechnicalMath::technical_rating).
struct RatingVote {
    QString name;  ///< "SMA 50", "RSI (14)" — display name (English source, translated by the view)
    QString key;   ///< stable id ("sma_50", "rsi")
    double value = std::numeric_limits<double>::quiet_NaN();
    TechSignal signal = TechSignal::Neutral;
};

/// Moving-average or oscillator group: score = (buys − sells) / voters, −1..1.
struct RatingGroup {
    double score = std::numeric_limits<double>::quiet_NaN();
    TechSignal signal = TechSignal::Neutral;
    int buy = 0;
    int sell = 0;
    int neutral = 0;
    QVector<RatingVote> votes;
};

struct TechnicalsData {
    QString symbol;
    QVector<TechIndicator> trend;
    QVector<TechIndicator> momentum;
    QVector<TechIndicator> volatility;
    QVector<TechIndicator> volume;
    /// = summary_signal when the rating could be computed (see below).
    TechSignal overall_signal = TechSignal::Neutral;
    /// Vote counts of the rating's VOTING indicators only (moving averages + oscillators);
    /// informational readings no longer count as "neutral" votes. Strong buy / sell stay 0
    /// in the two-group rating (votes are buy / neutral / sell).
    int strong_buy = 0;
    int buy = 0;
    int neutral = 0;
    int sell = 0;
    int strong_sell = 0;
    /// Two-group rating off the daily bars: price vs moving averages, and oscillators.
    RatingGroup ma;
    RatingGroup osc;
    double summary_score = std::numeric_limits<double>::quiet_NaN(); ///< mean of the two group scores
    TechSignal summary_signal = TechSignal::Neutral;
    bool rating_ok = false;
};

// ── Corporate events: dividends, splits, earnings ─────────────────────────────
struct DividendEvent {
    QDate date;         ///< ex-date
    double amount = 0;  ///< per share, AS QUOTED (pence for LSE, cents for JSE)
};
struct SplitEvent {
    QDate date;
    double ratio = 1.0; ///< new shares per old share (4.0 = 4-for-1)
};
struct EarningsEvent {
    QDate quarter;      ///< fiscal quarter end (invalid for an older calendar-only row)
    QDate report_date;  ///< invalid when unknown
    bool date_estimated = false; ///< report day inferred from the volume spike, not reported
    double eps_estimate = std::numeric_limits<double>::quiet_NaN();
    double eps_actual = std::numeric_limits<double>::quiet_NaN();
    double surprise_pct = std::numeric_limits<double>::quiet_NaN(); ///< percent units
};
struct PeriodEps {
    QDate period;
    double eps_diluted = std::numeric_limits<double>::quiet_NaN();
    double eps_basic = std::numeric_limits<double>::quiet_NaN();
    double net_income = std::numeric_limits<double>::quiet_NaN();
    double revenue = std::numeric_limits<double>::quiet_NaN();
};
struct EquityEvents {
    QString symbol;
    QString currency;           ///< trading currency as quoted ("GBp")
    QString financial_currency; ///< statements / EPS / dividend-rate currency
    QString quote_type;
    QString exchange;
    QString full_exchange_name;
    QString exchange_tz;
    QString industry_key;
    QString sector_key;
    double trailing_eps = std::numeric_limits<double>::quiet_NaN();   ///< reporting currency
    double forward_eps = std::numeric_limits<double>::quiet_NaN();
    double book_value = std::numeric_limits<double>::quiet_NaN();     ///< per share, reporting currency
    double dividend_rate = std::numeric_limits<double>::quiet_NaN();  ///< forward annual, per share
    double trailing_dividend_rate = std::numeric_limits<double>::quiet_NaN();
    double dividend_yield_pct = std::numeric_limits<double>::quiet_NaN();
    double five_year_avg_yield_pct = std::numeric_limits<double>::quiet_NaN();
    double payout_ratio = std::numeric_limits<double>::quiet_NaN();   ///< fraction of EPS
    double shares_outstanding = std::numeric_limits<double>::quiet_NaN();
    QDate ex_dividend_date;
    QDate dividend_pay_date;
    QDate last_fiscal_year_end;
    QDate most_recent_quarter;
    QDate next_earnings;
    QDate next_earnings_end;
    double est_eps_avg = std::numeric_limits<double>::quiet_NaN();
    double est_eps_low = std::numeric_limits<double>::quiet_NaN();
    double est_eps_high = std::numeric_limits<double>::quiet_NaN();
    double est_rev_avg = std::numeric_limits<double>::quiet_NaN();
    QVector<DividendEvent> dividends; ///< oldest first
    QVector<SplitEvent> splits;       ///< oldest first
    QVector<EarningsEvent> earnings;  ///< oldest first
    QVector<PeriodEps> eps_quarterly; ///< oldest first
    QVector<PeriodEps> eps_annual;    ///< oldest first
};

// ── Peer comparison ───────────────────────────────────────────────────────────
struct PeerData {
    QString symbol;
    QString name;
    QString sector;
    double market_cap = 0.0;
    double pe_ratio = 0.0;
    double forward_pe = 0.0;
    double price_to_book = 0.0;
    double price_to_sales = 0.0;
    double peg_ratio = 0.0;
    double roe = 0.0;
    double roa = 0.0;
    double profit_margin = 0.0;
    double operating_margin = 0.0;
    double gross_margin = 0.0;
    double revenue_growth = 0.0;
    double earnings_growth = 0.0;
    double debt_to_equity = 0.0;
    double current_ratio = 0.0;
    double quick_ratio = 0.0;
    double dividend_yield = 0.0;
    double beta = 0.0;
    double price = 0.0;      ///< as quoted (pence for LSE)
    double change_pct = 0.0; ///< percent units
    // Listing context — market caps of peers listed in different currencies only compare
    // through market_cap_usd.
    QString currency;           ///< trading currency as quoted
    QString financial_currency; ///< statements currency
    QString country;
    QString industry;
    QString exchange;
    double market_cap_usd = 0.0;   ///< 0 = no FX rate
    double enterprise_value = 0.0; ///< trading currency (major units)
    double ev_to_ebitda = 0.0;
    double ev_to_revenue = 0.0;
};

// ── News article ──────────────────────────────────────────────────────────────
struct NewsArticle {
    QString title;
    QString description;
    QString url;
    QString publisher;
    QString published_date;
    /// Finance-tuned VADER tone of title + description (the model the News tab and the
    /// Sentiment tab use): "BULLISH" | "BEARISH" | "NEUTRAL", score -1..1.
    QString sentiment;
    double sentiment_score = 0.0;
};

/// Unix seconds of a provider news timestamp — ISO-8601 (with or without a zone),
/// "yyyy-MM-dd HH:mm:ss", RFC 2822, or a bare date. 0 when unparseable.
inline qint64 news_timestamp(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return 0;
    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::RFC2822Date);
    if (!dt.isValid())
        dt = QDateTime::fromString(s.left(19), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid()) {
        const QDate d = QDate::fromString(s.left(10), Qt::ISODate);
        if (d.isValid())
            dt = d.startOfDay();
    }
    return dt.isValid() ? dt.toSecsSinceEpoch() : 0;
}

// ── Optional market sentiment snapshot ──────────────────────────────────────
struct SentimentSourceSnapshot {
    QString source_id;
    QString label;
    bool available = false;
    double buzz_score = 0.0;
    double bullish_pct = 0.0;
    double sentiment_score = 0.0;
    double activity_count = 0.0;
};

struct MarketSentimentSnapshot {
    QString symbol;
    bool configured = false;
    bool available = false;
    QString status;
    QString message;
    double average_buzz = 0.0;
    double average_bullish_pct = 0.0;
    int coverage = 0;
    QString source_alignment;
    QVector<SentimentSourceSnapshot> sources;
    QString fetched_at;
};

// ── Self-computed (keyless) sentiment ───────────────────────────────────────
// Produced by EquitySentimentService by blending free signals (news headline
// NLP + price/technical momentum, plus optional Adanos when keyed). Replaces
// the Adanos-only view as the default Sentiment tab content.

// One scored headline, for the per-article list.
struct ArticleSentiment {
    QString title;
    QString publisher;
    QString published_date;
    QString url;
    QString label;      // "BULLISH" | "BEARISH" | "NEUTRAL"
    double score = 0.0; // -1..1
};

// One contributing signal in the blend (news / price / adanos).
struct SentimentSource {
    QString id;              // "news" | "price" | "adanos"
    QString label;           // human-readable
    double score = 0.0;      // -1..1
    double weight = 0.0;     // normalized blend weight actually applied (0..1)
    double confidence = 0.0; // 0..1
    bool available = false;
};

struct EquitySentimentSnapshot {
    QString symbol;
    bool available = false;
    QString status; // "ok" | "loading" | "unavailable"
    QString message;
    QString engine;             // "vader" | "lexicon"
    double overall_score = 0.0; // -1..1 blended
    QString label;              // "BULLISH" | "BEARISH" | "NEUTRAL"
    double confidence = 0.0;    // 0..1
    int bullish = 0;
    int bearish = 0;
    int neutral = 0;
    int article_count = 0;
    QVector<SentimentSource> sources;
    QVector<ArticleSentiment> articles;
    QString fetched_at;
};

} // namespace fincept::services::equity
