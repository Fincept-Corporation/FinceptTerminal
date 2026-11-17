// RunListPanel.cpp — the RUNS section of Chat Mode's left rail (WP2; regrouped in
// the 2026-11-17 revamp).
//
// Model / view, no widget per row: RlpModel holds one row per run (arrival
// order, ids only — the run itself is read from TaskRunService at paint time),
// RlpProxy filters (All / Active / Needs attention / Finished, the engine, the
// text search) and sorts into groups — PINNED, NEEDS YOU, TODAY, YESTERDAY, THIS
// WEEK, OLDER (newest first inside each), or PINNED then one group per state in
// the "By status" order. RlpDelegate paints a group header above the first row of
// each group and a two-line row (state mark, title, time; state pill, snippet,
// engine chip, step n/m, credits). Rows have two heights (with / without header),
// both O(1) from cached font metrics. TaskRunService signals update single rows
// (dataChanged / insert / remove) and ask for one coalesced relayout (a row's
// header can appear or go when a neighbour changes group); the model is only
// merged — never rebuilt — when persisted runs arrive (runs_reset). Pinned runs
// are kept in QSettings "chat_mode/pinned_runs" (pruned of deleted runs).
// Destructive actions confirm in an inline bar (no exec() dialogs).

#include "screens/chat_mode/RunListPanel.h"

#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAbstractListModel>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace run_list_panel_detail {

constexpr int kRlpTickMs = 1000;
constexpr int kRlpSearchDebounceMs = 150;
constexpr int kRlpRunIdRole = Qt::UserRole + 1;
constexpr const char* kRlpPinnedKey = "chat_mode/pinned_runs";
constexpr int kRlpPromptScanChars = 600; ///< the snippet reads the beginning of the prompt only

/// The engine filter (TaskRun::engine and plan_first).
enum class RlpEngine { All = 0, CloudReply, CloudTask, Local, Harness };

agt::TaskRunService& rlp_svc() {
    return agt::TaskRunService::instance();
}

qint64 rlp_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// "14:32" today / yesterday, "Mon 14:32" this week, "12 Nov" before.
QString rlp_when(qint64 ms) {
    if (ms <= 0)
        return {};
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
    const QDate today = QDate::currentDate();
    const QDate d = dt.date();
    if (d >= today.addDays(-1))
        return QLocale().toString(dt.time(), QStringLiteral("HH:mm"));
    if (d >= today.addDays(-6))
        return QLocale().toString(dt, QStringLiteral("ddd HH:mm"));
    return QLocale().toString(d, QStringLiteral("dd MMM"));
}

bool rlp_is_harness(const agt::TaskRun& r) {
    return r.engine == QLatin1String(rv::kHarnessEngineId);
}

bool rlp_engine_matches(const agt::TaskRun& r, RlpEngine e) {
    const bool cloud = r.engine == QLatin1String(agt::kEngineCloud) || r.engine.isEmpty();
    switch (e) {
        case RlpEngine::All:
            return true;
        case RlpEngine::CloudReply:
            return cloud && r.executor_id() != QLatin1String(agt::kExecutorCloudPlan);
        case RlpEngine::CloudTask:
            return cloud && r.executor_id() == QLatin1String(agt::kExecutorCloudPlan);
        case RlpEngine::Local:
            return r.engine == QLatin1String(agt::kEngineLocal);
        case RlpEngine::Harness:
            return rlp_is_harness(r);
    }
    return true;
}

/// The engine chip of a row.
QString rlp_engine_chip(const agt::TaskRun& r) {
    if (rlp_is_harness(r))
        return RunListPanel::tr("HARNESS");
    if (r.engine == QLatin1String(agt::kEngineLocal))
        return RunListPanel::tr("LOCAL");
    if (r.executor_id() == QLatin1String(agt::kExecutorCloudPlan))
        return RunListPanel::tr("TASK");
    return RunListPanel::tr("CLOUD");
}

QString rlp_engine_name(const agt::TaskRun& r) {
    if (rlp_is_harness(r))
        return RunListPanel::tr("Agent harness (your own API key)");
    if (r.engine == QLatin1String(agt::kEngineLocal))
        return RunListPanel::tr("Local agent (your own model)");
    if (r.executor_id() == QLatin1String(agt::kExecutorCloudPlan))
        return RunListPanel::tr("Cloud · multi-step task");
    return RunListPanel::tr("Cloud · one reply");
}

// ── Groups ───────────────────────────────────────────────────────────────────

/// Group keys: 0 pinned; Newest: 1 needs you, 2 today, 3 yesterday, 4 this week,
/// 5 older; By status: 1 + run_view::run_status_rank().
int rlp_group_key(const agt::TaskRun& r, rv::RunSort sort, bool pinned, const QDate& today) {
    if (pinned)
        return 0;
    if (sort == rv::RunSort::Status)
        return 1 + rv::run_status_rank(r.status);
    if (agt::needs_attention(r.status))
        return 1;
    if (r.created_at_ms <= 0)
        return 5;
    const QDate d = QDateTime::fromMSecsSinceEpoch(r.created_at_ms).date();
    if (d >= today)
        return 2;
    if (d == today.addDays(-1))
        return 3;
    const int first = static_cast<int>(QLocale().firstDayOfWeek());
    const QDate week_start = today.addDays(-((today.dayOfWeek() - first + 7) % 7));
    if (d >= week_start)
        return 4;
    return 5;
}

QString rlp_group_label(int key, rv::RunSort sort) {
    if (key == 0)
        return RunListPanel::tr("PINNED");
    if (sort == rv::RunSort::Status) {
        switch (key - 1) {
            case 0:
                return RunListPanel::tr("NEEDS YOU");
            case 1:
                return RunListPanel::tr("WORKING");
            case 2:
                return RunListPanel::tr("QUEUED");
            case 3:
                return RunListPanel::tr("PAUSED");
            case 4:
                return RunListPanel::tr("INTERRUPTED");
            case 5:
                return RunListPanel::tr("BUDGET STOP");
            case 6:
                return RunListPanel::tr("FAILED");
            case 7:
                return RunListPanel::tr("CANCELLED");
            case 8:
                return RunListPanel::tr("COMPLETED");
            default:
                return RunListPanel::tr("OTHER");
        }
    }
    switch (key) {
        case 1:
            return RunListPanel::tr("NEEDS YOU");
        case 2:
            return RunListPanel::tr("TODAY");
        case 3:
            return RunListPanel::tr("YESTERDAY");
        case 4:
            return RunListPanel::tr("THIS WEEK");
        default:
            return RunListPanel::tr("OLDER");
    }
}

/// The right-hand figures of a row's second line: step n/m · queued #n · credits.
QString rlp_figures(const agt::TaskRun& r) {
    auto& svc = rlp_svc();
    QStringList parts;
    const QString progress = rv::step_progress_text(r, svc.steps(r.id));
    if (!progress.isEmpty())
        parts << progress;
    if (r.status == agt::RunStatus::Queued) {
        const int pos = svc.queue_position(r.id);
        parts << (pos > 0 ? RunListPanel::tr("queued #%1").arg(pos) : RunListPanel::tr("queued"));
    }
    if (r.credits_spent > 0)
        parts << RunListPanel::tr("%1 cr").arg(rv::format_credits(r.credits_spent));
    return parts.join(QStringLiteral(" · "));
}

/// The time on a row's first line: the working time of a run that is going, when it
/// ended for a finished one, when it was made otherwise.
QString rlp_time(const agt::TaskRun& r) {
    if (agt::is_terminal(r.status))
        return rlp_when(r.finished_at_ms > 0 ? r.finished_at_ms : r.updated_at_ms);
    if (r.status != agt::RunStatus::Queued && r.started_at_ms > 0)
        return rv::format_duration(rv::run_elapsed_ms(r, rlp_now_ms()));
    return rlp_when(r.created_at_ms);
}

/// Text form of the second line (accessibility).
QString rlp_meta(const agt::TaskRun& r) {
    QStringList parts;
    parts << rlp_engine_chip(r);
    const QString figures = rlp_figures(r);
    if (!figures.isEmpty())
        parts << figures;
    const QString t = rlp_time(r);
    if (!t.isEmpty())
        parts << t;
    return parts.join(QStringLiteral(" · "));
}

