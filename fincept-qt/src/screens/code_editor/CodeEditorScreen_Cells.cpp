// src/screens/code_editor/CodeEditorScreen_Cells.cpp — CodeTextEdit, its line
// numbers and syntax highlighter, and CellWidget. See CodeEditorScreen_Cells.h.

#include "screens/code_editor/CodeEditorScreen_Cells.h"

#include "core/keys/KeyConfigManager.h"
#include "screens/code_editor/NotebookOutputs.h"
#include "screens/code_editor/NotebookStyle.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QToolButton>
#include <QVBoxLayout>

#include <QResizeEvent>
#include <QTextDocument>

#include <algorithm>
#include <cmath>
#include <climits>

namespace fincept::screens {

using notebook::CellOutput;
using notebook::NotebookCell;
namespace colors = ui::colors;

namespace {

constexpr int kIndent = 4;

QString format_elapsed(qint64 ms) {
    if (ms < 0)
        return {};
    if (ms < 1000)
        return CellWidget::tr("%1 ms").arg(ms);
    if (ms < 60000)
        return CellWidget::tr("%1 s").arg(ms / 1000.0, 0, 'f', ms < 10000 ? 2 : 1);
    return CellWidget::tr("%1m %2s").arg(ms / 60000).arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'));
}

QColor with_alpha(QColor c, double a) {
    c.setAlphaF(static_cast<float>(a));
    return c;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Syntax highlighting — formats re-derived from the theme on refresh().

class NbHighlighter : public QSyntaxHighlighter {
  public:
    explicit NbHighlighter(QTextDocument* doc) : QSyntaxHighlighter(doc) { refresh_formats(); }

    void set_markdown(bool on) {
        if (markdown_ == on)
            return;
        markdown_ = on;
        rehighlight();
    }

    void refresh() {
        refresh_formats();
        rehighlight();
    }

  protected:
    void highlightBlock(const QString& text) override {
        if (markdown_)
            highlight_markdown(text);
        else
            highlight_python(text);
    }

  private:
    enum BlockState { Normal = 0, InTripleDouble = 1, InTripleSingle = 2, InFence = 3 };

    void refresh_formats() {
        const NbSyntaxPalette p = nb_syntax_palette();
        keyword_ = QTextCharFormat();
        keyword_.setForeground(p.keyword);
        keyword_.setFontWeight(QFont::DemiBold);
        builtin_ = QTextCharFormat();
        builtin_.setForeground(p.builtin);
        string_ = QTextCharFormat();
        string_.setForeground(p.string);
        number_ = QTextCharFormat();
        number_.setForeground(p.number);
        comment_ = QTextCharFormat();
        comment_.setForeground(p.comment);
        comment_.setFontItalic(true);
        definition_ = QTextCharFormat();
        definition_.setForeground(p.definition);
        definition_.setFontWeight(QFont::DemiBold);
        decorator_ = QTextCharFormat();
        decorator_.setForeground(p.decorator);
        self_ = QTextCharFormat();
        self_.setForeground(p.self_);
        self_.setFontItalic(true);
        heading_ = QTextCharFormat();
        heading_.setForeground(p.heading);
        heading_.setFontWeight(QFont::Bold);
        code_span_ = QTextCharFormat();
        code_span_.setForeground(p.code_span);
        link_ = QTextCharFormat();
        link_.setForeground(p.link);
        link_.setFontUnderline(true);
    }

    void apply(const QString& text, const QRegularExpression& re, const QTextCharFormat& fmt, int group = 0) {
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart(group)), static_cast<int>(m.capturedLength(group)), fmt);
        }
    }

    void highlight_python(const QString& text) {
        static const QRegularExpression kw(QStringLiteral(
            "\\b(and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|"
            "import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield|None|True|False|match|case)\\b"));
        static const QRegularExpression bi(QStringLiteral(
            "\\b(print|len|range|enumerate|zip|map|filter|sum|min|max|abs|round|sorted|list|dict|set|tuple|str|int|"
            "float|bool|type|isinstance|issubclass|open|super|any|all|getattr|setattr|hasattr|repr|format|iter|next|"
            "reversed|divmod|pow|vars|dir|help|id|hash|object|display|Exception|ValueError|TypeError|KeyError|"
            "IndexError|RuntimeError|ZeroDivisionError|AttributeError|ImportError|StopIteration|NotImplementedError)\\b"));
        static const QRegularExpression num(QStringLiteral(
            "\\b(0[xX][0-9a-fA-F_]+|0[bB][01_]+|\\d[\\d_]*(\\.\\d*)?([eE][-+]?\\d+)?j?)\\b|(?<![\\w.])\\.\\d+([eE][-+]?\\d+)?"));
        static const QRegularExpression defn(QStringLiteral("\\b(?:def|class)\\s+([A-Za-z_]\\w*)"));
        static const QRegularExpression deco(QStringLiteral("^\\s*@[\\w.]+"));
        static const QRegularExpression selfre(QStringLiteral("\\b(self|cls)\\b"));
        apply(text, num, number_);
        apply(text, bi, builtin_);
        apply(text, selfre, self_);
        apply(text, kw, keyword_);
        apply(text, defn, definition_, 1);
        apply(text, deco, decorator_);

        // Strings and comments last: they win over everything inside them.
        const int len = static_cast<int>(text.size());
        int i = 0;
        int state = previousBlockState();
        setCurrentBlockState(Normal);
        if (state == InTripleDouble || state == InTripleSingle) {
            const QString delim = state == InTripleDouble ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
            const int end = static_cast<int>(text.indexOf(delim));
            if (end < 0) {
                setFormat(0, len, string_);
                setCurrentBlockState(state);
                return;
            }
            setFormat(0, end + 3, string_);
            i = end + 3;
        }
        while (i < len) {
            const QChar c = text.at(i);
            if (c == QLatin1Char('#')) {
                setFormat(i, len - i, comment_);
                return;
            }
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                const QString triple(3, c);
                if (text.mid(i, 3) == triple) {
                    const int end = static_cast<int>(text.indexOf(triple, i + 3));
                    if (end < 0) {
                        setFormat(i, len - i, string_);
                        setCurrentBlockState(c == QLatin1Char('"') ? InTripleDouble : InTripleSingle);
                        return;
                    }
                    setFormat(i, end + 3 - i, string_);
                    i = end + 3;
                    continue;
                }
                int j = i + 1;
                while (j < len && text.at(j) != c) {
                    if (text.at(j) == QLatin1Char('\\'))
                        ++j;
                    ++j;
                }
                const int end = std::min(j + 1, len);
                setFormat(i, end - i, string_);
                i = end;
                continue;
            }
            ++i;
        }
    }

    void highlight_markdown(const QString& text) {
        const int prev = previousBlockState();
        const QString trimmed = text.trimmed();
        const bool fence = trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"));
        if (prev == InFence) {
            setFormat(0, static_cast<int>(text.size()), code_span_);
            setCurrentBlockState(fence ? Normal : InFence);
            return;
        }
        if (fence) {
            setFormat(0, static_cast<int>(text.size()), code_span_);
            setCurrentBlockState(InFence);
            return;
        }
        setCurrentBlockState(Normal);
        static const QRegularExpression heading(QStringLiteral("^\\s{0,3}#{1,6}\\s.*$"));
        static const QRegularExpression bullet(QStringLiteral("^\\s*([-*+]|\\d+[.)])\\s"));
        static const QRegularExpression bold(QStringLiteral("\\*\\*[^*]+\\*\\*|__[^_]+__"));
        static const QRegularExpression code(QStringLiteral("`[^`]+`"));
        static const QRegularExpression link(QStringLiteral("\\[[^\\]]+\\]\\([^)]+\\)"));
        apply(text, heading, heading_);
        apply(text, bullet, keyword_);
        QTextCharFormat strong;
        strong.setFontWeight(QFont::Bold);
        apply(text, bold, strong);
        apply(text, link, link_);
        apply(text, code, code_span_);
    }

    bool markdown_ = false;
    QTextCharFormat keyword_, builtin_, string_, number_, comment_, definition_, decorator_, self_, heading_,
        code_span_, link_;
};

