// src/screens/excel/SpreadsheetWidget.cpp
#include "screens/excel/SpreadsheetWidget.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QHash>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QRegularExpression>
#include <QSet>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <numeric>

namespace fincept::screens {

using namespace fincept::ui;

// ═════════════════════════════════════════════════════════════════════════════
// SpreadsheetItem — formula-aware cell
// ═════════════════════════════════════════════════════════════════════════════

SpreadsheetItem::SpreadsheetItem() : QTableWidgetItem(QTableWidgetItem::UserType) {}

SpreadsheetItem::SpreadsheetItem(const QString& text) : QTableWidgetItem(QTableWidgetItem::UserType), raw_text_(text) {}

QTableWidgetItem* SpreadsheetItem::clone() const {
    return new SpreadsheetItem(raw_text_);
}

void SpreadsheetItem::setData(int role, const QVariant& value) {
    if (role == Qt::EditRole) {
        raw_text_ = value.toString();
    }
    QTableWidgetItem::setData(role, value);
}

QVariant SpreadsheetItem::data(int role) const {
    if (role == Qt::DisplayRole && raw_text_.startsWith('=')) {
        return evaluate_formula();
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return raw_text_;
    }

    // ── Presentation ─────────────────────────────────────────────────────
    // The grid rendered every cell identically: left-aligned, same weight. A
    // financial model laid out that way is unreadable — figures don't line up
    // on the decimal and section headings look like data. Both are derived
    // from the raw text, never from evaluating the formula, so painting stays
    // cheap.
    if (role == Qt::TextAlignmentRole) {
        bool numeric = false;
        raw_text_.toDouble(&numeric);
        const bool right = numeric || raw_text_.startsWith(QLatin1Char('='));
        return QVariant(static_cast<int>((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter));
    }

    if (role == Qt::FontRole) {
        // A label in column A with nothing beside it is a section heading
        // ("INPUTS & ASSUMPTIONS"), not a data row.
        if (column() == 0 && !raw_text_.trimmed().isEmpty()) {
            const auto* tbl = tableWidget();
            if (tbl) {
                bool row_is_bare = true;
                for (int c = 1; c < tbl->columnCount() && row_is_bare; ++c) {
                    const auto* other = tbl->item(row(), c);
                    if (other && !other->data(Qt::EditRole).toString().trimmed().isEmpty())
                        row_is_bare = false;
                }
                if (row_is_bare) {
                    QFont f = tbl->font();
                    f.setBold(true);
                    return f;
                }
            }
        }
    }

    return QTableWidgetItem::data(role);
}

// ── Formula evaluation ───────────────────────────────────────────────────────

int SpreadsheetItem::column_from_letters(const QString& letters) {
    int col = 0;
    for (const QChar& c : letters) {
        col = col * 26 + (c.toUpper().unicode() - 'A' + 1);
    }
    return col - 1; // 0-based
}

void SpreadsheetItem::parse_cell_ref(const QString& ref, int& row, int& col) {
    static QRegularExpression re("^\\$?([A-Za-z]+)\\$?(\\d+)$");
    auto m = re.match(ref.trimmed());
    if (m.hasMatch()) {
        col = column_from_letters(m.captured(1));
        row = m.captured(2).toInt() - 1; // 0-based
    } else {
        row = -1;
        col = -1;
    }
}

double SpreadsheetItem::resolve_cell_value(int row, int col) const {
    if (!tableWidget())
        return 0.0;
    auto* item = tableWidget()->item(row, col);
    if (!item)
        return 0.0;

    QVariant val = item->data(Qt::DisplayRole);
    bool ok = false;
    double dv = val.toDouble(&ok);
    return ok ? dv : 0.0;
}

QVector<double> SpreadsheetItem::resolve_range(const QString& from, const QString& to) const {
    int r1, c1, r2, c2;
    parse_cell_ref(from, r1, c1);
    parse_cell_ref(to, r2, c2);

    if (r1 < 0 || c1 < 0 || r2 < 0 || c2 < 0)
        return {};

    if (r1 > r2)
        std::swap(r1, r2);
    if (c1 > c2)
        std::swap(c1, c2);

    QVector<double> out;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            double v = resolve_cell_value(r, c);
            out.append(v);
        }
    }
    return out;
}