QString rlp_tooltip(const agt::TaskRun& r) {
    QStringList lines;
    lines << r.title;
    lines << agt::run_status_label(r.status);
    lines << rlp_engine_name(r);
    if (!r.activity.isEmpty() && !agt::is_terminal(r.status))
        lines << r.activity;
    if (!r.error.isEmpty() && r.status != agt::RunStatus::Completed)
        lines << r.error.left(400);
    lines << RunListPanel::tr("Started %1").arg(rlp_when(r.created_at_ms));
    if (r.credits_spent > 0 || r.tokens_used > 0)
        lines << RunListPanel::tr("%1 credits · %2 tokens").arg(rv::format_credits(r.credits_spent)).arg(r.tokens_used);
    if (r.is_unattended())
        lines << RunListPanel::tr("Scheduled run (paper only)");
    return lines.join(QLatin1Char('\n'));
}

// ── Source model ─────────────────────────────────────────────────────────────

class RlpModel : public QAbstractListModel {
  public:
    explicit RlpModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(ids_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= ids_.size())
            return {};
        const QString& id = ids_.at(index.row());
        if (role == kRlpRunIdRole)
            return id;
        const agt::TaskRun* r = rlp_svc().find_run(id);
        if (!r)
            return {};
        switch (role) {
            case Qt::DisplayRole:
                return r->title;
            case Qt::ToolTipRole:
                return rlp_tooltip(*r);
            case Qt::AccessibleTextRole:
                return RunListPanel::tr("%1, %2, %3").arg(r->title, agt::run_status_label(r->status), rlp_meta(*r));
            default:
                return {};
        }
    }

    QString id_at(int row) const { return (row >= 0 && row < ids_.size()) ? ids_.at(row) : QString(); }
    int row_of(const QString& id) const { return pos_.value(id, -1); }
    bool contains(const QString& id) const { return pos_.contains(id); }

    void add(const QString& id) {
        if (id.isEmpty() || pos_.contains(id))
            return;
        const int row = static_cast<int>(ids_.size());
        beginInsertRows({}, row, row);
        ids_.append(id);
        pos_.insert(id, row);
        endInsertRows();
    }

    void remove(const QString& id) {
        const int row = row_of(id);
        if (row < 0)
            return;
        beginRemoveRows({}, row, row);
        ids_.removeAt(row);
        reindex();
        endRemoveRows();
    }

    void touch(const QString& id) {
        const int row = row_of(id);
        if (row < 0)
            return;
        const QModelIndex i = index(row);
        emit dataChanged(i, i);
    }

    void touch_all() {
        if (!ids_.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(ids_.size()) - 1));
    }

    /// Make the rows exactly `ids` (persisted runs arrived, or the service was
    /// reset): removes what is gone and appends what is new — the rest keep
    /// their rows, selection and scroll position.
    void merge(const QStringList& ids) {
        const QSet<QString> want(ids.cbegin(), ids.cend());
        for (int row = static_cast<int>(ids_.size()) - 1; row >= 0; --row) {
            if (want.contains(ids_.at(row)))
                continue;
            beginRemoveRows({}, row, row);
            ids_.removeAt(row);
            endRemoveRows();
        }
        reindex();
        QStringList fresh;
        for (const QString& id : ids)
            if (!pos_.contains(id))
                fresh << id;
        if (fresh.isEmpty())
            return;
        const int first = static_cast<int>(ids_.size());
        beginInsertRows({}, first, first + static_cast<int>(fresh.size()) - 1);
        ids_ << fresh;
        reindex();
        endInsertRows();
    }

    const QStringList& ids() const { return ids_; }

  private:
    void reindex() {
        pos_.clear();
        for (int i = 0; i < ids_.size(); ++i)
            pos_.insert(ids_.at(i), i);
    }

    QStringList ids_;
    QHash<QString, int> pos_;
};

// ── Filter / sort proxy ──────────────────────────────────────────────────────

class RlpProxy : public QSortFilterProxyModel {
  public:
    explicit RlpProxy(QObject* parent) : QSortFilterProxyModel(parent), today_(QDate::currentDate()) {
        setDynamicSortFilter(true);
    }

    void set_filter(rv::RunFilter f) {
        if (f == filter_)
            return;
        filter_ = f;
        invalidateFilter();
    }
    void set_engine(RlpEngine e) {
        if (e == engine_)
            return;
        engine_ = e;
        invalidateFilter();
    }
    void set_search(const QString& q) {
        if (q == search_)
            return;
        search_ = q;
        invalidateFilter();
    }
    void set_order(rv::RunSort s) {
        if (s == sort_)
            return;
        sort_ = s;
        invalidate();
    }
    void set_pinned(const QSet<QString>& pinned) {
        if (pinned == pinned_)
            return;
        pinned_ = pinned;
        invalidate();
    }
    /// The day the date groups are relative to; true when it changed (a re-sort followed).
    bool refresh_today() {
        const QDate t = QDate::currentDate();
        if (t == today_)
            return false;
        today_ = t;
        invalidate();
        return true;
    }

    rv::RunFilter filter() const { return filter_; }
    RlpEngine engine() const { return engine_; }
    rv::RunSort order() const { return sort_; }
    const QString& search() const { return search_; }
    const QSet<QString>& pinned() const { return pinned_; }
    bool is_pinned(const QString& id) const { return pinned_.contains(id); }

    /// The group of the run at proxy row `row` (-1 when there is none).
    int group_at(int row) const {
        if (row < 0 || row >= rowCount())
            return -1;
        const QString id = index(row, 0).data(kRlpRunIdRole).toString();
        const agt::TaskRun* r = rlp_svc().find_run(id);
        return r ? rlp_group_key(*r, sort_, pinned_.contains(id), today_) : -1;
    }

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
        const QString id = sourceModel()->index(source_row, 0, source_parent).data(kRlpRunIdRole).toString();
        const agt::TaskRun* r = rlp_svc().find_run(id);
        return r && rv::run_matches_filter(r->status, filter_) && rlp_engine_matches(*r, engine_) &&
               rv::run_matches_search(*r, search_);
    }

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        const QString ida = left.data(kRlpRunIdRole).toString();
        const QString idb = right.data(kRlpRunIdRole).toString();
        const agt::TaskRun* a = rlp_svc().find_run(ida);
        const agt::TaskRun* b = rlp_svc().find_run(idb);
        if (!a || !b)
            return a && !b; // a vanished run sorts last
        const int ga = rlp_group_key(*a, sort_, pinned_.contains(ida), today_);
        const int gb = rlp_group_key(*b, sort_, pinned_.contains(idb), today_);
        if (ga != gb)
            return ga < gb;
        if (a->created_at_ms != b->created_at_ms)
            return a->created_at_ms > b->created_at_ms;
        return a->id < b->id;
    }

  private:
    rv::RunFilter filter_ = rv::RunFilter::All;
    RlpEngine engine_ = RlpEngine::All;
    rv::RunSort sort_ = rv::RunSort::Recent;
    QString search_;
    QSet<QString> pinned_;
    QDate today_;
};

// ── Painted rows ─────────────────────────────────────────────────────────────

class RlpDelegate : public QStyledItemDelegate {
  public:
    RlpDelegate(RlpProxy* proxy, QObject* parent) : QStyledItemDelegate(parent), proxy_(proxy) { refresh_fonts(); }

    void refresh_fonts() {
        title_font_ = ui::fonts::font(-2, QFont::DemiBold);
        meta_font_ = ui::fonts::font(-3);
        pill_font_ = ui::fonts::font(-5, QFont::Bold);
        header_font_ = ui::fonts::font(-4, QFont::Bold);
        header_font_.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        const QFontMetrics tfm(title_font_);
        const QFontMetrics mfm(meta_font_);
        const QFontMetrics pfm(pill_font_);
        const QFontMetrics hfm(header_font_);
        pad_v_ = std::max(5, mfm.height() / 3);
        line1_h_ = std::max(tfm.height(), mfm.height());
        line2_h_ = std::max(mfm.height(), rv::pill_height(pfm));
        row_h_ = pad_v_ + line1_h_ + kGap + line2_h_ + pad_v_ + 1;
        header_h_ = hfm.height() + std::max(10, hfm.height() * 3 / 4);
        dot_ = std::max(6, mfm.height() / 2);
        prompt_lines_.clear();
    }

