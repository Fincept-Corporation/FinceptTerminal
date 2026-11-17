// DataSourcesModels.cpp — see DataSourcesModels.h.

#include "screens/data_sources/DataSourcesModels.h"

#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "screens/data_sources/DataWorkspace.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/data_sources/ConnectionDrivers.h"
#include "ui/theme/Theme.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace fincept::screens::datasources {

namespace conn = fincept::services::connectors;
namespace dsrc = fincept::services::datasources;
namespace col = fincept::ui::colors;

namespace {

QColor dsm_health_colour(SourceHealth h) {
    switch (h) {
        case SourceHealth::Works:
        case SourceHealth::Keyless:
        case SourceHealth::KeyReady:
            return QColor(col::SUCCESS());
        case SourceHealth::NeedsDriver:
        case SourceHealth::KeyPartial:
        case SourceHealth::KeyMissing:
            return QColor(col::WARNING());
        case SourceHealth::Broken:
            return QColor(col::DANGER());
        case SourceHealth::Unavailable:
        case SourceHealth::KeyUnknown:
            break;
    }
    return QColor(col::TEXT_TERTIARY());
}

int dsm_health_rank(SourceHealth h) {
    switch (h) {
        case SourceHealth::Works:
        case SourceHealth::Keyless:
        case SourceHealth::KeyReady:
            return 0;
        case SourceHealth::KeyPartial:
        case SourceHealth::KeyUnknown:
            return 1;
        case SourceHealth::KeyMissing:
        case SourceHealth::NeedsDriver:
            return 2;
        case SourceHealth::Broken:
            return 3;
        case SourceHealth::Unavailable:
            break;
    }
    return 4;
}

} // namespace

QVector<SourceRow> build_source_rows(const QVector<DataSource>& connections) {
    register_all_connectors();
    QHash<QString, int> saved;
    for (const DataSource& ds : connections)
        ++saved[normalized_provider_key(ds)];

    QVector<SourceRow> rows;
    for (const ConnectorConfig& cfg : ConnectorRegistry::instance().all()) {
        SourceRow r;
        r.kind = SourceKind::Type;
        r.id = cfg.id;
        r.title = cfg.name;
        r.category = category_label(cfg.category);
        r.description = cfg.description;
        r.saved = saved.value(cfg.id);
        const dsrc::TypeSupport s = dsrc::effective_support(cfg.id);
        if (s.supported) {
            r.health = SourceHealth::Works;
            r.health_text = QObject::tr("works here");
            r.reason = s.parity.isEmpty() ? cfg.description : cfg.description + QStringLiteral("\n\n") + s.parity;
        } else if (s.driver == dsrc::DriverKind::Sql) {
            r.health = SourceHealth::NeedsDriver;
            r.health_text = QObject::tr("driver missing");
            r.reason = s.fix.isEmpty() ? s.reason : s.reason + QStringLiteral("\n") + s.fix;
        } else {
            r.health = SourceHealth::Unavailable;
            r.health_text = QObject::tr("not available");
            r.reason = s.fix.isEmpty() ? s.reason : s.reason + QStringLiteral("\n") + s.fix;
        }
        r.search = QStringLiteral("%1 %2 %3 %4 %5").arg(cfg.name, cfg.id, r.category, cfg.description, r.health_text).toLower();
        rows.append(r);
    }

    auto& catalog = conn::ConnectorCatalog::instance();
    if (catalog.ensure_loaded()) {
        catalog.refresh_key_presence();
        for (const conn::ConnectorInfo& c : catalog.connectors()) {
            SourceRow r;
            r.kind = SourceKind::Connector;
            r.id = c.name;
            r.title = c.title;
            r.category = c.category;
            r.description = c.description;
            if (!c.broken.isEmpty()) {
                r.health = SourceHealth::Broken;
                r.health_text = QObject::tr("upstream broken");
                r.reason = c.broken;
            } else {
                switch (catalog.key_state(c)) {
                    case conn::KeyState::NotNeeded:
                        r.health = SourceHealth::Keyless;
                        r.health_text = QObject::tr("no key needed");
                        break;
                    case conn::KeyState::Ready:
                        r.health = SourceHealth::KeyReady;
                        r.health_text = QObject::tr("key ready");
                        break;
                    case conn::KeyState::Partial:
                        r.health = SourceHealth::KeyPartial;
                        r.health_text = QObject::tr("key partly set");
                        break;
                    case conn::KeyState::Missing:
                        r.health = c.key_required ? SourceHealth::KeyMissing : SourceHealth::KeyPartial;
                        r.health_text = c.key_required ? QObject::tr("key missing") : QObject::tr("optional key");
                        break;
                    case conn::KeyState::Unknown:
                        r.health = SourceHealth::KeyUnknown;
                        r.health_text = QObject::tr("key unknown");
                        break;
                    case conn::KeyState::Unreadable:
                        // The key may be stored: not "missing".
                        r.health = SourceHealth::KeyUnknown;
                        r.health_text = QObject::tr("key unreadable");
                        break;
                }
                r.reason = conn::key_requirement_text(c);
                if (catalog.key_state(c) == conn::KeyState::Unreadable)
                    r.reason = QObject::tr("Could not read secure storage: %1").arg(catalog.key_state_reason(c));
            }
            r.search = QStringLiteral("%1 %2 %3 %4 %5 %6 %7")
                           .arg(c.title, c.name, c.category, c.region, c.description, c.env_keys.join(QLatin1Char(' ')),
                                c.command_names().join(QLatin1Char(' ')))
                           .toLower();
            rows.append(r);
        }
    }
    return rows;
}

