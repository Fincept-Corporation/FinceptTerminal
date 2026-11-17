// AiChatSessionRail.cpp — see AiChatSessionRail.h.
//
//   AicSessionModel     every session, and the visible rows: group headers, sessions, an empty line
//   AicSessionDelegate  paints a row in two lines; the hover glyphs (pin / rename / delete) are
//                       hit-tested in editorEvent
//   AicSessionListView  keys: Enter opens, F2 renames, Del deletes, Up / Down skip the headers
//   AicSessionRail      header strip, search, list, footer
//
// Rows are painted, never widgets: 2,000 sessions cost 2,000 small structs.

#include "screens/ai_chat/AiChatSessionRail.h"

#include "core/agent/AgentRiskMarkers.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QAbstractItemView>
#include <QAbstractListModel>
#include <QCursor>
#include <QDate>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygon>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <initializer_list>

namespace fincept::screens::aichat {

namespace {

constexpr int kSrGlyphPin = 0;
constexpr int kSrGlyphRename = 1;
constexpr int kSrGlyphDelete = 2;

QString sr_wait_tip() {
    return AicSessionRail::tr("Wait for the answer to finish (or stop it) to switch sessions");
}

/// "GPT-4O", "SONNET", "FINCEPT" — the short model chip of a row.
QString sr_model_label(const SessionSummary& s) {
    QString m = s.model.trimmed();
    if (m.isEmpty()) {
        m = s.provider.trimmed();
        if (m.startsWith(QLatin1String("harness:"), Qt::CaseInsensitive))
            m = m.mid(8);
    }
    const int slash = int(m.lastIndexOf(QLatin1Char('/')));
    if (slash >= 0)
        m = m.mid(slash + 1);
    return m.toUpper();
}

/// "openai / gpt-4o" — the long form for tooltips.
QString sr_model_long(const SessionSummary& s) {
    if (s.provider.isEmpty())
        return s.model;
    if (s.model.isEmpty())
        return s.provider;
    return s.provider + QStringLiteral(" / ") + s.model;
}

/// Outside text (titles, snippets) in a tooltip: escaped rich text, so markup is never interpreted.
QString sr_tip(const QStringList& lines) {
    QStringList esc;
    esc.reserve(lines.size());
    for (const QString& l : lines)
        esc.append(l.toHtmlEscaped());
    return QStringLiteral("<p>") + esc.join(QStringLiteral("<br>")) + QStringLiteral("</p>");
}

QString sr_tooltip(const SessionSummary& s) {
    QStringList lines{s.title};
    if (!s.snippet.isEmpty())
        lines.append(s.snippet);
    QStringList meta;
    if (s.updated.isValid())
        meta.append(s.updated.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    meta.append(AicSessionRail::tr("%n message(s)", "", s.messages));
    const QString model = sr_model_long(s);
    if (!model.isEmpty())
        meta.append(model);
    lines.append(meta.join(QStringLiteral(" · ")));
    return sr_tip(lines);
}

/// Plain text for screen readers (the row's title is its accessible text).
QString sr_description(const SessionSummary& s) {
    QStringList parts;
    if (s.running)
        parts.append(AicSessionRail::tr("answering"));
    else if (s.unread)
        parts.append(AicSessionRail::tr("new answer"));
    if (s.pinned)
        parts.append(AicSessionRail::tr("pinned"));
    if (s.updated.isValid())
        parts.append(s.updated.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    parts.append(AicSessionRail::tr("%n message(s)", "", s.messages));
    const QString model = sr_model_long(s);
    if (!model.isEmpty())
        parts.append(model);
    if (!s.snippet.isEmpty())
        parts.append(s.snippet);
    return parts.join(QStringLiteral(" · "));
}

} // namespace

// ── Model ────────────────────────────────────────────────────────────────────

class AicSessionModel : public QAbstractListModel {
  public:
    enum class Kind { Header, Session, Empty };
    static constexpr int kIdRole = Qt::UserRole + 1;

    explicit AicSessionModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : int(rows_.size());
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        return is_session(index.row()) ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
    }

    QVariant data(const QModelIndex& index, int role) const override {
        const int r = index.row();
        if (!index.isValid() || r < 0 || r >= int(rows_.size()))
            return {};
        const Row& row = rows_.at(r);
        if (row.kind == Kind::Header) {
            if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole)
                return date_group_title(row.group);
            return {};
        }
        if (row.kind == Kind::Empty) {
            if (role == Qt::DisplayRole || role == Qt::AccessibleTextRole)
                return all_.isEmpty() ? AicSessionRail::tr("No conversations yet")
                                      : AicSessionRail::tr("No session matches the search");
            return {};
        }
        const SessionSummary& s = all_.at(row.index);
        switch (role) {
            case Qt::DisplayRole:
            case Qt::AccessibleTextRole:
                return s.title;
            case Qt::ToolTipRole:
                return interactive_ ? sr_tooltip(s) : sr_wait_tip();
            case Qt::AccessibleDescriptionRole:
                return sr_description(s);
            case kIdRole:
                return s.id;
            default:
                return {};
        }
    }

