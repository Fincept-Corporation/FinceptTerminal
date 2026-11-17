// UiItemViews.cpp — tables, lists and trees, read through their models (never
// their item widgets): visible columns, row counts, one-pass row visits, row
// text and identity, cell controls, and ui_read's table pages (text or json).

#include "mcp/ResultStore.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/core/UiErrors.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QTableView>
#include <QTreeView>

#include <algorithm>
#include <cmath>

namespace fincept::mcp::core::ui {

namespace ui_views_detail {

/// What a control with no text of its own is called.
QString control_name(const QWidget* w) {
    if (!w->accessibleName().trimmed().isEmpty())
        return w->accessibleName().trimmed();
    if (!w->toolTip().trimmed().isEmpty())
        return common::clip(plain_text(w->toolTip()), 60);
    return humanize_object_name(w->objectName());
}

} // namespace ui_views_detail

QString cell_widget_text(QWidget* cell, bool with_refs) {
    if (!cell || is_secret(cell) || is_hidden_from_agents(cell))
        return {};
    QList<QWidget*> all{cell};
    all += cell->findChildren<QWidget*>();
    QStringList parts;
    for (QWidget* c : all) {
        // Visible as part of the cell — a cell scrolled out of view is hidden
        // itself, so its own visibility says nothing.
        if (c != cell && !c->isVisibleTo(cell))
            continue;
        if (is_secret(c))
            continue;
        const bool control = qobject_cast<QAbstractButton*>(c) || qobject_cast<QComboBox*>(c) ||
                             qobject_cast<QAbstractSpinBox*>(c) ||
                             (qobject_cast<QLineEdit*>(c) && !qobject_cast<QComboBox*>(c->parentWidget()) &&
                              !qobject_cast<QAbstractSpinBox*>(c->parentWidget()));
        if (!control && !qobject_cast<QLabel*>(c))
            continue;
        QString t = widget_text(c).simplified();
        if (t.isEmpty() && control)
            t = ui_views_detail::control_name(c);
        if (t.isEmpty())
            continue;
        if (control && with_refs) {
            const auto node = describe_widget(c, Detail::Brief);
            parts << QStringLiteral("[%1 %2 %3]")
                         .arg(Refs::instance().ref_for(c, RefKind::Element),
                              node ? node->role : QStringLiteral("control"), common::quoted(common::clip(t, 30)));
        } else {
            parts << common::clip(t, 40);
        }
        if (parts.size() >= 4)
            break;
    }
    return parts.join(QStringLiteral(" · "));
}

// ═════════════════════════════════════════════════════════════════════════════
// Item views
// ═════════════════════════════════════════════════════════════════════════════

QList<int> visible_columns(QAbstractItemView* view) {
    QList<int> cols;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return cols;
    if (auto* lv = qobject_cast<QListView*>(view)) {
        cols << lv->modelColumn();
        return cols;
    }
    const int n = model->columnCount(view->rootIndex());
    auto* tv = qobject_cast<QTableView*>(view);
    auto* tr = qobject_cast<QTreeView*>(view);
    for (int c = 0; c < n; ++c) {
        if (tv && tv->isColumnHidden(c))
            continue;
        if (tr && tr->isColumnHidden(c))
            continue;
        cols << c;
    }
    return cols;
}

QStringList column_headers(QAbstractItemView* view, const QList<int>& cols) {
    QStringList out;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return out;
    for (int c : cols) {
        QString h = model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().simplified();
        if (h.isEmpty())
            h = cols.size() == 1 ? QString() : QStringLiteral("col%1").arg(c);
        out << h;
    }
    return out;
}

int view_row_count(QAbstractItemView* view, bool* capped, int tree_cap) {
    if (capped)
        *capped = false;
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        int n = 0;
        for (QModelIndex idx = model->index(0, 0, tree->rootIndex()); idx.isValid(); idx = tree->indexBelow(idx)) {
            if (++n >= tree_cap) {
                if (capped)
                    *capped = true;
                break;
            }
        }
        return n;
    }
    return model->rowCount(view->rootIndex()) - view_hidden_rows(view);
}

