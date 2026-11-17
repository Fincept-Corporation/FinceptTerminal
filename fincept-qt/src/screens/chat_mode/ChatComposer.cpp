// ChatComposer.cpp — Chat Mode's composer (2026-11-17 revamp). See ChatComposer.h.
//
// One selector stylesheet on the root (object-name selectors only, built from the
// theme tokens before any child exists; the central re-style pass keeps it in
// step). The meter and the permissions chip are painted from the live tokens.
// The input grows by font metrics (1 to kCcmpMaxLines lines), re-fitted when its
// width or the text changes and after ThemeManager::appearance_settled. Nothing
// here sends: submit_requested() hands the text to ChatMessagePanel.

#include "screens/chat_mode/ChatComposer.h"

#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ComposerContextBar.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/RunTemplateLibrary.h"
#include "services/agent_tasks/RunTemplates.h"
#include "services/agent_tasks/TaskRunTypes.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ScreenCommandBar.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace chat_composer_detail {

constexpr int kCcmpMaxLines = 8;
constexpr int kCcmpNoteMs = 5000;
constexpr int kCcmpTickMs = 1000;
constexpr int kCcmpQuestionChars = 600;

/// "1:05" for 65 s, "1:02:05" past an hour.
QString ccmp_elapsed(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms) / 1000;
    if (s >= 3600)
        return QStringLiteral("%1:%2:%3")
            .arg(s / 3600)
            .arg((s % 3600) / 60, 2, 10, QLatin1Char('0'))
            .arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