    void set_sessions(const QVector<SessionSummary>& sessions) {
        all_ = sessions;
        const int n = int(all_.size());
        hay_.resize(n);
        group_.resize(n);
        order_.resize(n);
        pos_.clear();
        pos_.reserve(n);
        const QDate today = QDate::currentDate();
        for (int i = 0; i < n; ++i) {
            const SessionSummary& s = all_.at(i);
            hay_[i] = (s.title + QLatin1Char(' ') + s.snippet).toCaseFolded();
            group_[i] = s.pinned ? DateGroup::Pinned : date_group(s.updated, today);
            order_[i] = i;
            pos_.insert(s.id, i);
        }
        std::stable_sort(order_.begin(), order_.end(), [this](int a, int b) {
            if (group_.at(a) != group_.at(b))
                return int(group_.at(a)) < int(group_.at(b));
            return all_.at(a).updated > all_.at(b).updated;
        });
        rebuild();
    }

    /// Every word must appear in the title or the snippet (case-insensitive).
    void set_filter(const QString& text) {
        const QStringList words = text.simplified().toCaseFolded().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words == words_)
            return;
        words_ = words;
        rebuild();
    }
    bool has_filter() const { return !words_.isEmpty(); }

    void set_active(const QString& id) {
        if (id == active_)
            return;
        const QString old = active_;
        active_ = id;
        touch(old);
        touch(id);
    }
    const QString& active() const { return active_; }
    void set_interactive(bool on) { interactive_ = on; }
    bool interactive() const { return interactive_; }

    bool set_running(const QString& id, bool on) {
        const int i = pos_.value(id, -1);
        if (i < 0 || all_.at(i).running == on)
            return false;
        all_[i].running = on;
        touch(id);
        return true;
    }
    bool set_unread(const QString& id, bool on) {
        const int i = pos_.value(id, -1);
        if (i < 0 || all_.at(i).unread == on)
            return false;
        all_[i].unread = on;
        touch(id);
        return true;
    }
    /// True when the visible rows were rebuilt (a search is active).
    bool set_snippet(const QString& id, const QString& snippet) {
        const int i = pos_.value(id, -1);
        if (i < 0 || all_.at(i).snippet == snippet)
            return false;
        all_[i].snippet = snippet;
        hay_[i] = (all_.at(i).title + QLatin1Char(' ') + snippet).toCaseFolded();
        if (!words_.isEmpty()) {
            rebuild();
            return true;
        }
        touch(id);
        return false;
    }

    Kind kind_at(int row) const {
        return row >= 0 && row < int(rows_.size()) ? rows_.at(row).kind : Kind::Empty;
    }
    bool is_session(int row) const { return kind_at(row) == Kind::Session; }
    DateGroup group_at(int row) const {
        return row >= 0 && row < int(rows_.size()) ? rows_.at(row).group : DateGroup::Older;
    }
    const SessionSummary* session_at(int row) const {
        return is_session(row) ? &all_.at(rows_.at(row).index) : nullptr;
    }
    const SessionSummary* find(const QString& id) const {
        const int i = id.isEmpty() ? -1 : pos_.value(id, -1);
        return i >= 0 ? &all_.at(i) : nullptr;
    }
    QString id_at(int row) const {
        const SessionSummary* s = session_at(row);
        return s ? s->id : QString();
    }
    int row_of(const QString& id) const { return row_of_.value(id, -1); }
    int first_session_row() const {
        for (int r = 0; r < int(rows_.size()); ++r)
            if (rows_.at(r).kind == Kind::Session)
                return r;
        return -1;
    }
    int total() const { return int(all_.size()); }
    int shown() const { return shown_; }

  private:
    struct Row {
        Kind kind = Kind::Empty;
        DateGroup group = DateGroup::Older;
        int index = -1; ///< into all_ (sessions)
    };

    bool matches(int i) const {
        const QString& hay = hay_.at(i);
        for (const QString& w : words_)
            if (!hay.contains(w))
                return false;
        return true;
    }

    void rebuild() {
        beginResetModel();
        rows_.clear();
        row_of_.clear();
        shown_ = 0;
        int last_group = -1;
        for (const int i : order_) {
            if (!words_.isEmpty() && !matches(i))
                continue;
            const int g = int(group_.at(i));
            if (g != last_group) {
                rows_.append(Row{Kind::Header, group_.at(i), -1});
                last_group = g;
            }
            row_of_.insert(all_.at(i).id, int(rows_.size()));
            rows_.append(Row{Kind::Session, group_.at(i), i});
            ++shown_;
        }
        if (rows_.isEmpty())
            rows_.append(Row{Kind::Empty, DateGroup::Older, -1});
        endResetModel();
    }

    void touch(const QString& id) {
        const int r = id.isEmpty() ? -1 : row_of(id);
        if (r < 0)
            return;
        const QModelIndex mi = index(r, 0);
        emit dataChanged(mi, mi);
    }

    QVector<SessionSummary> all_;
    QVector<QString> hay_;      ///< case-folded "title snippet" per session
    QVector<DateGroup> group_;  ///< per session
    QVector<int> order_;        ///< indices into all_: group order, newest first
    QHash<QString, int> pos_;   ///< id → index into all_
    QVector<Row> rows_;
    QHash<QString, int> row_of_; ///< id → visible row
    QStringList words_;
    QString active_;
    bool interactive_ = true;
    int shown_ = 0;
};

namespace {

// ── Row painter ──────────────────────────────────────────────────────────────

class AicSessionDelegate : public QStyledItemDelegate {
  public:
    AicSessionDelegate(AicSessionModel* model, QObject* parent) : QStyledItemDelegate(parent), model_(model) {}

