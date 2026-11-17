#include "screens/markets/MarketPanelModel.h"

#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketSessions.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "screens/markets/MarketColumns.h"
#include "services/markets/FxRateService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/RangeBarDelegate.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"

#include <QBrush>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>

#include <cmath>
#include <limits>

namespace fincept::screens {

namespace fmt = fincept::ui::formatting;
namespace colors = fincept::ui::colors;
using markets_logic::RowState;
using markets_logic::Session;

namespace {

/// Linked-symbol row wash (spec 3.4) — fraction of the group colour.
constexpr double kLinkedWashAlpha = 0.20;

bool mpm_finite_pos(double v) {
    return std::isfinite(v) && v > 0.0;
}

fmt::InstrumentFmt mpm_inst(const QString& sym, const services::QuoteData& q) {
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(sym, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    return inst;
}

int mpm_decimals(const QString& sym, const services::QuoteData& q) {
    return q.price_digits >= 0 ? q.price_digits : fmt::price_decimals(q.price, mpm_inst(sym, q));
}

/// The row's prices are money in the instrument's own currency: not an index level,
/// an FX rate or a yield (never a currency unit on those), and the feed said which.
bool mpm_is_money(const QString& sym, const services::QuoteData& q, core::instrument::AssetClass cls) {
    using core::instrument::AssetClass;
    return cls != AssetClass::Index && cls != AssetClass::Fx && !q.currency.isEmpty() &&
           fmt::is_money_quote(q.quote_type, sym);
}

/// A price in the instrument's currency ("$189.84", "117.70p", "¥2,845"); never converted.
QString mpm_price(const QString& sym, const services::QuoteData& q, core::instrument::AssetClass cls, double v) {
    if (!mpm_finite_pos(v))
        return fmt::placeholder();
    const fmt::InstrumentFmt inst = mpm_inst(sym, q);
    return mpm_is_money(sym, q, cls) ? fmt::price_with_symbol(v, inst) : fmt::price(v, inst);
}

/// Export cell: C locale, '.', no grouping, never an exponent; empty when missing.
QString mpm_num(double v) {
    return fmt::export_number(v);
}

/// Columns holding a price (or a price change) in the instrument's currency.
bool mpm_is_price_column(int c) {
    return c == mcols::Last || c == mcols::Change || c == mcols::High || c == mcols::Low || c == mcols::Open ||
           c == mcols::PrevClose;
}

QString mpm_retry_line(const QString& reason) {
    const int minutes = markets_logic::retry_minutes(markets_logic::retry_kind_for(reason));
    const QString first =
        minutes > 0
            ? QCoreApplication::translate("MarketPanelModel", "Next retry in about %n minute(s).", nullptr, minutes)
            : QCoreApplication::translate("MarketPanelModel", "Retrying automatically.");
    return first + QLatin1Char(' ') + QCoreApplication::translate("MarketPanelModel", "Press F5 to retry now.");
}

bool mpm_is_text_column(int c) {
    return c == mcols::Name || c == mcols::Ticker || c == mcols::State || c == mcols::Panel;
}

/// The quote of a row that has none (data only; read-only).
const services::QuoteData& mpm_empty_quote() {
    static const services::QuoteData kEmpty{};
    return kEmpty;
}

/// Position of `v` in [lo, hi] (0..1), NaN when the range is unusable.
double mpm_position(double v, double lo, double hi) {
    if (!mpm_finite_pos(v) || !mpm_finite_pos(lo) || !mpm_finite_pos(hi) || !(hi > lo))
        return std::numeric_limits<double>::quiet_NaN();
    return (v - lo) / (hi - lo);
}

} // namespace

// ── MarketPanelModel ────────────────────────────────────────────────────────

MarketPanelModel::MarketPanelModel(MarketsQuoteStore* store, QObject* parent)
    : QAbstractTableModel(parent), store_(store) {
    if (store_) {
        connect(store_, &MarketsQuoteStore::rows_changed, this, &MarketPanelModel::on_rows_changed);
        connect(store_, &MarketsQuoteStore::flash_tick, this, &MarketPanelModel::on_flash_tick);
        connect(store_, &MarketsQuoteStore::linked_symbol_changed, this, &MarketPanelModel::on_linked_changed);
        linked_ = store_->linked_symbol();
    }
    // Cells format in data(): a currency / number-format change only needs a repaint.
    fmt::refresh_model_on_format_change(this);
}

void MarketPanelModel::rebuild_index() {
    row_index_.clear();
    for (int i = 0; i < symbols_.size(); ++i)
        row_index_.insert(symbols_.at(i), i);
}

void MarketPanelModel::set_symbols(const QStringList& symbols) {
    if (symbols == symbols_)
        return;
    beginResetModel(); // a panel edit, never a tick
    symbols_ = symbols;
    symbols_.removeDuplicates();
    rebuild_index();
    session_.clear();
    for (const QString& s : std::as_const(symbols_))
        update_session(s);
    endResetModel();
}

void MarketPanelModel::insert_symbol(const QString& symbol, int at) {
    if (symbol.isEmpty() || row_index_.contains(symbol))
        return;
    at = std::clamp(at < 0 ? static_cast<int>(symbols_.size()) : at, 0, static_cast<int>(symbols_.size()));
    beginInsertRows({}, at, at);
    symbols_.insert(at, symbol);
    rebuild_index();
    update_session(symbol);
    endInsertRows();
}

void MarketPanelModel::remove_symbol(const QString& symbol) {
    const int r = row_of(symbol);
    if (r < 0)
        return;
    beginRemoveRows({}, r, r);
    symbols_.removeAt(r);
    rebuild_index();
    session_.remove(symbol);
    endRemoveRows();
}

void MarketPanelModel::move_row(int from, int to) {
    const int n = static_cast<int>(symbols_.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    // beginMoveRows wants the destination as "insert before" in the old numbering.
    const int dest = to > from ? to + 1 : to;
    if (!beginMoveRows({}, from, from, {}, dest))
        return;
    symbols_.move(from, to);
    rebuild_index();
    endMoveRows();
}

void MarketPanelModel::set_link_group(fincept::SymbolGroup group) {
    if (link_group_ == group)
        return;
    link_group_ = group;
    const int r = row_of(linked_);
    if (r >= 0)
        emit dataChanged(index(r, 0), index(r, mcols::count() - 1), {Qt::BackgroundRole, LinkedRole});
}

void MarketPanelModel::set_panel_names(const QHash<QString, QString>& names) {
    panel_names_ = names;
    if (!symbols_.isEmpty())
        emit dataChanged(index(0, mcols::Panel), index(rowCount() - 1, mcols::Panel));
}

void MarketPanelModel::set_notes(const QHash<QString, QString>& notes) {
    notes_ = notes;
    if (!symbols_.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount() - 1, mcols::count() - 1));
}

void MarketPanelModel::refresh_all() {
    for (const QString& s : std::as_const(symbols_))
        update_session(s);
    if (!symbols_.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount() - 1, mcols::count() - 1));
    emit headerDataChanged(Qt::Horizontal, 0, mcols::count() - 1);
}

void MarketPanelModel::update_session(const QString& symbol) {
    const MarketRow* r = store_ ? store_->row(symbol) : nullptr;
    Session s = r ? markets_logic::session_from_state(r->quote.market_state) : Session::Unknown;
    if (s == Session::Unknown || s == Session::Closed) {
        // The feed's state is missing, or says closed: the venue calendar knows holidays.
        if (const auto* venue = core::market::venue_for_symbol(symbol, r ? r->quote.exchange : QString())) {
            const auto st = core::market::session_status(*venue, QDateTime::currentDateTimeUtc());
            using core::market::SessionState;
            if (st.state == SessionState::Holiday)
                s = Session::Holiday;
            else if (s == Session::Unknown && r && r->has_quote) {
                switch (st.state) {
                    case SessionState::Open:
                        s = Session::Regular;
                        break;
                    case SessionState::Pre:
                    case SessionState::Post:
                    case SessionState::Lunch:
                        s = Session::Extended;
                        break;
                    default:
                        s = Session::Closed;
                        break;
                }
            }
        }
    }
    session_.insert(symbol, static_cast<int>(s));
}

void MarketPanelModel::on_rows_changed(const QSet<QString>& symbols) {
    if (symbols_.isEmpty())
        return;
    const int last_col = mcols::count() - 1;
    for (const QString& s : symbols) {
        const int r = row_of(s);
        if (r < 0)
            continue;
        update_session(s);
        emit dataChanged(index(r, 0), index(r, last_col));
    }
}

void MarketPanelModel::on_flash_tick(const QSet<QString>& symbols) {
    for (const QString& s : symbols) {
        const int r = row_of(s);
        if (r >= 0)
            emit dataChanged(index(r, mcols::Last), index(r, mcols::ChangePct), {FlashRole});
    }
}

void MarketPanelModel::on_linked_changed(const QString& symbol) {
    const QString old = linked_;
    linked_ = symbol;
    const int last_col = mcols::count() - 1;
    for (const QString& s : {old, symbol}) {
        const int r = row_of(s);
        if (r >= 0)
            emit dataChanged(index(r, 0), index(r, last_col), {Qt::BackgroundRole, LinkedRole});
    }
}

int MarketPanelModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(symbols_.size());
}

int MarketPanelModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : mcols::count();
}

Qt::ItemFlags MarketPanelModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QVariant MarketPanelModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || section < 0 || section >= mcols::count())
        return {};
    switch (role) {
        case Qt::DisplayRole:
            return mcols::header(section);
        case Qt::ToolTipRole:
            return mcols::tooltip(section);
        case Qt::TextAlignmentRole:
            return static_cast<int>((mpm_is_text_column(section) ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
        default:
            return {};
    }
}

QString MarketPanelModel::session_text(const QString& symbol, const MarketRow* r) const {
    const QString state = r ? r->quote.market_state : QString();
    if (state == QLatin1String("PRE"))
        return tr("Pre-market");
    if (state == QLatin1String("POST"))
        return tr("Post-market");
    switch (static_cast<Session>(session_.value(symbol, static_cast<int>(Session::Unknown)))) {
        case Session::Regular:
            return tr("Open");
        case Session::Extended:
            return tr("Extended hours");
        case Session::Closed:
            return tr("Closed");
        case Session::Holiday:
            return tr("Holiday");
        case Session::Unknown:
            break;
    }
    return {};
}

QString MarketPanelModel::row_tooltip(const QString& symbol, const MarketRow* r) const {
    const QString name = store_ ? store_->display_name(symbol) : symbol;
    QString ticker = core::instrument::display_symbol(symbol);
    if (ticker.isEmpty())
        ticker = symbol;
    QStringList lines;
    lines << (name == ticker ? ticker : QStringLiteral("%1  (%2)").arg(name, ticker));
    if (r && r->has_quote) {
        QStringList facts;
        if (!r->quote.exchange_name.isEmpty())
            facts << r->quote.exchange_name;
        const QString sess = session_text(symbol, r);
        if (!sess.isEmpty())
            facts << sess;
        const QTimeZone tz = r->quote.exchange_timezone.isEmpty()
                                 ? QTimeZone()
                                 : QTimeZone(r->quote.exchange_timezone.toUtf8());
        if (r->quote.market_time > 0)
            facts << tr("as of %1").arg(fmt::date_time(r->quote.market_time, fmt::DateStyle::Medium, false, tz));
        else if (!r->quote.market_date.isEmpty())
            facts << tr("as of %1").arg(r->quote.market_date);
        if (!facts.isEmpty())
            lines << facts.join(QStringLiteral(" · "));
    }
    if (!notes_.value(symbol).isEmpty())
        lines << notes_.value(symbol);
    if (!r || !store_)
        return lines.join(QLatin1Char('\n'));
    switch (store_->row_state(*r)) {
        case RowState::Loading:
            lines << tr("Waiting for the first quote");
            break;
        case RowState::Closed:
            if (r->quote.market_time > 0)
                lines << tr("%1 · last trade %2")
                             .arg(session_text(symbol, r),
                                  fmt::date_time(r->quote.market_time, fmt::DateStyle::Medium, false,
                                                 r->quote.exchange_timezone.isEmpty()
                                                     ? QTimeZone()
                                                     : QTimeZone(r->quote.exchange_timezone.toUtf8())));
            break;
        case RowState::Stale:
            lines << tr("Stale: last refreshed %1.").arg(fmt::relative(r->quote_ms / 1000));
            break;
        case RowState::FailedOld:
            lines << tr("Last refresh failed - showing the last quote.\n%1\n%2").arg(r->error, mpm_retry_line(r->error));
            break;
        case RowState::FailedNoData:
            lines << tr("No quote: %1\n%2").arg(r->error, mpm_retry_line(r->error));
            break;
        case RowState::Ok:
            break;
    }
    return lines.join(QLatin1Char('\n'));
}

QVariant MarketPanelModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= symbols_.size())
        return {};
    const int c = index.column();
    const QString& sym = symbols_.at(index.row());
    const MarketRow* r = store_ ? store_->row(sym) : nullptr;
    const bool has = r && r->has_quote;
    const services::QuoteData& q = has ? r->quote : mpm_empty_quote();
    const auto cls = r ? r->cls : core::instrument::asset_class_of(sym);
    const bool na = has && !mcols::applies(c, cls, q);

    switch (role) {
        case SymbolRole:
            return sym;
        case LinkedRole:
            return !linked_.isEmpty() && sym == linked_;
        case NaRole:
            return na;
        case StateRole:
            return static_cast<int>(r && store_ ? store_->row_state(*r) : RowState::Loading);
        case SessionRole:
            return session_.value(sym, static_cast<int>(Session::Unknown));
        case FailedRole:
            return r && !r->error.isEmpty();
        case NoteRole:
            return notes_.value(sym);
        case CurrencyRole:
            return has && mpm_is_money(sym, q, cls) ? q.currency : QString();
        case TickerRole: {
            const QString d = core::instrument::display_symbol(sym);
            return d.isEmpty() ? sym : d;
        }
        case PriceDecimalsRole:
            return has ? mpm_decimals(sym, q) : -1;
        case BaselineRole:
            return has && std::isfinite(q.previous_close) ? QVariant(q.previous_close) : QVariant();
        case FlashRole: {
            if (!r || !store_ || (c != mcols::Last && c != mcols::Change && c != mcols::ChangePct))
                return 0.0;
            return store_->row_state(*r) == RowState::Ok ? store_->flash_strength(*r) : 0.0;
        }
        case Qt::BackgroundRole: {
            if (linked_.isEmpty() || sym != linked_)
                return {};
            QColor g = link_group_ != fincept::SymbolGroup::None
                           ? fincept::SymbolGroupRegistry::instance().color(link_group_)
                           : QColor(colors::AMBER());
            g.setAlphaF(static_cast<float>(kLinkedWashAlpha));
            return QBrush(g);
        }
        case Qt::TextAlignmentRole:
            return static_cast<int>((mpm_is_text_column(c) ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
        case Qt::ToolTipRole:
            if (na)
                return tr("Not reported for %1.").arg(mcols::class_word(cls));
            return row_tooltip(sym, r);
        default:
            break;
    }

    if (role == ui::SparklineDelegate::kDefaultRole) {
        if (c == mcols::Spark && r && !r->spark.isEmpty())
            return QVariant::fromValue(r->spark);
        return {};
    }
    if (role == ui::RangeBarDelegate::LowRole || role == ui::RangeBarDelegate::HighRole ||
        role == ui::RangeBarDelegate::ValueRole) {
        if (!has)
            return {};
        double lo = 0, hi = 0;
        if (c == mcols::DayRange) {
            lo = q.low;
            hi = q.high;
        } else if (c == mcols::Range52w) {
            lo = q.fifty_two_week_low;
            hi = q.fifty_two_week_high;
        } else {
            return {};
        }
        if (!mpm_finite_pos(lo) || !mpm_finite_pos(hi))
            return {};
        if (role == ui::RangeBarDelegate::LowRole)
            return lo;
        if (role == ui::RangeBarDelegate::HighRole)
            return hi;
        return q.price;
    }

    const RowState state = r && store_ ? store_->row_state(*r) : RowState::Loading;
    const bool stale = state == RowState::Stale || state == RowState::FailedOld;

    if (role == Qt::ForegroundRole) {
        if (na)
            return QBrush(QColor(colors::TEXT_TERTIARY()));
        switch (c) {
            case mcols::Name:
                return QBrush(QColor(!has || stale ? colors::TEXT_TERTIARY() : colors::TEXT_PRIMARY()));
            case mcols::Last:
                if (state == RowState::Ok)
                    return QBrush(QColor(colors::TEXT_PRIMARY()));
                if (state == RowState::Closed)
                    return QBrush(QColor(colors::TEXT_SECONDARY()));
                return QBrush(QColor(colors::TEXT_TERTIARY()));
            case mcols::Change:
            case mcols::ChangePct: {
                if (!has || stale)
                    return QBrush(QColor(colors::TEXT_TERTIARY()));
                const int d = markets_logic::direction_of(q.change);
                return QBrush(QColor(d > 0 ? colors::POSITIVE() : (d < 0 ? colors::NEGATIVE() : colors::TEXT_SECONDARY())));
            }
            default:
                return QBrush(QColor(!has || stale ? colors::TEXT_TERTIARY() : colors::TEXT_SECONDARY()));
        }
    }

    if (role == Qt::DisplayRole) {
        if (c == mcols::Name)
            return store_ ? store_->display_name(sym) : sym;
        if (c == mcols::Ticker) {
            const QString d = core::instrument::display_symbol(sym);
            return d.isEmpty() ? sym : d;
        }
        if (c == mcols::Panel)
            return panel_names_.value(sym);
        if (!has) {
            if (c == mcols::Last)
                return (r && !r->error.isEmpty()) ? QStringLiteral("-") : QStringLiteral("...");
            return QString();
        }
        if (na)
            return tr("n/a");
        switch (c) {
            case mcols::Last:
                return mpm_price(sym, q, cls, q.price);
            case mcols::Change:
                return fmt::price_change(q.change, q.price, mpm_inst(sym, q));
            case mcols::ChangePct:
                return fmt::pct(q.change_pct);
            case mcols::Spark:
                return r->spark_state == MarketRow::Spark::Failed ? fmt::placeholder() : QString();
            case mcols::DayRange:
                return mpm_finite_pos(q.low) && mpm_finite_pos(q.high)
                           ? QStringLiteral("%1 - %2").arg(mpm_price(sym, q, cls, q.low), mpm_price(sym, q, cls, q.high))
                           : fmt::placeholder();
            case mcols::Range52w:
                return mpm_finite_pos(q.fifty_two_week_low) && mpm_finite_pos(q.fifty_two_week_high)
                           ? QStringLiteral("%1 - %2").arg(mpm_price(sym, q, cls, q.fifty_two_week_low),
                                                           mpm_price(sym, q, cls, q.fifty_two_week_high))
                           : fmt::placeholder();
            case mcols::Volume:
                return q.volume > 0.0 ? fmt::volume(q.volume) : fmt::placeholder();
            case mcols::RelVolume:
                return q.volume > 0.0 && mpm_finite_pos(q.average_volume) ? fmt::ratio(q.volume / q.average_volume)
                                                                          : fmt::placeholder();
            case mcols::High:
                return mpm_price(sym, q, cls, q.high);
            case mcols::Low:
                return mpm_price(sym, q, cls, q.low);
            case mcols::Open:
                return mpm_price(sym, q, cls, q.open);
            case mcols::PrevClose:
                return mpm_price(sym, q, cls, q.previous_close);
            case mcols::State: {
                const QString s = session_text(sym, r);
                return s.isEmpty() ? fmt::placeholder() : s;
            }
            default:
                return QString();
        }
    }

    if (role == Qt::EditRole) {
        if (!has || na)
            return {};
        switch (c) {
            case mcols::Last:
                return q.price;
            case mcols::Change:
                return q.change;
            case mcols::ChangePct:
                return q.change_pct;
            case mcols::Volume:
                return q.volume > 0.0 ? QVariant(q.volume) : QVariant();
            case mcols::RelVolume:
                return q.volume > 0.0 && mpm_finite_pos(q.average_volume) ? QVariant(q.volume / q.average_volume)
                                                                          : QVariant();
            case mcols::High:
                return mpm_finite_pos(q.high) ? QVariant(q.high) : QVariant();
            case mcols::Low:
                return mpm_finite_pos(q.low) ? QVariant(q.low) : QVariant();
            case mcols::Open:
                return mpm_finite_pos(q.open) ? QVariant(q.open) : QVariant();
            case mcols::PrevClose:
                return mpm_finite_pos(q.previous_close) ? QVariant(q.previous_close) : QVariant();
            default:
                return {};
        }
    }

    if (role == SortRole) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        switch (c) {
            case mcols::Name:
                return (store_ ? store_->display_name(sym) : sym).toLower();
            case mcols::Ticker: {
                const QString d = core::instrument::display_symbol(sym);
                return (d.isEmpty() ? sym : d).toLower();
            }
            case mcols::Panel:
                return panel_names_.value(sym).toLower();
            case mcols::State:
                return session_.value(sym, static_cast<int>(Session::Unknown));
            default:
                break;
        }
        if (!has)
            return nan;
        switch (c) {
            case mcols::Last:
                return q.price;
            case mcols::Change:
                return q.change;
            case mcols::ChangePct:
                return q.change_pct;
            case mcols::Spark:
                return r->spark.size() >= 2 && r->spark.first() > 0 ? r->spark.last() / r->spark.first() - 1.0 : nan;
            case mcols::DayRange:
                return mpm_position(q.price, q.low, q.high);
            case mcols::Range52w:
                return mpm_position(q.price, q.fifty_two_week_low, q.fifty_two_week_high);
            case mcols::Volume:
                return q.volume > 0.0 ? q.volume : nan;
            case mcols::RelVolume:
                return q.volume > 0.0 && mpm_finite_pos(q.average_volume) ? q.volume / q.average_volume : nan;
            case mcols::High:
                return mpm_finite_pos(q.high) ? q.high : nan;
            case mcols::Low:
                return mpm_finite_pos(q.low) ? q.low : nan;
            case mcols::Open:
                return mpm_finite_pos(q.open) ? q.open : nan;
            case mcols::PrevClose:
                return mpm_finite_pos(q.previous_close) ? q.previous_close : nan;
            default:
                return nan;
        }
    }
    return {};
}