/// '&' shown as '&' on a button (no mnemonics).
QString ccmp_amp(QString text) {
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

/// Local files carried by a drop / paste (URLs that are not local are ignored).
QStringList ccmp_local_files(const QMimeData* mime) {
    QStringList out;
    if (!mime || !mime->hasUrls())
        return out;
    for (const QUrl& u : mime->urls())
        if (u.isLocalFile() && !u.toLocalFile().isEmpty())
            out << u.toLocalFile();
    return out;
}

QString ccmp_style() {
    const QString base = ui::colors::BG_BASE;
    const QString surface = ui::colors::BG_SURFACE;
    const QString raised = ui::colors::BG_RAISED;
    const QString hover = ui::colors::BG_HOVER;
    const QString dim_border = ui::colors::BORDER_DIM;
    const QString med_border = ui::colors::BORDER_MED;
    const QString bright_border = ui::colors::BORDER_BRIGHT;
    const QString text = ui::colors::TEXT_PRIMARY;
    const QString text2 = ui::colors::TEXT_SECONDARY;
    const QString text3 = ui::colors::TEXT_TERTIARY;
    const QString text_dim = ui::colors::TEXT_DIM;
    const QString accent = ui::colors::AMBER;
    const QString accent_dim = ui::colors::AMBER_DIM;
    const QString accent_bg = ui::colors::ACCENT_BG;
    const QString on_accent = ui::colors::TEXT_ON_ACCENT;
    const QString danger = ui::colors::DANGER;
    const QString info = ui::colors::CYAN;
    const QString danger_bg = ui::colors::tint(QColor(ui::colors::DANGER()), 0.14);
    const QString danger_bg_hover = ui::colors::tint(QColor(ui::colors::DANGER()), 0.24);
    // Font sizes through ui::fonts::px() (concatenation: the number carries its offset).
    const auto px = [](int offset) { return ui::fonts::px(offset); };

    QString s;
    s.reserve(6000);
    s += "#chatComposer{background:" + surface + ";border:none;border-top:1px solid " + dim_border + ";}";
    // Answer banner (the old composer's, same object names).
    s += "#chatComposer QFrame#chatAnswerBanner{background:" + base + ";border:1px solid " + dim_border +
         ";border-left:2px solid " + info + ";}";
    s += "#chatComposer QLabel#chatAnswerBannerTitle{background:transparent;border:none;color:" + info +
         ";font-size:" + px(-3) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatComposer QLabel#chatAnswerBannerQuestion{background:transparent;border:none;color:" + text +
         ";font-size:" + px(-2) + "px;}";
    s += "#chatComposer QLabel#chatAnswerBannerHint{background:transparent;border:none;color:" + text_dim +
         ";font-size:" + px(-3) + "px;}";
    // Input.
    s += "#chatComposer QPlainTextEdit#ccmpInput{background:" + base + ";color:" + text + ";border:1px solid " +
         med_border + ";padding:4px 8px;font-size:" + px(0) + "px;selection-background-color:" + accent_dim +
         ";selection-color:" + on_accent + ";}";
    s += "#chatComposer QPlainTextEdit#ccmpInput:focus{border:1px solid " + accent + ";}";
    s += "#chatComposer QPlainTextEdit#ccmpInput QScrollBar:vertical{background:transparent;width:6px;margin:2px;}";
    s += "#chatComposer QPlainTextEdit#ccmpInput QScrollBar::handle:vertical{background:" + med_border +
         ";min-height:16px;}";
    s += "#chatComposer QPlainTextEdit#ccmpInput QScrollBar::add-line:vertical,"
         "#chatComposer QPlainTextEdit#ccmpInput QScrollBar::sub-line:vertical{height:0;}";
    // Lines under the input.
    s += "#chatComposer QLabel#ccmpActivity{background:transparent;border:none;color:" + text3 + ";font-size:" +
         px(-3) + "px;}";
    s += "#chatComposer QLabel#ccmpNote{background:transparent;border:none;color:" + text2 + ";font-size:" +
         px(-3) + "px;}";
    s += "#chatComposer QLabel#ccmpNote[error=\"true\"]{color:" + danger + ";}";
    // LITE | DEEP.
    s += "#chatComposer QPushButton#ccmpSeg{background:" + base + ";color:" + text3 + ";border:1px solid " +
         med_border + ";padding:2px 8px;font-size:" + px(-4) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatComposer QPushButton#ccmpSeg:hover{color:" + text + ";}";
    s += "#chatComposer QPushButton#ccmpSeg:checked{background:" + accent_bg + ";color:" + accent +
         ";border-color:" + accent + ";}";
    s += "#chatComposer QPushButton#ccmpSeg:focus{border-color:" + bright_border + ";}";
    // Investment profile.
    s += "#chatComposer QComboBox#ccmpProfile{background:" + base + ";color:" + text2 + ";border:1px solid " +
         med_border + ";padding:1px 6px;font-size:" + px(-3) + "px;}";
    s += "#chatComposer QComboBox#ccmpProfile:hover,#chatComposer QComboBox#ccmpProfile:focus{border-color:" +
         accent + ";}";
    s += "#chatComposer QComboBox#ccmpProfile QAbstractItemView{background:" + surface + ";color:" + text +
         ";border:1px solid " + med_border + ";selection-background-color:" + hover + ";}";
    // Quiet buttons (OPTIMIZE, mention actions).
    s += "#chatComposer QPushButton#ccmpGhost{background:transparent;color:" + text2 + ";border:1px solid " +
         med_border + ";padding:3px 10px;font-size:" + px(-4) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatComposer QPushButton#ccmpGhost:hover,#chatComposer QPushButton#ccmpGhost:focus{color:" + text +
         ";border-color:" + accent + ";}";
    s += "#chatComposer QPushButton#ccmpGhost:disabled{color:" + text_dim + ";border-color:" + dim_border + ";}";
    // SEND <-> STOP.
    s += "#chatComposer QPushButton#ccmpSend{background:" + accent + ";color:" + on_accent + ";border:1px solid " +
         accent + ";padding:4px 16px;font-size:" + px(-3) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatComposer QPushButton#ccmpSend:hover{background:" + accent_dim + ";border-color:" + accent_dim + ";}";
    s += "#chatComposer QPushButton#ccmpSend:focus{border:1px solid " + text + ";}";
    s += "#chatComposer QPushButton#ccmpSend:disabled{background:" + raised + ";color:" + text_dim +
         ";border-color:" + dim_border + ";}";
    s += "#chatComposer QPushButton#ccmpSend[ccmpMode=\"stop\"]{background:" + danger_bg + ";color:" + danger +
         ";border-color:" + danger + ";}";
    s += "#chatComposer QPushButton#ccmpSend[ccmpMode=\"stop\"]:hover{background:" + danger_bg_hover + ";}";
    s += "#chatComposer QPushButton#ccmpSend[ccmpMode=\"stop\"]:disabled{background:" + raised + ";color:" +
         text_dim + ";border-color:" + dim_border + ";}";
    // "@" mention strip.
    s += "#chatComposer QFrame#ccmpMention{background:" + base + ";border:1px solid " + dim_border +
         ";border-left:2px solid " + accent + ";}";
    s += "#chatComposer QLabel#ccmpMentionLabel{background:transparent;border:none;color:" + accent +
         ";font-size:" + px(-4) + "px;font-weight:700;letter-spacing:1px;}";
    s += "#chatComposer QPushButton#ccmpClose{background:transparent;border:none;color:" + text3 + ";font-size:" +
         px(-1) + "px;padding:0 4px;}";
    s += "#chatComposer QPushButton#ccmpClose:hover,#chatComposer QPushButton#ccmpClose:focus{color:" + text + ";}";
    return s;
}

// ── Input ────────────────────────────────────────────────────────────────────

/// The message box: local files pasted or dropped onto it become the attachment
/// (the callback says whether it took them); everything else is ordinary text.
class CcmpInput : public QPlainTextEdit {
  public:
    explicit CcmpInput(QWidget* parent) : QPlainTextEdit(parent) {}
    std::function<bool(const QMimeData*)> on_files;

  protected:
    bool canInsertFromMimeData(const QMimeData* source) const override {
        if (!ccmp_local_files(source).isEmpty())
            return true;
        return QPlainTextEdit::canInsertFromMimeData(source);
    }
    void insertFromMimeData(const QMimeData* source) override {
        if (on_files && !ccmp_local_files(source).isEmpty() && on_files(source))
            return;
        QPlainTextEdit::insertFromMimeData(source);
    }
};

/// Drops anywhere else on the composer (the chips row, the controls).
class CcmpDropFilter : public QObject {
  public:
    CcmpDropFilter(QWidget* target, std::function<bool(const QMimeData*)> fn) : QObject(target), fn_(std::move(fn)) {
        target->setAcceptDrops(true);
        target->installEventFilter(this);
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* e) override {
        if (watched == parent()) {
            if (e->type() == QEvent::DragEnter || e->type() == QEvent::DragMove) {
                auto* de = static_cast<QDropEvent*>(e);
                if (!ccmp_local_files(de->mimeData()).isEmpty()) {
                    de->acceptProposedAction();
                    return true;
                }
            } else if (e->type() == QEvent::Drop) {
                auto* de = static_cast<QDropEvent*>(e);
                if (fn_ && fn_(de->mimeData())) {
                    de->acceptProposedAction();
                    return true;
                }
            }
        }
        return QObject::eventFilter(watched, e);
    }

  private:
    std::function<bool(const QMimeData*)> fn_;
};

// ── Painted pieces ───────────────────────────────────────────────────────────

/// "1,234 / 20,000 · one reply · about 5 credits" with a hairline fill bar under
/// the count: WARNING past 90 %, DANGER past the limit. Elided to its room.
class CcmpMeter : public QWidget {
  public:
    explicit CcmpMeter(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setFocusPolicy(Qt::NoFocus);
    }

    std::function<QString()> summary; ///< the budget of the next message

    void set_count(int n) {
        if (n == n_)
            return;
        n_ = n;
        updateGeometry();
        update();
    }
    int count() const { return n_; }

    QString count_text() const {
        return QStringLiteral("%1 / %2").arg(QLocale().toString(n_), QLocale().toString(agt::kChatMessageMaxChars));
    }
    QString full_text() const {
        const QString sum = summary ? summary() : QString();
        return sum.isEmpty() ? count_text() : count_text() + QStringLiteral("  ·  ") + sum;
    }

    QSize sizeHint() const override {
        const QFontMetrics fm(ui::fonts::font(-3));
        return {fm.horizontalAdvance(full_text()) + 8, fm.height() + 6};
    }
    QSize minimumSizeHint() const override {
        const QFontMetrics fm(ui::fonts::font(-3));
        return {fm.horizontalAdvance(count_text()) + 8, fm.height() + 6};
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QFont f = ui::fonts::font(-3);
        const QFontMetrics fm(f);
        p.setFont(f);
        const int max = agt::kChatMessageMaxChars;
        const QColor tone = n_ > max          ? QColor(ui::colors::DANGER())
                            : n_ > max * 9 / 10 ? QColor(ui::colors::WARNING())
                                                : QColor(ui::colors::TEXT_TERTIARY());
        const QString count = count_text();
        const QString sum = summary ? summary() : QString();
        const QString rest = sum.isEmpty() ? QString() : QStringLiteral("  ·  ") + sum;
        const int count_w = fm.horizontalAdvance(count);
        const int text_h = fm.height();
        const int top = std::max(0, (height() - text_h - 3) / 2);
        // Right-aligned: the count sits next to the buttons, the summary before it.
        const int avail = std::max(0, width() - 4);
        const QString rest_el = fm.elidedText(rest, Qt::ElideLeft, std::max(0, avail - count_w));
        const int rest_w = fm.horizontalAdvance(rest_el);
        int x = std::max(0, width() - 2 - rest_w - count_w);
        p.setPen(tone);
        p.drawText(QRect(x, top, count_w, text_h), Qt::AlignLeft | Qt::AlignVCenter, count);
        // The fill bar under the count (tabular figures: the count is the user's font).
        const int bar_y = top + text_h + 1;
        p.fillRect(QRect(x, bar_y, count_w, 2), QColor(ui::colors::BORDER_DIM()));
        const double frac = std::clamp(max > 0 ? double(n_) / max : 0.0, 0.0, 1.0);
        if (frac > 0)
            p.fillRect(QRect(x, bar_y, std::max(1, int(std::lround(count_w * frac))), 2),
                       n_ > max * 9 / 10 ? tone : QColor(ui::colors::AMBER_DIM()));
        if (!rest_el.isEmpty()) {
            p.setPen(QColor(ui::colors::TEXT_DIM()));
            p.drawText(QRect(x + count_w, top, rest_w, text_h), Qt::AlignLeft | Qt::AlignVCenter, rest_el);
        }
    }

  private:
    int n_ = 0;
};

/// The read-only "MAY: …" chip: what the next run may do. Elided to its room; the
/// tooltip has the whole explanation.
class CcmpChip : public QWidget {
  public:
    explicit CcmpChip(QWidget* parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setFocusPolicy(Qt::NoFocus);
    }
    void set_text(const QString& text) {
        if (text == text_)
            return;
        text_ = text;
        setAccessibleName(text);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override {
        const QFontMetrics fm(ui::fonts::font(-4));
        return {fm.horizontalAdvance(text_) + 2 * pad(fm) + 2, fm.height() + 6};
    }
    QSize minimumSizeHint() const override {
        const QFontMetrics fm(ui::fonts::font(-4));
        return {std::min(sizeHint().width(), fm.averageCharWidth() * 10 + 2 * pad(fm)), fm.height() + 6};
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QFont f = ui::fonts::font(-4);
        const QFontMetrics fm(f);
        const int h = fm.height() + 4;
        const QRect r(0, (height() - h) / 2, width() - 1, h);
        p.setPen(QColor(ui::colors::BORDER_MED()));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        p.setFont(f);
        p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
        const QRect text_rect = r.adjusted(pad(fm), 0, -pad(fm), 0);
        p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(text_, Qt::ElideRight, text_rect.width()));
    }

  private:
    static int pad(const QFontMetrics& fm) { return std::max(4, fm.averageCharWidth()); }
    QString text_;
};

} // namespace chat_composer_detail

