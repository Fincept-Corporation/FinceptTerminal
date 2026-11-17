// src/ui/widgets/DataGridEditor.cpp — see DataGridEditor.h.
//
// File-scope helpers carry a dge_ prefix (this file may be compiled in a unity batch).
#include "ui/widgets/DataGridEditor.h"

#include "ui/formatting/AmountInput.h"
#include "ui/theme/Theme.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <map>

namespace fincept::ui {

// ── Column factories ─────────────────────────────────────────────────────────

DataGridColumn DataGridColumn::date(const QString& key, const QString& title, bool required) {
    DataGridColumn c;
    c.key = key;
    c.title = title;
    c.type = Type::Date;
    c.required = required;
    return c;
}

DataGridColumn DataGridColumn::number(const QString& key, const QString& title, int decimals, bool required) {
    DataGridColumn c;
    c.key = key;
    c.title = title;
    c.type = Type::Number;
    c.decimals = decimals;
    c.required = required;
    return c;
}

DataGridColumn DataGridColumn::amount(const QString& key, const QString& title, const QString& currency, int decimals,
                                      bool required) {
    DataGridColumn c = number(key, title, decimals, required);
    c.money = true;
    c.currency = currency;
    return c;
}

DataGridColumn DataGridColumn::text(const QString& key, const QString& title, bool required) {
    DataGridColumn c;
    c.key = key;
    c.title = title;
    c.type = Type::Text;
    c.required = required;
    return c;
}

DataGridColumn DataGridColumn::choice(const QString& key, const QString& title, const QStringList& choices,
                                      const QStringList& labels, bool required) {
    DataGridColumn c;
    c.key = key;
    c.title = title;
    c.type = Type::Choice;
    c.choices = choices;
    c.choice_labels = labels;
    c.required = required;
    return c;
}

// ── Parsing ──────────────────────────────────────────────────────────────────

bool DataGridEditor::parse_number(const QString& text, double* out) {
    QString s = text.trimmed();
    if (s.isEmpty())
        return false;
    s.replace(QChar(0x2212), QLatin1Char('-')); // unicode minus
    bool negative = false;
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        negative = true;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    // Currency codes / symbols and grouping characters that are not separators.
    static const QRegularExpression kLeadAlpha(QStringLiteral("^[A-Za-z]{1,4}\\s*"));
    static const QRegularExpression kTrailAlpha(QStringLiteral("\\s*[A-Za-z]{2,4}$"));
    s.remove(kLeadAlpha);
    s.remove(kTrailAlpha);
    QString t;
    t.reserve(s.size());
    for (const QChar ch : s) {
        if (ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char(',') || ch == QLatin1Char('-') ||
            ch == QLatin1Char('+') || ch == QLatin1Char('e') || ch == QLatin1Char('E') || ch == QLatin1Char('%'))
            t.append(ch);
        else if (ch.isSpace() || ch == QLatin1Char('\'') || ch == QChar(0x00A0) || ch == QChar(0x202F) ||
                 ch.category() == QChar::Symbol_Currency)
            continue;
        else
            return false;
    }
    if (t.endsWith(QLatin1Char('%')))
        t.chop(1);
    if (t.endsWith(QLatin1Char('-')) && t.size() > 1 && !t.startsWith(QLatin1Char('-'))) { // "1000-"
        t.chop(1);
        negative = !negative;
    }
    if (t.isEmpty() || t.contains(QLatin1Char('%')))
        return false;
    const int commas = static_cast<int>(t.count(QLatin1Char(',')));
    const int dots = static_cast<int>(t.count(QLatin1Char('.')));
    if (commas > 0 && dots > 0) {
        // The separator that comes last is the decimal one.
        if (t.lastIndexOf(QLatin1Char(',')) > t.lastIndexOf(QLatin1Char('.'))) {
            t.remove(QLatin1Char('.'));
            t.replace(QLatin1Char(','), QLatin1Char('.'));
        } else {
            t.remove(QLatin1Char(','));
        }
    } else if (commas == 1) {
        const int at = static_cast<int>(t.indexOf(QLatin1Char(',')));
        const QString before = t.left(at);
        const int digits_after = static_cast<int>(t.size()) - at - 1;
        const bool grouping = digits_after == 3 && !before.isEmpty() && before != QLatin1String("0") &&
                              before != QLatin1String("-0") && before != QLatin1String("-");
        if (grouping)
            t.remove(QLatin1Char(','));
        else
            t.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (commas > 1) {
        t.remove(QLatin1Char(','));
    } else if (dots > 1) {
        t.remove(QLatin1Char('.')); // 1.000.000
    }
    bool ok = false;
    double v = QLocale::c().toDouble(t, &ok);
    if (!ok || !std::isfinite(v))
        return false;
    if (negative)
        v = -v;
    if (out)
        *out = v;
    return true;
}

static int dge_month_from_name(const QString& name) {
    static const QStringList kMonths = {QStringLiteral("jan"), QStringLiteral("feb"), QStringLiteral("mar"),
                                        QStringLiteral("apr"), QStringLiteral("may"), QStringLiteral("jun"),
                                        QStringLiteral("jul"), QStringLiteral("aug"), QStringLiteral("sep"),
                                        QStringLiteral("oct"), QStringLiteral("nov"), QStringLiteral("dec")};
    const QString n = name.left(3).toLower();
    const int i = static_cast<int>(kMonths.indexOf(n));
    return i < 0 ? 0 : i + 1;
}

static int dge_full_year(int y) {
    if (y >= 100)
        return y;
    return y < 70 ? 2000 + y : 1900 + y;
}

// True when the system's short date format puts the day before the month.
static bool dge_day_first() {
    const QString f = QLocale::system().dateFormat(QLocale::ShortFormat);
    const auto d = f.indexOf(QLatin1Char('d'));
    const auto m = f.indexOf(QLatin1Char('M'));
    return d >= 0 && m >= 0 && d < m;
}

bool DataGridEditor::parse_date(const QString& text, QDate* out) {
    QString s = text.trimmed();
    if (s.isEmpty())
        return false;
    // Date-times: keep the date part ("2020-03-31T00:00:00", "2020-03-31 00:00").
    static const QRegularExpression kIsoPrefix(QStringLiteral("^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})(?:[T ].*)?$"));
    static const QRegularExpression kNumeric(QStringLiteral("^(\\d{1,2})[-/.](\\d{1,2})[-/.](\\d{2}|\\d{4})$"));
    static const QRegularExpression kCompact(QStringLiteral("^(\\d{4})(\\d{2})(\\d{2})$"));
    static const QRegularExpression kDayName(
        QStringLiteral("^(\\d{1,2})[\\s\\-/.]+([A-Za-z]{3,9})\\.?[\\s\\-/.,]+(\\d{2}|\\d{4})$"));
    static const QRegularExpression kNameDay(
        QStringLiteral("^([A-Za-z]{3,9})\\.?[\\s\\-/.]+(\\d{1,2})(?:st|nd|rd|th)?[\\s,\\-/.]+(\\d{2}|\\d{4})$"));
    QDate d;
    QRegularExpressionMatch m;
    if ((m = kIsoPrefix.match(s)).hasMatch()) {
        d = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    } else if ((m = kNumeric.match(s)).hasMatch()) {
        const int a = m.captured(1).toInt();
        const int b = m.captured(2).toInt();
        const int y = dge_full_year(m.captured(3).toInt());
        bool day_first = dge_day_first();
        if (a > 12)
            day_first = true;
        else if (b > 12)
            day_first = false;
        d = day_first ? QDate(y, b, a) : QDate(y, a, b);
    } else if ((m = kCompact.match(s)).hasMatch()) {
        d = QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    } else if ((m = kDayName.match(s)).hasMatch()) {
        const int mon = dge_month_from_name(m.captured(2));
        if (mon > 0)
            d = QDate(dge_full_year(m.captured(3).toInt()), mon, m.captured(1).toInt());
    } else if ((m = kNameDay.match(s)).hasMatch()) {
        const int mon = dge_month_from_name(m.captured(1));
        if (mon > 0)
            d = QDate(dge_full_year(m.captured(3).toInt()), mon, m.captured(2).toInt());
    }
    if (!d.isValid())
        return false;
    if (out)
        *out = d;
    return true;
}

QVector<QStringList> DataGridEditor::parse_delimited(const QString& text_in) {
    QString text = text_in;
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QChar delim = QLatin1Char(',');
    if (text.contains(QLatin1Char('\t'))) {
        delim = QLatin1Char('\t');
    } else if (text.contains(QLatin1Char(';'))) {
        // ';' when every non-empty line has the same number (>= 1) of them — the European CSV
        // dialect, whose decimals are commas.
        int expect = -1;
        bool same = true;
        for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int n = static_cast<int>(line.count(QLatin1Char(';')));
            if (expect < 0)
                expect = n;
            else if (n != expect)
                same = false;
        }
        if (same && expect >= 1)
            delim = QLatin1Char(';');
    }

    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (quoted) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    cell.append(QLatin1Char('"'));
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cell.append(ch);
            }
            continue;
        }
        if (ch == QLatin1Char('"') && cell.trimmed().isEmpty()) {
            quoted = true;
            cell.clear();
        } else if (ch == delim) {
            row << cell.trimmed();
            cell.clear();
        } else if (ch == QLatin1Char('\n')) {
            row << cell.trimmed();
            cell.clear();
            rows << row;
            row.clear();
        } else {
            cell.append(ch);
        }
    }
    if (!cell.isEmpty() || !row.isEmpty()) {
        row << cell.trimmed();
        rows << row;
    }
    // Drop rows with nothing in them.
    QVector<QStringList> out;
    out.reserve(rows.size());
    for (const QStringList& r : rows) {
        const bool empty = std::all_of(r.begin(), r.end(), [](const QString& c) { return c.isEmpty(); });
        if (!empty)
            out << r;
    }
    return out;
}