QStringList MarketPanelModel::export_headers(int column) const {
    if (column == mcols::DayRange || column == mcols::Range52w) {
        const QString id = mcols::id_at(column);
        return {id + QStringLiteral("_low"), id + QStringLiteral("_high")};
    }
    if (column == mcols::Name) // NAME is always exported first: CURRENCY lines up across panels
        return {mcols::header(column), tr("SYMBOL"), tr("CURRENCY")};
    return {mcols::header(column)};
}

QStringList MarketPanelModel::export_cells(int row, int column) const {
    const QString sym = symbol_at(row);
    const MarketRow* r = store_ ? store_->row(sym) : nullptr;
    const bool has = r && r->has_quote;
    const services::QuoteData& q = has ? r->quote : mpm_empty_quote();
    auto pos = [](double v) { return mpm_finite_pos(v) ? mpm_num(v) : QString(); };
    switch (column) {
        case mcols::Name: {
            // ISO code of the row's price columns, as quoted ("GBp": the prices are in pence);
            // empty for index levels and FX rates, which are not money.
            const QString ccy = has && mpm_is_money(sym, q, r->cls) ? q.currency : QString();
            return {store_ ? store_->display_name(sym) : sym, sym, ccy};
        }
        case mcols::Ticker: {
            const QString d = core::instrument::display_symbol(sym);
            return {d.isEmpty() ? sym : d};
        }
        case mcols::Panel:
            return {panel_names_.value(sym)};
        case mcols::State:
            return {session_text(sym, r)};
        case mcols::DayRange:
            return {has ? pos(q.low) : QString(), has ? pos(q.high) : QString()};
        case mcols::Range52w:
            return {has ? pos(q.fifty_two_week_low) : QString(), has ? pos(q.fifty_two_week_high) : QString()};
        default:
            break;
    }
    if (!has || !mcols::applies(column, r->cls, q))
        return {QString()};
    switch (column) {
        case mcols::Last:
            return {pos(q.price)};
        case mcols::Change:
            return {mpm_num(q.change)};
        case mcols::ChangePct:
            return {mpm_num(q.change_pct)};
        case mcols::Spark:
            return {r->spark.size() >= 2 && r->spark.first() > 0
                        ? mpm_num((r->spark.last() / r->spark.first() - 1.0) * 100.0)
                        : QString()};
        case mcols::Volume:
            return {q.volume > 0.0 ? mpm_num(q.volume) : QString()};
        case mcols::RelVolume:
            return {q.volume > 0.0 && mpm_finite_pos(q.average_volume) ? mpm_num(q.volume / q.average_volume)
                                                                       : QString()};
        case mcols::High:
            return {pos(q.high)};
        case mcols::Low:
            return {pos(q.low)};
        case mcols::Open:
            return {pos(q.open)};
        case mcols::PrevClose:
            return {pos(q.previous_close)};
        default:
            return {QString()};
    }
}

