// TranscriptView.cpp — the virtualised block-document transcript (see the header).
//
// Layout model: items_[i] has a height (measured when it was realised at the current
// column width, else an estimate from font metrics); tops_[i] is the prefix sum
// (tops_[0] = top margin). Only items within one viewport-half of the visible area
// get a widget (TranscriptBlocks); widgets further than two viewports away are
// destroyed. When a realised item's measured height differs from what the layout
// assumed, the prefix sums are corrected and the scroll position is moved by the same
// amount above an anchor (the first item at the top of the viewport), so what the
// reader looks at stays put. While following the tail the view sticks to the bottom.

#include "screens/chat_mode/TranscriptView.h"

#include "screens/chat_mode/TranscriptBlocks.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QHash>
#include <QKeyEvent>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <vector>

namespace fincept::chat_mode {

namespace tr_ = fincept::chat_mode::transcript;
namespace al = fincept::agent_tasks::artefact_logic;

namespace transcript_view_detail {

QString ctv_tr(const char* text) {
    return QCoreApplication::translate("fincept::chat_mode::TranscriptView", text);
}

} // namespace transcript_view_detail

using namespace transcript_view_detail;

struct TranscriptView::Impl {
    TranscriptView* q = nullptr;
    std::vector<tr_::ItemPtr> items;
    QHash<quint64, int> index_of;
    std::vector<int> tops{0};
    int dirty_from = 0; ///< tops_[k] valid for k <= dirty_from
    QVector<tr_::Turn> turns;
    QVector<quint64> turn_header; ///< turn -> its header item (0: none yet)
    quint64 next_id = 1;
    QHash<quint64, tr_::Block*> realized;
    tr_::BlockHost host;
    int col_x = 16;
    int col_w = 600;
    int top_margin = 8;
    int bottom_margin = 24;
    bool following = true;
    bool unseen = false;
    bool in_layout = false;
    bool approvals = true;
    int hovered_turn = -1;
    QPushButton* pill = nullptr;
    QTimer* timer = nullptr;

    // ── geometry ──
    int used_height(int i) const {
        tr_::Item& it = *items[static_cast<size_t>(i)];
        if (it.hidden)
            return 0;
        if (it.height >= 0 && it.measured_width == col_w)
            return it.height;
        return tr_::estimate_height(it, col_w);
    }

    void ensure_tops() {
        const int n = static_cast<int>(items.size());
        if (static_cast<int>(tops.size()) != n + 1)
            tops.resize(static_cast<size_t>(n) + 1);
        tops[0] = top_margin;
        dirty_from = std::clamp(dirty_from, 0, n);
        for (int k = dirty_from; k < n; ++k)
            tops[static_cast<size_t>(k) + 1] = tops[static_cast<size_t>(k)] + used_height(k);
        dirty_from = n;
    }

    void invalidate_from(int i) { dirty_from = std::min(dirty_from, std::max(0, i)); }

    int total_height() const { return tops.back() + bottom_margin; }

    /// The item whose box contains y (the last one when y is beyond the end); -1 when empty.
    int index_at(int y) const {
        if (items.empty())
            return -1;
        const auto it = std::upper_bound(tops.begin() + 1, tops.end(), y);
        const int i = static_cast<int>(it - (tops.begin() + 1));
        return std::clamp(i, 0, static_cast<int>(items.size()) - 1);
    }

    void update_column() {
        const int vw = q->viewport()->width();
        const QFontMetrics fm(ui::fonts::font(0));
        const int side = std::max(16, fm.averageCharWidth() * 2);
        const int max_w = std::max(720, fm.averageCharWidth() * 140);
        const int w = std::max(160, std::min(vw - 2 * side, max_w));
        const int x = std::max(0, (vw - w) / 2);
        top_margin = std::max(8, fm.lineSpacing() / 2);
        bottom_margin = std::max(24, fm.lineSpacing() * 2);
        if (w != col_w) {
            col_w = w;
            invalidate_from(0);
        }
        col_x = x;
    }