    /// A hover glyph was clicked (kSrGlyph*, session id). Called inside the view's mouse handler.
    std::function<void(int, const QString&)> on_glyph;

    void invalidate() { metrics_ok_ = false; }
    /// True once after a glyph consumed a click (the view still emits clicked()).
    bool take_click_suppression() {
        const bool s = suppress_;
        suppress_ = false;
        return s;
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
        const Metrics& mm = metrics();
        switch (model_->kind_at(index.row())) {
            case AicSessionModel::Kind::Header:
                return QSize(style::u(10), mm.h_head);
            case AicSessionModel::Kind::Empty:
                return QSize(style::u(10), mm.h_empty);
            case AicSessionModel::Kind::Session:
                break;
        }
        return QSize(style::u(10), mm.h_row);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override {
        const int row = index.row();
        p->save();
        switch (model_->kind_at(row)) {
            case AicSessionModel::Kind::Header:
                paint_header(p, opt.rect, model_->group_at(row));
                break;
            case AicSessionModel::Kind::Empty:
                paint_empty(p, opt.rect, index.data(Qt::DisplayRole).toString());
                break;
            case AicSessionModel::Kind::Session:
                if (const SessionSummary* s = model_->session_at(row))
                    paint_session(p, opt, *s);
                break;
        }
        p->restore();
    }

    bool editorEvent(QEvent* e, QAbstractItemModel*, const QStyleOptionViewItem& opt,
                     const QModelIndex& index) override {
        const QEvent::Type t = e->type();
        if (t != QEvent::MouseButtonPress && t != QEvent::MouseButtonRelease && t != QEvent::MouseButtonDblClick)
            return false;
        if (!model_->interactive() || !model_->is_session(index.row()))
            return false;
        const auto* me = static_cast<QMouseEvent*>(e);
        if (me->button() != Qt::LeftButton)
            return false;
        const int g = glyph_at(opt.rect, me->position().toPoint());
        if (g < 0)
            return false;
        if (t == QEvent::MouseButtonRelease) {
            suppress_ = true;
            if (on_glyph)
                on_glyph(g, model_->id_at(index.row()));
        }
        return true;
    }

  private:
    struct Metrics {
        QFont title, title_bold, snippet, time, chip, head, empty, glyph;
        int h_title = 0;
        int h_snip = 0;
        int h_row = 0;
        int h_head = 0;
        int h_empty = 0;
        int glyph_box = 0;
    };

    const Metrics& metrics() const {
        if (metrics_ok_)
            return m_;
        m_.title = style::ui_font(-1);
        m_.title_bold = style::ui_font(-1, QFont::DemiBold);
        m_.snippet = style::ui_font(-3);
        m_.time = style::mono_font(-3);
        m_.chip = style::ui_font(-4, QFont::DemiBold);
        m_.head = style::ui_font(-4, QFont::Bold);
        m_.head.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        m_.empty = style::ui_font(-2);
        m_.glyph = style::ui_font(-1);
        const QFontMetrics ft(m_.title_bold), fs(m_.snippet), fc(m_.chip), fh(m_.head), fe(m_.empty);
        m_.h_title = ft.height();
        m_.h_snip = std::max(fs.height(), fc.height() + 2);
        m_.h_row = style::u(2) + m_.h_title + style::u(1) + m_.h_snip + style::u(2);
        m_.h_head = fh.height() + style::u(3);
        m_.h_empty = fe.height() + style::u(6);
        m_.glyph_box = std::max(18, m_.h_title + style::u(1));
        metrics_ok_ = true;
        return m_;
    }

    QRect glyph_rect(const QRect& r, int i) const {
        const int b = metrics().glyph_box;
        const int right = r.right() - style::u(2);
        const int top = r.top() + (r.height() - b) / 2;
        const int x = right - (3 - i) * b - (2 - i) * style::u(1) + 1;
        return QRect(x, top, b, b);
    }

    int glyph_at(const QRect& r, const QPoint& pos) const {
        for (int i = kSrGlyphPin; i <= kSrGlyphDelete; ++i)
            if (glyph_rect(r, i).contains(pos))
                return i;
        return -1;
    }

    static void paint_diamond(QPainter* p, const QPoint& c, int d, const QColor& color, bool filled) {
        const int h = d / 2 + 1;
        QPolygon poly;
        poly << QPoint(c.x(), c.y() - h) << QPoint(c.x() + h, c.y()) << QPoint(c.x(), c.y() + h)
             << QPoint(c.x() - h, c.y());
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(color, 1.0));
        p->setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
        p->drawPolygon(poly);
        p->setRenderHint(QPainter::Antialiasing, false);
    }

    void paint_header(QPainter* p, const QRect& r, DateGroup group) const {
        const Metrics& mm = metrics();
        const QString text = date_group_title(group);
        const QFontMetrics fm(mm.head);
        const int x = r.left() + style::u(3);
        const int base = r.bottom() - style::u(1) - fm.descent();
        p->setFont(mm.head);
        p->setPen(style::text_tertiary());
        p->drawText(QPoint(x, base), text);
        const int tx = x + fm.horizontalAdvance(text) + style::u(2);
        const int rx = r.right() - style::u(2);
        if (tx < rx)
            p->fillRect(QRect(tx, base - fm.capHeight() / 2, rx - tx + 1, 1), style::border_dim());
    }