    /// Forget the cached prompt line of a run (it was deleted).
    void forget(const QString& id) { prompt_lines_.remove(id); }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        return QSize(option.rect.width(), row_h_ + (has_header(index) ? header_h_ : 0));
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const QString id = index.data(kRlpRunIdRole).toString();
        const agt::TaskRun* r = rlp_svc().find_run(id);
        QRect rect = option.rect;
        p->save();
        p->fillRect(rect, QColor(ui::colors::BG_SURFACE()));
        if (has_header(index)) {
            const QRect header(rect.left(), rect.top(), rect.width(), header_h_);
            paint_header(p, header, proxy_ ? proxy_->group_at(index.row()) : -1);
            rect.setTop(rect.top() + header_h_);
        }
        if (!r) {
            p->restore();
            return;
        }
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool attention = agt::needs_attention(r->status);
        QColor bg(ui::colors::BG_SURFACE());
        if (selected)
            bg = QColor(ui::colors::BG_HOVER());
        else if (attention)
            bg = rv::tone_fill(rv::Tone::Warning, 0.10);
        else if (hovered)
            bg = QColor(ui::colors::BG_RAISED());
        p->fillRect(rect, bg);
        // Left bar: attention (WARNING) wins over the selection marker (accent).
        if (attention)
            p->fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), rv::tone_color(rv::Tone::Warning));
        else if (selected)
            p->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), QColor(ui::colors::AMBER()));

        const int left = rect.left() + 10;
        const int right = rect.right() - 8;
        const rv::Tone tone = rv::run_status_tone(r->status);
        const QFontMetrics tfm(title_font_);
        const QFontMetrics mfm(meta_font_);
        const QFontMetrics pfm(pill_font_);

        // ── Line 1: state mark · title ………… time ──
        const int top1 = rect.top() + pad_v_;
        {
            p->setRenderHint(QPainter::Antialiasing, true);
            const QRectF dot(left, top1 + (line1_h_ - dot_) / 2.0, dot_, dot_);
            const QColor c = rv::tone_color(tone);
            if (agt::holds_slot(r->status)) {
                // Working: a ring (it is still going), filled once it has ended.
                p->setPen(QPen(c, 1.5));
                p->setBrush(Qt::NoBrush);
                p->drawEllipse(dot.adjusted(0.75, 0.75, -0.75, -0.75));
            } else {
                p->setPen(Qt::NoPen);
                p->setBrush(c);
                p->drawEllipse(dot);
            }
            p->setRenderHint(QPainter::Antialiasing, false);
        }
        const QString when = rlp_time(*r);
        const int when_w = when.isEmpty() ? 0 : mfm.horizontalAdvance(when);
        p->setFont(meta_font_);
        p->setPen(QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY()));
        if (!when.isEmpty())
            p->drawText(QRect(right - when_w, top1, when_w, line1_h_), Qt::AlignRight | Qt::AlignVCenter, when);
        const int tx = left + dot_ + 6;
        const int tw = std::max(10, right - when_w - (when_w > 0 ? 8 : 0) - tx);
        p->setFont(title_font_);
        p->setPen(QColor(ui::colors::TEXT_PRIMARY()));
        const QString title = r->title.trimmed().isEmpty() ? RunListPanel::tr("(untitled run)") : r->title.simplified();
        p->drawText(QRect(tx, top1, tw, line1_h_), Qt::AlignLeft | Qt::AlignVCenter,
                    tfm.elidedText(title, Qt::ElideRight, tw));

        // ── Line 2: [state pill] snippet ………… [ENGINE] figures ──
        const int top2 = top1 + line1_h_ + kGap;
        int x = tx;
        int rx = right;
        const QString figures = rlp_figures(*r);
        if (!figures.isEmpty()) {
            const int fw = mfm.horizontalAdvance(figures);
            p->setFont(meta_font_);
            p->setPen(QColor(ui::colors::TEXT_TERTIARY()));
            p->drawText(QRect(rx - fw, top2, fw, line2_h_), Qt::AlignRight | Qt::AlignVCenter, figures);
            rx -= fw + 6;
        }
        const QString chip = rlp_engine_chip(*r);
        const int cw = rv::pill_width(pfm, chip);
        if (rx - cw > x + 40) {
            const QRect cr(rx - cw, top2 + (line2_h_ - rv::pill_height(pfm)) / 2, cw, rv::pill_height(pfm));
            p->setPen(QPen(QColor(ui::colors::BORDER_MED()), 1));
            p->setBrush(Qt::NoBrush);
            p->drawRect(cr.adjusted(0, 0, -1, -1));
            p->setFont(pill_font_);
            p->setPen(QColor(ui::colors::TEXT_SECONDARY()));
            p->drawText(cr, Qt::AlignCenter, chip);
            rx -= cw + 6;
        }
        if (r->status != agt::RunStatus::Completed) {
            const QString pill = rv::run_status_pill(r->status);
            const int pw = rv::pill_width(pfm, pill);
            if (x + pw < rx) {
                rv::paint_pill(p, QPoint(x, top2 + (line2_h_ - rv::pill_height(pfm)) / 2), pill, tone, pill_font_);
                x += pw + 6;
            }
        }
        const bool failed = r->status == agt::RunStatus::Failed && !r->error.isEmpty();
        const QString question = r->waiting_payload.value(QStringLiteral("question")).toString();
        QString snippet;
        QColor snippet_color(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY());
        if (failed) {
            snippet = r->error;
            snippet_color = rv::tone_color(rv::Tone::Danger);
        } else if (attention && !question.isEmpty()) {
            snippet = question;
            snippet_color = rv::tone_color(rv::Tone::Warning);
        } else if (!agt::is_terminal(r->status) && r->status != agt::RunStatus::Queued && !r->activity.isEmpty()) {
            snippet = r->activity;
        } else {
            snippet = prompt_line(*r);
        }
        if (rx - x > 12 && !snippet.isEmpty()) {
            p->setFont(meta_font_);
            p->setPen(snippet_color);
            p->drawText(QRect(x, top2, rx - x, line2_h_), Qt::AlignLeft | Qt::AlignVCenter,
                        mfm.elidedText(snippet.simplified(), Qt::ElideRight, rx - x));
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
    static constexpr int kGap = 3;

    bool has_header(const QModelIndex& index) const {
        if (!proxy_ || !index.isValid())
            return false;
        const int row = index.row();
        if (row == 0)
            return true;
        return proxy_->group_at(row) != proxy_->group_at(row - 1);
    }

    void paint_header(QPainter* p, const QRect& r, int group) const {
        p->fillRect(r, QColor(ui::colors::BG_SURFACE()));
        if (group < 0)
            return;
        const QString label = rlp_group_label(group, proxy_->order());
        p->setFont(header_font_);
        // Group 1 is NEEDS YOU in both orders (By status: rank 0).
        p->setPen(group == 1 ? rv::tone_color(rv::Tone::Warning) : QColor(ui::colors::TEXT_TERTIARY()));
        const QRect text_r = r.adjusted(10, 0, -8, -2);
        p->drawText(text_r, Qt::AlignLeft | Qt::AlignBottom, label);
        p->setPen(QColor(ui::colors::BORDER_DIM()));
        p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
    }

    /// The beginning of what the user typed (the composed context left out), one line.
    QString prompt_line(const agt::TaskRun& r) const {
        const auto it = prompt_lines_.constFind(r.id);
        if (it != prompt_lines_.constEnd())
            return it.value();
        QString typed = rv::split_composed_prompt(r.prompt.left(kRlpPromptScanChars)).text;
        const qsizetype nl = typed.indexOf(QLatin1Char('\n'));
        if (nl > 0)
            typed = typed.left(nl);
        typed = typed.simplified();
        prompt_lines_.insert(r.id, typed);
        return typed;
    }

    RlpProxy* proxy_ = nullptr;
    QFont title_font_;
    QFont meta_font_;
    QFont pill_font_;
    QFont header_font_;
    int pad_v_ = 6;
    int line1_h_ = 16;
    int line2_h_ = 14;
    int row_h_ = 44;
    int header_h_ = 22;
    int dot_ = 7;
    mutable QHash<QString, QString> prompt_lines_;
};

