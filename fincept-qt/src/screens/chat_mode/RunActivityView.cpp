// RunActivityView.cpp — the ACTIVITY tab of Chat Mode's inspector (WP2).
//
// A virtualised, append-only timeline of a run's events:
//   * RavModel wraps run_view::ActivityLog (pairing: a tool call and its result
//     share a row, a delegation's start and end too; questions / approvals are
//     marked settled). A live event inserts one row at the end or updates its
//     pair's row — the model is reset only when another run is shown or the
//     "detail rows" toggle changes.
//   * RavDelegate paints each row (no widget per event) and caches its height
//     per width; detail rows (data.verbose) are collapsed to their title.
//   * History comes from TaskRunService::fetch_events() (synchronous when the
//     events are cached, from the database otherwise); live events arriving
//     meanwhile are buffered and merged (the log drops seq duplicates).
//   * "Follow" keeps the newest row in view and switches itself off when the
//     user scrolls up; the list lays out in batches so thousands of events
//     never freeze the GUI.

#include "screens/chat_mode/RunActivityView.h"

#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/chat_mode/ToolResultCard.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace run_activity_view_detail {

constexpr int kRavPadV = 4;
constexpr int kRavExpandedMaxLines = 4000;
constexpr int kRavExpandedMaxChars = 60000;
constexpr int kRavCharsPerLine = 320; ///< bound on the text a collapsed line can need
constexpr int kRavBatchSize = 200;

agt::TaskRunService& rav_svc() {
    return agt::TaskRunService::instance();
}

QString rav_tag(const rv::ActivityRow& r) {
    using K = agt::RunEventKind;
    switch (r.ev.kind) {
        case K::Plan:
            return RunActivityView::tr("PLAN");
        case K::StepStart:
            return r.ev.data.value(QStringLiteral("sub")).toString() == QLatin1String("window")
                       ? RunActivityView::tr("WINDOW")
                       : RunActivityView::tr("STEP");
        case K::StepEnd: {
            if (r.ev.is_error)
                return RunActivityView::tr("FAILED");
            const QString st = r.ev.data.value(QStringLiteral("status")).toString();
            return st == QLatin1String("skipped") ? RunActivityView::tr("SKIPPED") : RunActivityView::tr("DONE");
        }
        case K::Thinking:
            return RunActivityView::tr("THINK");
        case K::ToolCall:
            return RunActivityView::tr("TOOL");
        case K::ToolResult:
            return RunActivityView::tr("OUTPUT");
        case K::Delegation:
            return RunActivityView::tr("AGENTS");
        case K::Question:
            return RunActivityView::tr("ASKS");
        case K::Answer:
            return RunActivityView::tr("ANSWER");
        case K::Approval:
            return RunActivityView::tr("APPROVAL");
        case K::ApprovalDecided:
            return RunActivityView::tr("DECIDED");
        case K::Usage:
            return RunActivityView::tr("USAGE");
        case K::Note:
            return RunActivityView::tr("NOTE");
        case K::Artefact:
            return RunActivityView::tr("FILE");
        case K::Error:
            return RunActivityView::tr("ERROR");
        case K::Final:
            return RunActivityView::tr("RESULT");
        case K::Status:
            break;
    }
    return RunActivityView::tr("STATUS");
}

/// Lines of body a collapsed row shows (0: title only).
int rav_collapsed_lines(const rv::ActivityRow& r) {
    using K = agt::RunEventKind;
    if (r.verbose)
        return 0;
    switch (r.ev.kind) {
        case K::Final:
            return 8;
        case K::Error:
        case K::Question:
        case K::Plan:
            return 6;
        case K::Approval:
            return 4;
        case K::Delegation:
        case K::Answer:
            return 3;
        case K::ToolCall:
            return r.has_end ? 3 : 2;
        case K::Usage:
        case K::Artefact:
        case K::StepStart:
            return 1;
        default:
            return 2;
    }
}

/// The text under the title: a pair shows the call's input, then its output.
QString rav_body(const rv::ActivityRow& r) {
    QString body = r.ev.body;
    if (r.has_end) {
        QString out = r.end.body;
        if (r.ev.kind == agt::RunEventKind::Delegation && !r.end.title.isEmpty())
            out = out.isEmpty() ? r.end.title : r.end.title + QLatin1Char('\n') + out;
        if (!out.isEmpty())
            body = body.isEmpty() ? QStringLiteral("→ ") + out : body + QStringLiteral("\n→ ") + out;
    }
    return body;
}

