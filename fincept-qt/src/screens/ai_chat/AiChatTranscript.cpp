// AiChatTranscript.cpp — the virtualised AI Chat transcript (see AiChatTranscript.h).
//
//   Entry          one turn: its data, the cached ParsedDoc, a measured (or estimated) height
//                  and — only while it is on or near the screen — its widget
//   Private        the entries with prefix-sum offsets, the stream flush timer, the parse
//                  workers and the "↓ NEW" pill; layout_now() measures, places, realises and
//                  evicts turn widgets
//   AicTurnWidget  one realised turn: header line, REASONING, TOOLS, body blocks (their widgets:
//                  AiChatTranscript_Blocks.cpp), notice / error block, SOURCES
//
// Turn widgets are children of the viewport placed by hand at their prefix offset: no layout
// ever measures an off-screen turn. A streaming answer re-parses only its tail block (the
// text from Entry::tail_begin), at most every style::kStreamFlushMs.

#include "screens/ai_chat/AiChatTranscript.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/report/ReportBlocks.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QEnterEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens::aichat {

namespace {

using TrDocPtr = std::shared_ptr<const ParsedDoc>;

constexpr int tr_kFollowSlack = 48;                // px from the end that still counts as "at the tail"
constexpr qsizetype tr_kAsyncParseChars = 6000;    // longer answers parse on a worker thread
constexpr qsizetype tr_kThinkShowChars = 24000;    // reasoning shown when expanded (its tail)
constexpr qsizetype tr_kUserShowChars = 40000;     // a pasted wall of text is cut for display
constexpr int tr_kMaxColumnChars = 120;            // content column: max average characters
constexpr int tr_kMaxSourceButtons = 30;
constexpr int tr_kMaxMenuLinks = 10;

QString tr_spend() {
    return QStringLiteral("spend");
}

int tr_header_h() {
    return std::max(style::button_h(), style::strip_h() - style::u(1));
}

void tr_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

/// Set a selector property ([tone="…"], [role="…"]) and re-evaluate the sheet when it changed.
void tr_set_prop(QWidget* w, const char* name, const QString& value) {
    if (w->property(name).toString() == value)
        return;
    w->setProperty(name, value);
    tr_repolish(w);
}

/// Content of `from` changed: drop the cached size hints of it and of every container up to
/// `top`, so `top`'s height-for-width is right at once (not only after the posted layout
/// requests have travelled up one level per event).
void tr_invalidate_up(QWidget* from, QWidget* top) {
    for (QWidget* w = from; w && w != top; w = w->parentWidget()) {
        if (QLayout* l = w->layout())
            l->invalidate();
        w->updateGeometry();
    }
    if (top && top->layout())
        top->layout()->invalidate();
}

QPushButton* tr_make_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QLatin1String(object_name));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedHeight(style::button_h());
    return b;
}

void tr_label_button(QPushButton* b, const QString& text, const QString& accessible, const QString& tip) {
    b->setText(text);
    b->setAccessibleName(accessible);
    b->setToolTip(tip);
}

QLabel* tr_make_label(const char* object_name, QWidget* parent, Qt::TextFormat format = Qt::PlainText) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(object_name));
    l->setTextFormat(format);
    return l;
}

/// A wrapped label that lays out at any width (its height follows the width).
void tr_make_wrapping(QLabel* l) {
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QSizePolicy sp(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    l->setSizePolicy(sp);
}

QString tr_when(const QDateTime& when) {
    if (!when.isValid())
        return {};
    return when.date() == QDate::currentDate() ? when.toString(QStringLiteral("HH:mm"))
                                               : when.toString(QStringLiteral("MMM d, HH:mm"));
}

QString tr_caret_html() {
    return QStringLiteral("<span style='color:%1'>%2</span>").arg(style::accent().name(), QString(QChar(0x258D)));
}

/// The first ATX heading of an answer (outside code fences), as plain text; "" when none.
QString tr_first_heading(const QString& markdown) {
    bool fence = false;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        if (t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"))) {
            fence = !fence;
            continue;
        }
        if (fence || !t.startsWith(QLatin1Char('#')))
            continue;
        int k = 0;
        while (k < t.size() && t.at(k) == QLatin1Char('#'))
            ++k;
        if (k > 6 || k >= t.size() || t.at(k) != QLatin1Char(' '))
            continue;
        QString h = t.mid(k).trimmed();
        while (h.endsWith(QLatin1Char('#')))
            h.chop(1);
        h = strip_inline_markdown(h.trimmed()).trimmed();
        if (!h.isEmpty())
            return h;
    }
    return {};
}

bool tr_can_regenerate(const Turn& t) {
    return t.role == TurnRole::Assistant && t.state == TurnState::Done;
}

bool tr_can_retry(const Turn& t) {
    if (t.role == TurnRole::Assistant)
        return t.state == TurnState::Failed || t.state == TurnState::Stopped || t.state == TurnState::Empty;
    if (t.role == TurnRole::Notice)
        return t.notice == NoticeKind::Credits || t.notice == NoticeKind::SignIn || t.notice == NoticeKind::Error;
    return false;
}

/// The buttons of a notice turn (and the same entries in its action menu).
struct TrNoticeAction {
    TurnAction action = TurnAction::Copy;
    QString text;
    bool spend = false;     ///< marked fincept_agent_risk = "spend"
    bool live_only = false; ///< only on the live turn while actions are enabled
};

QVector<TrNoticeAction> tr_notice_actions(const Turn& t) {
    using V = AicTranscriptView;
    const TrNoticeAction retry{TurnAction::Retry, V::tr("RETRY"), true, true};
    const TrNoticeAction llm{TurnAction::LlmSettings, V::tr("LLM SETTINGS"), false, false};
    const TrNoticeAction model{TurnAction::ModelSettings, V::tr("MODEL SETTINGS"), false, false};
    switch (t.notice) {
        case NoticeKind::Credits:
            return {TrNoticeAction{TurnAction::ViewPlans, V::tr("VIEW PLANS"), false, false}, llm, retry};
        case NoticeKind::SignIn:
            return {llm, retry};
        case NoticeKind::Route:
            return {model};
        case NoticeKind::RouteOffer: {
            const QString who = t.offer_label.trimmed().isEmpty() ? V::tr("ANOTHER MODEL")
                                                                  : fincept::ui::literal_ampersands(t.offer_label.trimmed());
            return {TrNoticeAction{TurnAction::AnswerWith, V::tr("ANSWER WITH %1").arg(who), true, true}, model};
        }
        case NoticeKind::Setup:
            return {TrNoticeAction{TurnAction::OpenSettings, V::tr("OPEN SETTINGS"), false, false}};
        case NoticeKind::Error:
            return {retry};
        case NoticeKind::Info:
        case NoticeKind::None:
            break;
    }
    return {};
}

QString tr_notice_tone(NoticeKind k) {
    switch (k) {
        case NoticeKind::Credits:
        case NoticeKind::SignIn:
            return QStringLiteral("warn");
        case NoticeKind::RouteOffer:
        case NoticeKind::Setup:
            return QStringLiteral("accent");
        case NoticeKind::Error:
            return QStringLiteral("danger");
        case NoticeKind::Route:
        case NoticeKind::Info:
        case NoticeKind::None:
            break;
    }
    return QStringLiteral("info");
}

// ── Small painted widgets ────────────────────────────────────────────────────

/// A one-line label that elides instead of pushing its neighbours (billing note).
class AicTrElide : public QLabel {
  public:
    explicit AicTrElide(QWidget* parent) : QLabel(parent) { setTextFormat(Qt::PlainText); }
    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setFont(font());
        p.setPen(palette().color(foregroundRole()));
        const QRect r = contentsRect();
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, r.width()));
    }
};

/// The STREAMING dot: pulses while visible when motion is on, still otherwise.
class AicTrPulse : public QWidget {
  public:
    explicit AicTrPulse(QWidget* parent) : QWidget(parent) {
        timer_.setInterval(80);
        QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
            phase_ = std::fmod(phase_ + 0.07, 1.0);
            update();
        });
        setAccessibleName(AicTranscriptView::tr("Streaming"));
        refit();
    }
    void refit() {
        const int d = std::max(6, style::text_h(-6) / 2 + 2);
        setFixedSize(d, d);
    }

  protected:
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e);
        if (style::motion())
            timer_.start();
    }
    void hideEvent(QHideEvent* e) override {
        timer_.stop();
        QWidget::hideEvent(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c = style::accent();
        if (timer_.isActive())
            c.setAlphaF(float(0.35 + 0.65 * (0.5 + 0.5 * std::cos(phase_ * 2.0 * 3.14159265358979))));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
    }

  private:
    QTimer timer_;
    double phase_ = 0.0;
};

/// TOOLS rows, painted: glyph · tool (mono) · args (dim, elided) · ×N … duration, and a dim
/// second line with the failure reason / job status / note.
class AicTrToolRows : public QWidget {
  public:
    explicit AicTrToolRows(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setAccessibleName(AicTranscriptView::tr("Tool calls"));
    }
    /// `jobs`: per row the live job status (text, tone) or ("", -1).
    void set_rows(const QVector<ToolRow>& rows, const QVector<QPair<QString, int>>& jobs) {
        rows_ = rows;
        jobs_ = jobs;
        refit();
        update();
    }
    void refit() {
        line_h_ = QFontMetrics(style::mono_font(-2)).height() + style::u(1);
        int lines = 0;
        for (int i = 0; i < int(rows_.size()); ++i)
            lines += second_line(i).isEmpty() ? 1 : 2;
        setFixedHeight(lines * line_h_ + style::u(1));
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QFont mono = style::mono_font(-2);
        const QFont uif = style::ui_font(-2);
        const QFontMetrics fm(mono), fu(uif);
        const int glyph_w = fm.height() + style::u(1);
        const int w = width();
        int y = style::u(1) / 2;
        for (int i = 0; i < int(rows_.size()); ++i) {
            const ToolRow& r = rows_.at(i);
            QChar glyph(0x2713);
            QColor gc = style::ok();
            if (r.state == ToolStep::State::Running) {
                glyph = QChar(0x2026);
                gc = style::accent();
            } else if (r.state == ToolStep::State::Failed) {
                glyph = QChar(0x2717);
                gc = style::danger();
            } else if (r.state == ToolStep::State::Job) {
                glyph = QChar(0x29D7);
                gc = style::warn();
            } else if (!r.timed) {
                glyph = QChar(0x2022); // no completion record from this provider's loop
                gc = style::text_dim();
            }
            p.setFont(mono);
            p.setPen(gc);
            p.drawText(QRect(0, y, glyph_w, line_h_), Qt::AlignLeft | Qt::AlignVCenter, QString(glyph));

            QString dur;
            if (r.running == 0 && r.timed) {
                dur = format_ms(r.ms);
                if (r.parallel)
                    dur += QStringLiteral(" ") + QChar(0x2225);
            }
            const int dur_w = dur.isEmpty() ? 0 : fm.horizontalAdvance(dur);
            if (dur_w > 0) {
                p.setPen(style::text_tertiary());
                p.drawText(QRect(w - dur_w, y, dur_w, line_h_), Qt::AlignRight | Qt::AlignVCenter, dur);
            }
            const int right = w - dur_w - (dur_w > 0 ? style::u(2) : 0);
            int x = glyph_w;
            QString name = r.tool;
            if (r.count > 1)
                name += QStringLiteral(" ") + QChar(0x00D7) + QString::number(r.count);
            name = fm.elidedText(name, Qt::ElideRight, std::max(0, right - x));
            p.setPen(style::text_primary());
            p.drawText(QRect(x, y, std::max(0, right - x), line_h_), Qt::AlignLeft | Qt::AlignVCenter, name);
            x += fm.horizontalAdvance(name) + style::u(2);
            if (!r.args.isEmpty() && x < right) {
                p.setFont(uif);
                p.setPen(style::text_tertiary());
                p.drawText(QRect(x, y, right - x, line_h_), Qt::AlignLeft | Qt::AlignVCenter,
                           fu.elidedText(r.args, Qt::ElideRight, right - x));
            }
            y += line_h_;

            const QString second = second_line(i);
            if (second.isEmpty())
                continue;
            x = glyph_w;
            const int tone = jobs_.value(i).second;
            if (!jobs_.value(i).first.isEmpty() && tone >= 0) {
                QChar jg(0x29D7);
                QColor jc = style::warn();
                if (tone == 1) {
                    jg = QChar(0x2713);
                    jc = style::ok();
                } else if (tone == 2) {
                    jg = QChar(0x2717);
                    jc = style::danger();
                } else if (tone == 3) {
                    jg = QChar(0x2298);
                    jc = style::text_dim();
                }
                p.setFont(mono);
                p.setPen(jc);
                p.drawText(QRect(x, y, glyph_w, line_h_), Qt::AlignLeft | Qt::AlignVCenter, QString(jg));
                x += glyph_w;
            }
            p.setFont(uif);
            p.setPen(style::text_tertiary());
            p.drawText(QRect(x, y, std::max(0, w - x), line_h_), Qt::AlignLeft | Qt::AlignVCenter,
                       fu.elidedText(second, Qt::ElideRight, std::max(0, w - x)));
            y += line_h_;
        }
    }
    bool event(QEvent* e) override {
        if (e->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(e);
            int y = style::u(1) / 2;
            for (int i = 0; i < int(rows_.size()); ++i) {
                const QString second = second_line(i);
                const int h = line_h_ * (second.isEmpty() ? 1 : 2);
                if (he->pos().y() >= y && he->pos().y() < y + h) {
                    const ToolRow& r = rows_.at(i);
                    QString tip = r.tool;
                    if (!r.args.isEmpty())
                        tip += QStringLiteral(" · ") + r.args;
                    if (r.count > 1)
                        tip += QStringLiteral(" ") + QChar(0x00D7) + QString::number(r.count);
                    if (r.timed && r.running == 0)
                        tip += QStringLiteral(" · ") + format_ms(r.ms);
                    if (!second.isEmpty())
                        tip += QStringLiteral("\n") + second;
                    QToolTip::showText(he->globalPos(), tip, this);
                    return true;
                }
                y += h;
            }
            QToolTip::hideText();
            e->ignore();
            return true;
        }
        return QWidget::event(e);
    }

