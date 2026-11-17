#include "screens/watchlist/WatchlistModel.h"

#include "core/market/InstrumentCurrency.h"
#include "core/symbol/SymbolMime.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeData>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
namespace wcols = fincept::screens::watchlist_columns;

namespace {

constexpr double kWlmNaN = std::numeric_limits<double>::quiet_NaN();
constexpr qint64 kWlmFlashMs = 1200;      // tick flash fade-out
constexpr qint64 kWlmStaleMs = 150'000;   // quote not refreshed for 2.5 min (TTL 30 s)
constexpr qint64 kWlmSessionCacheMs = 10'000;

QString wlm_tr(const char* s) {
    return QCoreApplication::translate("WatchlistModel", s);
}

// Column identity, resolved once from the registry ids.
enum class WlCol {
    Symbol, Alert, Name, Last, Change, ChangePct, DayRange, Spark, Volume, RelVolume, Perf1M, PerfYtd, Range52w,
    State, Open, PrevClose, DayHigh, DayLow, ExtPrice, ExtChangePct, DisplayPrice, Currency, AvgVolume, Perf1W,
    Perf3M, Perf6M, Perf1Y, FromHigh52w, FromLow52w, Vol20d, VsMa50, VsMa200, MarketCap, Pe, DivYield, Beta, Sector,
    Industry, Country, Exchange, Earnings, ExDiv, Group, Notes, Unknown
};

const QVector<WlCol>& wlm_col_map() {
    static const QVector<WlCol> kMap = [] {
        const QHash<QString, WlCol> by_id = {
            {"symbol", WlCol::Symbol},         {"alert", WlCol::Alert},
            {"name", WlCol::Name},             {"last", WlCol::Last},
            {"change", WlCol::Change},         {"change_pct", WlCol::ChangePct},
            {"day_range", WlCol::DayRange},    {"spark", WlCol::Spark},
            {"volume", WlCol::Volume},         {"rel_volume", WlCol::RelVolume},
            {"perf_1m", WlCol::Perf1M},        {"perf_ytd", WlCol::PerfYtd},
            {"range_52w", WlCol::Range52w},    {"state", WlCol::State},
            {"open", WlCol::Open},             {"prev_close", WlCol::PrevClose},
            {"day_high", WlCol::DayHigh},      {"day_low", WlCol::DayLow},
            {"ext_price", WlCol::ExtPrice},    {"ext_change_pct", WlCol::ExtChangePct},
            {"display_price", WlCol::DisplayPrice}, {"currency", WlCol::Currency},
            {"avg_volume", WlCol::AvgVolume},  {"perf_1w", WlCol::Perf1W},
            {"perf_3m", WlCol::Perf3M},        {"perf_6m", WlCol::Perf6M},
            {"perf_1y", WlCol::Perf1Y},        {"from_high_52w", WlCol::FromHigh52w},
            {"from_low_52w", WlCol::FromLow52w}, {"vol_20d", WlCol::Vol20d},
            {"vs_ma50", WlCol::VsMa50},        {"vs_ma200", WlCol::VsMa200},
            {"market_cap", WlCol::MarketCap},  {"pe", WlCol::Pe},
            {"div_yield", WlCol::DivYield},    {"beta", WlCol::Beta},
            {"sector", WlCol::Sector},         {"industry", WlCol::Industry},
            {"country", WlCol::Country},       {"exchange", WlCol::Exchange},
            {"earnings", WlCol::Earnings},     {"ex_div", WlCol::ExDiv},
            {"group", WlCol::Group},           {"notes", WlCol::Notes},
        };
        QVector<WlCol> m;
        for (const auto& d : wcols::all())
            m.append(by_id.value(QString::fromLatin1(d.id), WlCol::Unknown));
        return m;
    }();
    return kMap;
}

WlCol wlm_col(int column) {
    const auto& m = wlm_col_map();
    return column >= 0 && column < m.size() ? m[column] : WlCol::Unknown;
}

int wlm_col_index(WlCol c) {
    const auto& m = wlm_col_map();
    return static_cast<int>(m.indexOf(c));
}

bool wlm_finite(double v) {
    return std::isfinite(v);
}

bool wlm_is_perf(WlCol c) {
    return c == WlCol::Perf1W || c == WlCol::Perf1M || c == WlCol::Perf3M || c == WlCol::Perf6M ||
           c == WlCol::PerfYtd || c == WlCol::Perf1Y || c == WlCol::Vol20d;
}

bool wlm_is_flash(WlCol c) {
    return c == WlCol::Last || c == WlCol::Change || c == WlCol::ChangePct || c == WlCol::DisplayPrice;
}

/// Columns whose group-row cell shows the members' average.
bool wlm_is_averaged(WlCol c) {
    switch (c) {
        case WlCol::ChangePct:
        case WlCol::ExtChangePct:
        case WlCol::RelVolume:
        case WlCol::Perf1W:
        case WlCol::Perf1M:
        case WlCol::Perf3M:
        case WlCol::Perf6M:
        case WlCol::PerfYtd:
        case WlCol::Perf1Y:
        case WlCol::FromHigh52w:
        case WlCol::FromLow52w:
        case WlCol::Vol20d:
        case WlCol::VsMa50:
        case WlCol::VsMa200:
            return true;
        default:
            return false;
    }
}

/// Signed percent columns painted in the direction colours.
bool wlm_is_direction(WlCol c) {
    return c == WlCol::Change || c == WlCol::ChangePct || c == WlCol::ExtChangePct || c == WlCol::VsMa50 ||
           c == WlCol::VsMa200;
}

fmt::InstrumentFmt wlm_inst(const WatchlistRowState& r) {
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(r.stock.symbol, r.currency());
    if (r.has_quote && r.quote.price_digits >= 0)
        inst.decimals = r.quote.price_digits;
    return inst;
}

/// Show the currency symbol with prices: shares, funds, crypto, futures — not
/// FX rates (the pair says it) or index levels (points, not money).
bool wlm_show_ccy_symbol(const WatchlistRowState& r, const fmt::InstrumentFmt& inst) {
    if (inst.fx || inst.currency.isEmpty())
        return false;
    const QString ac = r.asset_class();
    return ac == QLatin1String("equity") || ac == QLatin1String("crypto") || ac == QLatin1String("future");
}

QString wlm_duration(qint64 secs) {
    if (secs < 0)
        secs = 0;
    const qint64 d = secs / 86400;
    const qint64 h = (secs % 86400) / 3600;
    const qint64 m = (secs % 3600) / 60;
    if (d > 0)
        return wlm_tr("%1d %2h").arg(d).arg(h);
    if (h > 0)
        return wlm_tr("%1h %2m").arg(h).arg(m, 2, 10, QLatin1Char('0'));
    return wlm_tr("%1m").arg(std::max<qint64>(1, m));
}

int wlm_state_rank(core::market::SessionState s) {
    using S = core::market::SessionState;
    switch (s) {
        case S::Open:
            return 0;
        case S::Pre:
        case S::Post:
            return 1;
        case S::Lunch:
            return 2;
        case S::Closed:
            return 3;
        case S::Holiday:
            return 4;
    }
    return 5;
}

QString wlm_yahoo_state_label(const QString& s) {
    if (s == QLatin1String("REGULAR"))
        return wlm_tr("Open");
    if (s == QLatin1String("PRE"))
        return wlm_tr("Pre-market");
    if (s == QLatin1String("POST"))
        return wlm_tr("After hours");
    if (s == QLatin1String("CLOSED"))
        return wlm_tr("Closed");
    return {};
}

/// Extended-hours price / change by session (Yahoo keeps the last print around).
double wlm_ext_price(const services::QuoteData& q) {
    if (q.market_state == QLatin1String("PRE"))
        return q.pre_market_price;
    if (q.market_state == QLatin1String("POST") || q.market_state == QLatin1String("CLOSED"))
        return q.post_market_price;
    return kWlmNaN;
}

double wlm_ext_change_pct(const services::QuoteData& q) {
    if (q.market_state == QLatin1String("PRE"))
        return q.pre_market_change_pct;
    if (q.market_state == QLatin1String("POST") || q.market_state == QLatin1String("CLOSED"))
        return q.post_market_change_pct;
    return kWlmNaN;
}

QString wlm_asset_group_key(const WatchlistRowState& r) {
    if (r.has_quote) {
        if (r.quote.quote_type == QLatin1String("ETF"))
            return QStringLiteral("etf");
        if (r.quote.quote_type == QLatin1String("MUTUALFUND"))
            return QStringLiteral("fund");
    }
    return r.asset_class();
}

QString wlm_asset_group_label(const QString& key) {
    if (key == QLatin1String("equity"))
        return wlm_tr("Equities");
    if (key == QLatin1String("etf"))
        return wlm_tr("ETFs");
    if (key == QLatin1String("fund"))
        return wlm_tr("Funds");
    if (key == QLatin1String("index"))
        return wlm_tr("Indices");
    if (key == QLatin1String("fx"))
        return wlm_tr("Currencies");
    if (key == QLatin1String("crypto"))
        return wlm_tr("Crypto");
    if (key == QLatin1String("future"))
        return wlm_tr("Futures");
    return key;
}

QString wlm_c_number(double v, int max_decimals) {
    // Full precision, C locale, trailing zeros trimmed — re-importable, sums in a spreadsheet.
    if (!wlm_finite(v))
        return {};
    QString s = QString::number(v, 'f', max_decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

} // namespace

// ── Free helpers ─────────────────────────────────────────────────────────────

QString watchlist_grouping_key(WatchlistGrouping g) {
    switch (g) {
        case WatchlistGrouping::None:
            return QStringLiteral("none");
        case WatchlistGrouping::Sector:
            return QStringLiteral("sector");
        case WatchlistGrouping::Industry:
            return QStringLiteral("industry");
        case WatchlistGrouping::Exchange:
            return QStringLiteral("exchange");
        case WatchlistGrouping::Currency:
            return QStringLiteral("currency");
        case WatchlistGrouping::AssetClass:
            return QStringLiteral("asset_class");
        case WatchlistGrouping::Country:
            return QStringLiteral("country");
        case WatchlistGrouping::Custom:
            return QStringLiteral("custom");
    }
    return QStringLiteral("none");
}

WatchlistGrouping watchlist_grouping_from_key(const QString& key) {
    for (WatchlistGrouping g : watchlist_groupings()) {
        if (watchlist_grouping_key(g) == key)
            return g;
    }
    return WatchlistGrouping::None;
}

QString watchlist_grouping_label(WatchlistGrouping g) {
    switch (g) {
        case WatchlistGrouping::None:
            return wlm_tr("No grouping");
        case WatchlistGrouping::Sector:
            return wlm_tr("Sector");
        case WatchlistGrouping::Industry:
            return wlm_tr("Industry");
        case WatchlistGrouping::Exchange:
            return wlm_tr("Exchange");
        case WatchlistGrouping::Currency:
            return wlm_tr("Currency");
        case WatchlistGrouping::AssetClass:
            return wlm_tr("Asset class");
        case WatchlistGrouping::Country:
            return wlm_tr("Country");
        case WatchlistGrouping::Custom:
            return wlm_tr("My groups");
    }
    return {};
}

QVector<WatchlistGrouping> watchlist_groupings() {
    return {WatchlistGrouping::None,     WatchlistGrouping::Sector,     WatchlistGrouping::Industry,
            WatchlistGrouping::Exchange, WatchlistGrouping::Currency,   WatchlistGrouping::AssetClass,
            WatchlistGrouping::Country,  WatchlistGrouping::Custom};
}

QString watchlist_asset_class(const QString& symbol, const QString& quote_type) {
    const QString qt = quote_type.toUpper();
    if (qt == QLatin1String("CURRENCY"))
        return QStringLiteral("fx");
    if (qt == QLatin1String("CRYPTOCURRENCY"))
        return QStringLiteral("crypto");
    if (qt == QLatin1String("INDEX"))
        return QStringLiteral("index");
    if (qt == QLatin1String("FUTURE"))
        return QStringLiteral("future");
    if (qt == QLatin1String("EQUITY") || qt == QLatin1String("ETF") || qt == QLatin1String("MUTUALFUND"))
        return QStringLiteral("equity");

    const QString s = symbol.toUpper();
    if (s.startsWith(QLatin1Char('^')))
        return QStringLiteral("index");
    if (s.endsWith(QLatin1String("=X")))
        return QStringLiteral("fx");
    if (s.endsWith(QLatin1String("=F")))
        return QStringLiteral("future");
    const int dash = static_cast<int>(s.lastIndexOf(QLatin1Char('-')));
    if (dash > 0) {
        // BTC-USD, ETH-EUR, SOL-USDT — but not class shares (BRK-B, BF-B). A parse list of the
        // quote assets crypto pairs are written against, not a currency the user sees.
        static const QStringList kQuoteCcys = {"USD", "USDT", "USDC", "EUR", "GBP", "JPY", "BTC", "ETH", "INR",
                                               "AUD", "CAD"};
        if (kQuoteCcys.contains(s.mid(dash + 1)))
            return QStringLiteral("crypto");
    }
    return QStringLiteral("equity");
}

// ── WatchlistRowState ───────────────────────────────────────────────────────

QString WatchlistRowState::display_name() const {
    if (!resolved_name.isEmpty())
        return resolved_name;
    if (!stock.name.isEmpty())
        return stock.name;
    if (has_info && !info.name.isEmpty())
        return info.name;
    if (has_quote && !quote.name.isEmpty() && quote.name != stock.symbol)
        return quote.name;
    return {};
}

QString WatchlistRowState::currency() const {
    if (has_quote && !quote.currency.isEmpty())
        return quote.currency;
    if (has_info && !info.currency.isEmpty())
        return info.currency;
    return core::market::currency_for_symbol(stock.symbol, exchange_hint());
}

QString WatchlistRowState::asset_class() const {
    return watchlist_asset_class(stock.symbol, has_quote ? quote.quote_type : QString());
}

QString WatchlistRowState::exchange_hint() const {
    if (has_quote && !quote.exchange.isEmpty())
        return quote.exchange;
    return stock.exchange;
}

SymbolRef WatchlistRowState::symbol_ref() const {
    SymbolRef ref;
    ref.symbol = stock.symbol;
    ref.asset_class = asset_class();
    return ref;
}

// ── WatchlistModel ──────────────────────────────────────────────────────────

WatchlistModel::WatchlistModel(QObject* parent) : QAbstractItemModel(parent) {
    display_ccy_ = fmt::FormatSettings::instance().display_currency();
    regroup_timer_ = new QTimer(this);
    regroup_timer_->setSingleShot(true);
    regroup_timer_->setInterval(250);
    connect(regroup_timer_, &QTimer::timeout, this, &WatchlistModel::apply_regroup);

    flash_timer_ = new QTimer(this);
    flash_timer_->setInterval(60);
    connect(flash_timer_, &QTimer::timeout, this, &WatchlistModel::on_flash_tick);
}

WatchlistModel::~WatchlistModel() = default;

void WatchlistModel::set_stocks(const QString& list_id, const QVector<WatchlistStock>& stocks) {
    beginResetModel();
    QHash<QString, WatchlistRowState> old;
    for (auto& r : rows_)
        old.insert(r.stock.symbol, std::move(r));
    rows_.clear();
    rows_.reserve(stocks.size());
    row_by_symbol_.clear();
    for (const WatchlistStock& s : stocks) {
        if (row_by_symbol_.contains(s.symbol))
            continue; // the table has UNIQUE(watchlist_id, symbol); guard anyway
        WatchlistRowState st = old.take(s.symbol);
        st.stock = s;
        st.flash_ms = 0;
        st.resolved_name = names_.value(s.symbol, st.resolved_name);
        row_by_symbol_.insert(s.symbol, static_cast<int>(rows_.size()));
        rows_.append(std::move(st));
    }
    list_id_ = list_id;
    flashing_.clear();
    session_cache_.clear();
    rebuild_groups();
    endResetModel();
}

void WatchlistModel::set_grouping(WatchlistGrouping g) {
    if (g == grouping_)
        return;
    beginResetModel();
    grouping_ = g;
    rebuild_groups();
    endResetModel();
    emit groups_rebuilt();
}

QString WatchlistModel::group_key_of(const WatchlistRowState& r) const {
    switch (grouping_) {
        case WatchlistGrouping::None:
            return {};
        case WatchlistGrouping::Sector:
            if (r.has_info && !r.info.sector.isEmpty())
                return r.info.sector;
            // Indices, FX, crypto, ETFs have no sector: group them by kind instead of "Unclassified".
            if (wlm_asset_group_key(r) != QLatin1String("equity"))
                return QLatin1Char('#') + wlm_asset_group_key(r);
            return {};
        case WatchlistGrouping::Industry:
            return r.has_info ? r.info.industry : QString();
        case WatchlistGrouping::Exchange: {
            if (r.has_quote && !r.quote.exchange_name.isEmpty())
                return r.quote.exchange_name;
            if (const auto* v = core::market::venue_for_symbol(r.stock.symbol, r.exchange_hint()))
                return core::market::venue_display_name(*v);
            return r.stock.exchange;
        }
        case WatchlistGrouping::Currency:
            return core::market::major_currency(r.currency());
        case WatchlistGrouping::AssetClass:
            return wlm_asset_group_key(r);
        case WatchlistGrouping::Country:
            return r.has_info ? r.info.country : QString();
        case WatchlistGrouping::Custom:
            return r.stock.group_name.trimmed();
    }
    return {};
}

QString WatchlistModel::group_label(const QString& key) const {
    if (key.isEmpty())
        return grouping_ == WatchlistGrouping::Custom ? tr("Ungrouped") : tr("Unclassified");
    if (grouping_ == WatchlistGrouping::AssetClass)
        return wlm_asset_group_label(key);
    if (key.startsWith(QLatin1Char('#')))
        return wlm_asset_group_label(key.mid(1));
    if (grouping_ == WatchlistGrouping::Currency)
        return QStringLiteral("%1  %2").arg(key, fmt::currency_name(key));
    return key;
}

void WatchlistModel::rebuild_groups() {
    groups_.clear();
    row_group_.fill(-1, rows_.size());
    row_pos_.fill(-1, rows_.size());
    if (grouping_ == WatchlistGrouping::None) {
        for (int i = 0; i < rows_.size(); ++i)
            row_pos_[i] = i;
        return;
    }
    QHash<QString, int> by_key;
    for (int i = 0; i < rows_.size(); ++i) {
        const QString key = group_key_of(rows_[i]);
        int g = by_key.value(key, -1);
        if (g < 0) {
            g = static_cast<int>(groups_.size());
            by_key.insert(key, g);
            groups_.append(Group{key, group_label(key), {}});
        }
        groups_[g].members.append(i);
    }
    // Custom groups keep the order the user's rows give them; data-derived
    // groups are alphabetical. "Ungrouped" / "Unclassified" always last.
    if (grouping_ != WatchlistGrouping::Custom) {
        std::stable_sort(groups_.begin(), groups_.end(), [](const Group& a, const Group& b) {
            if (a.key.isEmpty() != b.key.isEmpty())
                return b.key.isEmpty();
            return QString::localeAwareCompare(a.label, b.label) < 0;
        });
    } else {
        std::stable_sort(groups_.begin(), groups_.end(),
                         [](const Group& a, const Group& b) { return !a.key.isEmpty() && b.key.isEmpty(); });
    }
    for (int g = 0; g < groups_.size(); ++g) {
        const auto& m = groups_[g].members;
        for (int p = 0; p < m.size(); ++p) {
            row_group_[m[p]] = g;
            row_pos_[m[p]] = p;
        }
    }
}

void WatchlistModel::schedule_regroup() {
    if (!regroup_timer_->isActive())
        regroup_timer_->start();
}

void WatchlistModel::apply_regroup() {
    if (!grouped())
        return;
    beginResetModel();
    rebuild_groups();
    endResetModel();
    emit groups_rebuilt();
}

void WatchlistModel::check_group_key(int row) {
    if (!grouped() || row < 0 || row >= rows_.size())
        return;
    const int g = row_group_.value(row, -1);
    if (g < 0 || g >= groups_.size() || groups_[g].key != group_key_of(rows_[row]))
        schedule_regroup();
}

int WatchlistModel::row_index(const QModelIndex& idx) const {
    if (!idx.isValid() || idx.model() != this)
        return -1;
    if (!grouped())
        return idx.internalId() == 0 && idx.row() < rows_.size() ? idx.row() : -1;
    const quintptr id = idx.internalId();
    if (id == 0)
        return -1;
    const int g = static_cast<int>(id - 1);
    if (g >= groups_.size())
        return -1;
    return groups_[g].members.value(idx.row(), -1);
}

QModelIndex WatchlistModel::model_index(int row, int column) const {
    if (row < 0 || row >= rows_.size())
        return {};
    if (!grouped())
        return createIndex(row, column, quintptr(0));
    const int g = row_group_.value(row, -1);
    if (g < 0)
        return {};
    return createIndex(row_pos_[row], column, quintptr(g + 1));
}

void WatchlistModel::emit_row_changed(int row) {
    const QModelIndex a = model_index(row, 0);
    if (!a.isValid())
        return;
    emit dataChanged(a, model_index(row, columnCount() - 1));
    if (grouped()) {
        const int g = row_group_.value(row, -1);
        if (g >= 0)
            emit dataChanged(createIndex(g, 0, quintptr(0)), createIndex(g, columnCount() - 1, quintptr(0)));
    }
}

void WatchlistModel::emit_columns_changed(const QVector<int>& columns, const QList<int>& roles) {
    for (int c : columns) {
        if (c < 0)
            continue;
        if (!grouped()) {
            if (!rows_.isEmpty())
                emit dataChanged(createIndex(0, c, quintptr(0)), createIndex(static_cast<int>(rows_.size()) - 1, c,
                                                                              quintptr(0)),
                                 roles);
            continue;
        }
        if (!groups_.isEmpty())
            emit dataChanged(createIndex(0, c, quintptr(0)),
                             createIndex(static_cast<int>(groups_.size()) - 1, c, quintptr(0)), roles);
        for (int g = 0; g < groups_.size(); ++g) {
            const int n = static_cast<int>(groups_[g].members.size());
            if (n > 0)
                emit dataChanged(createIndex(0, c, quintptr(g + 1)), createIndex(n - 1, c, quintptr(g + 1)), roles);
        }
    }
}

// ── Data feeds ──────────────────────────────────────────────────────────────

void WatchlistModel::set_quote(const QString& sym, const services::QuoteData& q, qint64 received_ms) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    WatchlistRowState& r = rows_[i];
    const double prev = r.has_quote ? r.quote.price : kWlmNaN;
    r.quote = q;
    r.has_quote = true;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    r.quote_ms = received_ms > 0 && received_ms <= now ? received_ms : now;
    r.error.clear();
    if (wlm_finite(prev) && prev > 0 && q.price > 0 && q.price != prev) {
        r.flash_ms = now;
        r.flash_dir = q.price > prev ? 1 : -1;
        flashing_.insert(i);
        if (!flash_timer_->isActive())
            flash_timer_->start();
    }
    session_cache_.remove(sym); // the exchange hint may have arrived with this quote
    emit_row_changed(i);
    check_group_key(i);
    emit row_data_changed(sym);
}

void WatchlistModel::set_error(const QString& sym, const QString& error) {
    const int i = row_of(sym);
    if (i < 0 || rows_[i].error == error)
        return;
    rows_[i].error = error;
    emit_row_changed(i);
    emit row_data_changed(sym);
}

void WatchlistModel::set_info(const QString& sym, const services::InfoData& info) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    rows_[i].info = info;
    rows_[i].has_info = true;
    emit_row_changed(i);
    check_group_key(i);
    emit row_data_changed(sym);
}