/// Right-hand status of the title line: duration and outcome of a pair / step,
/// "running…" for a call without result, the time otherwise.
QString rav_status(const rv::ActivityRow& r, bool run_live) {
    using K = agt::RunEventKind;
    const bool pairable = r.ev.kind == K::ToolCall || r.ev.kind == K::Delegation;
    if (pairable && !r.has_end)
        return run_live ? RunActivityView::tr("running…") : RunActivityView::tr("no result");
    if (r.has_end || (r.ev.kind == K::StepEnd && r.duration_ms >= 0)) {
        QString s = rv::ActivityLog::format_duration_precise(r.duration_ms);
        if (r.has_end)
            s += rv::activity_row_failed(r) ? QStringLiteral(" ✗") : QStringLiteral(" ✓");
        return s.trimmed();
    }
    if (r.settled)
        return r.ev.kind == K::Question ? RunActivityView::tr("answered") : RunActivityView::tr("decided");
    return r.ev.ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(r.ev.ts_ms).toString(QStringLiteral("HH:mm:ss")) : QString();
}

// ── Model ────────────────────────────────────────────────────────────────────

struct RavCache {
    int width = -1;
    int height = 0;
    bool more = false;
};

class RavModel : public QAbstractListModel {
  public:
    explicit RavModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(visible_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        const rv::ActivityRow* r = row_at(index.row());
        if (!r)
            return {};
        switch (role) {
            case Qt::DisplayRole:
                return r->ev.title;
            case Qt::AccessibleTextRole:
                return QStringLiteral("%1 %2 %3").arg(rav_tag(*r), r->ev.title, rav_status(*r, live_));
            case Qt::ToolTipRole: {
                QString tip = r->ev.ts_ms > 0 ? QLocale().toString(QDateTime::fromMSecsSinceEpoch(r->ev.ts_ms),
                                                                   QLocale::ShortFormat)
                                              : QString();
                if (r->ev.data.value(QStringLiteral("ambiguous")).toBool())
                    tip += QLatin1Char('\n') + RunActivityView::tr("Several runs were going: this terminal tool call "
                                                                   "may belong to another of them.");
                return tip.trimmed();
            }
            default:
                return {};
        }
    }

    const rv::ActivityRow* row_at(int view_row) const {
        if (view_row < 0 || view_row >= visible_.size())
            return nullptr;
        return &log_.at(visible_.at(view_row));
    }
    RavCache& cache_at(int view_row) const { return cache_[visible_.at(view_row)]; }
    int event_count() const { return log_.size(); }
    bool live() const { return live_; }
    void set_live(bool live) {
        if (live == live_)
            return;
        live_ = live;
        repaint_open_rows();
    }

    void reset(const QVector<agt::RunEvent>& events) {
        beginResetModel();
        log_.reset(events);
        cache_ = QVector<RavCache>(log_.size());
        rebuild_visible();
        endResetModel();
    }

    void clear() {
        beginResetModel();
        log_.clear();
        cache_.clear();
        visible_.clear();
        endResetModel();
    }

    /// One live event. Returns true when a visible row was added.
    bool append(const agt::RunEvent& ev) {
        const rv::ActivityLog::Change c = log_.append(ev);
        if (c.ignored)
            return false;
        if (c.updated >= 0) {
            cache_[c.updated] = {};
            const int vr = view_row_of(c.updated);
            if (vr >= 0) {
                const QModelIndex i = index(vr);
                emit dataChanged(i, i);
            }
        }
        if (c.inserted < 0)
            return false;
        cache_.append(RavCache{});
        if (!shows(log_.at(c.inserted)))
            return false;
        const int row = static_cast<int>(visible_.size());
        beginInsertRows({}, row, row);
        visible_.append(c.inserted);
        endInsertRows();
        return true;
    }

    void set_hide_verbose(bool hide) {
        if (hide == hide_verbose_)
            return;
        beginResetModel();
        hide_verbose_ = hide;
        rebuild_visible();
        endResetModel();
    }

    void toggle_expanded(int view_row) {
        if (view_row < 0 || view_row >= visible_.size())
            return;
        const int li = visible_.at(view_row);
        log_.row(li).expanded = !log_.row(li).expanded;
        cache_[li] = {};
        const QModelIndex i = index(view_row);
        emit dataChanged(i, i);
    }

    void set_all_expanded(bool expanded) {
        emit layoutAboutToBeChanged();
        for (int i = 0; i < log_.size(); ++i) {
            log_.row(i).expanded = expanded;
            cache_[i] = {};
        }
        emit layoutChanged();
    }

    void invalidate_layout() {
        emit layoutAboutToBeChanged();
        for (RavCache& c : cache_)
            c = {};
        emit layoutChanged();
    }

    QString all_text() const {
        QStringList parts;
        parts.reserve(visible_.size());
        for (int li : visible_)
            parts << rv::ActivityLog::row_text(log_.at(li));
        return parts.join(QStringLiteral("\n\n"));
    }