using namespace chat_composer_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ChatComposer::Impl {
    ChatComposer* q = nullptr;

    // Answer banner
    QFrame* banner = nullptr;
    QLabel* banner_title = nullptr;
    QLabel* banner_question = nullptr;
    QLabel* banner_hint = nullptr;
    QPushButton* banner_cancel = nullptr;

    ComposerContextBar* bar = nullptr;

    // "@" mention strip
    QFrame* mention = nullptr;
    QLabel* mention_label = nullptr;
    ui::SymbolSearchBox* mention_box = nullptr;
    QPushButton* mention_file = nullptr;
    QPushButton* mention_close = nullptr;
    int mention_pos = -1;

    CcmpInput* input = nullptr;
    QLabel* activity = nullptr;
    QLabel* note = nullptr;
    QTimer* tick = nullptr;
    QTimer* note_timer = nullptr;

    // Controls row
    QPushButton* lite = nullptr;
    QPushButton* deep = nullptr;
    QButtonGroup* mode_group = nullptr;
    QComboBox* profile = nullptr;
    CcmpChip* perm = nullptr;
    CcmpMeter* meter = nullptr;
    QPushButton* optimize = nullptr;
    QPushButton* send = nullptr;

    // State
    bool busy = false;
    bool pending = false;
    bool can_stop = false;
    QString stop_text;
    QString stop_tip;
    bool awaiting = false;
    QString question;
    QString activity_text;
    qint64 activity_since = 0;
    StreamMode mode = StreamMode::Lite;
    QVector<AgentProfile> profiles;
    int preferred_profile = -1;
    double credits = -1;
    bool optimizing = false;
    int fitted_width = -1;
    int fitted_lines = -1;

    void build();
    void wire();
    void retranslate();
    void update_send();
    void update_perm();
    void update_meter();
    void fit_input();
    void refresh_activity();
    void rebuild_profiles();
    void set_mode(StreamMode m, bool user);
    void submit();
    void on_escape();
    void after_trigger(QChar ch);
    void open_template_menu(int slash_pos);
    void apply_template(const QString& id, int slash_pos);
    bool replace_token(int pos, QChar ch, const QString& with);
    void open_mention(int at_pos);
    void close_mention(bool focus_input);
    void pick_symbol(const core::instrument::Instrument& inst);
    bool attach_files(const QMimeData* mime);
    void optimize_now();
    void show_note(const QString& text, bool error);
    QString trimmed_text() const { return input->toPlainText().trimmed(); }
};

