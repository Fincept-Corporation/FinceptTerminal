// AgentTranscriptView.cpp — painted, height-cached transcript rows with tool
// cards (call + result in one row) and a live row for streamed text.

#include "screens/agent_config/AgentTranscriptView.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDialog>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextLayout>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace agent_transcript_detail {

constexpr int kPadV = 4;
constexpr int kGutterTime = 64;
constexpr int kGutterTag = 62;
constexpr int kRightPad = 10;
constexpr int kIndicatorW = 18;
constexpr int kMarkW = 64;         // "running…" / "ok" / "failed" on a tool card
constexpr int kExpandedMaxLines = 4000;
constexpr int kExpandedMaxChars = 60000;
constexpr int kJoinLookBack = 400; // a result is joined to a call at most this many rows back
constexpr int kCharsPerLine = 320;

QColor kind_color(const ca::AgentEvent& ev) {
    using namespace ui;
    if (ev.is_error)
        return QColor(colors::DANGER());
    switch (ev.kind) {
        case ca::EventKind::Text:
            return QColor(colors::TEXT_PRIMARY());
        case ca::EventKind::Thinking:
            return QColor(colors::TEXT_TERTIARY());
        case ca::EventKind::ToolCall:
            return QColor(colors::AMBER());
        case ca::EventKind::ToolResult:
            return QColor(colors::CYAN());
        case ca::EventKind::Command:
            return QColor(colors::INFO());
        case ca::EventKind::FileChange:
            return QColor(colors::SUCCESS());
        case ca::EventKind::Usage:
            return QColor(colors::TEXT_TERTIARY());
        case ca::EventKind::Error:
            return QColor(colors::DANGER());
        case ca::EventKind::Final:
            return QColor(colors::AMBER());
        case ca::EventKind::Status:
            break;
    }
    return QColor(colors::TEXT_SECONDARY());
}

/// A row that opens a call (a result can be joined to it).
bool is_call(const ca::AgentEvent& ev) {
    return ev.kind == ca::EventKind::ToolCall || ev.kind == ca::EventKind::Command;
}

/// The card's outcome: its joined result's, else the call's own error flag.
bool call_failed(const AgentTranscriptModel::Row& row) {
    return row.result ? row.result->is_error : row.ev.is_error;
}

} // namespace agent_transcript_detail

using namespace agent_transcript_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Model
// ═════════════════════════════════════════════════════════════════════════════

AgentTranscriptModel::AgentTranscriptModel(QObject* parent) : QAbstractListModel(parent) {}

int AgentTranscriptModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant AgentTranscriptModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        return {};
    if (role == Qt::DisplayRole)
        return rows_[index.row()].ev.title;
    return {};
}

const AgentTranscriptModel::Row* AgentTranscriptModel::row_at(int row) const {
    return (row >= 0 && row < rows_.size()) ? &rows_[row] : nullptr;
}

int AgentTranscriptModel::live_row() const {
    return (!rows_.isEmpty() && rows_.constLast().live) ? static_cast<int>(rows_.size()) - 1 : -1;
}

int AgentTranscriptModel::join_target(const ca::AgentEvent& ev) const {
    if (ev.ref_id.isEmpty())
        return -1;
    const bool result_like = ev.kind == ca::EventKind::ToolResult || ev.kind == ca::EventKind::Command;
    if (!result_like)
        return -1;
    const int last = static_cast<int>(rows_.size()) - 1;
    for (int i = last; i >= 0 && i >= last - kJoinLookBack; --i) {
        const Row& r = rows_[i];
        if (r.live || r.result || r.ev.ref_id != ev.ref_id)
            continue;
        // A tool result joins its tool call; a finished command joins its start.
        if ((ev.kind == ca::EventKind::ToolResult && r.ev.kind == ca::EventKind::ToolCall) ||
            (ev.kind == ca::EventKind::Command && r.ev.kind == ca::EventKind::Command))
            return i;
    }
    return -1;
}