QVariant SpreadsheetItem::evaluate_formula() const {
    if (!tableWidget())
        return "#ERR";

    // Guard against self-referential / circular formulas ("=A1" in A1, or
    // A1→B1→A1). Evaluating a formula reads other cells' DisplayRole, which
    // re-enters evaluate_formula(); without this a cycle recurses until the
    // stack overflows and the whole terminal crashes on the next repaint. Track
    // the items currently on the evaluation stack (single UI thread) and return
    // #CYCLE on re-entry instead of recursing.
    static thread_local QSet<const SpreadsheetItem*> evaluating;
    if (evaluating.contains(this))
        return QStringLiteral("#CYCLE");

    // ── Memoise within one evaluation pass ────────────────────────────────
    // Formulas are evaluated lazily from data(DisplayRole), and each one reads
    // its dependencies' DisplayRole, which evaluates THEM from scratch. On a
    // chain — the shape of every financial model, Revenue → EBIT → NOPAT → FCF
    // → PV → EV → equity → per-share — that is O(2^depth) per cell, and the
    // whole grid repaints together. A DCF was enough to wedge the UI thread
    // long enough for set_excel_sheet_data to blow its timeout mid-write.
    //
    // The cache lives only for the outermost evaluation: entries are dropped
    // when the last frame unwinds, so an edit is still picked up immediately.
    // Invalidation is therefore not a concern — nothing can change while a
    // single synchronous pass is running.
    static thread_local QHash<const SpreadsheetItem*, QVariant> eval_cache;
    static thread_local int eval_depth = 0;
    const auto cached = eval_cache.constFind(this);
    if (cached != eval_cache.constEnd())
        return cached.value();

    evaluating.insert(this);
    ++eval_depth;
    struct EvalGuard {
        const SpreadsheetItem* item;
        ~EvalGuard() {
            evaluating.remove(item);
            if (--eval_depth == 0)
                eval_cache.clear();
        }
    } eval_guard{this};

    // Store into the cache on every return path.
    struct Memo {
        const SpreadsheetItem* item;
        QVariant operator()(QVariant v) const {
            eval_cache.insert(item, v);
            return v;
        }
    } memo{this};

    QString expr = raw_text_.mid(1).trimmed().toUpper();

    // ── Function calls: SUM, AVG/AVERAGE, MIN, MAX, COUNT ────────────────
    static QRegularExpression func_re("^(SUM|AVG|AVERAGE|MIN|MAX|COUNT)\\(([A-Z]+\\d+):([A-Z]+\\d+)\\)$");
    auto fm = func_re.match(expr);
    if (fm.hasMatch()) {
        QString func = fm.captured(1);
        auto vals = resolve_range(fm.captured(2), fm.captured(3));
        if (vals.isEmpty())
            return memo(0.0);

        if (func == "SUM")
            return memo(std::accumulate(vals.begin(), vals.end(), 0.0));
        if (func == "AVG" || func == "AVERAGE")
            return memo(std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size());
        if (func == "MIN")
            return memo(*std::min_element(vals.begin(), vals.end()));
        if (func == "MAX")
            return memo(*std::max_element(vals.begin(), vals.end()));
        if (func == "COUNT")
            return memo(static_cast<double>(vals.size()));
    }

    // ── Simple cell reference: =A1 ───────────────────────────────────────
    static QRegularExpression cell_re("^\\$?([A-Z]+)\\$?(\\d+)$");
    auto cm = cell_re.match(expr);
    if (cm.hasMatch()) {
        int r, c;
        parse_cell_ref(expr, r, c);
        if (r >= 0 && c >= 0)
            return memo(resolve_cell_value(r, c));
    }

    // ── Simple arithmetic with cell refs: =A1+B1, =A1*2, etc. ───────────
    // Replace cell references with their numeric values, then evaluate
    static QRegularExpression ref_re("\\$?([A-Z]+)\\$?(\\d+)");
    QString eval_expr = expr;
    QRegularExpressionMatchIterator it = ref_re.globalMatch(expr);

    // Collect all matches first (replace longest first to avoid overlap)
    QVector<QPair<QString, double>> replacements;
    while (it.hasNext()) {
        auto match = it.next();
        int r, c;
        parse_cell_ref(match.captured(0), r, c);
        double val = (r >= 0 && c >= 0) ? resolve_cell_value(r, c) : 0.0;
        replacements.append({match.captured(0), val});
    }
    // Sort by length descending to avoid partial replacements
    std::sort(replacements.begin(), replacements.end(),
              [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
    for (const auto& [ref, val] : replacements) {
        eval_expr.replace(ref, QString::number(val, 'g', 15));
    }

    // Simple arithmetic evaluator for +, -, *, /, parentheses
    // Remove spaces
    eval_expr.remove(' ');

    // Validate: only digits, operators, decimal points, parens
    static QRegularExpression valid_re("^[0-9+\\-*/().eE]+$");
    if (!valid_re.match(eval_expr).hasMatch()) {
        return memo("#ERR");
    }

    // Use a simple recursive descent parser
    // For safety, evaluate using basic parsing
    try {
        // Simple approach: iterate and compute
        // Stack-based evaluation for +, -, *, /
        QVector<double> nums;
        QVector<QChar> ops;
        QString num_buf;

        auto flush_num = [&]() {
            if (!num_buf.isEmpty()) {
                bool ok;
                double v = num_buf.toDouble(&ok);
                if (ok)
                    nums.append(v);
                else
                    return false;
                num_buf.clear();
            }
            return true;
        };

        // Returns false when the expression is malformed (an operator with no
        // operands, e.g. "=1++2" or "=3*-5"). The old version returned void and
        // silently did NOT pop the operator, so the `while (precedence(...) >=
        // ...) apply_op();` loops below spun forever and HUNG the whole UI
        // thread on any double-operator formula. Signalling failure lets the
        // caller bail out with #ERR instead.
        bool div_zero = false;
        auto apply_op = [&]() -> bool {
            if (ops.isEmpty() || nums.size() < 2)
                return false;
            double b = nums.takeLast();
            double a = nums.takeLast();
            QChar op = ops.takeLast();
            if (op == '+')
                nums.append(a + b);
            else if (op == '-')
                nums.append(a - b);
            else if (op == '*')
                nums.append(a * b);
            else if (op == '/') {
                if (b == 0.0) {
                    div_zero = true;
                    return false;
                }
                nums.append(a / b);
            } else {
                return false; // stray '(' — unbalanced parentheses
            }
            return true;
        };

        auto precedence = [](QChar op) -> int {
            if (op == '+' || op == '-')
                return 1;
            if (op == '*' || op == '/')
                return 2;
            return 0;
        };

        for (int i = 0; i < eval_expr.length(); ++i) {
            QChar ch = eval_expr[i];
            // A '-' is unary (part of the number) at the start of the expression,
            // right after '(', or right after another operator — "3*-5", "2^-1".
            const bool unary_minus =
                ch == '-' && (i == 0 || eval_expr[i - 1] == '(' || eval_expr[i - 1] == '+' ||
                              eval_expr[i - 1] == '-' || eval_expr[i - 1] == '*' || eval_expr[i - 1] == '/');
            if (ch.isDigit() || ch == '.' || ch == 'e' || ch == 'E' || unary_minus) {
                num_buf += ch;
            } else if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
                if (!flush_num())
                    return memo(QVariant("#ERR"));
                while (!ops.isEmpty() && ops.last() != '(' && precedence(ops.last()) >= precedence(ch)) {
                    if (!apply_op())
                        return memo(QVariant(div_zero ? "#DIV/0!" : "#ERR"));
                }
                ops.append(ch);
            } else if (ch == '(') {
                ops.append(ch);
            } else if (ch == ')') {
                if (!flush_num())
                    return memo(QVariant("#ERR"));
                while (!ops.isEmpty() && ops.last() != '(') {
                    if (!apply_op())
                        return memo(QVariant(div_zero ? "#DIV/0!" : "#ERR"));
                }
                if (ops.isEmpty())
                    return memo(QVariant("#ERR")); // ')' with no matching '('
                ops.removeLast();            // remove '('
            }
        }
        if (!flush_num())
            return memo(QVariant("#ERR"));
        while (!ops.isEmpty()) {
            if (!apply_op())
                return memo(QVariant(div_zero ? "#DIV/0!" : "#ERR"));
        }

        if (nums.size() == 1) {
            double result = nums.first();
            if (!std::isfinite(result))
                return memo(QVariant("#NUM!"));
            if (std::floor(result) == result && std::abs(result) < 1e15)
                return memo(static_cast<qlonglong>(result));
            // 'g' with 12 significant digits: 2.5 renders as "2.5", not
            // "2.500000", while still keeping precision on long decimals.
            return memo(QString::number(result, 'g', 12));
        }
    } catch (...) {
        // fall through
    }

    return memo("#ERR");
}

// ═════════════════════════════════════════════════════════════════════════════
// SpreadsheetWidget
// ═════════════════════════════════════════════════════════════════════════════

SpreadsheetWidget::SpreadsheetWidget(const QString& sheet_name, int rows, int cols, QWidget* parent)
    : QWidget(parent), sheet_name_(sheet_name) {
    build_ui(rows, cols);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void SpreadsheetWidget::build_ui(int rows, int cols) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Formula bar
    auto* formula_row = new QWidget(this);
    formula_row->setFixedHeight(28);
    formula_row->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_MED()));

    auto* fhl = new QHBoxLayout(formula_row);
    fhl->setContentsMargins(8, 0, 8, 0);
    fhl->setSpacing(6);

    cell_ref_label_ = new QLabel("A1", formula_row);
    cell_ref_label_->setFixedWidth(50);
    cell_ref_label_->setAlignment(Qt::AlignCenter);
    cell_ref_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:700;"
                                           " border-right:1px solid %3; padding-right:6px;")
                                       .arg(colors::AMBER(), fonts::DATA_FAMILY, colors::BORDER_MED()));
    fhl->addWidget(cell_ref_label_);

    auto* fx_label = new QLabel("fx", formula_row);
    fx_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:700;")
                                .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    fhl->addWidget(fx_label);

    formula_bar_ = new QLineEdit(formula_row);
    formula_bar_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:11px; padding:2px 4px; }"
                "QLineEdit:focus { background:%4; }")
            .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), fonts::DATA_FAMILY, colors::BG_HOVER()));
    connect(formula_bar_, &QLineEdit::returnPressed, this, &SpreadsheetWidget::on_formula_bar_return);
    fhl->addWidget(formula_bar_, 1);

    root->addWidget(formula_row);

    // Table
    table_ = new QTableWidget(rows, cols, this);
    // Cells are created lazily (see below); the prototype makes every implicitly
    // created cell a formula-aware SpreadsheetItem rather than a plain one.
    table_->setItemPrototype(new SpreadsheetItem());
    setup_headers(cols);

    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->horizontalHeader()->setDefaultSectionSize(80);
    table_->horizontalHeader()->setMinimumSectionSize(30);
    table_->verticalHeader()->setMinimumSectionSize(18);
    table_->setAlternatingRowColors(false);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);

    // Cells are deliberately NOT pre-populated. A 20 000 × 512 import window is
    // 10.2M cells; allocating a SpreadsheetItem for each here cost ~2 GB and
    // several seconds on the UI thread, and set_data() then threw the whole lot
    // away and allocated a second set. Every read path tolerates a null item
    // (cell_text/get_data return "", set_cell creates on demand), so an empty
    // grid costs nothing until something is actually written to it.

    connect(table_, &QTableWidget::cellChanged, this, &SpreadsheetWidget::on_cell_changed);
    connect(table_, &QTableWidget::currentCellChanged, this, &SpreadsheetWidget::on_current_cell_changed);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &SpreadsheetWidget::on_context_menu);

    // Dark theme styling
    table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                  "  font-family:%4; font-size:11px; border:none; }"
                                  "QTableWidget::item { padding:2px 4px; }"
                                  "QTableWidget::item:selected { background:%5; }"
                                  "QHeaderView::section { background:%6; color:%7; border:none;"
                                  "  border-right:1px solid %3; border-bottom:1px solid %3;"
                                  "  font-family:%4; font-size:10px; font-weight:600; padding:3px 4px; }"
                                  "QTableCornerButton::section { background:%6; border:none; }")
                              .arg(colors::BG_HOVER(),         // %1 cell bg
                                   colors::TEXT_PRIMARY(),     // %2 cell text
                                   colors::TEXT_DIM(),         // %3 gridlines
                                   fonts::DATA_FAMILY,         // %4
                                   colors::BORDER_BRIGHT(),    // %5 selection bg
                                   colors::BORDER_MED(),       // %6 header bg
                                   colors::TEXT_SECONDARY())); // %7 header text

    root->addWidget(table_, 1);

    table_->setAccessibleName(tr("Spreadsheet grid"));
    formula_bar_->setAccessibleName(tr("Formula bar"));
    cell_ref_label_->setAccessibleName(tr("Active cell reference"));
    setTabOrder(formula_bar_, table_);

    install_shortcuts();
}

