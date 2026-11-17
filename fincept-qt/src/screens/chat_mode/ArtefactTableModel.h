#pragma once
// ArtefactTableModel.h — a read-only table model over an artefact_logic::
// ArtefactTable for QTableView (tool-result cards, the ARTEFACTS preview). Model /
// view, so a 100 000-row table costs only the rows on screen; numbers align right
// and sort as numbers. Header-only, no Q_OBJECT (nothing for moc).

#include "services/agent_tasks/ArtefactLogic.h"

#include <QAbstractTableModel>
#include <QCoreApplication>
#include <QLocale>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace fincept::chat_mode {

class ArtefactTableModel : public QAbstractTableModel {
  public:
    explicit ArtefactTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    /// Show `table`; `max_rows` > 0 shows only the first rows (total_rows() keeps the count).
    void set_table(const agent_tasks::artefact_logic::ArtefactTable& table, int max_rows = -1) {
        beginResetModel();
        table_ = table;
        total_rows_ = static_cast<int>(table_.rows.size());
        if (max_rows > 0 && table_.rows.size() > max_rows)
            table_.rows.resize(max_rows);
        numeric_.assign(table_.columns.size(), true);
        for (int c = 0; c < table_.columns.size(); ++c) {
            int seen = 0;
            for (int r = 0; r < table_.rows.size() && seen < 50; ++r) {
                const QString& v = table_.rows.at(r).value(c);
                if (v.trimmed().isEmpty())
                    continue;
                ++seen;
                if (!agent_tasks::artefact_logic::looks_numeric(v)) {
                    numeric_[static_cast<size_t>(c)] = false;
                    break;
                }
            }
            if (seen == 0)
                numeric_[static_cast<size_t>(c)] = false;
        }
        endResetModel();
    }

    int total_rows() const { return total_rows_; }
    bool is_cut() const { return total_rows_ > table_.rows.size(); }
    const agent_tasks::artefact_logic::ArtefactTable& table() const { return table_; }

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(table_.rows.size());
    }
    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(table_.columns.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= table_.rows.size())
            return {};
        const QString& v = table_.rows.at(index.row()).value(index.column());
        switch (role) {
            case Qt::DisplayRole:
                return v;
            case Qt::ToolTipRole:
                return v.size() > 40 ? QVariant(v) : QVariant();
            case Qt::TextAlignmentRole:
                return QVariant::fromValue(Qt::Alignment((is_numeric(index.column()) ? Qt::AlignRight : Qt::AlignLeft) |
                                                         Qt::AlignVCenter));
            default:
                return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role == Qt::DisplayRole) {
            if (orientation == Qt::Horizontal)
                return table_.columns.value(section);
            return section + 1;
        }
        if (role == Qt::TextAlignmentRole && orientation == Qt::Horizontal)
            return QVariant::fromValue(Qt::Alignment((is_numeric(section) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter));
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    void sort(int column, Qt::SortOrder order) override {
        if (column < 0 || column >= table_.columns.size())
            return;
        emit layoutAboutToBeChanged();
        const bool numeric = is_numeric(column);
        QVector<double> keys;
        if (numeric) {
            keys.reserve(table_.rows.size());
            for (const QStringList& r : std::as_const(table_.rows))
                keys.append(number_of(r.value(column)));
        }
        QVector<int> idx(table_.rows.size());
        std::iota(idx.begin(), idx.end(), 0);
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            bool less = false;
            if (numeric) {
                const double x = keys.at(a), y = keys.at(b);
                less = order == Qt::AscendingOrder ? x < y : x > y;
            } else {
                const int c = QString::localeAwareCompare(table_.rows.at(a).value(column), table_.rows.at(b).value(column));
                less = order == Qt::AscendingOrder ? c < 0 : c > 0;
            }
            return less;
        });
        QVector<QStringList> rows;
        rows.reserve(idx.size());
        for (int i : std::as_const(idx))
            rows.append(table_.rows.at(i));
        table_.rows = rows;
        emit layoutChanged();
    }

  private:
    bool is_numeric(int column) const {
        return column >= 0 && static_cast<size_t>(column) < numeric_.size() && numeric_[static_cast<size_t>(column)];
    }

    static double number_of(QString t) {
        t = t.trimmed();
        bool neg = t.startsWith(QLatin1Char('(')) && t.endsWith(QLatin1Char(')'));
        QString digits;
        for (const QChar c : std::as_const(t))
            if (c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char('-'))
                digits += c;
        bool ok = false;
        double v = QLocale::c().toDouble(digits, &ok);
        if (!ok)
            return -std::numeric_limits<double>::infinity(); // empties sort first
        return neg ? -std::abs(v) : v;
    }

    agent_tasks::artefact_logic::ArtefactTable table_;
    std::vector<bool> numeric_;
    int total_rows_ = 0;
};

} // namespace fincept::chat_mode