  private:
    bool shows(const rv::ActivityRow& r) const { return !(hide_verbose_ && r.verbose); }

    void rebuild_visible() {
        visible_.clear();
        visible_.reserve(log_.size());
        for (int i = 0; i < log_.size(); ++i)
            if (shows(log_.at(i)))
                visible_.append(i);
    }

    int view_row_of(int log_row) const {
        const auto it = std::lower_bound(visible_.cbegin(), visible_.cend(), log_row);
        return (it != visible_.cend() && *it == log_row) ? static_cast<int>(it - visible_.cbegin()) : -1;
    }

    void repaint_open_rows() {
        // "running…" becomes "no result" when the run ends (and back on resume).
        for (int vr = 0; vr < visible_.size(); ++vr) {
            const rv::ActivityRow& r = log_.at(visible_.at(vr));
            if ((r.ev.kind == agt::RunEventKind::ToolCall || r.ev.kind == agt::RunEventKind::Delegation) && !r.has_end) {
                const QModelIndex i = index(vr);
                emit dataChanged(i, i);
            }
        }
    }

    rv::ActivityLog log_;
    mutable QVector<RavCache> cache_; ///< per log row
    QVector<int> visible_;            ///< log row of each view row (ascending)
    bool hide_verbose_ = false;
    bool live_ = false;
};

// ── Delegate ─────────────────────────────────────────────────────────────────

class RavDelegate : public QStyledItemDelegate {
  public:
    explicit RavDelegate(QAbstractItemView* view) : QStyledItemDelegate(view), view_(view) { refresh_fonts(); }

    void refresh_fonts() {
        tag_font_ = ui::fonts::font(-5, QFont::Bold);
        title_font_ = ui::fonts::font(-3, QFont::DemiBold);
        body_font_ = ui::fonts::font(-3);
        meta_font_ = ui::fonts::font(-4);
        const QFontMetrics tfm(tag_font_);
        title_h_ = std::max(QFontMetrics(title_font_).height(), tfm.height() + 4);
        body_line_h_ = QFontMetrics(body_font_).lineSpacing();
        int w = 0;
        for (const char* t : {"PLAN", "WINDOW", "STEP", "FAILED", "SKIPPED", "DONE", "THINK", "TOOL", "OUTPUT", "AGENTS",
                              "ASKS", "ANSWER", "APPROVAL", "DECIDED", "USAGE", "NOTE", "FILE", "ERROR", "RESULT",
                              "STATUS"})
            w = std::max(w, tfm.horizontalAdvance(RunActivityView::tr(t)));
        tag_w_ = w + 2 * rv::pill_padding(tfm);
    }