int view_hidden_rows(QAbstractItemView* view) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    // A table keeps its hidden rows as hidden header sections: O(1), not a
    // walk over every row of a 100k-row model.
    if (auto* tv = qobject_cast<QTableView*>(view))
        return tv->verticalHeader() ? tv->verticalHeader()->hiddenSectionCount() : 0;
    auto* lv = qobject_cast<QListView*>(view);
    if (!lv)
        return 0;
    const int n = model->rowCount(view->rootIndex());
    int hidden = 0;
    for (int r = 0; r < n; ++r) {
        if (lv->isRowHidden(r))
            ++hidden;
    }
    return hidden;
}

int for_each_view_row(QAbstractItemView* view, int from,
                      const std::function<bool(int row, const QModelIndex& first)>& fn) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model)
        return 0;
    const int col = visible_columns(view).value(0, 0);
    int visited = 0;
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        int pos = 0;
        for (QModelIndex idx = model->index(0, 0, tree->rootIndex()); idx.isValid();
             idx = tree->indexBelow(idx), ++pos) {
            if (pos < from)
                continue;
            ++visited;
            if (!fn(pos, col == 0 ? idx : idx.siblingAtColumn(col)))
                break;
        }
        return visited;
    }
    auto* tv = qobject_cast<QTableView*>(view);
    auto* lv = qobject_cast<QListView*>(view);
    if (tv && tv->verticalHeader() && tv->verticalHeader()->hiddenSectionCount() == 0)
        tv = nullptr; // nothing hidden: no per-row check
    const QModelIndex root = view->rootIndex();
    const int n = model->rowCount(root);
    for (int r = std::max(0, from); r < n; ++r) {
        if ((tv && tv->isRowHidden(r)) || (lv && lv->isRowHidden(r)))
            continue;
        ++visited;
        if (!fn(r, model->index(r, col, root)))
            break;
    }
    return visited;
}

int row_of_index(QAbstractItemView* view, const QModelIndex& idx) {
    if (!view || !idx.isValid() || idx.model() != view->model())
        return -1;
    if (qobject_cast<QTreeView*>(view)) {
        const QModelIndex target = idx.siblingAtColumn(0);
        int found = -1;
        for_each_view_row(view, 0, [&](int row, const QModelIndex& first) {
            if (first.siblingAtColumn(0) == target) {
                found = row;
                return false;
            }
            return true;
        });
        return found;
    }
    if (idx.parent() != view->rootIndex())
        return -1;
    if (auto* tv = qobject_cast<QTableView*>(view); tv && tv->isRowHidden(idx.row()))
        return -1;
    if (auto* lv = qobject_cast<QListView*>(view); lv && lv->isRowHidden(idx.row()))
        return -1;
    return idx.row();
}

QString row_key(QAbstractItemView* view, const QModelIndex& first) {
    if (!view || !first.isValid())
        return {};
    // Cells a person names the row by: mostly letters (a symbol, a name, a side),
    // not prices or times, which tick. At most two; else the first cell.
    QStringList parts;
    QString fallback;
    int seen = 0;
    for (int c : visible_columns(view)) {
        if (++seen > 8 || parts.size() >= 2)
            break;
        const QString t = first.siblingAtColumn(c).data(Qt::DisplayRole).toString().simplified();
        if (fallback.isEmpty())
            fallback = t;
        int letters = 0;
        int digits = 0;
        for (const QChar ch : t) {
            if (ch.isLetter())
                ++letters;
            else if (ch.isDigit())
                ++digits;
        }
        if (letters >= 2 && letters >= digits)
            parts << t;
    }
    return parts.isEmpty() ? fallback : parts.join(QStringLiteral(" | "));
}

