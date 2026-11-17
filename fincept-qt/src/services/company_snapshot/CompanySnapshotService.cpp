#include "services/company_snapshot/CompanySnapshotService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityResearchService.h"
#include "services/equity/SecFilingsService.h"
#include "storage/cache/CacheManager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMetaObject>
#include <QTimer>

#include <cmath>

namespace fincept::services {

namespace {

const QString kSnapSecScript = QStringLiteral("Analytics/corporateFinance/valuation/sec_company_facts.py");
constexpr int kSnapSecTimeoutMs = 45 * 1000;

QString snap_cache_key(const QString& symbol) {
    return QStringLiteral("company_snapshot:") + symbol;
}

/// Every numeric field: key -> member.
struct SnapField {
    const char* key;
    double CompanySnapshot::*member;
    bool financial; ///< comes from the statements (only asked for with Options::financials)
};

const SnapField kSnapFields[] = {
    {CompanySnapshot::kPrice, &CompanySnapshot::price, false},
    {CompanySnapshot::kWeek52Low, &CompanySnapshot::week52_low, false},
    {CompanySnapshot::kWeek52High, &CompanySnapshot::week52_high, false},
    {CompanySnapshot::kShares, &CompanySnapshot::shares_outstanding, false},
    {CompanySnapshot::kMarketCap, &CompanySnapshot::market_cap, false},
    {CompanySnapshot::kEnterpriseValue, &CompanySnapshot::enterprise_value, false},
    {CompanySnapshot::kTotalDebt, &CompanySnapshot::total_debt, false},
    {CompanySnapshot::kCash, &CompanySnapshot::cash, false},
    {CompanySnapshot::kNetDebt, &CompanySnapshot::net_debt, false},
    {CompanySnapshot::kBeta, &CompanySnapshot::beta, false},
    {CompanySnapshot::kDividendYield, &CompanySnapshot::dividend_yield, false},
    {CompanySnapshot::kDividendPerShare, &CompanySnapshot::dividend_per_share, false},
    {CompanySnapshot::kBookValue, &CompanySnapshot::book_value_per_share, false},
    {CompanySnapshot::kPriceToBook, &CompanySnapshot::price_to_book, false},
    {CompanySnapshot::kRevenue, &CompanySnapshot::revenue_ltm, true},
    {CompanySnapshot::kEbitda, &CompanySnapshot::ebitda_ltm, true},
    {CompanySnapshot::kEbit, &CompanySnapshot::ebit_ltm, true},
    {CompanySnapshot::kNetIncome, &CompanySnapshot::net_income_ltm, true},
    {CompanySnapshot::kEps, &CompanySnapshot::eps_ltm, true},
    {CompanySnapshot::kDepreciation, &CompanySnapshot::depreciation_ltm, true},
    {CompanySnapshot::kInterestExpense, &CompanySnapshot::interest_expense_ltm, true},
    {CompanySnapshot::kTaxRate, &CompanySnapshot::effective_tax_rate, true},
};

const SnapField* snap_field(const QString& key) {
    for (const SnapField& f : kSnapFields)
        if (key == QLatin1String(f.key))
            return &f;
    return nullptr;
}

/// Yahoo's info payload uses 0 for "not reported": a positive value or nothing.
double snap_pos(double v) {
    return (std::isfinite(v) && v > 0) ? v : CompanySnapshot::kNaN;
}

/// One line item of a statement column, trying the names in order (NaN when none is a number).
double snap_item(const QJsonObject& col, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        const QJsonValue v = col.value(QLatin1String(n));
        if (v.isDouble() && std::isfinite(v.toDouble()))
            return v.toDouble();
    }
    return CompanySnapshot::kNaN;
}

void snap_set(CompanySnapshot& s, double CompanySnapshot::*member, const char* key, double v, const QString& src) {
    if (!std::isfinite(v))
        return;
    s.*member = v;
    s.field_source.insert(QString::fromLatin1(key), src);
}

} // namespace

// ── CompanySnapshot ─────────────────────────────────────────────────────────────

bool CompanySnapshot::has(const char* field) const {
    return std::isfinite(value(field));
}

double CompanySnapshot::value(const char* field) const {
    const SnapField* f = snap_field(QString::fromLatin1(field));
    return f ? this->*(f->member) : kNaN;
}