QString rlp_style() {
    return QStringLiteral(
               "#runListPanel{background:%1;}"
               "#runListPanel QLabel{background:transparent;border:none;color:%3;font-size:%9px;}"
               "#runListPanel QLabel#rlpEmptyTitle{color:%2;font-size:%10px;font-weight:700;}"
               "#runListPanel QLabel#rlpError{color:%12;}"
               "#runListPanel QLabel#rlpError[rlpPlain=\"true\"]{color:%2;}"
               "#runListPanel QLineEdit#rlpSearch{background:%4;color:%2;border:1px solid %5;padding:4px 8px;"
               "font-size:%11px;}"
               "#runListPanel QLineEdit#rlpSearch:focus{border:1px solid %6;}"
               "#runListPanel QPushButton{background:%4;color:%3;border:1px solid %5;padding:3px 10px;"
               "font-size:%9px;font-weight:700;}"
               "#runListPanel QPushButton:hover{color:%2;border-color:%6;}"
               "#runListPanel QPushButton:focus{border-color:%7;}"
               "#runListPanel QPushButton:disabled{color:%13;border-color:%5;}"
               "#runListPanel QPushButton#rlpNew{background:%7;color:%8;border-color:%7;}"
               "#runListPanel QPushButton#rlpNew:hover{background:%14;border-color:%14;}"
               "#runListPanel QPushButton#rlpDanger{background:%15;color:%12;border-color:%12;}"
               "#runListPanel QPushButton[rlpChip=\"true\"]{background:transparent;padding:2px 7px;font-size:%16px;}"
               "#runListPanel QPushButton[rlpChip=\"true\"]:checked{background:%7;color:%8;border-color:%7;}"
               "#runListPanel QPushButton[rlpAlert=\"true\"]{color:%17;border-color:%17;}"
               "#runListPanel QPushButton[rlpAlert=\"true\"]:checked{background:%18;color:%17;border-color:%17;}"
               "#runListPanel QToolButton{background:%4;color:%3;border:1px solid %5;padding:2px 6px;font-size:%9px;}"
               "#runListPanel QToolButton:hover{color:%2;border-color:%6;}"
               "#runListPanel QToolButton:focus{border-color:%7;}"
               "#runListPanel QToolButton::menu-indicator{image:none;width:0;}"
               "#runListPanel QComboBox{background:%4;color:%3;border:1px solid %5;padding:2px 6px;font-size:%16px;}"
               "#runListPanel QComboBox:focus{border-color:%7;}"
               "#runListPanel QComboBox QAbstractItemView{background:%1;color:%2;border:1px solid %5;"
               "selection-background-color:%19;}"
               "#runListPanel QFrame#rlpConfirm{background:%15;border:1px solid %12;}"
               "#runListPanel QListView#rlpList{background:%1;border:none;border-top:1px solid %5;outline:none;}"
               "#runListPanel QScrollBar:vertical{background:transparent;width:6px;}"
               "#runListPanel QScrollBar::handle:vertical{background:%5;min-height:20px;}"
               "#runListPanel QScrollBar::add-line:vertical,#runListPanel QScrollBar::sub-line:vertical{height:0;}"
               "#runListPanel QMenu{background:%1;color:%2;border:1px solid %5;padding:4px 0;}"
               "#runListPanel QMenu::item{padding:4px 18px 4px 12px;}"
               "#runListPanel QMenu::item:selected{background:%4;}"
               "#runListPanel QMenu::item:disabled{color:%13;}"
               "#runListPanel QMenu::separator{background:%5;height:1px;margin:4px 8px;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT(), ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(),
             ui::fonts::px(-3), ui::fonts::px(-1), ui::fonts::px(-2), ui::colors::DANGER(), ui::colors::TEXT_DIM(),
             ui::colors::AMBER_DIM(), ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::fonts::px(-4),
             ui::colors::WARNING(), ui::colors::tint(QColor(ui::colors::WARNING()), 0.18), ui::colors::BG_HOVER());
}

} // namespace run_list_panel_detail

using namespace run_list_panel_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct RunListPanel::Impl {
    RunListPanel* q = nullptr;
    RlpModel* model = nullptr;
    RlpProxy* proxy = nullptr;
    RlpDelegate* delegate = nullptr;
    QListView* view = nullptr;
    QStackedWidget* stack = nullptr;
    QLabel* empty_title = nullptr;
    QLabel* empty_text = nullptr;
    QPushButton* empty_btn = nullptr;
    QLineEdit* search = nullptr;
    QTimer* search_timer = nullptr;
    std::array<QPushButton*, 4> chips{};
    QButtonGroup* chip_group = nullptr;
    QComboBox* sort_combo = nullptr;
    QComboBox* engine_combo = nullptr;
    QPushButton* new_btn = nullptr;
    QPushButton* sched_btn = nullptr;
    QToolButton* more_btn = nullptr;
    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QPushButton* confirm_yes = nullptr;
    QPushButton* confirm_no = nullptr;
    std::function<void()> confirm_action;
    QLabel* error = nullptr;
    QTimer* error_timer = nullptr;
    QTimer* tick = nullptr;
    QTimer* counts_timer = nullptr;
    QString selected;
    bool syncing = false; ///< selection changes made by code: emit nothing
    bool relayout_pending = false;
    int not_finished = 0; ///< runs that are not finished (the tick runs only for them)

    void build();
    void wire();
    void retranslate();
    void schedule_counts();
    void update_counts();
    void update_empty_state();
    void update_tick();
    void apply_selection();
    void request_relayout();
    QString current_id() const;
    QStringList selected_finished_ids() const;
    QStringList all_finished_ids() const;

    void on_run_added(const QString& id);
    void on_run_changed(const QString& id);
    void on_runs_reset();

    void load_pinned();
    void save_pinned(const QSet<QString>& pinned);
    void set_pinned(const QString& id, bool on);
    void prune_pinned();

    void show_menu(const QPoint& global, const QString& id);
    void fill_bulk_menu(QMenu* menu);
    void ask(const QString& text, const QString& yes, std::function<void()> on_yes);
    void hide_confirm();
    void show_error(const QString& text);
    /// The line above the list: an error (danger tone) or a plain notice.
    void show_line(const QString& text, bool plain);
    void request_delete(const QStringList& ids);
    void request_cancel(const QString& id);
    void retry(const QString& id, int step);
    void show_all();
};