QString view_sort_state(QAbstractItemView* view) {
    QHeaderView* header = nullptr;
    if (auto* tv = qobject_cast<QTableView*>(view); tv && tv->isSortingEnabled())
        header = tv->horizontalHeader();
    else if (auto* tr = qobject_cast<QTreeView*>(view); tr && tr->isSortingEnabled())
        header = tr->header();
    if (!header || !header->isSortIndicatorShown() || !view->model())
        return {};
    const int c = header->sortIndicatorSection();
    if (c < 0)
        return {};
    const QString name = view->model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString().simplified();
    return QStringLiteral("sorted by %1 %2")
        .arg(name.isEmpty() ? QStringLiteral("col%1").arg(c) : name,
             header->sortIndicatorOrder() == Qt::AscendingOrder ? QStringLiteral("↑") : QStringLiteral("↓"));
}

QModelIndex view_index(QAbstractItemView* view, int row, int col) {
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model || row < 0)
        return {};
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        QModelIndex idx = model->index(0, 0, tree->rootIndex());
        for (int i = 0; i < row && idx.isValid(); ++i)
            idx = tree->indexBelow(idx);
        if (!idx.isValid())
            return {};
        return col == 0 ? idx : idx.siblingAtColumn(col);
    }
    if (row >= model->rowCount(view->rootIndex()))
        return {};
    return model->index(row, col, view->rootIndex());
}

QString view_row_text(QAbstractItemView* view, int row, const QList<int>& cols, int cell_chars) {
    return index_row_text(view, view_index(view, row, cols.value(0, 0)), cols, cell_chars, false);
}

QString index_row_text(QAbstractItemView* view, const QModelIndex& first, const QList<int>& cols, int cell_chars,
                       bool cell_refs) {
    QStringList cells;
    if (!first.isValid())
        return {};
    int depth = 0;
    if (qobject_cast<QTreeView*>(view)) {
        for (QModelIndex p = first.parent(); p.isValid(); p = p.parent())
            ++depth;
    }
    for (int c : cols) {
        const QModelIndex idx = first.siblingAtColumn(c);
        QString t = idx.data(Qt::DisplayRole).toString();
        if (t.isEmpty()) {
            const QVariant check = idx.data(Qt::CheckStateRole);
            if (check.isValid())
                t = check.toInt() == Qt::Checked ? QStringLiteral("[x]") : QStringLiteral("[ ]");
            else if (QWidget* cell = view->indexWidget(idx))
                t = cell_widget_text(cell, cell_refs); // buttons placed in the row: "Modify · Cancel"
            else if (const QString acc = idx.data(Qt::AccessibleTextRole).toString(); !acc.isEmpty())
                t = acc; // an icon-only cell that says what it shows
            else if (const QString tip = idx.data(Qt::ToolTipRole).toString(); !tip.isEmpty())
                t = common::clip(plain_text(tip), 30);
        }
        // Control refs must survive the clip: they are what the agent acts on.
        cells << (cell_refs && t.contains(QLatin1Char('[')) ? t : common::clip(t, cell_chars));
    }
    QString line = cells.join(QStringLiteral(" | "));
    if (depth > 0)
        line = QString(qsizetype{depth} * 2, QLatin1Char(' ')) + line;
    return line;
}

int view_find_row(QAbstractItemView* view, const QString& text, int from, bool* scan_capped) {
    if (scan_capped)
        *scan_capped = false;
    const QStringList words = query_words(text);
    if (words.isEmpty() || !view || !view->model())
        return -1;
    const QList<int> cols = visible_columns(view);
    // One pass: an exact cell match wins outright; otherwise the first row
    // holding every word.
    const QString exact = text.trimmed().toLower();
    int exact_row = -1;
    int words_row = -1;
    int scanned = 0;
    for_each_view_row(view, from, [&](int r, const QModelIndex& first) {
        if (++scanned > kMaxRowScan) {
            if (scan_capped)
                *scan_capped = true;
            return false;
        }
        for (int c : cols) {
            if (first.siblingAtColumn(c).data(Qt::DisplayRole).toString().trimmed().toLower() == exact) {
                exact_row = r;
                return false;
            }
        }
        if (words_row < 0 && words_match(index_row_text(view, first, cols, 200).toLower(), words))
            words_row = r;
        return true;
    });
    return exact_row >= 0 ? exact_row : words_row;
}