// ── Keyboard shortcuts ───────────────────────────────────────────────────────
// QTableWidget ships with no clipboard or find handling, so a spreadsheet
// without these bindings feels broken: Ctrl+C did nothing and Delete did
// nothing. Scoped to this widget so they don't collide with other screens.

void SpreadsheetWidget::install_shortcuts() {
    // WidgetWithChildrenShortcut also covers the in-place cell editor, so every
    // grid-level binding must stand down while a cell is being edited —
    // otherwise Delete would blank the whole selection mid-keystroke and Ctrl+C
    // would copy cells instead of the selected characters.
    auto add = [this](QKeySequence::StandardKey key, std::function<void()> fn) {
        auto* sc = new QShortcut(QKeySequence(key), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, fn = std::move(fn)]() {
            // QAbstractItemView::state() is protected, so detect an open in-place
            // editor via focus instead: the delegate's editor is a child widget of
            // the table, and it (not the table) holds focus while editing.
            const QWidget* fw = QApplication::focusWidget();
            if (fw && fw != table_ && table_->isAncestorOf(fw))
                return;
            fn();
        });
    };
    add(QKeySequence::Copy, [this]() { copy_selection(false); });
    add(QKeySequence::Cut, [this]() { copy_selection(true); });
    add(QKeySequence::Paste, [this]() { paste_clipboard(); });
    add(QKeySequence::Delete, [this]() { clear_selection(); });
    add(QKeySequence::Find, [this]() { find_next(true); });
    add(QKeySequence::FindNext, [this]() { find_next(false); });
}