// ── Cell validation (shared by the model and the editor) ─────────────────────

static QString dge_canonical_choice(const DataGridColumn& c, const QString& raw) {
    const QString v = raw.trimmed();
    if (v.isEmpty())
        return {};
    for (int i = 0; i < c.choices.size(); ++i) {
        if (c.choices[i].compare(v, Qt::CaseInsensitive) == 0)
            return c.choices[i];
        if (i < c.choice_labels.size() && c.choice_labels[i].compare(v, Qt::CaseInsensitive) == 0)
            return c.choices[i];
    }
    const auto it = c.synonyms.constFind(v.toLower());
    return it != c.synonyms.constEnd() ? it.value() : QString();
}

static QString dge_choice_label(const DataGridColumn& c, const QString& value) {
    const auto i = c.choices.indexOf(value);
    return (i >= 0 && i < c.choice_labels.size()) ? c.choice_labels.at(i) : value;
}

/// A Number cell's value: parse_number(), or for a money column the shared amount reader (which
/// also refuses an amount in another currency than the column's). False with the reason in *why.
static bool dge_number(const DataGridColumn& c, const QString& raw, double* out, QString* why) {
    if (!c.money) {
        if (DataGridEditor::parse_number(raw, out))
            return true;
        if (why)
            *why = DataGridEditor::tr("%1 is not a number").arg(c.title.toLower());
        return false;
    }
    const fincept::ui::formatting::AmountInput a = fincept::ui::formatting::read_amount_input(raw, c.currency);
    if (!a.ok()) {
        if (why)
            *why = DataGridEditor::tr("%1: %2").arg(c.title.toLower(), a.error);
        return false;
    }
    if (out)
        *out = *a.value;
    return true;
}