void ChatComposer::Impl::build() {
    // Stylesheet first, then the children under their final parent.
    q->setObjectName(QStringLiteral("chatComposer"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(ccmp_style());
    q->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // ── Answer banner (the agent paused on a question) ──
    banner = new QFrame(q);
    banner->setObjectName(QStringLiteral("chatAnswerBanner"));
    auto* bl = new QVBoxLayout(banner);
    bl->setContentsMargins(10, 6, 10, 6);
    bl->setSpacing(3);
    auto* banner_top = new QHBoxLayout;
    banner_top->setSpacing(6);
    banner_title = new QLabel(banner);
    banner_title->setObjectName(QStringLiteral("chatAnswerBannerTitle"));
    banner_top->addWidget(banner_title);
    banner_top->addStretch(1);
    banner_cancel = new QPushButton(banner);
    banner_cancel->setObjectName(QStringLiteral("ccmpGhost"));
    banner_cancel->setCursor(Qt::PointingHandCursor);
    banner_top->addWidget(banner_cancel);
    bl->addLayout(banner_top);
    banner_question = new QLabel(banner);
    banner_question->setObjectName(QStringLiteral("chatAnswerBannerQuestion"));
    banner_question->setTextFormat(Qt::PlainText); // server text
    banner_question->setWordWrap(true);
    banner_question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bl->addWidget(banner_question);
    banner_hint = new QLabel(banner);
    banner_hint->setObjectName(QStringLiteral("chatAnswerBannerHint"));
    banner_hint->setWordWrap(true);
    bl->addWidget(banner_hint);
    banner->setVisible(false);
    vl->addWidget(banner);

    // ── Context chips, engine, run as task, budget ──
    bar = new ComposerContextBar(q);
    vl->addWidget(bar);

    // ── "@" mention strip ──
    mention = new QFrame(q);
    mention->setObjectName(QStringLiteral("ccmpMention"));
    auto* ml = new QHBoxLayout(mention);
    ml->setContentsMargins(8, 4, 4, 4);
    ml->setSpacing(6);
    mention_label = new QLabel(mention);
    mention_label->setObjectName(QStringLiteral("ccmpMentionLabel"));
    ml->addWidget(mention_label);
    mention_box = new ui::SymbolSearchBox(mention);
    mention_box->set_compact(true);
    mention_box->set_clear_on_pick(true);
    mention_box->set_recent_key(QStringLiteral("chat_mode"));
    ml->addWidget(mention_box, 1);
    mention_file = new QPushButton(mention);
    mention_file->setObjectName(QStringLiteral("ccmpGhost"));
    mention_file->setCursor(Qt::PointingHandCursor);
    ml->addWidget(mention_file);
    mention_close = new QPushButton(QStringLiteral("×"), mention);
    mention_close->setObjectName(QStringLiteral("ccmpClose"));
    mention_close->setCursor(Qt::PointingHandCursor);
    ml->addWidget(mention_close);
    mention->setVisible(false);
    vl->addWidget(mention);

    // ── Input ──
    input = new CcmpInput(q);
    input->setObjectName(QStringLiteral("ccmpInput"));
    input->setProperty("fincept_agent_risk", "spend"); // agents: Enter sends a billed model request
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    input->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    input->setTabChangesFocus(true);
    vl->addWidget(input);

    // ── Activity / note lines ──
    activity = new QLabel(q);
    activity->setObjectName(QStringLiteral("ccmpActivity"));
    activity->setTextFormat(Qt::PlainText); // run activity is server / engine text
    activity->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    activity->setVisible(false);
    vl->addWidget(activity);
    note = new QLabel(q);
    note->setObjectName(QStringLiteral("ccmpNote"));
    note->setTextFormat(Qt::PlainText);
    note->setWordWrap(true);
    note->setVisible(false);
    vl->addWidget(note);

    // ── Controls row ──
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    lite = new QPushButton(QStringLiteral("LITE"), q); // fixed mode identifiers, not translated
    deep = new QPushButton(QStringLiteral("DEEP"), q);
    mode_group = new QButtonGroup(q);
    mode_group->setExclusive(true);
    for (QPushButton* b : {lite, deep}) {
        b->setObjectName(QStringLiteral("ccmpSeg"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        mode_group->addButton(b);
    }
    lite->setChecked(true);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(0);
    seg->addWidget(lite);
    seg->addWidget(deep);
    row->addLayout(seg);

    profile = new QComboBox(q);
    profile->setObjectName(QStringLiteral("ccmpProfile"));
    profile->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    profile->setVisible(false);
    row->addWidget(profile);

    perm = new CcmpChip(q);
    row->addWidget(perm);

    meter = new CcmpMeter(q);
    row->addWidget(meter, 1);

    optimize = new QPushButton(q);
    optimize->setObjectName(QStringLiteral("ccmpGhost"));
    optimize->setCursor(Qt::PointingHandCursor);
    row->addWidget(optimize);

    send = new QPushButton(q);
    send->setObjectName(QStringLiteral("ccmpSend"));
    send->setCursor(Qt::PointingHandCursor);
    send->setProperty("ccmpMode", QStringLiteral("send"));
    send->setProperty("fincept_agent_risk", "spend"); // agents: SEND sends a billed model request
    row->addWidget(send);
    vl->addLayout(row);

    tick = new QTimer(q);
    tick->setInterval(kCcmpTickMs);
    note_timer = new QTimer(q);
    note_timer->setSingleShot(true);
    note_timer->setInterval(kCcmpNoteMs);

    // Keyboard path: input -> mode -> profile -> optimize -> send.
    QWidget::setTabOrder(input, lite);
    QWidget::setTabOrder(lite, deep);
    QWidget::setTabOrder(deep, profile);
    QWidget::setTabOrder(profile, optimize);
    QWidget::setTabOrder(optimize, send);
}

void ChatComposer::Impl::wire() {
    QObject::connect(input, &QPlainTextEdit::textChanged, q, [this]() {
        fit_input();
        update_meter();
        update_send();
        emit q->draft_changed();
    });
    new rv::RvEventHook(input, [this](QEvent::Type t) {
        if (t == QEvent::Resize)
            fit_input();
    });

    // Enter / Ctrl+Enter send, Shift+Enter new line; Esc stops / goes back; "/" and
    // "@" open their menus once the character is in. The keys are taken at
    // ShortcutOverride, so the screen's and the shell's shortcuts never fire for them.
    new rv::RvKeyHook(input, [this](const QKeyEvent* ke, bool apply) {
        const int key = ke->key();
        const Qt::KeyboardModifiers mods = ke->modifiers();
        if (key == Qt::Key_Escape) {
            if (apply)
                on_escape();
            return true;
        }
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !(mods & Qt::ShiftModifier) &&
            !(mods & Qt::AltModifier)) {
            if (apply)
                submit();
            return true;
        }
        if (apply && (ke->text() == QLatin1String("/") || ke->text() == QLatin1String("@"))) {
            const QChar ch = ke->text().at(0);
            QTimer::singleShot(0, input, [this, ch]() { after_trigger(ch); });
        }
        return false;
    });

    input->on_files = [this](const QMimeData* mime) { return attach_files(mime); };
    new CcmpDropFilter(q, [this](const QMimeData* mime) { return attach_files(mime); });

    QObject::connect(send, &QPushButton::clicked, q, [this]() {
        if (busy) {
            if (can_stop)
                emit q->stop_requested();
            return;
        }
        submit();
    });
    QObject::connect(optimize, &QPushButton::clicked, q, [this]() { optimize_now(); });
    QObject::connect(banner_cancel, &QPushButton::clicked, q, [this]() {
        q->set_awaiting_answer(false);
        emit q->answer_cancelled();
        input->setFocus();
    });
    QObject::connect(mode_group, &QButtonGroup::buttonClicked, q, [this](QAbstractButton* b) {
        set_mode(b == deep ? StreamMode::Deep : StreamMode::Lite, true);
    });
    // activated = a user pick; programmatic re-selection must not overwrite the preference.
    QObject::connect(profile, &QComboBox::activated, q, [this](int index) {
        preferred_profile = profile->itemData(index).toInt();
        emit q->profile_changed();
    });

    // What the next run may do follows the engine choice.
    QObject::connect(bar, &ComposerContextBar::engine_changed, q, [this](const QString&) {
        update_perm();
        update_meter();
    });
    QObject::connect(bar, &ComposerContextBar::run_as_task_toggled, q, [this](bool) {
        update_perm();
        update_meter();
    });
    QObject::connect(bar, &ComposerContextBar::context_changed, q, [this]() { update_meter(); });
    meter->summary = [this]() { return bar->budget_summary(); };

    // "@" mention strip.
    QObject::connect(mention_box, &ui::SymbolSearchBox::instrument_picked, q,
                     [this](const core::instrument::Instrument& inst) { pick_symbol(inst); });
    QObject::connect(mention_box, &ui::SymbolSearchBox::unresolved, q,
                     [this](const QStringList&, const QString& message) { mention_box->setToolTip(message); });
    QObject::connect(mention_file, &QPushButton::clicked, q, [this]() {
        close_mention(false);
        bar->open_attach_dialog();
    });
    QObject::connect(mention_close, &QPushButton::clicked, q, [this]() { close_mention(true); });
    new rv::RvKeyHook(mention_box->line_edit(), [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() != Qt::Key_Escape || mention_box->is_popup_visible())
            return false; // the picker's own keys (Esc first closes its list)
        if (apply)
            close_mention(true);
        return true;
    });
    for (QPushButton* b : {mention_file, mention_close})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                close_mention(true);
            return true;
        });

    QObject::connect(tick, &QTimer::timeout, q, [this]() { refresh_activity(); });
    QObject::connect(note_timer, &QTimer::timeout, q, [this]() { note->setVisible(false); });
    new rv::RvEventHook(activity, [this](QEvent::Type t) {
        if (t == QEvent::Resize)
            refresh_activity();
    });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        switch (t) {
            case QEvent::LanguageChange:
                retranslate();
                break;
            case QEvent::Show:
                update_perm(); // executors register after the composer is built
                refresh_activity();
                fit_input();
                break;
            case QEvent::Hide:
                tick->stop();
                break;
            default:
                break;
        }
    });

    // Painted pieces read the live theme; sizes follow the font once it has settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, q,
                     [this](const ui::ThemeTokens&) {
                         meter->update();
                         perm->update();
                     });
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() {
        fitted_width = -1;
        fit_input();
        meter->updateGeometry();
        perm->updateGeometry();
        refresh_activity();
    });
}