    void set_range() {
        QScrollBar* bar = q->verticalScrollBar();
        const int vh = q->viewport()->height();
        const int max = std::max(0, total_height() - vh);
        if (bar->maximum() != max || bar->minimum() != 0)
            bar->setRange(0, max);
        bar->setPageStep(std::max(1, vh));
        bar->setSingleStep(std::max(8, QFontMetrics(ui::fonts::font(0)).lineSpacing()));
    }

    // ── realisation ──
    tr_::Block* realize(int i, int v) {
        const tr_::ItemPtr& it = items[static_cast<size_t>(i)];
        if (tr_::Block* b = realized.value(it->id, nullptr))
            return b;
        tr_::Block* b = tr_::create_block(it, &host, q->viewport());
        b->installEventFilter(q);
        b->setGeometry(col_x, tops[static_cast<size_t>(i)] - v, col_w, std::max(1, used_height(i)));
        b->show();
        b->set_hot(it->turn >= 0 && it->turn == hovered_turn);
        realized.insert(it->id, b);
        return b;
    }

    void unrealize(quint64 id) {
        tr_::Block* b = realized.take(id);
        if (!b)
            return;
        b->removeEventFilter(q);
        b->hide();
        b->deleteLater(); // never synchronously: we may be inside one of its own signals
    }

    void schedule() {
        if (!timer->isActive())
            timer->start();
    }

    void layout_now() {
        timer->stop();
        if (in_layout)
            return;
        in_layout = true;
        update_column();
        QScrollBar* bar = q->verticalScrollBar();
        const int vh = std::max(1, q->viewport()->height());
        ensure_tops();
        int v = bar->value();
        int anchor = -1;
        int anchor_off = 0;
        if (!following && !items.empty()) {
            anchor = index_at(v);
            anchor_off = tops[static_cast<size_t>(anchor)] - v;
        }
        const int n = static_cast<int>(items.size());
        for (int pass = 0; pass < 5; ++pass) {
            ensure_tops();
            set_range();
            if (following)
                v = bar->maximum();
            else if (anchor >= 0)
                v = std::clamp(tops[static_cast<size_t>(anchor)] - anchor_off, 0, bar->maximum());
            else
                v = std::min(v, bar->maximum());
            if (bar->value() != v)
                bar->setValue(v);
            bool changed = false;
            const int over = vh / 2;
            for (int i = n > 0 ? index_at(std::max(0, v - over)) : 0; i >= 0 && i < n; ++i) {
                if (tops[static_cast<size_t>(i)] >= v + vh + over)
                    break;
                tr_::Item& it = *items[static_cast<size_t>(i)];
                if (it.hidden)
                    continue;
                tr_::Block* b = realize(i, v);
                if (it.height >= 0 && it.measured_width == col_w)
                    continue;
                const int before = used_height(i);
                if (b->width() != col_w)
                    b->resize(col_w, std::max(1, b->height()));
                const int h = std::max(1, b->height_for(col_w));
                it.height = h;
                it.measured_width = col_w;
                if (h != before) {
                    invalidate_from(i);
                    changed = true;
                }
            }
            if (!changed)
                break;
        }
        ensure_tops();
        set_range();
        if (following && bar->value() != bar->maximum())
            bar->setValue(bar->maximum());
        v = bar->value();
        // Place what stays, drop what is far away.
        const int keep_top = v - vh * 2;
        const int keep_bottom = v + vh * 3;
        QVector<quint64> drop;
        for (auto rit = realized.cbegin(); rit != realized.cend(); ++rit) {
            const int i = index_of.value(rit.key(), -1);
            if (i < 0) {
                drop << rit.key();
                continue;
            }
            const tr_::Item& it = *items[static_cast<size_t>(i)];
            const int top = tops[static_cast<size_t>(i)];
            const int h = used_height(i);
            if (it.hidden || top + h < keep_top || top > keep_bottom) {
                drop << rit.key();
                continue;
            }
            const QRect r(col_x, top - v, col_w, std::max(1, h));
            if (rit.value()->geometry() != r)
                rit.value()->setGeometry(r);
        }
        for (quint64 id : std::as_const(drop))
            unrealize(id);
        place_pill();
        in_layout = false;
    }