// ═════════════════════════════════════════════════════════════════════════════
// Line numbers

class NbLineNumberArea : public QWidget {
  public:
    explicit NbLineNumberArea(CodeTextEdit* editor) : QWidget(editor), editor_(editor) {}
    QSize sizeHint() const override { return {editor_->line_number_area_width(), 0}; }

  protected:
    void paintEvent(QPaintEvent* e) override { editor_->paint_line_numbers(e); }

  private:
    CodeTextEdit* editor_;
};

// ═════════════════════════════════════════════════════════════════════════════
// CodeTextEdit

CodeTextEdit::CodeTextEdit(QWidget* parent) : QPlainTextEdit(parent) {
    setObjectName(QStringLiteral("nbCode"));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // the notebook scrolls, not the cell
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTabChangesFocus(false);
    setCenterOnScroll(false);
    document()->setDocumentMargin(4);
    setAccessibleName(tr("Cell source"));
    setAccessibleDescription(tr("Python source. Ctrl+Enter runs the cell, Shift+Enter runs it and moves to the next, "
                                "Esc leaves the editor."));

    line_area_ = new NbLineNumberArea(this);
    highlighter_ = new NbHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) {
        update_gutter_width();
        fit_height();
    });
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeTextEdit::update_gutter_area);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeTextEdit::update_extra_selections);
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) { fit_height(); });
    update_gutter_width();

    completion_model_ = new QStringListModel(this);
    completer_ = new QCompleter(completion_model_, this);
    completer_->setWidget(this);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setCaseSensitivity(Qt::CaseSensitive);
    completer_->setMaxVisibleItems(12);
    completer_->popup()->setObjectName(QStringLiteral("nbList"));
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            &CodeTextEdit::insert_completion);
}

void CodeTextEdit::set_markdown_mode(bool markdown) {
    markdown_ = markdown;
    highlighter_->set_markdown(markdown);
    setAccessibleName(markdown ? tr("Markdown source") : tr("Cell source"));
}

void CodeTextEdit::set_line_numbers_visible(bool on) {
    if (line_numbers_ == on)
        return;
    line_numbers_ = on;
    line_area_->setVisible(on);
    update_gutter_width();
}

void CodeTextEdit::refresh_theme() {
    highlighter_->refresh();
    update_extra_selections();
    line_area_->update();
}

int CodeTextEdit::line_number_area_width() const {
    if (!line_numbers_)
        return 0;
    int digits = 1;
    for (int n = std::max(1, blockCount()); n >= 10; n /= 10)
        ++digits;
    digits = std::max(digits, 2);
    return fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 14;
}

void CodeTextEdit::update_gutter_width() {
    setViewportMargins(line_number_area_width(), 0, 0, 0);
}

void CodeTextEdit::update_gutter_area(const QRect& rect, int dy) {
    if (dy)
        line_area_->scroll(0, dy);
    else
        line_area_->update(0, rect.y(), line_area_->width(), rect.height());
}

void CodeTextEdit::resizeEvent(QResizeEvent* e) {
    QPlainTextEdit::resizeEvent(e);
    const QRect cr = contentsRect();
    line_area_->setGeometry(QRect(cr.left(), cr.top(), line_number_area_width(), cr.height()));
}