void SpreadsheetWidget::setup_headers(int cols) {
    QStringList col_headers;
    col_headers.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        col_headers << column_label(c);
    }
    table_->setHorizontalHeaderLabels(col_headers);
}

QString SpreadsheetWidget::column_label(int col) {
    QString label;
    while (col >= 0) {
        label.prepend(QChar('A' + (col % 26)));
        col = col / 26 - 1;
    }
    return label;
}

// ── Data access ──────────────────────────────────────────────────────────────

QVector<QVector<QString>> SpreadsheetWidget::get_data() const {
    int rows = table_->rowCount();
    int cols = table_->columnCount();
    QVector<QVector<QString>> cells(rows);
    for (int r = 0; r < rows; ++r) {
        cells[r].resize(cols);
        for (int c = 0; c < cols; ++c) {
            auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(r, c));
            cells[r][c] = item ? item->raw_text() : "";
        }
    }
    return cells;
}

void SpreadsheetWidget::set_data(const QVector<QVector<QString>>& cells) {
    table_->blockSignals(true);

    int rows = cells.size();
    // Take the WIDEST row, not row 0 — a ragged import (CSV with a short header
    // line) previously truncated every following row to the header's width.
    int cols = 0;
    for (const auto& row : cells)
        cols = std::max<int>(cols, row.size());

    // Ensure minimum size
    rows = std::max(rows, 100);
    cols = std::max(cols, 26);

    table_->setRowCount(rows);
    table_->setColumnCount(cols);
    setup_headers(cols);

    // Allocate ONLY the cells that carry a value. The old loop allocated
    // rows × cols items unconditionally — for a 20 000-row import that is
    // millions of heap objects for cells that are, and stay, empty. Anything
    // reading a missing cell already gets "" (cell_text / get_data), and the
    // item prototype materialises a real SpreadsheetItem the moment the user
    // types into one.
    table_->clearContents(); // drop any items from a previous load
    for (int r = 0; r < cells.size() && r < rows; ++r) {
        const auto& src_row = cells[r];
        for (int c = 0; c < src_row.size() && c < cols; ++c) {
            if (src_row[c].isEmpty())
                continue;
            table_->setItem(r, c, new SpreadsheetItem(src_row[c]));
        }
    }

    table_->blockSignals(false);
    autofit_columns(cells);
    recalculate();
}

