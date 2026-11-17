// MappingModels.cpp — see MappingModels.h.

#include "screens/data_mapping/MappingModels.h"

#include "screens/data_sources/DataWorkspace.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens::datamapping {

namespace dmap = fincept::services::datamapping;
namespace dsrc = fincept::services::datasources;
namespace col = fincept::ui::colors;

// ─────────────────────────────────────────────────────────────────────────────
// MappingListModel
// ─────────────────────────────────────────────────────────────────────────────

MappingListModel::MappingListModel(QObject* parent) : QAbstractTableModel(parent) {}

void MappingListModel::set_mappings(const QVector<fincept::DataMapping>& rows) {
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

const fincept::DataMapping* MappingListModel::find(const QString& id) const {
    for (const auto& m : rows_)
        if (m.id == id)
            return &m;
    return nullptr;
}

int MappingListModel::row_of(const QString& id) const {
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_[i].id == id)
            return i;
    return -1;
}

void MappingListModel::refresh_colours() {
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, ColCount - 1), {Qt::ForegroundRole});
}

int MappingListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(rows_.size());
}

int MappingListModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant MappingListModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= rows_.size())
        return {};
    const fincept::DataMapping& m = rows_[index.row()];
    switch (role) {
        case Qt::DisplayRole:
            if (index.column() == ColName)
                return m.name;
            if (index.column() == ColSchema)
                return m.schema_name;
            if (index.column() == ColRun) {
                if (m.last_run_status.isEmpty())
                    return tr("not run");
                if (m.last_run_status == QLatin1String("error"))
                    return tr("failed");
                return m.last_run_errors > 0 ? tr("%1 rows · %2 issues").arg(m.last_run_rows).arg(m.last_run_errors)
                                             : tr("%1 rows").arg(m.last_run_rows);
            }
            return {};
        case Qt::ToolTipRole:
            if (index.column() == ColRun && !m.last_run_at.isEmpty())
                return tr("Last run %1").arg(fincept::screens::datasources::relative_time_text(m.last_run_at));
            return m.spec_version >= 2 ? tr("Revision %1").arg(m.revision) : tr("Saved before mappings had versions");
        case Qt::ForegroundRole:
            if (index.column() == ColRun) {
                if (m.last_run_status == QLatin1String("ok"))
                    return QColor(col::SUCCESS());
                if (m.last_run_status == QLatin1String("issues"))
                    return QColor(col::WARNING());
                if (m.last_run_status == QLatin1String("error"))
                    return QColor(col::DANGER());
                return QColor(col::TEXT_TERTIARY());
            }
            if (index.column() == ColSchema)
                return QColor(col::TEXT_SECONDARY());
            return {};
        case IdRole:
            return m.id;
        case SortRole:
            if (index.column() == ColRun)
                return m.last_run_at;
            if (index.column() == ColSchema)
                return m.schema_name;
            return m.name.toLower();
        default:
            return {};
    }
}

QVariant MappingListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColName:
            return tr("MAPPING");
        case ColSchema:
            return tr("SCHEMA");
        case ColRun:
            return tr("LAST RUN");
        default:
            return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FieldGridModel
// ─────────────────────────────────────────────────────────────────────────────

FieldGridModel::FieldGridModel(QObject* parent) : QAbstractTableModel(parent) {}

void FieldGridModel::set_fields(const QVector<dmap::FieldMapping>& fields, bool targets_editable) {
    beginResetModel();
    fields_ = fields;
    targets_editable_ = targets_editable;
    stats_.clear();
    samples_.clear();
    has_preview_ = false;
    endResetModel();
}

void FieldGridModel::set_preview(const dmap::MappingOutput& out) {
    stats_ = out.field_stats;
    samples_.clear();
    for (int c = 0; c < fields_.size(); ++c) {
        QString sample;
        for (int r = 0; r < out.table.row_count() && sample.isEmpty(); ++r) {
            const QJsonValue v = out.table.cell(r, c);
            if (!v.isNull() && !v.isUndefined())
                sample = dsrc::cell_text(v);
        }
        samples_ << sample.left(80);
    }
    has_preview_ = true;
    if (!fields_.isEmpty())
        emit dataChanged(index(0, ColSample), index(int(fields_.size()) - 1, ColCheck));
}

void FieldGridModel::clear_preview() {
    stats_.clear();
    samples_.clear();
    has_preview_ = false;
    if (!fields_.isEmpty())
        emit dataChanged(index(0, ColSample), index(int(fields_.size()) - 1, ColCheck));
}