void WatchlistModel::set_history_state(const QString& sym, WatchlistHistoryState st) {
    const int i = row_of(sym);
    if (i < 0 || rows_[i].history == st)
        return;
    rows_[i].history = st;
    emit_row_changed(i);
}

void WatchlistModel::set_perf(const QString& sym, const services::PerformanceStats& perf) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    rows_[i].perf = perf;
    rows_[i].history = WatchlistHistoryState::Ready;
    emit_row_changed(i);
    emit row_data_changed(sym);
}

void WatchlistModel::set_spark(const QString& sym, const QVector<double>& values) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    rows_[i].spark = values;
    const int c = wlm_col_index(WlCol::Spark);
    const QModelIndex idx = model_index(i, c);
    if (idx.isValid())
        emit dataChanged(idx, idx);
}

void WatchlistModel::set_names(const QHash<QString, QString>& names) {
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        names_.insert(it.key(), it.value());
        const int i = row_of(it.key());
        if (i < 0 || rows_[i].resolved_name == it.value())
            continue;
        rows_[i].resolved_name = it.value();
        const int c = wlm_col_index(WlCol::Name);
        const QModelIndex idx = model_index(i, c);
        if (idx.isValid())
            emit dataChanged(idx, idx);
    }
}

void WatchlistModel::set_stock_fields(const QString& sym, const QString& notes, const QString& group) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    rows_[i].stock.notes = notes;
    rows_[i].stock.group_name = group;
    emit_row_changed(i);
    check_group_key(i);
}