// Size columns to their content.
//
// Every column sat at the fixed 80 px default, so a model written into the grid
// showed as "Terminal …" / "Effective…" — the labels that make it readable were
// exactly the ones clipped. Widths are measured from the source text with the
// table's own font rather than via resizeColumnsToContents(), which would walk
// all 2 600 cells and evaluate every formula just to measure it.
void SpreadsheetWidget::autofit_columns(const QVector<QVector<QString>>& cells) {
    constexpr int kMinWidth = 64;
    constexpr int kMaxWidth = 340; // past this, wrapping/scrolling beats widening
    constexpr int kPadding = 18;

    // Text shaping is the expensive part: one horizontalAdvance() per non-empty
    // cell. On a large import that is millions of shaping calls on the UI thread
    // for column widths that are already decided by the first screenful of rows.
    // Sample the head of the sheet instead — headers and the first data rows are
    // what actually set the width.
    constexpr int kMaxMeasuredRows = 200;
    const int measured_rows = std::min<int>(cells.size(), kMaxMeasuredRows);

    const QFontMetrics fm(table_->font());
    // A formula's text is far longer than the number it produces; measuring the
    // source would push its column to the cap. Budget for a typical result.
    const int formula_width = fm.horizontalAdvance(QStringLiteral("1,234,567.89"));

    for (int c = 0; c < table_->columnCount(); ++c) {
        int width = kMinWidth;
        for (int r = 0; r < measured_rows; ++r) {
            if (c >= cells[r].size())
                continue;
            const QString& text = cells[r][c];
            if (text.trimmed().isEmpty())
                continue;
            const int w = text.startsWith(QLatin1Char('=')) ? formula_width : fm.horizontalAdvance(text);
            width = std::max(width, w + kPadding);
        }
        table_->setColumnWidth(c, std::clamp(width, kMinWidth, kMaxWidth));
    }
}