void CodeTextEdit::paint_line_numbers(QPaintEvent* event) {
    QPainter p(line_area_);
    p.fillRect(event->rect(), QColor(colors::BG_SURFACE()));
    p.setPen(QColor(colors::BORDER_DIM()));
    p.drawLine(line_area_->width() - 1, event->rect().top(), line_area_->width() - 1, event->rect().bottom());
    p.setFont(font());
    const int current = textCursor().blockNumber();
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int w = line_area_->width() - 7;
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const bool err = error_line_ == number + 1;
            QFont f = font();
            f.setBold(err);
            p.setFont(f);
            p.setPen(QColor(err ? colors::DANGER() : number == current && hasFocus() ? colors::TEXT_SECONDARY()
                                                                                       : colors::TEXT_TERTIARY()));
            p.drawText(0, top, w, fontMetrics().height(), Qt::AlignRight | Qt::AlignVCenter, QString::number(number + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++number;
    }
}

void CodeTextEdit::fit_height() {
    const int lines = std::max(1, blockCount());
    qreal per_line = blockBoundingRect(document()->firstBlock()).height();
    if (per_line <= 0)
        per_line = fontMetrics().lineSpacing();
    int h = qCeil(lines * per_line) + 2 * static_cast<int>(document()->documentMargin()) + 2 * frameWidth() + 2;
    if (horizontalScrollBar()->maximum() > 0)
        h += horizontalScrollBar()->sizeHint().height();
    if (height() != h)
        setFixedHeight(h);
}

void CodeTextEdit::focusInEvent(QFocusEvent* e) {
    QPlainTextEdit::focusInEvent(e);
    update_extra_selections();
    emit focused();
}

void CodeTextEdit::focusOutEvent(QFocusEvent* e) {
    QPlainTextEdit::focusOutEvent(e);
    update_extra_selections();
}

void CodeTextEdit::changeEvent(QEvent* e) {
    QPlainTextEdit::changeEvent(e);
    // The stylesheet font arrives at polish (and with every font-size change):
    // the height and the line-number width were measured with the old one.
    if (e->type() == QEvent::FontChange || e->type() == QEvent::StyleChange) {
        update_gutter_width();
        fit_height();
    }
}

void CodeTextEdit::set_error_line(int line) {
    if (error_line_ == line)
        return;
    error_line_ = line;
    update_extra_selections();
    line_area_->update();
}

void CodeTextEdit::set_search_ranges(const QVector<QPair<int, int>>& ranges) {
    if (search_ranges_.isEmpty() && ranges.isEmpty())
        return;
    search_ranges_ = ranges;
    update_extra_selections();
}

void CodeTextEdit::go_to_line(int line) {
    const QTextBlock block = document()->findBlockByNumber(std::max(0, line - 1));
    if (!block.isValid())
        return;
    QTextCursor c(block);
    setTextCursor(c);
    setFocus(Qt::OtherFocusReason);
}

void CodeTextEdit::update_extra_selections() {
    QList<QTextEdit::ExtraSelection> sels;
    if (hasFocus()) {
        QTextEdit::ExtraSelection line;
        line.format.setBackground(colors::overlay_color(kNbCurrentLineAlpha));
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        sels << line;
    }
    if (error_line_ > 0) {
        const QTextBlock block = document()->findBlockByNumber(error_line_ - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection err;
            err.format.setBackground(with_alpha(QColor(colors::DANGER()), kNbErrorLineAlpha));
            err.format.setProperty(QTextFormat::FullWidthSelection, true);
            err.cursor = QTextCursor(block);
            sels << err;
        }
    }
    for (const auto& [start, length] : search_ranges_) {
        QTextEdit::ExtraSelection m;
        m.format.setBackground(with_alpha(QColor(colors::AMBER()), kNbMatchAlpha));
        m.cursor = QTextCursor(document());
        m.cursor.setPosition(start);
        m.cursor.setPosition(start + length, QTextCursor::KeepAnchor);
        sels << m;
    }
    // Bracket matching around the cursor.
    if (!markdown_) {
        const QString text = toPlainText();
        const int pos = textCursor().position();
        static const QString open = QStringLiteral("([{");
        static const QString close = QStringLiteral(")]}");
        int at = -1;
        if (pos > 0 && (open.contains(text.at(pos - 1)) || close.contains(text.at(pos - 1))))
            at = pos - 1;
        else if (pos < text.size() && (open.contains(text.at(pos)) || close.contains(text.at(pos))))
            at = pos;
        if (at >= 0) {
            const QChar ch = text.at(at);
            const bool forward = open.contains(ch);
            const QChar mate = forward ? close.at(open.indexOf(ch)) : open.at(close.indexOf(ch));
            int depth = 0;
            int match = -1;
            for (int i = at; forward ? i < text.size() : i >= 0; i += forward ? 1 : -1) {
                if (text.at(i) == ch)
                    ++depth;
                else if (text.at(i) == mate && --depth == 0) {
                    match = i;
                    break;
                }
            }
            const QColor tone = match >= 0 ? with_alpha(QColor(colors::AMBER()), 0.35)
                                           : with_alpha(QColor(colors::DANGER()), 0.35);
            for (const int p : {at, match}) {
                if (p < 0)
                    continue;
                QTextEdit::ExtraSelection b;
                b.format.setBackground(tone);
                b.cursor = QTextCursor(document());
                b.cursor.setPosition(p);
                b.cursor.setPosition(p + 1, QTextCursor::KeepAnchor);
                sels << b;
            }
        }
    }
    setExtraSelections(sels);
    line_area_->update();
}

QString CodeTextEdit::symbol_at_cursor() const {
    const QTextCursor c = textCursor();
    const QString line = c.block().text();
    int pos = c.positionInBlock();
    auto is_sym = [](QChar ch) { return ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('.'); };
    int start = pos;
    while (start > 0 && is_sym(line.at(start - 1)))
        --start;
    int end = pos;
    while (end < line.size() && (line.at(end).isLetterOrNumber() || line.at(end) == QLatin1Char('_')))
        ++end;
    QString sym = line.mid(start, end - start);
    while (sym.startsWith(QLatin1Char('.')))
        sym.remove(0, 1);
    while (sym.endsWith(QLatin1Char('.')))
        sym.chop(1);
    if (sym.isEmpty() || sym.at(0).isDigit())
        return {};
    return sym;
}

QString CodeTextEdit::completion_prefix() const {
    const QTextCursor c = textCursor();
    const QString before = c.block().text().left(c.positionInBlock());
    static const QRegularExpression re(QStringLiteral("[A-Za-z_][A-Za-z0-9_]*(\\.[A-Za-z_]?[A-Za-z0-9_]*)*$"));
    const auto m = re.match(before);
    return m.hasMatch() ? m.captured(0) : QString();
}

bool CodeTextEdit::completion_visible() const {
    return completer_->popup()->isVisible();
}

void CodeTextEdit::show_completions(const QString& prefix, const QStringList& matches) {
    if (prefix != completion_prefix() || matches.isEmpty() || !hasFocus())
        return;
    const QString tail = prefix.mid(prefix.lastIndexOf(QLatin1Char('.')) + 1);
    if (matches.size() == 1) {
        completion_prefix_ = tail;
        insert_completion(matches.first());
        return;
    }
    completion_prefix_ = tail;
    completion_model_->setStringList(matches);
    completer_->setCompletionPrefix(tail);
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
    QRect r = cursorRect();
    r.translate(viewportMargins().left(), 0);
    r.setWidth(completer_->popup()->sizeHintForColumn(0) + completer_->popup()->verticalScrollBar()->sizeHint().width() + 16);
    completer_->complete(r);
}

void CodeTextEdit::insert_completion(const QString& completion) {
    const QString tail = completer_->popup()->isVisible() ? completer_->completionPrefix() : completion_prefix_;
    if (!completion.startsWith(tail))
        return;
    QTextCursor c = textCursor();
    c.insertText(completion.mid(tail.size()));
    setTextCursor(c);
    completer_->popup()->hide();
}

void CodeTextEdit::indent_selection(bool dedent) {
    QTextCursor c = textCursor();
    const int start = c.selectionStart();
    const int end = c.selectionEnd();
    QTextBlock first = document()->findBlock(start);
    QTextBlock last = document()->findBlock(end);
    if (c.hasSelection() && end == last.position() && last != first)
        last = last.previous(); // a selection ending at a line start does not include that line
    c.beginEditBlock();
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        QTextCursor bc(b);
        if (dedent) {
            const QString t = b.text();
            int n = 0;
            while (n < kIndent && n < t.size() && t.at(n) == QLatin1Char(' '))
                ++n;
            if (n == 0 && !t.isEmpty() && t.at(0) == QLatin1Char('\t'))
                n = 1;
            bc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, n);
            bc.removeSelectedText();
        } else if (!(b.text().isEmpty() && first != last)) {
            bc.insertText(QString(kIndent, QLatin1Char(' ')));
        }
        if (b == last)
            break;
    }
    c.endEditBlock();
}

