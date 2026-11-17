// src/screens/ma_analytics/MaDealsModel.cpp
#include "screens/ma_analytics/MaDealsModel.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QJsonValue>

#include <cmath>

namespace fincept::screens {

namespace {

/// A finite number stored under `key`, or NaN (absent, null, text).
double ma_model_number(const QJsonObject& row, const QString& key) {
    const QJsonValue v = row.value(key);
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        if (ok)
            return d;
    }
    return std::nan("");
}

bool ma_model_verified(const QJsonObject& row) {
    return row.value(QStringLiteral("verified")).toInt(0) == 1;
}

} // namespace

MaDealsModel::MaDealsModel(QVector<MaColumn> columns, QString id_key, QObject* parent)
    : QAbstractTableModel(parent), columns_(std::move(columns)), id_key_(std::move(id_key)) {}

void MaDealsModel::set_rows(const QJsonArray& rows) {
    beginResetModel();
    rows_.clear();
    rows_.reserve(rows.size());
    for (const QJsonValue& v : rows)
        if (v.isObject())
            rows_.append(v.toObject());
    endResetModel();
}

void MaDealsModel::upsert_row(const QJsonObject& row) {
    const int r = row_of(row.value(id_key_).toString());
    if (r >= 0) {
        rows_[r] = row;
        emit dataChanged(index(r, 0), index(r, columnCount() - 1));
        return;
    }
    beginInsertRows(QModelIndex(), 0, 0);
    rows_.prepend(row);
    endInsertRows();
}

void MaDealsModel::remove_id(const QString& id) {
    const int r = row_of(id);
    if (r < 0)
        return;
    beginRemoveRows(QModelIndex(), r, r);
    rows_.removeAt(r);
    endRemoveRows();
}

QJsonObject MaDealsModel::row_at(int row) const {
    return (row >= 0 && row < rows_.size()) ? rows_.at(row) : QJsonObject();
}

int MaDealsModel::row_of(const QString& id) const {
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < rows_.size(); ++i)
        if (rows_.at(i).value(id_key_).toString() == id)
            return i;
    return -1;
}

int MaDealsModel::verified_count() const {
    int n = 0;
    for (const QJsonObject& r : rows_)
        n += ma_model_verified(r) ? 1 : 0;
    return n;
}

void MaDealsModel::refresh_colors() {
    if (!rows_.isEmpty())
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::ForegroundRole});
}

int MaDealsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int MaDealsModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(columns_.size());
}

QString MaDealsModel::display_text(int row, int column) const {
    if (row < 0 || row >= rows_.size() || column < 0 || column >= columns_.size())
        return {};
    const QJsonObject& r = rows_.at(row);
    const MaColumn& c = columns_.at(column);
    switch (c.kind) {
        case MaColumn::Kind::Verified:
            return ma_model_verified(r) ? tr("VERIFIED") : tr("UNVERIFIED");
        case MaColumn::Kind::Parsed: {
            if (!r.value(QStringLiteral("deal_id")).toString().isEmpty())
                return tr("Deal extracted");
            return r.value(QStringLiteral("parsed_flag")).toInt(0) == 1 ? tr("Read") : QString();
        }
        case MaColumn::Kind::Money: {
            const double v = ma_model_number(r, c.key);
            // The deals database is USD (SEC/EDGAR filings): fixed currency, abbreviated per the user's scheme.
            return std::isfinite(v) ? fincept::ui::formatting::money(v, QStringLiteral("USD"),
                                                                     fincept::ui::formatting::Compact::FromSettings)
                                    : QString();
        }
        case MaColumn::Kind::Multiple: {
            const double v = ma_model_number(r, c.key);
            return std::isfinite(v) ? QString::number(v, 'f', 1) + QLatin1Char('x') : QString();
        }
        case MaColumn::Kind::Percent: {
            const double v = ma_model_number(r, c.key);
            return std::isfinite(v) ? QString::number(v, 'f', 1) + QLatin1Char('%') : QString();
        }
        case MaColumn::Kind::Score: {
            const double v = ma_model_number(r, c.key);
            return std::isfinite(v) ? QString::number(v * 100.0, 'f', 0) + QLatin1Char('%') : QString();
        }
        case MaColumn::Kind::Date:
        case MaColumn::Kind::Text:
            break;
    }
    const QJsonValue v = r.value(c.key);
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 12);
    return {};
}

QVariant MaDealsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size() || index.column() >= columns_.size())
        return {};
    const QJsonObject& r = rows_.at(index.row());
    const MaColumn& c = columns_.at(index.column());
    const bool numeric = c.kind == MaColumn::Kind::Money || c.kind == MaColumn::Kind::Multiple ||
                         c.kind == MaColumn::Kind::Percent || c.kind == MaColumn::Kind::Score;
    switch (role) {
        case Qt::DisplayRole:
            return display_text(index.row(), index.column());
        case kSortRole: {
            if (numeric) {
                const double v = ma_model_number(r, c.key);
                return std::isfinite(v) ? v : -1e300; // blanks sort together
            }
            if (c.kind == MaColumn::Kind::Verified)
                return ma_model_verified(r) ? 1 : 0;
            return display_text(index.row(), index.column()).toLower();
        }
        case Qt::TextAlignmentRole:
            return numeric ? QVariant(Qt::AlignRight | Qt::AlignVCenter) : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        case Qt::ForegroundRole:
            if (c.kind == MaColumn::Kind::Verified)
                return QColor(ma_model_verified(r) ? ui::colors::SUCCESS() : ui::colors::WARNING());
            if (c.kind == MaColumn::Kind::Parsed && !r.value(QStringLiteral("deal_id")).toString().isEmpty())
                return QColor(ui::colors::SUCCESS());
            return {};
        case Qt::ToolTipRole: {
            if (c.kind == MaColumn::Kind::Verified) {
                if (ma_model_verified(r))
                    return tr("Checked by you on %1").arg(r.value(QStringLiteral("verified_at")).toString());
                QStringList lines = {tr("Auto-extracted from an SEC filing and not checked yet. Open the filing, "
                                        "then Edit and Save (or Mark verified) once the figures are right.")};
                for (const QJsonValue& q : r.value(QStringLiteral("quality_issues")).toArray())
                    lines << QStringLiteral("- ") + q.toString();
                return lines.join(QLatin1Char('\n'));
            }
            const QString text = display_text(index.row(), index.column());
            return text.size() > 30 ? text : QVariant();
        }
        default:
            return {};
    }
}

QVariant MaDealsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < columns_.size())
        return columns_.at(section).header;
    return QAbstractTableModel::headerData(section, orientation, role);
}

} // namespace fincept::screens