void FieldGridModel::add_field(const dmap::FieldMapping& f) {
    const int row = int(fields_.size());
    beginInsertRows(QModelIndex(), row, row);
    fields_.append(f);
    endInsertRows();
    emit fields_edited();
}

void FieldGridModel::remove_field(int row) {
    if (row < 0 || row >= fields_.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    fields_.removeAt(row);
    if (row < stats_.size())
        stats_.removeAt(row);
    if (row < samples_.size())
        samples_.removeAt(row);
    endRemoveRows();
    emit fields_edited();
}

void FieldGridModel::refresh_colours() {
    if (!fields_.isEmpty())
        emit dataChanged(index(0, 0), index(int(fields_.size()) - 1, ColCount - 1), {Qt::ForegroundRole});
}

int FieldGridModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(fields_.size());
}

int FieldGridModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant FieldGridModel::data(const QModelIndex& index, int role) const {
    if (index.row() < 0 || index.row() >= fields_.size())
        return {};
    const dmap::FieldMapping& f = fields_[index.row()];
    const bool has_stats = has_preview_ && index.row() < stats_.size();
    const dmap::FieldStats st = has_stats ? stats_[index.row()] : dmap::FieldStats();
    if (role == Qt::CheckStateRole && index.column() == ColRequired)
        return f.required ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case ColTarget:
                return f.target;
            case ColType:
                return dmap::target_type_name(f.type);
            case ColSource:
                if (role == Qt::EditRole)
                    return f.source;
                return f.source.isEmpty() ? (f.default_value.isEmpty() ? tr("(none)") : tr("(constant)")) : f.source;
            case ColFormat:
                return f.format.isEmpty() && role == Qt::DisplayRole &&
                               (f.type == dmap::TargetType::Date || f.type == dmap::TargetType::DateTime)
                           ? QStringLiteral("auto")
                           : f.format;
            case ColScale:
                return f.scale == 1.0 && role == Qt::DisplayRole ? QString() : QString::number(f.scale, 'g', 12);
            case ColDefault:
                return f.default_value;
            case ColSample:
                return has_preview_ ? samples_.value(index.row()) : QString();
            case ColCheck:
                if (!has_stats)
                    return QString();
                if (st.errors > 0)
                    return tr("%1 failed").arg(st.errors);
                if (st.empty > 0)
                    return tr("ok · %1 empty").arg(st.empty);
                return tr("ok");
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColCheck && has_stats) {
            if (st.errors > 0)
                return QColor(col::DANGER());
            if (st.empty > 0 && f.required)
                return QColor(col::WARNING());
            return QColor(col::SUCCESS());
        }
        if (index.column() == ColSource && !f.source.isEmpty() && !source_columns_.isEmpty() &&
            !source_columns_.contains(f.source))
            return QColor(col::DANGER());
        if (index.column() == ColSample || index.column() == ColType)
            return QColor(col::TEXT_SECONDARY());
        return {};
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == ColCheck && has_stats)
            return st.first_error.isEmpty() ? tr("%1 cells ok, %2 empty").arg(st.ok).arg(st.empty) : st.first_error;
        if (index.column() == ColSource && !f.source.isEmpty() && !source_columns_.isEmpty() &&
            !source_columns_.contains(f.source))
            return tr("The source has no column \"%1\"").arg(f.source);
        if (index.column() == ColFormat)
            return tr("Dates: auto (ISO; a day/month order that cannot be decided is flagged, never guessed), iso, a "
                      "pattern like dd.MM.yyyy or MM/dd/yyyy, epoch_s, epoch_ms, year, quarter, month. Periods: start, "
                      "end or code.");
        if (index.column() == ColScale)
            return tr("Multiply numbers: 0.01 turns cents into units, 1000 turns thousands into units");
        if (index.column() == ColDefault)
            return tr("Used when the cell is empty — or as a constant when no source column is set");
        return {};
    }
    return {};
}

bool FieldGridModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (index.row() < 0 || index.row() >= fields_.size())
        return false;
    dmap::FieldMapping& f = fields_[index.row()];
    if (role == Qt::CheckStateRole && index.column() == ColRequired) {
        f.required = value.toInt() == Qt::Checked;
    } else if (role == Qt::EditRole) {
        const QString t = value.toString().trimmed();
        switch (index.column()) {
            case ColTarget:
                if (t.isEmpty())
                    return false;
                f.target = t;
                break;
            case ColType:
                f.type = dmap::target_type_from_name(t);
                break;
            case ColSource:
                f.source = t;
                break;
            case ColFormat:
                f.format = t == QLatin1String("auto") ? QString() : t;
                break;
            case ColScale: {
                bool ok = false;
                const double d = t.isEmpty() ? 1.0 : t.toDouble(&ok);
                if (!t.isEmpty() && (!ok || d == 0.0))
                    return false;
                f.scale = d;
                break;
            }
            case ColDefault:
                f.default_value = value.toString();
                break;
            default:
                return false;
        }
    } else {
        return false;
    }
    emit dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(ColCount - 1));
    emit fields_edited();
    return true;
}