void ChatComposer::Impl::retranslate() {
    banner_title->setText(ChatComposer::tr("THE AGENT IS WAITING FOR YOUR ANSWER"));
    banner_hint->setText(ChatComposer::tr("Your next message answers this question, and the agent continues where it "
                                          "stopped (one agent turn)."));
    banner_cancel->setText(ChatComposer::tr("ASK SOMETHING ELSE"));
    banner_cancel->setToolTip(ChatComposer::tr("Stop answering: your next message starts a new request instead"));
    banner_cancel->setAccessibleName(ChatComposer::tr("Ask something else instead of answering"));

    mention_label->setText(ChatComposer::tr("@ MENTION"));
    mention_box->set_placeholder(ChatComposer::tr("Symbol: ticker, name or ISIN"));
    mention_box->setAccessibleName(ChatComposer::tr("Symbol to mention"));
    mention_box->setToolTip(ChatComposer::tr("Pick a symbol (Enter): it goes into your message and into the context "
                                             "chips. Esc closes."));
    mention_file->setText(ChatComposer::tr("ATTACH A TEXT FILE…"));
    mention_file->setToolTip(ChatComposer::tr("Inline a text file into your next message"));
    mention_file->setAccessibleName(ChatComposer::tr("Attach a text file"));
    mention_close->setToolTip(ChatComposer::tr("Close (Esc)"));
    mention_close->setAccessibleName(ChatComposer::tr("Close the mention bar"));

    input->setAccessibleName(ChatComposer::tr("Message the Fincept agent"));
    input->setAccessibleDescription(ChatComposer::tr("Enter or Ctrl+Enter sends, Shift+Enter inserts a new line. "
                                                     "Type / at the start of a line for task templates, @ to "
                                                     "mention a symbol or attach a file. Esc stops a running reply."));

    lite->setToolTip(ChatComposer::tr("Lite: faster, shorter replies"));
    lite->setAccessibleName(ChatComposer::tr("Lite mode"));
    deep->setToolTip(ChatComposer::tr("Deep: more thorough analysis (slower)"));
    deep->setAccessibleName(ChatComposer::tr("Deep mode"));
    profile->setToolTip(ChatComposer::tr("Investment profile the agent takes into account"));
    profile->setAccessibleName(ChatComposer::tr("Investment profile"));
    if (profile->count() > 0)
        profile->setItemText(0, ChatComposer::tr("No profile"));
    optimize->setText(optimizing ? ChatComposer::tr("OPTIMIZING…") : ChatComposer::tr("OPTIMIZE"));
    optimize->setToolTip(ChatComposer::tr("Rewrite the prompt with AI (one request to the Fincept server)"));
    optimize->setAccessibleName(ChatComposer::tr("Optimize the prompt"));
    meter->setAccessibleName(ChatComposer::tr("Message length and budget"));
    q->set_awaiting_answer(awaiting, question); // placeholder + banner question
    update_perm();
    update_meter();
    update_send();
}

