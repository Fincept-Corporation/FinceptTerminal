#include "screens/screener/ScreenerResultsModel.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QCoreApplication>
#include <QFont>

#include <cmath>

namespace fincept::screens {

// The user's number format (Settings > Formatting: grouping / decimal marks, compact scheme) through
// fincept::ui::formatting — this used to be a fixed en_US formatter. The view re-reads the cells
// when that format changes (refresh_model_on_format_change in the constructor).
namespace screener_format {

namespace sfmt = fincept::ui::formatting;

QString fixed(double v, int decimals) {
    if (std::isnan(v))
        return sfmt::placeholder();
    return sfmt::number(v, decimals);
}

QString price(double v) {
    if (std::isnan(v))
        return sfmt::placeholder();
    return sfmt::price(v);
}

QString compact(double v) {
    if (std::isnan(v))
        return sfmt::placeholder();
    return sfmt::compact(v, sfmt::Compact::FromSettings);
}

QString percent(double v, bool with_sign) {
    if (std::isnan(v))
        return sfmt::placeholder();
    return sfmt::pct(v, 2, with_sign);
}

QString by_unit(double v, const QString& unit) {
    if (std::isnan(v))
        return sfmt::placeholder();
    if (unit == QLatin1String("pct"))
        return percent(v, false);
    if (unit == QLatin1String("price") || unit == QLatin1String("per_share"))
        return price(v);
    if (unit == QLatin1String("mcap") || unit == QLatin1String("money") || unit == QLatin1String("shares"))
        return compact(v);
    if (unit == QLatin1String("years"))
        return fixed(v, 0);
    return fixed(v, 2); // ratio, days, score
}

} // namespace screener_format

namespace {

QVariant number_or_invalid(double v) {
    return std::isnan(v) ? QVariant() : QVariant(v);
}

QVariant text_or_invalid(const QString& s) {
    return s.isEmpty() ? QVariant() : QVariant(s.toLower());
}

QVariant signed_colour(double v) {
    if (std::isnan(v) || v == 0.0)
        return QColor(ui::colors::TEXT_SECONDARY());
    return QColor(v > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
}

/// Extra columns whose sign carries meaning (moves and growth rates) get green/red.
bool is_signed_metric(const services::ScreenerColumn& c) {
    return c.unit == QLatin1String("pct") && (c.field.contains(QLatin1String("change")) ||
                                               c.field.contains(QLatin1String("growth")));
}

QString exact(double v) {
    return screener_format::fixed(v, 0);
}

QString model_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::ScreenerResultsModel", text);
}

} // namespace

ScreenerResultsModel::ScreenerResultsModel(QObject* parent) : ui::LiveTableModel<services::ScreenerRow>(parent) {
    set_key([](const services::ScreenerRow& r) { return r.symbol; });
    set_cell([this](const services::ScreenerRow& r, int col, int role) { return cell(r, col, role); });
    // Cells are formatted per the user's number format: re-read them when it changes.
    fincept::ui::formatting::refresh_model_on_format_change(this);
}

void ScreenerResultsModel::set_base_headers(const QStringList& headers, const QStringList& tooltips) {
    base_headers_ = headers;
    base_tooltips_ = tooltips;
    apply_headers();
}

void ScreenerResultsModel::apply_headers() {
    QStringList all = base_headers_;
    while (all.size() < kBaseColumnCount)
        all.append(QString());
    for (const auto& c : extra_)
        all.append(c.label.toUpper());
    set_headers(all);
}

void ScreenerResultsModel::set_result(const QVector<services::ScreenerRow>& rows,
                                      const QVector<services::ScreenerColumn>& extra) {
    bool same_columns = extra.size() == extra_.size();
    for (int i = 0; same_columns && i < extra.size(); ++i)
        same_columns = extra[i].id == extra_[i].id;
    if (!same_columns) {
        // A row diff would compare old rows through the NEW column set; start clean instead.
        clear();
        extra_ = extra;
        apply_headers();
    }
    upsert(rows);
}

bool ScreenerResultsModel::is_numeric_column(int col) const {
    if (col >= kBaseColumnCount)
        return true;
    switch (col) {
        case ColPrice:
        case ColChangePct:
        case ColVolume:
        case ColMarketCap:
        case ColPe:
            return true;
        default:
            return false;
    }
}

QVariant ScreenerResultsModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o == Qt::Horizontal && role == Qt::ToolTipRole) {
        if (section >= 0 && section < kBaseColumnCount)
            return section < base_tooltips_.size() ? QVariant(base_tooltips_.at(section)) : QVariant();
        const int i = section - kBaseColumnCount;
        if (i >= 0 && i < extra_.size())
            return extra_[i].description;
        return {};
    }
    if (o == Qt::Horizontal && role == Qt::TextAlignmentRole)
        return int((is_numeric_column(section) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    return ui::LiveTableModel<services::ScreenerRow>::headerData(section, o, role);
}

QVariant ScreenerResultsModel::cell(const services::ScreenerRow& r, int col, int role) const {
    if (role == SearchRole)
        return col == ColSymbol ? QVariant(r.symbol + QLatin1Char(' ') + r.name + QLatin1Char(' ') + r.sector +
                                           QLatin1Char(' ') + r.industry)
                                : QVariant();
    if (role == Qt::TextAlignmentRole)
        return int((is_numeric_column(col) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    if (col < kBaseColumnCount)
        return base_cell(r, col, role);
    const int i = col - kBaseColumnCount;
    return i < extra_.size() ? extra_cell(r, extra_[i], role) : QVariant();
}

QVariant ScreenerResultsModel::base_cell(const services::ScreenerRow& r, int col, int role) const {
    namespace fmt = screener_format;
    switch (col) {
        case ColSymbol:
            if (role == Qt::DisplayRole)
                return r.symbol;
            if (role == SortRole)
                return text_or_invalid(r.symbol);
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::INFO());
            if (role == Qt::FontRole) {
                QFont f;
                f.setBold(true); // resolved against the view font, so only the weight changes
                return f;
            }
            return {};
        case ColName:
            if (role == Qt::DisplayRole)
                return r.name;
            if (role == SortRole)
                return text_or_invalid(r.name);
            if (role == Qt::ToolTipRole)
                return r.industry.isEmpty() ? r.name : r.name + QLatin1Char('\n') + r.industry;
            return {};
        case ColPrice:
            if (role == Qt::DisplayRole)
                return fmt::price(r.price);
            if (role == SortRole)
                return number_or_invalid(r.price);
            if (role == Qt::ToolTipRole && !std::isnan(r.price))
                return fmt::price(r.price) + QLatin1Char(' ') + r.currency;
            return {};
        case ColChangePct:
            if (role == Qt::DisplayRole)
                return fmt::percent(r.change_pct, true);
            if (role == SortRole)
                return number_or_invalid(r.change_pct);
            if (role == Qt::ForegroundRole)
                return signed_colour(r.change_pct);
            if (role == Qt::ToolTipRole && !std::isnan(r.change))
                return (r.change > 0 ? QStringLiteral("+") : QString()) + fmt::price(r.change) + QLatin1Char(' ') +
                       r.currency;
            return {};
        case ColVolume:
            if (role == Qt::DisplayRole)
                return fmt::compact(r.volume);
            if (role == SortRole)
                return number_or_invalid(r.volume);
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::TEXT_SECONDARY());
            if (role == Qt::ToolTipRole && !std::isnan(r.volume))
                return std::isnan(r.avg_volume_3m)
                           ? exact(r.volume)
                           : model_tr("%1 (3M avg %2)").arg(exact(r.volume), fmt::compact(r.avg_volume_3m));
            return {};
        case ColMarketCap:
            if (role == Qt::DisplayRole)
                return fmt::compact(r.market_cap);
            if (role == SortRole)
                return number_or_invalid(r.market_cap);
            if (role == Qt::ToolTipRole && !std::isnan(r.market_cap))
                return exact(r.market_cap) + QLatin1Char(' ') + r.market_cap_currency;
            return {};
        case ColPe:
            if (role == Qt::DisplayRole)
                return fmt::fixed(r.pe, 2);
            if (role == SortRole)
                return number_or_invalid(r.pe);
            if (role == Qt::ToolTipRole && !std::isnan(r.forward_pe))
                return model_tr("Forward P/E %1").arg(fmt::fixed(r.forward_pe, 2));
            return {};
        case ColSector:
            if (role == Qt::DisplayRole)
                return r.sector.isEmpty() ? QStringLiteral("—") : r.sector;
            if (role == SortRole)
                return text_or_invalid(r.sector);
            if (role == Qt::ToolTipRole)
                return r.industry;
            return {};
        case ColExchange:
            if (role == Qt::DisplayRole)
                return r.exchange_name;
            if (role == SortRole)
                return text_or_invalid(r.exchange_name);
            if (role == Qt::ToolTipRole)
                return r.exchange;
            return {};
        case ColCurrency:
            if (role == Qt::DisplayRole)
                return r.currency;
            if (role == SortRole)
                return text_or_invalid(r.currency);
            if (role == Qt::ForegroundRole)
                return QColor(ui::colors::TEXT_TERTIARY());
            return {};
        default:
            return {};
    }
}

QVariant ScreenerResultsModel::extra_cell(const services::ScreenerRow& r, const services::ScreenerColumn& c,
                                          int role) const {
    const double v = r.fields.value(c.id, services::ScreenerRow::kMissing);
    const bool is_signed = is_signed_metric(c);
    switch (role) {
        case Qt::DisplayRole:
            return is_signed ? screener_format::percent(v, true) : screener_format::by_unit(v, c.unit);
        case ScreenerResultsModel::SortRole:
            return number_or_invalid(v);
        case Qt::ForegroundRole:
            return is_signed ? signed_colour(v) : QVariant();
        case Qt::ToolTipRole:
            if (std::isnan(v))
                return {};
            if (c.unit == QLatin1String("money") || c.unit == QLatin1String("per_share"))
                return screener_format::fixed(v, 2) + QLatin1Char(' ') +
                       (r.financial_currency.isEmpty() ? r.currency : r.financial_currency);
            if (c.unit == QLatin1String("mcap"))
                return exact(v) + QLatin1Char(' ') + r.currency;
            return screener_format::fixed(v, 4);
        default:
            return {};
    }
}

} // namespace fincept::screens