void WatchlistModel::set_alert_summary(const QString& sym, int active, int total, bool fired_today,
                                       const QString& tip) {
    const int i = row_of(sym);
    if (i < 0)
        return;
    WatchlistRowState& r = rows_[i];
    if (r.alerts_active == active && r.alerts_total == total && r.alert_fired_today == fired_today &&
        r.alerts_tip == tip)
        return;
    r.alerts_active = active;
    r.alerts_total = total;
    r.alert_fired_today = fired_today;
    r.alerts_tip = tip;
    const QModelIndex idx = model_index(i, wlm_col_index(WlCol::Alert));
    if (idx.isValid())
        emit dataChanged(idx, idx);
}

void WatchlistModel::clear_alert_summaries() {
    for (auto& r : rows_) {
        r.alerts_active = 0;
        r.alerts_total = 0;
        r.alert_fired_today = false;
        r.alerts_tip.clear();
    }
    emit_columns_changed({wlm_col_index(WlCol::Alert)});
}

void WatchlistModel::set_display_currency(const QString& ccy) {
    if (ccy.isEmpty() || ccy == display_ccy_)
        return;
    display_ccy_ = ccy;
    emit_columns_changed({wlm_col_index(WlCol::DisplayPrice), wlm_col_index(WlCol::MarketCap)});
    emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}