QString SpreadsheetWidget::cell_text(int row, int col) const {
    auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(row, col));
    return item ? item->raw_text() : "";
}

void SpreadsheetWidget::set_cell(int row, int col, const QString& text) {
    auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(row, col));
    if (!item) {
        item = new SpreadsheetItem(text);
        table_->setItem(row, col, item);
    } else {
        item->setData(Qt::EditRole, text);
    }

    // Grow the column if this value doesn't fit. A model written cell-by-cell
    // (the set_excel_cell path) never goes through set_data, so without this it
    // keeps the 80 px default and clips every label.
    if (!text.startsWith(QLatin1Char('=')) && !text.trimmed().isEmpty() && col >= 0) {
        constexpr int kMaxWidth = 340;
        constexpr int kPadding = 18;
        const QFontMetrics fm(table_->font());
        const int needed = std::min(fm.horizontalAdvance(text) + kPadding, kMaxWidth);
        if (needed > table_->columnWidth(col))
            table_->setColumnWidth(col, needed);
    }
}

void SpreadsheetWidget::recalculate() {
    table_->viewport()->update();
}

// ── Slots ────────────────────────────────────────────────────────────────────

void SpreadsheetWidget::on_cell_changed(int row, int col) {
    if (updating_formula_bar_)
        return;

    auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(row, col));
    if (item) {
        // Sync raw text from edit
        QString current = item->QTableWidgetItem::data(Qt::EditRole).toString();
        if (current != item->raw_text()) {
            item->setData(Qt::EditRole, current);
        }
    }

    // Trigger recalculation of dependent cells
    recalculate();
    emit data_changed();
}

void SpreadsheetWidget::on_current_cell_changed(int row, int col, int, int) {
    if (row < 0 || col < 0)
        return;

    updating_formula_bar_ = true;
    cell_ref_label_->setText(column_label(col) + QString::number(row + 1));

    auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(row, col));
    formula_bar_->setText(item ? item->raw_text() : "");
    updating_formula_bar_ = false;
}

void SpreadsheetWidget::on_formula_bar_return() {
    int row = table_->currentRow();
    int col = table_->currentColumn();
    if (row < 0 || col < 0)
        return;

    updating_formula_bar_ = true;
    set_cell(row, col, formula_bar_->text());
    recalculate();
    updating_formula_bar_ = false;
    emit data_changed();

    // Move to next row
    if (row + 1 < table_->rowCount()) {
        table_->setCurrentCell(row + 1, col);
    }
}

// ── Context menu ─────────────────────────────────────────────────────────────

