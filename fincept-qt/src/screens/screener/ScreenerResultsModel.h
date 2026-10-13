#pragma once
// ScreenerResultsModel — column layout and cell formatting for screener rows.
//
// Base columns first (always present), then the run's extra columns (criteria fields, the sort
// field, "@ screen" snapshot values). Display text is formatted here; SortRole carries the raw
// number so header-click sorting is numeric, and missing values carry an invalid QVariant that
// ScreenerSortProxy always places last.
//
// No Q_OBJECT (LiveTableModel is a template and declares no signals), so user-visible base header
// strings are translated by the owning screen and passed in via set_base_headers().

#include "services/screener/ScreenerService.h"
#include "ui/tables/LiveTableModel.h"

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVector>

namespace fincept::screens {

class ScreenerResultsModel : public ui::LiveTableModel<services::ScreenerRow> {
  public:
    enum BaseColumn : int {
        ColSymbol = 0,
        ColName,
        ColPrice,
        ColChangePct,
        ColVolume,
        ColMarketCap,
        ColPe,
        ColSector,
        ColExchange,
        ColCurrency,
        kBaseColumnCount,
    };
    /// "SYMBOL name sector industry" on column 0 — the proxy's text filter matches against it.
    static constexpr int SearchRole = Qt::UserRole + 10;

    explicit ScreenerResultsModel(QObject* parent = nullptr);

    /// Translated base headers + header tooltips (kBaseColumnCount entries each).
    void set_base_headers(const QStringList& headers, const QStringList& tooltips);
    /// Replace rows and extra columns. A different column set resets the model.
    void set_result(const QVector<services::ScreenerRow>& rows, const QVector<services::ScreenerColumn>& extra);
    const QVector<services::ScreenerColumn>& extra_columns() const { return extra_; }

    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;

    /// True for right-aligned numeric columns.
    bool is_numeric_column(int col) const;

  private:
    QVariant cell(const services::ScreenerRow& r, int col, int role) const;
    QVariant base_cell(const services::ScreenerRow& r, int col, int role) const;
    QVariant extra_cell(const services::ScreenerRow& r, const services::ScreenerColumn& c, int role) const;
    void apply_headers();

    QVector<services::ScreenerColumn> extra_;
    QStringList base_headers_;
    QStringList base_tooltips_;
};

/// Numeric sort on SortRole with missing values last in BOTH directions (a plain QVariant compare
/// either errors on invalid values or floats every "—" to the top of a descending sort).
class ScreenerSortProxy : public QSortFilterProxyModel {
  public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

  protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

namespace screener_format {
/// 1,234.56 — fixed decimals with thousands separators.
QString fixed(double v, int decimals);
/// Price with precision that suits its magnitude (sub-unit prices get 4 decimals).
QString price(double v);
/// 1.23T / 456.70B / 12.30M / 4.50K (sign-preserving).
QString compact(double v);
/// +1.23% (with_sign) or 1.23%.
QString percent(double v, bool with_sign);
/// Display text for an extra column value by catalogue unit.
QString by_unit(double v, const QString& unit);
} // namespace screener_format

} // namespace fincept::screens