void ChatComposer::Impl::update_send() {
    const QString t = trimmed_text();
    const int n = input->document()->characterCount() - 1;
    const int max = agt::kChatMessageMaxChars;
    QString mode_id;
    if (busy) {
        mode_id = QStringLiteral("stop");
        send->setText(ccmp_amp(stop_text.isEmpty() ? ChatComposer::tr("STOP") : stop_text));
        send->setEnabled(can_stop);
        QString tip = stop_tip.isEmpty() ? ChatComposer::tr("Stop this run") : stop_tip;
        send->setToolTip(tip + QStringLiteral(" (Esc)"));
        send->setAccessibleName(ChatComposer::tr("Stop the run"));
        // STOP commits nothing: no agent-risk marker in this state.
        if (send->property("fincept_agent_risk").isValid())
            send->setProperty("fincept_agent_risk", QVariant());
    } else {
        mode_id = QStringLiteral("send");
        QString label;
        if (pending)
            label = ChatComposer::tr("SENDING…");
        else
            label = (awaiting ? ChatComposer::tr("ANSWER") : ChatComposer::tr("SEND")) + QStringLiteral("  ⏎");
        send->setText(label);
        const bool ok = !pending && !t.isEmpty() && n <= max;
        send->setEnabled(ok);
        send->setToolTip(pending      ? ChatComposer::tr("Your message is on its way")
                         : t.isEmpty() ? ChatComposer::tr("Type a message first")
                         : n > max     ? ChatComposer::tr("Too long to send: shorten the message to %1 characters.")
                                         .arg(QLocale().toString(max))
                         : awaiting    ? ChatComposer::tr("Send your answer to the agent (Enter)")
                                       : ChatComposer::tr("Send the message (Enter)"));
        send->setAccessibleName(awaiting ? ChatComposer::tr("Send your answer to the agent")
                                         : ChatComposer::tr("Send message"));
        if (send->property("fincept_agent_risk").toString() != QLatin1String("spend"))
            send->setProperty("fincept_agent_risk", "spend"); // agents: SEND sends a billed model request
    }
    if (send->property("ccmpMode").toString() != mode_id) {
        send->setProperty("ccmpMode", mode_id);
        send->style()->unpolish(send);
        send->style()->polish(send);
    }
    optimize->setEnabled(!busy && !pending && !optimizing && !t.isEmpty());
}

void ChatComposer::Impl::update_perm() {
    const QString engine = bar->engine();
    QString text;
    QString tip;
    if (engine == QLatin1String(agt::kEngineLocal)) {
        text = ChatComposer::tr("MAY: your model · terminal tools · no credits");
        tip = ChatComposer::tr("The terminal's own agentic engine runs the task with your LLM profile and the "
                               "terminal's tools. It spends no Fincept credits and never places an order.");
    } else if (engine == QLatin1String(rv::kHarnessEngineId)) {
        text = ChatComposer::tr("MAY: read-only · paper only · your API key");
        tip = ChatComposer::tr("Your agent harness (Claude Code or Codex) works read-only with the terminal's tools "
                               "and paper only: it never touches a real order. It is billed to your own API key. "
                               "Chat Mode has no full-access mode.");
    } else {
        text = ChatComposer::tr("MAY: read data · terminal tools · trades need your APPROVE");
        tip = ChatComposer::tr("The Fincept cloud agent reads market and portfolio data and may call the terminal's "
                               "tools. It never trades on its own: a proposed trade waits for your APPROVE in the "
                               "transcript. Runs are paper-only in this version.");
    }
    perm->set_text(text);
    perm->setToolTip(tip);
    perm->setAccessibleDescription(tip);
}

void ChatComposer::Impl::update_meter() {
    meter->set_count(input->document()->characterCount() - 1);
    const QString sum = bar->budget_summary();
    QString tip = ChatComposer::tr("%1 of at most %2 characters.")
                      .arg(QLocale().toString(meter->count()), QLocale().toString(agt::kChatMessageMaxChars));
    if (!sum.isEmpty())
        tip += QLatin1Char('\n') + ChatComposer::tr("Next message: %1 (%2).").arg(sum, bar->engine_label());
    if (credits >= 0)
        tip += QLatin1Char('\n') + ChatComposer::tr("Balance: %1 credits.").arg(rv::format_credits(credits));
    meter->setToolTip(tip);
    meter->setAccessibleDescription(tip);
    meter->updateGeometry();
    meter->update();
}

void ChatComposer::Impl::fit_input() {
    const QFontMetrics fm(input->font());
    const int vw = std::max(40, input->viewport()->width() - 2 * static_cast<int>(input->document()->documentMargin()));
    // Visual lines by font metrics, counted only up to the cap.
    int lines = 0;
    const QString text = input->toPlainText();
    qsizetype start = 0;
    while (lines < kCcmpMaxLines) {
        qsizetype end = text.indexOf(QLatin1Char('\n'), start);
        const QStringView para = QStringView(text).mid(start, end < 0 ? text.size() - start : end - start);
        const int w = fm.horizontalAdvance(para.toString());
        lines += std::max(1, static_cast<int>(std::ceil(double(w) / vw)));
        if (end < 0)
            break;
        start = end + 1;
    }
    lines = std::clamp(lines, 1, kCcmpMaxLines);
    if (lines == fitted_lines && vw == fitted_width)
        return;
    fitted_lines = lines;
    fitted_width = vw;
    // The edit's chrome (frame, padding) — measured once it has a viewport.
    int chrome = input->height() - input->viewport()->height();
    if (input->viewport()->height() <= 0 || chrome <= 0)
        chrome = 2 * input->frameWidth() + 10;
    const int h = lines * fm.lineSpacing() + 2 * static_cast<int>(input->document()->documentMargin()) + chrome;
    if (input->height() != h)
        input->setFixedHeight(h);
}

void ChatComposer::Impl::refresh_activity() {
    const bool on = !activity_text.isEmpty() || activity_since > 0;
    if (activity->isVisibleTo(q) != on)
        activity->setVisible(on);
    if (!on) {
        tick->stop();
        return;
    }
    QString t = activity_text.isEmpty() ? ChatComposer::tr("working…") : activity_text.simplified();
    if (activity_since > 0)
        t += QStringLiteral("  ·  ") + ccmp_elapsed(QDateTime::currentMSecsSinceEpoch() - activity_since);
    t = QStringLiteral("● ") + t;
    const QString shown = QFontMetrics(activity->font()).elidedText(t, Qt::ElideRight, std::max(40, activity->width()));
    if (activity->text() != shown)
        activity->setText(shown);
    activity->setToolTip(t);
    const bool want = q->isVisible() && activity_since > 0;
    if (want && !tick->isActive())
        tick->start();
    else if (!want)
        tick->stop();
}

void ChatComposer::Impl::rebuild_profiles() {
    const QSignalBlocker block(profile);
    profile->clear();
    if (profiles.isEmpty()) {
        profile->setVisible(false);
        return;
    }
    profile->addItem(ChatComposer::tr("No profile"), 0);
    int default_id = 0;
    for (const AgentProfile& p : std::as_const(profiles)) {
        profile->addItem(p.name, p.id);
        if (p.is_default && default_id == 0)
            default_id = p.id;
    }
    // Never picked here -> the account's default profile; a pick that no longer
    // exists (deleted elsewhere) also falls back to it.
    int index = preferred_profile >= 0 ? profile->findData(preferred_profile) : -1;
    if (index < 0)
        index = profile->findData(default_id);
    profile->setCurrentIndex(std::max(0, index));
    profile->setVisible(true);
}