void SpreadsheetWidget::on_context_menu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(QString("QMenu { background:%1; color:%2; border:1px solid %3;"
                               " font-family:%4; font-size:11px; }"
                               "QMenu::item { padding:6px 20px; }"
                               "QMenu::item:selected { background:%5; }"
                               "QMenu::separator { height:1px; background:%3; margin:4px 8px; }")
                           .arg(colors::BORDER_MED(), colors::TEXT_PRIMARY(), colors::TEXT_DIM(), fonts::DATA_FAMILY,
                                colors::TEXT_DIM()));

    // Route the menu through the same range-aware clipboard the Ctrl+X/C/V
    // shortcuts use — the old handlers only ever touched the single active cell
    // and copied the *displayed* value, so copying a formula lost the formula.
    menu.addAction(tr("Cut"), this, [this]() { copy_selection(true); });
    menu.addAction(tr("Copy"), this, [this]() { copy_selection(false); });
    menu.addAction(tr("Paste"), this, [this]() { paste_clipboard(); });
    menu.addSeparator();
    menu.addAction(tr("Insert Row Above"), this, &SpreadsheetWidget::insert_row_above);
    menu.addAction(tr("Insert Row Below"), this, &SpreadsheetWidget::insert_row_below);
    menu.addAction(tr("Insert Column Left"), this, &SpreadsheetWidget::insert_col_left);
    menu.addAction(tr("Insert Column Right"), this, &SpreadsheetWidget::insert_col_right);
    menu.addSeparator();
    menu.addAction(tr("Delete Selected Rows"), this, &SpreadsheetWidget::delete_selected_rows);
    menu.addAction(tr("Delete Selected Columns"), this, &SpreadsheetWidget::delete_selected_cols);
    menu.addSeparator();
    menu.addAction(tr("Clear Cell"), this, [this]() { clear_selection(); });
    menu.addSeparator();
    menu.addAction(tr("Find…"), this, [this]() { find_next(true); });

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ── Insert / Delete ──────────────────────────────────────────────────────────

void SpreadsheetWidget::insert_row_above() {
    int row = table_->currentRow();
    if (row < 0)
        row = 0;
    table_->insertRow(row);
    for (int c = 0; c < table_->columnCount(); ++c) {
        table_->setItem(row, c, new SpreadsheetItem());
    }
    emit data_changed(); // structure edits are edits — mark the sheet dirty
}

void SpreadsheetWidget::insert_row_below() {
    int row = table_->currentRow() + 1;
    table_->insertRow(row);
    for (int c = 0; c < table_->columnCount(); ++c) {
        table_->setItem(row, c, new SpreadsheetItem());
    }
    emit data_changed();
}

void SpreadsheetWidget::insert_col_left() {
    int col = table_->currentColumn();
    if (col < 0)
        col = 0;
    table_->insertColumn(col);
    for (int r = 0; r < table_->rowCount(); ++r) {
        table_->setItem(r, col, new SpreadsheetItem());
    }
    setup_headers(table_->columnCount());
    emit data_changed();
}

void SpreadsheetWidget::insert_col_right() {
    int col = table_->currentColumn() + 1;
    table_->insertColumn(col);
    for (int r = 0; r < table_->rowCount(); ++r) {
        table_->setItem(r, col, new SpreadsheetItem());
    }
    setup_headers(table_->columnCount());
    emit data_changed();
}

void SpreadsheetWidget::delete_selected_rows() {
    auto ranges = table_->selectedRanges();
    QVector<int> rows;
    for (const auto& range : ranges) {
        for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
            if (!rows.contains(r))
                rows.append(r);
        }
    }
    if (rows.isEmpty())
        return;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        table_->removeRow(r);
    }
    emit data_changed();
}

void SpreadsheetWidget::delete_selected_cols() {
    auto ranges = table_->selectedRanges();
    QVector<int> cols;
    for (const auto& range : ranges) {
        for (int c = range.leftColumn(); c <= range.rightColumn(); ++c) {
            if (!cols.contains(c))
                cols.append(c);
        }
    }
    if (cols.isEmpty())
        return;
    std::sort(cols.begin(), cols.end(), std::greater<int>());
    for (int c : cols) {
        table_->removeColumn(c);
    }
    setup_headers(table_->columnCount());
    emit data_changed();
}

// ── Clipboard (TSV, Excel-compatible) ────────────────────────────────────────

