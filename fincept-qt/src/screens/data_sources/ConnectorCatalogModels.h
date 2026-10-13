#pragma once
// Models behind the Data Sources › CATALOG tab: the connector list (+ its filter proxy) and
// the result table of one connector run.

#include "services/connectors/ConnectorCatalog.h"

#include <QAbstractTableModel>
#include <QJsonArray>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::datasources {

/// One row per catalogue connector. Reads key presence from the catalogue cache only (never
/// touches SecureStorage) — ConnectorService::refresh_key_status() fills that cache off-thread.
class CatalogListModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int { ColConnector = 0, ColCategory, ColRegion, ColKey, ColCount };
    enum Role : int {
        NameRole = Qt::UserRole + 1,
        SearchTextRole,
        CategoryRole,
        KeylessRole,
        KeyReadyRole,
        BrokenRole,
        SortRole,
    };

    explicit CatalogListModel(QObject* parent = nullptr);

    /// Re-reads the (loaded) ConnectorCatalog.
    void reload();
    /// Key presence changed — repaint the KEY column.
    void refresh_key_states();

    const fincept::services::connectors::ConnectorInfo* connector_at(int row) const;
    int row_of(const QString& name) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    QVector<const fincept::services::connectors::ConnectorInfo*> rows_;
    QStringList haystack_; ///< lower-cased name/title/description/category/region/keys/commands per row
};

/// Search (all whitespace-separated terms must match), category, "keyless only" and
/// "configured keys" filters over CatalogListModel.
class CatalogFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit CatalogFilterProxy(QObject* parent = nullptr);

    void set_search(const QString& text);
    void set_category(const QString& category); ///< empty = all
    void set_keyless_only(bool on);
    void set_configured_only(bool on);

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QStringList terms_;
    QString category_;
    bool keyless_only_ = false;
    bool configured_only_ = false;
};

/// The table projection of a ConnectorResult (records, dict rows, key/value pairs, value lists
/// or arrays-of-arrays). Sort role is numeric for numbers so sorting is numeric.
class ConnectorResultModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    static constexpr int kSortRole = Qt::UserRole + 1;

    explicit ConnectorResultModel(QObject* parent = nullptr);

    void set_table(const QJsonArray& rows, const QStringList& columns, const QString& kind);
    void clear();

    /// Full-precision cell text (CSV export).
    QString cell_text(int row, int column) const;
    QStringList columns() const { return columns_; }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    QJsonValue cell(int row, int column) const;

    QJsonArray rows_;
    QStringList columns_;
    QString kind_;
};

} // namespace fincept::screens::datasources