/// "" when the cell is acceptable, else the plain reason.
static QString dge_cell_problem(const DataGridColumn& c, const QString& raw) {
    const QString v = raw.trimmed();
    const QString name = c.title.toLower();
    if (v.isEmpty())
        return c.required ? DataGridEditor::tr("%1 missing").arg(name) : QString();
    switch (c.type) {
    case DataGridColumn::Type::Date:
        return DataGridEditor::parse_date(v, nullptr) ? QString() : DataGridEditor::tr("%1 not recognised").arg(name);
    case DataGridColumn::Type::Number: {
        QString why;
        return dge_number(c, v, nullptr, &why) ? QString() : why;
    }
    case DataGridColumn::Type::Choice: {
        if (!dge_canonical_choice(c, v).isEmpty())
            return {};
        QStringList shown;
        for (const QString& ch : c.choices)
            shown << dge_choice_label(c, ch);
        return DataGridEditor::tr("%1 must be %2").arg(name, shown.join(QStringLiteral(" / ")));
    }
    case DataGridColumn::Type::Text:
        break;
    }
    return {};
}

// ── Model ────────────────────────────────────────────────────────────────────

class DataGridModel : public QAbstractTableModel {
  public:
    DataGridModel(const QVector<DataGridColumn>& columns, QObject* parent)
        : QAbstractTableModel(parent), cols_(columns) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows_.size());
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(cols_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= rows_.size() || index.column() >= cols_.size())
            return {};
        const DataGridColumn& c = cols_.at(index.column());
        const QString raw = rows_.at(index.row()).value(index.column());
        switch (role) {
        case Qt::EditRole:
            return raw;
        case Qt::DisplayRole: {
            if (raw.trimmed().isEmpty() || !dge_cell_problem(c, raw).isEmpty())
                return raw;
            if (c.type == DataGridColumn::Type::Number) {
                double v = 0;
                dge_number(c, raw, &v, nullptr);
                // A money column shows what its reader reads: the user's own number format.
                return c.money ? fincept::ui::formatting::number(v, c.decimals) : QLocale().toString(v, 'f', c.decimals);
            }
            if (c.type == DataGridColumn::Type::Date) {
                QDate d;
                DataGridEditor::parse_date(raw, &d);
                return d.toString(Qt::ISODate);
            }
            if (c.type == DataGridColumn::Type::Choice)
                return dge_choice_label(c, dge_canonical_choice(c, raw));
            return raw;
        }
        case Qt::TextAlignmentRole:
            return QVariant::fromValue(c.type == DataGridColumn::Type::Number ? (Qt::AlignRight | Qt::AlignVCenter)
                                                                              : (Qt::AlignLeft | Qt::AlignVCenter));
        case Qt::BackgroundRole:
            if (!row_problem(index.row()).isEmpty())
                return QBrush(colors::tint_color(QColor(colors::DANGER()), 0.20));
            return {};
        case Qt::FontRole:
            if (!dge_cell_problem(c, raw).isEmpty() && !is_empty_row(index.row())) {
                QFont f = fonts::font(-1, QFont::Bold);
                return f;
            }
            return {};
        case Qt::ToolTipRole: {
            const QString p = row_problem(index.row());
            return p.isEmpty() ? (c.tooltip.isEmpty() ? QVariant() : QVariant(c.tooltip))
                               : QVariant(DataGridEditor::tr("Ignored: %1").arg(p));
        }
        default:
            return {};
        }
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role) override {
        if (role != Qt::EditRole || !index.isValid() || index.row() >= rows_.size())
            return false;
        QStringList& r = rows_[index.row()];
        while (r.size() < cols_.size())
            r << QString();
        const QString v = value.toString();
        if (r[index.column()] == v)
            return false;
        r[index.column()] = v;
        // The whole row's tint / tooltip may change.
        emit dataChanged(this->index(index.row(), 0), this->index(index.row(), columnCount() - 1));
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid())
            return Qt::NoItemFlags;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation == Qt::Horizontal) {
            if (section < 0 || section >= cols_.size())
                return {};
            if (role == Qt::DisplayRole)
                return cols_.at(section).title;
            if (role == Qt::ToolTipRole && !cols_.at(section).tooltip.isEmpty())
                return cols_.at(section).tooltip;
            return {};
        }
        if (role == Qt::DisplayRole)
            return section + 1;
        return {};
    }

    bool is_empty_row(int r) const {
        const QStringList& row = rows_.at(r);
        return std::all_of(row.begin(), row.end(), [](const QString& c) { return c.trimmed().isEmpty(); });
    }

    /// "" for a valid or completely empty row, else the first problem.
    QString row_problem(int r) const {
        if (r < 0 || r >= rows_.size() || is_empty_row(r))
            return {};
        for (int c = 0; c < cols_.size(); ++c) {
            const QString p = dge_cell_problem(cols_.at(c), rows_.at(r).value(c));
            if (!p.isEmpty())
                return p;
        }
        return {};
    }

    void reset_rows(QVector<QStringList> rows) {
        beginResetModel();
        for (QStringList& r : rows) {
            while (r.size() < cols_.size())
                r << QString();
            while (r.size() > cols_.size())
                r.removeLast();
        }
        rows_ = std::move(rows);
        endResetModel();
    }

    void insert_blank(int at, int count) {
        if (count <= 0)
            return;
        at = std::clamp(at, 0, static_cast<int>(rows_.size()));
        beginInsertRows(QModelIndex(), at, at + count - 1);
        QStringList blank;
        for (int c = 0; c < cols_.size(); ++c) {
            const DataGridColumn& col = cols_.at(c);
            blank << (col.type == DataGridColumn::Type::Choice && !col.choices.isEmpty() ? col.choices.first()
                                                                                         : QString());
        }
        for (int i = 0; i < count; ++i)
            rows_.insert(at, blank);
        endInsertRows();
    }

    void remove_rows_desc(QList<int> rows) {
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows) {
            if (r < 0 || r >= rows_.size())
                continue;
            beginRemoveRows(QModelIndex(), r, r);
            rows_.removeAt(r);
            endRemoveRows();
        }
    }

    /// Overwrite rows [start_row, start_row + cells.size()) with one change notification (a cell-by-cell
    /// setData would re-validate and re-save the whole grid once per cell).
    void write_block(int start_row, const QVector<QStringList>& cells) {
        if (cells.isEmpty() || start_row < 0 || start_row + cells.size() > rows_.size())
            return;
        for (int i = 0; i < cells.size(); ++i) {
            QStringList& r = rows_[start_row + i];
            while (r.size() < cols_.size())
                r << QString();
            for (int c = 0; c < cols_.size(); ++c)
                r[c] = cells.at(i).value(c);
        }
        emit dataChanged(index(start_row, 0), index(start_row + static_cast<int>(cells.size()) - 1, columnCount() - 1));
    }

    /// Empty the given cells with one change notification.
    void clear_cells(const QModelIndexList& cells) {
        int r0 = INT_MAX, r1 = -1;
        for (const QModelIndex& i : cells) {
            if (!i.isValid() || i.row() >= rows_.size() || i.column() >= cols_.size())
                continue;
            QStringList& r = rows_[i.row()];
            while (r.size() < cols_.size())
                r << QString();
            r[i.column()].clear();
            r0 = std::min(r0, i.row());
            r1 = std::max(r1, i.row());
        }
        if (r1 >= 0)
            emit dataChanged(index(r0, 0), index(r1, columnCount() - 1));
    }

    void refresh_all() {
        if (!rows_.isEmpty())
            emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    }

    /// A column's rules changed (a money column's currency): every row is checked again.
    void set_column(int i, const DataGridColumn& c) {
        if (i < 0 || i >= cols_.size())
            return;
        cols_[i] = c;
        refresh_all();
    }

    const QVector<QStringList>& rows() const { return rows_; }
    QVector<QStringList>& rows_mut() { return rows_; }
    const QVector<DataGridColumn>& columns() const { return cols_; }

  private:
    QVector<DataGridColumn> cols_;
    QVector<QStringList> rows_;
};