QString CompanySnapshot::field_label(const QString& key) {
    static const QHash<QString, const char*> kLabels = {
        {QString::fromLatin1(kPrice), QT_TRANSLATE_NOOP("CompanySnapshot", "Share price")},
        {QString::fromLatin1(kWeek52Low), QT_TRANSLATE_NOOP("CompanySnapshot", "52-week low")},
        {QString::fromLatin1(kWeek52High), QT_TRANSLATE_NOOP("CompanySnapshot", "52-week high")},
        {QString::fromLatin1(kShares), QT_TRANSLATE_NOOP("CompanySnapshot", "Shares outstanding")},
        {QString::fromLatin1(kMarketCap), QT_TRANSLATE_NOOP("CompanySnapshot", "Market cap")},
        {QString::fromLatin1(kEnterpriseValue), QT_TRANSLATE_NOOP("CompanySnapshot", "Enterprise value")},
        {QString::fromLatin1(kTotalDebt), QT_TRANSLATE_NOOP("CompanySnapshot", "Total debt")},
        {QString::fromLatin1(kCash), QT_TRANSLATE_NOOP("CompanySnapshot", "Cash")},
        {QString::fromLatin1(kNetDebt), QT_TRANSLATE_NOOP("CompanySnapshot", "Net debt")},
        {QString::fromLatin1(kBeta), QT_TRANSLATE_NOOP("CompanySnapshot", "Beta")},
        {QString::fromLatin1(kDividendYield), QT_TRANSLATE_NOOP("CompanySnapshot", "Dividend yield")},
        {QString::fromLatin1(kDividendPerShare), QT_TRANSLATE_NOOP("CompanySnapshot", "Dividend per share")},
        {QString::fromLatin1(kBookValue), QT_TRANSLATE_NOOP("CompanySnapshot", "Book value per share")},
        {QString::fromLatin1(kPriceToBook), QT_TRANSLATE_NOOP("CompanySnapshot", "Price / book")},
        {QString::fromLatin1(kRevenue), QT_TRANSLATE_NOOP("CompanySnapshot", "Revenue (last 12 months)")},
        {QString::fromLatin1(kEbitda), QT_TRANSLATE_NOOP("CompanySnapshot", "EBITDA (last 12 months)")},
        {QString::fromLatin1(kEbit), QT_TRANSLATE_NOOP("CompanySnapshot", "EBIT (last 12 months)")},
        {QString::fromLatin1(kNetIncome), QT_TRANSLATE_NOOP("CompanySnapshot", "Net income (last 12 months)")},
        {QString::fromLatin1(kEps), QT_TRANSLATE_NOOP("CompanySnapshot", "Diluted EPS (last 12 months)")},
        {QString::fromLatin1(kDepreciation), QT_TRANSLATE_NOOP("CompanySnapshot", "D&A (last 12 months)")},
        {QString::fromLatin1(kInterestExpense), QT_TRANSLATE_NOOP("CompanySnapshot", "Interest expense (last 12 months)")},
        {QString::fromLatin1(kTaxRate), QT_TRANSLATE_NOOP("CompanySnapshot", "Effective tax rate")},
    };
    const char* l = kLabels.value(key, nullptr);
    return l ? QCoreApplication::translate("CompanySnapshot", l) : key;
}

QStringList CompanySnapshot::missing_labels() const {
    QStringList out;
    for (const QString& k : missing)
        out << field_label(k);
    return out;
}

QString CompanySnapshot::caption() const {
    QStringList parts;
    if (!source.isEmpty())
        parts << source;
    if (!ltm_label.isEmpty())
        parts << ltm_label;
    if (as_of.isValid()) {
        const bool today = as_of.date() == QDate::currentDate();
        parts << QCoreApplication::translate("CompanySnapshot", "as of %1")
                     .arg(QLocale().toString(as_of, today ? QStringLiteral("HH:mm") : QStringLiteral("d MMM yyyy HH:mm")));
    }
    return parts.join(QStringLiteral(" · "));
}