QAbstractItemView* cell_widget_owner(QWidget* w, QModelIndex* idx) {
    for (QWidget* child = w; child; child = child->parentWidget()) {
        QWidget* parent = child->parentWidget();
        auto* view = parent ? qobject_cast<QAbstractItemView*>(parent->parentWidget()) : nullptr;
        if (!view || view->viewport() != parent || !view->model())
            continue;
        // `child` sits on the viewport: find the cell it is the index widget of.
        const QList<int> cols = visible_columns(view);
        QModelIndex found;
        int cells = 0;
        for_each_view_row(view, 0, [&](int, const QModelIndex& first) {
            for (int c : cols) {
                const QModelIndex i = first.siblingAtColumn(c);
                if (view->indexWidget(i) == child) {
                    found = i;
                    return false;
                }
            }
            return ++cells < 20000;
        });
        if (!found.isValid())
            return nullptr;
        if (idx)
            *idx = found;
        return view;
    }
    return nullptr;
}

int resolve_column(QAbstractItemView* view, const QString& wanted) {
    if (!view || wanted.isEmpty())
        return -1;
    const QList<int> cols = visible_columns(view);
    bool is_num = false;
    const int n = wanted.toInt(&is_num);
    if (is_num)
        return n >= 0 && n < cols.size() ? cols[n] : -1;
    const QStringList headers = column_headers(view, cols);
    const QString w = wanted.toLower();
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].toLower() == w)
            return cols[i];
    }
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].toLower().contains(w))
            return cols[i];
    }
    return -1;
}



// ═════════════════════════════════════════════════════════════════════════════
// ui_read of a table / list / tree
// ═════════════════════════════════════════════════════════════════════════════

namespace ui_views_detail {

constexpr int kDefaultRows = 20; // rows per page (limit overrides, ≤ 500)
constexpr int kMaxRows = 500;
constexpr int kReadCols = 10;              // columns per text page (col_offset / columns for others)
constexpr int kScanBudgetMs = 250;         // a filtered scan stops after this long on the GUI thread
constexpr int kJsonInlineBytes = 48 * 1024; // a larger json page is parked in the ResultStore

/// "showing rows 0–19 of 250 · next: ui_read(ref="e12", offset=20)".
QString page_footer(const QString& what, int first, int count, int total, bool total_is_floor,
                    const QString& next_call) {
    if (count <= 0)
        return {};
    QString line = QStringLiteral("showing %1 %2–%3 of %4%5")
                       .arg(what)
                       .arg(first)
                       .arg(first + count - 1)
                       .arg(total)
                       .arg(total_is_floor ? QStringLiteral("+") : QString());
    if (!next_call.isEmpty())
        line += QStringLiteral(" · next: ") + next_call;
    return line;
}

struct ColumnFilter {
    int col = -1;
    QString words; // lower case
};

/// `query` words; a word "Header:text" keeps rows whose Header cell holds text.
void parse_row_query(QAbstractItemView* view, const QString& query, QList<ColumnFilter>* filters, QStringList* words) {
    for (const QString& w : query_words(query)) {
        const qsizetype colon = w.indexOf(QLatin1Char(':'));
        if (colon > 0 && colon + 1 < w.size()) {
            const int col = resolve_column(view, w.left(colon));
            if (col >= 0) {
                filters->append({col, w.mid(colon + 1)});
                continue;
            }
        }
        words->append(w);
    }
}

/// A displayed number as a raw value and its unit: "$1,245.10" → 1245.1 "$",
/// "+1.2%" → 1.2 "%", "245.1B" → 2.451e11, "(3.5)" → -3.5. Not a number → !ok.
struct ParsedNumber {
    bool ok = false;
    double value = 0.0;
    QString unit;
};

ParsedNumber parse_number(const QString& display) {
    ParsedNumber out;
    QString s = display.trimmed();
    if (s.isEmpty() || s.size() > 40)
        return out;
    bool negative = false;
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        negative = true;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    if (s.startsWith(QLatin1Char('+'))) {
        s.remove(0, 1);
    } else if (s.startsWith(QLatin1Char('-')) || s.startsWith(QChar(0x2212))) {
        negative = !negative;
        s.remove(0, 1);
    }
    static const QString kCurrency = QStringLiteral("$€£¥₹₩₽₺₦฿₫₪");
    if (!s.isEmpty() && kCurrency.contains(s.at(0))) {
        out.unit = s.left(1);
        s.remove(0, 1);
    }
    if (s.endsWith(QLatin1Char('%'))) {
        out.unit = QStringLiteral("%");
        s.chop(1);
    }
    s = s.trimmed();
    double scale = 1.0;
    if (!s.isEmpty() && !out.unit.contains(QLatin1Char('%'))) {
        const QChar last = s.at(s.size() - 1).toUpper();
        const double m = last == QLatin1Char('K')   ? 1e3
                         : last == QLatin1Char('M') ? 1e6
                         : last == QLatin1Char('B') ? 1e9
                         : last == QLatin1Char('T') ? 1e12
                                                    : 1.0;
        if (m > 1.0 && s.size() > 1 && s.at(s.size() - 2).isDigit()) {
            scale = m;
            s.chop(1);
        }
    }
    // Thousands separators and thin spaces between digits.
    s.remove(QLatin1Char(',')).remove(QChar(0x2009)).remove(QChar(0x202F)).remove(QLatin1Char(' '));
    if (s.isEmpty() || !(s.at(0).isDigit() || s.at(0) == QLatin1Char('.')))
        return out;
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || !std::isfinite(v))
        return out;
    out.ok = true;
    out.value = (negative ? -v : v) * scale;
    return out;
}