  private:
    QString second_line(int i) const {
        const ToolRow& r = rows_.at(i);
        const QPair<QString, int> job = jobs_.value(i, {QString(), -1});
        if (!job.first.isEmpty())
            return job.first;
        if (r.failed > 0) {
            const QString why = r.detail.isEmpty() ? AicTranscriptView::tr("no reason given") : r.detail;
            if (r.count > 1)
                return AicTranscriptView::tr("%1 of %2 failed: %3")
                    .arg(QString::number(r.failed), QString::number(r.count), why);
            return AicTranscriptView::tr("failed: %1").arg(why);
        }
        if (r.jobs > 0 && !r.job_id.isEmpty())
            return AicTranscriptView::tr("background job %1").arg(r.job_id);
        return r.detail;
    }

    QVector<ToolRow> rows_;
    QVector<QPair<QString, int>> jobs_;
    int line_h_ = 18;
};

} // namespace

// ── The turn widget (declaration) ────────────────────────────────────────────

class AicTurnWidget : public QWidget {
  public:
    AicTurnWidget(AicTranscriptView* view, quint64 id, QWidget* parent);

    quint64 id() const { return id_; }
    /// Everything from the entry (header, sections, body, footer, actions).
    void sync_all();
    void refresh_header();
    /// Hover / current / live / enabled state of the actions (and notice / error buttons).
    void refresh_actions();
    void sync_thinking();
    void sync_tools();
    void sync_body();
    void restyle(bool rerender_html);
    void set_current(bool on);
    void fade_in();
    int measure(int width);
    QPoint menu_anchor() const;

  protected:
    bool event(QEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

  private:
    enum Slot { SlotHeader, SlotChips, SlotUser, SlotThink, SlotTools, SlotContent, SlotPlaceholder, SlotError, SlotSources, SlotCount };
    struct BodyItem {
        QWidget* w = nullptr;
        BlockKind kind = BlockKind::Paragraph;
        QString source;
        bool closed = true;
        bool caret = false;
    };

    AicTranscriptView::Entry* entry() const;
    AicTranscriptView::Private* priv() const;
    bool compact() const;
    void build();
    void put(Slot slot, QWidget* w);
    void touched(QWidget* changed);
    void sync_user();
    void sync_blocks(const ParsedDoc& doc, bool streaming);
    void sync_placeholder();
    void sync_error();
    void sync_notice_buttons();
    void sync_sources();
    void set_dimmed(bool on);
    void popup_report_menu();
    void retranslate_actions();

    AicTranscriptView* view_;
    quint64 id_;
    TurnRole role_kind_ = TurnRole::User;
    QVBoxLayout* root_ = nullptr;
    QWidget* slots_[SlotCount] = {};

    // header
    QWidget* header_ = nullptr;
    QLabel* role_ = nullptr;
    QLabel* meta_ = nullptr;
    QWidget* pulse_ = nullptr;        // AicTrPulse (members keep to public types: unity-safe linkage)
    QLabel* tag_ = nullptr;
    QLabel* note_ = nullptr;          // AicTrElide
    QWidget* actions_ = nullptr;
    QPushButton* b_copy_ = nullptr;
    QPushButton* b_regen_ = nullptr;
    QPushButton* b_retry_ = nullptr;
    QPushButton* b_edit_ = nullptr;
    QPushButton* b_report_ = nullptr;
    QPushButton* b_quote_ = nullptr;

    // user turn
    QWidget* chips_ = nullptr;
    QVector<Attachment> chips_for_;
    QLabel* user_text_ = nullptr;
    QString user_for_;

    // activity
    QFrame* think_ = nullptr;
    QPushButton* think_head_ = nullptr;
    QLabel* think_body_ = nullptr;
    QFrame* tools_ = nullptr;
    QPushButton* tools_head_ = nullptr;
    QWidget* tool_rows_ = nullptr;    // AicTrToolRows

    // content
    QWidget* body_ = nullptr;
    QVBoxLayout* body_l_ = nullptr;
    QFrame* notice_ = nullptr;
    QWidget* notice_btns_ = nullptr;
    QVector<QPair<TurnAction, QPushButton*>> notice_buttons_;
    QString notice_built_for_;
    QVector<BodyItem> items_;
    QLabel* placeholder_ = nullptr;
    QFrame* error_ = nullptr;
    QLabel* error_text_ = nullptr;
    QPushButton* error_retry_ = nullptr;
    QWidget* sources_ = nullptr;
    QStringList sources_for_;

    bool hovered_ = false;
    bool current_ = false;
    bool dimmed_ = false;
    bool fading_ = false;
};

// ── Entry / Private ──────────────────────────────────────────────────────────

struct AicTranscriptView::Entry {
    Turn turn;
    TrDocPtr doc;              ///< parsed answer (assistant / notice); null = not parsed yet
    QString doc_text;          ///< the text `doc` was parsed from
    bool doc_streaming = false;///< `doc` is the streaming form (tail block open)
    int tail_begin = 0;        ///< streaming: offset in turn.text where the open block starts
    int tail_index = 0;        ///< streaming: closed blocks before it
    bool parsing = false;      ///< a worker parse is in flight
    QString parsing_text;
    QString pending;           ///< streamed, not merged into turn.text yet
    QString thinking_pending;
    bool body_dirty = false;   ///< merged text not yet shown
    bool think_dirty = false;
    qsizetype chars = 0;       ///< estimate inputs
    qsizetype breaks = 0;
    int height = 0;            ///< measured at measured_w, else estimated
    int measured_w = -1;
    QPointer<AicTurnWidget> widget;
    int tools_open = -1;       ///< -1 automatic, 0 collapsed, 1 expanded (the user's choice)
    bool think_open = false;
};

struct AicTranscriptView::Private {
    explicit Private(AicTranscriptView* view) : q(view) {}

    AicTranscriptView* q;
    std::vector<std::unique_ptr<Entry>> entries;
    QHash<quint64, int> index;
    QVector<int> offsets; ///< entries.size() + 1 prefix sums of the heights
    bool offsets_dirty = true;
    quint64 next_id = 1;
    QSet<QString> known_symbols;
    bool actions_enabled = true;
    QString report_source;
    QHash<QString, QPair<QString, int>> jobs;
    QSet<quint64> pending_ids;
    QTimer* flush_timer = nullptr;
    QTimer* layout_timer = nullptr;
    QPushButton* pill = nullptr;
    quint64 parse_gen = 1;
    quint64 inline_gen = 0;
    quint64 fade_id = 0;
    bool in_layout = false;
    bool programmatic = false;
    int resize_chain = 0; ///< layouts in a row re-run because the viewport resized during one
    int col_x = 0;
    int col_w = 0;
    int line_h = 18;
    int char_w = 8;

    bool compact() const { return q->density_ == Density::Compact; }
    Entry* find(quint64 id) const;
    int index_of(quint64 id) const;
    void reindex();
    ParseOptions parse_options() const;
    int pad_top() const { return compact() ? style::u(1) : style::u(3); }
    int pad_bottom() const { return compact() ? style::u(2) : style::u(6); }
    int top_of(int i) const { return pad_top() + offsets.value(i); }
    int max_scroll() const;
    int index_at(int y) const;
    void ensure_offsets();
    void update_metrics();
    void update_column();
    void count_text(Entry& e) const;
    int estimate(const Entry& e) const;
    void merge_pending(Entry& e) const;

    void ensure_doc(Entry& e);
    void stream_parse(Entry& e);
    void start_parse(Entry& e);

    AicTurnWidget* realise(Entry& e);
    bool measure(Entry& e);
    void evict(Entry& e);
    void layout_now();
    void schedule_layout();
    void notify_dirty(quint64 id);
    void set_follow(bool on);
    void show_pill();
    void place_pill();
    void flush_streams();
    void retranslate();
    void on_appearance_settled();

    void set_current(quint64 id);
    void ensure_visible(int idx);
    void move_current(int delta);
    void scroll_to(int y);