// ── Choice delegate (a combo box editor for Choice columns) ──────────────────

class DgeChoiceDelegate : public QStyledItemDelegate {
  public:
    DgeChoiceDelegate(const QVector<DataGridColumn>& columns, QObject* parent)
        : QStyledItemDelegate(parent), cols_(columns) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        // The application style sheet styles item-view cells, and a styled cell ignores the model's
        // background colour: the tint of a row that will be ignored is painted here, under the text.
        const QVariant bg = index.data(Qt::BackgroundRole);
        if (bg.canConvert<QBrush>() && !(option.state & QStyle::State_Selected))
            painter->fillRect(option.rect, bg.value<QBrush>());
        QStyledItemDelegate::paint(painter, option, index);
    }

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        const DataGridColumn* c = column(index);
        if (!c || c->type != DataGridColumn::Type::Choice)
            return QStyledItemDelegate::createEditor(parent, option, index);
        auto* cb = new QComboBox(parent);
        for (int i = 0; i < c->choices.size(); ++i)
            cb->addItem(dge_choice_label(*c, c->choices.at(i)), c->choices.at(i));
        return cb;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        const DataGridColumn* c = column(index);
        auto* cb = qobject_cast<QComboBox*>(editor);
        if (!c || c->type != DataGridColumn::Type::Choice || !cb) {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }
        const QString v = dge_canonical_choice(*c, index.data(Qt::EditRole).toString());
        const int i = cb->findData(v);
        cb->setCurrentIndex(i < 0 ? 0 : i);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        const DataGridColumn* c = column(index);
        auto* cb = qobject_cast<QComboBox*>(editor);
        if (!c || c->type != DataGridColumn::Type::Choice || !cb) {
            QStyledItemDelegate::setModelData(editor, model, index);
            return;
        }
        model->setData(index, cb->currentData().toString(), Qt::EditRole);
    }

  private:
    const DataGridColumn* column(const QModelIndex& index) const {
        return (index.column() >= 0 && index.column() < cols_.size()) ? &cols_.at(index.column()) : nullptr;
    }
    QVector<DataGridColumn> cols_;
};

// ── Import worker result ─────────────────────────────────────────────────────

struct DgeImportResult {
    QVector<QStringList> cells;
    QString error;
};

static DgeImportResult dge_read_delimited_file(const QString& path) {
    DgeImportResult r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.error = DataGridEditor::tr("Could not open the file: %1").arg(f.errorString());
        return r;
    }
    constexpr qint64 kMaxBytes = 20LL * 1024 * 1024;
    if (f.size() > kMaxBytes) {
        r.error = DataGridEditor::tr("The file is larger than 20 MB - split it or paste the rows you need.");
        return r;
    }
    const QByteArray bytes = f.readAll();
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8(bytes);
    if (utf8.hasError()) // not UTF-8: a spreadsheet's ANSI export
        text = QString::fromLatin1(bytes);
    r.cells = DataGridEditor::parse_delimited(text);
    if (r.cells.isEmpty())
        r.error = DataGridEditor::tr("The file has no rows.");
    return r;
}

// ── Editor ───────────────────────────────────────────────────────────────────