void WatchlistModel::refresh_time_cells() {
    session_cache_.clear();
    // MarketCap: its sort key is converted at the current FX rate.
    emit_columns_changed({wlm_col_index(WlCol::State), wlm_col_index(WlCol::Last),
                          wlm_col_index(WlCol::DisplayPrice), wlm_col_index(WlCol::Symbol),
                          wlm_col_index(WlCol::MarketCap)});
}

void WatchlistModel::refresh_all() {
    session_cache_.clear();
    QVector<int> all;
    for (int c = 0; c < columnCount(); ++c)
        all.append(c);
    emit_columns_changed(all);
    emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
}

void WatchlistModel::on_flash_tick() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QList<int> rows(flashing_.cbegin(), flashing_.cend());
    static const QList<int> kRoles = {FlashRole};
    for (int i : rows) {
        if (i < 0 || i >= rows_.size()) {
            flashing_.remove(i);
            continue;
        }
        WatchlistRowState& r = rows_[i];
        if (r.flash_ms == 0 || now - r.flash_ms >= kWlmFlashMs) {
            r.flash_ms = 0;
            flashing_.remove(i);
        }
        for (WlCol c : {WlCol::Last, WlCol::Change, WlCol::ChangePct, WlCol::DisplayPrice}) {
            const QModelIndex idx = model_index(i, wlm_col_index(c));
            if (idx.isValid())
                emit dataChanged(idx, idx, kRoles);
        }
    }
    if (flashing_.isEmpty())
        flash_timer_->stop();
}

double WatchlistModel::flash_strength(const WatchlistRowState& r) const {
    if (r.flash_ms == 0 || r.flash_dir == 0)
        return 0.0;
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - r.flash_ms;
    if (age < 0 || age >= kWlmFlashMs)
        return 0.0;
    return r.flash_dir * (1.0 - static_cast<double>(age) / static_cast<double>(kWlmFlashMs));
}

// ── Lookup ──────────────────────────────────────────────────────────────────

const WatchlistRowState* WatchlistModel::state(const QString& sym) const {
    const int i = row_of(sym);
    return i < 0 ? nullptr : &rows_[i];
}

QStringList WatchlistModel::symbols() const {
    QStringList out;
    out.reserve(rows_.size());
    for (const auto& r : rows_)
        out.append(r.stock.symbol);
    return out;
}

bool WatchlistModel::is_group(const QModelIndex& idx) const {
    return idx.isValid() && idx.model() == this && grouped() && idx.internalId() == 0;
}

const WatchlistRowState* WatchlistModel::state_at(const QModelIndex& idx) const {
    const int i = row_index(idx);
    return i < 0 ? nullptr : &rows_[i];
}

QString WatchlistModel::symbol_at(const QModelIndex& idx) const {
    const auto* r = state_at(idx);
    return r ? r->stock.symbol : QString();
}

QModelIndex WatchlistModel::index_of_symbol(const QString& sym, int column) const {
    return model_index(row_of(sym), column);
}

QModelIndex WatchlistModel::group_index(const QString& key) const {
    if (!grouped())
        return {};
    for (int g = 0; g < groups_.size(); ++g) {
        if (groups_[g].key == key)
            return createIndex(g, 0, quintptr(0));
    }
    return {};
}

QStringList WatchlistModel::group_keys() const {
    QStringList out;
    for (const auto& g : groups_)
        out << g.key;
    return out;
}

QStringList WatchlistModel::group_members(const QString& key) const {
    QStringList out;
    for (const auto& g : groups_) {
        if (g.key != key)
            continue;
        for (int i : g.members)
            out << rows_[i].stock.symbol;
    }
    return out;
}

core::market::SessionStatus WatchlistModel::session(const WatchlistRowState& r,
                                                    const core::market::Venue** venue) const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = session_cache_.find(r.stock.symbol);
    if (it == session_cache_.end() || now - it->at_ms > kWlmSessionCacheMs) {
        SessionCache c;
        c.at_ms = now;
        c.venue = core::market::venue_for_symbol(r.stock.symbol, r.exchange_hint());
        if (c.venue)
            c.status = core::market::session_status(*c.venue, QDateTime::currentDateTimeUtc());
        it = session_cache_.insert(r.stock.symbol, c);
    }
    if (venue)
        *venue = it->venue;
    return it->status;
}

bool WatchlistModel::is_stale(const WatchlistRowState& r) const {
    if (!r.has_quote)
        return false;
    if (!r.error.isEmpty())
        return true;
    if (QDateTime::currentMSecsSinceEpoch() - r.quote_ms <= kWlmStaleMs)
        return false;
    // Old, but a closed market's quote stays current until its venue reopens (the hub keeps
    // it for the session instead of re-publishing it every 30 s).
    return !services::MarketDataService::quote_is_current(r.stock.symbol);
}

// ── Values ──────────────────────────────────────────────────────────────────

