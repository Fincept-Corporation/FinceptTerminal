#pragma once
// EconPageModel — the page of an economics result that EconPanelBase's table shows: a model over the
// rows of the current page (at most the pager's 250), in the display order the analysis or a header
// click chose. No QTableWidgetItem per cell: a cell's text is formatted when its row is first
// painted and kept for the page; colours (the provider accent of the first column, gain / loss of
// change columns) are handed in by the panel and re-applied on an appearance change without a walk
// over the cells. Number formatting is handed in too (EconPanelBase passes the user's number locale),
// so this header depends on QtCore / QtGui / QtWidgets only and a standalone harness can time it.
//
// Header-only and without Q_OBJECT (no signals of its own): no build-list entry needed.

#include "screens/economics/panels/EconTableAnalysis.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QJsonArray>
#include <QTableView>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::screens::econ_table {

class EconPageModel : public QAbstractTableModel {
  public:
    /// Text of a number cell; `groupable` = not a date / identifier column (digit grouping allowed).
    using NumberText = std::function<QString(double value, bool groupable)>;

    using QAbstractTableModel::QAbstractTableModel;

    void set_number_text(NumberText fn) { number_text_ = std::move(fn); }

    /// Accent of the first column, gain and loss colours of change columns.
    void set_colors(const QColor& accent, const QColor& up, const QColor& down) {
        accent_ = accent;
        up_ = up;
        down_ = down;
        if (rowCount() > 0 && columnCount() > 0)
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::ForegroundRole});
    }

    /// A new result: rows (objects), columns in display order, display order of the rows. Shows no
    /// page until set_page().
    void set_table(const QJsonArray& rows, const QStringList& columns, const QVector<int>& order) {
        beginResetModel();
        rows_ = rows;
        columns_ = columns;
        order_ = order;
        groupable_.resize(columns_.size());
        signed_.resize(columns_.size());
        for (int c = 0; c < columns_.size(); ++c) {
            groupable_[c] = !is_date_key(columns_[c]) && !is_id_like(columns_[c]);
            signed_[c] = groupable_[c] && is_change_key(columns_[c]);
        }
        offset_ = 0;
        count_ = 0;
        cells_.clear();
        endResetModel();
    }

    /// The rows of the table in a new display order (a header click); the page shows from `offset`.
    void set_order(const QVector<int>& order) {
        beginResetModel();
        order_ = order;
        cells_.clear();
        cells_.resize(count_);
        endResetModel();
    }

    /// Show `count` rows from display position `offset`.
    void set_page(int offset, int count) {
        beginResetModel();
        offset_ = std::max(0, offset);
        count_ = std::clamp(count, 0, std::max(0, total_rows() - offset_));
        cells_.clear();
        cells_.resize(count_);
        endResetModel();
    }

    /// Drop the formatted text (the number locale changed); colours need set_colors().
    void reformat() {
        if (count_ == 0)
            return;
        cells_.clear();
        cells_.resize(count_);
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }

    void clear() {
        set_table({}, {}, {});
    }

    int total_rows() const { return static_cast<int>(order_.isEmpty() ? rows_.size() : order_.size()); }
    int page_offset() const { return offset_; }
    const QStringList& columns() const { return columns_; }
    const QJsonArray& rows() const { return rows_; }
    const QVector<int>& order() const { return order_; }

    /// Index into rows() of display row `view_row` of the page; -1 when out of range.
    int source_row(int view_row) const {
        const int pos = offset_ + view_row;
        if (view_row < 0 || view_row >= count_ || pos >= total_rows())
            return -1;
        return order_.isEmpty() ? pos : order_[pos];
    }

    /// Display text of a cell of the page.
    QString text(int view_row, int column) const {
        if (view_row < 0 || view_row >= count_ || column < 0 || column >= columns_.size())
            return {};
        return page_row(view_row).value(column).text;
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { return parent.isValid() ? 0 : count_; }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(columns_.size());
    }

    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= count_ || idx.column() >= columns_.size())
            return {};
        const Cell& cell = page_row(idx.row())[idx.column()];
        switch (role) {
            case Qt::DisplayRole:
                return cell.text;
            case Qt::ToolTipRole:
                return cell.tip.isEmpty() ? QVariant() : QVariant(cell.tip);
            case Qt::TextAlignmentRole:
                // Numeric columns are right-aligned.
                return static_cast<int>((cell.numeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
            case Qt::ForegroundRole:
                if (idx.column() == 0)
                    return accent_.isValid() ? QVariant(accent_) : QVariant();
                if (cell.numeric && signed_.value(idx.column()) && std::isfinite(cell.num) && cell.num != 0.0) {
                    const QColor& c = cell.num > 0.0 ? up_ : down_;
                    return c.isValid() ? QVariant(c) : QVariant();
                }
                return {};
            default:
                return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < columns_.size())
            return columns_[section];
        return QAbstractTableModel::headerData(section, orientation, role);
    }

  private:
    struct Cell {
        QString text;
        QString tip;
        bool numeric = false;
        double num = 0.0;
    };

    const QVector<Cell>& page_row(int view_row) const {
        QVector<Cell>& cached = cells_[view_row];
        if (cached.isEmpty() && !columns_.isEmpty()) {
            const int src = source_row(view_row);
            const QJsonObject obj = src >= 0 ? rows_[src].toObject() : QJsonObject();
            cached.resize(columns_.size());
            for (int c = 0; c < columns_.size(); ++c)
                cached[c] = make_cell(obj.value(columns_[c]), c);
        }
        return cached;
    }

    QString number(double v, bool groupable) const {
        return number_text_ ? number_text_(v, groupable) : QString::number(v, 'g', 12);
    }

    Cell make_cell(const QJsonValue& jv, int c) const {
        Cell cell;
        if (jv.isDouble()) {
            cell.numeric = true;
            cell.num = jv.toDouble();
            // Exact integers in full, grouped from five digits up (years and codes never are);
            // fractions without float noise — in the user's number locale.
            cell.text = number(cell.num, groupable_[c]);
        } else if (jv.isNull() || jv.isUndefined()) {
            cell.text = QStringLiteral("—"); // explicit gap, never a silent 0
        } else if (jv.isBool()) {
            cell.text = jv.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (jv.isObject() || jv.isArray()) {
            // An unexpected nested value: shown compactly rather than as a blank cell.
            cell.tip = cell_text(jv);
            cell.text = cell.tip.size() > 120 ? cell.tip.left(117) + QStringLiteral("…") : cell.tip;
        } else {
            cell.text = jv.toString();
            // Strings that are purely numeric (FRED / BLS send values as strings) still belong in a
            // right-aligned column, formatted like numbers when they are plain decimals.
            bool ok = false;
            cell.num = cell.text.toDouble(&ok);
            cell.numeric = ok && !cell.text.isEmpty();
            double plain = 0.0;
            if (cell.numeric && groupable_[c] && plain_number_string(cell.text.trimmed(), &plain)) {
                cell.num = plain;
                cell.text = number(plain, true);
            }
        }
        return cell;
    }

    QJsonArray rows_;
    QStringList columns_;
    QVector<int> order_;
    QVector<bool> groupable_;
    QVector<bool> signed_;
    int offset_ = 0;
    int count_ = 0;
    mutable QVector<QVector<Cell>> cells_; ///< per page row, formatted on first use
    NumberText number_text_;
    QColor accent_, up_, down_;
};

/// Width for every column of the page: its header and the first rows' text, at most `cap_px` (one
/// pass after a fill — never resize-to-contents while filling). Widths stay user-adjustable.
inline void fit_page_columns(QTableView* view, const EconPageModel* model, int cap_px) {
    QHeaderView* h = view->horizontalHeader();
    const QFontMetrics cell_fm(view->font());
    QFont hf = h->font();
    hf.setBold(true);
    const QFontMetrics head_fm(hf);
    const int rows = std::min(model->rowCount(), 80);
    const int pad = cell_fm.horizontalAdvance(QLatin1Char('M')) * 2 + 4;
    const int min_w = cell_fm.horizontalAdvance(QStringLiteral("0000")) + pad;
    for (int c = 0; c < model->columnCount(); ++c) {
        int w = head_fm.horizontalAdvance(model->columns().value(c)) + pad;
        for (int r = 0; r < rows; ++r)
            w = std::max(w, cell_fm.horizontalAdvance(model->text(r, c)) + pad);
        h->resizeSection(c, std::clamp(w, min_w, std::max(min_w, cap_px)));
    }
}

} // namespace fincept::screens::econ_table