QJsonObject CompanySnapshot::to_json() const {
    QJsonObject o;
    o.insert(QStringLiteral("symbol"), symbol);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("currency"), currency);
    o.insert(QStringLiteral("financial_currency"), financial_currency);
    o.insert(QStringLiteral("exchange"), exchange);
    o.insert(QStringLiteral("sector"), sector);
    o.insert(QStringLiteral("industry"), industry);
    for (const SnapField& f : kSnapFields) {
        const double v = this->*(f.member);
        if (std::isfinite(v))
            o.insert(QString::fromLatin1(f.key), v);
    }
    o.insert(QStringLiteral("ltm_label"), ltm_label);
    o.insert(QStringLiteral("as_of"), double(as_of.toMSecsSinceEpoch()));
    o.insert(QStringLiteral("source"), source);
    QJsonObject fs;
    for (auto it = field_source.constBegin(); it != field_source.constEnd(); ++it)
        fs.insert(it.key(), it.value());
    o.insert(QStringLiteral("field_source"), fs);
    o.insert(QStringLiteral("missing"), QJsonArray::fromStringList(missing));
    o.insert(QStringLiteral("notes"), QJsonArray::fromStringList(notes));
    o.insert(QStringLiteral("ok"), ok);
    o.insert(QStringLiteral("error"), error);
    return o;
}

CompanySnapshot CompanySnapshot::from_json(const QJsonObject& o) {
    CompanySnapshot s;
    s.symbol = o.value(QStringLiteral("symbol")).toString();
    s.name = o.value(QStringLiteral("name")).toString();
    s.currency = o.value(QStringLiteral("currency")).toString();
    s.financial_currency = o.value(QStringLiteral("financial_currency")).toString();
    s.exchange = o.value(QStringLiteral("exchange")).toString();
    s.sector = o.value(QStringLiteral("sector")).toString();
    s.industry = o.value(QStringLiteral("industry")).toString();
    for (const SnapField& f : kSnapFields) {
        const QJsonValue v = o.value(QLatin1String(f.key));
        if (v.isDouble())
            s.*(f.member) = v.toDouble();
    }
    s.ltm_label = o.value(QStringLiteral("ltm_label")).toString();
    s.as_of = QDateTime::fromMSecsSinceEpoch(qint64(o.value(QStringLiteral("as_of")).toDouble()));
    s.source = o.value(QStringLiteral("source")).toString();
    const QJsonObject fs = o.value(QStringLiteral("field_source")).toObject();
    for (auto it = fs.constBegin(); it != fs.constEnd(); ++it)
        s.field_source.insert(it.key(), it.value().toString());
    for (const QJsonValue& v : o.value(QStringLiteral("missing")).toArray())
        s.missing << v.toString();
    for (const QJsonValue& v : o.value(QStringLiteral("notes")).toArray())
        s.notes << v.toString();
    s.ok = o.value(QStringLiteral("ok")).toBool();
    s.error = o.value(QStringLiteral("error")).toString();
    return s;
}

// ── Service ─────────────────────────────────────────────────────────────────────

CompanySnapshotService& CompanySnapshotService::instance() {
    static CompanySnapshotService s;
    return s;
}

CompanySnapshotService::CompanySnapshotService(QObject* parent) : QObject(parent) {}

void CompanySnapshotService::deliver(const Waiter& w, const CompanySnapshot& s) {
    if (w.has_ctx && !w.ctx)
        return;
    if (w.cb)
        w.cb(s);
}

CompanySnapshot CompanySnapshotService::cached(const QString& symbol_in) const {
    const QString symbol = symbol_in.trimmed().toUpper();
    const auto it = memo_.constFind(symbol);
    if (it != memo_.constEnd() && it->as_of.isValid() &&
        it->as_of.secsTo(QDateTime::currentDateTime()) < kCacheTtlSec) {
        CompanySnapshot s = *it;
        s.from_cache = true;
        return s;
    }
    const QVariant v = fincept::CacheManager::instance().get(snap_cache_key(symbol));
    if (!v.isNull()) {
        CompanySnapshot s = CompanySnapshot::from_json(QJsonDocument::fromJson(v.toString().toUtf8()).object());
        if (s.ok) {
            s.from_cache = true;
            return s;
        }
    }
    return CompanySnapshot{};
}