    void row_height_changed(const QModelIndex& index) { emit sizeHintChanged(index); }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* m = static_cast<const RavModel*>(index.model());
        const rv::ActivityRow* r = m ? m->row_at(index.row()) : nullptr;
        if (!r)
            return QSize(0, title_h_ + 2 * kRavPadV);
        const int width = view_ ? view_->viewport()->width() : option.rect.width();
        RavCache& c = m->cache_at(index.row());
        if (c.width != width) {
            QTextLayout layout;
            const Body b = layout_body(layout, *r, body_width(width));
            c.more = b.more;
            c.height = kRavPadV + title_h_ + (b.lines > 0 ? 2 + b.lines * body_line_h_ : 0) + kRavPadV + 1;
            c.width = width;
        }
        return QSize(width, c.height);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* m = static_cast<const RavModel*>(index.model());
        const rv::ActivityRow* r = m ? m->row_at(index.row()) : nullptr;
        if (!r)
            return;
        const QRect rect = option.rect;
        p->save();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        p->fillRect(rect, QColor(selected ? ui::colors::BG_HOVER() : hovered ? ui::colors::BG_RAISED()
                                                                             : ui::colors::BG_SURFACE()));
        const rv::Tone tone = rv::activity_row_tone(*r);
        const bool final_row = r->ev.kind == agt::RunEventKind::Final;
        if (final_row)
            p->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), QColor(ui::colors::AMBER()));
        else if (tone == rv::Tone::Danger || tone == rv::Tone::Warning)
            p->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), rv::tone_color(tone));

        const int left = rect.left() + 8;
        const int right = rect.right() - 6;
        const int top = rect.top() + kRavPadV;
        const QColor tc = rv::tone_color(tone);

        // Kind tag.
        const QFontMetrics tfm(tag_font_);
        const QRect tag(left, top + (title_h_ - (tfm.height() + 2)) / 2, tag_w_, tfm.height() + 2);
        p->setFont(tag_font_);
        if (final_row) {
            p->fillRect(tag, QColor(ui::colors::AMBER()));
            p->setPen(QColor(ui::colors::TEXT_ON_ACCENT()));
        } else {
            p->setPen(tc);
            p->drawRect(tag.adjusted(0, 0, -1, -1));
        }
        p->drawText(tag, Qt::AlignCenter, rav_tag(*r));

        // Expand indicator, status, title.
        const RavCache& c = m->cache_at(index.row());
        int x_right = right;
        p->setFont(meta_font_);
        const QFontMetrics mfm(meta_font_);
        if (c.more || r->expanded) {
            const QString ind = r->expanded ? QStringLiteral("▾") : QStringLiteral("▸");
            const int iw = mfm.horizontalAdvance(ind) + 4;
            p->setPen(QColor(ui::colors::TEXT_TERTIARY()));
            p->drawText(QRect(x_right - iw, top, iw, title_h_), Qt::AlignRight | Qt::AlignVCenter, ind);
            x_right -= iw + 2;
        }
        const QString status = rav_status(*r, m->live());
        if (!status.isEmpty()) {
            const int sw = mfm.horizontalAdvance(status);
            const bool outcome = r->has_end || (r->ev.kind == agt::RunEventKind::StepEnd);
            p->setPen(outcome ? tc : QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_DIM()));
            p->drawText(QRect(x_right - sw, top, sw, title_h_), Qt::AlignRight | Qt::AlignVCenter, status);
            x_right -= sw + 6;
        }
        const int tx = tag.right() + 7;
        const int tw = std::max(10, x_right - tx);
        p->setFont(title_font_);
        const bool faint = r->verbose || r->ev.kind == agt::RunEventKind::Thinking || r->ev.kind == agt::RunEventKind::Usage;
        p->setPen(QColor(faint ? (selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY())
                               : ui::colors::TEXT_PRIMARY()));
        p->drawText(QRect(tx, top, tw, title_h_), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(title_font_).elidedText(r->ev.title.simplified(), Qt::ElideRight, tw));

        // Body.
        QTextLayout layout;
        const Body b = layout_body(layout, *r, body_width(rect.width()));
        if (b.lines > 0) {
            QColor body = QColor(ui::colors::TEXT_SECONDARY());
            if (final_row)
                body = QColor(ui::colors::TEXT_PRIMARY());
            else if (r->ev.kind == agt::RunEventKind::Thinking)
                body = QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY());
            else if (tone == rv::Tone::Danger)
                body = tc;
            p->setPen(body);
            p->setFont(body_font_);
            const QPointF origin(left, top + title_h_ + 2);
            for (int i = 0; i < b.lines && i < layout.lineCount(); ++i)
                layout.lineAt(i).draw(p, origin);
        }

        if (option.state & QStyle::State_HasFocus) {
            p->setPen(QColor(ui::colors::BORDER_BRIGHT()));
            p->setBrush(Qt::NoBrush);
            p->drawRect(rect.adjusted(0, 0, -1, -1));
        }
        p->setPen(QColor(ui::colors::BORDER_DIM()));
        p->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
        p->restore();
    }

  private:
    struct Body {
        int lines = 0;
        bool more = false;
    };

    int body_width(int row_width) const { return std::max(40, row_width - 8 - 6); }

    Body layout_body(QTextLayout& layout, const rv::ActivityRow& r, int width) const {
        Body out;
        const QString full = rav_body(r);
        if (full.isEmpty() || width <= 20)
            return out;
        const int max_lines = r.expanded ? kRavExpandedMaxLines : rav_collapsed_lines(r);
        if (max_lines <= 0) {
            out.more = true; // a detail row: its text shows when expanded
            return out;
        }
        const int cap = r.expanded ? kRavExpandedMaxChars : max_lines * kRavCharsPerLine;
        QString text = full.left(cap);
        text.replace(QLatin1Char('\t'), QStringLiteral("    "));
        text.remove(QLatin1Char('\r'));
        text.replace(QLatin1Char('\n'), QChar::LineSeparator);
        if (r.expanded && full.size() > cap)
            text += QChar::LineSeparator +
                    RunActivityView::tr("… %1 more characters: copy the event to get all of it").arg(full.size() - cap);
        layout.setText(text);
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
        if (!r.expanded && full.size() > cap)
            out.more = true;
        return out;
    }

    QPointer<QAbstractItemView> view_;
    QFont tag_font_;
    QFont title_font_;
    QFont body_font_;
    QFont meta_font_;
    int title_h_ = 16;
    int body_line_h_ = 14;
    int tag_w_ = 60;
};