void SpreadsheetWidget::copy_selection(bool cut) {
    const auto ranges = table_->selectedRanges();
    if (ranges.isEmpty()) {
        auto* item = table_->currentItem();
        if (!item)
            return;
        QApplication::clipboard()->setText(cell_text(table_->currentRow(), table_->currentColumn()));
        if (cut) {
            set_cell(table_->currentRow(), table_->currentColumn(), QString());
            recalculate();
            emit data_changed();
        }
        return;
    }

    // Use the bounding box of the selection so a rectangular copy round-trips
    // through Excel / Google Sheets unchanged.
    int top = INT_MAX, left = INT_MAX, bottom = -1, right = -1;
    for (const auto& r : ranges) {
        top = std::min(top, r.topRow());
        left = std::min(left, r.leftColumn());
        bottom = std::max(bottom, r.bottomRow());
        right = std::max(right, r.rightColumn());
    }

    QStringList lines;
    lines.reserve(bottom - top + 1);
    for (int r = top; r <= bottom; ++r) {
        QStringList row;
        row.reserve(right - left + 1);
        for (int c = left; c <= right; ++c)
            row << cell_text(r, c);
        lines << row.join('\t');
    }
    QApplication::clipboard()->setText(lines.join('\n'));

    if (cut) {
        for (int r = top; r <= bottom; ++r)
            for (int c = left; c <= right; ++c)
                set_cell(r, c, QString());
        recalculate();
        emit data_changed();
    }
}

void SpreadsheetWidget::paste_clipboard() {
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    int row = table_->currentRow();
    int col = table_->currentColumn();
    if (row < 0)
        row = 0;
    if (col < 0)
        col = 0;

    // Split on \n but tolerate CRLF from Windows apps; a single value with no
    // separators still pastes into the one active cell.
    const QStringList lines = text.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        if (line.endsWith('\r'))
            line.chop(1);
        // A trailing newline shouldn't create a phantom blank row.
        if (line.isEmpty() && i == lines.size() - 1)
            break;
        const QStringList cells = line.split('\t');
        const int target_row = row + i;
        if (target_row >= table_->rowCount())
            table_->setRowCount(target_row + 1);
        for (int j = 0; j < cells.size(); ++j) {
            const int target_col = col + j;
            if (target_col >= table_->columnCount()) {
                table_->setColumnCount(target_col + 1);
                setup_headers(table_->columnCount());
            }
            set_cell(target_row, target_col, cells[j]);
        }
    }
    recalculate();
    emit data_changed();
}

void SpreadsheetWidget::clear_selection() {
    const auto ranges = table_->selectedRanges();
    if (ranges.isEmpty()) {
        if (table_->currentRow() < 0)
            return;
        set_cell(table_->currentRow(), table_->currentColumn(), QString());
    } else {
        for (const auto& range : ranges)
            for (int r = range.topRow(); r <= range.bottomRow(); ++r)
                for (int c = range.leftColumn(); c <= range.rightColumn(); ++c)
                    set_cell(r, c, QString());
    }
    recalculate();
    emit data_changed();
}

// ── Find ─────────────────────────────────────────────────────────────────────

void SpreadsheetWidget::find_next(bool prompt) {
    if (prompt || last_find_.isEmpty()) {
        bool ok = false;
        const QString term =
            QInputDialog::getText(this, tr("Find"), tr("Find text:"), QLineEdit::Normal, last_find_, &ok);
        if (!ok || term.isEmpty())
            return;
        last_find_ = term;
    }

    const int rows = table_->rowCount();
    const int cols = table_->columnCount();
    if (rows == 0 || cols == 0)
        return;

    // Start just after the active cell so repeated F3 walks the sheet, wrapping
    // once back to the origin.
    const int start = std::max(0, table_->currentRow()) * cols + std::max(0, table_->currentColumn()) + 1;
    const int total = rows * cols;
    for (int n = 0; n < total; ++n) {
        const int idx = (start + n) % total;
        const int r = idx / cols;
        const int c = idx % cols;
        auto* item = table_->item(r, c);
        if (!item)
            continue;
        // Match on what the user SEES (the evaluated value) as well as the raw
        // formula text, so searching "1500" finds a cell showing =SUM(A1:A9).
        if (item->data(Qt::DisplayRole).toString().contains(last_find_, Qt::CaseInsensitive) ||
            cell_text(r, c).contains(last_find_, Qt::CaseInsensitive)) {
            table_->setCurrentCell(r, c);
            table_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

} // namespace fincept::screens