void RunListPanel::Impl::build() {
    // Stylesheet first, then the children under their final parent.
    q->setObjectName(QStringLiteral("runListPanel"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(rlp_style());

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(8, 8, 8, 0);
    vl->setSpacing(6);

    // The rail is narrow: the buttons wrap at large font sizes instead of clipping.
    auto* actions_host = new QWidget(q);
    auto* actions = new rv::RvFlowLayout(actions_host, 4, 4);
    new_btn = new QPushButton(actions_host);
    new_btn->setObjectName(QStringLiteral("rlpNew"));
    new_btn->setCursor(Qt::PointingHandCursor);
    sched_btn = new QPushButton(actions_host);
    sched_btn->setCursor(Qt::PointingHandCursor);
    more_btn = new QToolButton(actions_host);
    more_btn->setText(QStringLiteral("⋯")); // ⋯
    more_btn->setPopupMode(QToolButton::InstantPopup);
    more_btn->setCursor(Qt::PointingHandCursor);
    auto* more_menu = new QMenu(more_btn);
    more_btn->setMenu(more_menu);
    QObject::connect(more_menu, &QMenu::aboutToShow, q, [this, more_menu]() {
        more_menu->clear();
        fill_bulk_menu(more_menu);
    });
    actions->addWidget(new_btn);
    actions->addWidget(sched_btn);
    actions->addWidget(more_btn);
    vl->addWidget(actions_host);

    search = new QLineEdit(q);
    search->setObjectName(QStringLiteral("rlpSearch"));
    search->setClearButtonEnabled(true);
    vl->addWidget(search);
    search_timer = new QTimer(q);
    search_timer->setSingleShot(true);
    search_timer->setInterval(kRlpSearchDebounceMs);

    // State chips, then the order and the engine: they wrap at large font sizes.
    auto* chip_host = new QWidget(q);
    auto* flow = new rv::RvFlowLayout(chip_host, 4, 4);
    chip_group = new QButtonGroup(q);
    chip_group->setExclusive(true);
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(chip_host);
        b->setCheckable(true);
        b->setProperty("rlpChip", true);
        b->setCursor(Qt::PointingHandCursor);
        chip_group->addButton(b, i);
        flow->addWidget(b);
        chips[static_cast<size_t>(i)] = b;
    }
    chips[0]->setChecked(true);
    sort_combo = new QComboBox(chip_host);
    sort_combo->addItem(QString(), static_cast<int>(rv::RunSort::Recent));
    sort_combo->addItem(QString(), static_cast<int>(rv::RunSort::Status));
    flow->addWidget(sort_combo);
    engine_combo = new QComboBox(chip_host);
    for (RlpEngine e : {RlpEngine::All, RlpEngine::CloudReply, RlpEngine::CloudTask, RlpEngine::Local, RlpEngine::Harness})
        engine_combo->addItem(QString(), static_cast<int>(e));
    flow->addWidget(engine_combo);
    vl->addWidget(chip_host);

    // Inline confirmation (delete / cancel): never a modal dialog.
    confirm = new QFrame(q);
    confirm->setObjectName(QStringLiteral("rlpConfirm"));
    auto* cl = new QVBoxLayout(confirm);
    cl->setContentsMargins(8, 6, 8, 6);
    cl->setSpacing(4);
    confirm_text = new QLabel(confirm);
    confirm_text->setWordWrap(true);
    cl->addWidget(confirm_text);
    auto* cbtns = new QHBoxLayout;
    cbtns->setSpacing(4);
    confirm_yes = new QPushButton(confirm);
    confirm_yes->setObjectName(QStringLiteral("rlpDanger"));
    confirm_no = new QPushButton(confirm);
    cbtns->addWidget(confirm_yes);
    cbtns->addWidget(confirm_no);
    cbtns->addStretch();
    cl->addLayout(cbtns);
    confirm->setVisible(false);
    vl->addWidget(confirm);

    error = new QLabel(q);
    error->setObjectName(QStringLiteral("rlpError"));
    error->setWordWrap(true);
    error->setVisible(false);
    vl->addWidget(error);
    error_timer = new QTimer(q);
    error_timer->setSingleShot(true);
    error_timer->setInterval(8000);

    stack = new QStackedWidget(q);
    view = new QListView(stack);
    view->setObjectName(QStringLiteral("rlpList"));
    // Two row heights (with / without a group header): no uniform sizes, but every
    // size hint is O(1) from cached metrics; long lists are laid out in batches.
    view->setUniformItemSizes(false);
    view->setLayoutMode(QListView::Batched);
    view->setBatchSize(200);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setMouseTracking(true);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    model = new RlpModel(view);
    proxy = new RlpProxy(view);
    proxy->setSourceModel(model);
    proxy->sort(0, Qt::AscendingOrder);
    delegate = new RlpDelegate(proxy, view);
    view->setItemDelegate(delegate);
    view->setModel(proxy);
    stack->addWidget(view);

    auto* empty = new QWidget(stack);
    auto* el = new QVBoxLayout(empty);
    el->setContentsMargins(6, 24, 6, 6);
    el->setSpacing(8);
    empty_title = new QLabel(empty);
    empty_title->setObjectName(QStringLiteral("rlpEmptyTitle"));
    empty_title->setWordWrap(true);
    empty_title->setAlignment(Qt::AlignHCenter);
    empty_text = new QLabel(empty);
    empty_text->setWordWrap(true);
    empty_text->setAlignment(Qt::AlignHCenter);
    empty_btn = new QPushButton(empty);
    empty_btn->setCursor(Qt::PointingHandCursor);
    el->addWidget(empty_title);
    el->addWidget(empty_text);
    el->addWidget(empty_btn, 0, Qt::AlignHCenter);
    el->addStretch();
    stack->addWidget(empty);
    vl->addWidget(stack, 1);

    tick = new QTimer(q);
    tick->setInterval(kRlpTickMs);
    counts_timer = new QTimer(q);
    counts_timer->setSingleShot(true);
    counts_timer->setInterval(0);
}

void RunListPanel::Impl::wire() {
    auto& svc = rlp_svc();
    QObject::connect(new_btn, &QPushButton::clicked, q, &RunListPanel::new_run_requested);
    QObject::connect(sched_btn, &QPushButton::clicked, q, &RunListPanel::schedules_requested);
    QObject::connect(search, &QLineEdit::textChanged, q, [this]() { search_timer->start(); });
    QObject::connect(search_timer, &QTimer::timeout, q, [this]() {
        proxy->set_search(search->text());
        update_empty_state();
        apply_selection();
        request_relayout();
    });
    QObject::connect(chip_group, &QButtonGroup::idClicked, q, [this](int id) {
        proxy->set_filter(static_cast<rv::RunFilter>(id));
        update_empty_state();
        apply_selection();
        request_relayout();
    });
    QObject::connect(sort_combo, &QComboBox::currentIndexChanged, q, [this](int) {
        proxy->set_order(static_cast<rv::RunSort>(sort_combo->currentData().toInt()));
        apply_selection();
        request_relayout();
    });
    QObject::connect(engine_combo, &QComboBox::currentIndexChanged, q, [this](int) {
        proxy->set_engine(static_cast<RlpEngine>(engine_combo->currentData().toInt()));
        update_empty_state();
        apply_selection();
        request_relayout();
    });
    QObject::connect(confirm_no, &QPushButton::clicked, q, [this]() { hide_confirm(); });
    QObject::connect(confirm_yes, &QPushButton::clicked, q, [this]() {
        auto action = std::move(confirm_action);
        hide_confirm();
        if (action)
            action();
    });
    QObject::connect(error_timer, &QTimer::timeout, q, [this]() { error->setVisible(false); });
    QObject::connect(empty_btn, &QPushButton::clicked, q, [this]() {
        if (model->rowCount() == 0) {
            emit q->new_run_requested();
            return;
        }
        show_all(); // "Show all": drop the filters and the search
    });

    QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, q,
                     [this](const QModelIndex& current, const QModelIndex&) {
                         if (syncing || !current.isValid())
                             return;
                         const QString id = current.data(kRlpRunIdRole).toString();
                         if (id.isEmpty() || id == selected)
                             return;
                         selected = id;
                         emit q->run_selected(id);
                     });
    QObject::connect(view, &QListView::doubleClicked, q, [this](const QModelIndex& index) {
        const QString id = index.data(kRlpRunIdRole).toString();
        if (!id.isEmpty())
            emit q->run_activated(id);
    });
    QObject::connect(view, &QListView::customContextMenuRequested, q, [this](const QPoint& pos) {
        const QModelIndex index = view->indexAt(pos);
        show_menu(view->viewport()->mapToGlobal(pos), index.data(kRlpRunIdRole).toString());
    });

    // Keyboard: arrows move (QListView), Enter opens, Delete deletes (ended
    // runs, after the inline confirmation), Menu opens the actions, Esc backs out
    // of the confirmation / search (otherwise it goes on to the screen).
    new rv::RvKeyHook(view, [this](const QKeyEvent* ke, bool apply) {
        switch (ke->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (ke->modifiers() != Qt::NoModifier && ke->modifiers() != Qt::KeypadModifier)
                    return false;
                if (apply && !current_id().isEmpty())
                    emit q->run_activated(current_id());
                return true;
            case Qt::Key_Delete:
                if (apply) {
                    const QStringList ids = selected_finished_ids();
                    if (ids.isEmpty())
                        show_error(RunListPanel::tr("Only ended runs can be deleted. Cancel a run before deleting it."));
                    else
                        request_delete(ids);
                }
                return true;
            case Qt::Key_Menu:
                if (apply) {
                    const QRect r = view->visualRect(view->currentIndex());
                    show_menu(view->viewport()->mapToGlobal(r.isValid() ? r.center() : QPoint(8, 8)), current_id());
                }
                return true;
            case Qt::Key_Escape:
                if (!confirm->isVisible() && search->text().isEmpty())
                    return false;
                if (apply) {
                    if (confirm->isVisible())
                        hide_confirm();
                    else
                        search->clear();
                }
                return true;
            default:
                return false;
        }
    });
    new rv::RvKeyHook(search, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() == Qt::Key_Escape && !search->text().isEmpty()) {
            if (apply)
                search->clear();
            return true;
        }
        if (ke->key() == Qt::Key_Down && ke->type() == QEvent::KeyPress) {
            if (apply)
                q->focus_list();
            return apply;
        }
        return false;
    });
    for (QPushButton* b : {confirm_yes, confirm_no})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                hide_confirm();
            return true;
        });

    // Live updates: single rows, never a rebuild.
    QObject::connect(&svc, &agt::TaskRunService::run_added, q, [this](const QString& id) { on_run_added(id); });
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, [this](const QString& id) { on_run_changed(id); });
    QObject::connect(&svc, &agt::TaskRunService::run_finished, q, [this](const QString& id) { on_run_changed(id); });
    QObject::connect(&svc, &agt::TaskRunService::steps_changed, q, [this](const QString& id) { model->touch(id); });
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() { on_runs_reset(); });
    QObject::connect(&svc, &agt::TaskRunService::attention_changed, q, [this](int, int) { schedule_counts(); });
    QObject::connect(counts_timer, &QTimer::timeout, q, [this]() { update_counts(); });
    QObject::connect(tick, &QTimer::timeout, q, [this]() {
        if (proxy->refresh_today()) // midnight: TODAY becomes YESTERDAY
            request_relayout();
        view->viewport()->update();
    });

    // The panel's sheet follows theme and font through the central re-style pass;
    // the painted rows re-measure once the new appearance has settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() {
        delegate->refresh_fonts();
        request_relayout();
        view->viewport()->update();
    });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange) {
            retranslate();
            model->touch_all();
            view->viewport()->update();
        } else if (t == QEvent::Show) {
            if (proxy->refresh_today())
                request_relayout();
            update_counts();
            update_tick();
            view->viewport()->update();
        } else if (t == QEvent::Hide) {
            tick->stop();
        }
    });
}