void ChatComposer::Impl::set_mode(StreamMode m, bool user) {
    mode = m;
    {
        const QSignalBlocker b1(lite);
        const QSignalBlocker b2(deep);
        (m == StreamMode::Deep ? deep : lite)->setChecked(true);
    }
    if (user)
        emit q->mode_toggled(m);
}

void ChatComposer::Impl::submit() {
    const QString t = trimmed_text();
    if (t.isEmpty())
        return;
    if (busy) {
        show_note(ChatComposer::tr("The agent is still working in this conversation: STOP it or wait for the reply."),
                  false);
        return;
    }
    if (pending)
        return;
    if (t.size() > agt::kChatMessageMaxChars) {
        show_note(ChatComposer::tr("This message is %1 characters long; at most %2 can be sent. Shorten it, or attach "
                                   "long text as context.")
                      .arg(QLocale().toString(static_cast<qlonglong>(t.size())),
                           QLocale().toString(agt::kChatMessageMaxChars)),
                  true);
        return;
    }
    emit q->submit_requested(t);
}

void ChatComposer::Impl::on_escape() {
    if (busy) {
        if (can_stop)
            emit q->stop_requested();
        return;
    }
    if (trimmed_text().isEmpty())
        emit q->escape_pressed();
    // A draft stays as it is: Esc never discards typed text.
}

void ChatComposer::Impl::after_trigger(QChar ch) {
    const QTextCursor c = input->textCursor();
    const int pib = c.positionInBlock();
    const QString block = c.block().text();
    if (pib < 1 || pib > block.size() || block.at(pib - 1) != ch)
        return;
    if (ch == QLatin1Char('/')) {
        if (pib == 1) // "/" at the start of a line
            open_template_menu(c.position() - 1);
        return;
    }
    // "@" at the start of a line or after a space (not inside an e-mail address).
    if (pib >= 2 && !block.at(pib - 2).isSpace())
        return;
    open_mention(c.position() - 1);
}

void ChatComposer::Impl::open_template_menu(int slash_pos) {
    auto& lib = agt::RunTemplateLibrary::instance();
    lib.ensure_loaded();
    const QVector<agt::RunTemplate> list = lib.templates();
    auto* menu = new QMenu(q);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(ui::ScreenCommandBar::menu_css());
    menu->setToolTipsVisible(true);
    menu->setAccessibleName(ChatComposer::tr("Prompt templates"));
    QString category;
    for (const agt::RunTemplate& t : list) {
        const QString cat = t.category.isEmpty() ? (t.user_defined ? ChatComposer::tr("Mine") : QString()) : t.category;
        if (cat != category) {
            category = cat;
            if (!cat.isEmpty()) {
                QAction* head = menu->addAction(ccmp_amp(cat.toUpper()));
                head->setEnabled(false);
            }
        }
        QAction* a = menu->addAction(ccmp_amp(t.title));
        a->setToolTip(t.description);
        const QString id = t.id;
        QObject::connect(a, &QAction::triggered, q, [this, id, slash_pos]() { apply_template(id, slash_pos); });
    }
    if (list.isEmpty()) {
        QAction* none = menu->addAction(lib.is_loaded() ? ChatComposer::tr("No templates") : ChatComposer::tr("Loading…"));
        none->setEnabled(false);
    }
    menu->addSeparator();
    QAction* all = menu->addAction(ChatComposer::tr("All templates…"));
    all->setToolTip(ChatComposer::tr("Open NEW RUN: templates with their parameters, engine and budget"));
    QObject::connect(all, &QAction::triggered, q, [this, slash_pos]() {
        replace_token(slash_pos, QLatin1Char('/'), QString());
        emit q->templates_requested();
    });
    QObject::connect(menu, &QMenu::aboutToHide, q, [this]() {
        QTimer::singleShot(0, input, [this]() { input->setFocus(); });
    });
    menu->popup(input->viewport()->mapToGlobal(input->cursorRect().bottomLeft())); // non-blocking
}

void ChatComposer::Impl::apply_template(const QString& id, int slash_pos) {
    const std::optional<agt::RunTemplate> t = agt::RunTemplateLibrary::instance().find(id);
    if (!t)
        return;
    QJsonObject ctx = agt::template_default_context(*t);
    // Symbols the composer already carries fill the template's symbol parameters.
    const QStringList syms = bar->symbols();
    if (!syms.isEmpty()) {
        for (const agt::RunTemplateParam& p : t->params) {
            if (p.type == agt::TemplateParamType::Symbol && ctx.value(p.key).toString().isEmpty())
                ctx.insert(p.key, syms.first());
            else if (p.type == agt::TemplateParamType::Symbols && ctx.value(p.key).toString().isEmpty())
                ctx.insert(p.key, syms.join(QStringLiteral(", ")));
        }
    }
    const QString filled = agt::fill_run_template(t->prompt, ctx).trimmed();
    const bool needs_form = !agt::validate_template_params(*t, ctx).isEmpty() ||
                            !agt::run_template_placeholders(filled).isEmpty() || filled.isEmpty();
    if (needs_form) {
        // Parameters to choose: NEW RUN's form, preselected (it only opens the form).
        replace_token(slash_pos, QLatin1Char('/'), QString());
        emit q->template_requested(id);
        return;
    }
    if (!replace_token(slash_pos, QLatin1Char('/'), filled)) {
        QTextCursor c = input->textCursor();
        c.insertText(filled);
        input->setTextCursor(c);
    }
    input->setFocus();
}

bool ChatComposer::Impl::replace_token(int pos, QChar ch, const QString& with) {
    QTextDocument* doc = input->document();
    if (pos < 0 || pos >= doc->characterCount() - 1 || doc->characterAt(pos) != ch)
        return false;
    QTextCursor c(doc);
    c.setPosition(pos);
    c.setPosition(pos + 1, QTextCursor::KeepAnchor);
    c.insertText(with);
    input->setTextCursor(c);
    return true;
}

void ChatComposer::Impl::open_mention(int at_pos) {
    mention_pos = at_pos;
    mention_box->set_text(QString());
    mention->setVisible(true);
    mention_box->focus_and_select();
}