DataGridEditor::DataGridEditor(const QVector<DataGridColumn>& columns, QWidget* parent)
    : QWidget(parent), columns_(columns) {
    setObjectName(QStringLiteral("dgeRoot"));
    restyle(); // before the children exist
    build_ui();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens&) {
        restyle();
        if (model_)
            model_->refresh_all();
        update_status();
    });
}

DataGridEditor::~DataGridEditor() = default;

namespace {

// Three or more columns do not fit a narrow form as equal shares: dates and amounts were cut to
// "2020-0..." / "1,000,...". Size each column for what it holds (in the current font), let the last one
// take the rest, and scroll sideways when the form is narrower than that. Two columns keep equal shares.
void dge_size_columns(QTableView* view, const QVector<DataGridColumn>& columns) {
    if (!view || !view->model() || columns.size() <= 2)
        return;
    QHeaderView* hh = view->horizontalHeader();
    const QFontMetrics fm(fonts::font(-1));
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setStretchLastSection(true);
    for (int c = 0; c < columns.size(); ++c) {
        const DataGridColumn& col = columns.at(c);
        int chars = 10; // a date, or a short note
        if (col.type == DataGridColumn::Type::Number) {
            chars = 12;
        } else if (col.type == DataGridColumn::Type::Choice) {
            chars = 6;
            for (const QString& label : col.choice_labels.isEmpty() ? col.choices : col.choice_labels)
                chars = std::max(chars, int(label.size()));
            chars += 1; // the drop-down arrow while editing
        }
        // 12 px = the cell's own text margins plus the grid line. More than that made a date / type /
        // amount grid wider than the form it sits in, and the amount column - the one that matters -
        // ended up cut off behind a sideways scroll bar.
        // A date fills its ten characters exactly, so the cell's text margins need the full room:
        // with 12 px a date was 3 px short and showed as "2019-01-…" (seen at 1280 and 1600 px).
        const int margins = col.type == DataGridColumn::Type::Date ? 20 : 12;
        view->setColumnWidth(c, fm.horizontalAdvance(QString(chars, QLatin1Char('0'))) + margins);
    }
}

} // namespace

void DataGridEditor::restyle() {
    // %1 base, %2 row alt, %3 primary text, %4 border; %5 cell font size;
    // %6 selection, %7 raised, %8 secondary text; %9 header font size;
    // %10 surface, %11 bright border, %12 hover, %13 tertiary text, %14 danger.
    // Sizes through px() (the number only; "px" is in the format), never a bare
    // font_px() int, so the re-style pass can follow a font change.
    setStyleSheet(
        QStringLiteral("#dgeRoot { background: transparent; }"
                       "QTableView#dgeTable { background: %1; alternate-background-color: %2; color: %3;"
                       "  gridline-color: %4; border: 1px solid %4; font-size: %5px;"
                       "  selection-background-color: %6; selection-color: %3; }"
                       "QTableView#dgeTable QHeaderView::section { background: %7; color: %8; border: none;"
                       "  border-right: 1px solid %4; border-bottom: 1px solid %4; padding: 3px 6px;"
                       "  font-size: %9px; font-weight: 700; }"
                       "QTableView#dgeTable QTableCornerButton::section { background: %7; border: none; }"
                       "QTableView#dgeTable QLineEdit { background: %10; color: %3; border: 1px solid %11;"
                       "  padding: 0px 4px; font-size: %5px; }"
                       "QTableView#dgeTable QComboBox { background: %10; color: %3; border: 1px solid %11;"
                       "  padding: 0px 4px; font-size: %5px; }"
                       "QPushButton#dgeBtn { background: transparent; color: %8; border: 1px solid %4;"
                       "  padding: 3px 8px; font-size: %9px; font-weight: 700; }"
                       "QPushButton#dgeBtn:hover { color: %3; border-color: %11; background: %12; }"
                       "QPushButton#dgeBtn:disabled { color: %13; }"
                       "QLabel#dgeStatus { color: %8; font-size: %9px; background: transparent; }"
                       "QLabel#dgeStatus[state=\"bad\"] { color: %14; }")
            .arg(colors::BG_BASE(), colors::ROW_ALT(), colors::TEXT_PRIMARY(), colors::BORDER_DIM())
            .arg(fonts::px(-1))
            .arg(colors::alpha(colors::AMBER, 0.22), colors::BG_RAISED(), colors::TEXT_SECONDARY())
            .arg(fonts::px(-3))
            .arg(colors::BG_SURFACE(), colors::BORDER_BRIGHT(), colors::BG_HOVER(), colors::TEXT_TERTIARY(),
                 colors::DANGER()));
    if (view_) {
        const int row_h = QFontMetrics(fonts::font(-1)).height() + 8;
        view_->verticalHeader()->setDefaultSectionSize(row_h);
        const int header_h = QFontMetrics(fonts::font(-3, QFont::Bold)).height() + 10;
        view_->setMinimumHeight(header_h + row_h * std::max(2, visible_rows_) + 4);
        dge_size_columns(view_, columns_);
    }
}

