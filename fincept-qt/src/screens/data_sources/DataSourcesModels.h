#pragma once
// Models behind the Data Sources workspace:
//   SourceCatalogModel (+ SourceFilterProxy) — the left pane: every connection type of the
//     registry and every catalogue connector, with a health badge each (works here / driver
//     missing / unavailable; keyless / key ready / key missing / broken) and saved counts.
//   ConnectionListModel (+ ConnectionFilterProxy) — the middle pane: saved connections with
//     the result of their last real TEST (status, when, latency).
// Rows are computed off the GUI thread (build_source_rows) and handed over whole.

#include "storage/repositories/DataSourceRepository.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::datasources {

enum class SourceKind { Type, Connector };

enum class SourceHealth {
    Works,       ///< type: a driver in this build reaches it
    NeedsDriver, ///< type: needs a database driver that does not load here
    Unavailable, ///< type: nothing in this build can reach it (reason + alternative)
    Keyless,     ///< connector: no key needed
    KeyReady,    ///< connector: every key stored
    KeyPartial,  ///< connector: some keys stored / optional key missing
    KeyMissing,  ///< connector: required key missing
    KeyUnknown,  ///< connector: key presence not known
    Broken,      ///< connector: upstream did not work when last checked
};

struct SourceRow {
    SourceKind kind = SourceKind::Type;
    QString id;          ///< registry type id / catalogue connector name
    QString title;
    QString category;
    QString description;
    SourceHealth health = SourceHealth::Works;
    QString health_text; ///< "works here", "driver missing", "key missing", …
    QString reason;      ///< tooltip: why (unsupported reason, key names, broken reason)
    QString search;      ///< lower-cased haystack
    int saved = 0;       ///< types: number of saved connections
};

/// Compute every row (blocking: probes SQL drivers, loads the catalogue, reads key presence).
/// Worker threads only. `connections` gives the saved counts.
QVector<SourceRow> build_source_rows(const QVector<DataSource>& connections);

class SourceCatalogModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int { ColName = 0, ColCategory, ColStatus, ColCount };
    enum Role : int { KindRole = Qt::UserRole + 1, IdRole, HealthRole, CategoryRole, SearchRole, SortRole };

    explicit SourceCatalogModel(QObject* parent = nullptr);
    void set_rows(const QVector<SourceRow>& rows);
    /// Update saved counts without rebuilding (after a save / delete / import).
    void set_saved_counts(const QVector<DataSource>& connections);
    const SourceRow* row_at(int row) const;
    int row_of(SourceKind kind, const QString& id) const;
    QStringList categories() const;
    void refresh_colours();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    QVector<SourceRow> rows_;
};

class SourceFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    enum class KindFilter { All, Types, Connectors };
    explicit SourceFilterProxy(QObject* parent = nullptr);
    void set_search(const QString& text);
    void set_kind(KindFilter kind);
    void set_category(const QString& category); ///< "" = all
    void set_usable_only(bool on);

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QStringList terms_;
    KindFilter kind_ = KindFilter::All;
    QString category_;
    bool usable_only_ = false;
};

class ConnectionListModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int { ColName = 0, ColType, ColStatus, ColTested, ColLatency, ColCount };
    enum Role : int { IdRole = Qt::UserRole + 1, ProviderRole, SortRole, SearchRole };

    explicit ConnectionListModel(QObject* parent = nullptr);
    void set_connections(const QVector<DataSource>& rows);
    const QVector<DataSource>& connections() const { return rows_; }
    const DataSource* find(const QString& id) const;
    int row_of(const QString& id) const;
    void refresh_colours();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  private:
    QVector<DataSource> rows_;
};

class ConnectionFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit ConnectionFilterProxy(QObject* parent = nullptr);
    void set_search(const QString& text);
    void set_provider(const QString& provider); ///< "" = every type

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  private:
    QStringList terms_;
    QString provider_;
};

} // namespace fincept::screens::datasources