void CodeTextEdit::toggle_comment() {
    if (markdown_)
        return;
    QTextCursor c = textCursor();
    QTextBlock first = document()->findBlock(c.selectionStart());
    QTextBlock last = document()->findBlock(c.selectionEnd());
    if (c.hasSelection() && c.selectionEnd() == last.position() && last != first)
        last = last.previous();
    bool all_commented = true;
    int min_indent = INT_MAX;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const QString t = b.text();
        if (!t.trimmed().isEmpty()) {
            const int ind = static_cast<int>(t.size() - QString(t).remove(QRegularExpression(QStringLiteral("^\\s+"))).size());
            min_indent = std::min(min_indent, ind);
            if (!t.trimmed().startsWith(QLatin1Char('#')))
                all_commented = false;
        }
        if (b == last)
            break;
    }
    if (min_indent == INT_MAX)
        return;
    c.beginEditBlock();
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const QString t = b.text();
        if (!t.trimmed().isEmpty()) {
            QTextCursor bc(b);
            if (all_commented) {
                const int hash = static_cast<int>(t.indexOf(QLatin1Char('#')));
                bc.setPosition(b.position() + hash);
                const int n = (hash + 1 < t.size() && t.at(hash + 1) == QLatin1Char(' ')) ? 2 : 1;
                bc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, n);
                bc.removeSelectedText();
            } else {
                bc.setPosition(b.position() + min_indent);
                bc.insertText(QStringLiteral("# "));
            }
        }
        if (b == last)
            break;
    }
    c.endEditBlock();
}

void CodeTextEdit::insert_newline_with_indent() {
    QTextCursor c = textCursor();
    const QString line = c.block().text();
    const QString before = line.left(c.positionInBlock());
    int indent = 0;
    while (indent < line.size() && line.at(indent) == QLatin1Char(' '))
        ++indent;
    indent = std::min(indent, static_cast<int>(before.size()));
    if (!markdown_) {
        const QString stripped = before.trimmed();
        static const QRegularExpression ends_block(QStringLiteral("^(return|pass|break|continue|raise)\\b"));
        if (stripped.endsWith(QLatin1Char(':')) && !stripped.startsWith(QLatin1Char('#')))
            indent += kIndent;
        else if (ends_block.match(stripped).hasMatch())
            indent = std::max(0, indent - kIndent);
    }
    c.insertText(QLatin1Char('\n') + QString(indent, QLatin1Char(' ')));
    setTextCursor(c);
    ensureCursorVisible();
}

bool CodeTextEdit::smart_backspace() {
    QTextCursor c = textCursor();
    if (c.hasSelection())
        return false;
    const int col = c.positionInBlock();
    if (col == 0)
        return false;
    const QString before = c.block().text().left(col);
    if (!before.trimmed().isEmpty())
        return false;
    const int n = col % kIndent == 0 ? kIndent : col % kIndent;
    c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, std::min(n, col));
    c.removeSelectedText();
    return true;
}

