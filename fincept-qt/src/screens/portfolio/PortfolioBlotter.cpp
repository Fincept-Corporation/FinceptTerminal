// src/screens/portfolio/PortfolioBlotter.cpp
#include "screens/portfolio/PortfolioBlotter.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/markets/MarketDataService.h"
#include "services/portfolio/PortfolioClassify.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "ui/charts/ChartExport.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QClipboard>
#include <QDate>
#include <QEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

// ── Columns ──────────────────────────────────────────────────────────────────

enum PfBlotterCol {
    kPbSymbol,
    kPbName,
    kPbQty,
    kPbLast,
    kPbCcy,
    kPbFx,
    kPbAvgCost,
    kPbValueLocal,
    kPbValue,
    kPbCost,
    kPbPnl,
    kPbPnlPct,
    kPbPricePnl,
    kPbFxPnl,
    kPbDayPct,
    kPbDayPnl,
    kPbTrend,
    kPbWeight,
    kPbRiskShare,
    kPbBeta,
    kPbVol,
    kPbCompVar,
    kPbRealized,
    kPbDividends,
    kPbYield,
    kPbSector,
    kPbIndustry,
    kPbAssetClass,
    kPbCountry,
    kPbExchange,
    kPbRange52,
    kPbStatus,
    kPbLots,
    kPbColCount
};

struct PfBlotterColDef {
    const char* key;   // stable settings key
    const char* title; // translatable header
    const char* tip;   // translatable header tooltip
    bool numeric;
    bool default_visible;
};

