#pragma once
// CompanySnapshotService — one financial snapshot of a listed company, for any tab that wants to
// start from a ticker instead of a blank form (M&A prefill, REIT comparison, Derivatives spot /
// dividend, Report Builder company facts).
//
//   using namespace fincept::services;
//   CompanySnapshotService::instance().fetch(QStringLiteral("AAPL"), {}, this, [this](const CompanySnapshot& s) {
//       if (!s.ok) return show_error(s.error);                 // nothing usable came back
//       if (s.has(CompanySnapshot::kEbit)) ebit_->setValue(s.ebit_ltm);
//       caption_->setText(s.caption());                        // "Yahoo Finance · TTM to 30 Sep 2026 · as of 14:02"
//       if (!s.missing.isEmpty()) note_->setText(tr("Not available: %1").arg(s.missing_labels().join(", ")));
//   });
//
// Sources, in order, with nothing invented: (1) the terminal's existing Yahoo Finance path
// (EquityResearchService: company info + financial statements, both cached there); (2) for US
// SEC filers only, SEC EDGAR company facts (scripts/Analytics/corporateFinance/valuation/
// sec_company_facts.py: declared User-Agent "Fincept Terminal support@fincept.in", at most 8
// requests per second) fills the fields Yahoo did not give. A field neither source has stays NaN and
// is listed in `missing`. Each filled field records its source.
//
// Asynchronous throughout (signals and a Python process; nothing on the GUI thread waits); a result
// is cached 10 minutes (memory and CacheManager), partial results are delivered as ok = true with
// `missing` set, and a fetch for a symbol already in flight joins it.

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <limits>

namespace fincept::services {

struct CompanySnapshot {
    // Field keys (used by has(), `missing` and `field_source`).
    static constexpr const char* kPrice = "price";
    static constexpr const char* kWeek52Low = "week52_low";
    static constexpr const char* kWeek52High = "week52_high";
    static constexpr const char* kShares = "shares_outstanding";
    static constexpr const char* kMarketCap = "market_cap";
    static constexpr const char* kEnterpriseValue = "enterprise_value";
    static constexpr const char* kTotalDebt = "total_debt";
    static constexpr const char* kCash = "cash";
    static constexpr const char* kNetDebt = "net_debt";
    static constexpr const char* kBeta = "beta";
    static constexpr const char* kDividendYield = "dividend_yield";
    static constexpr const char* kDividendPerShare = "dividend_per_share";
    static constexpr const char* kRevenue = "revenue_ltm";
    static constexpr const char* kEbitda = "ebitda_ltm";
    static constexpr const char* kEbit = "ebit_ltm";
    static constexpr const char* kNetIncome = "net_income_ltm";
    static constexpr const char* kEps = "eps_ltm";
    static constexpr const char* kDepreciation = "depreciation_ltm";
    static constexpr const char* kInterestExpense = "interest_expense_ltm";
    static constexpr const char* kTaxRate = "effective_tax_rate";
    static constexpr const char* kBookValue = "book_value_per_share";
    static constexpr const char* kPriceToBook = "price_to_book";

    QString symbol;             ///< as requested (upper case)
    QString name;
    QString currency;           ///< trading currency of the price
    QString financial_currency; ///< currency of the statements (differs for ADRs / dual listings)
    QString exchange;
    QString sector;
    QString industry;

    static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    double price = kNaN;
    double week52_low = kNaN;
    double week52_high = kNaN;
    double shares_outstanding = kNaN;
    double market_cap = kNaN;
    double enterprise_value = kNaN;
    double total_debt = kNaN;
    double cash = kNaN;
    double net_debt = kNaN; ///< total_debt - cash, only when both are known
    double beta = kNaN;
    double dividend_yield = kNaN;     ///< fraction (0.012 = 1.2%)
    double dividend_per_share = kNaN; ///< annual, trading currency
    double revenue_ltm = kNaN;
    double ebitda_ltm = kNaN;
    double ebit_ltm = kNaN;
    double net_income_ltm = kNaN;
    double eps_ltm = kNaN; ///< diluted
    double depreciation_ltm = kNaN;
    double interest_expense_ltm = kNaN;
    double effective_tax_rate = kNaN; ///< fraction, from the statements (tax / pre-tax income)
    double book_value_per_share = kNaN;
    double price_to_book = kNaN;