bool is_blank_cell(const QString& t) {
    static const QStringList blanks = {QString(), QStringLiteral("—"), QStringLiteral("–"), QStringLiteral("-"),
                                       QStringLiteral("--"), QStringLiteral("N/A"), QStringLiteral("n/a"),
                                       QStringLiteral("NA")};
    return blanks.contains(t.trimmed());
}

/// The text a cell shows (display text, else its check state, controls, tooltip).
QString cell_text(QAbstractItemView* view, const QModelIndex& idx) {
    QString t = idx.data(Qt::DisplayRole).toString();
    if (!t.isEmpty())
        return t;
    if (const QVariant check = idx.data(Qt::CheckStateRole); check.isValid())
        return check.toInt() == Qt::Checked ? QStringLiteral("[x]") : QStringLiteral("[ ]");
    if (QWidget* cell = view->indexWidget(idx))
        return cell_widget_text(cell, true);
    if (const QString acc = idx.data(Qt::AccessibleTextRole).toString(); !acc.isEmpty())
        return acc;
    return common::clip(plain_text(idx.data(Qt::ToolTipRole).toString()), 30);
}

/// One page of a view as typed JSON: a header row, raw numbers in numeric
/// columns (their unit stated once), strings elsewhere, null for blanks.
QString page_json(QAbstractItemView* view, const QString& base_ref, const QList<QPair<int, QModelIndex>>& rows,
                  const QList<int>& cols, const QStringList& headers, const QJsonObject& head) {
    const int ncols = static_cast<int>(cols.size());
    // Pass 1: decide per column — numeric when every non-blank cell parses with one unit.
    QList<QString> unit(ncols);
    QList<bool> numeric(ncols, true);
    QList<bool> seen(ncols, false);
    QList<QStringList> texts;
    texts.reserve(rows.size());
    for (const auto& r : rows) {
        QStringList line;
        line.reserve(ncols);
        for (int i = 0; i < ncols; ++i) {
            const QModelIndex idx = r.second.siblingAtColumn(cols[i]);
            const QString t = cell_text(view, idx);
            line << t;
            if (!numeric[i] || is_blank_cell(t))
                continue;
            const QVariant edit = idx.data(Qt::EditRole);
            const bool typed_number =
                edit.typeId() == QMetaType::Double || edit.typeId() == QMetaType::Int ||
                edit.typeId() == QMetaType::LongLong || edit.typeId() == QMetaType::Float ||
                edit.typeId() == QMetaType::UInt || edit.typeId() == QMetaType::ULongLong;
            const ParsedNumber p = parse_number(t);
            if (!p.ok && !typed_number) {
                numeric[i] = false;
                continue;
            }
            if (!seen[i]) {
                unit[i] = p.unit;
                seen[i] = true;
            } else if (unit[i] != p.unit) {
                numeric[i] = false;
            }
        }
        texts << line;
    }
    QJsonArray header{QStringLiteral("#")};
    for (const QString& h : headers)
        header.append(h);
    QJsonObject units;
    for (int i = 0; i < ncols; ++i) {
        if (numeric[i] && seen[i] && !unit[i].isEmpty())
            units.insert(headers.value(i), unit[i]);
    }
    QJsonArray data;
    for (int r = 0; r < rows.size(); ++r) {
        QJsonArray row{rows[r].first};
        for (int i = 0; i < ncols; ++i) {
            const QString& t = texts[r][i];
            if (numeric[i] && seen[i]) {
                if (is_blank_cell(t)) {
                    row.append(QJsonValue::Null);
                    continue;
                }
                const QModelIndex idx = rows[r].second.siblingAtColumn(cols[i]);
                const QVariant edit = idx.data(Qt::EditRole);
                const ParsedNumber p = parse_number(t);
                // The model's own number when it has one ("189.3" behind "$189.30"),
                // except for percentages, whose model value may be a fraction.
                const bool typed = edit.typeId() == QMetaType::Double || edit.typeId() == QMetaType::Int ||
                                   edit.typeId() == QMetaType::LongLong || edit.typeId() == QMetaType::Float ||
                                   edit.typeId() == QMetaType::UInt || edit.typeId() == QMetaType::ULongLong;
                if (typed && (unit[i] != QLatin1String("%") || !p.ok))
                    row.append(edit.toDouble());
                else
                    row.append(p.value);
            } else {
                row.append(is_blank_cell(t) ? QJsonValue(QJsonValue::Null) : QJsonValue(common::clip(t, 200)));
            }
        }
        data.append(row);
    }
    QJsonObject out = head;
    out.insert(QStringLiteral("ref"), base_ref);
    out.insert(QStringLiteral("cols"), header);
    if (!units.isEmpty())
        out.insert(QStringLiteral("units"), units);
    out.insert(QStringLiteral("data"), data);
    QByteArray json = QJsonDocument(out).toJson(QJsonDocument::Compact);
    if (json.size() <= kJsonInlineBytes)
        return QString::fromUtf8(json);
    // Too big to inline: park the page, answer with its head and the first rows.
    const QString id = ResultStore::instance().put(QStringLiteral("ui_read"), out);
    QJsonArray first;
    for (int i = 0; i < data.size() && i < 10; ++i)
        first.append(data.at(i));
    out.insert(QStringLiteral("data"), first);
    out.insert(QStringLiteral("stored"), id);
    out.insert(QStringLiteral("note"),
               QStringLiteral("first 10 of %1 rows shown; all of them: ui_read(ref=\"%2\", path=\"data\")")
                   .arg(data.size())
                   .arg(id));
    return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
}

} // namespace ui_views_detail

