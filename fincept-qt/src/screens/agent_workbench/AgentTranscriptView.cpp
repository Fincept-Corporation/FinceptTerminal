// AgentTranscriptView.cpp — painted, height-cached transcript rows.

#include "screens/agent_workbench/AgentTranscriptView.h"

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

namespace {

constexpr int kPadV = 4;
constexpr int kGutterTime = 64;
constexpr int kGutterTag = 62;
constexpr int kRightPad = 10;
constexpr int kIndicatorW = 18;
constexpr int kExpandedMaxLines = 4000;
constexpr int kExpandedMaxChars = 60000;

QColor kind_color(const ca::AgentEvent& ev) {
    using namespace ui;
    if (ev.is_error)
        return QColor(colors::NEGATIVE());
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
            return QColor(colors::POSITIVE());
        case ca::EventKind::Usage:
            return QColor(colors::TEXT_TERTIARY());
        case ca::EventKind::Error:
            return QColor(colors::NEGATIVE());
        case ca::EventKind::Final:
            return QColor(colors::AMBER());
        case ca::EventKind::Status:
            break;
    }
    return QColor(colors::TEXT_SECONDARY());
}

} // namespace

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

int AgentTranscriptModel::last_seq() const {
    return rows_.isEmpty() ? 0 : rows_.constLast().ev.seq;
}