    void place_pill() {
        const bool show = unseen && !following;
        if (show) {
            const QSize s = pill->sizeHint();
            pill->setGeometry((q->viewport()->width() - s.width()) / 2, q->viewport()->height() - s.height() - 12, s.width(),
                              s.height());
            pill->raise();
        }
        if (pill->isVisibleTo(q->viewport()) != show)
            pill->setVisible(show);
    }

    void reindex_from(int k) {
        for (int j = std::max(0, k); j < static_cast<int>(items.size()); ++j)
            index_of.insert(items[static_cast<size_t>(j)]->id, j);
    }

    quint64 insert_at(int pos, const tr_::ItemPtr& item) {
        item->id = next_id++;
        item->height = -1;
        item->measured_width = -1;
        pos = std::clamp(pos, 0, static_cast<int>(items.size()));
        items.insert(items.begin() + pos, item);
        tops.resize(items.size() + 1);
        if (pos == static_cast<int>(items.size()) - 1)
            index_of.insert(item->id, pos);
        else
            reindex_from(pos);
        invalidate_from(pos);
        if (!following)
            unseen = true;
        schedule();
        return item->id;
    }

    void set_hovered(int turn) {
        if (turn == hovered_turn)
            return;
        const int old = hovered_turn;
        hovered_turn = turn;
        for (int t : {old, turn}) {
            if (t < 0 || t >= turn_header.size())
                continue;
            if (tr_::Block* b = realized.value(turn_header.at(t), nullptr))
                b->set_hot(t == turn);
        }
    }

    void open_link(const QString& url) {
        const QString scheme_prefix = QLatin1String(md::kSymbolScheme) + QLatin1Char(':');
        if (url.startsWith(scheme_prefix)) {
            const QString sym = url.mid(scheme_prefix.size()).trimmed();
            if (!sym.isEmpty())
                emit q->action_requested(tr_::Action{QStringLiteral("symbol"), 0, -1, sym});
            return;
        }
        const QUrl u(url, QUrl::StrictMode);
        if (!md::link_allowed(u))
            return; // only http / https / mailto ever leave the terminal
        QDesktopServices::openUrl(u);
    }

    void classify(quint64 id) {
        const tr_::ItemPtr it = q->item_ptr(id);
        if (!it)
            return;
        if (!it->result)
            it->result = std::make_shared<tr_::ToolResultCache>();
        if (it->result->ready || it->result->busy)
            return;
        it->result->busy = true;
        const QJsonValue full = it->data.value(QStringLiteral("result"));
        const QString preview = it->data.value(QStringLiteral("preview")).toString();
        const bool ok = it->state == 1;
        const std::weak_ptr<tr_::Item> weak = it;
        const QPointer<TranscriptView> guard(q);
        // A 50 000-row result is turned into a table here, never on the GUI thread.
        QThreadPool::globalInstance()->start([weak, guard, full, preview, ok]() {
            const al::ToolResultView view = al::classify_tool_result(full, preview, ok);
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [weak, guard, view]() {
                    const tr_::ItemPtr item = weak.lock();
                    if (!item || !item->result)
                        return;
                    item->result->view = view;
                    item->result->ready = true;
                    item->result->busy = false;
                    if (guard)
                        guard->changed(item->id);
                },
                Qt::QueuedConnection);
        });
    }
};