void RunListPanel::Impl::retranslate() {
    new_btn->setText(RunListPanel::tr("NEW RUN"));
    new_btn->setToolTip(RunListPanel::tr("Start a long-running task (template, context, engine and budget)"));
    new_btn->setAccessibleName(RunListPanel::tr("New run"));
    sched_btn->setText(RunListPanel::tr("SCHEDULES"));
    sched_btn->setToolTip(RunListPanel::tr("Runs that start on their own, daily or at an interval"));
    sched_btn->setAccessibleName(RunListPanel::tr("Schedules"));
    more_btn->setToolTip(RunListPanel::tr("More actions (delete finished runs)"));
    more_btn->setAccessibleName(RunListPanel::tr("More run actions"));
    search->setPlaceholderText(RunListPanel::tr("Search runs…"));
    search->setAccessibleName(RunListPanel::tr("Search runs by title or task"));
    sort_combo->setItemText(0, RunListPanel::tr("Newest"));
    sort_combo->setItemText(1, RunListPanel::tr("By status"));
    sort_combo->setToolTip(RunListPanel::tr("Order of the list. Pinned runs come first, then the runs that need you."));
    sort_combo->setAccessibleName(RunListPanel::tr("Sort runs"));
    engine_combo->setItemText(0, RunListPanel::tr("All engines"));
    engine_combo->setItemText(1, RunListPanel::tr("Cloud · reply"));
    engine_combo->setItemText(2, RunListPanel::tr("Cloud · task"));
    engine_combo->setItemText(3, RunListPanel::tr("Local"));
    engine_combo->setItemText(4, RunListPanel::tr("Harness"));
    engine_combo->setToolTip(RunListPanel::tr("Show the runs of one engine only"));
    engine_combo->setAccessibleName(RunListPanel::tr("Filter runs by engine"));
    confirm_no->setText(RunListPanel::tr("KEEP"));
    view->setAccessibleName(RunListPanel::tr("Runs"));
    view->setAccessibleDescription(RunListPanel::tr("Grouped by pinned, needs you and date. Enter opens the run's "
                                                    "conversation, Delete deletes ended runs, the context menu has "
                                                    "every action."));
    update_counts();
    update_empty_state();
}

void RunListPanel::Impl::schedule_counts() {
    if (!counts_timer->isActive())
        counts_timer->start();
}

void RunListPanel::Impl::update_counts() {
    auto& svc = rlp_svc();
    std::array<int, 4> n{};
    for (const QString& id : model->ids()) {
        const agt::TaskRun* r = svc.find_run(id);
        if (!r)
            continue;
        ++n[0];
        if (rv::run_matches_filter(r->status, rv::RunFilter::Active))
            ++n[1];
        if (rv::run_matches_filter(r->status, rv::RunFilter::Attention))
            ++n[2];
        if (rv::run_matches_filter(r->status, rv::RunFilter::Finished))
            ++n[3];
    }
    not_finished = n[1];
    const std::array<QString, 4> labels = {RunListPanel::tr("ALL %1"), RunListPanel::tr("ACTIVE %1"),
                                           RunListPanel::tr("NEEDS YOU %1"), RunListPanel::tr("FINISHED %1")};
    const std::array<QString, 4> tips = {
        RunListPanel::tr("Every run"), RunListPanel::tr("Runs that have not ended: queued, running, waiting or paused"),
        RunListPanel::tr("Runs waiting for your answer or approval"),
        RunListPanel::tr("Completed, failed, cancelled, budget-stopped and interrupted runs")};
    for (size_t i = 0; i < chips.size(); ++i) {
        chips[i]->setText(labels[i].arg(n[i]));
        chips[i]->setToolTip(tips[i]);
        chips[i]->setAccessibleName(labels[i].arg(n[i]));
    }
    const bool alert = n[2] > 0;
    if (chips[2]->property("rlpAlert").toBool() != alert) {
        chips[2]->setProperty("rlpAlert", alert);
        chips[2]->style()->unpolish(chips[2]);
        chips[2]->style()->polish(chips[2]);
    }
    update_tick();
    update_empty_state();
}

void RunListPanel::Impl::update_tick() {
    const bool want = q->isVisible() && not_finished > 0;
    if (want && !tick->isActive())
        tick->start();
    else if (!want)
        tick->stop();
}

void RunListPanel::Impl::request_relayout() {
    // One relayout per event-loop turn: a row's group header can appear or go
    // when a neighbour changes group, which the view does not learn by itself.
    if (relayout_pending)
        return;
    relayout_pending = true;
    QTimer::singleShot(0, q, [this]() {
        relayout_pending = false;
        if (proxy->rowCount() > 0)
            emit delegate->sizeHintChanged(proxy->index(0, 0));
    });
}