    void copy_turn(quint64 id);
    void request(quint64 id, TurnAction a);
    fincept::report::ReportPayload report_payload(quint64 id) const;
    void add_report_entries(QMenu* menu, quint64 id);
    void fill_turn_menu(QMenu* menu, quint64 id);
    void popup_turn_menu(quint64 id, const QPoint& global_pos);
    void toggle_section(quint64 id, bool tools);
};

// ── Private: entries, offsets, estimates ─────────────────────────────────────

AicTranscriptView::Entry* AicTranscriptView::Private::find(quint64 id) const {
    const int i = index_of(id);
    return i >= 0 ? entries[size_t(i)].get() : nullptr;
}

int AicTranscriptView::Private::index_of(quint64 id) const {
    if (id == 0)
        return -1;
    const auto it = index.constFind(id);
    return it == index.constEnd() ? -1 : it.value();
}

void AicTranscriptView::Private::reindex() {
    index.clear();
    index.reserve(int(entries.size()));
    for (int i = 0; i < int(entries.size()); ++i)
        index.insert(entries[size_t(i)]->turn.id, i);
    offsets_dirty = true;
}

ParseOptions AicTranscriptView::Private::parse_options() const {
    ParseOptions o;
    o.known_symbols = known_symbols;
    return o;
}

int AicTranscriptView::Private::max_scroll() const {
    const int n = int(entries.size());
    const int total = pad_top() + offsets.value(n) + pad_bottom();
    return std::max(0, total - q->viewport()->height());
}

/// The first entry whose bottom is below content y (entries.size() when none).
int AicTranscriptView::Private::index_at(int y) const {
    const int n = int(entries.size());
    if (n == 0 || offsets.size() != n + 1)
        return 0;
    const int rel = y - pad_top();
    const auto it = std::upper_bound(offsets.constBegin() + 1, offsets.constEnd(), rel);
    return int(it - (offsets.constBegin() + 1));
}

void AicTranscriptView::Private::ensure_offsets() {
    const int n = int(entries.size());
    if (!offsets_dirty && offsets.size() == n + 1)
        return;
    offsets.resize(n + 1);
    int y = 0;
    offsets[0] = 0;
    for (int i = 0; i < n; ++i) {
        y += std::max(1, entries[size_t(i)]->height);
        offsets[i + 1] = y;
    }
    offsets_dirty = false;
}

void AicTranscriptView::Private::update_metrics() {
    const QFontMetrics fm(style::ui_font());
    line_h = std::max(8, fm.lineSpacing());
    char_w = std::max(1, style::avg_char());
}

void AicTranscriptView::Private::update_column() {
    const int vw = q->viewport()->width();
    int w = 0;
    int x = 0;
    if (compact()) {
        w = vw - 2 * style::u(2);
        x = style::u(2);
    } else {
        const int gutter = vw >= 900 ? style::u(6) : style::u(4);
        w = std::min(vw - 2 * gutter, tr_kMaxColumnChars * char_w + style::u(3));
    }
    w = std::max(w, style::u(30));
    if (!compact())
        x = std::max(0, (vw - w) / 2);
    col_x = x;
    if (w == col_w)
        return;
    col_w = w;
    for (auto& up : entries)
        if (!up->widget)
            up->height = estimate(*up);
    offsets_dirty = true;
}

void AicTranscriptView::Private::count_text(Entry& e) const {
    e.chars = e.turn.text.size() + e.pending.size();
    e.breaks = e.turn.text.count(QLatin1Char('\n')) + e.pending.count(QLatin1Char('\n'));
}

int AicTranscriptView::Private::estimate(const Entry& e) const {
    const Turn& t = e.turn;
    const int text_w = std::max(1, col_w - style::u(5));
    const qsizetype cpl = std::max(16, text_w / std::max(1, char_w));
    qsizetype lines = e.chars / cpl + 1 + (t.role == TurnRole::User ? e.breaks : e.breaks / 2);
    if (t.role == TurnRole::User && e.chars > tr_kUserShowChars)
        lines = tr_kUserShowChars / cpl + 1;
    lines = std::min<qsizetype>(lines, 200000);
    int h = tr_header_h() + style::u(5) + 1 + int(lines) * line_h;
    if (!t.thinking.isEmpty())
        h += line_h + style::u(2);
    if (!t.tools.isEmpty())
        h += line_h + style::u(2);
    if (!t.attachments.isEmpty())
        h += style::button_h() + style::u(1);
    if (t.role == TurnRole::Notice)
        h += style::button_h() + style::u(5);
    if (t.role == TurnRole::Assistant && (t.state == TurnState::Failed || t.state == TurnState::Empty))
        h += line_h + style::button_h() + style::u(4);
    return h;
}

void AicTranscriptView::Private::merge_pending(Entry& e) const {
    if (!e.pending.isEmpty()) {
        e.turn.text += e.pending;
        e.pending.clear();
        e.body_dirty = true;
    }
    if (!e.thinking_pending.isEmpty()) {
        e.turn.thinking += e.thinking_pending;
        e.thinking_pending.clear();
        e.think_dirty = true;
    }
}

// ── Private: parsing ─────────────────────────────────────────────────────────

void AicTranscriptView::Private::ensure_doc(Entry& e) {
    const Turn& t = e.turn;
    if (t.role == TurnRole::User)
        return;
    const bool streaming = t.state == TurnState::Streaming;
    if (t.text.isEmpty()) {
        if (!e.doc || !e.doc->blocks.isEmpty() || !e.doc_text.isEmpty()) {
            e.doc = std::make_shared<const ParsedDoc>();
            e.doc_text.clear();
        }
        e.doc_streaming = streaming;
        e.tail_begin = 0;
        e.tail_index = 0;
        return;
    }
    if (streaming) {
        stream_parse(e);
        return;
    }
    if (e.doc && !e.doc_streaming && e.doc_text == t.text)
        return;
    if (t.text.size() > tr_kAsyncParseChars) {
        // Keep showing what was streamed while the final parse runs; an unrelated old
        // document gives way to the "rendering…" line.
        if (e.doc && !t.text.startsWith(e.doc_text))
            e.doc.reset();
        start_parse(e);
        return;
    }
    e.doc = std::make_shared<const ParsedDoc>(parse_markdown(t.text, parse_options()));
    e.doc_text = t.text;
    e.doc_streaming = false;
}

void AicTranscriptView::Private::stream_parse(Entry& e) {
    const QString& text = e.turn.text;
    if (e.doc && e.doc_streaming && e.doc_text == text)
        return;
    const ParseOptions opts = parse_options();
    if (!e.doc || !e.doc_streaming || e.tail_begin > text.size() || !text.startsWith(e.doc_text) ||
        e.tail_index > int(e.doc->blocks.size())) {
        ParsedDoc full = parse_markdown(text, opts, true);
        e.tail_index = std::max(0, int(full.blocks.size()) - 1);
        e.tail_begin = full.blocks.isEmpty() ? 0 : full.blocks.constLast().src_begin;
        e.doc = std::make_shared<const ParsedDoc>(std::move(full));
    } else {
        // Only the open block (and whatever arrived after it) is parsed again.
        const ParsedDoc part = parse_markdown(text.mid(e.tail_begin), opts, true);
        auto nd = std::make_shared<ParsedDoc>();
        nd->blocks = e.doc->blocks.mid(0, e.tail_index);
        nd->blocks.reserve(e.tail_index + part.blocks.size());
        for (Block b : part.blocks) {
            b.src_begin += e.tail_begin;
            b.src_end += e.tail_begin;
            nd->blocks.append(std::move(b));
        }
        nd->tickers = e.doc->tickers;
        for (const QString& s : part.tickers)
            if (!nd->tickers.contains(s))
                nd->tickers.append(s);
        nd->sources = e.doc->sources;
        for (SourceLink s : part.sources) {
            bool known = false;
            for (const SourceLink& k : std::as_const(nd->sources))
                known = known || k.url == s.url;
            if (known)
                continue;
            s.number = int(nd->sources.size()) + 1;
            nd->sources.append(s);
        }
        if (part.blocks.size() >= 2) {
            e.tail_index = int(nd->blocks.size()) - 1;
            e.tail_begin = nd->blocks.constLast().src_begin;
        }
        e.doc = std::move(nd);
    }
    e.doc_text = text;
    e.doc_streaming = true;
}

void AicTranscriptView::Private::start_parse(Entry& e) {
    const QString text = e.turn.text;
    if (e.parsing && e.parsing_text == text)
        return;
    e.parsing = true;
    e.parsing_text = text;
    const quint64 gen = parse_gen;
    const quint64 id = e.turn.id;
    const ParseOptions opts = parse_options();
    QPointer<AicTranscriptView> pv(q);
    auto* watcher = new QFutureWatcher<TrDocPtr>(q);
    QObject::connect(watcher, &QFutureWatcherBase::finished, q, [pv, watcher, gen, id, text]() {
        watcher->deleteLater();
        if (!pv || pv->d_->parse_gen != gen)
            return;
        Private* d = pv->d_.get();
        Entry* x = d->find(id);
        if (!x)
            return;
        if (x->parsing_text == text)
            x->parsing = false;
        if (x->turn.text != text || x->turn.state == TurnState::Streaming)
            return; // superseded by newer text: its own parse (or the stream) takes over
        x->doc = watcher->result();
        x->doc_text = text;
        x->doc_streaming = false;
        if (x->widget) {
            x->widget->sync_body();
            x->measured_w = -1;
        }
        d->schedule_layout();
    });
    watcher->setFuture(QtConcurrent::run([text, opts]() -> TrDocPtr {
        return std::make_shared<const ParsedDoc>(parse_markdown(text, opts));
    }));
}

// ── Private: layout ──────────────────────────────────────────────────────────

AicTurnWidget* AicTranscriptView::Private::realise(Entry& e) {
    ensure_doc(e);
    auto* w = new AicTurnWidget(q, e.turn.id, q->viewport());
    e.widget = w;
    e.body_dirty = false;
    e.think_dirty = false;
    if (e.turn.id == q->current_)
        w->set_current(true);
    e.measured_w = -1;
    measure(e);
    if (e.turn.id == fade_id && style::motion())
        w->fade_in();
    return w;
}

bool AicTranscriptView::Private::measure(Entry& e) {
    if (!e.widget)
        return false;
    const int h = std::max(1, e.widget->measure(col_w));
    e.measured_w = col_w;
    if (h == e.height)
        return false;
    e.height = h;
    offsets_dirty = true;
    return true;
}

void AicTranscriptView::Private::evict(Entry& e) {
    QWidget* w = e.widget;
    e.widget = nullptr;
    if (!w)
        return;
    w->hide();
    w->deleteLater();
}

void AicTranscriptView::Private::layout_now() {
    if (in_layout)
        return;
    in_layout = true;
    layout_timer->stop();
    update_column();
    const QSize vp_size = q->viewport()->size();

    QScrollBar* sb = q->verticalScrollBar();
    const int vh = std::max(1, q->viewport()->height());
    const int n = int(entries.size());
    ensure_offsets();

    // Anchor: the first visible turn keeps its screen position while heights above change.
    int scroll = sb->value();
    int anchor = -1;
    int anchor_dy = 0;
    if (!q->follow_ && n > 0) {
        anchor = index_at(scroll);
        if (anchor >= n)
            anchor = -1;
        else
            anchor_dy = top_of(anchor) - scroll;
    }
    auto target = [&]() {
        const int mx = max_scroll();
        if (q->follow_)
            return mx;
        if (anchor >= 0)
            return std::clamp(top_of(anchor) - anchor_dy, 0, mx);
        return std::clamp(scroll, 0, mx);
    };

    for (auto& up : entries)
        if (up->widget && up->measured_w != col_w)
            measure(*up);

    // Realise the visible range plus one viewport above and below; realising measures, and
    // measured heights move the range, so settle it in a few passes.
    for (int pass = 0; pass < 4; ++pass) {
        ensure_offsets();
        scroll = target();
        const int lo = scroll - vh;
        const int hi = scroll + 2 * vh;
        for (int i = index_at(std::max(0, lo)); i < n && top_of(i) < hi; ++i) {
            Entry& e = *entries[size_t(i)];
            if (!e.widget)
                realise(e);
            else if (e.measured_w != col_w)
                measure(e);
        }
        if (!offsets_dirty)
            break;
    }
    ensure_offsets();
    scroll = target();

    programmatic = true;
    sb->setRange(0, max_scroll());
    sb->setPageStep(vh);
    sb->setSingleStep(std::max(8, line_h));
    sb->setValue(scroll);
    programmatic = false;
    scroll = sb->value();

    const int keep_lo = scroll - 2 * vh;
    const int keep_hi = scroll + 3 * vh;
    QWidget* focus = QApplication::focusWidget();
    for (int i = 0; i < n; ++i) {
        Entry& e = *entries[size_t(i)];
        if (!e.widget)
            continue;
        const int top = top_of(i);
        const int bottom = top + e.height;
        const bool keep = e.turn.state == TurnState::Streaming || e.turn.id == q->current_ ||
                          (focus && e.widget->isAncestorOf(focus));
        if (!keep && (bottom < keep_lo || top > keep_hi)) {
            evict(e);
            continue;
        }
        const QRect r(col_x, top - scroll, col_w, e.height);
        if (e.widget->geometry() != r)
            e.widget->setGeometry(r);
        if (e.widget->isHidden())
            e.widget->show();
    }
    fade_id = 0;
    place_pill();
    in_layout = false;
    // The scroll bar appearing / disappearing resized the viewport during this pass (its
    // resize event was swallowed by in_layout): lay out again at the new width.
    // (Bounded: content right at the scroll bar's threshold must not ping-pong forever.)
    if (q->viewport()->size() == vp_size)
        resize_chain = 0;
    else if (++resize_chain <= 3)
        schedule_layout();
}

void AicTranscriptView::Private::schedule_layout() {
    if (!layout_timer->isActive())
        layout_timer->start();
}

void AicTranscriptView::Private::notify_dirty(quint64 id) {
    Entry* e = find(id);
    if (!e || !e->widget)
        return;
    e->measured_w = -1;
    schedule_layout();
}

void AicTranscriptView::Private::set_follow(bool on) {
    if (q->follow_ == on)
        return;
    q->follow_ = on;
    if (on && pill)
        pill->hide();
    emit q->following_changed(on);
}

void AicTranscriptView::Private::show_pill() {
    if (!pill || q->follow_)
        return;
    if (pill->isHidden())
        pill->show();
    place_pill();
}

void AicTranscriptView::Private::place_pill() {
    if (!pill || pill->isHidden())
        return;
    pill->adjustSize();
    const QWidget* vp = q->viewport();
    pill->move(std::max(0, (vp->width() - pill->width()) / 2), std::max(0, vp->height() - pill->height() - style::u(3)));
    pill->raise();
}

void AicTranscriptView::Private::flush_streams() {
    if (pending_ids.isEmpty())
        return;
    const QSet<quint64> ids = std::exchange(pending_ids, {});
    bool grew = false;
    for (const quint64 id : ids) {
        Entry* e = find(id);
        if (!e)
            continue;
        merge_pending(*e);
        if (!e->body_dirty && !e->think_dirty)
            continue;
        grew = true;
        count_text(*e);
        if (e->widget) {
            if (e->think_dirty)
                e->widget->sync_thinking();
            if (e->body_dirty)
                e->widget->sync_body();
            e->measured_w = -1;
        } else {
            e->height = estimate(*e);
            offsets_dirty = true;
        }
        e->body_dirty = false;
        e->think_dirty = false;
    }
    if (grew)
        show_pill();
    layout_now();
}

void AicTranscriptView::Private::retranslate() {
    if (pill) {
        pill->setText(QString(QChar(0x2193)) + QStringLiteral(" ") + AicTranscriptView::tr("NEW"));
        pill->setAccessibleName(AicTranscriptView::tr("Jump to the newest content"));
        pill->setToolTip(AicTranscriptView::tr("New content below. Click or press End to jump to it."));
    }
    // Rare: rebuild the realised turns (their texts come from tr()).
    for (auto& up : entries)
        evict(*up);
    layout_now();
}

void AicTranscriptView::Private::on_appearance_settled() {
    const quint64 gen = style::inline_generation();
    const bool rerender = gen != inline_gen;
    inline_gen = gen;
    update_metrics();
    col_w = 0; // re-estimate the unrealised turns at the new metrics
    for (auto& up : entries) {
        if (!up->widget)
            continue;
        up->widget->restyle(rerender);
        up->measured_w = -1;
    }
    if (pill && !pill->isHidden())
        place_pill();
    layout_now();
}

// ── Private: keyboard / scrolling ────────────────────────────────────────────

void AicTranscriptView::Private::set_current(quint64 id) {
    if (q->current_ == id)
        return;
    if (Entry* old = find(q->current_))
        if (old->widget)
            old->widget->set_current(false);
    q->current_ = id;
    if (Entry* e = find(id))
        if (e->widget)
            e->widget->set_current(true);
}

void AicTranscriptView::Private::scroll_to(int y) {
    layout_now();
    const int mx = max_scroll();
    const int v = std::clamp(y, 0, mx);
    set_follow(v >= mx - tr_kFollowSlack);
    QScrollBar* sb = q->verticalScrollBar();
    if (sb->value() != v)
        sb->setValue(v); // scrollContentsBy → layout_now
    else
        layout_now();
}

void AicTranscriptView::Private::ensure_visible(int idx) {
    if (idx < 0 || idx >= int(entries.size()))
        return;
    layout_now();
    const int vh = q->viewport()->height();
    const int top = top_of(idx);
    const int bottom = top + entries[size_t(idx)]->height;
    int v = q->verticalScrollBar()->value();
    if (top < v)
        v = top - style::u(2);
    else if (bottom > v + vh)
        v = std::min(top - style::u(2), bottom - vh + style::u(2));
    scroll_to(v);
}

void AicTranscriptView::Private::move_current(int delta) {
    const int n = int(entries.size());
    if (n == 0)
        return;
    int idx = index_of(q->current_);
    if (idx < 0) {
        // Start from what is on screen: the first visible turn going down, the last going up.
        ensure_offsets();
        const int v = q->verticalScrollBar()->value();
        if (delta > 0) {
            idx = std::min(index_at(v), n - 1);
        } else {
            idx = std::min(index_at(v + q->viewport()->height() - 1), n - 1);
        }
    } else {
        idx = std::clamp(idx + delta, 0, n - 1);
    }
    set_current(entries[size_t(idx)]->turn.id);
    ensure_visible(idx);
}

// ── Private: actions ─────────────────────────────────────────────────────────

void AicTranscriptView::Private::copy_turn(quint64 id) {
    Entry* e = find(id);
    if (!e)
        return;
    Turn t = e->turn;
    t.text += e->pending;
    t.thinking += e->thinking_pending;
    const QString md = turn_markdown(t);
    QApplication::clipboard()->setText(md);
    emit q->status_message(AicTranscriptView::tr("Copied %1 characters").arg(QLocale().toString(qlonglong(md.size()))));
    emit q->action_requested(id, TurnAction::Copy);
}

void AicTranscriptView::Private::request(quint64 id, TurnAction a) {
    if (a == TurnAction::Copy) {
        copy_turn(id);
        return;
    }
    if (!find(id))
        return;
    emit q->action_requested(id, a);
}

fincept::report::ReportPayload AicTranscriptView::Private::report_payload(quint64 id) const {
    fincept::report::ReportPayload p;
    const Entry* e = find(id);
    if (!e)
        return p;
    const QString text = e->turn.text + e->pending;
    const QString heading = tr_first_heading(text);
    p.title = heading.isEmpty() ? AicTranscriptView::tr("AI Chat answer") : heading;
    p.source = report_source.isEmpty() ? AicTranscriptView::tr("AI Chat") : report_source;
    p.as_of = e->turn.when.isValid() ? e->turn.when : QDateTime::currentDateTime();
    p.markdown = text;
    return p;
}

void AicTranscriptView::Private::add_report_entries(QMenu* menu, quint64 id) {
    QPointer<AicTranscriptView> pv(q);
    const QString source = report_source.isEmpty() ? AicTranscriptView::tr("AI Chat") : report_source;
    fincept::ui::add_send_to_report_action(menu, source, [pv, id]() {
        return pv ? pv->d_->report_payload(id) : fincept::report::ReportPayload{};
    });
    QObject::connect(menu, &QMenu::triggered, q, [pv, id](QAction*) {
        if (pv)
            emit pv->action_requested(id, TurnAction::AddToReport);
    });
}

void AicTranscriptView::Private::toggle_section(quint64 id, bool tools) {
    Entry* e = find(id);
    if (!e)
        return;
    if (tools) {
        const ToolSummary s = summarize_tools(e->turn.tools);
        const bool open = e->tools_open >= 0 ? e->tools_open == 1 : (!compact() && (s.running > 0 || s.failed > 0));
        e->tools_open = open ? 0 : 1;
        if (e->widget)
            e->widget->sync_tools();
    } else {
        e->think_open = !e->think_open;
        if (e->widget)
            e->widget->sync_thinking();
    }
    notify_dirty(id);
}

void AicTranscriptView::Private::fill_turn_menu(QMenu* menu, quint64 id) {
    Entry* e = find(id);
    if (!e)
        return;
    const Turn& t = e->turn;
    using V = AicTranscriptView;
    const bool live = q->live_turn_ == id;
    const bool en = actions_enabled;
    QPointer<AicTranscriptView> pv(q);
    auto add = [&](const QString& text, TurnAction a, bool spend) {
        QAction* act = menu->addAction(text);
        if (spend)
            act->setProperty(fincept::core::agent::kAgentRiskProperty, tr_spend());
        QObject::connect(act, &QAction::triggered, q, [pv, id, a]() {
            if (pv)
                pv->d_->request(id, a);
        });
    };
    add(V::tr("Copy"), TurnAction::Copy, false);
    if (compact())
        return;
    if (live && en && tr_can_regenerate(t))
        add(V::tr("Regenerate"), TurnAction::Regenerate, true);
    if (live && en && tr_can_retry(t))
        add(V::tr("Retry"), TurnAction::Retry, true);
    if (t.role == TurnRole::User && en)
        add(V::tr("Edit && resend"), TurnAction::EditResend, false);
    if (t.role != TurnRole::Notice && !(t.text + e->pending).isEmpty())
        add(V::tr("Quote"), TurnAction::Quote, false);
    if (t.role == TurnRole::Assistant && t.state != TurnState::Streaming && !t.text.isEmpty())
        add_report_entries(menu->addMenu(V::tr("Add to report")), id);
    if (t.role == TurnRole::Notice) {
        const QVector<TrNoticeAction> acts = tr_notice_actions(t);
        bool sep = false;
        for (const TrNoticeAction& na : acts) {
            if (na.action == TurnAction::Retry || (na.live_only && !(live && en)))
                continue;
            if (!sep) {
                menu->addSeparator();
                sep = true;
            }
            add(na.text, na.action, na.spend);
        }
    }
    if (!t.thinking.isEmpty() || !t.tools.isEmpty()) {
        menu->addSeparator();
        if (!t.thinking.isEmpty()) {
            QAction* a = menu->addAction(e->think_open ? V::tr("Hide reasoning") : V::tr("Show reasoning"));
            QObject::connect(a, &QAction::triggered, q, [pv, id]() {
                if (pv)
                    pv->d_->toggle_section(id, false);
            });
        }
        if (!t.tools.isEmpty()) {
            QAction* a = menu->addAction(V::tr("Show or hide tool calls"));
            QObject::connect(a, &QAction::triggered, q, [pv, id]() {
                if (pv)
                    pv->d_->toggle_section(id, true);
            });
        }
    }
    // Links of the answer, so tickers and sources are reachable from the keyboard too.
    if (e->doc && (!e->doc->tickers.isEmpty() || !e->doc->sources.isEmpty())) {
        QMenu* links = menu->addMenu(V::tr("Links"));
        int k = 0;
        for (const QString& sym : e->doc->tickers) {
            if (++k > tr_kMaxMenuLinks)
                break;
            QAction* a = links->addAction(V::tr("Research %1").arg(fincept::ui::literal_ampersands(sym)));
            QObject::connect(a, &QAction::triggered, q, [sym]() {
                fincept::screens::equity_links::open_symbol_in(QStringLiteral("equity_research"), sym);
            });
        }
        k = 0;
        for (const SourceLink& s : e->doc->sources) {
            if (++k > tr_kMaxMenuLinks)
                break;
            const QString url = s.url;
            QAction* a = links->addAction(QStringLiteral("[%1] %2").arg(QString::number(s.number),
                                                                         fincept::ui::literal_ampersands(s.host)));
            a->setToolTip(url);
            QObject::connect(a, &QAction::triggered, q, [pv, id, url]() {
                if (pv)
                    pv->open_link(id, url);
            });
        }
    }
}

void AicTranscriptView::Private::popup_turn_menu(quint64 id, const QPoint& global_pos) {
    if (!find(id))
        return;
    auto* m = new QMenu(q);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->setStyleSheet(style::menu_sheet());
    fill_turn_menu(m, id);
    if (m->isEmpty()) {
        delete m;
        return;
    }
    m->popup(global_pos);
}

// ── AicTurnWidget ────────────────────────────────────────────────────────────

AicTurnWidget::AicTurnWidget(AicTranscriptView* view, quint64 id, QWidget* parent)
    : QWidget(parent), view_(view), id_(id) {
    setObjectName(QLatin1String(style::kTurn));
    setAttribute(Qt::WA_StyledBackground);
    setProperty("current", false);
    root_ = new QVBoxLayout(this);
    const bool c = compact();
    root_->setContentsMargins(c ? style::u(2) : style::u(3), c ? style::u(1) : style::u(2), c ? style::u(1) : style::u(2),
                              (c ? style::u(1) : style::u(3)) + 1);
    root_->setSpacing(c ? style::u(1) : style::u(2));
    build();
}

AicTranscriptView::Entry* AicTurnWidget::entry() const {
    return view_ && view_->d_ ? view_->d_->find(id_) : nullptr;
}

AicTranscriptView::Private* AicTurnWidget::priv() const {
    return view_ ? view_->d_.get() : nullptr;
}

bool AicTurnWidget::compact() const {
    return view_ && view_->density_ == AicTranscriptView::Density::Compact;
}

void AicTurnWidget::put(Slot slot, QWidget* w) {
    slots_[slot] = w;
    int at = root_->count();
    for (int k = slot + 1; k < SlotCount; ++k) {
        if (slots_[k]) {
            at = root_->indexOf(slots_[k]);
            break;
        }
    }
    root_->insertWidget(at, w);
}

void AicTurnWidget::touched(QWidget* changed) {
    tr_invalidate_up(changed, this);
    if (AicTranscriptView::Private* d = priv())
        d->notify_dirty(id_);
}

void AicTurnWidget::build() {
    const AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    const Turn& t = e->turn;
    role_kind_ = t.role;

    // Header line: role · meta · state … actions (fixed height, never stretches).
    header_ = new QWidget(this);
    header_->setFixedHeight(tr_header_h());
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(style::u(2));
    role_ = tr_make_label(style::kTurnRole, header_);
    role_->setProperty("role", t.role == TurnRole::User        ? QStringLiteral("user")
                               : t.role == TurnRole::Assistant ? QStringLiteral("ai")
                                                               : QStringLiteral("notice"));
    meta_ = tr_make_label(style::kTurnMeta, header_);
    pulse_ = new AicTrPulse(header_);
    pulse_->hide();
    tag_ = tr_make_label(style::kChip, header_);
    tag_->hide();
    note_ = new AicTrElide(header_);
    note_->setObjectName(QLatin1String(style::kDim));
    actions_ = new QWidget(header_);
    auto* al = new QHBoxLayout(actions_);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(style::u(1));
    auto add_btn = [&](QPushButton*& b, auto&& slot) {
        b = tr_make_button(style::kLinkBtn, actions_);
        al->addWidget(b);
        QObject::connect(b, &QPushButton::clicked, this, slot);
    };
    add_btn(b_copy_, [this]() {
        if (auto* d = priv())
            d->copy_turn(id_);
    });
    if (!compact()) {
        if (t.role == TurnRole::Assistant) {
            add_btn(b_regen_, [this]() {
                if (auto* d = priv())
                    d->request(id_, TurnAction::Regenerate);
            });
            b_regen_->setProperty(fincept::core::agent::kAgentRiskProperty, tr_spend());
        }
        if (t.role != TurnRole::User) {
            add_btn(b_retry_, [this]() {
                if (auto* d = priv())
                    d->request(id_, TurnAction::Retry);
            });
            b_retry_->setProperty(fincept::core::agent::kAgentRiskProperty, tr_spend());
        }
        if (t.role == TurnRole::User)
            add_btn(b_edit_, [this]() {
                if (auto* d = priv())
                    d->request(id_, TurnAction::EditResend);
            });
        if (t.role == TurnRole::Assistant)
            add_btn(b_report_, [this]() { popup_report_menu(); });
        if (t.role != TurnRole::Notice)
            add_btn(b_quote_, [this]() {
                if (auto* d = priv())
                    d->request(id_, TurnAction::Quote);
            });
    }
    retranslate_actions();
    actions_->hide();
    hl->addWidget(role_);
    hl->addWidget(meta_);
    hl->addWidget(pulse_, 0, Qt::AlignVCenter);
    hl->addWidget(tag_, 0, Qt::AlignVCenter);
    hl->addWidget(note_, 1);
    hl->addWidget(actions_);
    put(SlotHeader, header_);

    // Content: the body blocks, inside the notice frame for a notice.
    body_ = new QWidget(this);
    body_l_ = new QVBoxLayout(body_);
    body_l_->setContentsMargins(0, 0, 0, 0);
    body_l_->setSpacing(compact() ? style::u(1) : style::u(2));
    if (t.role == TurnRole::Notice) {
        notice_ = new QFrame(this);
        notice_->setObjectName(QLatin1String(style::kErrorBlock));
        notice_->setProperty("tone", tr_notice_tone(t.notice));
        auto* nl = new QVBoxLayout(notice_);
        nl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(2));
        nl->setSpacing(style::u(2));
        body_->setParent(notice_);
        nl->addWidget(body_);
        notice_btns_ = new QWidget(notice_);
        auto* bl = new QHBoxLayout(notice_btns_);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(style::u(2));
        bl->addStretch(1);
        nl->addWidget(notice_btns_);
        notice_btns_->hide();
        put(SlotContent, notice_);
    } else {
        put(SlotContent, body_);
    }
    sync_all();
}