void DataGridEditor::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(4);
    auto make_btn = [this](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QStringLiteral("dgeBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };
    add_btn_ = make_btn(tr("+ ROW"), tr("Add a row (Insert)"));
    remove_btn_ = make_btn(tr("- ROW"), tr("Remove the selected rows (Ctrl+Delete)"));
    paste_btn_ = make_btn(tr("PASTE"), tr("Replace the rows with the table on the clipboard - copy the cells in "
                                          "Excel / Sheets first. Ctrl+V inside the grid pastes at the selected row."));
    import_btn_ = make_btn(tr("IMPORT CSV"), tr("Replace the rows with a CSV / TSV file"));
    add_btn_->setAccessibleName(tr("Add row"));
    remove_btn_->setAccessibleName(tr("Remove selected rows"));
    paste_btn_->setAccessibleName(tr("Paste rows from the clipboard"));
    import_btn_->setAccessibleName(tr("Import rows from a CSV file"));
    bar->addWidget(add_btn_);
    bar->addWidget(remove_btn_);
    bar->addStretch(1);
    bar->addWidget(paste_btn_);
    bar->addWidget(import_btn_);
    root->addLayout(bar);

    view_ = new QTableView(this);
    view_->setObjectName(QStringLiteral("dgeTable"));
    model_ = new DataGridModel(columns_, this);
    view_->setModel(model_);
    view_->setItemDelegate(new DgeChoiceDelegate(columns_, view_));
    view_->setAlternatingRowColors(true);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setSelectionBehavior(QAbstractItemView::SelectItems);
    view_->setEditTriggers(QAbstractItemView::AnyKeyPressed | QAbstractItemView::DoubleClicked |
                           QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    view_->setTabKeyNavigation(true);
    view_->setWordWrap(false);
    view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    dge_size_columns(view_, columns_); // three or more columns: sized for their content
    view_->horizontalHeader()->setHighlightSections(false);
    view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    view_->setAccessibleName(tr("Data grid"));
    view_->installEventFilter(this);
    root->addWidget(view_, 1);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("dgeStatus"));
    status_->setWordWrap(true);
    root->addWidget(status_);

    restyle(); // sizes that depend on the view

    connect(add_btn_, &QPushButton::clicked, this, &DataGridEditor::add_row);
    connect(remove_btn_, &QPushButton::clicked, this, &DataGridEditor::remove_selected_rows);
    connect(paste_btn_, &QPushButton::clicked, this, [this]() { paste_from_clipboard(/*replace=*/true); });
    connect(import_btn_, &QPushButton::clicked, this, &DataGridEditor::import_csv);

    auto on_changed = [this]() {
        update_status();
        emit changed();
    };
    connect(model_, &QAbstractItemModel::dataChanged, this, on_changed);
    connect(model_, &QAbstractItemModel::rowsInserted, this, on_changed);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, on_changed);
    connect(model_, &QAbstractItemModel::modelReset, this, on_changed);

    connect(view_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(tr("Add row"), this, [this]() { add_row(); });
        menu.addAction(tr("Remove selected rows"), this, [this]() { remove_selected_rows(); });
        menu.addAction(tr("Clear selected cells"), this, [this]() { clear_selected_cells(); });
        menu.addSeparator();
        menu.addAction(tr("Copy"), this, [this]() { copy_selection(); });
        menu.addAction(tr("Paste at this row"), this, [this]() { paste_from_clipboard(/*replace=*/false); });
        menu.addAction(tr("Paste, replacing every row"), this, [this]() { paste_from_clipboard(/*replace=*/true); });
        menu.addAction(tr("Import CSV..."), this, [this]() { import_csv(); });
        menu.addSeparator();
        menu.addAction(tr("Clear all rows"), this, [this]() { clear_rows(); });
        menu.exec(view_->viewport()->mapToGlobal(pos));
    });

    update_status();
}

void DataGridEditor::set_paste_adapter(PasteAdapter adapter) {
    adapter_ = std::move(adapter);
}

void DataGridEditor::set_max_rows(int rows) {
    max_rows_ = std::max(1, rows);
}

void DataGridEditor::set_visible_rows(int rows) {
    visible_rows_ = std::max(2, rows);
    restyle();
}

void DataGridEditor::set_hint(const QString& hint) {
    hint_ = hint;
    update_status();
}

void DataGridEditor::set_column_currency(const QString& key, const QString& currency) {
    for (int i = 0; i < columns_.size(); ++i) {
        DataGridColumn& c = columns_[i];
        if (c.key != key || !c.money || c.currency == currency)
            continue;
        c.currency = currency;
        model_->set_column(i, c);
        update_status();
    }
}

// ── Data ─────────────────────────────────────────────────────────────────────

void DataGridEditor::from_json(const QJsonArray& rows) {
    QVector<QStringList> out;
    out.reserve(std::min(static_cast<int>(rows.size()), max_rows_));
    for (const QJsonValue& v : rows) {
        if (out.size() >= max_rows_)
            break;
        const QJsonObject o = v.toObject();
        QStringList r;
        for (const DataGridColumn& c : columns_) {
            const QJsonValue cell = o.value(c.key);
            if (cell.isDouble() && c.money) // in the user's number format: the amount reader reads it back exactly
                r << fincept::ui::formatting::amount_input_text(cell.toDouble(), 6);
            else if (cell.isDouble())
                r << QString::number(cell.toDouble(), 'g', 15);
            else if (cell.isString())
                r << cell.toString();
            else
                r << QString();
        }
        out << r;
    }
    last_note_.clear();
    model_->reset_rows(out);
}

QJsonArray DataGridEditor::to_json() const {
    QJsonArray out;
    for (int r = 0; r < model_->rowCount(); ++r) {
        if (model_->is_empty_row(r) || !model_->row_problem(r).isEmpty())
            continue;
        QJsonObject o;
        const QStringList& row = model_->rows().at(r);
        for (int c = 0; c < columns_.size(); ++c) {
            const DataGridColumn& col = columns_.at(c);
            const QString raw = row.value(c).trimmed();
            if (raw.isEmpty())
                continue;
            switch (col.type) {
            case DataGridColumn::Type::Date: {
                QDate d;
                parse_date(raw, &d);
                o.insert(col.key, d.toString(Qt::ISODate));
                break;
            }
            case DataGridColumn::Type::Number: {
                double v = 0;
                dge_number(col, raw, &v, nullptr);
                o.insert(col.key, v);
                break;
            }
            case DataGridColumn::Type::Choice:
                o.insert(col.key, dge_canonical_choice(col, raw));
                break;
            case DataGridColumn::Type::Text:
                o.insert(col.key, raw);
                break;
            }
        }
        out.append(o);
    }
    return out;
}