QString read_view(QAbstractItemView* view, const QString& ref, const QJsonObject& args) {
    using namespace ui_views_detail;
    const int offset = clamp_int(args.value(QStringLiteral("offset")), 0, 0, 100000000);
    const int limit = clamp_int(args.value(QStringLiteral("limit")), kDefaultRows, 1, kMaxRows);
    const bool json = args.value(QStringLiteral("format")).toString().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0;
    QList<ColumnFilter> filters;
    QStringList words;
    parse_row_query(view, args.value(QStringLiteral("query")).toString(), &filters, &words);
    const bool filtered = !filters.isEmpty() || !words.isEmpty();
    const QList<int> all_cols = visible_columns(view);
    const QStringList all_headers = column_headers(view, all_cols);
    const QString base_ref = ref.section(QLatin1Char(':'), 0, 0);

    // Columns: by header (exact, then contained) or by number; else a window
    // (text) or all of them (json).
    QList<int> cols;
    QStringList headers;
    QStringList unknown;
    const QJsonArray wanted = args.value(QStringLiteral("columns")).toArray();
    for (const QJsonValue& w : wanted) {
        int at = -1;
        if (w.isDouble()) {
            at = w.toInt(-1);
        } else {
            const QString want = w.toString().trimmed().toLower();
            for (int i = 0; i < all_headers.size() && at < 0; ++i) {
                if (all_headers[i].toLower() == want)
                    at = i;
            }
            for (int i = 0; i < all_headers.size() && at < 0 && !want.isEmpty(); ++i) {
                if (all_headers[i].toLower().contains(want))
                    at = i;
            }
        }
        if (at >= 0 && at < all_cols.size()) {
            if (!cols.contains(all_cols[at])) {
                cols << all_cols[at];
                headers << all_headers[at];
            }
        } else {
            unknown << (w.isDouble() ? QString::number(w.toInt()) : w.toString());
        }
    }
    int col_offset = 0;
    if (cols.isEmpty()) {
        col_offset = clamp_int(args.value(QStringLiteral("col_offset")), 0, 0,
                               std::max(0, static_cast<int>(all_cols.size()) - 1));
        const int width = json ? 60 : kReadCols;
        cols = all_cols.mid(col_offset, width);
        headers = all_headers.mid(col_offset, width);
    }

    bool capped = false;
    const int total = view_row_count(view, &capped);
    QString name = view->accessibleName().trimmed();
    if (auto node = describe_widget(view, Detail::Brief); node && !node->name.isEmpty())
        name = node->name;
    const QString kind = view->inherits("QTreeView")    ? QStringLiteral("tree")
                         : view->inherits("QTableView") ? QStringLiteral("table")
                                                        : QStringLiteral("list");
    const int hidden = view_hidden_rows(view);
    const QString sort = view_sort_state(view);
    int selected = -1;
    if (QItemSelectionModel* sel = view->selectionModel()) {
        const QModelIndex cur = sel->currentIndex();
        if (cur.isValid() && sel->isSelected(cur))
            selected = row_of_index(view, cur);
    }

    // The rows: one pass, from the first one needed. Unfiltered tables and lists
    // with nothing hidden start AT the offset (row number = position).
    QList<QPair<int, QModelIndex>> page;
    int matched = 0;
    int scanned = 0;
    bool scan_capped = false;
    QElapsedTimer clock;
    clock.start();
    const bool direct = !filtered && hidden == 0 && !qobject_cast<QTreeView*>(view);
    const int from = direct ? offset : 0;
    if (direct)
        matched = offset;
    for_each_view_row(view, from, [&](int r, const QModelIndex& first) {
        if (filtered) {
            if (++scanned > kMaxRowScan || (scanned % 256 == 0 && clock.elapsed() > kScanBudgetMs)) {
                scan_capped = true;
                return false;
            }
            for (const ColumnFilter& f : filters) {
                if (!cell_text(view, first.siblingAtColumn(f.col)).toLower().contains(f.words))
                    return true;
            }
            // Free words match against every column, not only the ones shown.
            if (!words.isEmpty() && !words_match(index_row_text(view, first, all_cols, 80).toLower(), words))
                return true;
        }
        const int ordinal = matched++;
        if (ordinal < offset)
            return true;
        if (static_cast<int>(page.size()) >= limit)
            return filtered; // keep counting matches; plain rows: the total is known
        page.append({r, first});
        Refs::instance().anchor_row(view, base_ref, r, first);
        return true;
    });
    const int found = filtered ? matched : total;
    const int emitted = static_cast<int>(page.size());
    QString next;
    if (emitted > 0 && offset + emitted < found) {
        QString more;
        if (!args.value(QStringLiteral("query")).toString().trimmed().isEmpty())
            more += QStringLiteral(", query=\"%1\"").arg(args.value(QStringLiteral("query")).toString().trimmed());
        if (col_offset > 0)
            more += QStringLiteral(", col_offset=%1").arg(col_offset);
        if (json)
            more += QStringLiteral(", format=\"json\"");
        next = QStringLiteral("ui_read(ref=\"%1\", offset=%2%3)").arg(base_ref).arg(offset + emitted).arg(more);
    }

    if (json) {
        QJsonObject head{{QStringLiteral("kind"), kind},
                         {QStringLiteral("rows"), found},
                         {QStringLiteral("offset"), offset}};
        if (!name.isEmpty())
            head.insert(QStringLiteral("name"), name);
        if (capped || scan_capped)
            head.insert(QStringLiteral("rows_at_least"), true);
        if (filtered)
            head.insert(QStringLiteral("of"), total);
        if (hidden > 0)
            head.insert(QStringLiteral("hidden"), hidden);
        if (!sort.isEmpty())
            head.insert(QStringLiteral("sort"), sort);
        if (selected >= 0)
            head.insert(QStringLiteral("selected"), selected);
        if (!next.isEmpty())
            head.insert(QStringLiteral("next"), offset + emitted);
        if (!unknown.isEmpty())
            head.insert(QStringLiteral("unknown_columns"), QJsonArray::fromStringList(unknown));
        if (all_cols.size() > cols.size() && wanted.isEmpty())
            head.insert(QStringLiteral("more_cols"), static_cast<int>(all_cols.size() - cols.size()));
        return page_json(view, base_ref, page, cols, headers, head);
    }

    QStringList lines;
    QString head = QStringLiteral("%1 %2%3 · %4%5 rows")
                       .arg(base_ref, kind, name.isEmpty() ? QString() : QLatin1Char(' ') + common::quoted(name))
                       .arg(total)
                       .arg(capped ? QStringLiteral("+") : QString());
    if (hidden > 0)
        head += QStringLiteral(" (+%1 hidden by a filter)").arg(hidden);
    if (!sort.isEmpty())
        head += QStringLiteral(" · ") + sort;
    if (selected >= 0)
        head += QStringLiteral(" · selected %1:r%2").arg(base_ref).arg(selected);
    lines << head;
    if (cols.size() > 1 || !headers.value(0).isEmpty()) {
        QString cl = QStringLiteral("row | ") + headers.join(QStringLiteral(" | "));
        if (wanted.isEmpty() && all_cols.size() > cols.size())
            cl += QStringLiteral("   (columns %1–%2 of %3; others: col_offset=%4, or columns=[\"<header>\", …])")
                      .arg(col_offset + 1)
                      .arg(col_offset + cols.size())
                      .arg(all_cols.size())
                      .arg(col_offset + cols.size() < all_cols.size() ? col_offset + cols.size() : 0);
        lines << cl;
    }
    if (!unknown.isEmpty())
        lines << QStringLiteral("(no column %1 — columns: %2)")
                     .arg(unknown.join(QStringLiteral(", ")),
                          common::clip(all_headers.join(QStringLiteral(" | ")), 600));
    for (const auto& r : page)
        lines << QStringLiteral("%1:r%2 | %3")
                     .arg(base_ref)
                     .arg(r.first)
                     .arg(common::clip(index_row_text(view, r.second, cols, 60, true), 700));
    if (emitted == 0) {
        lines << (filtered ? QStringLiteral("(no rows hold \"%1\"%2)")
                                 .arg(args.value(QStringLiteral("query")).toString().trimmed(),
                                      offset > 0 ? QStringLiteral(" past offset %1").arg(offset) : QString())
                           : QStringLiteral("(no rows at offset %1 — the list has %2)").arg(offset).arg(total));
    } else {
        lines << page_footer(filtered ? QStringLiteral("matching rows") : QStringLiteral("rows"), offset, emitted,
                             found, capped || scan_capped, next);
    }
    if (scan_capped)
        lines << QStringLiteral("(searched the first %1 rows only)").arg(scanned);
    return lines.join(QLatin1Char('\n'));
}

} // namespace fincept::mcp::core::ui