double WatchlistModel::value(const WatchlistRowState& r, int column) const {
    const WlCol c = wlm_col(column);
    const services::QuoteData& q = r.quote;
    const bool quote_ok = r.has_quote && q.price > 0;
    const auto pos = [](double v) { return wlm_finite(v) && v > 0 ? v : kWlmNaN; };
    const auto info_num = [&r](double v) { return r.has_info && wlm_finite(v) ? v : kWlmNaN; };
    switch (c) {
        case WlCol::Last:
            return quote_ok ? q.price : kWlmNaN;
        case WlCol::Change:
            return quote_ok ? q.change : kWlmNaN;
        case WlCol::ChangePct:
            return quote_ok ? q.change_pct : kWlmNaN;
        case WlCol::Open:
            return r.has_quote ? pos(q.open) : kWlmNaN;
        case WlCol::PrevClose:
            return r.has_quote ? pos(q.previous_close) : kWlmNaN;
        case WlCol::DayHigh:
            return r.has_quote ? pos(q.high) : kWlmNaN;
        case WlCol::DayLow:
            return r.has_quote ? pos(q.low) : kWlmNaN;
        case WlCol::DayRange:
            if (quote_ok && q.high > q.low && q.low > 0)
                return (q.price - q.low) / (q.high - q.low) * 100.0;
            return kWlmNaN;
        case WlCol::Range52w:
            if (quote_ok && wlm_finite(q.fifty_two_week_high) && wlm_finite(q.fifty_two_week_low) &&
                q.fifty_two_week_high > q.fifty_two_week_low)
                return (q.price - q.fifty_two_week_low) / (q.fifty_two_week_high - q.fifty_two_week_low) * 100.0;
            return kWlmNaN;
        case WlCol::ExtPrice:
            return r.has_quote ? pos(wlm_ext_price(q)) : kWlmNaN;
        case WlCol::ExtChangePct:
            return r.has_quote && wlm_finite(wlm_ext_price(q)) ? wlm_ext_change_pct(q) : kWlmNaN;
        case WlCol::DisplayPrice: {
            if (!quote_ok)
                return kWlmNaN;
            const QString ccy = r.currency();
            if (ccy.isEmpty())
                return kWlmNaN;
            const auto v = services::FxRateService::instance().convert(q.price, ccy, display_ccy_);
            return v ? *v : kWlmNaN;
        }
        case WlCol::Volume:
            return r.has_quote && q.volume > 0 ? q.volume : kWlmNaN;
        case WlCol::AvgVolume:
            return r.has_quote ? pos(q.average_volume) : kWlmNaN;
        case WlCol::RelVolume:
            if (r.has_quote && q.volume > 0 && wlm_finite(q.average_volume) && q.average_volume > 0)
                return q.volume / q.average_volume;
            return kWlmNaN;
        case WlCol::Spark:
            if (r.spark.size() >= 2 && wlm_finite(r.spark.first()) && r.spark.first() != 0 &&
                wlm_finite(r.spark.last()))
                return (r.spark.last() / r.spark.first() - 1.0) * 100.0;
            return kWlmNaN;
        case WlCol::Perf1W:
            return r.perf.change_1w_pct;
        case WlCol::Perf1M:
            return r.perf.change_1m_pct;
        case WlCol::Perf3M:
            return r.perf.change_3m_pct;
        case WlCol::Perf6M:
            return r.perf.change_6m_pct;
        case WlCol::PerfYtd:
            return r.perf.change_ytd_pct;
        case WlCol::Perf1Y:
            return r.perf.change_1y_pct;
        case WlCol::Vol20d:
            return r.perf.volatility_20d_pct;
        case WlCol::FromHigh52w:
            if (quote_ok && wlm_finite(q.fifty_two_week_high) && q.fifty_two_week_high > 0)
                return std::min(0.0, (q.price / q.fifty_two_week_high - 1.0) * 100.0);
            return r.perf.from_52w_high_pct;
        case WlCol::FromLow52w:
            if (quote_ok && wlm_finite(q.fifty_two_week_low) && q.fifty_two_week_low > 0)
                return std::max(0.0, (q.price / q.fifty_two_week_low - 1.0) * 100.0);
            return r.perf.from_52w_low_pct;
        case WlCol::VsMa50:
            if (quote_ok && wlm_finite(q.fifty_day_average) && q.fifty_day_average > 0)
                return (q.price / q.fifty_day_average - 1.0) * 100.0;
            return kWlmNaN;
        case WlCol::VsMa200:
            if (quote_ok && wlm_finite(q.two_hundred_day_average) && q.two_hundred_day_average > 0)
                return (q.price / q.two_hundred_day_average - 1.0) * 100.0;
            return kWlmNaN;
        case WlCol::MarketCap:
            return r.has_info ? pos(r.info.market_cap) : kWlmNaN;
        case WlCol::Pe:
            return info_num(r.info.pe_ratio);
        case WlCol::DivYield:
            return r.has_info && wlm_finite(r.info.dividend_yield) ? r.info.dividend_yield * 100.0 : kWlmNaN;
        case WlCol::Beta:
            return info_num(r.info.beta);
        case WlCol::Earnings:
            return r.has_info && r.info.earnings_date.isValid() ? static_cast<double>(r.info.earnings_date.toJulianDay())
                                                                : kWlmNaN;
        case WlCol::ExDiv:
            return r.has_info && r.info.ex_dividend_date.isValid()
                       ? static_cast<double>(r.info.ex_dividend_date.toJulianDay())
                       : kWlmNaN;
        case WlCol::Alert:
            return r.alerts_total > 0 ? static_cast<double>(r.alerts_active) : kWlmNaN;
        case WlCol::State: {
            const core::market::Venue* v = nullptr;
            const auto st = session(r, &v);
            return v ? static_cast<double>(wlm_state_rank(st.state)) : kWlmNaN;
        }
        default:
            return kWlmNaN;
    }
}

QString WatchlistModel::display_text(const WatchlistRowState& r, int column) const {
    const WlCol c = wlm_col(column);
    const services::QuoteData& q = r.quote;
    const QString dash = fmt::placeholder();
    const double v = value(r, column);
    const fmt::InstrumentFmt inst = wlm_inst(r);

    // Quote-driven cells before the first quote: "…" while loading, "—" once it failed.
    const auto quote_pending = [&r, &dash]() { return r.error.isEmpty() ? QStringLiteral("…") : dash; };
    const auto info_cell = [&r](const QString& s) { return r.has_info ? s : QString(); };

    switch (c) {
        case WlCol::Symbol:
            return r.stock.symbol;
        case WlCol::Name:
            return r.display_name();
        case WlCol::Alert:
            if (r.alerts_active > 0)
                return r.alert_fired_today ? QStringLiteral("◆ %1 !").arg(r.alerts_active)
                                           : QStringLiteral("◆ %1").arg(r.alerts_active);
            return r.alerts_total > 0 ? QStringLiteral("◇") : QString();
        case WlCol::Last:
            if (!r.has_quote)
                return quote_pending();
            if (!wlm_finite(v))
                return dash;
            return wlm_show_ccy_symbol(r, inst) ? fmt::price_with_symbol(v, inst) : fmt::price(v, inst);
        case WlCol::Change: {
            if (!r.has_quote)
                return QString();
            if (!wlm_finite(v))
                return dash;
            if (wlm_show_ccy_symbol(r, inst))
                return fmt::signed_money(v, inst.currency, fmt::Compact::None, fmt::price_decimals(q.price, inst));
            return fmt::price_change(v, q.price, inst);
        }
        case WlCol::ChangePct:
        case WlCol::ExtChangePct:
            if (!r.has_quote)
                return QString();
            return fmt::pct(v);
        case WlCol::Open:
        case WlCol::PrevClose:
        case WlCol::DayHigh:
        case WlCol::DayLow:
        case WlCol::ExtPrice:
            if (!r.has_quote)
                return QString();
            return fmt::price(v, inst);
        case WlCol::DisplayPrice: {
            if (!r.has_quote)
                return QString();
            if (!wlm_finite(v))
                return r.currency().isEmpty() ? dash : QStringLiteral("…");
            fmt::InstrumentFmt dinst;
            dinst.currency = display_ccy_;
            return fmt::money(v, display_ccy_, fmt::Compact::None, fmt::price_decimals(v, dinst));
        }
        case WlCol::Currency:
            return r.currency();
        case WlCol::Volume:
        case WlCol::AvgVolume:
            if (!r.has_quote)
                return QString();
            return wlm_finite(v) ? fmt::volume(v) : dash;
        case WlCol::RelVolume:
            if (!r.has_quote)
                return QString();
            return wlm_finite(v) ? fmt::ratio(v, 2) : dash;
        case WlCol::DayRange:
        case WlCol::Range52w:
        case WlCol::Spark:
            return QString(); // painted by their delegates
        case WlCol::Perf1W:
        case WlCol::Perf1M:
        case WlCol::Perf3M:
        case WlCol::Perf6M:
        case WlCol::PerfYtd:
        case WlCol::Perf1Y:
            if (r.history == WatchlistHistoryState::Loading)
                return QStringLiteral("…");
            if (r.history == WatchlistHistoryState::None)
                return QString();
            return fmt::pct(v, 1);
        case WlCol::Vol20d:
            if (r.history == WatchlistHistoryState::Loading)
                return QStringLiteral("…");
            if (r.history == WatchlistHistoryState::None)
                return QString();
            return fmt::pct(v, 1, false);
        case WlCol::FromHigh52w:
        case WlCol::FromLow52w:
        case WlCol::VsMa50:
        case WlCol::VsMa200:
            if (!r.has_quote)
                return QString();
            return fmt::pct(v, 1);
        case WlCol::MarketCap:
            return info_cell(wlm_finite(v) ? fmt::money(v, core::market::major_currency(r.currency()),
                                                        fmt::Compact::FromSettings)
                                           : dash);
        case WlCol::Pe:
            return info_cell(wlm_finite(v) ? fmt::number(v, 1) : dash);
        case WlCol::DivYield:
            return info_cell(fmt::pct(v, 2, false));
        case WlCol::Beta:
            return info_cell(wlm_finite(v) ? fmt::number(v, 2) : dash);
        case WlCol::Sector:
            return info_cell(r.info.sector);
        case WlCol::Industry:
            return info_cell(r.info.industry);
        case WlCol::Country:
            return info_cell(r.info.country);
        case WlCol::Exchange: {
            if (r.has_quote && !q.exchange_name.isEmpty())
                return q.exchange_name;
            if (const auto* venue = core::market::venue_for_symbol(r.stock.symbol, r.exchange_hint()))
                return core::market::venue_display_name(*venue);
            return r.stock.exchange;
        }
        case WlCol::Earnings:
            return info_cell(r.info.earnings_date.isValid() ? fmt::calendar_date(r.info.earnings_date) : dash);
        case WlCol::ExDiv:
            return info_cell(r.info.ex_dividend_date.isValid() ? fmt::calendar_date(r.info.ex_dividend_date) : dash);
        case WlCol::Group:
            return r.stock.group_name;
        case WlCol::Notes:
            return r.stock.notes.section(QLatin1Char('\n'), 0, 0).trimmed();
        case WlCol::State: {
            const core::market::Venue* venue = nullptr;
            const auto st = session(r, &venue);
            if (!venue)
                return r.has_quote ? wlm_yahoo_state_label(q.market_state) : QString();
            const QString t = fmt::FormatSettings::instance().locale().toString(st.venue_local_now.time(),
                                                                               fmt::time_pattern(false));
            return QStringLiteral("%1  %2").arg(core::market::session_state_label(st.state), t);
        }
        case WlCol::Unknown:
            break;
    }
    return {};
}