// ── MarketPanelProxy ────────────────────────────────────────────────────────

MarketPanelProxy::MarketPanelProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(MarketPanelModel::SortRole);
    setDynamicSortFilter(false); // never re-sort on a tick
}

void MarketPanelProxy::sort(int column, Qt::SortOrder order) {
    usd_factor_.clear();
    normalise_ = false;
    if (column >= 0 && mpm_is_price_column(column) && sourceModel()) {
        // One currency (the usual panel): raw prices sort exactly as before, no FX needed.
        fmt::CurrencyMix mix;
        const int n = sourceModel()->rowCount();
        for (int r = 0; r < n && !mix.mixed(); ++r)
            mix.add(sourceModel()->index(r, column).data(MarketPanelModel::CurrencyRole).toString());
        normalise_ = mix.mixed();
    }
    QSortFilterProxyModel::sort(column, order);
}

double MarketPanelProxy::usd_per_unit(const QString& ccy) const {
    const auto it = usd_factor_.constFind(ccy);
    if (it != usd_factor_.constEnd())
        return it.value();
    // Per unit AS QUOTED (a pence price is scaled to pounds first); NaN when no rate is
    // known yet (the missing leg is requested and a later re-sort picks it up).
    const double f = services::fx::convert_for_display(1.0, ccy, QStringLiteral("USD")).amount;
    usd_factor_.insert(ccy, f);
    return f;
}

