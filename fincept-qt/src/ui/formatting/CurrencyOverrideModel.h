#pragma once
// CurrencyOverrideModel — the per-currency symbol / decimals table of the
// Currency & Number Format settings panel: every code of the currency table
// plus the user's custom codes, with the user's overrides applied. Edited in
// place (one FormatSettings setter per committed cell); refreshes itself on
// FormatSettings::changed without a reset (selection and scroll kept).

#include <QAbstractTableModel>
#include <QStringList>

namespace fincept::ui::formatting {

class CurrencyOverrideModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column { ColCode, ColName, ColSymbol, ColDecimals, ColExample, ColReset, ColumnCount };
    /// Raw value for sorting (code, name, decimals as int).
    static constexpr int SortRole = Qt::UserRole + 1;
    /// True when the row's currency has a user override.
    static constexpr int OverriddenRole = Qt::UserRole + 2;

    explicit CurrencyOverrideModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    /// The code of `row` (canonical spelling).
    QString code_at(int row) const;
    /// Row of `code`, -1 if absent.
    int row_of(const QString& code) const;
    /// Removes the override of `row` (one FormatSettings change).
    void reset_row(int row);

  private:
    void sync_rows(); // custom codes added / removed since the last sync
    static QStringList custom_codes();

    QStringList table_codes_;  ///< fixed: the currency table
    QStringList custom_codes_; ///< sorted user-defined codes, after the table
};

} // namespace fincept::ui::formatting