QString rav_style() {
    return QStringLiteral(
               "#runActivityView{background:%1;}"
               "#runActivityView QLabel{background:transparent;border:none;color:%3;font-size:%7px;}"
               "#runActivityView QLabel#ravEmpty{color:%4;}"
               "#runActivityView QPushButton{background:%5;color:%3;border:1px solid %6;padding:2px 8px;"
               "font-size:%8px;font-weight:700;}"
               "#runActivityView QPushButton:hover{color:%2;border-color:%9;}"
               "#runActivityView QPushButton:focus{border-color:%10;}"
               "#runActivityView QPushButton:checked{background:%10;color:%11;border-color:%10;}"
               "#runActivityView QPushButton:disabled{color:%12;background:transparent;}"
               "#runActivityView QListView#ravList{background:%1;border:none;outline:none;}"
               "#runActivityView QScrollBar:vertical{background:transparent;width:6px;}"
               "#runActivityView QScrollBar::handle:vertical{background:%6;min-height:20px;}"
               "#runActivityView QScrollBar::add-line:vertical,#runActivityView QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
             ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::fonts::px(-3),
             ui::fonts::px(-4), ui::colors::BORDER_BRIGHT())
        .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::TEXT_DIM());
}

} // namespace run_activity_view_detail

using namespace run_activity_view_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct RunActivityView::Impl {
    RunActivityView* q = nullptr;
    QString run_id;
    int generation = 0;   ///< bumps on every set_run(): late history loads are dropped
    bool loading = false; ///< history not here yet: live events are buffered
    QVector<agt::RunEvent> buffered;
    bool follow = true;
    bool programmatic_scroll = false;
    int unseen = 0; ///< rows added while not following

    QLabel* count = nullptr;
    QPushButton* details_btn = nullptr;
    QPushButton* follow_btn = nullptr;
    QPushButton* copy_btn = nullptr;
    QStackedWidget* stack = nullptr;
    QLabel* empty = nullptr;
    QListView* view = nullptr;
    RavModel* model = nullptr;
    RavDelegate* delegate = nullptr;
    QTimer* follow_timer = nullptr;
    /// WP5: the selected tool call as a ToolResultCard under the list (its result
    /// by shape, Save as artefact). Built on the first tool row selected.
    ToolResultCard* drawer = nullptr;
    QString drawer_ref;

    void update_drawer(const QModelIndex& index);

    void build();
    void wire();
    void retranslate();
    void load();
    void on_event(const QString& id, const agt::RunEvent& ev);
    void update_header();
    void update_live();
    void set_follow(bool on);
    void scroll_to_end();
    void copy_selection() const;
    void show_menu(const QPoint& global, const QModelIndex& index);
};

