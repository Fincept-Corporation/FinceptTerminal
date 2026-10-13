// ConnectorCatalogModels.cpp — list / filter / result models for the Data Sources › CATALOG tab.

#include "screens/data_sources/ConnectorCatalogModels.h"

#include "ui/theme/Theme.h"

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>

namespace fincept::screens::datasources {

namespace conn = fincept::services::connectors;
namespace col = fincept::ui::colors;

namespace {

constexpr int kCellDisplayChars = 300;

QString catalog_number_text(double v) {
    if (std::isfinite(v) && std::floor(v) == v && std::fabs(v) < 1e15)
        return QString::number(static_cast<qint64>(v));
    QString s = QString::number(v, 'f', 8);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

QString catalog_value_text(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::String:
            return v.toString();
        case QJsonValue::Double:
            return catalog_number_text(v.toDouble());
        case QJsonValue::Bool:
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Object:
            return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        case QJsonValue::Array:
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        default:
            return {};
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// CatalogListModel
// ─────────────────────────────────────────────────────────────────────────────

CatalogListModel::CatalogListModel(QObject* parent) : QAbstractTableModel(parent) {}

void CatalogListModel::reload() {
    beginResetModel();
    rows_.clear();
    haystack_.clear();
    const auto& all = conn::ConnectorCatalog::instance().connectors();
    rows_.reserve(all.size());
    haystack_.reserve(all.size());
    for (const auto& c : all) {
        rows_.append(&c);
        haystack_ << QStringList{c.name,
                                 c.title,
                                 c.description,
                                 c.category,
                                 c.region,
                                 c.env_keys.join(QLatin1Char(' ')),
                                 c.command_names().join(QLatin1Char(' ')),
                                 c.script}
                         .join(QLatin1Char(' '))
                         .toLower();
    }
    endResetModel();
}

void CatalogListModel::refresh_key_states() {
    if (rows_.isEmpty())
        return;
    emit dataChanged(index(0, ColKey), index(int(rows_.size()) - 1, ColKey));
}

const conn::ConnectorInfo* CatalogListModel::connector_at(int row) const {
    return (row >= 0 && row < rows_.size()) ? rows_[row] : nullptr;
}

int CatalogListModel::row_of(const QString& name) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i]->name == name)
            return i;
    return -1;
}

int CatalogListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int CatalogListModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant CatalogListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const conn::ConnectorInfo& c = *rows_[index.row()];
    const conn::KeyState ks = conn::ConnectorCatalog::instance().key_state(c);

    switch (role) {
        case NameRole:
            return c.name;
        case SearchTextRole:
            return haystack_.value(index.row());
        case CategoryRole:
            return c.category;
        case KeylessRole:
            return c.env_keys.isEmpty();
        case KeyReadyRole:
            return ks == conn::KeyState::Ready;
        case BrokenRole:
            return !c.broken.isEmpty();
        default:
            break;
    }

    if (role == Qt::DisplayRole || role == SortRole) {
        switch (index.column()) {
            case ColConnector:
                return c.title;
            case ColCategory:
                return c.category;
            case ColRegion:
                return c.region;
            case ColKey: {
                if (role == SortRole) {
                    switch (ks) {
                        case conn::KeyState::Ready:
                            return 0;
                        case conn::KeyState::NotNeeded:
                            return 1;
                        case conn::KeyState::Partial:
                            return 2;
                        case conn::KeyState::Unknown:
                            return 3;
                        case conn::KeyState::Missing:
                            return c.key_required ? 5 : 4;
                    }
                    return 6;
                }
                switch (ks) {
                    case conn::KeyState::NotNeeded:
                        return QStringLiteral("—");
                    case conn::KeyState::Ready:
                        return tr("● ready");
                    case conn::KeyState::Partial:
                        return tr("◐ partial");
                    case conn::KeyState::Unknown:
                        return QStringLiteral("…");
                    case conn::KeyState::Missing:
                        return c.key_required ? tr("○ needs key") : tr("○ optional");
                }
                return {};
            }
            default:
                return {};
        }
    }

    if (role == Qt::ForegroundRole) {
        if (!c.broken.isEmpty())
            return QColor(col::TEXT_TERTIARY());
        if (index.column() == ColKey) {
            switch (ks) {
                case conn::KeyState::Ready:
                    return QColor(col::POSITIVE());
                case conn::KeyState::Missing:
                    return c.key_required ? QColor(col::WARNING()) : QColor(col::TEXT_SECONDARY());
                case conn::KeyState::Partial:
                    return QColor(col::WARNING());
                default:
                    return QColor(col::TEXT_TERTIARY());
            }
        }
        if (index.column() == ColConnector)
            return QColor(col::TEXT_PRIMARY());
        return QColor(col::TEXT_SECONDARY());
    }