void AgentTranscriptModel::reset_events(const QVector<ca::AgentEvent>& events) {
    beginResetModel();
    rows_.clear();
    max_seq_ = 0;
    const qsizetype from = qMax<qsizetype>(0, events.size() - kMaxRows);
    hidden_ = static_cast<int>(from);
    rows_.reserve(events.size() - from);
    for (qsizetype i = from; i < events.size(); ++i) {
        const ca::AgentEvent& e = events[i];
        if (e.partial)
            continue;
        max_seq_ = qMax(max_seq_, e.seq);
        const int target = join_target(e);
        if (target >= 0) {
            rows_[target].result = e;
            continue;
        }
        Row r;
        r.ev = e;
        rows_.append(r);
    }
    for (int i = 1; i < rows_.size(); ++i) {
        if (rows_[i].ev.kind == ca::EventKind::Final) {
            for (int j = i - 1; j >= qMax(0, i - 3); --j) {
                if (rows_[j].ev.kind == ca::EventKind::Text &&
                    rows_[j].ev.body.trimmed() == rows_[i].ev.body.trimmed()) {
                    rows_[j].superseded = true;
                    break;
                }
            }
        }
    }
    endResetModel();
}

void AgentTranscriptModel::remove_live() {
    const int lr = live_row();
    if (lr < 0)
        return;
    beginRemoveRows({}, lr, lr);
    rows_.remove(lr);
    endRemoveRows();
}

void AgentTranscriptModel::append_events(const QVector<ca::AgentEvent>& events) {
    QVector<ca::AgentEvent> fresh;
    fresh.reserve(events.size());
    for (const auto& e : events) {
        if (!e.partial && e.seq > max_seq_)
            fresh.append(e);
    }
    if (fresh.isEmpty())
        return;
    if (fresh.size() > kMaxRows) {
        hidden_ += static_cast<int>(fresh.size() - kMaxRows);
        fresh = fresh.mid(fresh.size() - kMaxRows);
    }
    for (const auto& e : fresh) {
        max_seq_ = qMax(max_seq_, e.seq);
        // The complete message replaces the streamed text.
        if (e.kind == ca::EventKind::Text || e.kind == ca::EventKind::Thinking || e.kind == ca::EventKind::Final)
            remove_live();
        const int target = join_target(e);
        if (target >= 0) {
            Row& r = rows_[target];
            r.result = e;
            r.cached_width = -1;
            const QModelIndex idx = index(target);
            emit dataChanged(idx, idx); // the view relayouts the row (see AgentTranscriptView)
            continue;
        }
        // Keep the live row last: insert before it.
        const int lr = live_row();
        const int at = lr >= 0 ? lr : static_cast<int>(rows_.size());
        beginInsertRows({}, at, at);
        Row r;
        r.ev = e;
        rows_.insert(at, r);
        endInsertRows();
        if (e.kind == ca::EventKind::Final)
            mark_superseded(e);
    }
    const qsizetype overflow = rows_.size() - kMaxRows;
    if (overflow > 0) {
        const int remove_n = static_cast<int>(overflow);
        beginRemoveRows({}, 0, remove_n - 1);
        rows_.remove(0, remove_n);
        endRemoveRows();
        hidden_ += remove_n;
    }
}

void AgentTranscriptModel::set_live(const ca::AgentEvent& partial) {
    if (!partial.partial)
        return;
    const int lr = live_row();
    if (lr >= 0) {
        Row& r = rows_[lr];
        r.ev = partial;
        r.cached_width = -1;
        const QModelIndex idx = index(lr);
        emit dataChanged(idx, idx);
        return;
    }
    const int at = static_cast<int>(rows_.size());
    beginInsertRows({}, at, at);
    Row r;
    r.ev = partial;
    r.live = true;
    rows_.append(r);
    endInsertRows();
}