quint64 CompanySnapshotService::fetch(const QString& symbol_in, const Options& options, QObject* ctx, Callback cb) {
    const quint64 id = ++next_id_;
    Waiter w;
    w.id = id;
    w.ctx = ctx;
    w.has_ctx = ctx != nullptr;
    w.cb = std::move(cb);
    const QString symbol = symbol_in.trimmed().toUpper();

    // Answers always arrive from the event loop (never inside fetch()), so callers can rely on
    // the request id being stored before their callback runs.
    auto deliver_later = [this, w, id](CompanySnapshot s) {
        id_symbol_.insert(id, QString());
        QMetaObject::invokeMethod(this, [this, w, id, s = std::move(s)]() {
            if (!id_symbol_.contains(id))
                return; // cancelled
            id_symbol_.remove(id);
            deliver(w, s);
        }, Qt::QueuedConnection);
    };

    if (symbol.isEmpty()) {
        CompanySnapshot s;
        s.error = tr("Enter a ticker first.");
        deliver_later(s);
        return id;
    }
    if (!options.force) {
        CompanySnapshot hit = cached(symbol);
        if (hit.ok && (!options.financials || !hit.ltm_label.isEmpty() ||
                       hit.field_source.contains(QString::fromLatin1(CompanySnapshot::kRevenue)))) {
            deliver_later(hit);
            return id;
        }
    }

    id_symbol_.insert(id, symbol);
    auto it = jobs_.find(symbol);
    if (it != jobs_.end()) {
        it->waiters.append(w); // joins the load in flight
        return id;
    }

    connect_equity_once();
    Job job;
    job.serial = ++next_serial_;
    job.symbol = symbol;
    job.options = options;
    job.snap.symbol = symbol;
    job.waiters.append(w);
    job.financials_done = !options.financials;
    jobs_.insert(symbol, job);
    LOG_INFO("CompanySnapshot", QStringLiteral("Snapshot load started symbol=%1 financials=%2")
                                    .arg(symbol, options.financials ? QStringLiteral("yes") : QStringLiteral("no")));

    auto& eq = equity::EquityResearchService::instance();
    eq.load_info_only(symbol);
    if (options.financials)
        eq.fetch_financials(symbol);

    const quint64 serial = job.serial;
    QTimer::singleShot(std::max(5000, options.timeout_ms), this, [this, symbol, serial]() {
        const auto j = jobs_.constFind(symbol);
        if (j != jobs_.constEnd() && j->serial == serial)
            finish(symbol, /*timed_out=*/true);
    });
    return id;
}

void CompanySnapshotService::cancel(quint64 id) {
    const auto it = id_symbol_.find(id);
    if (it == id_symbol_.end())
        return;
    const QString symbol = it.value();
    id_symbol_.erase(it);
    auto job = jobs_.find(symbol);
    if (job == jobs_.end())
        return;
    for (int i = 0; i < job->waiters.size(); ++i)
        if (job->waiters.at(i).id == id) {
            job->waiters.removeAt(i);
            break;
        }
    // The loads themselves finish (EquityResearchService caches them for the next caller).
}