    void paint_empty(QPainter* p, const QRect& r, const QString& text) const {
        const Metrics& mm = metrics();
        const QFontMetrics fm(mm.empty);
        const QRect area(r.adjusted(style::u(3), 0, -style::u(2), 0));
        p->setFont(mm.empty);
        p->setPen(style::text_tertiary());
        p->drawText(area, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(text, Qt::ElideRight, area.width()));
    }

    void paint_session(QPainter* p, const QStyleOptionViewItem& opt, const SessionSummary& s) const {
        const Metrics& mm = metrics();
        const QRect r = opt.rect;
        const bool active = !s.id.isEmpty() && s.id == model_->active();
        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const bool hover = opt.state.testFlag(QStyle::State_MouseOver);
        const bool live = model_->interactive();

        QColor bg;
        if (selected)
            bg = style::accent_bg();
        else if (active)
            bg = style::bg_raised();
        else if (hover && live)
            bg = style::bg_hover();
        if (bg.isValid())
            p->fillRect(r, bg);
        p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), style::border_dim());
        if (active || selected)
            p->fillRect(QRect(r.left(), r.top(), 2, r.height() - 1), style::accent());

        const int x0 = r.left() + style::u(3);
        const int right = r.right() - style::u(2);
        const int y1 = r.top() + style::u(2);
        const QRect line1(x0, y1, right - x0 + 1, mm.h_title);
        const QRect line2(x0, y1 + mm.h_title + style::u(1), right - x0 + 1, mm.h_snip);

        // Line 1, right to left: running / unread dot, time, pin mark.
        int r1 = right;
        if (s.running || s.unread) {
            const int d = std::max(6, mm.h_title / 3);
            const QRect dot(r1 - d + 1, line1.center().y() - d / 2, d, d);
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(Qt::NoPen);
            p->setBrush(s.running ? style::accent() : style::info());
            p->drawEllipse(dot);
            p->setRenderHint(QPainter::Antialiasing, false);
            r1 -= d + style::u(2);
        }
        const QString when = short_when(s.updated, QDate::currentDate());
        if (!when.isEmpty()) {
            const QFontMetrics fm(mm.time);
            const int tw = fm.horizontalAdvance(when);
            p->setFont(mm.time);
            p->setPen(style::text_tertiary());
            p->drawText(QRect(r1 - tw + 1, line1.top(), tw, line1.height()), Qt::AlignRight | Qt::AlignVCenter, when);
            r1 -= tw + style::u(2);
        }
        if (s.pinned) {
            const int d = std::max(5, mm.h_title / 3);
            paint_diamond(p, QPoint(r1 - d / 2, line1.center().y()), d, style::text_secondary(), true);
            r1 -= d + style::u(2);
        }

        // Title.
        const QFont& tf = active ? mm.title_bold : mm.title;
        const QFontMetrics ftm(tf);
        const int title_w = std::max(0, r1 - x0 + 1);
        p->setFont(tf);
        p->setPen(live || active ? style::text_primary() : style::text_tertiary());
        p->drawText(QRect(x0, line1.top(), title_w, line1.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    ftm.elidedText(s.title, Qt::ElideRight, title_w));

        // Line 2: snippet (or the message count), model chip at the right.
        int r2 = right;
        const QString chip = sr_model_label(s);
        if (!chip.isEmpty()) {
            const QFontMetrics fc(mm.chip);
            const int max_w = std::max(style::u(8), (right - x0) * 2 / 5);
            const QString shown = fc.elidedText(chip, Qt::ElideRight, max_w);
            const int cw = fc.horizontalAdvance(shown) + style::u(2);
            const int ch = fc.height() + 2;
            const QRect box(r2 - cw + 1, line2.center().y() - ch / 2, cw, ch);
            p->setPen(style::border_med());
            p->setBrush(Qt::NoBrush);
            p->drawRect(box.adjusted(0, 0, -1, -1));
            p->setFont(mm.chip);
            p->setPen(style::text_tertiary());
            p->drawText(box, Qt::AlignCenter, shown);
            r2 -= cw + style::u(2);
        }
        const QString snip = s.snippet.isEmpty() ? AicSessionRail::tr("%n msg", "", s.messages) : s.snippet;
        const QFontMetrics fsn(mm.snippet);
        const int snip_w = std::max(0, r2 - x0 + 1);
        p->setFont(mm.snippet);
        p->setPen(style::text_tertiary());
        p->drawText(QRect(x0, line2.top(), snip_w, line2.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    fsn.elidedText(snip, Qt::ElideRight, snip_w));

        // Hover: PIN / RENAME / DELETE glyphs over the right end of the row.
        if (!(hover && live))
            return;
        const QRect first = glyph_rect(r, kSrGlyphPin);
        p->fillRect(QRect(first.left() - style::u(2), r.top(), r.right() - first.left() + style::u(2) + 1,
                          r.height() - 1),
                    bg.isValid() ? bg : style::bg_hover());
        QPoint cursor(-1, -1);
        if (const auto* view = qobject_cast<const QAbstractItemView*>(opt.widget))
            cursor = view->viewport()->mapFromGlobal(QCursor::pos());
        for (int i = kSrGlyphPin; i <= kSrGlyphDelete; ++i) {
            const QRect g = glyph_rect(r, i);
            const bool over = g.contains(cursor);
            const QColor c = over ? (i == kSrGlyphDelete ? style::danger() : style::text_primary())
                                  : style::text_secondary();
            if (over) {
                p->setPen(i == kSrGlyphDelete ? style::danger() : style::border_med());
                p->setBrush(Qt::NoBrush);
                p->drawRect(g.adjusted(0, 0, -1, -1));
            }
            if (i == kSrGlyphPin) {
                paint_diamond(p, g.center(), std::max(6, g.height() / 3), c, s.pinned);
            } else {
                p->setFont(mm.glyph);
                p->setPen(c);
                p->drawText(g, Qt::AlignCenter,
                            i == kSrGlyphRename ? QStringLiteral("✎") : QStringLiteral("✕"));
            }
        }
    }

    AicSessionModel* model_;
    mutable Metrics m_;
    mutable bool metrics_ok_ = false;
    bool suppress_ = false;
};

// ── List view: keys, hover repaint, header skipping ──────────────────────────

class AicSessionListView : public QListView {
  public:
    explicit AicSessionListView(QWidget* parent) : QListView(parent) {}