void AgentTranscriptModel::mark_superseded(const ca::AgentEvent& final_ev) {
    // The CLIs report the final answer twice (last assistant message + result):
    // keep the RESULT row prominent and fold the duplicate to one line.
    int final_row = -1;
    for (int i = static_cast<int>(rows_.size()) - 1; i >= 0; --i) {
        if (!rows_[i].live && rows_[i].ev.seq == final_ev.seq) {
            final_row = i;
            break;
        }
    }
    const QString body = final_ev.body.trimmed();
    for (int j = final_row - 1; j >= qMax(0, final_row - 3); --j) {
        Row& r = rows_[j];
        if (r.ev.kind == ca::EventKind::Text && r.ev.body.trimmed() == body) {
            r.superseded = true;
            r.cached_width = -1;
            const QModelIndex idx = index(j);
            emit dataChanged(idx, idx);
            break;
        }
    }
}

void AgentTranscriptModel::clear_events() {
    beginResetModel();
    rows_.clear();
    hidden_ = 0;
    max_seq_ = 0;
    endResetModel();
}

void AgentTranscriptModel::toggle_expanded(int row) {
    if (row < 0 || row >= rows_.size())
        return;
    rows_[row].expanded = !rows_[row].expanded;
    rows_[row].cached_width = -1;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void AgentTranscriptModel::set_all_expanded(bool expanded) {
    emit layoutAboutToBeChanged();
    for (auto& r : rows_) {
        r.expanded = expanded;
        r.cached_width = -1;
    }
    emit layoutChanged();
}

void AgentTranscriptModel::invalidate_layout() {
    emit layoutAboutToBeChanged();
    for (auto& r : rows_)
        r.cached_width = -1;
    emit layoutChanged();
}

QString AgentTranscriptModel::event_text(const ca::AgentEvent& ev) {
    const QString when =
        ev.ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(ev.ts_ms).toString(QStringLiteral("HH:mm:ss")) : QString();
    QString out = QStringLiteral("[%1] %2%3  %4")
                      .arg(when, ca::event_kind_id(ev.kind).toUpper(), ev.is_error ? QStringLiteral(" (error)") : QString(),
                           ev.title);
    if (!ev.body.isEmpty())
        out += QLatin1Char('\n') + ev.body;
    return out;
}

QString AgentTranscriptModel::row_text(const Row& row) {
    QString out = event_text(row.ev);
    if (row.result)
        out += QStringLiteral("\n") + event_text(*row.result);
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// Delegate
// ═════════════════════════════════════════════════════════════════════════════

AgentTranscriptDelegate::AgentTranscriptDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    refresh_fonts();
}

void AgentTranscriptDelegate::refresh_fonts() {
    meta_font_ = ui::fonts::font(-3);
    tag_font_ = ui::fonts::font(-4, QFont::Bold);
    title_font_ = ui::fonts::font(-1, QFont::Bold);
    body_font_ = ui::fonts::font(-2);
    body_line_h_ = QFontMetrics(body_font_).lineSpacing();
    title_h_ = QFontMetrics(title_font_).lineSpacing() + 2;

    // The time and tag gutters grow with the font so large sizes do not clip.
    gutter_time_ =
        qMax(kGutterTime, QFontMetrics(meta_font_).horizontalAdvance(QStringLiteral("00:00:00")) + 18);
    const QFontMetrics tag_fm(tag_font_);
    int tag_w = 0;
    for (const QString& t : {tr("AGENT"), tr("THINK"), tr("TOOL"), tr("FAILED"), tr("RESULT"), tr("CMD"), tr("FILES"),
                             tr("USAGE"), tr("ERROR"), tr("ANSWER"), tr("WARN"), tr("INFO"), tr("LIVE")})
        tag_w = qMax(tag_w, tag_fm.horizontalAdvance(t));
    gutter_tag_ = qMax(kGutterTag, tag_w + 18);
}

QString AgentTranscriptDelegate::tag_text(const AgentTranscriptModel::Row& row) const {
    const ca::AgentEvent& ev = row.ev;
    if (row.live)
        return ev.kind == ca::EventKind::Thinking ? tr("THINK") : tr("LIVE");
    switch (ev.kind) {
        case ca::EventKind::Text:
            return tr("AGENT");
        case ca::EventKind::Thinking:
            return tr("THINK");
        case ca::EventKind::ToolCall:
            return call_failed(row) ? tr("FAILED") : tr("TOOL");
        case ca::EventKind::ToolResult:
            return ev.is_error ? tr("FAILED") : tr("RESULT");
        case ca::EventKind::Command:
            return tr("CMD");
        case ca::EventKind::FileChange:
            return tr("FILES");
        case ca::EventKind::Usage:
            return tr("USAGE");
        case ca::EventKind::Error:
            return tr("ERROR");
        case ca::EventKind::Final:
            return tr("ANSWER");
        case ca::EventKind::Status:
            break;
    }
    return ev.is_error ? tr("WARN") : tr("INFO");
}

int AgentTranscriptDelegate::collapsed_lines(const AgentTranscriptModel::Row& row) {
    if (row.superseded)
        return 1;
    if (row.live)
        return 14;
    switch (row.ev.kind) {
        case ca::EventKind::Text:
            return 14;
        case ca::EventKind::Final:
            return 40;
        case ca::EventKind::Thinking:
        case ca::EventKind::ToolCall:
            return 2;
        case ca::EventKind::ToolResult:
        case ca::EventKind::Command:
        case ca::EventKind::Status:
            return 3;
        case ca::EventKind::FileChange:
            return 6;
        case ca::EventKind::Usage:
            return 1;
        case ca::EventKind::Error:
            return 8;
    }
    return 3;
}

QString AgentTranscriptDelegate::prepared(const QString& text, int cap, bool tail, bool expanded) {
    // Bound the layout work: a collapsed row never needs more than its first (or,
    // for streaming text, last) few hundred characters per visible line.
    QString out = tail ? text.right(cap) : text.left(cap);
    out.replace(QLatin1Char('\t'), QStringLiteral("    "));
    out.replace(QLatin1Char('\r'), QString());
    out.replace(QLatin1Char('\n'), QChar::LineSeparator);
    if (text.size() > cap && expanded && !tail)
        out += QChar::LineSeparator + tr("… (%1 more characters — right-click › View full event)").arg(text.size() - cap);
    return out;
}

QString AgentTranscriptDelegate::part_a_text(const AgentTranscriptModel::Row& row) {
    // A command whose result arrived shows only its output (the start event's
    // body is a "(running)" placeholder; the title already holds the command).
    if (row.result && row.ev.kind == ca::EventKind::Command)
        return {};
    return row.ev.body;
}

int AgentTranscriptDelegate::lay_out(QTextLayout& layout, const QString& text, int width, int max_lines,
                                     bool* more) const {
    if (text.isEmpty() || max_lines <= 0)
        return 0;
    layout.setText(text);
    layout.setFont(body_font_);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    int lines = 0;
    qreal y = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        if (lines >= max_lines) {
            *more = true;
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += body_line_h_;
        ++lines;
    }
    layout.endLayout();
    return lines;
}

AgentTranscriptDelegate::BodyLayout AgentTranscriptDelegate::layout_body(QTextLayout& a, QTextLayout& b,
                                                                         const AgentTranscriptModel::Row& row,
                                                                         int width) const {
    BodyLayout out;
    if (width <= 20)
        return out;
    const bool expanded = row.expanded;
    const int lines_a = expanded ? kExpandedMaxLines : collapsed_lines(row);
    const int lines_b = expanded ? kExpandedMaxLines : 3;
    const int cap_a = expanded ? kExpandedMaxChars : lines_a * kCharsPerLine;
    const int cap_b = expanded ? kExpandedMaxChars : lines_b * kCharsPerLine;
    const QString text_a = part_a_text(row);
    out.lines_a = lay_out(a, prepared(text_a, cap_a, row.live, expanded), width, lines_a, &out.more);
    if (row.result && !row.result->body.isEmpty())
        out.lines_b = lay_out(b, prepared(row.result->body, cap_b, false, expanded), width, lines_b, &out.more);
    if (!expanded && (text_a.size() > cap_a || (row.result && row.result->body.size() > cap_b)))
        out.more = true;
    return out;
}

int AgentTranscriptDelegate::content_width(int row_width) const {
    return qMax(40, row_width - gutter_time_ - gutter_tag_ - kRightPad - 6);
}

QSize AgentTranscriptDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const auto* model = qobject_cast<const AgentTranscriptModel*>(index.model());
    const AgentTranscriptModel::Row* row = model ? model->row_at(index.row()) : nullptr;
    if (!row)
        return QSize(0, title_h_ + 2 * kPadV);
    int width = option.rect.width();
    if (const auto* view = qobject_cast<const QAbstractItemView*>(parent()))
        width = view->viewport()->width();
    if (row->cached_width != width) {
        QTextLayout a;
        QTextLayout b;
        const BodyLayout bl = layout_body(a, b, *row, content_width(width));
        row->cached_more = bl.more;
        int h = kPadV + title_h_;
        if (bl.lines_a > 0)
            h += 2 + bl.lines_a * body_line_h_;
        if (bl.lines_b > 0)
            h += 4 + bl.lines_b * body_line_h_;
        row->cached_height = h + kPadV + 1;
        row->cached_width = width;
    }
    return QSize(width, row->cached_height);
}

void AgentTranscriptDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const {
    const auto* model = qobject_cast<const AgentTranscriptModel*>(index.model());
    const AgentTranscriptModel::Row* row = model ? model->row_at(index.row()) : nullptr;
    if (!row)
        return;
    const ca::AgentEvent& ev = row->ev;
    // A call with its result joined is a card with an ok / failed mark. A call
    // still waiting shows no mark: some calls never get a result event (Codex
    // web_search), and "running…" there would never go away.
    const bool card = row->result.has_value() && is_call(ev);
    const QRect r = option.rect;
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    painter->fillRect(r, QColor(selected ? ui::colors::BG_HOVER() : (hovered ? ui::colors::BG_RAISED() : ui::colors::BG_BASE())));
    const bool failed = card ? call_failed(*row) : ev.is_error;
    if (ev.kind == ca::EventKind::Final)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::AMBER()));
    else if (failed)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::DANGER()));
    else if (row->live)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::INFO()));
    // Faint text steps up a level on the selection fill, where the dim tones lose contrast.
    const QColor faint(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY());

    const int top = r.top() + kPadV;
    // Time
    painter->setFont(meta_font_);
    painter->setPen(QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_DIM()));
    const QString when =
        ev.ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(ev.ts_ms).toString(QStringLiteral("HH:mm:ss")) : QString();
    painter->drawText(QRect(r.left() + 8, top, gutter_time_ - 10, title_h_), Qt::AlignLeft | Qt::AlignVCenter, when);

    // Kind tag
    QColor kc = kind_color(ev);
    if (card && failed)
        kc = QColor(ui::colors::DANGER());
    if (row->live)
        kc = QColor(ui::colors::INFO());
    const QRect tag(r.left() + gutter_time_, top + 2, gutter_tag_ - 10, title_h_ - 4);
    painter->setFont(tag_font_);
    if (ev.kind == ca::EventKind::Final) {
        painter->fillRect(tag, kc);
        painter->setPen(QColor(ui::colors::TEXT_ON_ACCENT()));
    } else {
        painter->setPen(kc);
        painter->drawRect(tag.adjusted(0, 0, -1, -1));
    }
    painter->drawText(tag, Qt::AlignCenter, tag_text(*row));

    // Title (+ a tool card's running / ok / failed mark)
    const int x = r.left() + gutter_time_ + gutter_tag_;
    const int mark_w =
        card ? qMax(kMarkW / 2, QFontMetrics(meta_font_).horizontalAdvance(tr("failed")) + 12) : 0; // grows with the font
    const int title_w = qMax(20, r.right() - kRightPad - kIndicatorW - mark_w - x);
    painter->setFont(title_font_);
    QColor title_color = ev.kind == ca::EventKind::Text || ev.kind == ca::EventKind::Final || row->live
                             ? QColor(ui::colors::TEXT_PRIMARY())
                             : kc;
    if (row->superseded)
        title_color = faint;
    painter->setPen(title_color);
    QString title_text = (row->result && ev.kind == ca::EventKind::Command) ? row->result->title : ev.title;
    if (row->live)
        title_text = (title_text.isEmpty() ? tr("Assistant") : title_text) + QStringLiteral(" · ") + tr("streaming…");
    const QString title = QFontMetrics(title_font_).elidedText(title_text, Qt::ElideRight, title_w);
    painter->drawText(QRect(x, top, title_w, title_h_), Qt::AlignLeft | Qt::AlignVCenter, title);
    if (card) {
        painter->setFont(meta_font_);
        QString mark;
        QColor mark_color;
        if (failed) {
            mark = tr("failed");
            mark_color = QColor(ui::colors::DANGER());
        } else {
            mark = tr("ok");
            mark_color = QColor(ui::colors::SUCCESS());
        }
        painter->setPen(mark_color);
        painter->drawText(QRect(r.right() - kRightPad - kIndicatorW - mark_w, top, mark_w - 4, title_h_),
                          Qt::AlignRight | Qt::AlignVCenter, mark);
    }

    // Expand indicator
    if (row->cached_more || row->expanded) {
        painter->setFont(meta_font_);
        painter->setPen(faint);
        painter->drawText(QRect(r.right() - kRightPad - kIndicatorW, top, kIndicatorW, title_h_),
                          Qt::AlignRight | Qt::AlignVCenter, row->expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
    }

    // Body: the event's own text, then a joined result.
    QTextLayout a;
    QTextLayout b;
    const BodyLayout bl = layout_body(a, b, *row, content_width(r.width()));
    int y = top + title_h_ + 2;
    if (bl.lines_a > 0) {
        QColor body_color = QColor(ui::colors::TEXT_SECONDARY());
        if (ev.kind == ca::EventKind::Text || ev.kind == ca::EventKind::Final || row->live)
            body_color = QColor(ui::colors::TEXT_PRIMARY());
        if (ev.kind == ca::EventKind::Thinking || row->superseded)
            body_color = faint;
        if (ev.is_error)
            body_color = QColor(ui::colors::DANGER());
        painter->setPen(body_color);
        painter->setFont(body_font_);
        const QPointF origin(x, y);
        for (int i = 0; i < bl.lines_a && i < a.lineCount(); ++i)
            a.lineAt(i).draw(painter, origin);
        y += bl.lines_a * body_line_h_;
    }
    if (bl.lines_b > 0 && row->result) {
        y += 4;
        const QColor rc = row->result->is_error ? QColor(ui::colors::DANGER()) : QColor(ui::colors::CYAN());
        painter->fillRect(QRect(x - 6, y, 2, bl.lines_b * body_line_h_), rc); // result rule
        painter->setPen(row->result->is_error ? QColor(ui::colors::DANGER()) : QColor(ui::colors::TEXT_SECONDARY()));
        painter->setFont(body_font_);
        const QPointF origin(x, y);
        for (int i = 0; i < bl.lines_b && i < b.lineCount(); ++i)
            b.lineAt(i).draw(painter, origin);
    }

    // Hairline separator
    painter->setPen(QColor(ui::colors::BORDER_DIM()));
    painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
    painter->restore();
}