TranscriptView::TranscriptView(QWidget* parent) : QAbstractScrollArea(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    setObjectName(QStringLiteral("chatTranscript"));
    viewport()->setObjectName(QStringLiteral("chatTranscriptViewport"));
    // The one stylesheet, before any child exists.
    setStyleSheet(transcript::transcript_sheet());
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(ctv_tr("Conversation"));
    setAccessibleDescription(ctv_tr("The conversation as a document. Up / Down / Page Up / Page Down scroll, End jumps "
                                    "to the newest message; Tab reaches the actions of the messages on screen."));
    viewport()->installEventFilter(this);

    d_->pill = new QPushButton(ctv_tr("↓ NEW"), viewport());
    d_->pill->setObjectName(QStringLiteral("ctbNewPill"));
    d_->pill->setCursor(Qt::PointingHandCursor);
    d_->pill->setToolTip(ctv_tr("New output below — jump to the newest and follow it"));
    d_->pill->setAccessibleName(ctv_tr("Jump to the newest output"));
    d_->pill->setVisible(false);
    connect(d_->pill, &QPushButton::clicked, this, [this]() { scroll_to_bottom(); });

    d_->timer = new QTimer(this);
    d_->timer->setSingleShot(true);
    d_->timer->setInterval(0);
    connect(d_->timer, &QTimer::timeout, this, [this]() { d_->layout_now(); });

    Impl* d = d_.get();
    d->host.act = [this](const tr_::Action& a) { emit action_requested(a); };
    d->host.relayout = [d](quint64 id) {
        const int i = d->index_of.value(id, -1);
        if (i < 0)
            return;
        d->items[static_cast<size_t>(i)]->height = -1;
        d->invalidate_from(i);
        d->schedule();
    };
    d->host.toggle_group = [d](quint64 id) {
        const int i = d->index_of.value(id, -1);
        if (i < 0)
            return;
        tr_::Item& g = *d->items[static_cast<size_t>(i)];
        g.expanded = !g.expanded;
        int first = static_cast<int>(d->items.size());
        for (quint64 child : std::as_const(g.children)) {
            const int ci = d->index_of.value(child, -1);
            if (ci < 0)
                continue;
            d->items[static_cast<size_t>(ci)]->hidden = !g.expanded;
            first = std::min(first, ci);
        }
        g.height = -1;
        d->invalidate_from(std::min(first, i));
        d->schedule();
    };
    d->host.classify = [d](quint64 id) { d->classify(id); };
    d->host.turn = [d](int t) -> const tr_::Turn* {
        return t >= 0 && t < d->turns.size() ? &d->turns.at(t) : nullptr;
    };
    d->host.approvals_enabled = [d]() { return d->approvals; };
    d->host.hovered_turn = [d]() { return d->hovered_turn; };
    d->host.open_link = [d](const QString& url) { d->open_link(url); };

    // Fonts and colours changed: rich text is rebuilt and every height measured again.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        transcript::reset_metrics();
        for (const tr_::ItemPtr& it : d_->items)
            it->height = -1;
        for (tr_::Block* b : std::as_const(d_->realized))
            b->restyle();
        d_->invalidate_from(0);
        d_->schedule();
    });
}

TranscriptView::~TranscriptView() = default;

// ── Turns ────────────────────────────────────────────────────────────────────

int TranscriptView::add_turn(const transcript::Turn& turn) {
    const int index = static_cast<int>(d_->turns.size());
    d_->turns.append(turn);
    auto header = std::make_shared<tr_::Item>();
    header->kind = tr_::ItemKind::Header;
    header->turn = index;
    d_->turn_header.append(append(header));
    return index;
}

transcript::Turn* TranscriptView::turn(int index) {
    return index >= 0 && index < d_->turns.size() ? &d_->turns[index] : nullptr;
}

int TranscriptView::turn_count() const {
    return static_cast<int>(d_->turns.size());
}

void TranscriptView::update_turn(int index) {
    if (index >= 0 && index < d_->turn_header.size() && d_->turn_header.at(index) != 0)
        changed(d_->turn_header.at(index));
}

// ── Items ────────────────────────────────────────────────────────────────────