void CodeTextEdit::keyPressEvent(QKeyEvent* e) {
    if (completer_->popup()->isVisible()) {
        switch (e->key()) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                e->ignore(); // the completer takes these
                return;
            default:
                break;
        }
    }
    auto& km = KeyConfigManager::instance();
    const QKeySequence pressed(e->keyCombination());
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    const bool enter = e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter;

    if (pressed == km.key(KeyAction::RunCell)) {
        emit run_key(RunKey::Run);
        return;
    }
    if (pressed == km.key(KeyAction::RunAndNext)) {
        emit run_key(RunKey::RunAdvance);
        return;
    }
    if (enter && mods == Qt::AltModifier) {
        emit run_key(RunKey::RunInsert);
        return;
    }
    if (e->key() == Qt::Key_Escape && mods == Qt::NoModifier) {
        emit escape_pressed();
        return;
    }
    if (e->key() == Qt::Key_Space && mods == Qt::ControlModifier) {
        const QString prefix = completion_prefix();
        if (!prefix.isEmpty())
            emit completion_requested(prefix);
        return;
    }
    if (e->key() == Qt::Key_I && mods == Qt::ControlModifier) {
        emit inspect_requested(symbol_at_cursor());
        return;
    }
    if (e->key() == Qt::Key_Slash && mods == Qt::ControlModifier) {
        toggle_comment();
        return;
    }
    if (e->key() == Qt::Key_Tab && mods == Qt::NoModifier) {
        QTextCursor c = textCursor();
        if (c.hasSelection() && document()->findBlock(c.selectionStart()) != document()->findBlock(c.selectionEnd())) {
            indent_selection(false);
            return;
        }
        const QString prefix = markdown_ ? QString() : completion_prefix();
        if (!prefix.isEmpty()) {
            emit completion_requested(prefix);
            return;
        }
        const int col = c.positionInBlock();
        c.insertText(QString(kIndent - col % kIndent, QLatin1Char(' ')));
        return;
    }
    if (e->key() == Qt::Key_Backtab) {
        indent_selection(true);
        return;
    }
    if (enter && mods == Qt::NoModifier) {
        insert_newline_with_indent();
        return;
    }
    if (e->key() == Qt::Key_Backspace && mods == Qt::NoModifier && smart_backspace())
        return;
    if (e->key() == Qt::Key_Up && mods == Qt::NoModifier && !textCursor().hasSelection() &&
        textCursor().blockNumber() == 0) {
        emit leave_edge(-1);
        return;
    }
    if (e->key() == Qt::Key_Down && mods == Qt::NoModifier && !textCursor().hasSelection() &&
        textCursor().blockNumber() == blockCount() - 1) {
        emit leave_edge(1);
        return;
    }
    QPlainTextEdit::keyPressEvent(e);
    if (completer_->popup()->isVisible()) {
        const QString prefix = completion_prefix();
        const QString tail = prefix.mid(prefix.lastIndexOf(QLatin1Char('.')) + 1);
        if (prefix.isEmpty() || prefix.endsWith(QLatin1Char('.'))) {
            completer_->popup()->hide();
        } else {
            completer_->setCompletionPrefix(tail);
            completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// CellWidget

CellWidget::CellWidget(const NotebookCell& cell, QWidget* parent)
    : QFrame(parent), id_(cell.id), type_(cell.type), title_(cell.title), exec_count_(cell.execution_count) {
    build();
    editor_->blockSignals(true);
    editor_->setPlainText(cell.source);
    editor_->blockSignals(false);
    editor_->document()->setModified(false);
    set_type(cell.type);
    if (cell.type != NotebookCell::Type::Code)
        set_markdown_editing(cell.source.trimmed().isEmpty());
    output_collapsed_ = cell.output_collapsed;
    input_collapsed_ = cell.input_collapsed;
    set_outputs(cell.outputs);
    for (const CellOutput& o : cell.outputs)
        if (o.kind == CellOutput::Kind::Error)
            editor_->set_error_line(o.error_line);
    update_folds();
    update_gutter();
    retranslate();
}

void CellWidget::build() {
    setObjectName(QStringLiteral("nbCell"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 4, 10, 4);
    row->setSpacing(6);

    gutter_ = new QWidget(this);
    auto* g = new QVBoxLayout(gutter_);
    g->setContentsMargins(4, 5, 2, 0);
    g->setSpacing(3);
    count_ = new QLabel(gutter_);
    count_->setObjectName(QStringLiteral("nbCount"));
    count_->setAlignment(Qt::AlignRight | Qt::AlignTop);
    g->addWidget(count_);
    state_ = new QLabel(gutter_);
    state_->setObjectName(QStringLiteral("nbRunState"));
    state_->setAlignment(Qt::AlignRight | Qt::AlignTop);
    g->addWidget(state_);
    g->addStretch(1);
    gutter_->installEventFilter(this);
    row->addWidget(gutter_);

    body_ = new QWidget(this);
    body_layout_ = new QVBoxLayout(body_);
    body_layout_->setContentsMargins(0, 0, 0, 0);
    body_layout_->setSpacing(3);

    input_fold_ = new QPushButton(body_);
    input_fold_->setObjectName(QStringLiteral("nbFolded"));
    input_fold_->setCursor(Qt::PointingHandCursor);
    input_fold_->setVisible(false);
    connect(input_fold_, &QPushButton::clicked, this, [this]() { emit action(id_, Action::ToggleInput); });
    body_layout_->addWidget(input_fold_);

    editor_ = new CodeTextEdit(body_);
    body_layout_->addWidget(editor_);

    markdown_ = new QTextBrowser(body_);
    markdown_->setObjectName(QStringLiteral("nbMarkdown"));
    markdown_->setOpenExternalLinks(true);
    markdown_->setFrameShape(QFrame::NoFrame);
    markdown_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    markdown_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    markdown_->setVisible(false);
    markdown_->viewport()->installEventFilter(this);
    markdown_->setAccessibleName(tr("Rendered markdown — double-click or press Enter to edit"));
    body_layout_->addWidget(markdown_);

    outputs_ = new QWidget(body_);
    outputs_->setObjectName(QStringLiteral("nbOutputs"));
    outputs_layout_ = new QVBoxLayout(outputs_);
    outputs_layout_->setContentsMargins(0, 2, 0, 0);
    outputs_layout_->setSpacing(2);
    outputs_->setVisible(false);
    outputs_->installEventFilter(this);
    body_layout_->addWidget(outputs_);

    output_fold_ = new QPushButton(body_);
    output_fold_->setObjectName(QStringLiteral("nbFolded"));
    output_fold_->setCursor(Qt::PointingHandCursor);
    output_fold_->setVisible(false);
    connect(output_fold_, &QPushButton::clicked, this, [this]() { emit action(id_, Action::ToggleOutput); });
    body_layout_->addWidget(output_fold_);
    row->addWidget(body_, 1);

    // Tool bar over the top-right corner (not in the layout: no shift on hover).
    bar_ = new QFrame(this);
    bar_->setObjectName(QStringLiteral("nbCellBar"));
    auto* b = new QHBoxLayout(bar_);
    b->setContentsMargins(1, 1, 1, 1);
    b->setSpacing(0);
    auto make = [&](NbGlyph glyph, Action act, NbTone tone = NbTone::Normal) {
        auto* btn = new QToolButton(bar_);
        btn->setObjectName(QStringLiteral("nbBtn"));
        btn->setIcon(nb_icon(glyph, tone));
        btn->setIconSize(QSize(13, 13));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        connect(btn, &QToolButton::clicked, this, [this, act, btn]() {
            if (act == Action::Menu)
                emit menu_requested(id_, btn->mapToGlobal(QPoint(0, btn->height())));
            else
                emit action(id_, act);
        });
        b->addWidget(btn);
        return btn;
    };
    bar_run_ = make(NbGlyph::Play, Action::Run, NbTone::Accent);
    bar_run_->setProperty("fincept_agent_risk", "code");
    bar_up_ = make(NbGlyph::Up, Action::MoveUp);
    bar_down_ = make(NbGlyph::Down, Action::MoveDown);
    bar_type_ = make(NbGlyph::Text, Action::ToggleType);
    bar_delete_ = make(NbGlyph::Trash, Action::Delete);
    bar_more_ = make(NbGlyph::Dots, Action::Menu);
    bar_->setVisible(false);

    connect(editor_, &CodeTextEdit::run_key, this, [this](CodeTextEdit::RunKey k) { emit run_key(id_, k); });
    connect(editor_, &CodeTextEdit::escape_pressed, this, [this]() { emit escape_pressed(id_); });
    connect(editor_, &CodeTextEdit::leave_edge, this, [this](int d) { emit leave_edge(id_, d); });
    connect(editor_, &CodeTextEdit::completion_requested, this,
            [this](const QString& p) { emit completion_requested(id_, p); });
    connect(editor_, &CodeTextEdit::inspect_requested, this, [this](const QString& s) { emit inspect_requested(id_, s); });
    connect(editor_, &CodeTextEdit::focused, this, [this]() { emit activated(id_, true); });
    connect(editor_, &QPlainTextEdit::textChanged, this, [this]() {
        editor_->set_error_line(0); // the failing line no longer matches the source
        emit edited(id_);
    });
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        if (editor_->hasFocus()) {
            const QTextCursor c = editor_->textCursor();
            emit cursor_moved(id_, c.blockNumber() + 1, c.positionInBlock() + 1);
        }
    });
    apply_metrics();
}

QString CellWidget::source() const {
    return editor_->toPlainText();
}

void CellWidget::set_source(const QString& source) {
    if (editor_->toPlainText() == source)
        return;
    editor_->setPlainText(source);
    if (type_ != NotebookCell::Type::Code && !md_editing_)
        render_markdown();
}

void CellWidget::set_title(const QString& title) {
    title_ = title;
    update_gutter();
}

void CellWidget::set_type(NotebookCell::Type type) {
    type_ = type;
    const bool code = type == NotebookCell::Type::Code;
    editor_->set_markdown_mode(!code);
    if (code) {
        md_editing_ = false;
        markdown_->setVisible(false);
        editor_->setVisible(!input_collapsed_);
    }
    bar_run_->setVisible(true);
    bar_type_->setIcon(nb_icon(code ? NbGlyph::Text : NbGlyph::Code));
    update_gutter();
    retranslate();
}

void CellWidget::set_selection(Selection s) {
    if (selection_ == s)
        return;
    selection_ = s;
    const QString v = s == Selection::Command ? QStringLiteral("cmd") : s == Selection::Edit ? QStringLiteral("edit")
                                                                                            : QString();
    // Rules keyed on the cell's property style its count / editor / markdown too:
    // re-polish exactly those, not the whole subtree.
    nb_set_prop(this, "sel", v);
    for (QWidget* w : {static_cast<QWidget*>(count_), static_cast<QWidget*>(editor_), static_cast<QWidget*>(markdown_)}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
    update_bar_visibility();
}

void CellWidget::focus_editor(bool to_end) {
    if (input_collapsed_)
        set_input_collapsed(false);
    if (type_ != NotebookCell::Type::Code && !md_editing_)
        set_markdown_editing(true);
    if (to_end) {
        QTextCursor c = editor_->textCursor();
        c.movePosition(QTextCursor::End);
        editor_->setTextCursor(c);
    }
    editor_->setFocus(Qt::OtherFocusReason);
}

void CellWidget::set_exec_count(int count) {
    exec_count_ = count;
    update_gutter();
}

void CellWidget::set_run_state(RunState state, qint64 elapsed_ms) {
    run_state_ = state;
    if (elapsed_ms >= 0)
        last_elapsed_ms_ = elapsed_ms;
    if (state == RunState::Idle || state == RunState::Queued)
        last_elapsed_ms_ = state == RunState::Idle ? last_elapsed_ms_ : -1;
    update_gutter();
}

void CellWidget::tick(qint64 elapsed_ms) {
    if (run_state_ != RunState::Running)
        return;
    last_elapsed_ms_ = elapsed_ms;
    const QString t = QStringLiteral("▶ ") + format_elapsed(elapsed_ms);
    if (state_->text() != t)
        state_->setText(t);
}

void CellWidget::update_gutter() {
    QString count;
    if (type_ == NotebookCell::Type::Markdown)
        count = QStringLiteral("MD");
    else if (type_ == NotebookCell::Type::Raw)
        count = QStringLiteral("RAW");
    else if (run_state_ == RunState::Running)
        count = QStringLiteral("[*]");
    else if (exec_count_ > 0)
        count = QStringLiteral("[%1]").arg(exec_count_);
    else
        count = QStringLiteral("[ ]");
    if (count_->text() != count)
        count_->setText(count);
    count_->setToolTip(type_ == NotebookCell::Type::Code
                           ? (exec_count_ > 0 ? tr("Run #%1 of this kernel session").arg(exec_count_)
                                              : tr("Not run in this session"))
                           : tr("Markdown cell"));

    QString text;
    QString tone = QStringLiteral("dim");
    QString tip;
    switch (run_state_) {
        case RunState::Idle:
            if (last_elapsed_ms_ >= 0 && exec_count_ > 0)
                text = QStringLiteral("✓ ") + format_elapsed(last_elapsed_ms_);
            break;
        case RunState::Queued:
            text = tr("QUEUED");
            tone = QStringLiteral("warn");
            tip = tr("Waiting for the cells before it to finish");
            break;
        case RunState::Running:
            text = QStringLiteral("▶ ") + format_elapsed(std::max<qint64>(0, last_elapsed_ms_));
            tone = QStringLiteral("warn");
            tip = tr("Running — Esc then I, I (or the stop button) interrupts it");
            break;
        case RunState::Done:
            text = QStringLiteral("✓ ") + format_elapsed(last_elapsed_ms_);
            tip = tr("Finished in %1").arg(format_elapsed(last_elapsed_ms_));
            break;
        case RunState::Error:
            text = tr("✕ ERROR");
            tone = QStringLiteral("danger");
            tip = tr("Failed after %1").arg(format_elapsed(last_elapsed_ms_));
            break;
        case RunState::Cancelled:
            text = tr("■ STOPPED");
            tone = QStringLiteral("warn");
            tip = tr("Interrupted after %1").arg(format_elapsed(last_elapsed_ms_));
            break;
    }
    if (state_->text() != text)
        state_->setText(text);
    state_->setToolTip(tip);
    nb_set_prop(state_, "tone", tone);
    setToolTip(title_.isEmpty() ? QString() : title_);
}

QWidget* CellWidget::make_output(const CellOutput& out, int index) {
    NbOutputView* view = make_output_view(out, false, outputs_);
    connect(view, &NbOutputView::expand_requested, this, [this, index]() { emit output_expand_requested(id_, index); });
    connect(view, &NbOutputView::jump_to_line, this, [this](int line) {
        focus_editor();
        editor_->go_to_line(line);
    });
    view->installEventFilter(this);
    for (QWidget* w : view->findChildren<QWidget*>())
        w->installEventFilter(this);
    return view;
}

void CellWidget::set_outputs(const QVector<CellOutput>& outputs) {
    for (QWidget* w : output_widgets_) {
        if (!w)
            continue;
        outputs_layout_->removeWidget(w);
        w->deleteLater();
    }
    output_widgets_.clear();
    output_kinds_.clear();
    for (int i = 0; i < outputs.size(); ++i) {
        QWidget* w = make_output(outputs[i], i);
        outputs_layout_->addWidget(w);
        output_widgets_.append(w);
        output_kinds_.append(outputs[i].kind);
    }
    output_count_ = static_cast<int>(outputs.size());
    editor_->set_error_line(0);
    for (const CellOutput& o : outputs)
        if (o.kind == CellOutput::Kind::Error && o.error_line > 0)
            editor_->set_error_line(o.error_line);
    update_folds();
}

void CellWidget::apply_output(int index, const CellOutput& output) {
    if (index < 0)
        return;
    if (index < output_widgets_.size()) {
        if (output_kinds_[index] == output.kind) {
            if (auto* v = qobject_cast<NbOutputView*>(output_widgets_[index]))
                v->update_output(output);
        } else {
            // A repr that turned into a table: replace the widget in place.
            QWidget* old = output_widgets_[index];
            QWidget* w = make_output(output, index);
            outputs_layout_->replaceWidget(old, w);
            old->deleteLater();
            output_widgets_[index] = w;
            output_kinds_[index] = output.kind;
        }
    } else {
        while (output_widgets_.size() < index) { // never expected: keep indices aligned
            output_widgets_.append(nullptr);
            output_kinds_.append(CellOutput::Kind::Stream);
        }
        QWidget* w = make_output(output, index);
        outputs_layout_->addWidget(w);
        output_widgets_.append(w);
        output_kinds_.append(output.kind);
    }
    output_count_ = static_cast<int>(output_widgets_.size());
    if (output.kind == CellOutput::Kind::Error && output.error_line > 0)
        editor_->set_error_line(output.error_line);
    update_folds();
}

void CellWidget::set_output_collapsed(bool collapsed) {
    output_collapsed_ = collapsed;
    update_folds();
}

void CellWidget::set_input_collapsed(bool collapsed) {
    input_collapsed_ = collapsed;
    update_folds();
}

void CellWidget::update_folds() {
    const bool code = type_ == NotebookCell::Type::Code;
    const bool show_editor = !input_collapsed_ && (code || md_editing_);
    editor_->setVisible(show_editor);
    markdown_->setVisible(!input_collapsed_ && !code && !md_editing_);
    input_fold_->setVisible(input_collapsed_);
    if (input_collapsed_) {
        NotebookCell probe;
        probe.type = type_;
        probe.source = source();
        QString first = notebook::cell_display_name(probe);
        if (first.size() > 80)
            first = first.left(79) + QStringLiteral("…");
        input_fold_->setText(tr("▸  %1 hidden — %2").arg(code ? tr("Code") : tr("Text"), first));
    }
    outputs_->setVisible(code && output_count_ > 0 && !output_collapsed_);
    output_fold_->setVisible(code && output_count_ > 0 && output_collapsed_);
    if (output_fold_->isVisible())
        output_fold_->setText(tr("▸  Output hidden — %n item(s). Click to show.", "", output_count_));
}

void CellWidget::set_markdown_editing(bool editing) {
    if (type_ == NotebookCell::Type::Code)
        return;
    md_editing_ = editing;
    if (!editing)
        render_markdown();
    update_folds();
    if (editing)
        editor_->fit_height();
}

namespace {
/// Rendered text cells are exactly as tall as their text at the width they are shown at.
/// `width` is the viewport's width once the cell has been laid out; before that (a cell is
/// rendered when it is created, when the browser still has its default 100 px size) the
/// caller passes the body's width, and the viewport's Resize event fits it again.
void nb_fit_markdown_height(QTextBrowser* md, int width) {
    if (!md || width <= 50)
        return;
    QTextDocument* doc = md->document();
    if (qAbs(doc->textWidth() - width) > 0.5)
        doc->setTextWidth(width);
    const int h = std::max(static_cast<int>(std::ceil(doc->size().height())) + 6,
                           QFontMetrics(ui::fonts::font()).lineSpacing() + 12);
    if (md->minimumHeight() != h || md->maximumHeight() != h)
        md->setFixedHeight(h);
}
} // namespace

void CellWidget::render_markdown() {
    const QString src = source();
    QString html;
    if (src.trimmed().isEmpty())
        html = QStringLiteral("<p style=\"color:%1;\"><i>%2</i></p>")
                   .arg(colors::TEXT_TERTIARY(), tr("Empty text cell — double-click or press Enter to write").toHtmlEscaped());
    else if (type_ == NotebookCell::Type::Raw)
        html = QStringLiteral("<pre>%1</pre>").arg(src.toHtmlEscaped());
    else
        html = notebook::markdown_to_html(src, nb_html_palette());
    markdown_->setHtml(html);
    // Not laid out yet (or hidden): the viewport still has a placeholder width, which would
    // wrap the text into a narrow column and leave a tall blank cell — measure against the
    // body instead. The viewport's Resize event (eventFilter) fits the final width.
    const bool laid_out = markdown_->isVisible() && markdown_->viewport()->width() > 50 &&
                          markdown_->viewport()->width() >= body_->width() / 2;
    nb_fit_markdown_height(markdown_, laid_out ? markdown_->viewport()->width() : std::max(200, body_->width() - 8));
}

void CellWidget::set_line_numbers_visible(bool on) {
    editor_->set_line_numbers_visible(on);
}

void CellWidget::apply_metrics() {
    const QFontMetrics count_fm(ui::fonts::font(-3, QFont::Bold));
    const QFontMetrics state_fm(ui::fonts::font(-4, QFont::DemiBold));
    const int w = std::max(count_fm.horizontalAdvance(QStringLiteral("[888]")),
                           state_fm.horizontalAdvance(QStringLiteral("✕ ERROR 88"))) +
                  10;
    gutter_->setFixedWidth(w);
    const int btn = std::max(22, QFontMetrics(ui::fonts::font(-3)).height() + 8);
    for (QToolButton* b : {bar_run_, bar_up_, bar_down_, bar_type_, bar_delete_, bar_more_})
        b->setFixedSize(btn, btn);
}

void CellWidget::refresh_theme() {
    editor_->refresh_theme();
    if (type_ != NotebookCell::Type::Code && !md_editing_)
        render_markdown();
    for (QWidget* w : output_widgets_)
        if (auto* v = qobject_cast<NbOutputView*>(w))
            v->refresh_theme();
}

void CellWidget::refit() {
    apply_metrics();
    editor_->fit_height();
    if (markdown_->isVisibleTo(this))
        render_markdown();
    for (QWidget* w : output_widgets_)
        if (auto* v = qobject_cast<NbOutputView*>(w))
            v->refit();
    place_bar();
}

void CellWidget::update_bar_visibility() {
    const bool show = hovered_ || selection_ != Selection::None;
    if (bar_->isVisible() != show) {
        bar_->setVisible(show);
        if (show) {
            place_bar();
            bar_->raise();
        }
    }
}

void CellWidget::place_bar() {
    if (!bar_->isVisible())
        return;
    bar_->adjustSize();
    bar_->move(width() - bar_->width() - 14, 1);
}

void CellWidget::enterEvent(QEnterEvent* e) {
    hovered_ = true;
    update_bar_visibility();
    QFrame::enterEvent(e);
}

void CellWidget::leaveEvent(QEvent* e) {
    hovered_ = false;
    update_bar_visibility();
    QFrame::leaveEvent(e);
}

void CellWidget::mousePressEvent(QMouseEvent* e) {
    emit activated(id_, false);
    QFrame::mousePressEvent(e);
}

void CellWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    if (type_ != NotebookCell::Type::Code)
        emit activated(id_, true);
    QFrame::mouseDoubleClickEvent(e);
}

void CellWidget::resizeEvent(QResizeEvent* e) {
    QFrame::resizeEvent(e);
    place_bar();
    if (markdown_->isVisibleTo(this)) {
        nb_fit_markdown_height(markdown_, markdown_->viewport()->width());
    }
}

void CellWidget::contextMenuEvent(QContextMenuEvent* e) {
    emit menu_requested(id_, e->globalPos());
}

bool CellWidget::eventFilter(QObject* watched, QEvent* event) {
    const QEvent::Type t = event->type();
    if (t == QEvent::MouseButtonPress) {
        emit activated(id_, false); // a click on the outputs / gutter / rendered text selects the cell
    } else if (t == QEvent::MouseButtonDblClick && watched == markdown_->viewport()) {
        emit activated(id_, true);
        return true;
    } else if (t == QEvent::Resize && watched == markdown_->viewport()) {
        // The layout gives the browser its real width after the cell was rendered: fit the
        // height to the text at that width (a height-only change leaves the width, so no loop).
        nb_fit_markdown_height(markdown_, static_cast<QResizeEvent*>(event)->size().width());
    }
    return QFrame::eventFilter(watched, event);
}

void CellWidget::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QFrame::changeEvent(e);
}

void CellWidget::retranslate() {
    const bool code = type_ == NotebookCell::Type::Code;
    bar_run_->setToolTip(code ? tr("Run this cell (Ctrl+Enter)") : tr("Render this text cell (Ctrl+Enter)"));
    bar_up_->setToolTip(tr("Move up (Alt+Up)"));
    bar_down_->setToolTip(tr("Move down (Alt+Down)"));
    bar_type_->setToolTip(code ? tr("Turn into a text (markdown) cell (M)") : tr("Turn into a code cell (Y)"));
    bar_delete_->setToolTip(tr("Delete this cell (D, D) — undo from the status bar"));
    bar_more_->setToolTip(tr("More: run above / below, insert, collapse, rename, copy…"));
    for (QToolButton* b : {bar_run_, bar_up_, bar_down_, bar_type_, bar_delete_, bar_more_})
        b->setAccessibleName(b->toolTip());
    update_gutter();
    update_folds();
}

} // namespace fincept::screens