void AicTurnWidget::retranslate_actions() {
    using V = AicTranscriptView;
    if (b_copy_)
        tr_label_button(b_copy_, V::tr("COPY"), V::tr("Copy this turn"), V::tr("Copy this turn as Markdown (Ctrl+C)"));
    if (b_regen_)
        tr_label_button(b_regen_, V::tr("REGENERATE"), V::tr("Regenerate the answer"),
                        V::tr("Ask again for a new answer (Ctrl+R). Uses the model again."));
    if (b_retry_)
        tr_label_button(b_retry_, V::tr("RETRY"), V::tr("Retry the request"),
                        V::tr("Send the request again (Ctrl+R). Uses the model again."));
    if (b_edit_)
        tr_label_button(b_edit_, V::tr("EDIT && RESEND"), V::tr("Edit and resend"),
                        V::tr("Put this message into the composer to edit it (nothing is sent)"));
    if (b_report_)
        tr_label_button(b_report_, V::tr("REPORT") + QStringLiteral(" ") + QChar(0x25BE), V::tr("Add to report"),
                        V::tr("Add this answer to a report"));
    if (b_quote_)
        tr_label_button(b_quote_, V::tr("QUOTE"), V::tr("Quote in the composer"),
                        V::tr("Quote this turn in the composer"));
}