void RunListPanel::Impl::update_empty_state() {
    auto& svc = rlp_svc();
    const bool rows = proxy->rowCount() > 0;
    stack->setCurrentIndex(rows ? 0 : 1);
    if (rows)
        return;
    if (!svc.is_loaded() && model->rowCount() == 0) {
        empty_title->setText(RunListPanel::tr("Loading runs…"));
        empty_text->clear();
        empty_btn->setVisible(false);
        return;
    }
    empty_btn->setVisible(true);
    if (model->rowCount() == 0) {
        empty_title->setText(RunListPanel::tr("No runs yet"));
        empty_text->setText(RunListPanel::tr("Every message you send in a conversation is a run. Start a longer task "
                                             "with NEW RUN: it keeps going while you work elsewhere, and you can "
                                             "follow, pause or retry it here."));
        empty_btn->setText(RunListPanel::tr("NEW RUN"));
        empty_btn->setToolTip(RunListPanel::tr("Start a long-running task"));
        return;
    }
    empty_title->setText(RunListPanel::tr("Nothing to show"));
    if (!proxy->search().trimmed().isEmpty()) {
        empty_text->setText(RunListPanel::tr("No run matches “%1”.").arg(proxy->search().trimmed()));
    } else if (proxy->engine() != RlpEngine::All) {
        empty_text->setText(RunListPanel::tr("No run of this engine matches the filter."));
    } else {
        switch (proxy->filter()) {
            case rv::RunFilter::Attention:
                empty_text->setText(RunListPanel::tr("No run is waiting for you."));
                break;
            case rv::RunFilter::Active:
                empty_text->setText(RunListPanel::tr("No run is in progress."));
                break;
            case rv::RunFilter::Finished:
                empty_text->setText(RunListPanel::tr("No run has ended yet."));
                break;
            case rv::RunFilter::All:
                empty_text->clear();
                break;
        }
    }
    empty_btn->setText(RunListPanel::tr("SHOW ALL"));
    empty_btn->setToolTip(RunListPanel::tr("Clear the filters and the search"));
}

QString RunListPanel::Impl::current_id() const {
    return view->currentIndex().data(kRlpRunIdRole).toString();
}

QStringList RunListPanel::Impl::selected_finished_ids() const {
    QStringList out;
    auto& svc = rlp_svc();
    for (const QModelIndex& idx : view->selectionModel()->selectedIndexes()) {
        const QString id = idx.data(kRlpRunIdRole).toString();
        const agt::TaskRun* r = svc.find_run(id);
        if (r && agt::is_terminal(r->status) && !out.contains(id))
            out << id;
    }
    return out;
}

QStringList RunListPanel::Impl::all_finished_ids() const {
    QStringList out;
    auto& svc = rlp_svc();
    for (const QString& id : model->ids()) {
        const agt::TaskRun* r = svc.find_run(id);
        if (r && agt::is_terminal(r->status))
            out << id;
    }
    return out;
}