// ═════════════════════════════════════════════════════════════════════════════
// View
// ═════════════════════════════════════════════════════════════════════════════

AgentTranscriptView::AgentTranscriptView(QWidget* parent) : QListView(parent) {
    setObjectName(QStringLiteral("awbTranscript"));
    setUniformItemSizes(false);
    setResizeMode(QListView::Adjust); // heights depend on width — relayout on resize
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Row heights depend on the viewport width: a scrollbar that toggles on and off
    // would change the width and re-trigger layout, so keep it permanently on.
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    verticalScrollBar()->setSingleStep(18);

    delegate_ = new AgentTranscriptDelegate(this);
    setItemDelegate(delegate_);
    connect(this, &QListView::clicked, this, &AgentTranscriptView::on_clicked);
    // Fonts and row heights follow the appearance — once it has settled (a
    // relayout inside a theme_changed handler re-styles widget by widget).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        delegate_->refresh_fonts();
        if (model_)
            model_->invalidate_layout();
        viewport()->update();
    });
}

void AgentTranscriptView::set_transcript_model(AgentTranscriptModel* model) {
    model_ = model;
    setModel(model);
    connect(model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& tl, const QModelIndex& br) {
        // Expand/collapse, a joined result and live text change a row's height.
        for (int r = tl.row(); r <= br.row(); ++r)
            delegate_->row_height_changed(model_->index(r));
    });
}