    /// Handles a key before the view does; true = consumed.
    std::function<bool(QKeyEvent*)> on_key;

  protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (on_key && on_key(e)) {
            e->accept();
            return;
        }
        QListView::keyPressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        QListView::mouseMoveEvent(e);
        // The hovered glyph follows the pointer inside one row (the view repaints only on row changes).
        const QModelIndex i = indexAt(e->position().toPoint());
        if (i.isValid())
            viewport()->update(visualRect(i));
    }

    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers mods) override {
        const QModelIndex cur = currentIndex();
        const QModelIndex next = QListView::moveCursor(action, mods);
        if (!next.isValid() || !model() || model()->flags(next).testFlag(Qt::ItemIsSelectable))
            return next;
        // Landed on a group header: keep going the same way, else the other way.
        const bool up = action == MoveUp || action == MovePageUp || action == MoveEnd || action == MovePrevious ||
                        action == MoveLeft;
        const int n = model()->rowCount();
        for (int pass = 0; pass < 2; ++pass) {
            const int dir = (up == (pass == 0)) ? -1 : 1;
            for (int r = next.row() + dir; r >= 0 && r < n; r += dir) {
                const QModelIndex i = model()->index(r, 0);
                if (model()->flags(i).testFlag(Qt::ItemIsSelectable))
                    return i;
            }
        }
        return cur;
    }
};

class AicSessionSearchEdit : public QLineEdit {
  public:
    explicit AicSessionSearchEdit(QWidget* parent) : QLineEdit(parent) {}
    std::function<bool(QKeyEvent*)> on_key;

  protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (on_key && on_key(e)) {
            e->accept();
            return;
        }
        QLineEdit::keyPressEvent(e);
    }
};

class AicSessionFooter : public QWidget {
  public:
    explicit AicSessionFooter(QWidget* parent) : QWidget(parent) {}
    std::function<void()> on_resize;

  protected:
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (on_resize)
            on_resize();
    }
};

AicSessionDelegate* sr_delegate(QStyledItemDelegate* d) {
    return static_cast<AicSessionDelegate*>(d);
}

} // namespace

// ── Rail ─────────────────────────────────────────────────────────────────────