QStringList WatchlistModel::export_headers(int column) {
    const QString id = wcols::id_at(column);
    const WlCol c = wlm_col(column);
    if (c == WlCol::DayRange || c == WlCol::Range52w)
        return {id + QStringLiteral("_low"), id + QStringLiteral("_high")};
    return {id};
}

QStringList WatchlistModel::export_cells(const WatchlistRowState& r, int column) const {
    const WlCol c = wlm_col(column);
    const services::QuoteData& q = r.quote;
    switch (c) {
        case WlCol::DayRange:
            if (!r.has_quote || !(q.high > 0) || !(q.low > 0))
                return {QString(), QString()};
            return {wlm_c_number(q.low, 10), wlm_c_number(q.high, 10)};
        case WlCol::Range52w:
            if (!r.has_quote)
                return {QString(), QString()};
            return {wlm_c_number(q.fifty_two_week_low, 10), wlm_c_number(q.fifty_two_week_high, 10)};
        case WlCol::Spark:
            return {wlm_c_number(value(r, column), 6)};
        case WlCol::Symbol:
        case WlCol::Name:
        case WlCol::Currency:
        case WlCol::Sector:
        case WlCol::Industry:
        case WlCol::Country:
        case WlCol::Exchange:
        case WlCol::Group:
            return {display_text(r, column)};
        case WlCol::Notes:
            return {r.stock.notes};
        case WlCol::Alert:
            return {QString::number(r.alerts_active)};
        case WlCol::Earnings:
            return {r.has_info && r.info.earnings_date.isValid() ? r.info.earnings_date.toString(Qt::ISODate)
                                                                 : QString()};
        case WlCol::ExDiv:
            return {r.has_info && r.info.ex_dividend_date.isValid() ? r.info.ex_dividend_date.toString(Qt::ISODate)
                                                                    : QString()};
        case WlCol::State: {
            const core::market::Venue* venue = nullptr;
            const auto st = session(r, &venue);
            return {venue ? core::market::session_state_key(st.state) : QString()};
        }
        case WlCol::Volume:
        case WlCol::AvgVolume:
            return {wlm_c_number(value(r, column), 0)};
        default:
            return {wlm_c_number(value(r, column), 10)};
    }
}

// ── Cell roles ──────────────────────────────────────────────────────────────

QString WatchlistModel::cell_tooltip(const WatchlistRowState& r, int column) const {
    const WlCol c = wlm_col(column);
    const services::QuoteData& q = r.quote;
    switch (c) {
        case WlCol::Symbol:
        case WlCol::Name: {
            QStringList lines;
            const QString name = r.display_name();
            lines << (name.isEmpty() ? r.stock.symbol : QStringLiteral("%1 — %2").arg(r.stock.symbol, name));
            if (r.has_quote && !q.exchange_name.isEmpty())
                lines << q.exchange_name;
            if (!r.error.isEmpty())
                lines << (r.has_quote ? tr("Last refresh failed — showing the last quote.\n%1").arg(r.error)
                                      : tr("No quote: %1").arg(r.error));
            if (!r.stock.notes.isEmpty())
                lines << QString() << r.stock.notes;
            return lines.join(QLatin1Char('\n'));
        }
        case WlCol::Last:
        case WlCol::Change:
        case WlCol::ChangePct: {
            if (!r.has_quote)
                return r.error.isEmpty() ? tr("Waiting for the first quote") : tr("No quote: %1").arg(r.error);
            QStringList lines;
            const QTimeZone tz(q.exchange_timezone.toUtf8());
            if (q.market_time > 0)
                lines << tr("As of %1 (%2)")
                             .arg(fmt::date_time(q.market_time, fmt::DateStyle::Medium, false, tz),
                                  fmt::zone_label(QDateTime::fromSecsSinceEpoch(q.market_time), tz));
            else if (!q.market_date.isEmpty())
                lines << tr("As of %1").arg(fmt::calendar_date(QDate::fromString(q.market_date, Qt::ISODate)));
            const QString state = wlm_yahoo_state_label(q.market_state);
            if (!state.isEmpty())
                lines << tr("Session: %1").arg(state);
            if (q.exchange_delay_min > 0)
                lines << tr("Delayed %1 min").arg(q.exchange_delay_min);
            lines << tr("Updated %1").arg(fmt::relative(r.quote_ms / 1000));
            if (is_stale(r))
                lines << tr("Stale: not refreshed recently.");
            if (!r.error.isEmpty())
                lines << tr("Last refresh failed: %1").arg(r.error);
            if (wlm_finite(q.previous_close))
                lines << tr("Previous close %1").arg(fmt::price(q.previous_close, wlm_inst(r)));
            return lines.join(QLatin1Char('\n'));
        }
        case WlCol::DisplayPrice: {
            const QString ccy = r.currency();
            if (ccy.isEmpty())
                return tr("The quote currency is unknown, so the price cannot be converted.");
            const auto fx = services::FxRateService::instance().quote(ccy, display_ccy_);
            if (!fx)
                return tr("Waiting for the %1/%2 rate").arg(ccy, display_ccy_);
            return tr("1 %1 = %2 %3, as of %4%5")
                .arg(ccy, fmt::number(fx->rate, 6), display_ccy_, fmt::time(fx->as_of),
                     fx->triangulated ? tr(" (via USD)") : QString());
        }
        case WlCol::State: {
            const core::market::Venue* venue = nullptr;
            const auto st = session(r, &venue);
            if (!venue)
                return r.has_quote && !q.market_state.isEmpty() ? tr("Session: %1").arg(
                                                                      wlm_yahoo_state_label(q.market_state))
                                                                : tr("Unknown exchange");
            QStringList lines;
            lines << core::market::venue_display_name(*venue);
            lines << tr("Local time %1").arg(fmt::FormatSettings::instance().locale().toString(
                st.venue_local_now, fmt::date_pattern(fmt::DateStyle::Medium) + QLatin1Char(' ') +
                                        fmt::time_pattern(false)));
            if (st.next_change_utc.isValid()) {
                const QDateTime local = st.next_change_utc.toTimeZone(venue->tz);
                lines << tr("%1 at %2 (in %3)")
                             .arg(core::market::session_state_label(st.next_state),
                                  fmt::FormatSettings::instance().locale().toString(local.time(),
                                                                                    fmt::time_pattern(false)),
                                  wlm_duration(QDateTime::currentDateTimeUtc().secsTo(st.next_change_utc)));
            }
            if (st.state == core::market::SessionState::Holiday) {
                const QString h = core::market::holiday_name(*venue, st.trading_date);
                if (!h.isEmpty())
                    lines << h;
            }
            if (r.has_quote && !q.market_state.isEmpty())
                lines << tr("Data source session: %1").arg(wlm_yahoo_state_label(q.market_state));
            return lines.join(QLatin1Char('\n'));
        }
        case WlCol::Alert:
            return r.alerts_tip;
        case WlCol::Notes:
            return r.stock.notes;
        case WlCol::Earnings:
            if (r.has_info && r.info.earnings_date.isValid()) {
                const qint64 days = QDate::currentDate().daysTo(r.info.earnings_date);
                return days >= 0 ? tr("In %n day(s)", "", static_cast<int>(days)) : QString();
            }
            return {};
        case WlCol::Perf1W:
        case WlCol::Perf1M:
        case WlCol::Perf3M:
        case WlCol::Perf6M:
        case WlCol::PerfYtd:
        case WlCol::Perf1Y:
        case WlCol::Vol20d:
            if (r.history == WatchlistHistoryState::Failed)
                return tr("The daily history could not be loaded.");
            if (r.perf.as_of.isValid())
                return tr("From daily closes up to %1").arg(fmt::calendar_date(r.perf.as_of));
            return {};
        case WlCol::MarketCap:
            return r.has_info ? tr("Currency: %1").arg(core::market::major_currency(r.currency())) : QString();
        default:
            return {};
    }
}