void AicTurnWidget::sync_all() {
    refresh_header();
    sync_thinking();
    sync_tools();
    sync_body();
    refresh_actions();
    const AicTranscriptView::Entry* e = entry();
    set_dimmed(e && e->turn.state == TurnState::Superseded);
}

void AicTurnWidget::refresh_header() {
    const AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    using V = AicTranscriptView;
    const Turn& t = e->turn;
    QString role;
    if (t.role == TurnRole::User)
        role = V::tr("YOU");
    else if (t.role == TurnRole::Assistant)
        role = turn_speaker(t).toUpper();
    else
        role = (t.notice == NoticeKind::Info || t.notice == NoticeKind::Error) ? V::tr("SYSTEM") : V::tr("NOTICE");
    role_->setText(role);

    QStringList parts;
    const QString when = tr_when(t.when);
    if (!when.isEmpty())
        parts << when;
    QString tok_tip;
    if (t.role != TurnRole::User && t.tokens > 0) {
        parts << V::tr("%1 tok").arg(format_count(t.tokens));
        if (t.prompt_tokens >= 0 || t.completion_tokens >= 0)
            tok_tip = V::tr("%1 in · %2 out")
                          .arg(t.prompt_tokens >= 0 ? QLocale().toString(t.prompt_tokens) : QStringLiteral("?"),
                               t.completion_tokens >= 0 ? QLocale().toString(t.completion_tokens) : QStringLiteral("?"));
    }
    if (t.role == TurnRole::Assistant && t.duration_ms >= 0)
        parts << format_ms(t.duration_ms);
    if (t.cost_usd >= 0.0)
        parts << QStringLiteral("$") + QString::number(t.cost_usd, 'f', 4);
    meta_->setText(parts.join(QStringLiteral(" · ")));
    meta_->setToolTip(tok_tip);
    meta_->setVisible(!parts.isEmpty());
    note_->setText(t.billing_note);
    note_->setToolTip(t.billing_note);

    QString tag;
    QString tone;
    switch (t.state) {
        case TurnState::Streaming:
            tag = V::tr("STREAMING");
            tone = QStringLiteral("accent");
            break;
        case TurnState::Stopped:
            tag = V::tr("STOPPED");
            tone = QStringLiteral("warn");
            break;
        case TurnState::Failed:
            tag = V::tr("FAILED");
            tone = QStringLiteral("danger");
            break;
        case TurnState::Superseded:
            tag = V::tr("SUPERSEDED — REGENERATED BELOW");
            break;
        case TurnState::Done:
        case TurnState::Empty:
            break;
    }
    tag_->setText(tag);
    tr_set_prop(tag_, "tone", tone);
    tag_->setVisible(!tag.isEmpty());
    pulse_->setVisible(t.state == TurnState::Streaming);
    setAccessibleName(when.isEmpty() ? role : V::tr("%1, %2").arg(role, when));
}

void AicTurnWidget::refresh_actions() {
    const AicTranscriptView::Entry* e = entry();
    const AicTranscriptView::Private* d = priv();
    if (!e || !d)
        return;
    const Turn& t = e->turn;
    const bool live = view_->live_turn_ == id_;
    const bool en = d->actions_enabled;
    const bool has_text = !(t.text.isEmpty() && e->pending.isEmpty());
    if (b_regen_)
        b_regen_->setVisible(live && en && tr_can_regenerate(t));
    if (b_retry_)
        b_retry_->setVisible(live && en && tr_can_retry(t));
    if (b_edit_)
        b_edit_->setVisible(en);
    if (b_report_)
        b_report_->setVisible(t.state != TurnState::Streaming && has_text);
    if (b_quote_)
        b_quote_->setVisible(has_text);
    actions_->setVisible(hovered_ || current_);
    sync_notice_buttons();
    sync_error();
}

void AicTurnWidget::sync_thinking() {
    AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    const QString& th = e->turn.thinking;
    if (th.isEmpty()) {
        if (think_ && !think_->isHidden()) {
            think_->hide();
            touched(think_);
        }
        return;
    }
    if (!think_) {
        think_ = new QFrame(this);
        think_->setObjectName(QLatin1String(style::kActivity));
        auto* vl = new QVBoxLayout(think_);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(style::u(1));
        think_head_ = tr_make_button(style::kActivityHead, think_);
        think_body_ = tr_make_label(style::kDim, think_);
        tr_make_wrapping(think_body_);
        think_body_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        think_body_->hide();
        vl->addWidget(think_head_);
        vl->addWidget(think_body_);
        QObject::connect(think_head_, &QPushButton::clicked, this, [this]() {
            if (auto* d = priv())
                d->toggle_section(id_, false);
        });
        put(SlotThink, think_);
    }
    const bool open = e->think_open;
    using V = AicTranscriptView;
    // While the answer is still being worked out, the collapsed head shows the latest line of
    // reasoning, so the turn visibly moves without having to expand the section.
    QString head = V::tr("REASONING · %1 chars").arg(QLocale().toString(qlonglong(th.size())));
    if (!open && e->turn.state == TurnState::Streaming) {
        const QString last = th.trimmed().section(QLatin1Char('\n'), -1).simplified();
        if (!last.isEmpty())
            head = V::tr("THINKING") + QStringLiteral("  ·  ") + (last.size() > 96 ? last.left(95) + QChar(0x2026) : last);
    }
    think_head_->setText(QString(QChar(open ? 0x25BE : 0x25B8)) + QStringLiteral("  ") + head);
    think_head_->setAccessibleName(open ? V::tr("Hide reasoning") : V::tr("Show reasoning"));
    think_head_->setToolTip(V::tr("The model's reasoning before the answer"));
    if (open) {
        // The label holds text only while expanded: a collapsed section never lays it out.
        think_body_->setText(th.size() > tr_kThinkShowChars ? QString(QChar(0x2026)) + th.right(tr_kThinkShowChars) : th);
        think_body_->show();
    } else if (!think_body_->isHidden()) {
        think_body_->clear();
        think_body_->hide();
    }
    think_->show();
    touched(think_body_);
}