void RunActivityView::Impl::build() {
    q->setObjectName(QStringLiteral("runActivityView"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(rav_style());

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(0, 6, 0, 0);
    vl->setSpacing(4);

    auto* bar = new QWidget(q);
    auto* flow = new rv::RvFlowLayout(bar, 4, 4);
    flow->setContentsMargins(8, 0, 8, 0);
    count = new QLabel(bar);
    details_btn = new QPushButton(bar);
    details_btn->setCheckable(true);
    details_btn->setChecked(true);
    follow_btn = new QPushButton(bar);
    follow_btn->setCheckable(true);
    follow_btn->setChecked(true);
    copy_btn = new QPushButton(bar);
    for (QPushButton* b : {details_btn, follow_btn, copy_btn})
        b->setCursor(Qt::PointingHandCursor);
    flow->addWidget(count);
    flow->addWidget(details_btn);
    flow->addWidget(follow_btn);
    flow->addWidget(copy_btn);
    vl->addWidget(bar);

    stack = new QStackedWidget(q);
    empty = new QLabel(stack);
    empty->setObjectName(QStringLiteral("ravEmpty"));
    empty->setWordWrap(true);
    empty->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    empty->setContentsMargins(12, 24, 12, 12);
    stack->addWidget(empty);

    view = new QListView(stack);
    view->setObjectName(QStringLiteral("ravList"));
    view->setUniformItemSizes(false);
    view->setResizeMode(QListView::Adjust);
    view->setLayoutMode(QListView::Batched); // thousands of rows lay out across event-loop turns
    view->setBatchSize(kRavBatchSize);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Heights depend on the viewport width: a scroll bar that comes and goes would re-wrap every row.
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setMouseTracking(true);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->verticalScrollBar()->setSingleStep(18);
    model = new RavModel(view);
    delegate = new RavDelegate(view);
    view->setItemDelegate(delegate);
    view->setModel(model);
    stack->addWidget(view);
    vl->addWidget(stack, 1);

    follow_timer = new QTimer(q);
    follow_timer->setSingleShot(true);
    follow_timer->setInterval(0);
}

void RunActivityView::Impl::wire() {
    auto& svc = rav_svc();
    QObject::connect(&svc, &agt::TaskRunService::run_event, q,
                     [this](const QString& id, const agt::RunEvent& ev) { on_event(id, ev); });
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, [this](const QString& id) {
        if (id != run_id)
            return;
        if (!rav_svc().find_run(id)) {
            // Deleted while shown.
            generation++;
            loading = false;
            buffered.clear();
            model->clear();
            if (drawer)
                drawer->setVisible(false);
            empty->setText(RunActivityView::tr("This run was deleted."));
            stack->setCurrentIndex(0);
            update_header();
            return;
        }
        update_live();
    });
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() {
        if (!run_id.isEmpty() && model->event_count() == 0 && !loading)
            load();
    });

    QObject::connect(details_btn, &QPushButton::toggled, q, [this](bool on) {
        model->set_hide_verbose(!on);
        update_header();
        if (follow)
            follow_timer->start();
    });
    QObject::connect(follow_btn, &QPushButton::toggled, q, [this](bool on) { set_follow(on); });
    QObject::connect(copy_btn, &QPushButton::clicked, q, [this]() {
        if (auto* cb = QApplication::clipboard())
            cb->setText(model->all_text());
    });
    QObject::connect(follow_timer, &QTimer::timeout, q, [this]() { scroll_to_end(); });

    QScrollBar* sb = view->verticalScrollBar();
    QObject::connect(sb, &QScrollBar::valueChanged, q, [this, sb](int value) {
        if (programmatic_scroll)
            return;
        const bool at_end = value >= sb->maximum() - 4;
        if (at_end != follow) {
            follow = at_end;
            if (follow)
                unseen = 0;
            const QSignalBlocker block(follow_btn);
            follow_btn->setChecked(follow);
            update_header();
        }
    });
    QObject::connect(sb, &QScrollBar::rangeChanged, q, [this](int, int) {
        if (follow)
            follow_timer->start(); // new rows (or a batch of the layout) grew the list
    });

    QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, q,
                     [this](const QModelIndex& index) { update_drawer(index); });
    QObject::connect(view, &QListView::clicked, q, [this](const QModelIndex& index) {
        if (QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::ShiftModifier))
            return; // extending the selection
        const rv::ActivityRow* r = model->row_at(index.row());
        if (r && (model->cache_at(index.row()).more || r->expanded))
            model->toggle_expanded(index.row());
    });
    QObject::connect(model, &QAbstractItemModel::dataChanged, q, [this](const QModelIndex& tl, const QModelIndex& br) {
        for (int row = tl.row(); row <= br.row(); ++row)
            delegate->row_height_changed(model->index(row));
    });
    QObject::connect(view, &QListView::customContextMenuRequested, q, [this](const QPoint& pos) {
        show_menu(view->viewport()->mapToGlobal(pos), view->indexAt(pos));
    });
    new rv::RvKeyHook(view, [this](const QKeyEvent* ke, bool apply) {
        if (ke->matches(QKeySequence::Copy)) {
            if (apply)
                copy_selection();
            return true;
        }
        if ((ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            ke->modifiers() == Qt::NoModifier) {
            if (apply)
                model->toggle_expanded(view->currentIndex().row());
            return true;
        }
        if (ke->key() == Qt::Key_Menu) {
            if (apply) {
                const QRect r = view->visualRect(view->currentIndex());
                show_menu(view->viewport()->mapToGlobal(r.isValid() ? r.center() : QPoint(8, 8)), view->currentIndex());
            }
            return true;
        }
        if (ke->key() == Qt::Key_End && ke->modifiers() == Qt::NoModifier) {
            if (apply) {
                follow_btn->setChecked(true);
                set_follow(true);
            }
            return true;
        }
        return false;
    });

    // The view's stylesheet follows theme and font through the theme manager's
    // re-style pass; the painted rows re-measure once the new appearance settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() {
        delegate->refresh_fonts();
        model->invalidate_layout();
        view->viewport()->update();
    });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange) {
            retranslate();
            model->invalidate_layout();
        } else if (t == QEvent::Show && follow) {
            follow_timer->start();
        }
    });
}

void RunActivityView::Impl::retranslate() {
    details_btn->setText(RunActivityView::tr("DETAILS"));
    details_btn->setToolTip(RunActivityView::tr("Show detail rows (model calls, token updates, connection notes), "
                                                "collapsed to one line"));
    follow_btn->setToolTip(RunActivityView::tr("Keep the newest event in view (End). Scrolling up turns it off."));
    copy_btn->setText(RunActivityView::tr("COPY ALL"));
    copy_btn->setToolTip(RunActivityView::tr("Copy every shown event as text"));
    view->setAccessibleName(RunActivityView::tr("Run activity"));
    view->setAccessibleDescription(RunActivityView::tr("Space or Enter expands an event, Ctrl+C copies the selected "
                                                       "events, End follows the newest one."));
    update_header();
}

