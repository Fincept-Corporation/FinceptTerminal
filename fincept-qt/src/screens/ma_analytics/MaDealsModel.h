// src/screens/ma_analytics/MaDealsModel.h
#pragma once
// Table model over the JSON rows of the M&A deal database (deals and SEC filings). The rows stay JSON objects;
// each column picks one key and formats it. Sort keys are exposed on kSortRole for a QSortFilterProxyModel.
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::screens {

struct MaColumn {
    enum class Kind {
        Text,       ///< plain text
        Date,       ///< ISO date, shown as is
        Money,      ///< USD amount, compact ("$5.8B")
        Multiple,   ///< "8.7x"
        Percent,    ///< value already in percent units: "45.3%"
        Score,      ///< 0..1 confidence, shown as a percent
        Verified,   ///< 0/1 verified flag: "VERIFIED" / "UNVERIFIED" badge text, coloured
        Parsed,     ///< filing row: whether a deal was extracted from it
    };
    QString key;
    QString header;
    Kind kind = Kind::Text;
};

class MaDealsModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    static constexpr int kSortRole = Qt::UserRole + 1;

    MaDealsModel(QVector<MaColumn> columns, QString id_key, QObject* parent = nullptr);

    void set_rows(const QJsonArray& rows);
    /// Replace the row with the same id (or append it).
    void upsert_row(const QJsonObject& row);
    void remove_id(const QString& id);
    QJsonObject row_at(int row) const;
    int row_of(const QString& id) const;
    int verified_count() const;
    QVector<MaColumn> columns() const { return columns_; }

    /// Badge colours come from the active theme at paint time; tell the views to repaint after a theme change.
    void refresh_colors();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// Text of one cell as shown (CSV export uses it).
    QString display_text(int row, int column) const;

  private:
    QVector<MaColumn> columns_;
    QString id_key_;
    QVector<QJsonObject> rows_;
};

} // namespace fincept::screens