void AicTurnWidget::sync_tools() {
    AicTranscriptView::Entry* e = entry();
    AicTranscriptView::Private* d = priv();
    if (!e || !d)
        return;
    const QVector<ToolStep>& steps = e->turn.tools;
    if (steps.isEmpty()) {
        if (tools_ && !tools_->isHidden()) {
            tools_->hide();
            touched(tools_);
        }
        return;
    }
    if (!tools_) {
        tools_ = new QFrame(this);
        tools_->setObjectName(QLatin1String(style::kActivity));
        auto* vl = new QVBoxLayout(tools_);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(style::u(1));
        tools_head_ = tr_make_button(style::kActivityHead, tools_);
        tool_rows_ = new AicTrToolRows(tools_);
        tool_rows_->hide();
        vl->addWidget(tools_head_);
        vl->addWidget(tool_rows_);
        QObject::connect(tools_head_, &QPushButton::clicked, this, [this]() {
            if (auto* dd = priv())
                dd->toggle_section(id_, true);
        });
        put(SlotTools, tools_);
    }
    using V = AicTranscriptView;
    const ToolSummary s = summarize_tools(steps);
    const bool open = e->tools_open >= 0 ? e->tools_open == 1 : (!compact() && (s.running > 0 || s.failed > 0));
    QString head = QString(QChar(open ? 0x25BE : 0x25B8)) + QStringLiteral("  ") + V::tr("TOOLS") +
                   QStringLiteral(" · ") + V::tr("%n call(s)", "", s.calls);
    if (s.failed > 0)
        head += QStringLiteral(" · ") + V::tr("%n failed", "", s.failed);
    if (s.jobs > 0)
        head += QStringLiteral(" · ") + V::tr("%n in background", "", s.jobs);
    if (s.running > 0)
        head += QStringLiteral(" · ") + V::tr("running");
    else if (s.timed)
        head += QStringLiteral(" · ") + format_ms(s.ms);
    tools_head_->setText(head);
    tools_head_->setAccessibleName(open ? V::tr("Hide tool calls") : V::tr("Show tool calls"));
    tools_head_->setToolTip(V::tr("Tools the model called for this answer"));
    if (open) {
        const QVector<ToolRow> rows = tool_rows(steps);
        QVector<QPair<QString, int>> jobs;
        jobs.reserve(rows.size());
        for (const ToolRow& r : rows) {
            const auto it = r.job_id.isEmpty() ? d->jobs.constEnd() : d->jobs.constFind(r.job_id);
            jobs.append(it == d->jobs.constEnd() ? QPair<QString, int>(QString(), -1) : it.value());
        }
        static_cast<AicTrToolRows*>(tool_rows_)->set_rows(rows, jobs);
        tool_rows_->show();
    } else {
        tool_rows_->hide();
    }
    tools_->show();
    touched(tool_rows_);
}

void AicTurnWidget::sync_body() {
    AicTranscriptView::Entry* e = entry();
    AicTranscriptView::Private* d = priv();
    if (!e || !d)
        return;
    if (e->turn.role == TurnRole::User) {
        sync_user();
        return;
    }
    d->ensure_doc(*e);
    if (e->doc)
        sync_blocks(*e->doc, e->turn.state == TurnState::Streaming);
    else
        sync_blocks(ParsedDoc{}, false); // parse pending: the placeholder stands in
    sync_placeholder();
    sync_error();
    sync_notice_buttons();
    sync_sources();
    touched(body_);
}

void AicTurnWidget::sync_user() {
    const AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    using V = AicTranscriptView;
    const Turn& t = e->turn;
    if (t.attachments != chips_for_) {
        chips_for_ = t.attachments;
        if (chips_) {
            slots_[SlotChips] = nullptr;
            root_->removeWidget(chips_);
            chips_->hide();
            chips_->deleteLater();
            chips_ = nullptr;
        }
        if (!t.attachments.isEmpty()) {
            chips_ = new QWidget(this);
            auto* flow = new fincept::ui::FlowLayout(chips_, style::u(1), style::u(1));
            flow->setContentsMargins(0, 0, 0, 0);
            for (const Attachment& a : t.attachments) {
                QString text = a.name;
                QString tip = a.detail;
                switch (a.kind) {
                    case Attachment::Kind::File:
                    case Attachment::Kind::Image:
                        if (a.bytes >= 0)
                            text += QStringLiteral(" · ") + QLocale().formattedDataSize(a.bytes);
                        if (a.truncated)
                            text += QStringLiteral(" · ") + V::tr("truncated");
                        tip = a.truncated ? V::tr("Attached file (cut to the first part when sent)") : V::tr("Attached file");
                        break;
                    case Attachment::Kind::Symbol:
                        tip = a.linked ? V::tr("Linked symbol") : V::tr("Symbol");
                        break;
                    case Attachment::Kind::Portfolio:
                        text = V::tr("Portfolio · %1").arg(a.name);
                        break;
                    case Attachment::Kind::Watchlist:
                        text = V::tr("Watchlist · %1").arg(a.name);
                        break;
                }
                const QString sym = (a.ref.isEmpty() ? a.name : a.ref).trimmed();
                if (a.kind == Attachment::Kind::Symbol && !sym.isEmpty()) {
                    auto* b = tr_make_button(style::kChip, chips_);
                    tr_label_button(b, fincept::ui::literal_ampersands(text), V::tr("Research %1").arg(sym),
                                    V::tr("Open %1 in Equity Research").arg(sym));
                    QObject::connect(b, &QPushButton::clicked, b, [sym]() {
                        fincept::screens::equity_links::open_symbol_in(QStringLiteral("equity_research"), sym);
                    });
                    flow->addWidget(b);
                } else {
                    auto* l = tr_make_label(style::kChip, chips_);
                    l->setText(text);
                    l->setToolTip(tip);
                    l->setAccessibleName(text);
                    flow->addWidget(l);
                }
            }
            put(SlotChips, chips_);
            chips_->ensurePolished();
            chips_->show();
        }
    }
    if (!user_text_) {
        user_text_ = tr_make_label(style::kUserText, this, Qt::RichText);
        tr_make_wrapping(user_text_);
        user_text_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        user_text_->setFocusPolicy(Qt::ClickFocus);
        user_text_->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(user_text_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (view_ && user_text_)
                view_->show_text_menu(id_, user_text_->mapToGlobal(pos), QString(), user_text_->selectedText());
        });
        put(SlotUser, user_text_);
        user_text_->ensurePolished();
    }
    const QString shown = t.text.size() > tr_kUserShowChars ? t.text.left(tr_kUserShowChars) + QChar(0x2026) : t.text;
    if (shown != user_for_ || user_text_->text().isEmpty()) {
        user_for_ = shown;
        QString html = shown.toHtmlEscaped();
        html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
        user_text_->setText(html);
    }
    user_text_->setVisible(!shown.isEmpty());
    touched(user_text_);
}

void AicTurnWidget::sync_blocks(const ParsedDoc& doc, bool streaming) {
    const int n = int(doc.blocks.size());
    for (int i = 0; i < n; ++i) {
        const Block& b = doc.blocks.at(i);
        const bool caret = streaming && i == n - 1 && !b.closed;
        if (i < int(items_.size())) {
            BodyItem& it = items_[i];
            if (it.kind == b.kind && it.closed == b.closed && it.caret == caret && it.source == b.source)
                continue;
            if (it.kind == b.kind && AicTranscriptView::update_block_widget(it.w, b, caret)) {
                it.source = b.source;
                it.closed = b.closed;
                it.caret = caret;
                tr_invalidate_up(it.w, this);
                continue;
            }
            QWidget* nw = AicTranscriptView::create_block_widget(view_, id_, b, caret, body_);
            body_l_->insertWidget(i, nw);
            nw->show();
            body_l_->removeWidget(it.w);
            it.w->hide();
            it.w->deleteLater();
            it = BodyItem{nw, b.kind, b.source, b.closed, caret};
            continue;
        }
        QWidget* nw = AicTranscriptView::create_block_widget(view_, id_, b, caret, body_);
        body_l_->addWidget(nw);
        nw->show(); // a child added to a visible parent stays hidden (and unmeasured) until shown
        items_.append(BodyItem{nw, b.kind, b.source, b.closed, caret});
    }
    while (int(items_.size()) > n) {
        QWidget* w = items_.takeLast().w;
        body_l_->removeWidget(w);
        w->hide();
        w->deleteLater();
    }
    body_->setVisible(n > 0 || role_kind_ == TurnRole::Notice);
}

void AicTurnWidget::sync_placeholder() {
    const AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    using V = AicTranscriptView;
    const Turn& t = e->turn;
    QString text;
    bool caret = false;
    if (t.role != TurnRole::User) {
        if (t.state == TurnState::Streaming && (!e->doc || e->doc->blocks.isEmpty()))
            caret = true;
        else if (!e->doc && !t.text.isEmpty())
            text = V::tr("rendering…");
        else if (t.role == TurnRole::Assistant && t.state == TurnState::Stopped && t.text.trimmed().isEmpty())
            text = V::tr("Stopped before any answer.");
    }
    if (!caret && text.isEmpty()) {
        if (placeholder_ && !placeholder_->isHidden()) {
            placeholder_->hide();
            touched(placeholder_);
        }
        return;
    }
    if (!placeholder_) {
        placeholder_ = tr_make_label(style::kDim, this);
        put(SlotPlaceholder, placeholder_);
        placeholder_->ensurePolished();
    }
    if (caret) {
        placeholder_->setTextFormat(Qt::RichText);
        placeholder_->setText(tr_caret_html());
    } else {
        placeholder_->setTextFormat(Qt::PlainText);
        placeholder_->setText(text);
    }
    placeholder_->show();
    touched(placeholder_);
}

void AicTurnWidget::sync_error() {
    const AicTranscriptView::Entry* e = entry();
    const AicTranscriptView::Private* d = priv();
    if (!e || !d)
        return;
    using V = AicTranscriptView;
    const Turn& t = e->turn;
    const bool show = t.role == TurnRole::Assistant && (t.state == TurnState::Failed || t.state == TurnState::Empty);
    if (!show) {
        if (error_ && !error_->isHidden()) {
            error_->hide();
            touched(error_);
        }
        return;
    }
    if (!error_) {
        error_ = new QFrame(this);
        error_->setObjectName(QLatin1String(style::kErrorBlock));
        auto* vl = new QVBoxLayout(error_);
        vl->setContentsMargins(style::u(3), style::u(2), style::u(3), style::u(2));
        vl->setSpacing(style::u(2));
        error_text_ = tr_make_label(style::kBody, error_);
        tr_make_wrapping(error_text_);
        error_text_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* row = new QWidget(error_);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        error_retry_ = tr_make_button(style::kBtn, row);
        error_retry_->setFocusPolicy(Qt::TabFocus);
        error_retry_->setProperty(fincept::core::agent::kAgentRiskProperty, tr_spend());
        QObject::connect(error_retry_, &QPushButton::clicked, this, [this]() {
            if (auto* dd = priv())
                dd->request(id_, TurnAction::Retry);
        });
        rl->addWidget(error_retry_);
        rl->addStretch(1);
        vl->addWidget(error_text_);
        vl->addWidget(row);
        put(SlotError, error_);
    }
    const bool failed = t.state == TurnState::Failed;
    tr_set_prop(error_, "tone", failed ? QStringLiteral("danger") : QStringLiteral("warn"));
    error_text_->setText(failed ? V::tr("FAILED") + QStringLiteral(" · ") +
                                      (t.error.trimmed().isEmpty() ? V::tr("no reason given") : t.error.trimmed())
                                : V::tr("Empty response — the model returned no content."));
    tr_label_button(error_retry_, V::tr("RETRY"), V::tr("Retry the request"),
                    V::tr("Send the request again. Uses the model again."));
    const bool can = view_->live_turn_ == id_ && d->actions_enabled;
    error_retry_->parentWidget()->setVisible(can);
    error_->show();
    error_->ensurePolished();
    touched(error_text_);
}