QJsonArray DataGridEditor::raw_json() const {
    QJsonArray out;
    for (int r = 0; r < model_->rowCount(); ++r) {
        if (model_->is_empty_row(r))
            continue;
        QJsonObject o;
        const QStringList& row = model_->rows().at(r);
        for (int c = 0; c < columns_.size(); ++c)
            if (!row.value(c).isEmpty())
                o.insert(columns_.at(c).key, row.value(c));
        out.append(o);
    }
    return out;
}

int DataGridEditor::row_count() const {
    int n = 0;
    for (int r = 0; r < model_->rowCount(); ++r)
        if (!model_->is_empty_row(r))
            ++n;
    return n;
}

int DataGridEditor::valid_row_count() const {
    int n = 0;
    for (int r = 0; r < model_->rowCount(); ++r)
        if (!model_->is_empty_row(r) && model_->row_problem(r).isEmpty())
            ++n;
    return n;
}

int DataGridEditor::invalid_row_count() const {
    return row_count() - valid_row_count();
}

QString DataGridEditor::problems_text() const {
    // Group the ignored rows by reason: "2 rows ignored: date not recognised (rows 3, 7)".
    std::map<QString, QList<int>> by_reason;
    for (int r = 0; r < model_->rowCount(); ++r) {
        const QString p = model_->row_problem(r);
        if (!p.isEmpty())
            by_reason[p].append(r + 1);
    }
    if (by_reason.empty())
        return {};
    int total = 0;
    for (const auto& [reason, rows] : by_reason)
        total += static_cast<int>(rows.size());
    QStringList parts;
    for (const auto& [reason, rows] : by_reason) {
        QStringList nums;
        for (int i = 0; i < rows.size() && i < 6; ++i)
            nums << QString::number(rows.at(i));
        if (rows.size() > 6)
            nums << QStringLiteral("...");
        parts << tr("%1 (%2 %3)").arg(reason, rows.size() == 1 ? tr("row") : tr("rows"), nums.join(QStringLiteral(", ")));
    }
    return (total == 1 ? tr("1 row ignored: %1") : tr("%1 rows ignored: %2").arg(total))
        .arg(parts.join(QStringLiteral("; ")));
}

QStringList DataGridEditor::headers() const {
    QStringList h;
    for (const DataGridColumn& c : columns_)
        h << c.title;
    return h;
}

QVector<QStringList> DataGridEditor::rows_text() const {
    QVector<QStringList> out;
    for (int r = 0; r < model_->rowCount(); ++r) {
        if (model_->is_empty_row(r))
            continue;
        QStringList row;
        for (int c = 0; c < columns_.size(); ++c)
            row << model_->data(model_->index(r, c), Qt::DisplayRole).toString();
        out << row;
    }
    return out;
}

void DataGridEditor::update_status() {
    if (!status_)
        return;
    const QString problems = problems_text();
    QString text;
    if (!problems.isEmpty()) {
        text = problems;
    } else {
        const int n = row_count();
        text = n == 0 ? tr("No rows yet.") : n == 1 ? tr("1 row") : tr("%1 rows").arg(n);
        if (!last_note_.isEmpty())
            text += QStringLiteral("  \xB7  ") + last_note_;
        if (!hint_.isEmpty())
            text += QStringLiteral("  \xB7  ") + hint_;
    }
    const QString state = problems.isEmpty() ? QStringLiteral("ok") : QStringLiteral("bad");
    if (status_->property("state").toString() != state) {
        status_->setProperty("state", state);
        status_->style()->unpolish(status_);
        status_->style()->polish(status_);
    }
    status_->setText(text);
    remove_btn_->setEnabled(model_->rowCount() > 0);
}

// ── Actions ──────────────────────────────────────────────────────────────────

void DataGridEditor::add_row() {
    if (model_->rowCount() >= max_rows_)
        return;
    const QModelIndex cur = view_->currentIndex();
    const int at = cur.isValid() ? cur.row() + 1 : model_->rowCount();
    model_->insert_blank(at, 1);
    const QModelIndex idx = model_->index(at, 0);
    view_->setCurrentIndex(idx);
    view_->scrollTo(idx);
    view_->setFocus(Qt::OtherFocusReason);
}

void DataGridEditor::remove_selected_rows() {
    QSet<int> rows;
    for (const QModelIndex& i : view_->selectionModel()->selectedIndexes())
        rows.insert(i.row());
    if (rows.isEmpty() && view_->currentIndex().isValid())
        rows.insert(view_->currentIndex().row());
    if (rows.isEmpty())
        return;
    model_->remove_rows_desc(QList<int>(rows.begin(), rows.end()));
}

void DataGridEditor::clear_rows() {
    last_note_.clear();
    model_->reset_rows({});
}

void DataGridEditor::clear_selected_cells() {
    model_->clear_cells(view_->selectionModel()->selectedIndexes());
}