void AgentTranscriptModel::reset_events(const QVector<ca::AgentEvent>& events) {
    beginResetModel();
    rows_.clear();
    const qsizetype from = qMax<qsizetype>(0, events.size() - kMaxRows);
    hidden_ = static_cast<int>(from);
    rows_.reserve(events.size() - from);
    for (qsizetype i = from; i < events.size(); ++i) {
        Row r;
        r.ev = events[i];
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

void AgentTranscriptModel::append_events(const QVector<ca::AgentEvent>& events) {
    const int last = last_seq();
    QVector<ca::AgentEvent> fresh;
    fresh.reserve(events.size());
    for (const auto& e : events) {
        if (e.seq > last)
            fresh.append(e);
    }
    if (fresh.isEmpty())
        return;
    if (fresh.size() > kMaxRows) {
        hidden_ += static_cast<int>(fresh.size() - kMaxRows);
        fresh = fresh.mid(fresh.size() - kMaxRows);
    }
    const qsizetype overflow = rows_.size() + fresh.size() - kMaxRows;
    if (overflow > 0) {
        const int remove_n = static_cast<int>(qMin(overflow, rows_.size()));
        beginRemoveRows({}, 0, remove_n - 1);
        rows_.remove(0, remove_n);
        endRemoveRows();
        hidden_ += remove_n;
    }
    const int first = static_cast<int>(rows_.size());
    beginInsertRows({}, first, first + static_cast<int>(fresh.size()) - 1);
    for (const auto& e : fresh) {
        Row r;
        r.ev = e;
        rows_.append(r);
    }
    endInsertRows();
    for (const auto& e : fresh) {
        if (e.kind == ca::EventKind::Final)
            mark_superseded(e);
    }
}

void AgentTranscriptModel::mark_superseded(const ca::AgentEvent& final_ev) {
    // The CLIs report the final answer twice (last assistant message + result):
    // keep the RESULT row prominent and fold the duplicate to one line.
    int final_row = -1;
    for (int i = static_cast<int>(rows_.size()) - 1; i >= 0; --i) {
        if (rows_[i].ev.seq == final_ev.seq) {
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
            emit dataChanged(idx, idx); // the view relayouts the row (see AgentTranscriptView)
            break;
        }
    }
}

void AgentTranscriptModel::clear_events() {
    beginResetModel();
    rows_.clear();
    hidden_ = 0;
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

// ═════════════════════════════════════════════════════════════════════════════
// Delegate
// ═════════════════════════════════════════════════════════════════════════════

AgentTranscriptDelegate::AgentTranscriptDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    refresh_fonts();
}

void AgentTranscriptDelegate::refresh_fonts() {
    const QString family = ui::fonts::DATA_FAMILY;
    meta_font_ = QFont(family);
    meta_font_.setPixelSize(ui::fonts::font_px(-3));
    tag_font_ = QFont(family);
    tag_font_.setPixelSize(ui::fonts::font_px(-4));
    tag_font_.setBold(true);
    title_font_ = QFont(family);
    title_font_.setPixelSize(ui::fonts::font_px(-1));
    title_font_.setBold(true);
    body_font_ = QFont(family);
    body_font_.setPixelSize(ui::fonts::font_px(-2));
    body_line_h_ = QFontMetrics(body_font_).lineSpacing();
    title_h_ = QFontMetrics(title_font_).lineSpacing() + 2;
}

QString AgentTranscriptDelegate::tag_text(const ca::AgentEvent& ev) const {
    switch (ev.kind) {
        case ca::EventKind::Text:
            return tr("AGENT");
        case ca::EventKind::Thinking:
            return tr("THINK");
        case ca::EventKind::ToolCall:
            return tr("TOOL");
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

QString AgentTranscriptDelegate::prepared_body(const AgentTranscriptModel::Row& row) {
    // Bound the layout work: a collapsed row never needs more than its first few
    // hundred characters per visible line.
    const int cap = row.expanded ? kExpandedMaxChars : collapsed_lines(row) * 320;
    QString text = row.ev.body.left(cap);
    text.replace(QLatin1Char('\t'), QStringLiteral("    "));
    text.replace(QLatin1Char('\r'), QString());
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    if (row.ev.body.size() > cap && row.expanded)
        text += QChar::LineSeparator + tr("… (%1 more characters — right-click › View full event)")
                                           .arg(row.ev.body.size() - cap);
    return text;
}

AgentTranscriptDelegate::BodyLayout AgentTranscriptDelegate::layout_body(QTextLayout& layout,
                                                                         const AgentTranscriptModel::Row& row,
                                                                         int width) const {
    BodyLayout out;
    if (row.ev.body.isEmpty() || width <= 20)
        return out;
    const int max_lines = row.expanded ? kExpandedMaxLines : collapsed_lines(row);
    layout.setText(prepared_body(row));
    layout.setFont(body_font_);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    qreal y = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        if (out.lines >= max_lines) {
            out.more = true;
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += body_line_h_;
        ++out.lines;
    }
    layout.endLayout();
    if (!row.expanded && row.ev.body.size() > collapsed_lines(row) * 320)
        out.more = true;
    return out;
}

int AgentTranscriptDelegate::content_width(int row_width) const {
    return qMax(40, row_width - kGutterTime - kGutterTag - kRightPad - 6);
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
        QTextLayout layout;
        const BodyLayout bl = layout_body(layout, *row, content_width(width));
        row->cached_more = bl.more;
        row->cached_height = kPadV + title_h_ + (bl.lines > 0 ? 2 + bl.lines * body_line_h_ : 0) + kPadV + 1;
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
    const QRect r = option.rect;
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    painter->fillRect(r, QColor(selected ? ui::colors::BG_HOVER() : (hovered ? ui::colors::BG_RAISED() : ui::colors::BG_BASE())));
    if (ev.kind == ca::EventKind::Final)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::AMBER()));
    else if (ev.is_error)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::NEGATIVE()));

    const int top = r.top() + kPadV;
    // Time
    painter->setFont(meta_font_);
    painter->setPen(QColor(ui::colors::TEXT_DIM()));
    const QString when =
        ev.ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(ev.ts_ms).toString(QStringLiteral("HH:mm:ss")) : QString();
    painter->drawText(QRect(r.left() + 8, top, kGutterTime - 10, title_h_), Qt::AlignLeft | Qt::AlignVCenter, when);

    // Kind tag
    const QColor kc = kind_color(ev);
    const QRect tag(r.left() + kGutterTime, top + 2, kGutterTag - 10, title_h_ - 4);
    painter->setFont(tag_font_);
    if (ev.kind == ca::EventKind::Final) {
        painter->fillRect(tag, kc);
        painter->setPen(QColor(ui::colors::TEXT_ON_ACCENT()));
    } else {
        painter->setPen(kc);
        painter->drawRect(tag.adjusted(0, 0, -1, -1));
    }
    painter->drawText(tag, Qt::AlignCenter, tag_text(ev));

    // Title
    const int x = r.left() + kGutterTime + kGutterTag;
    const int title_w = qMax(20, r.right() - kRightPad - kIndicatorW - x);
    painter->setFont(title_font_);
    QColor title_color = ev.kind == ca::EventKind::Text || ev.kind == ca::EventKind::Final
                             ? QColor(ui::colors::TEXT_PRIMARY())
                             : kc;
    if (row->superseded)
        title_color = QColor(ui::colors::TEXT_TERTIARY());
    painter->setPen(title_color);
    const QString title = QFontMetrics(title_font_).elidedText(ev.title, Qt::ElideRight, title_w);
    painter->drawText(QRect(x, top, title_w, title_h_), Qt::AlignLeft | Qt::AlignVCenter, title);

    // Expand indicator
    if (row->cached_more || row->expanded) {
        painter->setFont(meta_font_);
        painter->setPen(QColor(ui::colors::TEXT_TERTIARY()));
        painter->drawText(QRect(r.right() - kRightPad - kIndicatorW, top, kIndicatorW, title_h_),
                          Qt::AlignRight | Qt::AlignVCenter, row->expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
    }

    // Body
    if (!ev.body.isEmpty()) {
        QTextLayout layout;
        const BodyLayout bl = layout_body(layout, *row, content_width(r.width()));
        QColor body_color = QColor(ui::colors::TEXT_SECONDARY());
        if (ev.kind == ca::EventKind::Text || ev.kind == ca::EventKind::Final)
            body_color = QColor(ui::colors::TEXT_PRIMARY());
        if (ev.kind == ca::EventKind::Thinking || row->superseded)
            body_color = QColor(ui::colors::TEXT_TERTIARY());
        if (ev.is_error)
            body_color = QColor(ui::colors::NEGATIVE());
        painter->setPen(body_color);
        painter->setFont(body_font_);
        const QPointF origin(x, top + title_h_ + 2);
        for (int i = 0; i < bl.lines && i < layout.lineCount(); ++i)
            layout.lineAt(i).draw(painter, origin);
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
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
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
        // Expand/collapse changes a row's height — ask the view to relayout it.
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
        if (const AgentTranscriptModel::Row* row = model_->row_at(idx.row()))
            parts << AgentTranscriptModel::event_text(row->ev);
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
        for (int r = 0; r < model_->rowCount(); ++r)
            parts << AgentTranscriptModel::event_text(model_->row_at(r)->ev);
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
    QString content = AgentTranscriptModel::event_text(row->ev);
    if (!row->ev.raw_json.isEmpty())
        content += QStringLiteral("\n\n— raw —\n") + row->ev.raw_json;
    text->setPlainText(content);
    lay->addWidget(text, 1);
    auto* close = new QPushButton(tr("CLOSE"), &dlg);
    close->setObjectName(QStringLiteral("awbBtn"));
    connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(close, 0, Qt::AlignRight);
    dlg.exec();
}

} // namespace fincept::screens