void AicTurnWidget::sync_notice_buttons() {
    const AicTranscriptView::Entry* e = entry();
    const AicTranscriptView::Private* d = priv();
    if (!e || !d || !notice_btns_)
        return;
    const Turn& t = e->turn;
    const QVector<TrNoticeAction> acts = tr_notice_actions(t);
    QString key = QString::number(int(t.notice));
    for (const TrNoticeAction& a : acts)
        key += QLatin1Char('|') + a.text;
    if (key != notice_built_for_) {
        notice_built_for_ = key;
        for (const auto& pr : std::as_const(notice_buttons_)) {
            notice_btns_->layout()->removeWidget(pr.second);
            pr.second->hide();
            pr.second->deleteLater();
        }
        notice_buttons_.clear();
        auto* bl = static_cast<QHBoxLayout*>(notice_btns_->layout());
        int at = 0;
        for (const TrNoticeAction& a : acts) {
            QPushButton* b = tr_make_button(style::kBtn, notice_btns_);
            b->setFocusPolicy(Qt::TabFocus);
            b->setText(a.text);
            b->setAccessibleName(a.text);
            if (a.spend) {
                b->setProperty(fincept::core::agent::kAgentRiskProperty, tr_spend());
                b->setToolTip(AicTranscriptView::tr("Uses the model again"));
            }
            const TurnAction act = a.action;
            QObject::connect(b, &QPushButton::clicked, this, [this, act]() {
                if (auto* dd = priv())
                    dd->request(id_, act);
            });
            bl->insertWidget(at++, b);
            notice_buttons_.append({act, b});
        }
    }
    const bool can = view_->live_turn_ == id_ && d->actions_enabled;
    int shown = 0;
    for (int i = 0; i < int(acts.size()) && i < int(notice_buttons_.size()); ++i) {
        const bool vis = !acts.at(i).live_only || can;
        notice_buttons_.at(i).second->setVisible(vis);
        shown += vis ? 1 : 0;
    }
    if (notice_btns_->isHidden() == (shown > 0)) {
        notice_btns_->setVisible(shown > 0);
        touched(notice_btns_);
    }
}

void AicTurnWidget::sync_sources() {
    const AicTranscriptView::Entry* e = entry();
    if (!e)
        return;
    const Turn& t = e->turn;
    const bool show = t.role == TurnRole::Assistant && t.state != TurnState::Streaming && e->doc &&
                      !e->doc->sources.isEmpty();
    if (!show) {
        if (sources_ && !sources_->isHidden()) {
            sources_->hide();
            touched(sources_);
        }
        return;
    }
    QStringList urls;
    for (const SourceLink& s : e->doc->sources)
        urls << s.url;
    if (urls != sources_for_ || !sources_) {
        sources_for_ = urls;
        if (sources_) {
            slots_[SlotSources] = nullptr;
            root_->removeWidget(sources_);
            sources_->hide();
            sources_->deleteLater();
        }
        using V = AicTranscriptView;
        sources_ = new QWidget(this);
        auto* flow = new fincept::ui::FlowLayout(sources_, style::u(2), style::u(1));
        flow->setContentsMargins(0, 0, 0, 0);
        auto* title = tr_make_label(style::kSectionTitle, sources_);
        title->setText(V::tr("SOURCES"));
        flow->addWidget(title);
        int k = 0;
        for (const SourceLink& s : e->doc->sources) {
            if (++k > tr_kMaxSourceButtons)
                break;
            auto* b = tr_make_button(style::kLinkBtn, sources_);
            const QString host = s.host.isEmpty() ? s.url : s.host;
            b->setText(QStringLiteral("[%1] %2").arg(QString::number(s.number), fincept::ui::literal_ampersands(host)));
            b->setToolTip(s.title.trimmed().isEmpty() ? s.url : s.title.trimmed() + QStringLiteral("\n") + s.url);
            b->setAccessibleName(V::tr("Open source %1: %2").arg(QString::number(s.number), host));
            const QString url = s.url;
            QObject::connect(b, &QPushButton::clicked, this, [this, url]() {
                if (view_)
                    view_->open_link(id_, url);
            });
            flow->addWidget(b);
        }
        put(SlotSources, sources_);
        sources_->ensurePolished();
    }
    sources_->show();
    touched(sources_);
}

void AicTurnWidget::set_dimmed(bool on) {
    if (on == dimmed_)
        return;
    dimmed_ = on;
    if (fading_)
        return; // the fade ends at the right opacity
    if (on) {
        auto* fx = new QGraphicsOpacityEffect(this);
        fx->setOpacity(0.5);
        setGraphicsEffect(fx);
    } else {
        setGraphicsEffect(nullptr);
    }
}

void AicTurnWidget::fade_in() {
    fading_ = true;
    auto* fx = new QGraphicsOpacityEffect(this);
    fx->setOpacity(0.0);
    setGraphicsEffect(fx);
    auto* anim = new QPropertyAnimation(fx, "opacity", this);
    anim->setDuration(style::kFadeMs);
    anim->setStartValue(0.0);
    anim->setEndValue(dimmed_ ? 0.5 : 1.0);
    QObject::connect(anim, &QAbstractAnimation::finished, this, [this]() {
        // Deferred: the effect owns nothing of the animation, but it is its target.
        QTimer::singleShot(0, this, [this]() {
            fading_ = false;
            if (!dimmed_) {
                setGraphicsEffect(nullptr);
            } else if (auto* g = qobject_cast<QGraphicsOpacityEffect*>(graphicsEffect())) {
                g->setOpacity(0.5);
            }
        });
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AicTurnWidget::popup_report_menu() {
    AicTranscriptView::Private* d = priv();
    if (!d || !b_report_)
        return;
    auto* m = new QMenu(view_);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->setStyleSheet(style::menu_sheet());
    d->add_report_entries(m, id_);
    m->popup(b_report_->mapToGlobal(QPoint(0, b_report_->height())));
}

void AicTurnWidget::restyle(bool rerender_html) {
    header_->setFixedHeight(tr_header_h());
    for (QPushButton* b : {b_copy_, b_regen_, b_retry_, b_edit_, b_report_, b_quote_})
        if (b)
            b->setFixedHeight(style::button_h());
    if (think_head_)
        think_head_->setFixedHeight(style::button_h());
    if (tools_head_)
        tools_head_->setFixedHeight(style::button_h());
    if (error_retry_)
        error_retry_->setFixedHeight(style::button_h());
    for (const auto& pr : std::as_const(notice_buttons_))
        pr.second->setFixedHeight(style::button_h());
    static_cast<AicTrPulse*>(pulse_)->refit();
    if (tool_rows_)
        static_cast<AicTrToolRows*>(tool_rows_)->refit();
    for (const BodyItem& it : std::as_const(items_))
        AicTranscriptView::restyle_block_widget(it.w, rerender_html);
    sync_placeholder(); // the caret carries the accent colour
    // Fonts may have changed: every cached size hint in the tree is stale.
    const QList<QWidget*> all = findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (QLayout* l = w->layout())
            l->invalidate();
        w->updateGeometry();
    }
    root_->invalidate();
    update();
}

void AicTurnWidget::set_current(bool on) {
    if (current_ == on)
        return;
    current_ = on;
    setProperty("current", on);
    tr_repolish(this);
    refresh_actions();
}

int AicTurnWidget::measure(int width) {
    ensurePolished();
    QLayout* l = layout();
    if (!l)
        return sizeHint().height();
    return l->hasHeightForWidth() ? l->totalHeightForWidth(width) : l->totalSizeHint().height();
}

QPoint AicTurnWidget::menu_anchor() const {
    if (actions_ && actions_->isVisible())
        return actions_->mapToGlobal(QPoint(0, actions_->height()));
    return mapToGlobal(QPoint(root_->contentsMargins().left(), header_ ? header_->geometry().bottom() : 0));
}

bool AicTurnWidget::event(QEvent* e) {
    if (e->type() == QEvent::LayoutRequest) {
        if (AicTranscriptView::Private* d = priv())
            d->notify_dirty(id_);
    }
    return QWidget::event(e);
}

void AicTurnWidget::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    hovered_ = true;
    actions_->setVisible(true);
}

void AicTurnWidget::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    hovered_ = false;
    actions_->setVisible(current_);
}

void AicTurnWidget::paintEvent(QPaintEvent*) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    // The hairline that separates turns (BORDER_DIM, like style::kHairline).
    const int left = root_->contentsMargins().left();
    p.fillRect(QRect(left, height() - 1, std::max(0, width() - left), 1), style::border_dim());
}

void AicTurnWidget::contextMenuEvent(QContextMenuEvent* e) {
    if (AicTranscriptView::Private* d = priv())
        d->popup_turn_menu(id_, e->globalPos());
    e->accept();
}

// ── AicTranscriptView ────────────────────────────────────────────────────────

AicTranscriptView::AicTranscriptView(Density density, QWidget* parent)
    : QAbstractScrollArea(parent), d_(std::make_unique<Private>(this)), density_(density) {
    setObjectName(QLatin1String(style::kTranscript));
    viewport()->setObjectName(QLatin1String(style::kTranscript));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Conversation transcript"));
    setProperty(fincept::core::agent::kAgentRiskKeysProperty, QStringLiteral("Ctrl+R=spend"));

    d_->flush_timer = new QTimer(this);
    d_->flush_timer->setSingleShot(true);
    d_->flush_timer->setInterval(style::kStreamFlushMs);
    connect(d_->flush_timer, &QTimer::timeout, this, [this]() { d_->flush_streams(); });

    d_->layout_timer = new QTimer(this);
    d_->layout_timer->setSingleShot(true);
    d_->layout_timer->setInterval(0);
    connect(d_->layout_timer, &QTimer::timeout, this, [this]() { d_->layout_now(); });

    d_->pill = new QPushButton(viewport());
    d_->pill->setObjectName(QLatin1String(style::kPill));
    d_->pill->setCursor(Qt::PointingHandCursor);
    d_->pill->setFocusPolicy(Qt::NoFocus);
    d_->pill->hide();
    connect(d_->pill, &QPushButton::clicked, this, [this]() { scroll_to_bottom(); });
    d_->pill->setText(QString(QChar(0x2193)) + QStringLiteral(" ") + tr("NEW"));
    d_->pill->setAccessibleName(tr("Jump to the newest content"));
    d_->pill->setToolTip(tr("New content below. Click or press End to jump to it."));

    d_->inline_gen = style::inline_generation();
    d_->update_metrics();
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this,
            [this]() { d_->on_appearance_settled(); });
}

AicTranscriptView::~AicTranscriptView() {
    // The turn widgets talk to d_: they go first.
    d_->parse_gen = 0;
    d_->flush_timer->stop();
    d_->layout_timer->stop();
    for (auto& up : d_->entries)
        delete up->widget.data();
    d_->entries.clear();
}

void AicTranscriptView::clear() {
    ++d_->parse_gen; // pending parses are dropped
    d_->flush_timer->stop();
    d_->pending_ids.clear();
    for (auto& up : d_->entries)
        d_->evict(*up);
    d_->entries.clear();
    d_->index.clear();
    d_->offsets_dirty = true;
    current_ = 0;
    live_turn_ = 0;
    d_->fade_id = 0;
    d_->pill->hide();
    d_->set_follow(true);
    d_->layout_now();
}

QVector<quint64> AicTranscriptView::set_turns(const QVector<Turn>& turns) {
    clear();
    QVector<quint64> ids;
    ids.reserve(turns.size());
    d_->entries.reserve(size_t(turns.size()));
    d_->update_column();
    for (const Turn& t : turns) {
        auto e = std::make_unique<Entry>();
        e->turn = t;
        e->turn.id = d_->next_id++;
        d_->count_text(*e);
        e->height = d_->estimate(*e);
        ids.append(e->turn.id);
        d_->entries.push_back(std::move(e));
    }
    d_->reindex();
    d_->set_follow(true);
    d_->layout_now();
    return ids;
}

quint64 AicTranscriptView::append_turn(const Turn& turn) {
    auto e = std::make_unique<Entry>();
    e->turn = turn;
    const quint64 id = d_->next_id++;
    e->turn.id = id;
    d_->count_text(*e);
    e->height = d_->estimate(*e);
    d_->index.insert(id, int(d_->entries.size()));
    d_->entries.push_back(std::move(e));
    d_->offsets_dirty = true;
    if (style::motion())
        d_->fade_id = id;
    d_->show_pill();
    d_->layout_now();
    return id;
}

void AicTranscriptView::update_turn(quint64 id, const Turn& turn) {
    Entry* e = d_->find(id);
    if (!e)
        return;
    d_->merge_pending(*e);
    d_->pending_ids.remove(id);
    e->body_dirty = false;
    e->think_dirty = false;
    e->turn = turn;
    e->turn.id = id;
    d_->count_text(*e);
    if (e->widget) {
        e->widget->sync_all();
        e->measured_w = -1;
    } else {
        e->height = d_->estimate(*e);
        d_->offsets_dirty = true;
    }
    if (d_->index_of(id) == int(d_->entries.size()) - 1)
        d_->show_pill();
    d_->layout_now();
}