void RunActivityView::Impl::update_header() {
    const int n = model->event_count();
    count->setText(RunActivityView::tr("%n event(s)", nullptr, n));
    follow_btn->setText(follow || unseen == 0 ? RunActivityView::tr("FOLLOW")
                                              : RunActivityView::tr("FOLLOW · %1 NEW").arg(unseen));
    follow_btn->setAccessibleName(follow_btn->text());
    copy_btn->setEnabled(model->rowCount() > 0);
}

void RunActivityView::Impl::update_live() {
    const agt::TaskRun* r = rav_svc().find_run(run_id);
    model->set_live(r && !agt::is_terminal(r->status));
}

void RunActivityView::Impl::set_follow(bool on) {
    follow = on;
    if (on) {
        unseen = 0;
        follow_timer->start();
    }
    update_header();
}

void RunActivityView::Impl::scroll_to_end() {
    if (!follow || model->rowCount() == 0)
        return;
    programmatic_scroll = true;
    view->scrollToBottom();
    QScrollBar* sb = view->verticalScrollBar();
    sb->setValue(sb->maximum());
    programmatic_scroll = false;
}

void RunActivityView::Impl::load() {
    const int gen = ++generation;
    loading = true;
    buffered.clear();
    model->clear();
    if (drawer)
        drawer->setVisible(false);
    drawer_ref.clear();
    unseen = 0;
    empty->setText(RunActivityView::tr("Loading activity…"));
    stack->setCurrentIndex(0);
    update_header();
    QPointer<RunActivityView> self(q);
    const QString id = run_id;
    rav_svc().fetch_events(id, q, [this, self, gen, id](const QVector<agt::RunEvent>& events) {
        if (!self || gen != generation || id != run_id)
            return; // another run is shown now
        loading = false;
        model->reset(events);
        for (const agt::RunEvent& ev : std::as_const(buffered))
            model->append(ev); // the log drops what the history already holds
        buffered.clear();
        update_live();
        if (model->event_count() == 0) {
            const agt::TaskRun* r = rav_svc().find_run(id);
            empty->setText(!r ? RunActivityView::tr("This run was deleted.")
                              : agt::is_terminal(r->status) ? RunActivityView::tr("No activity was recorded for this run.")
                                                            : RunActivityView::tr("No activity yet: events appear here "
                                                                                  "as the run works."));
            stack->setCurrentIndex(0);
        } else {
            stack->setCurrentIndex(1);
        }
        follow = true;
        {
            const QSignalBlocker block(follow_btn);
            follow_btn->setChecked(true);
        }
        update_header();
        follow_timer->start();
    });
}

void RunActivityView::Impl::update_drawer(const QModelIndex& index) {
    const rv::ActivityRow* r = model->row_at(index.row());
    const bool tool = r && (r->ev.kind == agt::RunEventKind::ToolCall || r->ev.kind == agt::RunEventKind::ToolResult);
    if (!tool) {
        if (drawer)
            drawer->setVisible(false);
        drawer_ref.clear();
        return;
    }
    if (!drawer) {
        drawer = new ToolResultCard(q);
        static_cast<QVBoxLayout*>(q->layout())->addWidget(drawer);
    }
    const agt::RunEvent& call = r->ev;
    const QString name = call.data.value(QStringLiteral("tool")).toString(call.title);
    drawer_ref = call.ref_id;
    drawer->set_call(name, call.kind == agt::RunEventKind::ToolCall ? call.body : QString(), run_id, call.step,
                     call.ref_id);
    const agt::RunEvent* end = r->has_end ? &r->end : (call.kind == agt::RunEventKind::ToolResult ? &call : nullptr);
    if (end) {
        const bool ok = !end->is_error && end->data.value(QStringLiteral("success")).toBool(true);
        const QJsonValue full = end->data.contains(QStringLiteral("result")) ? end->data.value(QStringLiteral("result"))
                                                                             : QJsonValue(QJsonValue::Undefined);
        drawer->set_result(ok, r->duration_ms, end->body, full);
        drawer->set_expanded(true);
    }
    drawer->setVisible(true);
}