AicSessionRail::AicSessionRail(QWidget* parent) : QWidget(parent) {
    setObjectName(style::kRailLeft);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── ▌SESSIONS 42 ……………… [+]
    header_ = ui::make_panel_header(tr("SESSIONS"), this).header;
    new_btn_ = new QPushButton(QStringLiteral("+"), header_);
    new_btn_->setObjectName(style::kIconBtn);
    new_btn_->setCursor(Qt::PointingHandCursor);
    connect(new_btn_, &QPushButton::clicked, this, [this]() {
        if (interactive_)
            emit new_requested();
    });
    header_->add_control(new_btn_);
    vl->addWidget(header_);

    // ── Search
    auto* search_row = new QWidget(this);
    auto* sl = new QHBoxLayout(search_row);
    sl->setContentsMargins(style::u(2), style::u(2), style::u(2), style::u(2));
    sl->setSpacing(0);
    auto* search = new AicSessionSearchEdit(search_row);
    search_ = search;
    search_->setObjectName(style::kSearch);
    search_->setClearButtonEnabled(true);
    sl->addWidget(search_);
    vl->addWidget(search_row);
    connect(search_, &QLineEdit::textChanged, this,
            [this](const QString& text) { apply_search(text, !programmatic_search_); });

    // ── List
    model_ = new AicSessionModel(this);
    auto* list = new AicSessionListView(this);
    list_ = list;
    list_->setObjectName(style::kList);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setUniformItemSizes(false);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setMouseTracking(true);
    list_->viewport()->setAttribute(Qt::WA_Hover, true);
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    list_->setFocusPolicy(Qt::StrongFocus);
    // Keys keyPressEvent handles on the list (invisible to agents otherwise): Del asks to delete.
    list_->setProperty(core::agent::kAgentRiskKeysProperty, QStringLiteral("Delete=delete"));
    auto* delegate = new AicSessionDelegate(model_, list_);
    delegate_ = delegate;
    list_->setItemDelegate(delegate);
    list_->setModel(model_);
    vl->addWidget(list_, 1);

    delegate->on_glyph = [this](int glyph, const QString& id) {
        // Out of the view's mouse handler first: the host may confirm with a dialog and reload the list.
        QTimer::singleShot(0, this, [this, glyph, id]() { run_glyph(glyph, id); });
    };
    connect(list_, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        if (sr_delegate(delegate_)->take_click_suppression())
            return;
        activate_row(index.row());
    });
    connect(list_, &QWidget::customContextMenuRequested, this, &AicSessionRail::show_menu);
    connect(list_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { update_footer(); });
    connect(list_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { update_footer(); });

    search->on_key = [this](QKeyEvent* e) {
        if ((e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != Qt::NoModifier)
            return false;
        switch (e->key()) {
            case Qt::Key_Down:
                focus_list();
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                // A search narrows the list: Enter opens the best (first) match.
                if (model_->has_filter() && model_->first_session_row() >= 0)
                    activate_row(model_->first_session_row());
                else
                    focus_list();
                return true;
            case Qt::Key_Escape:
                if (search_->text().isEmpty())
                    return false;
                search_->clear();
                return true;
            default:
                return false;
        }
    };
    list->on_key = [this](QKeyEvent* e) {
        const QModelIndex cur = list_->currentIndex();
        const int row = cur.isValid() ? cur.row() : -1;
        const bool on_session = model_->is_session(row);
        const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
        switch (e->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (mods != Qt::NoModifier)
                    return false;
                if (on_session)
                    activate_row(row);
                return true;
            case Qt::Key_F2:
                if (interactive_ && on_session)
                    emit rename_requested(model_->id_at(row));
                return true;
            case Qt::Key_Delete:
                if (interactive_ && on_session)
                    emit delete_requested(model_->id_at(row));
                return true;
            case Qt::Key_Up: {
                if (mods != Qt::NoModifier)
                    return false;
                // Above the first session: back to the search field.
                for (int r = row - 1; r >= 0; --r)
                    if (model_->is_session(r))
                        return false;
                focus_search();
                return true;
            }
            default: {
                // Typing in the list searches: the character goes to the search field.
                const QString t = e->text();
                if ((mods == Qt::NoModifier || mods == Qt::ShiftModifier) && t.size() == 1 && t.at(0).isPrint() &&
                    !t.at(0).isSpace()) {
                    search_->setFocus(Qt::ShortcutFocusReason);
                    search_->end(false);
                    search_->insert(t);
                    return true;
                }
                return false;
            }
        }
    };

    // ── Footer: RENAME · PIN · EXPORT ………… DELETE
    auto* rule = new QFrame(this);
    rule->setObjectName(style::kHairline);
    rule->setFrameShape(QFrame::NoFrame);
    rule->setFixedHeight(1);
    vl->addWidget(rule);

    auto* footer = new AicSessionFooter(this);
    footer_ = footer;
    // The rail's width decides the footer's; labels switch to glyphs when they do not fit.
    footer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* fl = new QHBoxLayout(footer_);
    fl->setContentsMargins(style::u(2), 0, style::u(2), 0);
    fl->setSpacing(style::u(1));
    auto make_btn = [this](const char* object_name) {
        auto* b = new QPushButton(footer_);
        b->setObjectName(object_name);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    rename_btn_ = make_btn(style::kBtn);
    pin_btn_ = make_btn(style::kBtn);
    export_btn_ = make_btn(style::kBtn);
    delete_btn_ = make_btn(style::kBtnDanger);
    delete_btn_->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("delete"));
    fl->addWidget(rename_btn_);
    fl->addWidget(pin_btn_);
    fl->addWidget(export_btn_);
    fl->addStretch(1);
    fl->addWidget(delete_btn_);
    vl->addWidget(footer_);
    footer->on_resize = [this]() { fit_footer(); };

    connect(rename_btn_, &QPushButton::clicked, this, [this]() {
        const QString id = target_id();
        if (interactive_ && model_->find(id))
            emit rename_requested(id);
    });
    connect(pin_btn_, &QPushButton::clicked, this, [this]() {
        const QString id = target_id();
        const SessionSummary* s = model_->find(id);
        if (interactive_ && s)
            emit pin_toggled(id, !s->pinned);
    });
    connect(export_btn_, &QPushButton::clicked, this, [this]() {
        const QString id = target_id();
        if (interactive_ && model_->find(id))
            open_export_menu(id);
    });
    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        const QString id = target_id();
        if (interactive_ && model_->find(id))
            emit delete_requested(id);
    });

    setTabOrder(search_, list_);
    setTabOrder(list_, rename_btn_);
    setTabOrder(rename_btn_, pin_btn_);
    setTabOrder(pin_btn_, export_btn_);
    setTabOrder(export_btn_, delete_btn_);

    model_->set_sessions({});
    retranslate();
    fit_metrics();
    // Font size / family changed: row heights, button sizes and the footer labels follow.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { fit_metrics(); });
}

AicSessionRail::~AicSessionRail() = default;

void AicSessionRail::set_sessions(const QVector<SessionSummary>& sessions) {
    const QString keep = target_id();
    model_->set_sessions(sessions);
    model_->set_active(active_);
    select_id(keep, false);
    update_badge();
    update_footer();
}

void AicSessionRail::set_active(const QString& id) {
    active_ = id;
    model_->set_active(id);
    select_id(id, true);
    update_footer();
}

void AicSessionRail::set_running(const QString& id, bool running) {
    model_->set_running(id, running);
}