Qt::ItemFlags FieldGridModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags fl = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    switch (index.column()) {
        case ColTarget:
            if (targets_editable_)
                fl |= Qt::ItemIsEditable;
            break;
        case ColRequired:
            fl |= Qt::ItemIsUserCheckable;
            break;
        case ColType:
            if (targets_editable_)
                fl |= Qt::ItemIsEditable;
            break;
        case ColSource:
        case ColFormat:
        case ColScale:
        case ColDefault:
            fl |= Qt::ItemIsEditable;
            break;
        default:
            break;
    }
    return fl;
}

QVariant FieldGridModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::ToolTipRole && section == ColRequired)
        return tr("Required: an empty cell (after the default) is reported as a problem");
    if (role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColTarget:
            return tr("TARGET FIELD");
        case ColType:
            return tr("TYPE");
        case ColSource:
            return tr("SOURCE COLUMN");
        case ColFormat:
            return tr("FORMAT");
        case ColScale:
            return tr("SCALE");
        case ColDefault:
            return tr("DEFAULT");
        case ColRequired:
            return tr("REQ");
        case ColSample:
            return tr("FIRST VALUE");
        case ColCheck:
            return tr("CHECK");
        default:
            return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FieldGridDelegate
// ─────────────────────────────────────────────────────────────────────────────

FieldGridDelegate::FieldGridDelegate(FieldGridModel* model, QObject* parent) : QStyledItemDelegate(parent), model_(model) {}

QStringList FieldGridDelegate::formats_for(dmap::TargetType type) {
    switch (type) {
        case dmap::TargetType::Date:
        case dmap::TargetType::DateTime:
            return {QStringLiteral("auto"),       QStringLiteral("iso"),        QStringLiteral("yyyy-MM-dd"),
                    QStringLiteral("dd.MM.yyyy"), QStringLiteral("dd/MM/yyyy"), QStringLiteral("MM/dd/yyyy"),
                    QStringLiteral("yyyyMMdd"),   QStringLiteral("epoch_s"),    QStringLiteral("epoch_ms"),
                    QStringLiteral("year"),       QStringLiteral("quarter"),    QStringLiteral("month")};
        case dmap::TargetType::Period:
            return {QStringLiteral("code"), QStringLiteral("start"), QStringLiteral("end")};
        default:
            return {QString()};
    }
}

QWidget* FieldGridDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    switch (index.column()) {
        case FieldGridModel::ColType: {
            auto* combo = new QComboBox(parent);
            for (auto t : {dmap::TargetType::String, dmap::TargetType::Integer, dmap::TargetType::Number,
                           dmap::TargetType::Bool, dmap::TargetType::Date, dmap::TargetType::DateTime,
                           dmap::TargetType::Period, dmap::TargetType::Json})
                combo->addItem(dmap::target_type_name(t));
            return combo;
        }
        case FieldGridModel::ColSource: {
            auto* combo = new QComboBox(parent);
            combo->setEditable(true);
            combo->addItem(QString());
            combo->addItems(model_->source_columns());
            return combo;
        }
        case FieldGridModel::ColFormat: {
            auto* combo = new QComboBox(parent);
            combo->setEditable(true);
            const auto& fields = model_->fields();
            const dmap::TargetType t = index.row() < fields.size() ? fields[index.row()].type : dmap::TargetType::String;
            combo->addItems(formats_for(t));
            return combo;
        }
        default:
            return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void FieldGridDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        const QString v = index.data(Qt::EditRole).toString();
        const int i = combo->findText(v);
        if (i >= 0)
            combo->setCurrentIndex(i);
        else if (combo->isEditable())
            combo->setEditText(v);
        return;
    }
    QStyledItemDelegate::setEditorData(editor, index);
}

void FieldGridDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    if (auto* combo = qobject_cast<QComboBox*>(editor)) {
        model->setData(index, combo->currentText(), Qt::EditRole);
        return;
    }
    QStyledItemDelegate::setModelData(editor, model, index);
}

} // namespace fincept::screens::datamapping
