// CurrencyOverrideModel.cpp — see CurrencyOverrideModel.h.

#include "ui/formatting/CurrencyOverrideModel.h"

#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"

#include <QFont>
#include <QTimer>

#include <algorithm>

namespace fincept::ui::formatting {

CurrencyOverrideModel::CurrencyOverrideModel(QObject* parent) : QAbstractTableModel(parent) {
    table_codes_ = known_currency_codes();
    custom_codes_ = custom_codes();
    // Mechanism A: cell texts follow every format change without a reset.
    refresh_model_on_format_change(this);
    // Custom codes come and go with the overrides: insert / remove those rows.
    connect(&FormatSettings::instance(), &FormatSettings::changed, this, [this]() {
        QTimer::singleShot(0, this, [this]() { sync_rows(); });
    });
}

QStringList CurrencyOverrideModel::custom_codes() {
    QStringList out;
    const auto ov = FormatSettings::instance().currency_overrides();
    for (auto it = ov.constBegin(); it != ov.constEnd(); ++it)
        if (!is_known_currency(it.key()))
            out.append(it.key());
    std::sort(out.begin(), out.end());
    return out;
}

void CurrencyOverrideModel::sync_rows() {
    const QStringList now = custom_codes();
    if (now == custom_codes_)
        return;
    const int base = int(table_codes_.size());
    if (!custom_codes_.isEmpty()) {
        beginRemoveRows({}, base, base + int(custom_codes_.size()) - 1);
        custom_codes_.clear();
        endRemoveRows();
    }
    if (!now.isEmpty()) {
        beginInsertRows({}, base, base + int(now.size()) - 1);
        custom_codes_ = now;
        endInsertRows();
    }
}

int CurrencyOverrideModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(table_codes_.size() + custom_codes_.size());
}

int CurrencyOverrideModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QString CurrencyOverrideModel::code_at(int row) const {
    if (row < 0)
        return {};
    if (row < table_codes_.size())
        return table_codes_.at(row);
    const int c = row - int(table_codes_.size());
    return c < custom_codes_.size() ? custom_codes_.at(c) : QString();
}

int CurrencyOverrideModel::row_of(const QString& code) const {
    const qsizetype t = table_codes_.indexOf(code);
    if (t >= 0)
        return int(t);
    const qsizetype c = custom_codes_.indexOf(code);
    return c >= 0 ? int(table_codes_.size() + c) : -1;
}

QVariant CurrencyOverrideModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    const QString code = code_at(index.row());
    if (code.isEmpty())
        return {};
    CurrencySpec table;
    const bool known = find_currency(code, &table);
    CurrencyOverride o;
    const bool overridden = FormatSettings::instance().currency_override(code, &o);
    CurrencySpec eff;
    effective_currency(code, &eff);

    if (role == OverriddenRole)
        return overridden;
    switch (index.column()) {
        case ColCode:
            if (role == Qt::DisplayRole || role == SortRole || role == Qt::AccessibleTextRole)
                return code;
            if (role == Qt::ToolTipRole && !known)
                return tr("Custom code");
            break;
        case ColName:
            if (role == Qt::DisplayRole || role == Qt::EditRole || role == SortRole)
                return eff.name;
            break;
        case ColSymbol:
            if (role == Qt::DisplayRole || role == Qt::EditRole || role == SortRole || role == Qt::AccessibleTextRole)
                return eff.symbol;
            if (role == Qt::ToolTipRole)
                return overridden && !o.symbol.isEmpty()
                           ? tr("Your symbol. The table's is \"%1\".").arg(known ? table.symbol : code)
                           : tr("Table symbol. Double-click to change it.");
            if (role == Qt::FontRole && overridden && !o.symbol.isEmpty()) {
                QFont f;
                f.setBold(true);
                return f;
            }
            break;
        case ColDecimals:
            if (role == Qt::EditRole)
                return o.decimals;
            if (role == SortRole)
                return eff.decimals;
            if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole)
                return o.decimals >= 0 ? QString::number(o.decimals)
                                       : tr("default (%1)").arg(known ? table.decimals : 2);
            if (role == Qt::FontRole && o.decimals >= 0) {
                QFont f;
                f.setBold(true);
                return f;
            }
            break;
        case ColExample:
            if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole)
                return eff.minor_divisor > 1 ? price_with_symbol(117.7, InstrumentFmt{code}) : money(1234.5, code);
            break;
        case ColReset:
            if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole)
                return overridden ? (known ? tr("Reset") : tr("Remove")) : QString();
            if (role == Qt::ToolTipRole && overridden)
                return known ? tr("Back to the table's symbol and decimals") : tr("Remove this custom code");
            break;
        default:
            break;
    }
    if (role == Qt::TextAlignmentRole && (index.column() == ColDecimals || index.column() == ColExample))
        return int(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

QVariant CurrencyOverrideModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColCode:
            return tr("Code");
        case ColName:
            return tr("Name");
        case ColSymbol:
            return tr("Symbol");
        case ColDecimals:
            return tr("Decimals");
        case ColExample:
            return tr("Example");
        case ColReset:
            return QString();
        default:
            return {};
    }
}

Qt::ItemFlags CurrencyOverrideModel::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    const QString code = code_at(index.row());
    if (index.column() == ColSymbol || index.column() == ColDecimals ||
        (index.column() == ColName && !is_known_currency(code)))
        f |= Qt::ItemIsEditable;
    return f;
}

bool CurrencyOverrideModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    const QString code = code_at(index.row());
    if (code.isEmpty())
        return false;
    CurrencyOverride o;
    FormatSettings::instance().currency_override(code, &o);
    switch (index.column()) {
        case ColSymbol:
            o.symbol = value.toString().trimmed();
            break;
        case ColDecimals: {
            bool ok = false;
            const int d = value.toInt(&ok);
            if (!ok)
                return false;
            o.decimals = d;
            break;
        }
        case ColName:
            o.name = value.toString().trimmed();
            break;
        default:
            return false;
    }
    // One setter call; the cell refreshes through the format-change hook.
    return FormatSettings::instance().set_currency_override(code, o);
}

void CurrencyOverrideModel::reset_row(int row) {
    const QString code = code_at(row);
    if (!code.isEmpty())
        FormatSettings::instance().remove_currency_override(code);
}

} // namespace fincept::ui::formatting