    /// What the flow figures cover: "TTM to 2026-09-30", "FY ending 2025-12-31".
    QString ltm_label;
    QDateTime as_of; ///< when the data was fetched
    /// "Yahoo Finance", "SEC EDGAR company facts" or both joined with " + ".
    QString source;
    /// field key -> "yahoo" | "sec" (only for fields that have a value).
    QHash<QString, QString> field_source;
    /// Field keys with no value from any source.
    QStringList missing;
    /// Non-fatal remarks ("statements are in TWD while the price is in USD").
    QStringList notes;

    bool ok = false; ///< at least one figure came back
    QString error;   ///< plain-language reason when !ok
    bool from_cache = false;

    bool has(const char* field) const;
    double value(const char* field) const;
    /// "Yahoo Finance · TTM to 30 Sep 2026 · as of 14:02"
    QString caption() const;
    /// Human labels of `missing` ("EBITDA (last 12 months)", "Total debt").
    QStringList missing_labels() const;
    static QString field_label(const QString& key);

    QJsonObject to_json() const;
    static CompanySnapshot from_json(const QJsonObject& o);
};

class CompanySnapshotService : public QObject {
    Q_OBJECT
  public:
    static CompanySnapshotService& instance();

    struct Options {
        /// Also load the financial statements (revenue, EBITDA, EBIT, net income, EPS). Off = the quote
        /// side only (price, range, shares, market cap, debt, cash, beta, dividend) — one call.
        bool financials = true;
        /// Fill gaps from SEC EDGAR company facts (US filers only).
        bool sec_fallback = true;
        /// Skip the 10-minute cache.
        bool force = false;
        /// Give up waiting after this long and deliver what arrived (partial or an error).
        int timeout_ms = 60 * 1000;
    };
    using Callback = std::function<void(const CompanySnapshot&)>;

    /// Start (or join) a fetch. `cb` runs on the GUI thread once, unless `ctx` is destroyed first or
    /// the request is cancelled. Returns a request id (never 0).
    quint64 fetch(const QString& symbol, const Options& options, QObject* ctx, Callback cb);
    quint64 fetch(const QString& symbol, QObject* ctx, Callback cb) { return fetch(symbol, Options{}, ctx, cb); }
    /// Drop request `id` (its callback will not run). The underlying loads finish for other waiters.
    void cancel(quint64 id);
    /// A snapshot fetched within the last 10 minutes (memory or persistent cache), else !ok.
    CompanySnapshot cached(const QString& symbol) const;

    static constexpr int kCacheTtlSec = 10 * 60;

  private:
    explicit CompanySnapshotService(QObject* parent = nullptr);
    Q_DISABLE_COPY(CompanySnapshotService)

    struct Waiter {
        quint64 id = 0;
        QPointer<QObject> ctx;
        bool has_ctx = false;
        Callback cb;
    };
    struct Job {
        quint64 serial = 0; ///< guards the timeout / SEC reply of an older job for the same symbol
        QString symbol;
        Options options;
        CompanySnapshot snap;
        bool info_done = false;
        bool financials_done = false;
        bool sec_started = false;
        bool sec_done = false;
        QString info_error;
        QString financials_error;
        QVector<Waiter> waiters;
    };

    void connect_equity_once();
    /// Every source that was asked has answered (or failed): fill gaps from SEC once, then finish.
    void maybe_continue(const QString& symbol);
    void start_sec(const QString& symbol);
    void finish(const QString& symbol, bool timed_out = false);
    static void deliver(const Waiter& w, const CompanySnapshot& s);

    QHash<QString, Job> jobs_; ///< by symbol
    QHash<quint64, QString> id_symbol_;
    QHash<QString, CompanySnapshot> memo_; ///< finished snapshots (10-minute TTL by as_of)
    quint64 next_id_ = 0;
    quint64 next_serial_ = 0;
    bool equity_connected_ = false;
};

} // namespace fincept::services