quint64 TranscriptView::append(const transcript::ItemPtr& item) {
    return d_->insert_at(static_cast<int>(d_->items.size()), item);
}

quint64 TranscriptView::insert_before(quint64 before, const transcript::ItemPtr& item) {
    const int i = d_->index_of.value(before, -1);
    return d_->insert_at(i < 0 ? static_cast<int>(d_->items.size()) : i, item);
}

void TranscriptView::append_history(const QVector<transcript::Turn>& turns, const QVector<transcript::ItemPtr>& items) {
    const int offset = static_cast<int>(d_->turns.size());
    for (const tr_::Turn& t : turns) {
        d_->turns.append(t);
        d_->turn_header.append(0);
    }
    const int start = static_cast<int>(d_->items.size());
    d_->items.reserve(d_->items.size() + static_cast<size_t>(items.size()) + static_cast<size_t>(turns.size()));
    int last_turn = -1;
    const auto push = [this](const tr_::ItemPtr& it) {
        it->id = d_->next_id++;
        it->height = -1;
        it->measured_width = -1;
        d_->items.push_back(it);
        d_->index_of.insert(it->id, static_cast<int>(d_->items.size()) - 1);
    };
    for (const tr_::ItemPtr& it : items) {
        if (it->turn >= 0) {
            it->turn += offset;
            if (it->turn != last_turn && d_->turn_header.value(it->turn, 1) == 0) {
                auto header = std::make_shared<tr_::Item>();
                header->kind = tr_::ItemKind::Header;
                header->turn = it->turn;
                push(header);
                d_->turn_header[it->turn] = header->id;
            }
            last_turn = it->turn;
        }
        push(it);
    }
    d_->tops.resize(d_->items.size() + 1);
    d_->invalidate_from(start);
    d_->schedule();
}

transcript::Item* TranscriptView::item(quint64 id) const {
    const int i = d_->index_of.value(id, -1);
    return i < 0 ? nullptr : d_->items[static_cast<size_t>(i)].get();
}

transcript::ItemPtr TranscriptView::item_ptr(quint64 id) const {
    const int i = d_->index_of.value(id, -1);
    return i < 0 ? nullptr : d_->items[static_cast<size_t>(i)];
}

void TranscriptView::changed(quint64 id) {
    const int i = d_->index_of.value(id, -1);
    if (i < 0)
        return;
    tr_::Item& it = *d_->items[static_cast<size_t>(i)];
    it.height = -1;
    it.est_chars = -1;
    if (tr_::Block* b = d_->realized.value(id, nullptr))
        b->sync();
    d_->invalidate_from(i);
    d_->schedule();
}

void TranscriptView::remove(quint64 id) {
    const int i = d_->index_of.value(id, -1);
    if (i < 0)
        return;
    d_->unrealize(id);
    d_->index_of.remove(id);
    d_->items.erase(d_->items.begin() + i);
    d_->tops.resize(d_->items.size() + 1);
    d_->reindex_from(i);
    d_->invalidate_from(i);
    d_->schedule();
}

void TranscriptView::clear() {
    const QList<quint64> ids = d_->realized.keys();
    for (quint64 id : ids)
        d_->unrealize(id);
    d_->items.clear();
    d_->index_of.clear();
    d_->tops.assign(1, d_->top_margin);
    d_->dirty_from = 0;
    d_->turns.clear();
    d_->turn_header.clear();
    d_->hovered_turn = -1;
    d_->following = true;
    d_->unseen = false;
    d_->schedule();
}

bool TranscriptView::is_empty() const {
    return d_->items.empty();
}