// ─────────────────────────────────────────────────────────────────────────────
// SourceCatalogModel
// ─────────────────────────────────────────────────────────────────────────────

SourceCatalogModel::SourceCatalogModel(QObject* parent) : QAbstractTableModel(parent) {}

void SourceCatalogModel::set_rows(const QVector<SourceRow>& rows) {
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

void SourceCatalogModel::set_saved_counts(const QVector<DataSource>& connections) {
    QHash<QString, int> saved;
    for (const DataSource& ds : connections)
        ++saved[normalized_provider_key(ds)];
    for (int i = 0; i < rows_.size(); ++i) {
        if (rows_[i].kind != SourceKind::Type)
            continue;
        const int n = saved.value(rows_[i].id);
        if (n != rows_[i].saved) {
            rows_[i].saved = n;
            emit dataChanged(index(i, ColName), index(i, ColStatus));
        }
    }
}

const SourceRow* SourceCatalogModel::row_at(int row) const {
    return row >= 0 && row < rows_.size() ? &rows_[row] : nullptr;
}

int SourceCatalogModel::row_of(SourceKind kind, const QString& id) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].kind == kind && rows_[i].id == id)
            return i;
    return -1;
}

QStringList SourceCatalogModel::categories() const {
    QStringList types;
    QStringList conns;
    for (const SourceRow& r : rows_) {
        QStringList& list = r.kind == SourceKind::Type ? types : conns;
        if (!list.contains(r.category))
            list << r.category;
    }
    std::sort(conns.begin(), conns.end());
    return types + conns;
}

void SourceCatalogModel::refresh_colours() {
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, ColCount - 1), {Qt::ForegroundRole});
}

int SourceCatalogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int SourceCatalogModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant SourceCatalogModel::data(const QModelIndex& index, int role) const {
    const SourceRow* r = row_at(index.row());
    if (!r)
        return {};
    switch (role) {
        case Qt::DisplayRole:
            if (index.column() == ColName)
                return r->kind == SourceKind::Type && r->saved > 0 ? QStringLiteral("%1  (%2)").arg(r->title).arg(r->saved)
                                                                     : r->title;
            if (index.column() == ColCategory)
                return r->category;
            if (index.column() == ColStatus)
                return r->health_text;
            return {};
        case Qt::ToolTipRole:
            if (index.column() == ColStatus)
                return r->reason;
            return r->description;
        case Qt::ForegroundRole:
            if (index.column() == ColStatus)
                return dsm_health_colour(r->health);
            if (index.column() == ColCategory)
                return QColor(col::TEXT_SECONDARY());
            return {};
        case KindRole:
            return int(r->kind);
        case IdRole:
            return r->id;
        case HealthRole:
            return int(r->health);
        case CategoryRole:
            return r->category;
        case SearchRole:
            return r->search;
        case SortRole:
            if (index.column() == ColStatus)
                return dsm_health_rank(r->health);
            if (index.column() == ColCategory)
                return r->category.toLower();
            return r->title.toLower();
        default:
            return {};
    }
}

QVariant SourceCatalogModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColName:
            return tr("SOURCE");
        case ColCategory:
            return tr("CATEGORY");
        case ColStatus:
            return tr("HERE");
        default:
            return {};
    }
}

SourceFilterProxy::SourceFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(SourceCatalogModel::SortRole);
}

void SourceFilterProxy::set_search(const QString& text) {
    terms_ = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    invalidateRowsFilter();
}

void SourceFilterProxy::set_kind(KindFilter kind) {
    kind_ = kind;
    invalidateRowsFilter();
}

void SourceFilterProxy::set_category(const QString& category) {
    category_ = category;
    invalidateRowsFilter();
}

void SourceFilterProxy::set_usable_only(bool on) {
    usable_only_ = on;
    invalidateRowsFilter();
}