    if (role == Qt::ToolTipRole) {
        if (index.column() == ColKey)
            return conn::key_requirement_text(c);
        QString tip = c.name + QStringLiteral("\n") + c.description;
        if (!c.broken.isEmpty())
            tip += QStringLiteral("\n\n") + tr("Known issue: %1").arg(c.broken);
        return tip;
    }
    return {};
}

QVariant CatalogListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColConnector:
            return tr("CONNECTOR");
        case ColCategory:
            return tr("CATEGORY");
        case ColRegion:
            return tr("REGION");
        case ColKey:
            return tr("KEY");
        default:
            return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CatalogFilterProxy
// ─────────────────────────────────────────────────────────────────────────────

CatalogFilterProxy::CatalogFilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setSortRole(CatalogListModel::SortRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void CatalogFilterProxy::set_search(const QString& text) {
    static const QRegularExpression kWs(QStringLiteral("\\s+"));
    terms_ = text.trimmed().toLower().split(kWs, Qt::SkipEmptyParts);
    invalidateFilter();
}

void CatalogFilterProxy::set_category(const QString& category) {
    category_ = category;
    invalidateFilter();
}

void CatalogFilterProxy::set_keyless_only(bool on) {
    keyless_only_ = on;
    invalidateFilter();
}

void CatalogFilterProxy::set_configured_only(bool on) {
    configured_only_ = on;
    invalidateFilter();
}

bool CatalogFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const {
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    if (!category_.isEmpty() && idx.data(CatalogListModel::CategoryRole).toString() != category_)
        return false;
    if (keyless_only_ && !idx.data(CatalogListModel::KeylessRole).toBool())
        return false;
    if (configured_only_ && !idx.data(CatalogListModel::KeyReadyRole).toBool())
        return false;
    if (terms_.isEmpty())
        return true;
    const QString hay = idx.data(CatalogListModel::SearchTextRole).toString();
    for (const QString& t : terms_)
        if (!hay.contains(t))
            return false;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ConnectorResultModel
// ─────────────────────────────────────────────────────────────────────────────

ConnectorResultModel::ConnectorResultModel(QObject* parent) : QAbstractTableModel(parent) {}

void ConnectorResultModel::set_table(const QJsonArray& rows, const QStringList& columns, const QString& kind) {
    beginResetModel();
    rows_ = rows;
    columns_ = columns;
    kind_ = kind;
    endResetModel();
}

void ConnectorResultModel::clear() {
    set_table({}, {}, {});
}

QJsonValue ConnectorResultModel::cell(int row, int column) const {
    if (row < 0 || row >= rows_.size() || column < 0 || column >= columns_.size())
        return {};
    const QJsonValue r = rows_.at(row);
    if (r.isObject())
        return r.toObject().value(columns_.at(column));
    if (r.isArray()) {
        bool ok = false;
        const int i = columns_.at(column).toInt(&ok);
        return ok ? r.toArray().at(i) : QJsonValue();
    }
    // Scalar row: it is the "value" column.
    return columns_.at(column) == QLatin1String("value") ? r : QJsonValue();
}

QString ConnectorResultModel::cell_text(int row, int column) const {
    return catalog_value_text(cell(row, column));
}

int ConnectorResultModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int ConnectorResultModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(columns_.size());
}

QVariant ConnectorResultModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    const QJsonValue v = cell(index.row(), index.column());
    switch (role) {
        case Qt::DisplayRole: {
            const QString s = catalog_value_text(v);
            return s.size() > kCellDisplayChars ? s.left(kCellDisplayChars) + QStringLiteral("…") : s;
        }
        case Qt::ToolTipRole: {
            const QString s = catalog_value_text(v);
            return s.size() > 60 ? s.left(2000) : QVariant();
        }
        case Qt::TextAlignmentRole:
            return v.isDouble() ? QVariant(int(Qt::AlignRight | Qt::AlignVCenter))
                                : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));
        case Qt::ForegroundRole:
            if (v.isNull() || v.isUndefined())
                return QColor(col::TEXT_TERTIARY());
            return v.isDouble() ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_SECONDARY());
        case kSortRole:
            if (v.isDouble())
                return v.toDouble();
            if (v.isBool())
                return v.toBool() ? 1.0 : 0.0;
            return catalog_value_text(v).toLower();
        default:
            return {};
    }
}

QVariant ConnectorResultModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Horizontal)
        return section >= 0 && section < columns_.size() ? columns_.at(section).toUpper() : QVariant();
    return section + 1;
}

} // namespace fincept::screens::datasources