QString TranscriptView::to_markdown() const {
    QStringList out;
    for (const tr_::Turn& t : d_->turns) {
        if (t.text.trimmed().isEmpty())
            continue;
        const QString who = t.role == QLatin1String("user") ? ctv_tr("You") : ctv_tr("Agent");
        const QString when = t.when.isValid() ? QLocale().toString(t.when.toLocalTime(), QLocale::ShortFormat) : QString();
        out << QStringLiteral("### %1%2").arg(who, when.isEmpty() ? QString() : QStringLiteral(" · ") + when);
        out << (t.role == QLatin1String("user") ? QStringLiteral("> ") + QString(t.text).replace(QLatin1Char('\n'), QStringLiteral("\n> "))
                                                : t.text);
    }
    return out.join(QStringLiteral("\n\n")) + QLatin1Char('\n');
}

// ── Approvals ────────────────────────────────────────────────────────────────

void TranscriptView::set_approvals_enabled(bool enabled) {
    if (d_->approvals == enabled)
        return;
    d_->approvals = enabled;
    for (tr_::Block* b : std::as_const(d_->realized))
        if (b->item()->kind == tr_::ItemKind::Approval)
            b->sync();
}

bool TranscriptView::approvals_enabled() const {
    return d_->approvals;
}

// ── Scrolling ────────────────────────────────────────────────────────────────

bool TranscriptView::following() const {
    return d_->following;
}

void TranscriptView::scroll_to_bottom() {
    d_->following = true;
    d_->unseen = false;
    d_->layout_now();
    emit scroll_changed();
}

int TranscriptView::scroll_value() const {
    return verticalScrollBar()->value();
}

void TranscriptView::set_scroll_value(int value) {
    d_->layout_now();
    QScrollBar* bar = verticalScrollBar();
    d_->following = value >= bar->maximum();
    bar->setValue(std::clamp(value, 0, bar->maximum()));
    d_->layout_now();
}

void TranscriptView::focus_document() {
    setFocus(Qt::ShortcutFocusReason);
}

// ── Events ───────────────────────────────────────────────────────────────────

void TranscriptView::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    d_->layout_now();
}

void TranscriptView::scrollContentsBy(int, int) {
    if (d_->in_layout)
        return;
    QScrollBar* bar = verticalScrollBar();
    const int slack = std::max(4, QFontMetrics(ui::fonts::font(0)).lineSpacing() / 2);
    d_->following = bar->value() >= bar->maximum() - slack;
    if (d_->following)
        d_->unseen = false;
    d_->layout_now();
    emit scroll_changed();
}

void TranscriptView::showEvent(QShowEvent* e) {
    QAbstractScrollArea::showEvent(e);
    d_->layout_now();
}

void TranscriptView::changeEvent(QEvent* e) {
    QAbstractScrollArea::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        d_->pill->setText(ctv_tr("↓ NEW"));
        for (tr_::Block* b : std::as_const(d_->realized))
            b->sync();
    }
}

void TranscriptView::keyPressEvent(QKeyEvent* e) {
    QScrollBar* bar = verticalScrollBar();
    const int line = bar->singleStep() * 3;
    switch (e->key()) {
        case Qt::Key_Up:
            bar->setValue(bar->value() - line);
            return;
        case Qt::Key_Down:
            bar->setValue(bar->value() + line);
            return;
        case Qt::Key_PageUp:
            bar->setValue(bar->value() - bar->pageStep() * 9 / 10);
            return;
        case Qt::Key_PageDown:
            bar->setValue(bar->value() + bar->pageStep() * 9 / 10);
            return;
        case Qt::Key_Home:
            bar->setValue(0);
            return;
        case Qt::Key_End:
            scroll_to_bottom();
            return;
        default:
            break;
    }
    QAbstractScrollArea::keyPressEvent(e);
}

bool TranscriptView::eventFilter(QObject* watched, QEvent* e) {
    if (watched == viewport()) {
        if (e->type() == QEvent::Leave)
            d_->set_hovered(-1);
    } else if (e->type() == QEvent::Enter) {
        if (auto* b = dynamic_cast<tr_::Block*>(watched))
            d_->set_hovered(b->item()->turn);
    }
    return QAbstractScrollArea::eventFilter(watched, e);
}

} // namespace fincept::chat_mode