bool SourceFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    const auto kind = SourceKind(idx.data(SourceCatalogModel::KindRole).toInt());
    if (kind_ == KindFilter::Types && kind != SourceKind::Type)
        return false;
    if (kind_ == KindFilter::Connectors && kind != SourceKind::Connector)
        return false;
    if (!category_.isEmpty() && idx.data(SourceCatalogModel::CategoryRole).toString() != category_)
        return false;
    if (usable_only_) {
        const auto h = SourceHealth(idx.data(SourceCatalogModel::HealthRole).toInt());
        if (h == SourceHealth::Unavailable || h == SourceHealth::NeedsDriver || h == SourceHealth::Broken)
            return false;
    }
    if (terms_.isEmpty())
        return true;
    const QString hay = idx.data(SourceCatalogModel::SearchRole).toString();
    for (const QString& t : terms_)
        if (!hay.contains(t))
            return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionListModel
// ─────────────────────────────────────────────────────────────────────────────

ConnectionListModel::ConnectionListModel(QObject* parent) : QAbstractTableModel(parent) {}

void ConnectionListModel::set_connections(const QVector<DataSource>& rows) {
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

const DataSource* ConnectionListModel::find(const QString& id) const {
    for (const DataSource& d : rows_)
        if (d.id == id)
            return &d;
    return nullptr;
}

int ConnectionListModel::row_of(const QString& id) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == id)
            return i;
    return -1;
}

void ConnectionListModel::refresh_colours() {
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, ColCount - 1), {Qt::ForegroundRole});
}

int ConnectionListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int ConnectionListModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant ConnectionListModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= rows_.size())
        return {};
    const DataSource& d = rows_[index.row()];
    const ConnectorConfig* cfg = find_connector_config(d.provider);
    const bool tested = !d.last_status.isEmpty();
    const bool ok = d.last_status == QLatin1String("ok");
    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ColName:
                    return d.display_name.isEmpty() ? d.alias : d.display_name;
                case ColType:
                    return cfg ? cfg->name : d.provider;
                case ColStatus:
                    if (!d.enabled)
                        return tr("disabled");
                    if (!tested)
                        return tr("not tested");
                    return ok ? tr("OK") : (d.last_code.isEmpty() ? tr("error") : d.last_code);
                case ColTested:
                    return relative_time_text(d.last_tested_at);
                case ColLatency:
                    return ok && d.last_latency_ms >= 0 ? tr("%1 ms").arg(d.last_latency_ms) : QString();
                default:
                    return {};
            }
        case Qt::ToolTipRole:
            if (index.column() == ColStatus || index.column() == ColTested)
                return tested ? d.last_message : tr("Select it and press TEST to check it for real");
            return d.description;
        case Qt::ForegroundRole:
            if (index.column() == ColStatus) {
                if (!d.enabled || !tested)
                    return QColor(col::TEXT_TERTIARY());
                return ok ? QColor(col::SUCCESS()) : QColor(col::DANGER());
            }
            if (index.column() == ColTested || index.column() == ColLatency || index.column() == ColType)
                return QColor(col::TEXT_SECONDARY());
            return {};
        case Qt::TextAlignmentRole:
            if (index.column() == ColLatency)
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            return {};
        case IdRole:
            return d.id;
        case ProviderRole:
            return normalized_provider_key(d);
        case SearchRole:
            return QStringLiteral("%1 %2 %3 %4").arg(d.display_name, d.provider, d.tags, cfg ? cfg->name : QString()).toLower();
        case SortRole:
            switch (index.column()) {
                case ColStatus:
                    return !d.enabled ? 3 : !tested ? 2 : ok ? 0 : 1;
                case ColTested:
                    return d.last_tested_at;
                case ColLatency:
                    return d.last_latency_ms;
                case ColType:
                    return (cfg ? cfg->name : d.provider).toLower();
                default:
                    return d.display_name.toLower();
            }
        default:
            return {};
    }
}

QVariant ConnectionListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColName:
            return tr("NAME");
        case ColType:
            return tr("TYPE");
        case ColStatus:
            return tr("STATUS");
        case ColTested:
            return tr("LAST TEST");
        case ColLatency:
            return tr("LATENCY");
        default:
            return {};
    }
}

ConnectionFilterProxy::ConnectionFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(ConnectionListModel::SortRole);
}

void ConnectionFilterProxy::set_search(const QString& text) {
    terms_ = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    invalidateRowsFilter();
}

void ConnectionFilterProxy::set_provider(const QString& provider) {
    provider_ = provider;
    invalidateRowsFilter();
}

bool ConnectionFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    if (!provider_.isEmpty() && idx.data(ConnectionListModel::ProviderRole).toString() != provider_)
        return false;
    if (terms_.isEmpty())
        return true;
    const QString hay = idx.data(ConnectionListModel::SearchRole).toString();
    for (const QString& t : terms_)
        if (!hay.contains(t))
            return false;
    return true;
}

} // namespace fincept::screens::datasources