QVariant WatchlistModel::cell_foreground(const WatchlistRowState& r, int column) const {
    const WlCol c = wlm_col(column);
    const double v = value(r, column);
    if (wlm_is_direction(c)) {
        if (!wlm_finite(v))
            return QColor(colors::TEXT_TERTIARY());
        if (v == 0.0)
            return QColor(colors::TEXT_SECONDARY());
        return QColor(v > 0 ? colors::POSITIVE() : colors::NEGATIVE());
    }
    switch (c) {
        case WlCol::Symbol:
            return QColor(!r.has_quote && !r.error.isEmpty() ? colors::TEXT_TERTIARY() : colors::TEXT_PRIMARY());
        case WlCol::Last:
        case WlCol::DisplayPrice: {
            if (!r.has_quote)
                return QColor(colors::TEXT_TERTIARY());
            if (is_stale(r))
                return QColor(colors::TEXT_TERTIARY());
            // A price from a closed session is a reference, not a live print.
            const core::market::Venue* venue = nullptr;
            const auto st = session(r, &venue);
            const bool live = venue ? (st.state == core::market::SessionState::Open)
                                    : r.quote.market_state == QLatin1String("REGULAR");
            return QColor(live ? colors::TEXT_PRIMARY() : colors::TEXT_SECONDARY());
        }
        case WlCol::State: {
            const core::market::Venue* venue = nullptr;
            const auto st = session(r, &venue);
            if (!venue)
                return QColor(colors::TEXT_TERTIARY());
            using S = core::market::SessionState;
            if (st.state == S::Open)
                return QColor(colors::SUCCESS());
            if (st.state == S::Pre || st.state == S::Post || st.state == S::Lunch)
                return QColor(colors::WARNING());
            return QColor(colors::TEXT_TERTIARY());
        }
        case WlCol::Alert:
            if (r.alert_fired_today)
                return QColor(colors::WARNING());
            return QColor(r.alerts_active > 0 ? colors::AMBER() : colors::TEXT_TERTIARY());
        case WlCol::Earnings:
            if (r.has_info && r.info.earnings_date.isValid()) {
                const qint64 days = QDate::currentDate().daysTo(r.info.earnings_date);
                if (days >= 0 && days <= 7)
                    return QColor(colors::AMBER());
            }
            return QColor(colors::TEXT_PRIMARY());
        case WlCol::Name:
        case WlCol::Notes:
        case WlCol::Group:
        case WlCol::Sector:
        case WlCol::Industry:
        case WlCol::Country:
        case WlCol::Exchange:
        case WlCol::Currency:
            return QColor(colors::TEXT_SECONDARY());
        default:
            if (wlm_is_perf(c) && r.history == WatchlistHistoryState::Loading)
                return QColor(colors::TEXT_TERTIARY());
            return QColor(wlm_finite(v) ? colors::TEXT_PRIMARY() : colors::TEXT_TERTIARY());
    }
}

QVariant WatchlistModel::cell_data(const WatchlistRowState& r, int column, int role) const {
    const WlCol c = wlm_col(column);
    const WatchlistColKind kind = wcols::def(column).kind;
    switch (role) {
        case Qt::DisplayRole:
            return display_text(r, column);
        case Qt::EditRole: {
            const double v = value(r, column);
            return wlm_finite(v) ? QVariant(v) : QVariant();
        }
        case Qt::ToolTipRole:
            return cell_tooltip(r, column);
        case Qt::ForegroundRole:
            return cell_foreground(r, column);
        case Qt::TextAlignmentRole:
            switch (kind) {
                case WatchlistColKind::Symbol:
                case WatchlistColKind::Text:
                case WatchlistColKind::State:
                    return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
                case WatchlistColKind::Alert:
                    return QVariant(Qt::AlignCenter);
                default:
                    return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            }
        case SortRole: {
            switch (kind) {
                case WatchlistColKind::Symbol:
                case WatchlistColKind::Text:
                    return display_text(r, column).toLower();
                default: {
                    const double v = value(r, column);
                    if (c == WlCol::MarketCap && wlm_finite(v)) {
                        // Capitalisations are in each listing's own currency: rank them in
                        // the display currency, as the footer total does. Not convertible
                        // yet (rate loading) = missing, never compared across currencies.
                        const auto conv = services::FxRateService::instance().convert(
                            v, core::market::major_currency(r.currency()), display_ccy_);
                        return QVariant(conv ? *conv : kWlmNaN);
                    }
                    return QVariant(v); // NaN = missing (sorted last)
                }
            }
        }
        case SymbolRole:
            return r.stock.symbol;
        case IsGroupRole:
            return false;
        case GroupKeyRole:
            return group_key_of(r);
        case FlashRole:
            return wlm_is_flash(c) ? flash_strength(r) : 0.0;
        case FilterTextRole: {
            QStringList parts{r.stock.symbol, r.display_name(), r.stock.notes, r.stock.group_name};
            if (r.has_info)
                parts << r.info.sector << r.info.industry << r.info.country;
            if (r.has_quote)
                parts << r.quote.exchange_name << r.quote.currency;
            return parts.join(QLatin1Char(' ')).toLower();
        }
        case BaselineRole:
            if (r.has_quote && wlm_finite(r.quote.previous_close) && r.quote.previous_close > 0)
                return r.quote.previous_close;
            return {};
        case PriceDecimalsRole:
            if (r.has_quote && wlm_finite(r.quote.price) && r.quote.price > 0)
                return fmt::price_decimals(r.quote.price, wlm_inst(r));
            return {};
        case ui::RangeBarDelegate::LowRole:
        case ui::RangeBarDelegate::HighRole:
        case ui::RangeBarDelegate::ValueRole: {
            if (!r.has_quote)
                return {};
            double lo = kWlmNaN;
            double hi = kWlmNaN;
            if (c == WlCol::DayRange) {
                lo = r.quote.low;
                hi = r.quote.high;
            } else if (c == WlCol::Range52w) {
                lo = r.quote.fifty_two_week_low;
                hi = r.quote.fifty_two_week_high;
            } else {
                return {};
            }
            if (!wlm_finite(lo) || !wlm_finite(hi) || !(hi > lo) || !(lo > 0))
                return {};
            if (role == ui::RangeBarDelegate::LowRole)
                return lo;
            if (role == ui::RangeBarDelegate::HighRole)
                return hi;
            return r.quote.price > 0 ? QVariant(std::clamp(r.quote.price, lo, hi)) : QVariant();
        }
        case ui::SparklineDelegate::kDefaultRole:
            if (c == WlCol::Spark && r.spark.size() >= 2)
                return QVariant::fromValue(r.spark);
            return {};
        default:
            return {};
    }
}

QVariant WatchlistModel::group_data(int group, int column, int role) const {
    if (group < 0 || group >= groups_.size())
        return {};
    const Group& g = groups_[group];
    const WlCol c = wlm_col(column);

    const auto average = [&]() {
        double sum = 0;
        int n = 0;
        for (int i : g.members) {
            const double v = value(rows_[i], column);
            if (wlm_finite(v)) {
                sum += v;
                ++n;
            }
        }
        return n > 0 ? sum / n : kWlmNaN;
    };

    switch (role) {
        case Qt::DisplayRole:
            if (c == WlCol::Symbol)
                return QStringLiteral("%1  (%2)").arg(g.label).arg(g.members.size());
            if (c == WlCol::Name) {
                int up = 0;
                int down = 0;
                for (int i : g.members) {
                    const double v = value(rows_[i], wlm_col_index(WlCol::ChangePct));
                    if (wlm_finite(v) && v > 0)
                        ++up;
                    else if (wlm_finite(v) && v < 0)
                        ++down;
                }
                return tr("%1 up · %2 down").arg(up).arg(down);
            }
            if (wlm_is_averaged(c)) {
                const double v = average();
                if (!wlm_finite(v))
                    return QString();
                if (c == WlCol::RelVolume)
                    return fmt::ratio(v, 2);
                if (c == WlCol::Vol20d)
                    return fmt::pct(v, 1, false);
                return fmt::pct(v, wlm_is_perf(c) ? 1 : 2);
            }
            return {};
        case Qt::EditRole:
            if (wlm_is_averaged(c)) {
                const double v = average();
                return wlm_finite(v) ? QVariant(v) : QVariant();
            }
            return {};
        case Qt::ForegroundRole:
            if (wlm_is_direction(c)) {
                const double v = average();
                if (wlm_finite(v) && v != 0.0)
                    return QColor(v > 0 ? colors::POSITIVE() : colors::NEGATIVE());
            }
            return QColor(c == WlCol::Symbol ? colors::AMBER() : colors::TEXT_SECONDARY());
        case Qt::ToolTipRole:
            if (c == WlCol::Symbol || c == WlCol::Name)
                return tr("%1 — %n symbol(s)", "", static_cast<int>(g.members.size())).arg(g.label);
            if (wlm_is_averaged(c))
                return tr("Equal-weighted average of the group");
            return {};
        case Qt::TextAlignmentRole:
            if (c == WlCol::Symbol || c == WlCol::Name)
                return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case IsGroupRole:
            return true;
        case GroupKeyRole:
            return g.key;
        case SortRole:
            return g.label.toLower();
        case FlashRole:
            return 0.0;
        default:
            return {};
    }
}