void CompanySnapshotService::connect_equity_once() {
    if (equity_connected_)
        return;
    equity_connected_ = true;
    auto& eq = equity::EquityResearchService::instance();

    connect(&eq, &equity::EquityResearchService::info_loaded, this, [this](const equity::StockInfo& info) {
        const QString symbol = info.symbol.trimmed().toUpper();
        auto it = jobs_.find(symbol);
        if (it == jobs_.end() || it->info_done)
            return;
        CompanySnapshot& s = it->snap;
        const QString y = QStringLiteral("yahoo");
        s.name = info.company_name == QLatin1String("N/A") ? QString() : info.company_name;
        s.currency = info.currency;
        s.financial_currency = info.financial_currency;
        s.exchange = info.exchange;
        s.sector = info.sector == QLatin1String("N/A") ? QString() : info.sector;
        s.industry = info.industry == QLatin1String("N/A") ? QString() : info.industry;
        snap_set(s, &CompanySnapshot::price, CompanySnapshot::kPrice, snap_pos(info.current_price), y);
        snap_set(s, &CompanySnapshot::week52_low, CompanySnapshot::kWeek52Low, snap_pos(info.week52_low), y);
        snap_set(s, &CompanySnapshot::week52_high, CompanySnapshot::kWeek52High, snap_pos(info.week52_high), y);
        snap_set(s, &CompanySnapshot::shares_outstanding, CompanySnapshot::kShares, snap_pos(info.shares_outstanding), y);
        snap_set(s, &CompanySnapshot::market_cap, CompanySnapshot::kMarketCap, snap_pos(info.market_cap), y);
        snap_set(s, &CompanySnapshot::enterprise_value, CompanySnapshot::kEnterpriseValue, snap_pos(info.enterprise_value), y);
        snap_set(s, &CompanySnapshot::total_debt, CompanySnapshot::kTotalDebt, snap_pos(info.total_debt), y);
        snap_set(s, &CompanySnapshot::cash, CompanySnapshot::kCash, snap_pos(info.total_cash), y);
        // A beta of exactly 0 is Yahoo's "not reported", not a riskless stock.
        snap_set(s, &CompanySnapshot::beta, CompanySnapshot::kBeta,
                 (std::isfinite(info.beta) && info.beta != 0.0) ? info.beta : CompanySnapshot::kNaN, y);
        // yfinance reports the dividend yield in percent (0.41 = 0.41 %).
        const double dy = snap_pos(info.dividend_yield);
        if (std::isfinite(dy)) {
            snap_set(s, &CompanySnapshot::dividend_yield, CompanySnapshot::kDividendYield, dy / 100.0, y);
            if (std::isfinite(s.price))
                snap_set(s, &CompanySnapshot::dividend_per_share, CompanySnapshot::kDividendPerShare,
                         s.price * dy / 100.0, QStringLiteral("yahoo (yield x price)"));
        }
        snap_set(s, &CompanySnapshot::book_value_per_share, CompanySnapshot::kBookValue, snap_pos(info.book_value), y);
        snap_set(s, &CompanySnapshot::price_to_book, CompanySnapshot::kPriceToBook, snap_pos(info.price_to_book), y);
        if (!s.financial_currency.isEmpty() && !s.currency.isEmpty() &&
            s.financial_currency.compare(s.currency, Qt::CaseInsensitive) != 0)
            s.notes << tr("The statements are in %1 while the price is in %2: compare per-share figures with care.")
                           .arg(s.financial_currency, s.currency);
        it->info_done = true;
        maybe_continue(symbol);
    });

    connect(&eq, &equity::EquityResearchService::financials_loaded, this, [this](const equity::FinancialsData& fd) {
        const QString symbol = fd.symbol.trimmed().toUpper();
        auto it = jobs_.find(symbol);
        if (it == jobs_.end() || it->financials_done)
            return;
        CompanySnapshot& s = it->snap;
        // Trailing twelve months when Yahoo has the column, else the latest fiscal year.
        QJsonObject inc;
        QJsonObject cf;
        if (!fd.ttm_income.isEmpty()) {
            inc = fd.ttm_income.first().second;
            s.ltm_label = tr("TTM to %1").arg(fd.ttm_income.first().first.left(10));
            if (!fd.ttm_cash_flow.isEmpty())
                cf = fd.ttm_cash_flow.first().second;
        } else if (!fd.income_statement.isEmpty()) {
            inc = fd.income_statement.first().second;
            s.ltm_label = tr("FY ending %1").arg(fd.income_statement.first().first.left(10));
            if (!fd.cash_flow.isEmpty())
                cf = fd.cash_flow.first().second;
        }
        const QString y = QStringLiteral("yahoo");
        snap_set(s, &CompanySnapshot::revenue_ltm, CompanySnapshot::kRevenue,
                 snap_item(inc, {"Total Revenue", "Operating Revenue"}), y);
        snap_set(s, &CompanySnapshot::ebitda_ltm, CompanySnapshot::kEbitda, snap_item(inc, {"EBITDA", "Normalized EBITDA"}), y);
        snap_set(s, &CompanySnapshot::ebit_ltm, CompanySnapshot::kEbit, snap_item(inc, {"EBIT", "Operating Income"}), y);
        snap_set(s, &CompanySnapshot::net_income_ltm, CompanySnapshot::kNetIncome,
                 snap_item(inc, {"Net Income", "Net Income Common Stockholders"}), y);
        snap_set(s, &CompanySnapshot::eps_ltm, CompanySnapshot::kEps, snap_item(inc, {"Diluted EPS"}), y);
        double da = snap_item(inc, {"Reconciled Depreciation"});
        if (!std::isfinite(da))
            da = snap_item(cf, {"Depreciation And Amortization", "Depreciation Amortization Depletion"});
        snap_set(s, &CompanySnapshot::depreciation_ltm, CompanySnapshot::kDepreciation, da, y);
        const double interest = snap_item(inc, {"Interest Expense", "Interest Expense Non Operating"});
        snap_set(s, &CompanySnapshot::interest_expense_ltm, CompanySnapshot::kInterestExpense,
                 std::isfinite(interest) ? std::fabs(interest) : interest, y);
        const double tax = snap_item(inc, {"Tax Provision"});
        const double pretax = snap_item(inc, {"Pretax Income"});
        if (std::isfinite(tax) && std::isfinite(pretax) && pretax > 0)
            snap_set(s, &CompanySnapshot::effective_tax_rate, CompanySnapshot::kTaxRate, tax / pretax,
                     QStringLiteral("yahoo (tax / pre-tax income)"));
        it->financials_done = true;
        maybe_continue(symbol);
    });

    connect(&eq, &equity::EquityResearchService::load_failed, this, [this](const equity::LoadFailure& f) {
        const QString symbol = f.symbol.trimmed().toUpper();
        auto it = jobs_.find(symbol);
        if (it == jobs_.end())
            return;
        if (f.leg == QLatin1String("Info") && !it->info_done) {
            it->info_done = true;
            it->info_error = f.message;
        } else if (f.leg == QLatin1String("Financials") && !it->financials_done) {
            it->financials_done = true;
            it->financials_error = f.message;
        } else {
            return;
        }
        maybe_continue(symbol);
    });
}