void RunActivityView::Impl::on_event(const QString& id, const agt::RunEvent& ev) {
    if (id != run_id || run_id.isEmpty())
        return;
    if (loading) {
        buffered.append(ev);
        return;
    }
    // The tool call shown in the drawer finished: show its result.
    const bool drawer_pair = drawer && drawer->isVisible() && !drawer_ref.isEmpty() &&
                             ev.kind == agt::RunEventKind::ToolResult && ev.ref_id == drawer_ref;
    const bool added = model->append(ev);
    if (drawer_pair)
        update_drawer(view->currentIndex());
    if (stack->currentIndex() == 0 && model->event_count() > 0)
        stack->setCurrentIndex(1);
    if (added && !follow)
        ++unseen;
    update_header();
    // Following: rangeChanged scrolls once the row is laid out.
}

void RunActivityView::Impl::copy_selection() const {
    QModelIndexList rows = view->selectionModel() ? view->selectionModel()->selectedIndexes() : QModelIndexList{};
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
    QStringList parts;
    for (const QModelIndex& idx : rows)
        if (const rv::ActivityRow* r = model->row_at(idx.row()))
            parts << rv::ActivityLog::row_text(*r);
    if (parts.isEmpty())
        if (const rv::ActivityRow* r = model->row_at(view->currentIndex().row()))
            parts << rv::ActivityLog::row_text(*r);
    if (!parts.isEmpty())
        if (auto* cb = QApplication::clipboard())
            cb->setText(parts.join(QStringLiteral("\n\n")));
}

void RunActivityView::Impl::show_menu(const QPoint& global, const QModelIndex& index) {
    auto* menu = new QMenu(view);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const rv::ActivityRow* r = model->row_at(index.row());
    if (r && (r->ev.kind == agt::RunEventKind::ToolCall || r->ev.kind == agt::RunEventKind::ToolResult)) {
        // The tool's result: shown (and kept as an artefact) in the drawer below.
        QAction* show = menu->addAction(RunActivityView::tr("Show result / save as artefact"));
        show->setEnabled(r->has_end || r->ev.kind == agt::RunEventKind::ToolResult);
        const QModelIndex at = index;
        QObject::connect(show, &QAction::triggered, q, [this, at]() {
            view->setCurrentIndex(at);
            update_drawer(at);
        });
        menu->addSeparator();
    }
    QAction* copy_one = menu->addAction(RunActivityView::tr("Copy event"));
    copy_one->setEnabled(r != nullptr);
    const QString text = r ? rv::ActivityLog::row_text(*r) : QString();
    QObject::connect(copy_one, &QAction::triggered, q, [text]() {
        if (auto* cb = QApplication::clipboard())
            cb->setText(text);
    });
    QAction* copy_sel = menu->addAction(RunActivityView::tr("Copy selected events"));
    copy_sel->setShortcut(QKeySequence::Copy);
    QObject::connect(copy_sel, &QAction::triggered, q, [this]() { copy_selection(); });
    QAction* copy_all = menu->addAction(RunActivityView::tr("Copy all"));
    QObject::connect(copy_all, &QAction::triggered, q, [this]() {
        if (auto* cb = QApplication::clipboard())
            cb->setText(model->all_text());
    });
    menu->addSeparator();
    QAction* expand = menu->addAction(RunActivityView::tr("Expand all"));
    QObject::connect(expand, &QAction::triggered, q, [this]() { model->set_all_expanded(true); });
    QAction* collapse = menu->addAction(RunActivityView::tr("Collapse all"));
    QObject::connect(collapse, &QAction::triggered, q, [this]() { model->set_all_expanded(false); });
    menu->popup(global); // non-blocking
}

// ── RunActivityView ──────────────────────────────────────────────────────────

RunActivityView::RunActivityView(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
    d_->empty->setText(tr("No run selected.\n\nThe ACTIVITY of a run lists every step, tool call, question and "
                          "result as it happens."));
}

RunActivityView::~RunActivityView() = default;

void RunActivityView::set_run(const QString& run_id) {
    if (run_id == d_->run_id && (d_->loading || d_->model->event_count() > 0))
        return;
    d_->run_id = run_id;
    if (run_id.isEmpty()) {
        clear();
        return;
    }
    d_->load();
}

QString RunActivityView::run_id() const {
    return d_->run_id;
}

void RunActivityView::clear() {
    ++d_->generation;
    d_->run_id.clear();
    d_->loading = false;
    d_->buffered.clear();
    d_->model->clear();
    if (d_->drawer)
        d_->drawer->setVisible(false);
    d_->drawer_ref.clear();
    d_->unseen = 0;
    d_->empty->setText(tr("No run selected.\n\nThe ACTIVITY of a run lists every step, tool call, question and "
                          "result as it happens."));
    d_->stack->setCurrentIndex(0);
    d_->update_header();
}

} // namespace fincept::chat_mode