bool MarketPanelProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    const QVariant a = left.data(MarketPanelModel::SortRole);
    const QVariant b = right.data(MarketPanelModel::SortRole);
    if (a.typeId() == QMetaType::QString || b.typeId() == QMetaType::QString)
        return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
    const bool a_na = left.data(MarketPanelModel::NaRole).toBool();
    const bool b_na = right.data(MarketPanelModel::NaRole).toBool();
    bool ok_a = false, ok_b = false;
    double da = a.toDouble(&ok_a);
    double db = b.toDouble(&ok_b);
    if (!ok_a)
        da = std::numeric_limits<double>::quiet_NaN();
    if (!ok_b)
        db = std::numeric_limits<double>::quiet_NaN();
    if (normalise_ && mpm_is_price_column(left.column())) {
        // Prices in different currencies are compared in USD; a row without a rate sorts with
        // the missing values. A level (index, FX rate, yield) has no currency and keeps its scale.
        const QString ca = left.data(MarketPanelModel::CurrencyRole).toString();
        const QString cb = right.data(MarketPanelModel::CurrencyRole).toString();
        if (!ca.isEmpty())
            da *= usd_per_unit(ca);
        if (!cb.isEmpty())
            db *= usd_per_unit(cb);
    }
    return markets_logic::compare_keys(da, a_na, db, b_na, sortOrder() == Qt::DescendingOrder) < 0;
}

} // namespace fincept::screens