void CompanySnapshotService::maybe_continue(const QString& symbol) {
    auto it = jobs_.find(symbol);
    if (it == jobs_.end() || !it->info_done || !it->financials_done)
        return;
    if (it->sec_started && !it->sec_done)
        return;
    if (!it->sec_started && it->options.sec_fallback && equity::SecFilingsService::is_us_listing(symbol)) {
        // SEC values are USD: only for statements reported in USD (or not stated).
        const QString fc = it->snap.financial_currency.isEmpty() ? it->snap.currency : it->snap.financial_currency;
        const bool usd = fc.isEmpty() || fc.compare(QLatin1String("USD"), Qt::CaseInsensitive) == 0;
        const CompanySnapshot& s = it->snap;
        bool gaps = !std::isfinite(s.shares_outstanding) || !std::isfinite(s.total_debt) || !std::isfinite(s.cash);
        if (it->options.financials)
            gaps = gaps || !std::isfinite(s.revenue_ltm) || !std::isfinite(s.ebit_ltm) || !std::isfinite(s.net_income_ltm) ||
                   !std::isfinite(s.ebitda_ltm);
        if (usd && gaps) {
            start_sec(symbol);
            return;
        }
    }
    finish(symbol);
}

void CompanySnapshotService::start_sec(const QString& symbol) {
    auto it = jobs_.find(symbol);
    if (it == jobs_.end())
        return;
    it->sec_started = true;
    const quint64 serial = it->serial;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kSnapSecTimeoutMs;
    QPointer<CompanySnapshotService> self = this;
    python::PythonRunner::instance().run_with_options(
        kSnapSecScript, {QStringLiteral("snapshot"), symbol}, opts, [self, symbol, serial](python::PythonResult result) {
            if (!self)
                return;
            auto j = self->jobs_.find(symbol);
            if (j == self->jobs_.end() || j->serial != serial)
                return;
            j->sec_done = true;
            const QJsonObject obj = QJsonDocument::fromJson(python::extract_json(result.output).toUtf8()).object();
            if (!obj.value(QStringLiteral("success")).toBool()) {
                LOG_INFO("CompanySnapshot", QStringLiteral("SEC company facts not used symbol=%1 reason=%2")
                                                .arg(symbol, obj.value(QStringLiteral("code")).toString(QStringLiteral("failed"))));
                self->finish(symbol);
                return;
            }
            CompanySnapshot& s = j->snap;
            const QJsonObject data = obj.value(QStringLiteral("data")).toObject();
            const QJsonObject fields = data.value(QStringLiteral("fields")).toObject();
            int filled = 0;
            const bool needs_financials = j->options.financials;
            for (auto f = fields.constBegin(); f != fields.constEnd(); ++f) {
                const SnapField* sf = snap_field(f.key());
                if (!sf || (sf->financial && !needs_financials))
                    continue;
                if (std::isfinite(s.*(sf->member)))
                    continue; // Yahoo's figure stays; SEC only fills gaps
                const QJsonObject fo = f.value().toObject();
                const QJsonValue v = fo.value(QStringLiteral("value"));
                if (!v.isDouble())
                    continue;
                s.*(sf->member) = v.toDouble();
                s.field_source.insert(f.key(), QStringLiteral("sec"));
                ++filled;
            }
            if (s.ltm_label.isEmpty())
                s.ltm_label = data.value(QStringLiteral("ltm_label")).toString();
            if (s.name.isEmpty())
                s.name = data.value(QStringLiteral("name")).toString();
            if (filled > 0 && s.financial_currency.isEmpty())
                s.financial_currency = QStringLiteral("USD");
            LOG_INFO("CompanySnapshot", QStringLiteral("SEC company facts filled gaps symbol=%1 fields=%2").arg(symbol).arg(filled));
            self->finish(symbol);
        });
}