void AicTranscriptView::remove_turn(quint64 id) {
    const int i = d_->index_of(id);
    if (i < 0)
        return;
    d_->evict(*d_->entries[size_t(i)]);
    d_->entries.erase(d_->entries.begin() + i);
    d_->reindex();
    d_->pending_ids.remove(id);
    if (current_ == id)
        current_ = 0;
    if (live_turn_ == id)
        live_turn_ = 0;
    d_->layout_now();
}

const Turn* AicTranscriptView::turn(quint64 id) const {
    Entry* e = d_->find(id);
    if (!e)
        return nullptr;
    // The caller sees everything streamed so far; the widget catches up on the next flush.
    d_->merge_pending(*e);
    d_->count_text(*e);
    return &e->turn;
}

QVector<quint64> AicTranscriptView::turn_ids() const {
    QVector<quint64> ids;
    ids.reserve(int(d_->entries.size()));
    for (const auto& up : d_->entries)
        ids.append(up->turn.id);
    return ids;
}

int AicTranscriptView::turn_count() const {
    return int(d_->entries.size());
}

quint64 AicTranscriptView::last_turn_id(TurnRole role) const {
    for (auto it = d_->entries.rbegin(); it != d_->entries.rend(); ++it)
        if ((*it)->turn.role == role)
            return (*it)->turn.id;
    return 0;
}

void AicTranscriptView::set_known_symbols(const QSet<QString>& symbols) {
    d_->known_symbols = symbols;
}

void AicTranscriptView::set_live_turn(quint64 id) {
    const quint64 old = live_turn_;
    live_turn_ = id;
    for (const quint64 x : {old, id}) {
        Entry* e = d_->find(x);
        if (e && e->widget) {
            e->widget->refresh_actions();
            e->measured_w = -1;
        }
    }
    d_->schedule_layout();
}

void AicTranscriptView::set_actions_enabled(bool enabled) {
    if (d_->actions_enabled == enabled)
        return;
    d_->actions_enabled = enabled;
    for (auto& up : d_->entries) {
        if (up->widget) {
            up->widget->refresh_actions();
            up->measured_w = -1;
        }
    }
    d_->schedule_layout();
}

void AicTranscriptView::set_report_source(const QString& label) {
    d_->report_source = label;
}

void AicTranscriptView::stream_append(quint64 id, const QString& chunk) {
    Entry* e = d_->find(id);
    if (!e || chunk.isEmpty())
        return;
    e->pending += chunk;
    d_->pending_ids.insert(id);
    if (!d_->flush_timer->isActive())
        d_->flush_timer->start();
}

QString AicTranscriptView::stream_text(quint64 id) const {
    const Entry* e = d_->find(id);
    return e ? e->turn.text + e->pending : QString();
}

void AicTranscriptView::stream_drop_text(quint64 id) {
    Entry* e = d_->find(id);
    if (!e)
        return;
    e->pending.clear();
    e->turn.text.clear();
    e->doc.reset();
    e->doc_text.clear();
    e->doc_streaming = false;
    e->tail_begin = 0;
    e->tail_index = 0;
    e->body_dirty = false;
    d_->count_text(*e);
    if (e->widget) {
        e->widget->sync_body();
        e->widget->refresh_actions();
        e->measured_w = -1;
    } else {
        e->height = d_->estimate(*e);
        d_->offsets_dirty = true;
    }
    d_->layout_now();
}

void AicTranscriptView::stream_append_thinking(quint64 id, const QString& chunk) {
    Entry* e = d_->find(id);
    if (!e || chunk.isEmpty())
        return;
    e->thinking_pending += chunk;
    d_->pending_ids.insert(id);
    if (!d_->flush_timer->isActive())
        d_->flush_timer->start();
}

void AicTranscriptView::set_tool_steps(quint64 id, const QVector<ToolStep>& steps) {
    Entry* e = d_->find(id);
    if (!e)
        return;
    e->turn.tools = steps;
    if (e->widget) {
        e->widget->sync_tools();
        e->measured_w = -1;
    } else {
        e->height = d_->estimate(*e);
        d_->offsets_dirty = true;
    }
    if (d_->index_of(id) == int(d_->entries.size()) - 1)
        d_->show_pill();
    d_->schedule_layout();
}

void AicTranscriptView::set_job_status(const QString& job_id, const QString& text, int tone) {
    if (job_id.isEmpty())
        return;
    d_->jobs.insert(job_id, {text, tone});
    for (auto& up : d_->entries) {
        if (!up->widget)
            continue;
        bool mine = false;
        for (const ToolStep& s : std::as_const(up->turn.tools))
            mine = mine || s.detail == job_id || s.label.contains(job_id);
        if (mine) {
            up->widget->sync_tools();
            up->measured_w = -1;
        }
    }
    d_->schedule_layout();
}

void AicTranscriptView::scroll_to_bottom() {
    d_->set_follow(true);
    d_->pill->hide();
    d_->layout_now();
}

int AicTranscriptView::scroll_position() const {
    return verticalScrollBar()->value();
}

void AicTranscriptView::set_scroll_position(int y) {
    d_->scroll_to(y);
}

QSize AicTranscriptView::sizeHint() const {
    return {600, 400};
}

QSize AicTranscriptView::minimumSizeHint() const {
    return {200, 120};
}

void AicTranscriptView::open_link(quint64 turn_id, const QString& link) {
    Q_UNUSED(turn_id)
    const QString l = link.trimmed();
    if (l.startsWith(QLatin1String("fsym:"), Qt::CaseInsensitive)) {
        const QString sym = l.mid(5).trimmed();
        if (!sym.isEmpty())
            fincept::screens::equity_links::open_symbol_in(QStringLiteral("equity_research"), sym);
        return;
    }
    if (is_safe_link(l)) {
        QDesktopServices::openUrl(QUrl(l));
        return;
    }
    emit status_message(tr("Link not opened: only web and mail links open from answers."));
}

void AicTranscriptView::show_text_menu(quint64 turn_id, const QPoint& global_pos, const QString& link,
                                       const QString& selection) {
    const Entry* e = d_->find(turn_id);
    if (!e)
        return;
    auto* m = new QMenu(this);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->setStyleSheet(style::menu_sheet());
    QPointer<AicTranscriptView> self(this);
    const QString l = link.trimmed();
    if (l.startsWith(QLatin1String("fsym:"), Qt::CaseInsensitive) && l.size() > 5) {
        const QString sym = l.mid(5).trimmed();
        QAction* research = m->addAction(tr("Research %1").arg(fincept::ui::literal_ampersands(sym)));
        connect(research, &QAction::triggered, this, [sym]() {
            fincept::screens::equity_links::open_symbol_in(QStringLiteral("equity_research"), sym);
        });
        fincept::screens::equity_links::add_open_in_actions(m, this, sym);
        QAction* copy_sym = m->addAction(tr("Copy symbol"));
        connect(copy_sym, &QAction::triggered, this, [sym]() { QApplication::clipboard()->setText(sym); });
        m->addSeparator();
    } else if (!l.isEmpty() && is_safe_link(l)) {
        QAction* open = m->addAction(tr("Open link"));
        open->setToolTip(l);
        connect(open, &QAction::triggered, this, [self, turn_id, l]() {
            if (self)
                self->open_link(turn_id, l);
        });
        QAction* copy_link = m->addAction(tr("Copy link address"));
        connect(copy_link, &QAction::triggered, this, [l]() { QApplication::clipboard()->setText(l); });
        m->addSeparator();
    }
    if (!selection.isEmpty()) {
        QAction* copy_sel = m->addAction(tr("Copy selection"));
        connect(copy_sel, &QAction::triggered, this, [self, selection]() {
            QApplication::clipboard()->setText(selection);
            if (self)
                emit self->status_message(
                    tr("Copied %1 characters").arg(QLocale().toString(qlonglong(selection.size()))));
        });
    }
    QAction* copy_turn = m->addAction(tr("Copy turn"));
    connect(copy_turn, &QAction::triggered, this, [self, turn_id]() {
        if (self)
            self->d_->copy_turn(turn_id);
    });
    if (e->turn.role != TurnRole::Notice && density_ == Density::Desk) {
        QAction* quote = m->addAction(tr("Quote"));
        connect(quote, &QAction::triggered, this, [self, turn_id]() {
            if (self)
                self->d_->request(turn_id, TurnAction::Quote);
        });
    }
    m->popup(global_pos);
}

// ── Events ───────────────────────────────────────────────────────────────────

void AicTranscriptView::resizeEvent(QResizeEvent* e) {
    QAbstractScrollArea::resizeEvent(e);
    d_->layout_now();
}

void AicTranscriptView::scrollContentsBy(int, int) {
    if (d_->programmatic || d_->in_layout)
        return;
    // The user moved the scroll bar (wheel, drag, keys).
    const int v = verticalScrollBar()->value();
    d_->set_follow(v >= d_->max_scroll() - tr_kFollowSlack);
    d_->layout_now();
}

bool AicTranscriptView::viewportEvent(QEvent* e) {
    if (e->type() == QEvent::MouseButtonPress && !hasFocus())
        setFocus(Qt::MouseFocusReason);
    return QAbstractScrollArea::viewportEvent(e);
}

void AicTranscriptView::keyPressEvent(QKeyEvent* e) {
    const bool ctrl = e->modifiers().testFlag(Qt::ControlModifier);
    QScrollBar* sb = verticalScrollBar();
    const int page = std::max(d_->line_h, viewport()->height() - 2 * d_->line_h);
    switch (e->key()) {
        case Qt::Key_Up:
            if (!ctrl) {
                d_->move_current(-1);
                e->accept();
                return;
            }
            break;
        case Qt::Key_Down:
            if (!ctrl) {
                d_->move_current(1);
                e->accept();
                return;
            }
            break;
        case Qt::Key_PageUp:
            d_->scroll_to(sb->value() - page);
            e->accept();
            return;
        case Qt::Key_PageDown:
            d_->scroll_to(sb->value() + page);
            e->accept();
            return;
        case Qt::Key_Home:
            d_->scroll_to(0);
            e->accept();
            return;
        case Qt::Key_End:
            scroll_to_bottom();
            e->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Menu: {
            quint64 id = current_;
            if (!id)
                id = live_turn_;
            if (!id && !d_->entries.empty())
                id = d_->entries.back()->turn.id;
            if (!id)
                break;
            const int idx = d_->index_of(id);
            d_->ensure_visible(idx);
            QPoint at = viewport()->mapToGlobal(QPoint(d_->col_x, style::u(4)));
            if (Entry* x = d_->find(id))
                if (x->widget)
                    at = x->widget->menu_anchor();
            d_->popup_turn_menu(id, at);
            e->accept();
            return;
        }
        case Qt::Key_C:
            if (ctrl && current_) {
                d_->copy_turn(current_);
                e->accept();
                return;
            }
            break;
        case Qt::Key_R:
            if (ctrl) {
                const Entry* x = d_->find(live_turn_);
                const bool can = x && d_->actions_enabled && density_ == Density::Desk;
                if (can && tr_can_regenerate(x->turn))
                    d_->request(live_turn_, TurnAction::Regenerate);
                else if (can && tr_can_retry(x->turn))
                    d_->request(live_turn_, TurnAction::Retry);
                else
                    emit status_message(tr("Nothing to regenerate right now."));
                e->accept();
                return;
            }
            break;
        case Qt::Key_Escape:
            if (current_) {
                d_->set_current(0);
                e->accept();
                return;
            }
            break;
        default:
            break;
    }
    QAbstractScrollArea::keyPressEvent(e);
}

void AicTranscriptView::focusInEvent(QFocusEvent* e) {
    QAbstractScrollArea::focusInEvent(e);
    viewport()->update();
}

void AicTranscriptView::showEvent(QShowEvent* e) {
    QAbstractScrollArea::showEvent(e);
    d_->layout_now();
}

void AicTranscriptView::changeEvent(QEvent* e) {
    QAbstractScrollArea::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        d_->retranslate();
}

} // namespace fincept::screens::aichat