void ChatComposer::Impl::close_mention(bool focus_input) {
    mention->setVisible(false);
    mention_pos = -1;
    if (focus_input)
        input->setFocus();
}

void ChatComposer::Impl::pick_symbol(const core::instrument::Instrument& inst) {
    const QString sym = inst.symbol.isEmpty() ? inst.display : inst.symbol;
    if (sym.isEmpty())
        return;
    const int pos = mention_pos;
    close_mention(true);
    // The "@" becomes the symbol; it also goes with the message as a context chip.
    if (!replace_token(pos, QLatin1Char('@'), sym + QLatin1Char(' '))) {
        QTextCursor c = input->textCursor();
        c.insertText(sym + QLatin1Char(' '));
        input->setTextCursor(c);
    }
    bar->add_symbol(sym);
}

bool ChatComposer::Impl::attach_files(const QMimeData* mime) {
    const QStringList files = ccmp_local_files(mime);
    if (files.isEmpty())
        return false;
    bar->attach_file(files.first());
    if (files.size() > 1)
        show_note(ChatComposer::tr("One file is attached at a time: %1. Attach the others with later messages.")
                      .arg(QFileInfo(files.first()).fileName()),
                  false);
    return true;
}

void ChatComposer::Impl::optimize_now() {
    const QString text = trimmed_text();
    if (text.isEmpty() || busy || pending || optimizing)
        return;
    optimizing = true;
    optimize->setText(ChatComposer::tr("OPTIMIZING…"));
    update_send();
    QPointer<ChatComposer> self(q);
    ChatModeService::instance().optimize_prompt(
        text, mode == StreamMode::Deep ? QStringLiteral("deep") : QStringLiteral("lite"),
        [self](bool ok, OptimizedPrompt result, QString err) {
            if (!self)
                return;
            Impl* d = self->d_.get();
            d->optimizing = false;
            d->optimize->setText(ChatComposer::tr("OPTIMIZE"));
            if (!ok)
                d->show_note(ChatComposer::tr("Optimize failed: %1").arg(err), true);
            else if (!result.optimized.trimmed().isEmpty())
                self->set_text(result.optimized);
            d->update_send();
        });
}

void ChatComposer::Impl::show_note(const QString& text, bool error) {
    if (note->property("error").toBool() != error) {
        // The tone is a stylesheet selector ([error="true"]): re-polish on a change.
        note->setProperty("error", error);
        note->style()->unpolish(note);
        note->style()->polish(note);
    }
    note->setText(text);
    note->setVisible(!text.isEmpty());
    if (!text.isEmpty())
        note_timer->start();
}

// ── ChatComposer ─────────────────────────────────────────────────────────────

ChatComposer::ChatComposer(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
    d_->fit_input();
}

ChatComposer::~ChatComposer() = default;

ComposerContextBar* ChatComposer::context_bar() const {
    return d_->bar;
}

QPlainTextEdit* ChatComposer::input() const {
    return d_->input;
}

QString ChatComposer::text() const {
    return d_->input->toPlainText();
}

void ChatComposer::set_text(const QString& text) {
    if (d_->input->toPlainText() == text)
        return;
    d_->input->setPlainText(text);
    QTextCursor c = d_->input->textCursor();
    c.movePosition(QTextCursor::End);
    d_->input->setTextCursor(c);
}

void ChatComposer::clear_text() {
    if (!d_->input->document()->isEmpty())
        d_->input->clear();
}

void ChatComposer::focus_input() {
    d_->input->setFocus(Qt::ShortcutFocusReason);
}

void ChatComposer::insert_quote(const QString& text) {
    const QString body = text.trimmed();
    if (body.isEmpty())
        return;
    QStringList lines = body.split(QLatin1Char('\n'));
    for (QString& l : lines)
        l = QStringLiteral("> ") + l;
    QString draft = d_->input->toPlainText();
    if (!draft.isEmpty())
        draft += draft.endsWith(QLatin1Char('\n')) ? QStringLiteral("\n") : QStringLiteral("\n\n");
    draft += lines.join(QLatin1Char('\n')) + QStringLiteral("\n\n");
    set_text(draft);
    focus_input();
}

void ChatComposer::set_run_state(bool busy, bool pending, bool can_stop, const QString& stop_text,
                                 const QString& stop_tooltip) {
    d_->busy = busy;
    d_->pending = pending;
    d_->can_stop = can_stop;
    d_->stop_text = stop_text;
    d_->stop_tip = stop_tooltip;
    d_->update_send();
}

void ChatComposer::set_awaiting_answer(bool on, const QString& question) {
    d_->awaiting = on;
    d_->question = on ? question : QString();
    QString q = d_->question.trimmed();
    if (q.size() > kCcmpQuestionChars)
        q = q.left(kCcmpQuestionChars - 1) + QChar(0x2026);
    d_->banner_question->setText(q);
    d_->banner_question->setVisible(!q.isEmpty());
    if (d_->banner->isVisibleTo(this) != on)
        d_->banner->setVisible(on);
    d_->input->setPlaceholderText(on ? tr("Type your answer to the agent... (Enter to send)")
                                     : tr("Ask anything... (Enter to send, Shift+Enter for new line)"));
    d_->update_send();
}

bool ChatComposer::is_awaiting_answer() const {
    return d_->awaiting;
}

void ChatComposer::set_activity(const QString& text, qint64 since_ms) {
    d_->activity_text = text;
    d_->activity_since = since_ms;
    d_->refresh_activity();
}

void ChatComposer::show_note(const QString& text, bool error) {
    d_->show_note(text, error);
}

void ChatComposer::set_stream_mode(StreamMode mode) {
    d_->set_mode(mode, false);
}

StreamMode ChatComposer::stream_mode() const {
    return d_->mode;
}

void ChatComposer::set_profiles(const QVector<AgentProfile>& profiles) {
    d_->profiles = profiles;
    d_->rebuild_profiles();
}

int ChatComposer::selected_profile_id() const {
    if (d_->profiles.isEmpty() || d_->profile->currentIndex() < 0)
        return 0;
    return d_->profile->currentData().toInt();
}

int ChatComposer::preferred_profile_id() const {
    return d_->preferred_profile;
}

void ChatComposer::set_preferred_profile_id(int id) {
    d_->preferred_profile = id;
    d_->rebuild_profiles();
}

void ChatComposer::set_credits(double balance) {
    d_->credits = balance;
    d_->update_meter();
}

double ChatComposer::credits() const {
    return d_->credits;
}

} // namespace fincept::chat_mode