void CompanySnapshotService::finish(const QString& symbol, bool timed_out) {
    auto it = jobs_.find(symbol);
    if (it == jobs_.end())
        return;
    Job job = *it;
    jobs_.erase(it);
    CompanySnapshot& s = job.snap;
    s.as_of = QDateTime::currentDateTime();

    if (std::isfinite(s.total_debt) && std::isfinite(s.cash)) {
        s.net_debt = s.total_debt - s.cash;
        const QString a = s.field_source.value(QString::fromLatin1(CompanySnapshot::kTotalDebt));
        const QString b = s.field_source.value(QString::fromLatin1(CompanySnapshot::kCash));
        s.field_source.insert(QString::fromLatin1(CompanySnapshot::kNetDebt), a == b ? a : a + QStringLiteral(" + ") + b);
    }
    bool yahoo = false;
    bool sec = false;
    s.missing.clear();
    for (const SnapField& f : kSnapFields) {
        if (f.financial && !job.options.financials)
            continue;
        if (!std::isfinite(s.*(f.member)))
            s.missing << QString::fromLatin1(f.key);
    }
    for (auto fs = s.field_source.constBegin(); fs != s.field_source.constEnd(); ++fs) {
        yahoo = yahoo || fs.value().startsWith(QLatin1String("yahoo"));
        sec = sec || fs.value().contains(QLatin1String("sec"));
    }
    QStringList src;
    if (yahoo)
        src << QStringLiteral("Yahoo Finance");
    if (sec)
        src << QStringLiteral("SEC EDGAR company facts");
    s.source = src.join(QStringLiteral(" + "));
    s.ok = yahoo || sec;
    if (timed_out)
        s.notes << tr("Some data did not arrive within %1 s; what arrived is shown.").arg(job.options.timeout_ms / 1000);
    if (!s.ok) {
        if (!job.info_error.isEmpty() && job.info_error.contains(QLatin1String("no company information"), Qt::CaseInsensitive))
            s.error = tr("No financial data found for %1. Enter the figures by hand.").arg(symbol);
        else if (timed_out)
            s.error = tr("No reply from the market data source for %1. Enter the figures by hand or try again.").arg(symbol);
        else
            s.error = tr("No financial data found for %1. Enter the figures by hand.").arg(symbol);
    }
    LOG_INFO("CompanySnapshot", QStringLiteral("Snapshot ready symbol=%1 ok=%2 missing=%3 timed_out=%4")
                                    .arg(symbol, s.ok ? QStringLiteral("yes") : QStringLiteral("no"))
                                    .arg(s.missing.size())
                                    .arg(timed_out ? QStringLiteral("yes") : QStringLiteral("no")));
    if (s.ok) {
        memo_.insert(symbol, s);
        fincept::CacheManager::instance().put(snap_cache_key(symbol),
                                              QString::fromUtf8(QJsonDocument(s.to_json()).toJson(QJsonDocument::Compact)),
                                              kCacheTtlSec, QStringLiteral("company_snapshot"));
    }
    for (const Waiter& w : std::as_const(job.waiters)) {
        if (!id_symbol_.contains(w.id))
            continue; // cancelled
        id_symbol_.remove(w.id);
        deliver(w, s);
    }
}

} // namespace fincept::services