// QT_TRANSLATE_NOOP under the PortfolioBlotter context; tr() resolves them at runtime.
static const PfBlotterColDef kPbCols[kPbColCount] = {
    {"symbol", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "SYMBOL"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Ticker. ⚠ = no live price or no FX rate (excluded from totals); ◷ = price from an earlier session."), false, true},
    {"name", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "NAME"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Instrument name."), false, false},
    {"qty", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "QTY"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Units held."), true, true},
    {"last", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "LAST"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Last price in the instrument's quote currency."), true, true},
    {"ccy", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "CCY"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Quote currency (minor units such as GBp are pence)."), false, false},
    {"fx", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "FX"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Spot rate: base-currency units per one unit of the holding's currency."), true, false},
    {"avg_cost", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "AVG COST"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Average cost per unit (fees included when the ledger explains the position), quote currency."), true, true},
    {"value_local", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "VALUE (LOCAL)"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Market value in the holding's own currency."), true, false},
    {"value", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "VALUE"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Market value in the portfolio's base currency."), true, true},
    {"cost", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "COST"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Cost basis in the base currency (trade-date FX when recorded)."), true, false},
    {"pnl", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "P&L"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Unrealised P&L in the base currency."), true, true},
    {"pnl_pct", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "P&L %"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Unrealised P&L as % of cost."), true, true},
    {"price_pnl", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "PRICE P&L"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Part of the unrealised P&L from the price move in its own currency."), true, false},
    {"fx_pnl", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "FX P&L"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Part of the unrealised P&L from the currency move since purchase (needs trade-date FX)."), true, false},
    {"day_pct", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "DAY %"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Price change since the previous close."), true, true},
    {"day_pnl", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "DAY P&L"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Today's change of the position in the base currency."), true, true},
    {"trend", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "TREND"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Recent closes."), false, true},
    {"weight", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "WEIGHT"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Share of the valued holdings (cash excluded)."), true, true},
    {"risk_share", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "RISK SHARE"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Share of the portfolio's variance this holding contributes (ex-ante)."), true, false},
    {"beta", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "BETA"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Beta vs the portfolio's benchmark (trailing daily returns, base currency)."), true, false},
    {"vol", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "VOL"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Annualised volatility in the base currency (currency risk included)."), true, false},
    {"comp_var", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "COMP. VaR"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Component VaR: this holding's part of the parametric portfolio VaR, % of NAV."), true, false},
    {"realized", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "REALISED"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Realised P&L on this symbol (portfolio lot method), base currency."), true, false},
    {"dividends", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "DIVIDENDS"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Net dividends received, base currency."), true, false},
    {"yield", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "YIELD"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Trailing dividend yield reported by the market data source."), true, false},
    {"sector", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "SECTOR"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Sector."), false, false},
    {"industry", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "INDUSTRY"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Industry."), false, false},
    {"asset_class", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "CLASS"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Asset class (from the quote type; * = guessed from the ticker)."), false, false},
    {"country", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "COUNTRY"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Country of the issuer."), false, false},
    {"exchange", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "EXCHANGE"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Listing exchange."), false, false},
    {"range52", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "52W RANGE"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Last price within the 52-week low–high range."), false, false},
    {"status", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "MARKET"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Market session and the date of the price."), false, false},
    {"lots", QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "LOTS"), QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "Open tax lots (long-term units in the tooltip)."), true, false},
};

static constexpr int kPbSortRole = Qt::UserRole + 1;
static constexpr int kPbTotalRole = Qt::UserRole + 2;
static constexpr int kPbExportRole = Qt::UserRole + 3;
static constexpr int kPbSymbolRole = Qt::UserRole + 4;

static constexpr const char* kPbColumnsKey = "portfolio.blotter.columns";
static constexpr const char* kPbHeaderKey = "portfolio.blotter.header";
/// The user's own choice for the PRICE P&L / FX P&L columns ("price_pnl=1,fx_pnl=0"),
/// made in the column menu. Without one they follow the FX P&L split setting.
static constexpr const char* kPbSplitColsKey = "portfolio.blotter.fx_split_columns";

static QString pb_csv_num(double v, int dp) {
    return ui::formatting::export_number(v, dp); // NaN -> ""
}

// ── Model ────────────────────────────────────────────────────────────────────

class PfBlotterModel : public QAbstractTableModel {
  public:
    struct Live {
        double ltp = portfolio::kAbsent;
        double change_pct = portfolio::kAbsent;
    };

    explicit PfBlotterModel(QObject* parent) : QAbstractTableModel(parent) {}

    void set_summary(const portfolio::PortfolioSummary& s) {
        beginResetModel();
        summary_ = s;
        has_summary_ = true;
        live_.clear();
        update_mixes();
        endResetModel();
    }
    void set_holdings(const QVector<portfolio::HoldingWithQuote>& h) {
        beginResetModel();
        summary_.holdings = h;
        has_summary_ = false; // no totals without a summary
        live_.clear();
        update_mixes();
        endResetModel();
    }
    void set_analytics(const portfolio::MarketAnalytics& a) {
        risk_ = a.holdings;
        emit_all_changed();
    }
    void set_allocation(const portfolio::AllocationReport& r) {
        attrs_.clear();
        for (const auto& b : r.breakdowns)
            for (const auto& slice : b.slices)
                for (const auto& sym : slice.symbols)
                    attrs_[sym].insert(static_cast<int>(b.dimension), slice.key);
        class_guessed_ = QSet<QString>(r.class_guessed.begin(), r.class_guessed.end());
        emit_all_changed();
    }
    void set_info(const QHash<QString, services::InfoData>& info) {
        info_ = info;
        emit_all_changed();
    }
    void set_spark(const QString& sym, const QVector<double>& closes) {
        spark_.insert(sym, closes);
        const int r = row_of(sym);
        if (r >= 0)
            emit dataChanged(index(r, kPbTrend), index(r, kPbTrend));
    }
    void set_live(const QString& sym, double ltp, double chg_pct) {
        const int r = row_of(sym);
        if (r < 0 || !(ltp > 0))
            return;
        live_[sym] = Live{ltp, chg_pct};
        emit dataChanged(index(r, 0), index(r, kPbColCount - 1));
    }

    int row_of(const QString& sym) const {
        for (int i = 0; i < summary_.holdings.size(); ++i)
            if (summary_.holdings[i].symbol == sym)
                return i;
        return -1;
    }
    const portfolio::PortfolioSummary& summary() const { return summary_; }
    QString base() const { return pfmt::base_ccy(summary_); }
    int holding_count() const { return static_cast<int>(summary_.holdings.size()); }

    int rowCount(const QModelIndex& parent = {}) const override {
        if (parent.isValid())
            return 0;
        const int n = holding_count();
        return n + (has_summary_ && n > 0 ? 1 : 0);
    }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : kPbColCount; }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (o != Qt::Horizontal || section < 0 || section >= kPbColCount)
            return {};
        if (role == Qt::DisplayRole)
            return PortfolioBlotter::tr(kPbCols[section].title);
        if (role == Qt::ToolTipRole)
            return PortfolioBlotter::tr(kPbCols[section].tip);
        if (role == Qt::TextAlignmentRole)
            return QVariant::fromValue(kPbCols[section].numeric ? Qt::AlignRight | Qt::AlignVCenter
                                                                 : Qt::AlignLeft | Qt::AlignVCenter);
        return {};
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid())
            return {};
        const int col = idx.column();
        if (idx.row() >= holding_count())
            return total_data(col, role);
        const auto& h = summary_.holdings[idx.row()];
        switch (role) {
            case Qt::DisplayRole:
                return display(h, col);
            case kPbSortRole:
                return sort_value(h, col);
            case kPbExportRole:
                return export_value(h, col);
            case kPbSymbolRole:
                return h.symbol;
            case kPbTotalRole:
                return false;
            case Qt::TextAlignmentRole:
                return QVariant::fromValue(kPbCols[col].numeric ? Qt::AlignRight | Qt::AlignVCenter
                                                                 : Qt::AlignLeft | Qt::AlignVCenter);
            case Qt::ForegroundRole:
                return foreground(h, col);
            case Qt::ToolTipRole:
                return tooltip(h, col);
            case ui::SparklineDelegate::kDefaultRole:
                if (col == kPbTrend) {
                    const auto it = spark_.constFind(h.symbol);
                    if (it != spark_.constEnd() && it->size() >= 2)
                        return QVariant::fromValue(*it);
                }
                return {};
            case ui::RangeBarDelegate::LowRole:
            case ui::RangeBarDelegate::HighRole:
            case ui::RangeBarDelegate::ValueRole: {
                if (col != kPbRange52)
                    return {};
                const auto it = info_.constFind(h.symbol);
                if (it == info_.constEnd())
                    return {};
                const double lo = it->week52_low;
                const double hi = it->week52_high;
                const double v = last(h);
                if (!(std::isfinite(lo) && std::isfinite(hi) && hi > lo && lo > 0))
                    return {};
                if (role == ui::RangeBarDelegate::LowRole)
                    return lo;
                if (role == ui::RangeBarDelegate::HighRole)
                    return hi;
                return std::isfinite(v) ? QVariant(v) : QVariant();
            }
            default:
                return {};
        }
    }

  private:
    void emit_all_changed() {
        if (rowCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, kPbColCount - 1));
    }

    /// Currencies of the per-holding columns that are not in the base currency.
    void update_mixes() {
        const QString b = base();
        local_mix_ = ui::formatting::CurrencyMix();
        prices_need_unit_ = false;
        for (const auto& h : summary_.holdings) {
            local_mix_.add(h.currency.isEmpty() ? b : h.currency);
            const QString q = h.price_currency.isEmpty() ? h.currency : h.price_currency;
            if (!q.isEmpty() && q != b)
                prices_need_unit_ = true; // GBp counts: pence are not the base unit
        }
    }
    /// Last / average cost: as quoted, with the unit when the column holds more
    /// than the base currency (a bare number would not say which currency it is).
    QString price_cell(double v, const portfolio::HoldingWithQuote& h, const QString& qccy) const {
        return prices_need_unit_ ? pfmt::price_sym(v, qccy, h.symbol) : pfmt::price(v, qccy, h.symbol);
    }
    /// "≈ " on a base-currency figure converted at a stale rate (R3).
    bool fx_stale(const portfolio::HoldingWithQuote& h) const {
        return !summary_.fx_stale.isEmpty() && summary_.fx_stale.contains(h.currency, Qt::CaseInsensitive);
    }
    static QString approx(bool stale, double v, const QString& text) {
        return stale && std::isfinite(v) ? QStringLiteral("≈ ") + text : text;
    }

    // Live-tick aware price / value (ticks only override a priced holding).
    double last(const portfolio::HoldingWithQuote& h) const {
        const auto it = live_.constFind(h.symbol);
        return it != live_.constEnd() && h.priced ? it->ltp : h.current_price;
    }
    double value_scale(const portfolio::HoldingWithQuote& h) const {
        // base value per (quantity × quoted price): captures minor units and spot FX
        if (!(h.quantity > 0) || !(h.current_price > 0) || !std::isfinite(h.market_value))
            return portfolio::kAbsent;
        return h.market_value / (h.quantity * h.current_price);
    }
    double value(const portfolio::HoldingWithQuote& h) const {
        if (!live_.contains(h.symbol))
            return h.market_value;
        const double k = value_scale(h);
        return std::isfinite(k) ? h.quantity * last(h) * k : h.market_value;
    }
    double pnl(const portfolio::HoldingWithQuote& h) const {
        if (!live_.contains(h.symbol))
            return h.unrealized_pnl;
        const double v = value(h);
        return std::isfinite(v) ? v - h.cost_basis : portfolio::kAbsent;
    }
    double pnl_pct(const portfolio::HoldingWithQuote& h) const {
        if (!live_.contains(h.symbol))
            return h.unrealized_pnl_percent;
        const double p = pnl(h);
        return std::isfinite(p) && h.cost_basis > 0 ? p / h.cost_basis * 100.0 : portfolio::kAbsent;
    }
    double day_pct(const portfolio::HoldingWithQuote& h) const {
        const auto it = live_.constFind(h.symbol);
        return it != live_.constEnd() && std::isfinite(it->change_pct) ? it->change_pct : h.day_change_percent;
    }
    double day_pnl(const portfolio::HoldingWithQuote& h) const {
        const auto it = live_.constFind(h.symbol);
        if (it == live_.constEnd() || !std::isfinite(it->change_pct) || it->change_pct <= -100.0)
            return h.day_change_value;
        const double v = value(h);
        return std::isfinite(v) ? v - v / (1.0 + it->change_pct / 100.0) : portfolio::kAbsent;
    }
    const portfolio::HoldingRisk* risk(const QString& sym) const {
        const auto it = risk_.constFind(sym);
        return it == risk_.constEnd() ? nullptr : &*it;
    }
    QString attr(const portfolio::HoldingWithQuote& h, portfolio::AllocationDimension d) const {
        const auto it = attrs_.constFind(h.symbol);
        if (it != attrs_.constEnd()) {
            const QString v = it->value(static_cast<int>(d));
            if (!v.isEmpty())
                return v;
        }
        if (d == portfolio::AllocationDimension::Sector)
            return h.sector;
        if (d == portfolio::AllocationDimension::Exchange)
            return h.exchange;
        if (d == portfolio::AllocationDimension::AssetClass)
            return portfolio::asset_class_key(portfolio::classify_asset(h.symbol, h.quote_type, h.name).cls);
        return {};
    }
    double yield(const QString& sym) const {
        const auto it = info_.constFind(sym);
        return it == info_.constEnd() ? portfolio::kAbsent : it->dividend_yield;
    }
    bool stale(const portfolio::HoldingWithQuote& h) const {
        return h.priced && !h.as_of.isEmpty() && !summary_.as_of.isEmpty() && h.as_of < summary_.as_of;
    }

    double numeric(const portfolio::HoldingWithQuote& h, int col) const {
        const auto* r = risk(h.symbol);
        switch (col) {
            case kPbQty:
                return h.quantity;
            case kPbLast:
                return last(h);
            case kPbFx:
                return h.fx_rate;
            case kPbAvgCost:
                return h.avg_buy_price;
            case kPbValueLocal:
                return live_.contains(h.symbol) && std::isfinite(value_scale(h)) && std::isfinite(h.fx_rate) &&
                               h.fx_rate > 0
                           ? value(h) / h.fx_rate
                           : h.market_value_local;
            case kPbValue:
                return value(h);
            case kPbCost:
                return h.cost_basis;
            case kPbPnl:
                return pnl(h);
            case kPbPnlPct:
                return pnl_pct(h);
            case kPbPricePnl:
                return h.price_pnl;
            case kPbFxPnl:
                return h.fx_pnl;
            case kPbDayPct:
                return day_pct(h);
            case kPbDayPnl:
                return day_pnl(h);
            case kPbWeight:
                return h.weight;
            case kPbRiskShare:
                return r && r->risk_contribution ? *r->risk_contribution * 100.0 : portfolio::kAbsent;
            case kPbBeta:
                return r && r->beta ? *r->beta : portfolio::kAbsent;
            case kPbVol:
                return r && r->ann_vol ? *r->ann_vol * 100.0 : portfolio::kAbsent;
            case kPbCompVar:
                return r && r->component_var ? *r->component_var * 100.0 : portfolio::kAbsent;
            case kPbRealized:
                return h.realized_pnl;
            case kPbDividends:
                return h.dividends;
            case kPbYield: {
                const double y = yield(h.symbol);
                return std::isfinite(y) ? y * 100.0 : portfolio::kAbsent;
            }
            case kPbRange52: {
                const auto it = info_.constFind(h.symbol);
                if (it == info_.constEnd())
                    return portfolio::kAbsent;
                const double lo = it->week52_low, hi = it->week52_high, v = last(h);
                return std::isfinite(lo) && std::isfinite(hi) && hi > lo && std::isfinite(v) ? (v - lo) / (hi - lo)
                                                                                              : portfolio::kAbsent;
            }
            case kPbLots:
                return h.open_lots;
            default:
                return portfolio::kAbsent;
        }
    }

    QString text_attr(const portfolio::HoldingWithQuote& h, int col) const {
        using AD = portfolio::AllocationDimension;
        switch (col) {
            case kPbSymbol:
                return h.symbol;
            case kPbName:
                return h.name;
            case kPbCcy:
                return h.price_currency.isEmpty() ? h.currency : h.price_currency;
            case kPbSector:
                return attr(h, AD::Sector);
            case kPbIndustry:
                return attr(h, AD::Industry);
            case kPbAssetClass:
                return attr(h, AD::AssetClass);
            case kPbCountry:
                return attr(h, AD::Country);
            case kPbExchange:
                return attr(h, AD::Exchange);
            case kPbStatus:
                return h.market_state;
            default:
                return {};
        }
    }

    QVariant display(const portfolio::HoldingWithQuote& h, int col) const {
        const QString base_c = base();
        const QString qccy = h.price_currency.isEmpty() ? h.currency : h.price_currency;
        switch (col) {
            case kPbSymbol: {
                QString s = h.symbol;
                if (!h.valued())
                    s = QStringLiteral("⚠ ") + s;
                else if (stale(h))
                    s = QStringLiteral("◷ ") + s;
                return s;
            }
            case kPbName:
            case kPbSector:
            case kPbIndustry:
            case kPbCountry:
            case kPbExchange: {
                const QString t = text_attr(h, col);
                return t.isEmpty() ? pfmt::dash() : t;
            }
            case kPbAssetClass: {
                const QString t = text_attr(h, col);
                return t.isEmpty() ? pfmt::dash() : (class_guessed_.contains(h.symbol) ? t + QStringLiteral(" *") : t);
            }
            case kPbCcy:
                return qccy.isEmpty() ? pfmt::dash() : qccy;
            case kPbQty:
                return pfmt::qty(h.quantity);
            case kPbLast:
                return price_cell(last(h), h, qccy);
            case kPbAvgCost:
                return price_cell(h.avg_buy_price, h, qccy);
            case kPbFx: {
                if (!std::isfinite(h.fx_rate))
                    return pfmt::dash();
                ui::formatting::InstrumentFmt fx;
                fx.fx = true;
                return ui::formatting::price(h.fx_rate, fx);
            }
            case kPbValueLocal:
                return ui::formatting::money_in_column(numeric(h, col), h.currency.isEmpty() ? base_c : h.currency,
                                                       local_mix_);
            case kPbValue:
                return approx(fx_stale(h), numeric(h, col), pfmt::money(numeric(h, col), base_c));
            case kPbCost:
            case kPbDividends:
                return pfmt::money(numeric(h, col), base_c);
            case kPbPnl:
            case kPbPricePnl:
            case kPbFxPnl:
            case kPbDayPnl:
                return approx(fx_stale(h), numeric(h, col), pfmt::smoney(numeric(h, col), base_c));
            case kPbRealized:
                return pfmt::smoney(numeric(h, col), base_c);
            case kPbPnlPct:
            case kPbDayPct:
                return pfmt::pct(numeric(h, col));
            case kPbWeight:
            case kPbRiskShare:
                return pfmt::upct(numeric(h, col), 1);
            case kPbVol:
            case kPbYield:
                return pfmt::upct(numeric(h, col), 1);
            case kPbCompVar:
                return pfmt::upct(numeric(h, col), 2);
            case kPbBeta:
                return pfmt::num(numeric(h, col), 2);
            case kPbTrend:
                return spark_.contains(h.symbol) ? QString() : pfmt::dash();
            case kPbRange52: {
                const double t = numeric(h, col);
                return std::isfinite(t) ? QString() : pfmt::dash();
            }
            case kPbStatus: {
                QStringList parts;
                if (!h.market_state.isEmpty())
                    parts << h.market_state;
                if (!h.as_of.isEmpty())
                    parts << pfmt::date(h.as_of, ui::formatting::DateStyle::Short);
                return parts.isEmpty() ? pfmt::dash() : parts.join(QStringLiteral(" · "));
            }
            case kPbLots:
                return h.ledger_explained ? QString::number(h.open_lots) : pfmt::dash();
            default:
                return {};
        }
    }

    QVariant sort_value(const portfolio::HoldingWithQuote& h, int col) const {
        if (!kPbCols[col].numeric && col != kPbRange52)
            return text_attr(h, col);
        return numeric(h, col);
    }

    QVariant export_value(const portfolio::HoldingWithQuote& h, int col) const {
        switch (col) {
            case kPbTrend:
                return QString();
            case kPbRange52: {
                const auto it = info_.constFind(h.symbol);
                if (it == info_.constEnd())
                    return QString();
                return QStringLiteral("%1-%2").arg(pb_csv_num(it->week52_low, 6), pb_csv_num(it->week52_high, 6));
            }
            case kPbStatus:
                return QStringLiteral("%1 %2").arg(h.market_state, h.as_of).trimmed();
            case kPbQty:
                return pb_csv_num(h.quantity, 8);
            case kPbLast:
            case kPbAvgCost:
            case kPbFx:
                return pb_csv_num(numeric(h, col), 8);
            default:
                if (kPbCols[col].numeric)
                    return pb_csv_num(numeric(h, col), 4);
                return text_attr(h, col);
        }
    }

    QVariant foreground(const portfolio::HoldingWithQuote& h, int col) const {
        switch (col) {
            case kPbSymbol:
                if (!h.valued())
                    return QColor(ui::colors::WARNING());
                return QColor(ui::colors::CYAN());
            case kPbPnl:
            case kPbPnlPct:
            case kPbPricePnl:
            case kPbFxPnl:
            case kPbDayPct:
            case kPbDayPnl:
            case kPbRealized:
                return pfmt::dir_color(numeric(h, col));
            case kPbName:
            case kPbCcy:
            case kPbSector:
            case kPbIndustry:
            case kPbAssetClass:
            case kPbCountry:
            case kPbExchange:
            case kPbStatus:
                return QColor(ui::colors::TEXT_SECONDARY());
            default: {
                const QVariant v = sort_value(h, col);
                if (kPbCols[col].numeric && !std::isfinite(v.toDouble()))
                    return QColor(ui::colors::TEXT_TERTIARY());
                return QColor(ui::colors::TEXT_PRIMARY());
            }
        }
    }

    QVariant tooltip(const portfolio::HoldingWithQuote& h, int col) const {
        if (col == kPbSymbol) {
            QStringList lines;
            lines << (h.name.isEmpty() ? h.symbol : QStringLiteral("%1 — %2").arg(h.symbol, h.name));
            if (!h.priced)
                lines << PortfolioBlotter::tr("No live price: value, P&L and weight are unknown and the position is "
                                              "left out of the totals.");
            else if (!h.fx_ok)
                lines << PortfolioBlotter::tr("No %1→%2 rate yet: base-currency figures are unknown and the position "
                                              "is left out of the totals.")
                             .arg(h.currency, base());
            if (stale(h))
                lines << PortfolioBlotter::tr("Price from the %1 session (others are from %2).")
                             .arg(pfmt::date(h.as_of), pfmt::date(summary_.as_of));
            if (fx_stale(h))
                lines << PortfolioBlotter::tr("The %1→%2 rate is stale: base-currency figures (≈) use the last known "
                                              "rate.")
                             .arg(h.currency, base());
            if (!h.ledger_explained)
                lines << PortfolioBlotter::tr("The ledger does not reproduce this position: cost comes from the stored "
                                              "average price and realised figures may be partial.");
            return lines.join(QLatin1Char('\n'));
        }
        if (col == kPbLots && h.ledger_explained)
            return PortfolioBlotter::tr("%1 open lot(s); %2 units held long-term.")
                .arg(h.open_lots)
                .arg(pfmt::qty(h.long_term_quantity));
        if (col == kPbFxPnl && !std::isfinite(h.fx_pnl) && h.valued())
            return pfmt::fx_pnl_unknown_reason();
        if (col == kPbPricePnl && !std::isfinite(h.fx_pnl) && h.valued())
            return PortfolioBlotter::tr("The whole unrealised P&L, currency move included.") + QLatin1Char('\n') +
                   pfmt::fx_pnl_unknown_reason();
        if (col == kPbValue || col == kPbPnl) {
            if (!h.valued())
                return PortfolioBlotter::tr("Not valued — see the symbol's tooltip.");
        }
        if (col == kPbPnl && h.currency.compare(base(), Qt::CaseInsensitive) != 0 && pfmt::fx_split_shown(summary_)) {
            // Both parts of a foreign holding's P&L (Settings > Currency & Number Format > split).
            QString t = std::isfinite(h.fx_pnl)
                            ? PortfolioBlotter::tr("Price move: %1\nFX move (%2→%3 since purchase): %4")
                                  .arg(pfmt::smoney(h.price_pnl, base()), h.currency, base(),
                                       pfmt::smoney(h.fx_pnl, base()))
                            : PortfolioBlotter::tr("FX move: %1").arg(pfmt::dash()) + QLatin1Char('\n') +
                                  pfmt::fx_pnl_unknown_reason();
            if (live_.contains(h.symbol))
                t += QLatin1Char('\n') + PortfolioBlotter::tr("Split as of the last valuation; live ticks move the "
                                                              "total only.");
            return t;
        }
        return {};
    }

    QVariant total_data(int col, int role) const {
        const auto& s = summary_;
        const QString base_c = base();
        auto num = [&](int c) -> double {
            switch (c) {
                case kPbValue:
                    return pfmt::holdings_value(s);
                case kPbCost:
                    return s.total_cost_basis;
                case kPbPnl:
                    return s.total_unrealized_pnl;
                case kPbPnlPct:
                    return s.total_unrealized_pnl_percent;
                case kPbPricePnl:
                    return s.total_price_pnl;
                case kPbFxPnl:
                    return s.total_fx_pnl;
                case kPbDayPct:
                    return s.total_day_change_percent;
                case kPbDayPnl:
                    return s.total_day_change;
                case kPbWeight:
                    return s.valued_positions > 0 ? 100.0 : portfolio::kAbsent;
                case kPbRealized:
                    return s.total_realized_pnl;
                case kPbDividends:
                    return s.total_dividends;
                default:
                    return portfolio::kAbsent;
            }
        };
        switch (role) {
            case kPbTotalRole:
                return true;
            case kPbSortRole:
                return portfolio::kAbsent;
            case Qt::FontRole: {
                QFont f = ui::fonts::font(-1, QFont::Bold);
                return f;
            }
            case Qt::BackgroundRole:
                return QColor(ui::colors::BG_RAISED());
            case Qt::TextAlignmentRole:
                return QVariant::fromValue(kPbCols[col].numeric ? Qt::AlignRight | Qt::AlignVCenter
                                                                 : Qt::AlignLeft | Qt::AlignVCenter);
            case Qt::ForegroundRole:
                if (col == kPbPnl || col == kPbPnlPct || col == kPbPricePnl || col == kPbFxPnl || col == kPbDayPct ||
                    col == kPbDayPnl || col == kPbRealized)
                    return pfmt::dir_color(num(col));
                if (col == kPbSymbol && !s.complete)
                    return QColor(ui::colors::WARNING());
                return QColor(ui::colors::TEXT_PRIMARY());
            case Qt::ToolTipRole: {
                if (col == kPbSymbol) {
                    QStringList tip;
                    tip << (s.complete ? PortfolioBlotter::tr("Totals of every position, in %1.").arg(base_c)
                                       : pfmt::incomplete_line(s));
                    if (!s.fx_stale.isEmpty())
                        tip << pfmt::fx_stale_line(s);
                    return tip.join(QLatin1Char('\n'));
                }
                if (col == kPbFxPnl && !std::isfinite(s.total_fx_pnl) && s.valued_positions > 0)
                    return PortfolioBlotter::tr("Not every foreign-currency holding has a trade-date FX rate for its "
                                                "cost, so the total cannot be split.");
                if ((col == kPbValue || col == kPbPnl) && !s.fx_stale.isEmpty())
                    return pfmt::fx_stale_line(s);
                return {};
            }
            case kPbExportRole:
                if (col == kPbSymbol)
                    return QStringLiteral("TOTAL");
                return kPbCols[col].numeric ? QVariant(pb_csv_num(num(col), 4)) : QVariant(QString());
            case Qt::DisplayRole:
                switch (col) {
                    case kPbSymbol:
                        return s.complete ? PortfolioBlotter::tr("TOTAL")
                                          : PortfolioBlotter::tr("TOTAL (%1/%2 valued)")
                                                .arg(s.valued_positions)
                                                .arg(s.total_positions);
                    case kPbValue:
                        return approx(!s.fx_stale.isEmpty(), num(col), pfmt::money(num(col), base_c));
                    case kPbCost:
                    case kPbDividends:
                        return pfmt::money(num(col), base_c);
                    case kPbPnl:
                    case kPbPricePnl:
                    case kPbFxPnl:
                    case kPbDayPnl:
                        return approx(!s.fx_stale.isEmpty(), num(col), pfmt::smoney(num(col), base_c));
                    case kPbRealized:
                        return pfmt::smoney(num(col), base_c);
                    case kPbPnlPct:
                    case kPbDayPct:
                        return pfmt::pct(num(col));
                    case kPbWeight:
                        return pfmt::upct(num(col), 1);
                    default:
                        return QString();
                }
            default:
                return {};
        }
    }

    portfolio::PortfolioSummary summary_;
    bool has_summary_ = false;
    QHash<QString, portfolio::HoldingRisk> risk_;
    QHash<QString, QHash<int, QString>> attrs_; // symbol -> dimension -> key
    QSet<QString> class_guessed_;
    QHash<QString, services::InfoData> info_;
    QHash<QString, QVector<double>> spark_;
    QHash<QString, Live> live_;
    ui::formatting::CurrencyMix local_mix_; ///< currencies of VALUE (LOCAL)
    bool prices_need_unit_ = false;         ///< LAST / AVG COST hold a currency other than the base
};

// ── Proxy: text + sector filter, NaN-last sort, totals pinned last ───────────

class PfBlotterProxy : public QSortFilterProxyModel {
  public:
    explicit PfBlotterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
        setSortRole(kPbSortRole);
        setDynamicSortFilter(true);
    }
    void set_text(const QString& t) {
        text_ = t.trimmed().toLower();
        invalidateFilter();
    }
    void set_symbols(const QStringList& syms) {
        symbols_ = QSet<QString>(syms.begin(), syms.end());
        invalidateFilter();
    }
    bool filtered() const { return !text_.isEmpty() || !symbols_.isEmpty(); }

  protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const QModelIndex i = sourceModel()->index(row, kPbSymbol, parent);
        if (i.data(kPbTotalRole).toBool())
            return true;
        const QString sym = i.data(kPbSymbolRole).toString();
        if (!symbols_.isEmpty() && !symbols_.contains(sym))
            return false;
        if (text_.isEmpty())
            return true;
        if (sym.toLower().contains(text_))
            return true;
        return sourceModel()->index(row, kPbName, parent).data(Qt::DisplayRole).toString().toLower().contains(text_);
    }
    bool lessThan(const QModelIndex& l, const QModelIndex& r) const override {
        const bool asc = sortOrder() == Qt::AscendingOrder;
        const bool lt = l.data(kPbTotalRole).toBool();
        const bool rt = r.data(kPbTotalRole).toBool();
        if (lt != rt)
            return asc ? rt : lt;
        const QVariant a = l.data(kPbSortRole);
        const QVariant b = r.data(kPbSortRole);
        if (a.typeId() == QMetaType::Double || b.typeId() == QMetaType::Double) {
            const double x = a.typeId() == QMetaType::Double ? a.toDouble() : portfolio::kAbsent;
            const double y = b.typeId() == QMetaType::Double ? b.toDouble() : portfolio::kAbsent;
            const bool xn = !std::isfinite(x);
            const bool yn = !std::isfinite(y);
            if (xn != yn)
                return asc ? yn : xn; // unknown values always at the bottom
            if (xn)
                return false;
            return x < y;
        }
        const QString sa = a.toString();
        const QString sb = b.toString();
        if (sa.isEmpty() != sb.isEmpty())
            return asc ? sb.isEmpty() : sa.isEmpty();
        return QString::localeAwareCompare(sa, sb) < 0;
    }

  private:
    QString text_;
    QSet<QString> symbols_;
};

// ── Widget ───────────────────────────────────────────────────────────────────

PortfolioBlotter::PortfolioBlotter(QWidget* parent) : QWidget(parent) {
    build_ui();
}

PortfolioBlotter::~PortfolioBlotter() {
    datahub::DataHub::instance().unsubscribe(this);
}

void PortfolioBlotter::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    model_ = new PfBlotterModel(this);
    proxy_ = new PfBlotterProxy(this);
    proxy_->setSourceModel(model_);

    view_ = new QTableView(this);
    view_->setModel(proxy_);
    view_->setSortingEnabled(true);
    view_->sortByColumn(kPbWeight, Qt::DescendingOrder);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setShowGrid(false);
    view_->setWordWrap(false);
    view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->verticalHeader()->setVisible(false);
    ui::metrics::follow_row_height(view_, 28);

    auto* hdr = view_->horizontalHeader();
    hdr->setSectionsMovable(true);
    hdr->setHighlightSections(false);
    hdr->setMinimumSectionSize(48);
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(hdr, &QHeaderView::customContextMenuRequested, this,
            [this, hdr](const QPoint& p) { show_column_menu(hdr->mapToGlobal(p)); });
    connect(hdr, &QHeaderView::sectionMoved, this, [this]() { save_columns(); });
    // Widths are saved when the table is hidden (a drag would otherwise write per pixel).
    connect(hdr, &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
        portfolio::SortColumn c = portfolio::SortColumn::Weight;
        switch (section) {
            case kPbSymbol:
                c = portfolio::SortColumn::Symbol;
                break;
            case kPbLast:
                c = portfolio::SortColumn::Price;
                break;
            case kPbDayPct:
                c = portfolio::SortColumn::Change;
                break;
            case kPbPnl:
                c = portfolio::SortColumn::Pnl;
                break;
            case kPbPnlPct:
                c = portfolio::SortColumn::PnlPct;
                break;
            case kPbValue:
                c = portfolio::SortColumn::MarketValue;
                break;
            default:
                break;
        }
        emit sort_changed(c, order == Qt::AscendingOrder ? portfolio::SortDirection::Asc
                                                         : portfolio::SortDirection::Desc);
    });

    // Visual encodings.
    auto* spark = new ui::SparklineDelegate(view_);
    view_->setItemDelegateForColumn(kPbTrend, spark);
    auto* weight_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, view_);
    weight_bar->set_value_role(kPbSortRole);
    view_->setItemDelegateForColumn(kPbWeight, weight_bar);
    auto* risk_bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, view_);
    risk_bar->set_value_role(kPbSortRole);
    view_->setItemDelegateForColumn(kPbRiskShare, risk_bar);
    auto* range = new ui::RangeBarDelegate(view_);
    range->set_show_labels(false);
    view_->setItemDelegateForColumn(kPbRange52, range);

    connect(view_, &QTableView::clicked, this, [this](const QModelIndex& idx) {
        const QString sym = symbol_at_proxy_row(idx.row());
        if (sym.isEmpty())
            return;
        selected_symbol_ = sym;
        emit symbol_selected(sym);
    });
    connect(view_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const QString sym = symbol_at_proxy_row(idx.row());
        if (!sym.isEmpty())
            emit open_symbol_requested(QStringLiteral("equity_research"), sym);
    });
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QTableView::customContextMenuRequested, this, &PortfolioBlotter::on_context_menu);
    connect(proxy_, &QAbstractItemModel::modelReset, this, [this]() { reselect(); });
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, [this]() { update_footer(); });
    connect(proxy_, &QAbstractItemModel::rowsInserted, this, [this]() { update_footer(); });
    connect(proxy_, &QAbstractItemModel::rowsRemoved, this, [this]() { update_footer(); });

    layout->addWidget(view_, 1);

    footer_ = new QLabel(this);
    footer_->setObjectName("pfBlotterFooter");
    footer_->setContentsMargins(12, 3, 12, 3);
    layout->addWidget(footer_);

    // Default visibility, then any saved layout.
    for (int c = 0; c < kPbColCount; ++c)
        view_->setColumnHidden(c, !kPbCols[c].default_visible);
    restore_columns();
    apply_styles();
    update_footer();

    // A currency / number-format change re-formats in place (the model formats in
    // data()); the FX split setting also decides the PRICE / FX P&L columns.
    ui::formatting::refresh_model_on_format_change(model_);
    ui::formatting::on_format_changed(this, [this]() {
        apply_split_columns();
        update_footer();
    });
}

void PortfolioBlotter::apply_split_columns() {
    const bool want = pfmt::fx_split_shown(model_->summary());
    for (const int c : {static_cast<int>(kPbPricePnl), static_cast<int>(kPbFxPnl)}) {
        const auto it = split_explicit_.constFind(c);
        const bool show = it != split_explicit_.constEnd() ? *it : want; // the user's own choice wins
        if (view_->isColumnHidden(c) != show)
            continue;
        view_->setColumnHidden(c, !show);
        if (show && view_->columnWidth(c) < 40)
            view_->resizeColumnToContents(c);
    }
}

void PortfolioBlotter::save_split_choice() const {
    QStringList parts;
    for (auto it = split_explicit_.constBegin(); it != split_explicit_.constEnd(); ++it)
        parts << QString::fromLatin1(kPbCols[it.key()].key) + (it.value() ? QStringLiteral("=1") : QStringLiteral("=0"));
    SettingsRepository::instance().set(kPbSplitColsKey, parts.join(QLatin1Char(',')), "portfolio");
}

void PortfolioBlotter::apply_styles() {
    // Runs at the end of build_ui() and again from PortfolioScreen::refresh_theme() right
    // after construction: re-applying unchanged text re-styled the table for nothing.
    const auto set_css = [](QWidget* w, const QString& css) {
        if (w && w->styleSheet() != css)
            w->setStyleSheet(css);
    };
    set_css(view_, QString("QTableView { background:%1; color:%2; border:none; font-size:%7px;"
                                 "  font-family:%3; gridline-color:transparent; selection-background-color:%5; }"
                                 "QTableView::item { padding:3px 8px; border-bottom:1px solid %4; }"
                                 "QTableView::item:selected { background:%5; color:%2; }"
                                 "QTableView::item:hover { background:%6; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                  ui::colors::BORDER_DIM(), ui::colors::alpha(ui::colors::AMBER, 0.14),
                                  ui::colors::BG_HOVER(), ui::fonts::px(-1)));
    set_css(view_->horizontalHeader(),
        QString("QHeaderView::section { background:%1; color:%2; border:none; border-bottom:1px solid %3;"
                "  border-right:1px solid %4; padding:4px 8px; font-size:%5px; font-weight:700; letter-spacing:0.5px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                 ui::colors::BORDER_DIM(), ui::fonts::px(-3)));
    set_css(footer_, QString("#pfBlotterFooter { background:%1; color:%2; border-top:1px solid %3;"
                                   "  font-size:%4px; font-weight:600; }")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(),
                                    ui::fonts::px(-4)));
}

void PortfolioBlotter::set_summary(const portfolio::PortfolioSummary& summary) {
    model_->set_summary(summary);
    apply_split_columns();
    if (!columns_restored_ || view_->horizontalHeader()->sectionSize(kPbSymbol) <= 0)
        view_->resizeColumnsToContents();
    update_footer();
    fetch_sparklines();
}

void PortfolioBlotter::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    portfolio::PortfolioSummary s = model_->summary();
    s.holdings = holdings;
    set_summary(s);
}

void PortfolioBlotter::set_analytics(const portfolio::MarketAnalytics& analytics) {
    model_->set_analytics(analytics);
}

void PortfolioBlotter::set_allocation(const portfolio::AllocationReport& report) {
    model_->set_allocation(report);
}

void PortfolioBlotter::set_market_info(const QHash<QString, services::InfoData>& info) {
    model_->set_info(info);
}

int PortfolioBlotter::visible_count() const {
    int n = 0;
    for (int r = 0; r < proxy_->rowCount(); ++r)
        if (!proxy_->index(r, 0).data(kPbTotalRole).toBool())
            ++n;
    return n;
}

void PortfolioBlotter::update_footer() {
    if (!footer_)
        return;
    const int total = model_->holding_count();
    const int shown = visible_count();
    QString t = total == 0 ? tr("No positions yet — use BUY to add your first holding.")
                : proxy_->filtered() ? tr("%1 of %2 positions match the filter").arg(shown).arg(total)
                                     : tr("%n position(s)", "", total);
    const auto& s = model_->summary();
    if (total > 0 && !s.complete)
        t += QStringLiteral("  ·  ") + tr("⚠ %1 not valued (excluded from totals)").arg(s.total_positions - s.valued_positions);
    t += QStringLiteral("  ·  ") + tr("values in %1 · right-click the header to choose columns").arg(model_->base());
    footer_->setText(t);
}

QString PortfolioBlotter::symbol_at_proxy_row(int row) const {
    if (row < 0 || row >= proxy_->rowCount())
        return {};
    const QModelIndex i = proxy_->index(row, kPbSymbol);
    if (i.data(kPbTotalRole).toBool())
        return {};
    return i.data(kPbSymbolRole).toString();
}

void PortfolioBlotter::reselect() {
    update_footer();
    if (selected_symbol_.isEmpty())
        return;
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        if (symbol_at_proxy_row(r) == selected_symbol_) {
            view_->selectRow(r);
            return;
        }
    }
}

void PortfolioBlotter::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        if (symbol_at_proxy_row(r) == symbol) {
            view_->selectRow(r);
            view_->scrollTo(proxy_->index(r, kPbSymbol), QAbstractItemView::EnsureVisible);
            return;
        }
    }
    view_->clearSelection();
}

void PortfolioBlotter::set_filter(const QString& text) {
    proxy_->set_text(text);
    update_footer();
}

void PortfolioBlotter::set_sector_filter(const QStringList& symbols) {
    proxy_->set_symbols(symbols);
    update_footer();
}

// ── Column chooser / persistence ─────────────────────────────────────────────

void PortfolioBlotter::show_column_menu(const QPoint& global_pos) {
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    auto* title = menu.addAction(tr("Columns"));
    title->setEnabled(false);
    menu.addSeparator();
    for (int c = 0; c < kPbColCount; ++c) {
        if (c == kPbSymbol)
            continue; // the row key stays
        // "&" escaped: a menu reads it as a mnemonic ("P&L" showed as "PL").
        auto* a = menu.addAction(tr(kPbCols[c].title).replace(QLatin1Char('&'), QLatin1String("&&")));
        a->setToolTip(tr(kPbCols[c].tip));
        a->setCheckable(true);
        a->setChecked(!view_->isColumnHidden(c));
        connect(a, &QAction::toggled, this, [this, c](bool on) {
            view_->setColumnHidden(c, !on);
            if (on && view_->columnWidth(c) < 40)
                view_->resizeColumnToContents(c);
            if (c == kPbPricePnl || c == kPbFxPnl) {
                split_explicit_.insert(c, on); // from now on the user's choice, not the setting, decides
                save_split_choice();
            }
            save_columns();
        });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Reset to default columns")), &QAction::triggered, this, [this]() {
        for (int c = 0; c < kPbColCount; ++c) {
            view_->setColumnHidden(c, !kPbCols[c].default_visible);
            const int vis = view_->horizontalHeader()->visualIndex(c);
            if (vis != c)
                view_->horizontalHeader()->moveSection(vis, c);
        }
        split_explicit_.clear(); // the split columns follow the setting again
        save_split_choice();
        apply_split_columns();
        view_->resizeColumnsToContents();
        save_columns();
    });
    connect(menu.addAction(tr("Export visible columns (CSV)…")), &QAction::triggered, this,
            &PortfolioBlotter::export_csv);
    menu.exec(global_pos);
}

void PortfolioBlotter::save_columns() const {
    QStringList visible;
    for (int c = 0; c < kPbColCount; ++c)
        if (!view_->isColumnHidden(c))
            visible << QString::fromLatin1(kPbCols[c].key);
    SettingsRepository::instance().set(kPbColumnsKey, visible.join(QLatin1Char(',')), "portfolio");
    SettingsRepository::instance().set(kPbHeaderKey,
                                       QString::fromLatin1(view_->horizontalHeader()->saveState().toBase64()),
                                       "portfolio");
}

void PortfolioBlotter::restore_columns() {
    auto& repo = SettingsRepository::instance();
    auto hr = repo.get(kPbHeaderKey);
    if (hr.is_ok() && !hr.value().isEmpty())
        view_->horizontalHeader()->restoreState(QByteArray::fromBase64(hr.value().toLatin1()));
    auto cr = repo.get(kPbColumnsKey);
    if (cr.is_ok() && !cr.value().isEmpty()) {
        const QStringList keys = cr.value().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (int c = 0; c < kPbColCount; ++c)
            view_->setColumnHidden(c, c != kPbSymbol && !keys.contains(QString::fromLatin1(kPbCols[c].key)));
        columns_restored_ = true;
    }
    view_->setColumnHidden(kPbSymbol, false);
    split_explicit_.clear();
    auto sr = repo.get(kPbSplitColsKey);
    if (sr.is_ok()) {
        for (const QString& part : sr.value().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const QString key = part.section(QLatin1Char('='), 0, 0);
            const bool on = part.section(QLatin1Char('='), 1, 1) == QLatin1String("1");
            for (const int c : {static_cast<int>(kPbPricePnl), static_cast<int>(kPbFxPnl)})
                if (key == QLatin1String(kPbCols[c].key))
                    split_explicit_.insert(c, on);
        }
    }
    apply_split_columns();
}

// ── Export ───────────────────────────────────────────────────────────────────

void PortfolioBlotter::export_csv() {
    if (model_->holding_count() == 0) {
        QMessageBox::information(this, tr("Export Positions"), tr("There are no positions to export."));
        return;
    }
    auto* hdr = view_->horizontalHeader();
    QVector<int> cols;
    for (int v = 0; v < hdr->count(); ++v) {
        const int c = hdr->logicalIndex(v);
        if (!view_->isColumnHidden(c) && c != kPbTrend)
            cols.append(c);
    }
    QStringList headers;
    for (int c : cols) {
        QString h = QString::fromLatin1(kPbCols[c].key);
        // Money columns carry their currency in the header so the file is self-describing.
        if (c == kPbValue || c == kPbCost || c == kPbPnl || c == kPbPricePnl || c == kPbFxPnl || c == kPbDayPnl ||
            c == kPbRealized || c == kPbDividends)
            h += QLatin1Char('_') + model_->base();
        if (c == kPbWeight || c == kPbRiskShare || c == kPbPnlPct || c == kPbDayPct || c == kPbVol || c == kPbYield ||
            c == kPbCompVar)
            h += QStringLiteral("_pct");
        headers << h;
    }
    // Prices (LAST, AVG COST) are in the quote currency and VALUE (LOCAL) in its major
    // currency: name them when the CCY column is not exported (appended, order kept).
    const bool has_ccy = cols.contains(kPbCcy);
    const bool add_price_ccy = !has_ccy && (cols.contains(kPbLast) || cols.contains(kPbAvgCost));
    const bool add_local_ccy = cols.contains(kPbValueLocal);
    if (add_price_ccy)
        headers << QString::fromLatin1(kPbCols[kPbCcy].key);
    if (add_local_ccy)
        headers << QStringLiteral("value_local_ccy");
    QVector<QStringList> rows;
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        QStringList row;
        for (int c : cols)
            row << proxy_->index(r, c).data(kPbExportRole).toString();
        const QString qccy = proxy_->index(r, kPbCcy).data(kPbExportRole).toString(); // "" on the totals row
        if (add_price_ccy)
            row << qccy;
        if (add_local_ccy)
            row << (qccy.isEmpty() ? QString() : ui::formatting::major_currency(qccy));
        rows.append(row);
    }
    ui::ChartExport::save_csv(headers, rows,
                              QStringLiteral("positions_%1").arg(QDate::currentDate().toString(Qt::ISODate)), this);
}

// ── Context menu ─────────────────────────────────────────────────────────────

void PortfolioBlotter::on_context_menu(const QPoint& pos) {
    const QModelIndex idx = view_->indexAt(pos);
    const QString symbol = idx.isValid() ? symbol_at_proxy_row(idx.row()) : QString();

    QMenu menu(this);
    if (!symbol.isEmpty()) {
        auto* sym_label = menu.addAction(symbol);
        sym_label->setEnabled(false);
        sym_label->setFont(ui::fonts::font(-1, QFont::Bold));
        menu.addSeparator();
        connect(menu.addAction(tr("Buy more…")), &QAction::triggered, this, [this, symbol]() { emit buy_requested(symbol); });
        connect(menu.addAction(tr("Sell…")), &QAction::triggered, this, [this, symbol]() { emit sell_requested(symbol); });
        connect(menu.addAction(tr("Record split…")), &QAction::triggered, this,
                [this, symbol]() { emit split_requested(symbol); });
        menu.addSeparator();
        QMenu* open_in = menu.addMenu(tr("Open in"));
        const struct {
            const char* screen;
            const char* label;
        } kTargets[] = {
            {"equity_research", QT_TR_NOOP("Equity Research")}, {"equity_trading", QT_TR_NOOP("Equity Trading")},
            {"news", QT_TR_NOOP("News")},                       {"watchlist", QT_TR_NOOP("Watchlist")},
            {"backtesting", QT_TR_NOOP("Backtesting")},
        };
        for (const auto& t : kTargets) {
            const QString screen = QString::fromLatin1(t.screen);
            connect(open_in->addAction(tr(t.label)), &QAction::triggered, this,
                    [this, screen, symbol]() { emit open_symbol_requested(screen, symbol); });
        }
        connect(menu.addAction(tr("Copy Symbol")), &QAction::triggered, this,
                [symbol]() { QGuiApplication::clipboard()->setText(symbol); });
        menu.addSeparator();
    }
    connect(menu.addAction(tr("Choose columns…")), &QAction::triggered, this,
            [this, pos]() { show_column_menu(view_->viewport()->mapToGlobal(pos)); });
    connect(menu.addAction(tr("Export visible columns (CSV)…")), &QAction::triggered, this,
            &PortfolioBlotter::export_csv);
    if (!symbol.isEmpty()) {
        menu.addSeparator();
        auto* edit_act = menu.addAction(tr("Edit Last Transaction…"));
        auto* delete_act = menu.addAction(tr("Close Position…"));
        QFont f = menu.font();
        f.setBold(true);
        delete_act->setFont(f);
        connect(edit_act, &QAction::triggered, this, [this, symbol]() { emit edit_transaction_requested(symbol); });
        connect(delete_act, &QAction::triggered, this, [this, symbol]() { emit delete_position_requested(symbol); });
    }
    menu.exec(view_->viewport()->mapToGlobal(pos));
}

// ── Live data (sparklines, broker ticks) ─────────────────────────────────────

void PortfolioBlotter::fetch_sparklines() {
    const auto& holdings = model_->summary().holdings;
    if (holdings.isEmpty()) {
        hub_unsubscribe_all();
        return;
    }
    QStringList symbols;
    symbols.reserve(holdings.size());
    for (const auto& h : holdings)
        symbols.append(h.symbol);
    symbols.sort();
    // D3: subscriptions live exactly as long as the table is visible.
    if (!isVisible())
        return;
    if (hub_active_ && symbols == sub_symbols_)
        return;
    hub_resubscribe_sparklines(/*force_refresh=*/true);
}

void PortfolioBlotter::hub_resubscribe_sparklines(bool force_refresh) {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    hub_active_ = false;
    sub_symbols_.clear();
    const auto& holdings = model_->summary().holdings;
    if (holdings.isEmpty())
        return;
    QStringList topics;
    topics.reserve(holdings.size());
    for (const auto& h : holdings) {
        const QString sym = h.symbol;
        const QString topic = QStringLiteral("market:sparkline:") + sym;
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<QVector<double>>())
                return;
            model_->set_spark(sym, v.value<QVector<double>>());
        });
        sub_symbols_.append(sym);
    }
    if (force_refresh)
        hub.request(topics, /*force=*/true);
    hub_active_ = true;
    sub_symbols_.sort();
    subscribe_broker_quotes();
}

void PortfolioBlotter::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
    sub_symbols_.clear();
}

void PortfolioBlotter::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!hub_active_ && model_->holding_count() > 0)
        hub_resubscribe_sparklines(/*force_refresh=*/false);
}

void PortfolioBlotter::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    hub_unsubscribe_all();
    if (model_->holding_count() > 0) {
        save_columns();
        columns_restored_ = true;
    }
}

void PortfolioBlotter::hub_resubscribe_broker_quotes(const QString& broker_account_id) {
    if (broker_account_id_ == broker_account_id && hub_active_)
        return;
    broker_account_id_ = broker_account_id;
    if (hub_active_)
        hub_resubscribe_sparklines(/*force_refresh=*/false);
}

void PortfolioBlotter::subscribe_broker_quotes() {
    const auto& holdings = model_->summary().holdings;
    if (broker_account_id_.isEmpty() || holdings.isEmpty())
        return;
    auto account = trading::AccountManager::instance().get_account(broker_account_id_);
    if (account.broker_id.isEmpty() || account.state != trading::ConnectionState::Connected)
        return;
    auto& hub = datahub::DataHub::instance();
    const QString bid = account.broker_id;
    const QString aid = broker_account_id_;
    for (const auto& h : holdings) {
        // Broker-native ticker: the stored pair when the import recorded one, else
        // the yfinance symbol without its exchange suffix for NSE / BSE listings.
        QString broker_sym;
        if (h.symbol.endsWith(QStringLiteral(".NS")) || h.symbol.endsWith(QStringLiteral(".BO")))
            broker_sym = h.symbol.left(h.symbol.length() - 3);
        else
            continue;
        const QString topic = trading::broker_topic(bid, aid, QStringLiteral("quote"), broker_sym);
        const QString yf_sym = h.symbol;
        hub.subscribe(this, topic, [this, yf_sym](const QVariant& v) {
            if (!v.canConvert<trading::BrokerQuote>())
                return;
            const auto quote = v.value<trading::BrokerQuote>();
            model_->set_live(yf_sym, quote.ltp, quote.change_pct);
        });
    }
}

// ── Theme / language ─────────────────────────────────────────────────────────

void PortfolioBlotter::refresh_theme() {
    apply_styles();
    view_->viewport()->update();
}

void PortfolioBlotter::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioBlotter::retranslateUi() {
    emit model_->headerDataChanged(Qt::Horizontal, 0, kPbColCount - 1);
    update_footer();
}

} // namespace fincept::screens