void AicSessionRail::set_unread(const QString& id, bool unread) {
    model_->set_unread(id, unread);
}

void AicSessionRail::set_snippet(const QString& id, const QString& snippet) {
    const QString keep = target_id();
    if (model_->set_snippet(id, snippet)) {
        select_id(keep, false);
        update_badge();
        update_footer();
    }
}

void AicSessionRail::set_search_text(const QString& text) {
    programmatic_search_ = true;
    search_->setText(text);
    programmatic_search_ = false;
}

QString AicSessionRail::search_text() const {
    return search_->text();
}

void AicSessionRail::set_interactive(bool interactive) {
    if (interactive_ == interactive)
        return;
    interactive_ = interactive;
    model_->set_interactive(interactive);
    // While an answer streams a click must not even move the selection.
    list_->setSelectionMode(interactive ? QAbstractItemView::SingleSelection : QAbstractItemView::NoSelection);
    if (interactive)
        select_id(active_, false);
    new_btn_->setEnabled(interactive);
    new_btn_->setToolTip(interactive ? tr("New chat (Ctrl+N)") : sr_wait_tip());
    list_->viewport()->update();
    update_footer();
}

void AicSessionRail::focus_list() {
    list_->setFocus(Qt::OtherFocusReason);
    const QModelIndex cur = list_->currentIndex();
    if (cur.isValid() && model_->is_session(cur.row()) && !model_->has_filter())
        return;
    // A search: the first match; otherwise the active session (or the first one).
    int row = model_->has_filter() ? -1 : model_->row_of(active_);
    if (row < 0)
        row = model_->first_session_row();
    if (row < 0)
        return;
    const QModelIndex mi = model_->index(row, 0);
    list_->selectionModel()->setCurrentIndex(
        mi, interactive_ ? QItemSelectionModel::ClearAndSelect : QItemSelectionModel::NoUpdate);
    list_->scrollTo(mi, QAbstractItemView::EnsureVisible);
}

void AicSessionRail::focus_search() {
    search_->setFocus(Qt::ShortcutFocusReason);
    search_->selectAll();
}

QWidget* AicSessionRail::list_widget() const {
    return list_;
}

QWidget* AicSessionRail::search_widget() const {
    return search_;
}

int AicSessionRail::preferred_width() const {
    return std::max({220, 26 * style::avg_char(0) + style::u(6), footer_full_width()});
}

void AicSessionRail::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void AicSessionRail::retranslate() {
    header_->set_title(tr("SESSIONS"));
    new_btn_->setAccessibleName(tr("New chat"));
    new_btn_->setToolTip(interactive_ ? tr("New chat (Ctrl+N)") : sr_wait_tip());
    search_->setPlaceholderText(tr("Search sessions"));
    search_->setAccessibleName(tr("Search sessions"));
    search_->setToolTip(tr("Every word must appear in the title or the last message · Down moves into the list · "
                           "Enter opens the first match"));
    list_->setAccessibleName(tr("Sessions"));
    list_->setAccessibleDescription(
        tr("Enter opens the session · F2 renames · Del deletes · the context menu has more"));
    update_badge();
    update_footer();
    list_->viewport()->update();
}

void AicSessionRail::update_badge() {
    const int total = model_->total();
    header_->set_badge(model_->has_filter()
                           ? QString::number(model_->shown()) + QStringLiteral(" / ") + QString::number(total)
                           : QString::number(total));
}

void AicSessionRail::update_footer() {
    if (!delete_btn_)
        return;
    const QString id = target_id();
    const SessionSummary* s = model_->find(id);
    const bool can = interactive_ && s;
    const bool pinned = s && s->pinned;

    rename_btn_->setText(footer_compact_ ? QStringLiteral("✎") : tr("RENAME"));
    pin_btn_->setText(footer_compact_ ? (pinned ? QStringLiteral("◇") : QStringLiteral("◆"))
                                      : (pinned ? tr("UNPIN") : tr("PIN")));
    export_btn_->setText(footer_compact_ ? QStringLiteral("⇩") : tr("EXPORT"));
    delete_btn_->setText(footer_compact_ ? QStringLiteral("✕") : tr("DELETE"));

    rename_btn_->setAccessibleName(tr("Rename session"));
    pin_btn_->setAccessibleName(pinned ? tr("Unpin session") : tr("Pin session"));
    export_btn_->setAccessibleName(tr("Export session"));
    delete_btn_->setAccessibleName(tr("Delete session"));

    for (QPushButton* b : {rename_btn_, pin_btn_, export_btn_, delete_btn_})
        b->setEnabled(can);
    if (!interactive_) {
        for (QPushButton* b : {rename_btn_, pin_btn_, export_btn_, delete_btn_})
            b->setToolTip(sr_wait_tip());
        return;
    }
    if (!s) {
        for (QPushButton* b : {rename_btn_, pin_btn_, export_btn_, delete_btn_})
            b->setToolTip(tr("Select a session first"));
        return;
    }
    rename_btn_->setToolTip(tr("Rename this session (F2)"));
    pin_btn_->setToolTip(pinned ? tr("Unpin this session") : tr("Keep this session at the top of the list"));
    export_btn_->setToolTip(tr("Save or copy this session as Markdown"));
    delete_btn_->setToolTip(tr("Delete this session and all of its messages (Del) — you are asked first"));
}