// ── QAbstractItemModel ──────────────────────────────────────────────────────

QModelIndex WatchlistModel::index(int row, int column, const QModelIndex& parent) const {
    if (row < 0 || column < 0 || column >= columnCount())
        return {};
    if (!parent.isValid()) {
        if (row >= rowCount())
            return {};
        return createIndex(row, column, quintptr(0));
    }
    if (grouped() && parent.internalId() == 0 && parent.column() == 0 && parent.row() < groups_.size()) {
        if (row >= groups_[parent.row()].members.size())
            return {};
        return createIndex(row, column, quintptr(parent.row() + 1));
    }
    return {};
}

QModelIndex WatchlistModel::parent(const QModelIndex& child) const {
    if (!child.isValid())
        return {};
    const quintptr id = child.internalId();
    if (id == 0)
        return {};
    return createIndex(static_cast<int>(id - 1), 0, quintptr(0));
}

int WatchlistModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid())
        return grouped() ? static_cast<int>(groups_.size()) : static_cast<int>(rows_.size());
    if (grouped() && parent.internalId() == 0 && parent.column() == 0 && parent.row() < groups_.size())
        return static_cast<int>(groups_[parent.row()].members.size());
    return 0;
}

int WatchlistModel::columnCount(const QModelIndex&) const {
    return wcols::count();
}

QVariant WatchlistModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid())
        return {};
    if (is_group(idx))
        return group_data(idx.row(), idx.column(), role);
    const int i = row_index(idx);
    if (i < 0)
        return {};
    return cell_data(rows_[i], idx.column(), role);
}

QVariant WatchlistModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || section < 0 || section >= columnCount())
        return {};
    if (role == Qt::DisplayRole) {
        if (wlm_col(section) == WlCol::DisplayPrice)
            return tr("LAST (%1)").arg(display_ccy_);
        return wcols::header(section);
    }
    if (role == Qt::ToolTipRole)
        return wcols::tooltip(section);
    if (role == Qt::TextAlignmentRole) {
        switch (wcols::def(section).kind) {
            case WatchlistColKind::Symbol:
            case WatchlistColKind::Text:
            case WatchlistColKind::State:
                return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
            case WatchlistColKind::Alert:
                return QVariant(Qt::AlignCenter);
            default:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    return {};
}

Qt::ItemFlags WatchlistModel::flags(const QModelIndex& idx) const {
    if (!idx.isValid())
        return Qt::ItemIsDropEnabled;
    if (is_group(idx))
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
}

Qt::DropActions WatchlistModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions WatchlistModel::supportedDragActions() const {
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList WatchlistModel::mimeTypes() const {
    return {QString::fromLatin1(kRowsMime), QString::fromLatin1(symbol_mime::kMimeType)};
}

QMimeData* WatchlistModel::mimeData(const QModelIndexList& indexes) const {
    QVector<int> rows;
    for (const QModelIndex& idx : indexes) {
        const int i = row_index(idx);
        if (i >= 0 && !rows.contains(i))
            rows.append(i);
    }
    if (rows.isEmpty())
        return nullptr;
    std::sort(rows.begin(), rows.end());
    QJsonArray syms;
    for (int i : rows)
        syms.append(rows_[i].stock.symbol);
    // The shared symbol payload first, so every panel's symbol drop target accepts the drag.
    QMimeData* m = symbol_mime::pack(rows_[rows.first()].symbol_ref(), link_group_);
    if (!m)
        return nullptr;
    QJsonObject o;
    o.insert(QStringLiteral("list"), list_id_);
    o.insert(QStringLiteral("symbols"), syms);
    m->setData(QString::fromLatin1(kRowsMime), QJsonDocument(o).toJson(QJsonDocument::Compact));
    return m;
}

bool WatchlistModel::canDropMimeData(const QMimeData* data, Qt::DropAction, int, int, const QModelIndex&) const {
    return data && (data->hasFormat(QString::fromLatin1(kRowsMime)) || symbol_mime::has_symbol(data));
}

bool WatchlistModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int, const QModelIndex& parent) {
    if (!data)
        return false;
    if (action == Qt::IgnoreAction)
        return true;

    if (data->hasFormat(QString::fromLatin1(kRowsMime))) {
        const QJsonObject o = QJsonDocument::fromJson(data->data(QString::fromLatin1(kRowsMime))).object();
        QStringList moved;
        for (const auto& v : o.value(QStringLiteral("symbols")).toArray())
            moved << v.toString();
        if (moved.isEmpty())
            return false;
        if (o.value(QStringLiteral("list")).toString() != list_id_) {
            // Rows dragged from another list: add them here.
            QList<SymbolRef> refs;
            for (const QString& s : moved) {
                SymbolRef ref;
                ref.symbol = s;
                ref.asset_class = watchlist_asset_class(s);
                refs << ref;
            }
            if (external_drop_handler_)
                external_drop_handler_(refs);
            return true;
        }

        QString anchor;
        bool after = false;
        QString gkey;
        bool has_group = false;
        if (parent.isValid() && parent.model() == this) {
            if (is_group(parent)) {
                const Group& g = groups_[parent.row()];
                gkey = g.key;
                has_group = true;
                if (row >= 0 && row < g.members.size()) {
                    anchor = rows_[g.members[row]].stock.symbol;
                } else if (!g.members.isEmpty()) {
                    anchor = rows_[g.members.last()].stock.symbol;
                    after = true;
                }
            } else {
                const int i = row_index(parent);
                if (i >= 0) {
                    anchor = rows_[i].stock.symbol;
                    if (grouped()) {
                        gkey = groups_[row_group_[i]].key;
                        has_group = true;
                    }
                }
            }
        } else if (grouped()) {
            if (row >= 0 && row < groups_.size() && !groups_[row].members.isEmpty()) {
                anchor = rows_[groups_[row].members.first()].stock.symbol;
                gkey = groups_[row].key;
                has_group = true;
            } else if (!groups_.isEmpty() && !groups_.last().members.isEmpty()) {
                anchor = rows_[groups_.last().members.last()].stock.symbol;
                after = true;
                gkey = groups_.last().key;
                has_group = true;
            }
        } else if (row >= 0 && row < rows_.size()) {
            anchor = rows_[row].stock.symbol;
        } else if (!rows_.isEmpty()) {
            anchor = rows_.last().stock.symbol;
            after = true;
        }
        if (move_handler_)
            move_handler_(moved, anchor, after, gkey, has_group);
        return true;
    }

    if (symbol_mime::has_symbol(data)) {
        const SymbolRef ref = symbol_mime::unpack(data);
        if (ref.is_valid() && external_drop_handler_)
            external_drop_handler_({ref});
        return ref.is_valid();
    }
    return false;
}

// ── WatchlistProxyModel ─────────────────────────────────────────────────────

WatchlistProxyModel::WatchlistProxyModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(WatchlistModel::SortRole);
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
}

void WatchlistProxyModel::set_filter_text(const QString& text) {
    const QString f = text.trimmed().toLower();
    if (f == filter_)
        return;
    filter_ = f;
    invalidateRowsFilter();
}

bool WatchlistProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    if (filter_.isEmpty())
        return true;
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    if (idx.data(WatchlistModel::IsGroupRole).toBool())
        return false; // shown when one of its members matches (recursive filtering)
    const QString hay = idx.data(WatchlistModel::FilterTextRole).toString();
    const QStringList terms = filter_.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& t : terms) {
        if (!hay.contains(t))
            return false;
    }
    return true;
}

bool WatchlistProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    const bool asc = sortOrder() == Qt::AscendingOrder;
    // Group rows keep the model's group order whichever way the members sort.
    if (left.data(WatchlistModel::IsGroupRole).toBool() || right.data(WatchlistModel::IsGroupRole).toBool())
        return asc ? left.row() < right.row() : right.row() < left.row();

    const QVariant a = left.data(WatchlistModel::SortRole);
    const QVariant b = right.data(WatchlistModel::SortRole);
    // Missing values go last in either direction (the descending sort calls lessThan(right, left)).
    const auto missing = [](const QVariant& v) {
        if (!v.isValid())
            return true;
        if (v.typeId() == QMetaType::Double)
            return !std::isfinite(v.toDouble());
        return v.toString().isEmpty();
    };
    const bool ma = missing(a);
    const bool mb = missing(b);
    if (ma || mb) {
        if (ma && mb)
            return false;
        return ma ? !asc : asc;
    }
    if (a.typeId() == QMetaType::Double && b.typeId() == QMetaType::Double)
        return a.toDouble() < b.toDouble();
    return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
}

} // namespace fincept::screens