void RunListPanel::Impl::apply_selection() {
    syncing = true;
    const int src_row = model->row_of(selected);
    const QModelIndex idx = src_row >= 0 ? proxy->mapFromSource(model->index(src_row)) : QModelIndex();
    if (idx.isValid()) {
        if (view->currentIndex() != idx)
            view->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
        view->scrollTo(idx);
    } else {
        view->selectionModel()->clearSelection();
        view->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
    syncing = false;
}

void RunListPanel::Impl::show_all() {
    chips[0]->setChecked(true);
    proxy->set_filter(rv::RunFilter::All);
    {
        const QSignalBlocker block(engine_combo);
        engine_combo->setCurrentIndex(0);
    }
    proxy->set_engine(RlpEngine::All);
    search->clear();
    proxy->set_search({});
    update_empty_state();
    apply_selection();
    request_relayout();
}

void RunListPanel::Impl::on_run_added(const QString& id) {
    syncing = true;
    model->add(id);
    syncing = false;
    schedule_counts();
    request_relayout();
}

void RunListPanel::Impl::on_run_changed(const QString& id) {
    if (!rlp_svc().find_run(id)) {
        // Deleted: drop the row. When it was the selected run, the run listed
        // next takes its place (the one above when it was the last), so the
        // inspector never keeps showing a run that is gone; with none left the
        // selection is empty and the inspector shows its empty state.
        const bool was_selected = id == selected;
        QString neighbour;
        if (was_selected) {
            const int src_row = model->row_of(id);
            const QModelIndex at = src_row >= 0 ? proxy->mapFromSource(model->index(src_row)) : QModelIndex();
            if (at.isValid()) {
                for (int r : {at.row() + 1, at.row() - 1}) {
                    const QModelIndex n = proxy->index(r, 0);
                    const QString nid = n.isValid() ? model->id_at(proxy->mapToSource(n).row()) : QString();
                    if (!nid.isEmpty() && rlp_svc().find_run(nid)) {
                        neighbour = nid;
                        break;
                    }
                }
            }
        }
        syncing = true;
        model->remove(id);
        if (was_selected) {
            selected = neighbour;
            if (neighbour.isEmpty()) {
                view->selectionModel()->clearSelection();
                view->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
            }
        }
        syncing = false;
        delegate->forget(id);
        if (proxy->is_pinned(id))
            set_pinned(id, false); // a deleted run is no longer pinned
        if (was_selected) {
            apply_selection();
            emit q->run_selected(neighbour);
        }
        schedule_counts();
        request_relayout();
        return;
    }
    if (!model->contains(id)) {
        on_run_added(id);
        return;
    }
    syncing = true;
    model->touch(id); // the proxy re-filters / re-sorts this row only
    syncing = false;
    schedule_counts();
    request_relayout();
}

void RunListPanel::Impl::on_runs_reset() {
    syncing = true;
    model->merge(rlp_svc().run_ids());
    syncing = false;
    prune_pinned();
    apply_selection();
    update_counts();
    request_relayout();
}

// ── Pinned runs ──────────────────────────────────────────────────────────────

void RunListPanel::Impl::load_pinned() {
    const QStringList ids = QSettings().value(QLatin1String(kRlpPinnedKey)).toStringList();
    proxy->set_pinned(QSet<QString>(ids.cbegin(), ids.cend()));
}

void RunListPanel::Impl::save_pinned(const QSet<QString>& pinned) {
    QStringList ids(pinned.cbegin(), pinned.cend());
    ids.sort();
    QSettings().setValue(QLatin1String(kRlpPinnedKey), ids);
}

void RunListPanel::Impl::set_pinned(const QString& id, bool on) {
    QSet<QString> pinned = proxy->pinned();
    if (on == pinned.contains(id))
        return;
    if (on)
        pinned.insert(id);
    else
        pinned.remove(id);
    save_pinned(pinned);
    proxy->set_pinned(pinned); // re-sorts: the row moves to / out of PINNED
    apply_selection();
    request_relayout();
}

void RunListPanel::Impl::prune_pinned() {
    // Only once the persisted runs are known: before that every id looks deleted.
    auto& svc = rlp_svc();
    if (!svc.is_loaded() || proxy->pinned().isEmpty())
        return;
    QSet<QString> pinned = proxy->pinned();
    bool changed = false;
    for (auto it = pinned.begin(); it != pinned.end();) {
        if (!svc.find_run(*it)) {
            it = pinned.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (!changed)
        return;
    save_pinned(pinned);
    proxy->set_pinned(pinned);
}

// ── Actions ──────────────────────────────────────────────────────────────────

void RunListPanel::Impl::show_error(const QString& text) {
    show_line(text, /*plain=*/false);
}

void RunListPanel::Impl::show_line(const QString& text, bool plain) {
    if (error->property("rlpPlain").toBool() != plain) {
        // The tone is a stylesheet selector ([rlpPlain="true"]): re-polish on a change.
        error->setProperty("rlpPlain", plain);
        error->style()->unpolish(error);
        error->style()->polish(error);
    }
    error->setText(text);
    error->setVisible(!text.isEmpty());
    error_timer->start();
}

void RunListPanel::Impl::ask(const QString& text, const QString& yes, std::function<void()> on_yes) {
    confirm_text->setText(text);
    confirm_yes->setText(yes);
    confirm_action = std::move(on_yes);
    confirm->setVisible(true);
    confirm_yes->setFocus();
}

void RunListPanel::Impl::hide_confirm() {
    confirm->setVisible(false);
    confirm_action = nullptr;
    view->setFocus();
}

void RunListPanel::Impl::request_delete(const QStringList& ids) {
    if (ids.isEmpty())
        return;
    const agt::TaskRun* one = ids.size() == 1 ? rlp_svc().find_run(ids.first()) : nullptr;
    const QString text = one ? RunListPanel::tr("Delete the run “%1” with its activity and files? This "
                                                "cannot be undone. Its conversation is kept.")
                                   .arg(one->title)
                             : RunListPanel::tr("Delete %n ended run(s) with their activity and files? This cannot "
                                                "be undone. Their conversations are kept.",
                                                nullptr, static_cast<int>(ids.size()));
    QPointer<RunListPanel> self(q);
    ask(text, RunListPanel::tr("DELETE"), [this, self, ids]() {
        if (!self)
            return;
        const int n = rlp_svc().delete_runs(ids);
        if (n < ids.size())
            show_error(RunListPanel::tr("%n run(s) could not be deleted: they are still going.", nullptr,
                                        static_cast<int>(ids.size()) - n));
    });
}

void RunListPanel::Impl::request_cancel(const QString& id) {
    const agt::TaskRun* r = rlp_svc().find_run(id);
    if (!r)
        return;
    const bool cloud_turn = r->executor_id() == QLatin1String(agt::kExecutorCloudTurn) && agt::holds_slot(r->status);
    const QString text = cloud_turn ? RunListPanel::tr("Cancel “%1”? The server cannot be stopped: it "
                                                       "finishes this reply and charges it; the terminal stops "
                                                       "following it and picks the stored answer up.")
                                          .arg(r->title)
                                    : RunListPanel::tr("Cancel “%1”? Finished steps are kept; you can retry "
                                                       "it later from any step.")
                                          .arg(r->title);
    QPointer<RunListPanel> self(q);
    ask(text, RunListPanel::tr("CANCEL RUN"), [this, self, id]() {
        if (self && !rlp_svc().cancel_run(id))
            show_error(RunListPanel::tr("The run could not be cancelled: it has already ended."));
    });
}

void RunListPanel::Impl::retry(const QString& id, int step) {
    QString err;
    const QString child = rlp_svc().retry_from_step(id, step, &err);
    if (child.isEmpty()) {
        show_error(err.isEmpty() ? RunListPanel::tr("The run could not be retried.") : err);
        return;
    }
    selected = child;
    apply_selection();
    emit q->run_selected(child);
}

void RunListPanel::Impl::fill_bulk_menu(QMenu* menu) {
    const QStringList sel = selected_finished_ids();
    const QStringList all = all_finished_ids();
    QAction* del_sel = menu->addAction(RunListPanel::tr("Delete selected ended runs (%1)").arg(sel.size()));
    del_sel->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected ended runs
    del_sel->setEnabled(!sel.isEmpty());
    del_sel->setToolTip(sel.isEmpty() ? RunListPanel::tr("Select ended runs first (Ctrl+click / Shift+click)")
                                      : QString());
    QObject::connect(del_sel, &QAction::triggered, q, [this, sel]() { request_delete(sel); });
    QAction* del_all = menu->addAction(RunListPanel::tr("Delete all ended runs (%1)").arg(all.size()));
    del_all->setProperty("fincept_agent_risk", "delete"); // agents: deletes every ended run
    del_all->setEnabled(!all.isEmpty());
    del_all->setToolTip(all.isEmpty() ? RunListPanel::tr("No run has ended yet") : QString());
    QObject::connect(del_all, &QAction::triggered, q, [this, all]() { request_delete(all); });
    menu->setToolTipsVisible(true);
}

void RunListPanel::Impl::show_menu(const QPoint& global, const QString& id) {
    auto* menu = new QMenu(view);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);
    const agt::TaskRun* r = id.isEmpty() ? nullptr : rlp_svc().find_run(id);
    if (r) {
        const rv::RunActions a = rv::run_actions(*r, rv::executor_can_pause(*r));
        auto add = [&](const QString& text, const rv::ActionState& st, std::function<void()> fn) {
            QAction* act = menu->addAction(text);
            act->setEnabled(st.enabled);
            act->setToolTip(st.reason);
            QObject::connect(act, &QAction::triggered, q, std::move(fn));
            return act;
        };
        add(RunListPanel::tr("Open conversation"), a.open, [this, id]() { emit q->run_activated(id); });
        const bool pinned = proxy->is_pinned(id);
        QAction* pin = menu->addAction(pinned ? RunListPanel::tr("Unpin") : RunListPanel::tr("Pin to the top"));
        pin->setToolTip(pinned ? RunListPanel::tr("Back to its date group")
                               : RunListPanel::tr("Keep this run at the top of the list"));
        QObject::connect(pin, &QAction::triggered, q, [this, id, pinned]() { set_pinned(id, !pinned); });
        menu->addSeparator();
        add(RunListPanel::tr("Pause"), a.pause, [this, id]() {
            if (!rlp_svc().pause_run(id))
                show_error(RunListPanel::tr("The run could not be paused: it is not running, or its engine cannot "
                                            "pause it."));
        });
        add(RunListPanel::tr("Resume"), a.resume, [this, id]() {
            QString err;
            if (rlp_svc().resume_run(id, &err).isEmpty())
                show_error(err.isEmpty() ? RunListPanel::tr("The run could not be resumed.") : err);
        });
        add(RunListPanel::tr("Cancel…"), a.cancel, [this, id]() { request_cancel(id); });
        const QVector<agt::RunStep> steps = rlp_svc().steps(id);
        if (steps.size() <= 1 || rv::is_single_reply(*r, steps)) {
            add(RunListPanel::tr("Retry"), a.retry, [this, id]() { retry(id, 0); });
        } else {
            QMenu* sub = menu->addMenu(RunListPanel::tr("Retry from step"));
            sub->setToolTipsVisible(true);
            sub->setEnabled(a.retry.enabled);
            sub->setToolTip(a.retry.reason);
            for (const agt::RunStep& s : steps) {
                QString label = QStringLiteral("%1. %2").arg(s.idx + 1).arg(
                    s.title.isEmpty() ? RunListPanel::tr("Step %1").arg(s.idx + 1) : s.title.left(60));
                label.replace(QLatin1Char('&'), QStringLiteral("&&"));
                const int idx = s.idx;
                QAction* act = sub->addAction(label);
                QObject::connect(act, &QAction::triggered, q, [this, id, idx]() { retry(id, idx); });
            }
        }
        menu->addSeparator();
        add(RunListPanel::tr("Delete…"), a.remove, [this, id]() { request_delete({id}); });
        menu->addSeparator();
    }
    fill_bulk_menu(menu);
    menu->popup(global); // non-blocking
}

// ── RunListPanel ─────────────────────────────────────────────────────────────

RunListPanel::RunListPanel(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->load_pinned();
    d_->wire();
    d_->retranslate();
    d_->on_runs_reset(); // runs already loaded (a second window) are listed at once
}

RunListPanel::~RunListPanel() = default;

void RunListPanel::select_run(const QString& run_id) {
    d_->selected = run_id;
    d_->apply_selection();
}

void RunListPanel::reveal_run(const QString& run_id) {
    const int src_row = d_->model->row_of(run_id);
    if (src_row >= 0 && !d_->proxy->mapFromSource(d_->model->index(src_row)).isValid())
        d_->show_all(); // hidden by a filter or the search: show everything ("SHOW ALL")
    if (d_->error->property("rlpPlain").toBool())
        d_->error->setVisible(false); // an earlier notice is answered
    select_run(run_id);
}

void RunListPanel::show_notice(const QString& text) {
    d_->show_line(text, /*plain=*/true);
}

QString RunListPanel::selected_run() const {
    return d_->selected;
}

void RunListPanel::focus_list() {
    QListView* view = d_->view;
    if (d_->stack->currentIndex() != 0) {
        // Nothing listed: the empty state's button is the next thing to do.
        d_->empty_btn->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (!view->currentIndex().isValid() && d_->proxy->rowCount() > 0) {
        // A current row for the arrows, without picking a run (nothing is emitted).
        d_->syncing = true;
        view->selectionModel()->setCurrentIndex(d_->proxy->index(0, 0), QItemSelectionModel::NoUpdate);
        d_->syncing = false;
    }
    view->setFocus(Qt::OtherFocusReason);
}

void RunListPanel::focus_search() {
    d_->search->setFocus(Qt::ShortcutFocusReason);
    d_->search->selectAll();
}

} // namespace fincept::chat_mode
