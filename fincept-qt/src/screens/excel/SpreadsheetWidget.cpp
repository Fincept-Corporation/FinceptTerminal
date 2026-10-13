// src/screens/excel/SpreadsheetWidget.cpp
#include "screens/excel/SpreadsheetWidget.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QShortcut>
#include <QStackedWidget>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <vector>

namespace fincept::screens {

using namespace fincept::ui;
namespace fe = fincept::screens::formula;

namespace {

// Bumped on every edit anywhere in any workbook. A cell's cached value is valid
// while its generation matches — an edit invalidates everything at once, and
// the next paint recomputes only what is visible (plus its dependencies).
quint64 g_ssw_generation = 1;

void ssw_bump_generation() {
    ++g_ssw_generation;
}

// Private clipboard format: raw cell text (formulas included) plus where it was
// copied from, so a paste inside the terminal keeps formulas and shifts their
// relative references the way Excel does.
const char* const kSswClipboardMime = "application/x-fincept-sheet-cells";

// TSV as Excel writes it: fields containing a tab, newline or quote are quoted
// and inner quotes doubled.
QString ssw_tsv_field(const QString& s) {
    if (s.contains(QLatin1Char('\t')) || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('\r')) ||
        s.contains(QLatin1Char('"'))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

// Parse TSV from Excel / Google Sheets / a text editor: quoted fields may hold
// tabs and line breaks; CRLF and a trailing newline are tolerated.
QVector<QStringList> ssw_parse_tsv(const QString& text) {
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool in_quotes = false;
    bool field_started = false;
    const int n = static_cast<int>(text.size());
    for (int i = 0; i < n; ++i) {
        const QChar c = text[i];
        if (in_quotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < n && text[i + 1] == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                field += c;
            }
            continue;
        }
        if (c == QLatin1Char('"') && !field_started) {
            in_quotes = true;
            field_started = true;
        } else if (c == QLatin1Char('\t')) {
            row << field;
            field.clear();
            field_started = false;
        } else if (c == QLatin1Char('\r')) {
            continue;
        } else if (c == QLatin1Char('\n')) {
            row << field;
            rows << row;
            row.clear();
            field.clear();
            field_started = false;
        } else {
            field += c;
            field_started = true;
        }
    }
    if (field_started || !field.isEmpty() || !row.isEmpty()) {
        row << field;
        rows << row;
    }
    return rows;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Undo commands
// ═════════════════════════════════════════════════════════════════════════════

/// A batch of cell writes (typing, paste, clear, cut, formula bar, MCP edits).
class SheetEditCommand : public QUndoCommand {
  public:
    SheetEditCommand(SpreadsheetWidget* sheet, QVector<SpreadsheetWidget::CellEdit> edits, const QString& label,
                     bool already_applied)
        : QUndoCommand(label), sheet_(sheet), edits_(std::move(edits)), skip_first_redo_(already_applied) {}

    void undo() override { apply(true); }
    void redo() override {
        if (skip_first_redo_) { // the edit happened before the command was pushed
            skip_first_redo_ = false;
            return;
        }
        apply(false);
    }

  private:
    void apply(bool undo) {
        if (!sheet_)
            return;
        sheet_->applying_command_ = true;
        if (undo) {
            for (qsizetype i = edits_.size() - 1; i >= 0; --i)
                sheet_->write_raw(edits_[i].row, edits_[i].col, edits_[i].before);
        } else {
            for (const auto& e : edits_)
                sheet_->write_raw(e.row, e.col, e.after);
        }
        sheet_->applying_command_ = false;
        if (!edits_.isEmpty())
            sheet_->table_->setCurrentCell(edits_.first().row, edits_.first().col);
        sheet_->recalculate();
        emit sheet_->data_changed();
    }

    QPointer<SpreadsheetWidget> sheet_;
    QVector<SpreadsheetWidget::CellEdit> edits_;
    bool skip_first_redo_ = false;
};

/// Row/column insert or delete, including the formula references it rewrote
/// across the workbook (so undo puts every formula back exactly).
class SheetStructureCommand : public QUndoCommand {
  public:
    SheetStructureCommand(SpreadsheetWidget* sheet, fe::Axis axis, int at, int count, const QString& label)
        : QUndoCommand(label), sheet_(sheet), axis_(axis), at_(at), count_(count) {}

    void redo() override {
        if (!sheet_)
            return;
        QTableWidget* t = sheet_->table_;
        const bool rows = axis_ == fe::Axis::Rows;
        rewrites_.clear();

        // 1. Remember what a delete is about to destroy.
        if (count_ < 0 && removed_.isEmpty()) {
            const int n = -count_;
            const int other = rows ? t->columnCount() : t->rowCount();
            for (int k = 0; k < n; ++k) {
                QStringList line;
                for (int j = 0; j < other; ++j)
                    line << (rows ? sheet_->cell_text(at_ + k, j) : sheet_->cell_text(j, at_ + k));
                removed_ << line;
            }
        }

        // 2. Move every reference to this sheet, in every sheet of the workbook
        //    (pre-change coordinates). Writes here are part of THIS command, so no
        //    sheet may record them as edits of its own.
        set_applying(true);
        const QString target = sheet_->sheet_name();
        for (SpreadsheetWidget* s : sheet_->workbook_sheets()) {
            QTableWidget* st = s->table_;
            for (int r = 0; r < st->rowCount(); ++r) {
                for (int c = 0; c < st->columnCount(); ++c) {
                    const auto* it = dynamic_cast<const SpreadsheetItem*>(st->item(r, c));
                    if (!it || !it->is_formula())
                        continue;
                    bool changed = false;
                    const QString next =
                        fe::adjust_for_structure(it->raw_text(), s->sheet_name(), target, axis_, at_, count_, &changed);
                    if (!changed)
                        continue;
                    rewrites_.push_back({QPointer<SpreadsheetWidget>(s), r, c, it->raw_text(), next});
                    s->write_raw(r, c, next);
                }
            }
        }

        // 3. Change the grid.
        if (count_ > 0) {
            for (int k = 0; k < count_; ++k)
                rows ? t->insertRow(at_) : t->insertColumn(at_);
        } else {
            for (int k = 0; k < -count_; ++k)
                rows ? t->removeRow(at_) : t->removeColumn(at_);
        }
        set_applying(false);
        finish();
    }

    void undo() override {
        if (!sheet_)
            return;
        QTableWidget* t = sheet_->table_;
        const bool rows = axis_ == fe::Axis::Rows;
        set_applying(true);
        if (count_ > 0) {
            for (int k = 0; k < count_; ++k)
                rows ? t->removeRow(at_) : t->removeColumn(at_);
        } else {
            for (int k = 0; k < -count_; ++k)
                rows ? t->insertRow(at_) : t->insertColumn(at_);
            for (int k = 0; k < removed_.size(); ++k) {
                const QStringList& line = removed_[k];
                for (int j = 0; j < line.size(); ++j) {
                    if (line[j].isEmpty())
                        continue;
                    rows ? sheet_->write_raw(at_ + k, j, line[j]) : sheet_->write_raw(j, at_ + k, line[j]);
                }
            }
        }
        for (qsizetype i = rewrites_.size() - 1; i >= 0; --i) {
            const Rewrite& rw = rewrites_[i];
            if (rw.sheet)
                rw.sheet->write_raw(rw.row, rw.col, rw.before);
        }
        set_applying(false);
        finish();
    }

  private:
    struct Rewrite {
        QPointer<SpreadsheetWidget> sheet;
        int row = 0;
        int col = 0;
        QString before;
        QString after;
    };

    void set_applying(bool on) {
        for (SpreadsheetWidget* s : sheet_->workbook_sheets())
            s->applying_command_ = on;
    }

    void finish() {
        ssw_bump_generation();
        if (axis_ == fe::Axis::Cols)
            sheet_->setup_headers(sheet_->table_->columnCount());
        for (SpreadsheetWidget* s : sheet_->workbook_sheets())
            s->recalculate();
        emit sheet_->data_changed();
    }

    QPointer<SpreadsheetWidget> sheet_;
    fe::Axis axis_;
    int at_ = 0;
    int count_ = 0; // > 0 insert, < 0 delete
    QVector<QStringList> removed_;
    QVector<Rewrite> rewrites_;
};

// ═════════════════════════════════════════════════════════════════════════════
// SpreadsheetItem — formula-aware cell
// ═════════════════════════════════════════════════════════════════════════════

SpreadsheetItem::SpreadsheetItem() : QTableWidgetItem(QTableWidgetItem::UserType) {}

SpreadsheetItem::SpreadsheetItem(const QString& text) : QTableWidgetItem(QTableWidgetItem::UserType), raw_text_(text) {}

QTableWidgetItem* SpreadsheetItem::clone() const {
    return new SpreadsheetItem(raw_text_);
}

void SpreadsheetItem::setData(int role, const QVariant& value) {
    if (role == Qt::EditRole || role == Qt::DisplayRole) {
        const QString text = value.toString();
        if (text != raw_text_) {
            prev_raw_text_ = raw_text_;
            raw_text_ = text;
            compiled_.reset();
            import_cached_.clear(); // the file's stored value no longer describes this cell
            ssw_bump_generation();
        }
    }
    QTableWidgetItem::setData(role, value);
}

const formula::Compiled& SpreadsheetItem::compiled() const {
    if (!compiled_)
        compiled_ = std::make_shared<formula::Compiled>(fe::compile(raw_text_));
    return *compiled_;
}

QVariant SpreadsheetItem::data(int role) const {
    if (role == Qt::EditRole)
        return raw_text_;

    const auto* owner = tableWidget() ? qobject_cast<const SpreadsheetWidget*>(tableWidget()->parentWidget()) : nullptr;
    const bool formula = is_formula();

    // Value used by the presentation roles below.
    auto current_value = [this, owner, formula]() -> fe::Value {
        if (!formula)
            return fe::literal_value(raw_text_);
        if (!owner)
            return fe::Value::str(raw_text_);
        return owner->evaluate_item(this);
    };
    auto shows_import_cache = [this, formula](const fe::Value& v) {
        return formula && v.is_error() && v.text == fe::errors::name() && !import_cached_.isEmpty();
    };

    switch (role) {
        case Qt::DisplayRole: {
            if (!formula) // a leading apostrophe marks text; it is not shown (as in Excel)
                return raw_text_.startsWith(QLatin1Char('\'')) ? raw_text_.mid(1) : raw_text_;
            const fe::Value v = current_value();
            if (shows_import_cache(v))
                return import_cached_;
            return fe::display_text(v);
        }
        case Qt::TextAlignmentRole: {
            if (raw_text_.isEmpty())
                break;
            const fe::Value v = current_value();
            Qt::Alignment h = Qt::AlignLeft;
            if (v.is_number() || shows_import_cache(v))
                h = Qt::AlignRight;
            else if (v.is_bool() || v.is_error())
                h = Qt::AlignHCenter;
            return QVariant(static_cast<int>(h | Qt::AlignVCenter));
        }
        case Qt::ForegroundRole: {
            if (!formula && !raw_text_.startsWith(QLatin1Char('#')))
                break;
            const fe::Value v = current_value();
            if (shows_import_cache(v))
                return QColor(colors::TEXT_SECONDARY());
            if (v.is_error())
                return QColor(colors::NEGATIVE()); // an error must never read like a number
            break;
        }
        case Qt::ToolTipRole: {
            if (!formula)
                break;
            return owner ? owner->cell_tooltip(this) : QVariant(raw_text_);
        }
        case Qt::FontRole: {
            // An imported value standing in for an unsupported formula is italic.
            if (formula && !import_cached_.isEmpty() && owner) {
                const fe::Value v = current_value();
                if (shows_import_cache(v)) {
                    QFont f = tableWidget()->font();
                    f.setItalic(true);
                    return f;
                }
            }
            // A label in column A with nothing beside it is a section heading
            // ("INPUTS & ASSUMPTIONS"), not a data row.
            if (column() == 0 && !raw_text_.trimmed().isEmpty() && !formula) {
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
            break;
        }
        default:
            break;
    }
    return QTableWidgetItem::data(role);
}

// ═════════════════════════════════════════════════════════════════════════════
// Evaluation
// ═════════════════════════════════════════════════════════════════════════════

QVector<SpreadsheetWidget*> SpreadsheetWidget::workbook_sheets() const {
    QVector<SpreadsheetWidget*> out;
    // Pages of the workbook's QTabWidget live in its QStackedWidget. Count only
    // pages still in the stack — a tab removed on import stays a child until its
    // deleteLater() runs and must not shadow the new sheet of the same name.
    if (auto* stack = qobject_cast<QStackedWidget*>(parentWidget())) {
        for (int i = 0; i < stack->count(); ++i)
            if (auto* s = qobject_cast<SpreadsheetWidget*>(stack->widget(i)))
                out << s;
    } else if (QWidget* p = parentWidget()) {
        const auto siblings = p->findChildren<SpreadsheetWidget*>(Qt::FindDirectChildrenOnly);
        for (SpreadsheetWidget* s : siblings)
            out << s;
    }
    auto* self = const_cast<SpreadsheetWidget*>(this);
    if (!out.contains(self))
        out.prepend(self);
    return out;
}

const SpreadsheetWidget* SpreadsheetWidget::sheet_named(const QString& name) const {
    if (name.isEmpty() || name.compare(sheet_name_, Qt::CaseInsensitive) == 0)
        return this;
    for (const SpreadsheetWidget* s : workbook_sheets())
        if (s->sheet_name_.compare(name, Qt::CaseInsensitive) == 0)
            return s;
    return nullptr;
}

fe::Value SpreadsheetWidget::literal_or_cached(const SpreadsheetWidget* sheet, int row, int col) const {
    if (row < 0 || col < 0 || row >= sheet->table_->rowCount() || col >= sheet->table_->columnCount())
        return fe::Value::empty();
    const auto* it = dynamic_cast<const SpreadsheetItem*>(sheet->table_->item(row, col));
    if (!it)
        return fe::Value::empty();
    if (!it->is_formula())
        return fe::literal_value(it->raw_text_);
    if (it->cached_gen_ == g_ssw_generation) {
        const fe::Value& v = it->cached_value_;
        // An imported workbook's stored value stands in for a formula the
        // engine cannot evaluate, for dependents as well as for display.
        if (v.is_error() && v.text == fe::errors::name() && !it->import_cached_.isEmpty())
            return fe::literal_value(it->import_cached_);
        return v;
    }
    // Only reachable inside a reference cycle (dependencies are evaluated first).
    return fe::Value::err(fe::errors::cycle(), QStringLiteral("Circular reference"));
}

// Iterative depth-first evaluation: a dependency chain thousands of cells deep
// (=A1+1 filled down a column) is evaluated without recursing, so it cannot
// overflow the stack; a cycle yields #CYCLE! instead of hanging.
fe::Value SpreadsheetWidget::evaluate_item(const SpreadsheetItem* root) const {
    if (root->cached_gen_ == g_ssw_generation)
        return root->cached_value_;

    // Local class: shares this member function's access to the widget internals.
    class Context final : public fe::EvalContext {
      public:
        Context(const SpreadsheetWidget* home, const SpreadsheetWidget* sheet) : home_(home), sheet_(sheet) {}
        fe::Value cell(const QString& sheet, int row, int col) override {
            const SpreadsheetWidget* s = sheet.isEmpty() ? sheet_ : sheet_->sheet_named(sheet);
            if (!s)
                return fe::Value::err(fe::errors::ref(), QStringLiteral("Unknown sheet: %1").arg(sheet));
            return home_->literal_or_cached(s, row, col);
        }
        int row_count(const QString& sheet) override {
            const SpreadsheetWidget* s = sheet.isEmpty() ? sheet_ : sheet_->sheet_named(sheet);
            return s ? s->table_->rowCount() : -1;
        }
        int col_count(const QString& sheet) override {
            const SpreadsheetWidget* s = sheet.isEmpty() ? sheet_ : sheet_->sheet_named(sheet);
            return s ? s->table_->columnCount() : -1;
        }

      private:
        const SpreadsheetWidget* home_;
        const SpreadsheetWidget* sheet_;
    };

    struct Frame {
        const SpreadsheetItem* item;
        const SpreadsheetWidget* sheet;
        qsizetype ref = 0;
        int r = -1;
        int c = -1;
        bool cycle = false;
    };
    std::vector<Frame> stack;
    stack.push_back({root, this});
    root->on_stack_ = true;

    while (!stack.empty()) {
        bool pushed = false;
        {
            Frame& f = stack.back();
            const fe::Compiled& comp = f.item->compiled();
            while (!pushed && f.ref < comp.refs.size()) {
                const fe::RangeRef& rr = comp.refs[f.ref];
                const SpreadsheetWidget* dep_sheet = rr.sheet.isEmpty() ? f.sheet : f.sheet->sheet_named(rr.sheet);
                if (!dep_sheet) {
                    ++f.ref;
                    f.r = -1;
                    continue;
                }
                const int max_r = dep_sheet->table_->rowCount() - 1;
                const int max_c = dep_sheet->table_->columnCount() - 1;
                const int r1 = rr.whole_col ? 0 : rr.row1;
                const int r2 = rr.whole_col ? max_r : std::min(rr.row2, max_r);
                const int c1 = rr.whole_row ? 0 : rr.col1;
                const int c2 = rr.whole_row ? max_c : std::min(rr.col2, max_c);
                if (f.r < 0) {
                    f.r = r1;
                    f.c = c1;
                }
                while (f.r <= r2) {
                    if (f.c > c2) {
                        ++f.r;
                        f.c = c1;
                        continue;
                    }
                    const auto* dep = dynamic_cast<const SpreadsheetItem*>(dep_sheet->table_->item(f.r, f.c));
                    ++f.c;
                    if (!dep || !dep->is_formula() || dep->cached_gen_ == g_ssw_generation)
                        continue;
                    if (dep->on_stack_) {
                        f.cycle = true;
                        continue;
                    }
                    dep->on_stack_ = true;
                    stack.push_back({dep, dep_sheet}); // invalidates `f` — leave the loops now
                    pushed = true;
                    break;
                }
                if (!pushed) {
                    ++f.ref;
                    f.r = -1;
                }
            }
        }
        if (pushed)
            continue;

        const Frame done = stack.back();
        stack.pop_back();
        fe::Value v;
        if (done.cycle) {
            v = fe::Value::err(fe::errors::cycle(), QStringLiteral("Circular reference"));
        } else {
            Context ctx(this, done.sheet);
            v = fe::evaluate(done.item->compiled(), ctx);
        }
        done.item->cached_value_ = v;
        done.item->cached_gen_ = g_ssw_generation;
        done.item->on_stack_ = false;
    }
    return root->cached_value_;
}

QString SpreadsheetWidget::cell_tooltip(const SpreadsheetItem* item) const {
    QString tip = item->raw_text_;
    const fe::Value v = evaluate_item(item);
    if (v.is_error()) {
        if (v.text == fe::errors::name() && !item->import_cached_.isEmpty()) {
            tip += QLatin1Char('\n') +
                   tr("The built-in engine can't evaluate this formula (%1). Showing the value saved in the file — "
                      "it will not update when its inputs change.")
                       .arg(v.detail.isEmpty() ? v.text : v.detail);
        } else {
            tip += QLatin1Char('\n') + v.text + (v.detail.isEmpty() ? QString() : QStringLiteral(" — ") + v.detail);
        }
    }
    return tip;
}

fe::Value SpreadsheetWidget::value_at(int row, int col) const {
    const auto* it = dynamic_cast<const SpreadsheetItem*>(table_->item(row, col));
    if (!it)
        return fe::Value::empty();
    if (!it->is_formula())
        return fe::literal_value(it->raw_text_);
    const fe::Value v = evaluate_item(it);
    if (v.is_error() && v.text == fe::errors::name() && !it->import_cached_.isEmpty())
        return fe::literal_value(it->import_cached_);
    return v;
}

QString SpreadsheetWidget::display_value(int row, int col) const {
    const auto* it = table_->item(row, col);
    return it ? it->data(Qt::DisplayRole).toString() : QString();
}

// ═════════════════════════════════════════════════════════════════════════════
// SpreadsheetWidget
// ═════════════════════════════════════════════════════════════════════════════

SpreadsheetWidget::SpreadsheetWidget(const QString& sheet_name, int rows, int cols, QWidget* parent)
    : QWidget(parent), sheet_name_(sheet_name) {
    undo_stack_ = new QUndoStack(this);
    undo_stack_->setUndoLimit(200);
    build_ui(rows, cols);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void SpreadsheetWidget::build_ui(int rows, int cols) {
    // One stylesheet for the whole sheet, keyed on object names.
    setStyleSheet(QString("#sswFormulaRow { background:%1; border-bottom:1px solid %2; }"
                          "#sswCellRef { color:%3; font-family:%4; font-size:11px; font-weight:700;"
                          "  border-right:1px solid %2; padding-right:6px; background:transparent; }"
                          "#sswFx { color:%5; font-family:%4; font-size:11px; font-weight:700; background:transparent; }"
                          "#sswFormulaBar { background:%1; color:%6; border:none; font-family:%4; font-size:11px;"
                          "  padding:2px 4px; }"
                          "#sswFormulaBar:focus { background:%7; }"
                          "QTableWidget#sswTable { background:%7; color:%6; gridline-color:%8; font-family:%4;"
                          "  font-size:11px; border:none; }"
                          "QTableWidget#sswTable::item { padding:2px 4px; }"
                          "QTableWidget#sswTable::item:selected { background:%9; }"
                          "QTableWidget#sswTable QHeaderView::section { background:%2; color:%10; border:none;"
                          "  border-right:1px solid %8; border-bottom:1px solid %8; font-family:%4; font-size:10px;"
                          "  font-weight:600; padding:3px 4px; }"
                          "QTableWidget#sswTable QTableCornerButton::section { background:%2; border:none; }")
                      .arg(colors::BG_RAISED(), colors::BORDER_MED(), colors::AMBER(), fonts::DATA_FAMILY,
                           colors::TEXT_TERTIARY(), colors::TEXT_PRIMARY(), colors::BG_HOVER(), colors::TEXT_DIM(),
                           colors::BORDER_BRIGHT())
                      .arg(colors::TEXT_SECONDARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Formula bar
    auto* formula_row = new QWidget(this);
    formula_row->setObjectName("sswFormulaRow");
    formula_row->setAttribute(Qt::WA_StyledBackground, true);
    formula_row->setFixedHeight(28);

    auto* fhl = new QHBoxLayout(formula_row);
    fhl->setContentsMargins(8, 0, 8, 0);
    fhl->setSpacing(6);

    cell_ref_label_ = new QLabel("A1", formula_row);
    cell_ref_label_->setObjectName("sswCellRef");
    cell_ref_label_->setFixedWidth(64);
    cell_ref_label_->setAlignment(Qt::AlignCenter);
    fhl->addWidget(cell_ref_label_);

    auto* fx_label = new QLabel("fx", formula_row);
    fx_label->setObjectName("sswFx");
    fhl->addWidget(fx_label);

    formula_bar_ = new QLineEdit(formula_row);
    formula_bar_->setObjectName("sswFormulaBar");
    formula_bar_->setToolTip(tr("Type a value or a formula starting with = and press Enter.\n"
                                "Functions: %1")
                                 .arg(fe::supported_functions().join(QStringLiteral(", "))));
    connect(formula_bar_, &QLineEdit::returnPressed, this, &SpreadsheetWidget::on_formula_bar_return);
    fhl->addWidget(formula_bar_, 1);

    root->addWidget(formula_row);

    // Table
    table_ = new QTableWidget(rows, cols, this);
    table_->setObjectName("sswTable");
    // Cells are created lazily; the prototype makes every implicitly created
    // cell a formula-aware SpreadsheetItem rather than a plain one.
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

    // Cells are deliberately NOT pre-populated: a 20 000 × 64 import window is
    // 1.3M cells. Every read path tolerates a null item, so an empty grid costs
    // nothing until something is written to it.

    connect(table_, &QTableWidget::cellChanged, this, &SpreadsheetWidget::on_cell_changed);
    connect(table_, &QTableWidget::currentCellChanged, this, &SpreadsheetWidget::on_current_cell_changed);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &SpreadsheetWidget::on_context_menu);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &SpreadsheetWidget::selection_changed);

    root->addWidget(table_, 1);

    table_->setAccessibleName(tr("Spreadsheet grid"));
    formula_bar_->setAccessibleName(tr("Formula bar"));
    cell_ref_label_->setAccessibleName(tr("Active cell reference"));
    setTabOrder(formula_bar_, table_);

    install_shortcuts();
}

// ── Keyboard shortcuts ───────────────────────────────────────────────────────
// QTableWidget ships with no clipboard, undo or find handling.

void SpreadsheetWidget::install_shortcuts() {
    // WidgetWithChildrenShortcut also covers the in-place cell editor, so every
    // grid-level binding must stand down while a cell is being edited —
    // otherwise Delete would blank the whole selection mid-keystroke and Ctrl+C
    // would copy cells instead of the selected characters.
    auto add = [this](const QKeySequence& key, std::function<void()> fn) {
        auto* sc = new QShortcut(key, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, fn = std::move(fn)]() {
            const QWidget* fw = QApplication::focusWidget();
            if (fw && fw != table_ && table_->isAncestorOf(fw))
                return; // an in-place editor is open
            if (fw == formula_bar_)
                return; // the formula bar has its own undo/clipboard
            fn();
        });
    };
    add(QKeySequence(QKeySequence::Copy), [this]() { copy_selection(false); });
    add(QKeySequence(QKeySequence::Cut), [this]() { copy_selection(true); });
    add(QKeySequence(QKeySequence::Paste), [this]() { paste_clipboard(false); });
    add(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), [this]() { paste_clipboard(true); });
    add(QKeySequence(QKeySequence::Delete), [this]() { clear_selection(); });
    add(QKeySequence(QKeySequence::Undo), [this]() { undo(); });
    add(QKeySequence(QKeySequence::Redo), [this]() { redo(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Y), [this]() { redo(); });
    add(QKeySequence(QKeySequence::Find), [this]() { find_next(true); });
    add(QKeySequence(QKeySequence::FindNext), [this]() { find_next(false); });
}

void SpreadsheetWidget::setup_headers(int cols) {
    QStringList col_headers;
    col_headers.reserve(cols);
    for (int c = 0; c < cols; ++c)
        col_headers << column_label(c);
    table_->setHorizontalHeaderLabels(col_headers);
}

QString SpreadsheetWidget::column_label(int col) {
    return fe::column_label(col);
}

// ── Data access ──────────────────────────────────────────────────────────────

QVector<QVector<QString>> SpreadsheetWidget::get_data() const {
    const int rows = table_->rowCount();
    const int cols = table_->columnCount();
    QVector<QVector<QString>> cells(rows);
    for (int r = 0; r < rows; ++r) {
        cells[r].resize(cols);
        for (int c = 0; c < cols; ++c) {
            const auto* item = dynamic_cast<const SpreadsheetItem*>(table_->item(r, c));
            if (item)
                cells[r][c] = item->raw_text();
        }
    }
    return cells;
}

int SpreadsheetWidget::used_rows() const {
    for (int r = table_->rowCount() - 1; r >= 0; --r)
        for (int c = 0; c < table_->columnCount(); ++c)
            if (const auto* it = table_->item(r, c); it && !it->data(Qt::EditRole).toString().isEmpty())
                return r + 1;
    return 0;
}

int SpreadsheetWidget::used_cols() const {
    const int rows = used_rows();
    int last = -1;
    for (int r = 0; r < rows; ++r)
        for (int c = table_->columnCount() - 1; c > last; --c)
            if (const auto* it = table_->item(r, c); it && !it->data(Qt::EditRole).toString().isEmpty()) {
                last = c;
                break;
            }
    return last + 1;
}

QVector<QVector<QString>> SpreadsheetWidget::get_used_data() const {
    const int rows = used_rows();
    const int cols = rows > 0 ? used_cols() : 0;
    QVector<QVector<QString>> cells(rows);
    for (int r = 0; r < rows; ++r) {
        cells[r].resize(cols);
        for (int c = 0; c < cols; ++c)
            cells[r][c] = cell_text(r, c);
    }
    return cells;
}

void SpreadsheetWidget::set_data(const QVector<QVector<QString>>& cells, const QHash<qint64, QString>& cached) {
    table_->blockSignals(true);

    int rows = static_cast<int>(cells.size());
    // Take the WIDEST row, not row 0 — a ragged import (CSV with a short header
    // line) previously truncated every following row to the header's width.
    int cols = 0;
    for (const auto& row : cells)
        cols = std::max<int>(cols, static_cast<int>(row.size()));

    rows = std::max(rows, 100);
    cols = std::max(cols, 26);

    table_->clearContents(); // drop any items from a previous load
    table_->setRowCount(rows);
    table_->setColumnCount(cols);
    setup_headers(cols);

    // Allocate ONLY the cells that carry a value; the item prototype
    // materialises the rest the moment the user types into one.
    for (int r = 0; r < cells.size() && r < rows; ++r) {
        const auto& src_row = cells[r];
        for (int c = 0; c < src_row.size() && c < cols; ++c) {
            if (src_row[c].isEmpty())
                continue;
            auto* item = new SpreadsheetItem(src_row[c]);
            const auto hit = cached.constFind(cache_key(r, c));
            if (hit != cached.constEnd())
                item->set_import_cached(hit.value());
            table_->setItem(r, c, item);
        }
    }

    table_->blockSignals(false);
    ssw_bump_generation();
    // A wholesale replace has no meaningful per-cell undo.
    undo_stack_->clear();
    autofit_columns(cells);
    recalculate();
}

// Size columns to their content, measured from the source text with the
// table's own font (resizeColumnsToContents() would evaluate every formula).
void SpreadsheetWidget::autofit_columns(const QVector<QVector<QString>>& cells) {
    constexpr int kMinWidth = 64;
    constexpr int kMaxWidth = 340; // past this, wrapping/scrolling beats widening
    constexpr int kPadding = 18;

    // Sample the head of the sheet — headers and the first data rows set the width.
    constexpr int kMaxMeasuredRows = 200;
    const int measured_rows = std::min<int>(static_cast<int>(cells.size()), kMaxMeasuredRows);

    const QFontMetrics fm(table_->font());
    // A formula's text is far longer than the number it produces.
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

void SpreadsheetWidget::grow_column_for(int col, const QString& text) {
    if (text.startsWith(QLatin1Char('=')) || text.trimmed().isEmpty() || col < 0)
        return;
    constexpr int kMaxWidth = 340;
    constexpr int kPadding = 18;
    const QFontMetrics fm(table_->font());
    const int needed = std::min(fm.horizontalAdvance(text) + kPadding, kMaxWidth);
    if (needed > table_->columnWidth(col))
        table_->setColumnWidth(col, needed);
}

QString SpreadsheetWidget::cell_text(int row, int col) const {
    const auto* item = dynamic_cast<const SpreadsheetItem*>(table_->item(row, col));
    return item ? item->raw_text() : QString();
}

void SpreadsheetWidget::write_raw(int row, int col, const QString& text) {
    if (row < 0 || col < 0)
        return;
    if (row >= table_->rowCount())
        table_->setRowCount(row + 1);
    if (col >= table_->columnCount()) {
        table_->setColumnCount(col + 1);
        setup_headers(table_->columnCount());
    }
    auto* item = dynamic_cast<SpreadsheetItem*>(table_->item(row, col));
    if (!item) {
        if (text.isEmpty())
            return;
        item = new SpreadsheetItem(text);
        table_->setItem(row, col, item);
        ssw_bump_generation();
    } else {
        item->setData(Qt::EditRole, text);
    }
    grow_column_for(col, text);
}

void SpreadsheetWidget::apply_edits(const QVector<CellEdit>& edits, const QString& label) {
    QVector<CellEdit> real;
    real.reserve(edits.size());
    for (const CellEdit& e : edits)
        if (e.before != e.after)
            real.push_back(e);
    if (real.isEmpty())
        return;
    undo_stack_->push(new SheetEditCommand(this, real, label, false)); // push() runs redo()
}

void SpreadsheetWidget::set_cell(int row, int col, const QString& text) {
    apply_edits({{row, col, cell_text(row, col), text}}, tr("Edit %1").arg(fe::cell_label(row, col)));
}

void SpreadsheetWidget::set_sheet_name(const QString& name) {
    const QString old = sheet_name_;
    if (old == name)
        return;
    // Rewrite references from every sheet of the workbook ("Old!A1" → "New!A1").
    for (SpreadsheetWidget* s : workbook_sheets()) {
        for (int r = 0; r < s->table_->rowCount(); ++r) {
            for (int c = 0; c < s->table_->columnCount(); ++c) {
                const auto* it = dynamic_cast<const SpreadsheetItem*>(s->table_->item(r, c));
                if (!it || !it->is_formula())
                    continue;
                bool changed = false;
                const QString next = fe::rename_sheet_references(it->raw_text(), old, name, &changed);
                if (changed) {
                    s->applying_command_ = true;
                    s->write_raw(r, c, next);
                    s->applying_command_ = false;
                }
            }
        }
    }
    sheet_name_ = name;
    ssw_bump_generation();
    for (SpreadsheetWidget* s : workbook_sheets())
        s->recalculate();
}

void SpreadsheetWidget::recalculate() {
    table_->viewport()->update();
}

void SpreadsheetWidget::undo() {
    if (undo_stack_->canUndo())
        undo_stack_->undo();
}

QString SpreadsheetWidget::selection_summary() const {
    // Bounded so selecting whole columns of a large import stays instant.
    constexpr qint64 kMaxCells = 200000;
    const auto ranges = table_->selectedRanges();
    qint64 cells = 0;
    int count = 0;
    int numeric = 0;
    double sum = 0.0;
    for (const auto& range : ranges) {
        for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
            for (int c = range.leftColumn(); c <= range.rightColumn(); ++c) {
                if (++cells > kMaxCells)
                    return tr("%1+ cells selected").arg(kMaxCells);
                if (!table_->item(r, c))
                    continue;
                const fe::Value v = value_at(r, c);
                if (v.is_empty())
                    continue;
                ++count;
                if (v.is_number()) {
                    ++numeric;
                    sum += v.number;
                }
            }
        }
    }
    if (count < 2)
        return {};
    const QLocale loc = QLocale::system();
    QString out = tr("COUNT %1").arg(count);
    if (numeric > 0)
        out = tr("SUM %1  ·  AVG %2  ·  %3")
                  .arg(loc.toString(sum, 'g', 12), loc.toString(sum / numeric, 'g', 12), out);
    return out;
}

void SpreadsheetWidget::redo() {
    if (undo_stack_->canRedo())
        undo_stack_->redo();
}

// ── Slots ────────────────────────────────────────────────────────────────────

void SpreadsheetWidget::on_cell_changed(int row, int col) {
    if (updating_formula_bar_ || applying_command_)
        return;
    // An in-place edit: the item already holds the new text — record it so it
    // can be undone (the command skips its first redo).
    const auto* item = dynamic_cast<const SpreadsheetItem*>(table_->item(row, col));
    if (item && item->previous_raw_text() != item->raw_text()) {
        undo_stack_->push(new SheetEditCommand(this, {{row, col, item->previous_raw_text(), item->raw_text()}},
                                               tr("Edit %1").arg(fe::cell_label(row, col)), true));
        grow_column_for(col, item->raw_text());
    }
    recalculate();
    if (row == table_->currentRow() && col == table_->currentColumn()) {
        updating_formula_bar_ = true;
        formula_bar_->setText(cell_text(row, col));
        updating_formula_bar_ = false;
    }
    emit data_changed();
}

void SpreadsheetWidget::on_current_cell_changed(int row, int col, int, int) {
    if (row < 0 || col < 0)
        return;
    updating_formula_bar_ = true;
    cell_ref_label_->setText(fe::cell_label(row, col));
    formula_bar_->setText(cell_text(row, col));
    updating_formula_bar_ = false;
}

void SpreadsheetWidget::on_formula_bar_return() {
    const int row = table_->currentRow();
    const int col = table_->currentColumn();
    if (row < 0 || col < 0)
        return;

    updating_formula_bar_ = true;
    set_cell(row, col, formula_bar_->text());
    updating_formula_bar_ = false;

    // Move to next row
    if (row + 1 < table_->rowCount())
        table_->setCurrentCell(row + 1, col);
}

// ── Context menu ─────────────────────────────────────────────────────────────

void SpreadsheetWidget::on_context_menu(const QPoint& pos) {
    QMenu menu(this);
    menu.setStyleSheet(QString("QMenu { background:%1; color:%2; border:1px solid %3;"
                               " font-family:%4; font-size:11px; }"
                               "QMenu::item { padding:6px 20px; }"
                               "QMenu::item:selected { background:%5; }"
                               "QMenu::item:disabled { color:%3; }"
                               "QMenu::separator { height:1px; background:%3; margin:4px 8px; }")
                           .arg(colors::BORDER_MED(), colors::TEXT_PRIMARY(), colors::TEXT_DIM(), fonts::DATA_FAMILY,
                                colors::TEXT_DIM()));

    QAction* undo_act = menu.addAction(tr("Undo %1").arg(undo_stack_->undoText()), this, [this]() { undo(); });
    undo_act->setEnabled(undo_stack_->canUndo());
    QAction* redo_act = menu.addAction(tr("Redo %1").arg(undo_stack_->redoText()), this, [this]() { redo(); });
    redo_act->setEnabled(undo_stack_->canRedo());
    menu.addSeparator();
    menu.addAction(tr("Cut"), this, [this]() { copy_selection(true); });
    menu.addAction(tr("Copy"), this, [this]() { copy_selection(false); });
    menu.addAction(tr("Paste"), this, [this]() { paste_clipboard(false); });
    menu.addAction(tr("Paste Values"), this, [this]() { paste_clipboard(true); });
    menu.addSeparator();
    menu.addAction(tr("Insert Row Above"), this, &SpreadsheetWidget::insert_row_above);
    menu.addAction(tr("Insert Row Below"), this, &SpreadsheetWidget::insert_row_below);
    menu.addAction(tr("Insert Column Left"), this, &SpreadsheetWidget::insert_col_left);
    menu.addAction(tr("Insert Column Right"), this, &SpreadsheetWidget::insert_col_right);
    menu.addSeparator();
    menu.addAction(tr("Delete Selected Rows"), this, &SpreadsheetWidget::delete_selected_rows);
    menu.addAction(tr("Delete Selected Columns"), this, &SpreadsheetWidget::delete_selected_cols);
    menu.addSeparator();
    menu.addAction(tr("Clear Cells"), this, [this]() { clear_selection(); });
    menu.addSeparator();
    menu.addAction(tr("Find…"), this, [this]() { find_next(true); });

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ── Insert / Delete ──────────────────────────────────────────────────────────

void SpreadsheetWidget::apply_structure(fe::Axis axis, int at, int count, const QString& label) {
    if (count == 0 || at < 0)
        return;
    undo_stack_->push(new SheetStructureCommand(this, axis, at, count, label));
}

void SpreadsheetWidget::insert_rows(int at, int count) {
    if (count <= 0)
        return;
    at = std::clamp(at, 0, table_->rowCount());
    apply_structure(fe::Axis::Rows, at, count, tr("Insert %n row(s)", "", count));
}

void SpreadsheetWidget::delete_rows(int at, int count) {
    if (count <= 0 || at < 0 || at >= table_->rowCount())
        return;
    count = std::min(count, table_->rowCount() - at);
    apply_structure(fe::Axis::Rows, at, -count, tr("Delete %n row(s)", "", count));
}

void SpreadsheetWidget::insert_cols(int at, int count) {
    if (count <= 0)
        return;
    at = std::clamp(at, 0, table_->columnCount());
    apply_structure(fe::Axis::Cols, at, count, tr("Insert %n column(s)", "", count));
}

void SpreadsheetWidget::delete_cols(int at, int count) {
    if (count <= 0 || at < 0 || at >= table_->columnCount())
        return;
    count = std::min(count, table_->columnCount() - at);
    apply_structure(fe::Axis::Cols, at, -count, tr("Delete %n column(s)", "", count));
}

void SpreadsheetWidget::insert_row_above() {
    insert_rows(std::max(0, table_->currentRow()), 1);
}

void SpreadsheetWidget::insert_row_below() {
    insert_rows(table_->currentRow() + 1, 1);
}

void SpreadsheetWidget::insert_col_left() {
    insert_cols(std::max(0, table_->currentColumn()), 1);
}

void SpreadsheetWidget::insert_col_right() {
    insert_cols(table_->currentColumn() + 1, 1);
}

// Contiguous blocks of the selected rows/columns, highest first, so deleting
// one block never shifts the next.
static QVector<QPair<int, int>> ssw_selected_blocks(const QList<QTableWidgetSelectionRange>& ranges, bool rows,
                                                    int current) {
    QVector<int> idx;
    for (const auto& range : ranges) {
        const int lo = rows ? range.topRow() : range.leftColumn();
        const int hi = rows ? range.bottomRow() : range.rightColumn();
        for (int i = lo; i <= hi; ++i)
            if (!idx.contains(i))
                idx.append(i);
    }
    if (idx.isEmpty() && current >= 0)
        idx.append(current);
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    QVector<QPair<int, int>> blocks; // (start, count)
    for (int i : idx) {
        if (!blocks.isEmpty() && blocks.last().first == i + 1) {
            blocks.last().first = i;
            ++blocks.last().second;
        } else {
            blocks.append({i, 1});
        }
    }
    return blocks;
}

void SpreadsheetWidget::delete_selected_rows() {
    const auto blocks = ssw_selected_blocks(table_->selectedRanges(), true, table_->currentRow());
    if (blocks.isEmpty())
        return;
    undo_stack_->beginMacro(tr("Delete rows"));
    for (const auto& [start, count] : blocks)
        delete_rows(start, count);
    undo_stack_->endMacro();
}

void SpreadsheetWidget::delete_selected_cols() {
    const auto blocks = ssw_selected_blocks(table_->selectedRanges(), false, table_->currentColumn());
    if (blocks.isEmpty())
        return;
    undo_stack_->beginMacro(tr("Delete columns"));
    for (const auto& [start, count] : blocks)
        delete_cols(start, count);
    undo_stack_->endMacro();
}

// ── Clipboard (TSV, Excel-compatible) ────────────────────────────────────────

void SpreadsheetWidget::copy_selection(bool cut) {
    int top = INT_MAX, left = INT_MAX, bottom = -1, right = -1;
    const auto ranges = table_->selectedRanges();
    for (const auto& r : ranges) {
        top = std::min(top, r.topRow());
        left = std::min(left, r.leftColumn());
        bottom = std::max(bottom, r.bottomRow());
        right = std::max(right, r.rightColumn());
    }
    if (bottom < 0) {
        if (table_->currentRow() < 0 || table_->currentColumn() < 0)
            return;
        top = bottom = table_->currentRow();
        left = right = table_->currentColumn();
    }

    // Plain text = what the cells SHOW (Excel semantics: pasting into a text
    // editor or another app yields values, not formulas). The private format
    // carries the raw formulas for a paste back into the terminal.
    QStringList lines;
    QJsonArray raw_rows;
    lines.reserve(bottom - top + 1);
    for (int r = top; r <= bottom; ++r) {
        QStringList shown;
        QJsonArray raw;
        for (int c = left; c <= right; ++c) {
            shown << ssw_tsv_field(display_value(r, c));
            raw.append(cell_text(r, c));
        }
        lines << shown.join(QLatin1Char('\t'));
        raw_rows.append(raw);
    }
    auto* mime = new QMimeData;
    mime->setText(lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n"));
    mime->setData(QString::fromLatin1(kSswClipboardMime),
                  QJsonDocument(QJsonObject{{"top", top}, {"left", left}, {"rows", raw_rows}})
                      .toJson(QJsonDocument::Compact));
    QApplication::clipboard()->setMimeData(mime);

    if (cut) {
        QVector<CellEdit> edits;
        for (int r = top; r <= bottom; ++r)
            for (int c = left; c <= right; ++c)
                if (!cell_text(r, c).isEmpty())
                    edits.push_back({r, c, cell_text(r, c), QString()});
        apply_edits(edits, tr("Cut"));
    }
}

void SpreadsheetWidget::paste_clipboard(bool values_only) {
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime)
        return;

    // Source block: raw text with its origin (formulas keep working and their
    // relative references shift) from our own format; otherwise TSV from any
    // other application (Excel, Google Sheets, a text editor). Paste Values
    // always takes the TSV, which holds the displayed values.
    QVector<QStringList> block;
    int origin_row = -1;
    int origin_col = -1;
    const QString fmt = QString::fromLatin1(kSswClipboardMime);
    if (!values_only && mime->hasFormat(fmt)) {
        const QJsonObject obj = QJsonDocument::fromJson(mime->data(fmt)).object();
        origin_row = obj.value("top").toInt(-1);
        origin_col = obj.value("left").toInt(-1);
        const QJsonArray rows = obj.value("rows").toArray();
        for (const QJsonValue& rv : rows) {
            QStringList line;
            const QJsonArray cells = rv.toArray();
            for (const QJsonValue& cv : cells)
                line << cv.toString();
            block << line;
        }
    }
    if (block.isEmpty()) {
        origin_row = origin_col = -1;
        block = ssw_parse_tsv(mime->text());
    }
    if (block.isEmpty())
        return;

    int top = std::max(0, table_->currentRow());
    int left = std::max(0, table_->currentColumn());
    int out_rows = static_cast<int>(block.size());
    int out_cols = 0;
    for (const auto& line : block)
        out_cols = std::max(out_cols, static_cast<int>(line.size()));

    // One copied cell pasted onto a multi-cell selection fills all of it (Excel).
    const bool single_source = out_rows == 1 && out_cols == 1;
    const auto ranges = table_->selectedRanges();
    if (single_source && ranges.size() == 1 && (ranges.first().rowCount() > 1 || ranges.first().columnCount() > 1)) {
        top = ranges.first().topRow();
        left = ranges.first().leftColumn();
        out_rows = ranges.first().rowCount();
        out_cols = ranges.first().columnCount();
    }

    QVector<CellEdit> edits;
    for (int i = 0; i < out_rows; ++i) {
        for (int j = 0; j < out_cols; ++j) {
            const int src_i = single_source ? 0 : i;
            const int src_j = single_source ? 0 : j;
            if (src_j >= block[src_i].size())
                continue; // ragged source row
            QString text = block[src_i][src_j];
            const int dest_row = top + i;
            const int dest_col = left + j;
            if (text.startsWith(QLatin1Char('='))) {
                if (origin_row >= 0)
                    text = fe::shift_references(text, dest_row - (origin_row + src_i), dest_col - (origin_col + src_j));
                else if (values_only)
                    text.prepend(QLatin1Char('\'')); // keep external "=..." text as text
            }
            edits.push_back({dest_row, dest_col, cell_text(dest_row, dest_col), text});
        }
    }
    apply_edits(edits, values_only ? tr("Paste values") : tr("Paste"));
}

void SpreadsheetWidget::clear_selection() {
    QVector<CellEdit> edits;
    const auto ranges = table_->selectedRanges();
    if (ranges.isEmpty()) {
        const int r = table_->currentRow();
        const int c = table_->currentColumn();
        if (r < 0 || c < 0)
            return;
        edits.push_back({r, c, cell_text(r, c), QString()});
    } else {
        for (const auto& range : ranges)
            for (int r = range.topRow(); r <= range.bottomRow(); ++r)
                for (int c = range.leftColumn(); c <= range.rightColumn(); ++c)
                    if (!cell_text(r, c).isEmpty())
                        edits.push_back({r, c, cell_text(r, c), QString()});
    }
    apply_edits(edits, tr("Clear"));
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
    const qint64 start = static_cast<qint64>(std::max(0, table_->currentRow())) * cols +
                         std::max(0, table_->currentColumn()) + 1;
    const qint64 total = static_cast<qint64>(rows) * cols;
    for (qint64 n = 0; n < total; ++n) {
        const qint64 idx = (start + n) % total;
        const int r = static_cast<int>(idx / cols);
        const int c = static_cast<int>(idx % cols);
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