void AicSessionRail::show_menu(const QPoint& pos) {
    if (!interactive_)
        return;
    QModelIndex index = list_->indexAt(pos);
    QPoint at = pos;
    if (!index.isValid() || !model_->is_session(index.row())) {
        // Keyboard (Menu key) or empty space: the current row.
        index = list_->currentIndex();
        if (!index.isValid() || !model_->is_session(index.row()))
            return;
        at = list_->visualRect(index).bottomLeft();
    }
    const QString id = model_->id_at(index.row());
    const SessionSummary* s = model_->find(id);
    if (!s)
        return;
    // The menu acts on that row: select it (nothing is opened).
    list_->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    const bool pinned = s->pinned;

    auto* menu = new QMenu(list_);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(style::menu_sheet());
    connect(menu->addAction(tr("Rename…  (F2)")), &QAction::triggered, this,
            [this, id]() { emit rename_requested(id); });
    connect(menu->addAction(pinned ? tr("Unpin") : tr("Pin")), &QAction::triggered, this,
            [this, id, pinned]() { emit pin_toggled(id, !pinned); });
    connect(menu->addAction(tr("Export as Markdown…")), &QAction::triggered, this,
            [this, id]() { emit export_requested(id); });
    connect(menu->addAction(tr("Copy as Markdown")), &QAction::triggered, this,
            [this, id]() { emit copy_requested(id); });
    menu->addSeparator();
    QAction* del = menu->addAction(tr("Delete…  (Del)"));
    del->setProperty(core::agent::kAgentRiskProperty, QStringLiteral("delete"));
    connect(del, &QAction::triggered, this, [this, id]() { emit delete_requested(id); });
    menu->popup(list_->viewport()->mapToGlobal(at));
}

// ── private ──────────────────────────────────────────────────────────────────

void AicSessionRail::apply_search(const QString& text, bool user) {
    const QString keep = target_id();
    model_->set_filter(text);
    select_id(keep, false);
    update_badge();
    update_footer();
    if (user)
        emit search_changed(text);
}

QString AicSessionRail::selected_id() const {
    if (!list_ || !list_->selectionModel())
        return {};
    const QModelIndexList sel = list_->selectionModel()->selectedIndexes();
    for (const QModelIndex& i : sel)
        if (model_->is_session(i.row()))
            return model_->id_at(i.row());
    return {};
}

QString AicSessionRail::target_id() const {
    const QString s = selected_id();
    return s.isEmpty() ? active_ : s;
}

void AicSessionRail::select_id(const QString& id, bool scroll) {
    QItemSelectionModel* sm = list_->selectionModel();
    if (!sm)
        return;
    const int row = id.isEmpty() ? -1 : model_->row_of(id);
    if (row < 0) {
        sm->clearSelection();
        return;
    }
    const QModelIndex mi = model_->index(row, 0);
    sm->setCurrentIndex(mi, QItemSelectionModel::ClearAndSelect);
    if (scroll)
        list_->scrollTo(mi, QAbstractItemView::EnsureVisible);
}

void AicSessionRail::activate_row(int row) {
    if (!interactive_ || !model_->is_session(row))
        return;
    const QString id = model_->id_at(row);
    if (!id.isEmpty())
        emit session_activated(id);
}

void AicSessionRail::run_glyph(int glyph, const QString& id) {
    const SessionSummary* s = model_->find(id);
    if (!interactive_ || !s)
        return;
    if (glyph == kSrGlyphPin)
        emit pin_toggled(id, !s->pinned);
    else if (glyph == kSrGlyphRename)
        emit rename_requested(id);
    else if (glyph == kSrGlyphDelete)
        emit delete_requested(id);
}

void AicSessionRail::open_export_menu(const QString& id) {
    auto* menu = new QMenu(export_btn_);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(style::menu_sheet());
    connect(menu->addAction(tr("Save as Markdown…")), &QAction::triggered, this,
            [this, id]() { emit export_requested(id); });
    connect(menu->addAction(tr("Copy as Markdown")), &QAction::triggered, this,
            [this, id]() { emit copy_requested(id); });
    menu->popup(export_btn_->mapToGlobal(QPoint(0, export_btn_->height())));
}

void AicSessionRail::fit_metrics() {
    const int bh = style::button_h();
    new_btn_->setFixedSize(bh, bh);
    for (QPushButton* b : {rename_btn_, pin_btn_, export_btn_, delete_btn_})
        b->setFixedHeight(bh);
    footer_->setFixedHeight(style::strip_h());
    sr_delegate(delegate_)->invalidate();
    list_->doItemsLayout();
    list_->viewport()->update();
    fit_footer();
}

int AicSessionRail::footer_full_width() const {
    const QFontMetrics fm(style::ui_font(-3, QFont::Bold)); // the sheet's button font (+ 0.5 px tracking)
    int w = style::u(2) * 2 + style::u(1) * 3 + style::u(4);
    for (const QString& t : {tr("RENAME"), tr("UNPIN"), tr("EXPORT"), tr("DELETE")})
        w += fm.horizontalAdvance(t) + int(t.size()) + 18;
    return w;
}

void AicSessionRail::fit_footer() {
    if (!footer_)
        return;
    const bool compact = footer_->width() > 0 && footer_->width() < footer_full_width();
    if (compact == footer_compact_)
        return;
    footer_compact_ = compact;
    update_footer();
}

} // namespace fincept::screens::aichat