void AgentTranscriptView::scroll_to_end() {
    executeDelayedItemsLayout();
    scrollToBottom();
}

bool AgentTranscriptView::is_at_bottom() const {
    const QScrollBar* sb = verticalScrollBar();
    return sb->value() >= sb->maximum() - 8;
}

void AgentTranscriptView::on_clicked(const QModelIndex& index) {
    if (!model_ || !index.isValid())
        return;
    if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
        return; // extending the selection, not toggling
    const AgentTranscriptModel::Row* row = model_->row_at(index.row());
    if (row && (row->cached_more || row->expanded))
        model_->toggle_expanded(index.row());
}

void AgentTranscriptView::copy_selection() const {
    if (!model_)
        return;
    QModelIndexList rows = selectionModel() ? selectionModel()->selectedRows() : QModelIndexList{};
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
    QStringList parts;
    for (const auto& idx : rows) {
        if (const AgentTranscriptModel::Row* row = model_->row_at(idx.row()); row && !row->live)
            parts << AgentTranscriptModel::row_text(*row);
    }
    if (!parts.isEmpty())
        QApplication::clipboard()->setText(parts.join(QStringLiteral("\n\n")));
}

void AgentTranscriptView::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        copy_selection();
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

void AgentTranscriptView::contextMenuEvent(QContextMenuEvent* event) {
    if (!model_)
        return;
    const QModelIndex idx = indexAt(event->pos());
    QMenu menu(this);
    QAction* copy = menu.addAction(tr("Copy selected"));
    QAction* copy_all = menu.addAction(tr("Copy whole transcript"));
    QAction* view = menu.addAction(tr("View full event…"));
    view->setEnabled(idx.isValid());
    menu.addSeparator();
    QAction* expand = menu.addAction(tr("Expand all"));
    QAction* collapse = menu.addAction(tr("Collapse all"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == copy) {
        copy_selection();
    } else if (chosen == copy_all) {
        QStringList parts;
        for (int r = 0; r < model_->rowCount(); ++r) {
            const AgentTranscriptModel::Row* row = model_->row_at(r);
            if (row && !row->live)
                parts << AgentTranscriptModel::row_text(*row);
        }
        QApplication::clipboard()->setText(parts.join(QStringLiteral("\n\n")));
    } else if (chosen == view) {
        view_event(idx);
    } else if (chosen == expand) {
        model_->set_all_expanded(true);
    } else if (chosen == collapse) {
        model_->set_all_expanded(false);
    }
}

void AgentTranscriptView::view_event(const QModelIndex& index) {
    const AgentTranscriptModel::Row* row = model_ ? model_->row_at(index.row()) : nullptr;
    if (!row)
        return;
    QDialog dlg(this);
    dlg.setObjectName(QStringLiteral("awbDialog"));
    dlg.setWindowTitle(row->ev.title.left(120));
    dlg.resize(820, 560);
    auto* lay = new QVBoxLayout(&dlg);
    auto* text = new QPlainTextEdit(&dlg);
    text->setReadOnly(true);
    QString content = AgentTranscriptModel::row_text(*row);
    if (!row->ev.raw_json.isEmpty())
        content += QStringLiteral("\n\n— raw —\n") + row->ev.raw_json;
    if (row->result && !row->result->raw_json.isEmpty())
        content += QStringLiteral("\n\n— raw (result) —\n") + row->result->raw_json;
    text->setPlainText(content);
    lay->addWidget(text, 1);
    auto* close = new QPushButton(tr("CLOSE"), &dlg);
    close->setObjectName(QStringLiteral("awbBtn"));
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(close, 0, Qt::AlignRight);
    dlg.exec();
}

} // namespace fincept::screens