void DataGridEditor::copy_selection() const {
    const QModelIndexList sel = view_->selectionModel()->selectedIndexes();
    if (sel.isEmpty())
        return;
    int r0 = INT_MAX, r1 = -1, c0 = INT_MAX, c1 = -1;
    for (const QModelIndex& i : sel) {
        r0 = std::min(r0, i.row());
        r1 = std::max(r1, i.row());
        c0 = std::min(c0, i.column());
        c1 = std::max(c1, i.column());
    }
    QStringList lines;
    for (int r = r0; r <= r1; ++r) {
        QStringList cells;
        for (int c = c0; c <= c1; ++c)
            cells << model_->data(model_->index(r, c), Qt::EditRole).toString();
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

QVector<QStringList> DataGridEditor::adapt(const QVector<QStringList>& cells) const {
    if (adapter_)
        return adapter_(cells);
    return cells;
}

int DataGridEditor::apply_cells(const QVector<QStringList>& cells_in, bool replace, int start_row) {
    QVector<QStringList> cells = adapt(cells_in);
    // A header row ("Date, Amount"): every date / number cell of the first row fails to parse.
    if (!cells.isEmpty()) {
        int typed = 0, failed = 0;
        const QStringList& first = cells.first();
        for (int c = 0; c < columns_.size() && c < first.size(); ++c) {
            const DataGridColumn& col = columns_.at(c);
            if (col.type != DataGridColumn::Type::Date && col.type != DataGridColumn::Type::Number)
                continue;
            if (first.at(c).trimmed().isEmpty())
                continue;
            ++typed;
            const bool ok = col.type == DataGridColumn::Type::Date ? parse_date(first.at(c), nullptr)
                                                                   : dge_number(col, first.at(c), nullptr, nullptr);
            if (!ok)
                ++failed;
        }
        if (typed > 0 && failed == typed)
            cells.removeFirst();
    }
    if (cells.isEmpty())
        return 0;

    int dropped = 0;
    if (replace) {
        if (cells.size() > max_rows_) {
            dropped = static_cast<int>(cells.size()) - max_rows_;
            cells.resize(max_rows_);
        }
        model_->reset_rows(cells);
    } else {
        start_row = std::clamp(start_row, 0, model_->rowCount());
        const int room = max_rows_ - start_row;
        if (cells.size() > room) {
            dropped = static_cast<int>(cells.size()) - std::max(0, room);
            cells.resize(std::max(0, room));
        }
        const int need = start_row + static_cast<int>(cells.size()) - model_->rowCount();
        if (need > 0)
            model_->insert_blank(model_->rowCount(), need);
        model_->write_block(start_row, cells);
    }
    last_note_ = dropped > 0 ? tr("only the first %1 rows were kept").arg(max_rows_) : QString();
    update_status();
    return static_cast<int>(cells.size());
}

int DataGridEditor::paste_text(const QString& text, bool replace) {
    const QVector<QStringList> cells = parse_delimited(text);
    if (cells.isEmpty())
        return 0;
    const QModelIndex cur = view_->currentIndex();
    return apply_cells(cells, replace, cur.isValid() ? cur.row() : model_->rowCount());
}

void DataGridEditor::paste_from_clipboard(bool replace) {
    const QString text = QApplication::clipboard()->text();
    if (text.trimmed().isEmpty()) {
        last_note_ = tr("the clipboard has no text - copy the cells in your spreadsheet first");
        update_status();
        return;
    }
    const int n = paste_text(text, replace);
    if (n == 0) {
        last_note_ = tr("nothing usable on the clipboard");
        update_status();
    }
}

void DataGridEditor::import_csv() {
    if (importing_)
        return;
    const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(this, tr("Import rows"), start,
                                                      tr("Delimited text (*.csv *.tsv *.txt);;All files (*)"));
    if (path.isEmpty())
        return;
    importing_ = true;
    import_btn_->setEnabled(false);
    import_btn_->setText(tr("READING..."));
    auto* watcher = new QFutureWatcher<DgeImportResult>(this);
    const QString file_name = QFileInfo(path).fileName();
    connect(watcher, &QFutureWatcher<DgeImportResult>::finished, this, [this, watcher, file_name]() {
        const DgeImportResult r = watcher->result();
        watcher->deleteLater();
        on_import_parsed(r.cells, r.error, file_name);
    });
    watcher->setFuture(QtConcurrent::run([path]() { return dge_read_delimited_file(path); }));
}

void DataGridEditor::on_import_parsed(const QVector<QStringList>& cells, const QString& error,
                                      const QString& file_name) {
    importing_ = false;
    import_btn_->setEnabled(true);
    import_btn_->setText(tr("IMPORT CSV"));
    if (!error.isEmpty()) {
        last_note_ = error;
        update_status();
        return;
    }
    const int n = apply_cells(cells, /*replace=*/true, 0);
    if (last_note_.isEmpty())
        last_note_ = (n == 1 ? tr("1 row imported from %1") : tr("%1 rows imported from %2").arg(n)).arg(file_name);
    update_status();
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

bool DataGridEditor::eventFilter(QObject* watched, QEvent* event) {
    if (watched != view_)
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::ShortcutOverride) {
        // Keys the grid handles itself win over window / form shortcuts (Enter = "run" on the host).
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->matches(QKeySequence::Paste) || ke->matches(QKeySequence::Copy) || ke->key() == Qt::Key_Return ||
            ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace ||
            ke->key() == Qt::Key_Insert) {
            event->accept();
            return true;
        }
        return false;
    }
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    // (While a cell is being edited the keys go to its editor, not to the view.)
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->matches(QKeySequence::Paste)) {
        paste_from_clipboard(/*replace=*/false);
        return true;
    }
    if (ke->matches(QKeySequence::Copy)) {
        copy_selection();
        return true;
    }
    if (ke->key() == Qt::Key_Insert) {
        add_row();
        return true;
    }
    if ((ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) && (ke->modifiers() & Qt::ControlModifier)) {
        remove_selected_rows();
        return true;
    }
    if (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace) {
        clear_selected_cells();
        return true;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        const QModelIndex cur = view_->currentIndex();
        if (!cur.isValid()) {
            add_row();
            return true;
        }
        if (cur.row() == model_->rowCount() - 1) {
            model_->insert_blank(model_->rowCount(), 1);
        }
        const QModelIndex next = model_->index(cur.row() + 1, cur.column());
        view_->setCurrentIndex(next);
        view_->scrollTo(next);
        return true;
    }
    return false;
}

} // namespace fincept::ui
