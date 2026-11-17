// TranscriptBlocks.cpp — the widgets of Chat Mode's transcript blocks (see the header).

#include "screens/chat_mode/TranscriptBlocks.h"

#include "screens/chat_mode/ArtefactTableModel.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::chat_mode::transcript {

namespace al = fincept::agent_tasks::artefact_logic;
namespace rv = fincept::chat_mode::run_view;

namespace transcript_blocks_detail {

QString ctb_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::chat_mode::TranscriptView", text, disambiguation, n);
}

constexpr int kCtbTableRows = 25;        ///< table rows shown before SHOW ALL
constexpr int kCtbTableRowsAll = 5000;   ///< rows SHOW ALL draws in the transcript (COPY AS CSV has every row)
constexpr int kCtbCodeLines = 24;        ///< code lines shown before SHOW ALL
constexpr int kCtbResultRows = 200;      ///< tool result rows before SHOW MORE
constexpr int kCtbResultRowsAll = 5000;
constexpr int kCtbResultFields = 40;
constexpr int kCtbSources = 8;

// ── Font metrics (estimates) ────────────────────────────────────────────────

struct CtbMetrics {
    bool valid = false;
    double avg0 = 8, avg1 = 7, avg2 = 7, avg3 = 6;
    int lh0 = 18, lh1 = 16, lh2 = 15, lh3 = 14, lh_h1 = 24, lh_h2 = 20;
};

CtbMetrics& ctb_metrics() {
    static CtbMetrics m;
    if (!m.valid) {
        const QFontMetricsF f0(ui::fonts::font(0)), f1(ui::fonts::font(-1)), f2(ui::fonts::font(-2)), f3(ui::fonts::font(-3));
        m.avg0 = std::max(1.0, f0.averageCharWidth());
        m.avg1 = std::max(1.0, f1.averageCharWidth());
        m.avg2 = std::max(1.0, f2.averageCharWidth());
        m.avg3 = std::max(1.0, f3.averageCharWidth());
        m.lh0 = static_cast<int>(std::ceil(f0.lineSpacing()));
        m.lh1 = static_cast<int>(std::ceil(f1.lineSpacing()));
        m.lh2 = static_cast<int>(std::ceil(f2.lineSpacing()));
        m.lh3 = static_cast<int>(std::ceil(f3.lineSpacing()));
        m.lh_h1 = static_cast<int>(std::ceil(QFontMetricsF(ui::fonts::font(4, QFont::Bold)).lineSpacing()));
        m.lh_h2 = static_cast<int>(std::ceil(QFontMetricsF(ui::fonts::font(2, QFont::Bold)).lineSpacing()));
        m.valid = true;
    }
    return m;
}

/// Gap under a block, from the base line height.
int ctb_gap() {
    return std::max(4, ctb_metrics().lh0 / 3);
}

/// Wrapped lines of `chars` characters with `breaks` line breaks at `width`.
int ctb_lines(int chars, int breaks, double avg, int width) {
    const double per_line = std::max(8.0, static_cast<double>(width) / avg);
    return std::max(1, breaks + static_cast<int>(std::ceil(chars / per_line)));
}

void ctb_text_stats(Item& it, const QString& text) {
    if (it.est_chars >= 0)
        return;
    it.est_chars = static_cast<int>(text.size());
    it.est_breaks = static_cast<int>(text.count(QLatin1Char('\n')));
}

// ── Small widget helpers ────────────────────────────────────────────────────

QLabel* ctb_label(QWidget* parent, const char* name, bool selectable = true) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(name));
    l->setTextFormat(Qt::PlainText);
    l->setWordWrap(true);
    if (selectable)
        l->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    return l;
}

/// A rich-text label: our escaped HTML only, links through the host's allow-list.
QLabel* ctb_rich(QWidget* parent, const char* name, const BlockHost* host) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(name));
    l->setTextFormat(Qt::RichText);
    l->setWordWrap(true);
    l->setOpenExternalLinks(false);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QObject::connect(l, &QLabel::linkActivated, l, [host](const QString& url) {
        if (host && host->open_link)
            host->open_link(url);
    });
    QObject::connect(l, &QLabel::linkHovered, l, [l](const QString& url) {
        l->setToolTip(url.startsWith(QLatin1String(md::kSymbolScheme))
                          ? ctb_tr("Open %1 — click for the symbol actions").arg(url.section(QLatin1Char(':'), 1))
                          : url);
    });
    return l;
}

void ctb_set_rich(QLabel* l, const QString& inner_html) {
    l->setText(QStringLiteral("<html><head><style>") + rich_css() + QStringLiteral("</style></head><body>") + inner_html +
               QStringLiteral("</body></html>"));
}

QToolButton* ctb_mini(QWidget* parent, const QString& text, const QString& tip, const char* name = "ctbMini") {
    auto* b = new QToolButton(parent);
    b->setObjectName(QLatin1String(name));
    b->setText(QString(text).replace(QLatin1Char('&'), QStringLiteral("&&")));
    b->setToolTip(tip);
    b->setAccessibleName(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoRaise(true);
    return b;
}

QPushButton* ctb_button(QWidget* parent, const QString& text, const char* name) {
    auto* b = new QPushButton(QString(text).replace(QLatin1Char('&'), QStringLiteral("&&")), parent);
    b->setObjectName(QLatin1String(name));
    b->setCursor(Qt::PointingHandCursor);
    b->setAccessibleName(text);
    return b;
}

/// A dynamic-property tone ("info" | "success" | "warning" | "danger" | "neutral" | "accent") the sheet colours.
void ctb_set_tone(QWidget* w, const char* tone) {
    if (!w || w->property("tone").toString() == QLatin1String(tone))
        return;
    w->setProperty("tone", QString::fromLatin1(tone));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

const char* ctb_tone_name(rv::Tone t) {
    switch (t) {
        case rv::Tone::Info:
            return "info";
        case rv::Tone::Success:
            return "success";
        case rv::Tone::Warning:
            return "warning";
        case rv::Tone::Danger:
            return "danger";
        case rv::Tone::Neutral:
            break;
    }
    return "neutral";
}

/// Copy `text` and say so on `button` for a moment.
void ctb_copy(QAbstractButton* button, const QString& text) {
    if (QClipboard* cb = QApplication::clipboard())
        cb->setText(text);
    if (!button)
        return;
    const QString old = button->text();
    if (old == ctb_tr("COPIED"))
        return;
    button->setText(ctb_tr("COPIED"));
    QPointer<QAbstractButton> b(button);
    QTimer::singleShot(1200, button, [b, old]() {
        if (b)
            b->setText(old);
    });
}

int ctb_layout_height(QWidget* w, int width) {
    if (QLayout* l = w->layout()) {
        if (l->hasHeightForWidth())
            return l->totalHeightForWidth(width);
        return l->totalSizeHint().height();
    }
    return w->sizeHint().height();
}

QString ctb_when(const QDateTime& when) {
    if (!when.isValid())
        return {};
    const QDateTime local = when.toLocalTime();
    if (local.date() == QDate::currentDate())
        return local.toString(QStringLiteral("HH:mm"));
    return QLocale().toString(local, QStringLiteral("dd MMM HH:mm"));
}

// ── Header ──────────────────────────────────────────────────────────────────

class CtbHeader : public Block {
  public:
    CtbHeader(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        setObjectName(QStringLiteral("ctbHeader"));
        who_ = ctb_label(this, "ctbWho", false);
        who_->setWordWrap(false);
        meta_ = ctb_label(this, "ctbMeta", false);
        meta_->setWordWrap(false);
        acts_ = new QWidget(this);
        acts_->setObjectName(QStringLiteral("ctbActs"));
        auto* hl = new QHBoxLayout(acts_);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(2);
        copy_ = ctb_mini(acts_, ctb_tr("COPY"), ctb_tr("Copy this message"), "ctbAct");
        quote_ = ctb_mini(acts_, ctb_tr("QUOTE"), ctb_tr("Quote it in the message box"), "ctbAct");
        again_ = ctb_mini(acts_, ctb_tr("USE AGAIN"), ctb_tr("Put this message back in the message box (nothing is sent)"),
                          "ctbAct");
        notes_ = ctb_mini(acts_, ctb_tr("NOTES"), ctb_tr("Send this reply to Notes (kept with a link to its run)"), "ctbAct");
        retry_ = ctb_mini(acts_, ctb_tr("RETRY STEP"), ctb_tr("Run this step again (a new run from this step)"), "ctbAct");
        retry_->setProperty("fincept_agent_risk", "spend"); // agents: re-runs an agent step (model quota)
        for (QToolButton* b : {copy_, quote_, again_, notes_, retry_})
            hl->addWidget(b);
        QObject::connect(copy_, &QToolButton::clicked, this, [this]() {
            if (const Turn* t = turn_())
                ctb_copy(copy_, t->text);
        });
        QObject::connect(quote_, &QToolButton::clicked, this, [this]() { act("quote"); });
        QObject::connect(again_, &QToolButton::clicked, this, [this]() { act("use_again"); });
        QObject::connect(notes_, &QToolButton::clicked, this, [this]() { act("notes"); });
        QObject::connect(retry_, &QToolButton::clicked, this, [this]() { act("retry_step"); });
        sync();
    }

    void sync() override {
        const Turn* t = turn_();
        if (!t)
            return;
        const bool agent = t->role != QLatin1String("user");
        QString who = agent ? ctb_tr("AGENT") : ctb_tr("YOU");
        if (!t->label.isEmpty())
            who += QStringLiteral("  ·  ") + t->label.toUpper();
        who_->setText(who);
        if (who_->property("role").toString() != (agent ? QLatin1String("agent") : QLatin1String("user"))) {
            who_->setProperty("role", agent ? QStringLiteral("agent") : QStringLiteral("user"));
            who_->style()->unpolish(who_);
            who_->style()->polish(who_);
        }
        QStringList meta;
        const QString when = ctb_when(t->when);
        if (!when.isEmpty())
            meta << when;
        if (t->tokens > 0)
            meta << ctb_tr("%1 TOK").arg(QLocale().toString(t->tokens));
        if (t->duration_ms >= 0)
            meta << rv::ActivityLog::format_duration_precise(t->duration_ms).toUpper();
        if (t->credits > 0)
            meta << ctb_tr("%1 CR").arg(rv::format_credits(t->credits));
        if (t->streaming)
            meta << ctb_tr("WRITING…");
        if (t->failed)
            meta << ctb_tr("FAILED");
        meta_->setText(meta.join(QStringLiteral("  ·  ")));
        meta_->setAccessibleName(meta.join(QStringLiteral(", ")));
        setAccessibleName(who + QStringLiteral(" ") + meta.join(QStringLiteral(" ")));

        const bool has_text = !t->text.trimmed().isEmpty();
        copy_->setVisible(has_text);
        quote_->setVisible(has_text && !t->streaming);
        again_->setVisible(!agent && has_text);
        notes_->setVisible(agent && has_text && !t->streaming && !t->run_id.isEmpty());
        retry_->setVisible(agent && !t->streaming && t->step >= 0 && !t->run_id.isEmpty());
        const bool want_report = agent && has_text && !t->streaming;
        if (want_report && !report_) {
            report_ = new ui::AddToReportButton(QStringLiteral("Chat Mode"), acts_);
            report_->set_show_result(false);
            const QPointer<CtbHeader> self(this);
            report_->set_payload_provider([self]() {
                report::ReportPayload p;
                if (!self)
                    return p;
                const Turn* tt = self->turn_();
                if (!tt)
                    return p;
                QString title = tt->text.section(QLatin1Char('\n'), 0, 0).trimmed();
                while (title.startsWith(QLatin1Char('#')))
                    title = title.mid(1).trimmed();
                p.title = title.left(80);
                p.source = QStringLiteral("Chat Mode");
                p.as_of = tt->when.isValid() ? tt->when : QDateTime::currentDateTime();
                p.markdown = tt->text;
                return p;
            });
            static_cast<QHBoxLayout*>(acts_->layout())->addWidget(report_);
        }
        if (report_)
            report_->setVisible(want_report);
        set_hot(host_ && host_->hovered_turn && host_->hovered_turn() == item_->turn);
        place();
    }

    void set_hot(bool hot) override {
        for (QToolButton* b : {copy_, quote_, again_, notes_, retry_}) {
            if (b->property("hot").toBool() == hot)
                continue;
            b->setProperty("hot", hot);
            b->style()->unpolish(b);
            b->style()->polish(b);
        }
    }

    int height_for(int width) override {
        const int row = row_height();
        return top_pad() + (two_rows(width) ? row * 2 + 2 : row) + 2;
    }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        Block::resizeEvent(e);
        place();
    }

    void paintEvent(QPaintEvent*) override {
        if (item_->turn <= 0)
            return; // no rule above the first turn
        QPainter p(this);
        p.setPen(QColor(ui::colors::BORDER_DIM()));
        const int y = top_pad() / 2;
        p.drawLine(0, y, width(), y);
    }

  private:
    const Turn* turn_() const { return host_ && host_->turn ? host_->turn(item_->turn) : nullptr; }

    void act(const char* kind) {
        if (host_ && host_->act)
            host_->act(Action{QString::fromLatin1(kind), item_->id, item_->turn, {}});
    }

    int top_pad() const { return item_->turn <= 0 ? 2 : std::max(10, ctb_metrics().lh0); }
    int row_height() const {
        return std::max({who_->sizeHint().height(), meta_->sizeHint().height(), acts_->sizeHint().height()});
    }
    int left_width() const { return who_->sizeHint().width() + 12 + meta_->sizeHint().width(); }
    bool two_rows(int width) const { return left_width() + 16 + acts_->sizeHint().width() > width; }

    void place() {
        const int w = width();
        const int row = row_height();
        int y = top_pad();
        const QSize ws = who_->sizeHint();
        who_->setGeometry(0, y + (row - ws.height()) / 2, ws.width(), ws.height());
        const QSize ms = meta_->sizeHint();
        const int mx = ws.width() + 12;
        meta_->setGeometry(mx, y + (row - ms.height()) / 2, std::max(0, std::min(ms.width(), w - mx)), ms.height());
        const QSize as = acts_->sizeHint();
        if (two_rows(w))
            y += row + 2;
        acts_->setGeometry(std::max(0, w - as.width()), y + (row - as.height()) / 2, std::min(as.width(), w), as.height());
    }

    QLabel* who_ = nullptr;
    QLabel* meta_ = nullptr;
    QWidget* acts_ = nullptr;
    QToolButton* copy_ = nullptr;
    QToolButton* quote_ = nullptr;
    QToolButton* again_ = nullptr;
    QToolButton* notes_ = nullptr;
    QToolButton* retry_ = nullptr;
    ui::AddToReportButton* report_ = nullptr;
};

// ── User text ───────────────────────────────────────────────────────────────

class CtbUser : public Block {
  public:
    CtbUser(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(0);
        frame_ = new QFrame(this);
        frame_->setObjectName(QStringLiteral("ctbUser"));
        auto* fl = new QVBoxLayout(frame_);
        fl->setContentsMargins(12, 6, 10, 6);
        fl->setSpacing(3);
        text_ = ctb_label(frame_, "ctbUserText");
        sent_ = ctb_label(frame_, "ctbSentWith");
        fl->addWidget(text_);
        fl->addWidget(sent_);
        vl->addWidget(frame_);
        sync();
    }
    void sync() override {
        text_->setText(item_->text);
        sent_->setText(item_->text2.isEmpty() ? QString() : ctb_tr("Sent with: %1").arg(item_->text2));
        sent_->setVisible(!item_->text2.isEmpty());
    }

  private:
    QFrame* frame_ = nullptr;
    QLabel* text_ = nullptr;
    QLabel* sent_ = nullptr;
};

// ── Rich text (paragraph, heading, list, quote) and notes ───────────────────

class CtbRich : public Block {
  public:
    CtbRich(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        const md::Block& b = item_->block;
        const char* name = "ctbText";
        if (item_->kind == ItemKind::Note)
            name = "ctbNote";
        else if (b.kind == md::BlockKind::Heading)
            name = b.level <= 1 ? "ctbH1" : b.level == 2 ? "ctbH2" : "ctbH3";
        else if (b.kind == md::BlockKind::Quote)
            name = "ctbQuote";
        const bool heading = item_->kind == ItemKind::Markdown && b.kind == md::BlockKind::Heading;
        vl->setContentsMargins(0, heading ? ctb_gap() : 0, 0, ctb_gap());
        vl->setSpacing(0);
        label_ = item_->kind == ItemKind::Note ? ctb_label(this, name) : ctb_rich(this, name, host);
        vl->addWidget(label_);
        sync();
    }
    void sync() override {
        if (item_->kind == ItemKind::Note) {
            label_->setText(item_->text);
            return;
        }
        ctb_set_rich(label_, item_->block.html);
    }

  private:
    QLabel* label_ = nullptr;
};

class CtbRule : public Block {
  public:
    using Block::Block;
    int height_for(int) override { return ctb_gap() * 2 + 1; }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(QColor(ui::colors::BORDER_MED()));
        const int y = height() / 2;
        p.drawLine(0, y, width(), y);
    }
};

// ── Code ────────────────────────────────────────────────────────────────────

class CtbCode : public Block {
  public:
    CtbCode(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(0);
        frame_ = new QFrame(this);
        frame_->setObjectName(QStringLiteral("ctbCode"));
        auto* fl = new QVBoxLayout(frame_);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(0);
        auto* head = new QWidget(frame_);
        head->setObjectName(QStringLiteral("ctbCodeHead"));
        auto* hl = new QHBoxLayout(head);
        hl->setContentsMargins(10, 2, 4, 2);
        hl->setSpacing(4);
        lang_ = ctb_label(head, "ctbCap", false);
        lang_->setWordWrap(false);
        more_ = ctb_mini(head, QString(), QString());
        copy_ = ctb_mini(head, ctb_tr("COPY"), ctb_tr("Copy the code"));
        // Python only (as AI Chat): a one-cell notebook opened in the Code tab — nothing runs.
        open_ = ctb_mini(head, ctb_tr("OPEN IN CODE TAB"),
                         ctb_tr("Open this Python code in the Code tab (nothing runs by itself)"));
        hl->addWidget(lang_);
        hl->addStretch(1);
        hl->addWidget(more_);
        hl->addWidget(copy_);
        hl->addWidget(open_);
        fl->addWidget(head);
        body_ = new QPlainTextEdit(frame_);
        body_->setObjectName(QStringLiteral("ctbCodeBody"));
        body_->setReadOnly(true);
        body_->setLineWrapMode(QPlainTextEdit::NoWrap);
        body_->setFrameShape(QFrame::NoFrame);
        body_->setTabChangesFocus(true);
        body_->setAccessibleName(ctb_tr("Code"));
        fl->addWidget(body_);
        vl->addWidget(frame_);
        QObject::connect(copy_, &QToolButton::clicked, this, [this]() { ctb_copy(copy_, item_->block.plain); });
        QObject::connect(open_, &QToolButton::clicked, this, [this]() {
            if (host_ && host_->act)
                host_->act(Action{QStringLiteral("open_code"), item_->id, item_->turn, item_->block.plain});
        });
        QObject::connect(more_, &QToolButton::clicked, this, [this]() {
            item_->show_all = !item_->show_all;
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }

    void sync() override {
        const md::Block& b = item_->block;
        lang_->setText(b.lang.isEmpty() ? ctb_tr("CODE") : b.lang.toUpper());
        const QString l = b.lang.trimmed().toLower();
        open_->setVisible(l == QLatin1String("python") || l == QLatin1String("py") || l == QLatin1String("ipython") ||
                          l == QLatin1String("python3"));
        if (body_->toPlainText() != b.plain)
            body_->setPlainText(b.plain);
        lines_ = static_cast<int>(b.plain.count(QLatin1Char('\n'))) + 1;
        more_->setVisible(lines_ > kCtbCodeLines);
        more_->setText(item_->show_all ? ctb_tr("SHOW LESS") : ctb_tr("SHOW ALL %1 LINES").arg(lines_));
        body_->setVerticalScrollBarPolicy(item_->show_all || lines_ <= kCtbCodeLines ? Qt::ScrollBarAlwaysOff
                                                                                   : Qt::ScrollBarAsNeeded);
    }

    int height_for(int width) override {
        ensurePolished();
        const QFontMetrics fm(body_->font());
        const int shown = item_->show_all ? lines_ : std::min(lines_, kCtbCodeLines);
        int longest = 0;
        int i = 0;
        for (const QString& line : item_->block.plain.split(QLatin1Char('\n'))) {
            longest = std::max(longest, fm.horizontalAdvance(line));
            if (++i > 400)
                break;
        }
        const int margins = static_cast<int>(body_->document()->documentMargin() * 2) + 4;
        const bool hbar = longest + margins > width;
        int h = shown * fm.lineSpacing() + margins + (hbar ? body_->horizontalScrollBar()->sizeHint().height() : 0);
        body_->setFixedHeight(h);
        return 2 + frame_->layout()->itemAt(0)->widget()->sizeHint().height() + h + 2 + ctb_gap();
    }

  private:
    QFrame* frame_ = nullptr;
    QLabel* lang_ = nullptr;
    QToolButton* copy_ = nullptr;
    QToolButton* more_ = nullptr;
    QToolButton* open_ = nullptr;
    QPlainTextEdit* body_ = nullptr;
    int lines_ = 1;
};

// ── Table ───────────────────────────────────────────────────────────────────

/// A dense painted grid: header, right-aligned tabular numbers, hairlines. Paints
/// only the rows in the exposed rectangle.
class CtbGrid : public QWidget {
  public:
    explicit CtbGrid(QWidget* parent) : QWidget(parent) { setObjectName(QStringLiteral("ctbGrid")); }

    void set_table(const md::Block* block, int rows_shown) {
        block_ = block;
        rows_ = std::min(rows_shown, static_cast<int>(block->rows.size()));
        fit();
        update();
    }

    void fit() {
        if (!block_)
            return;
        const QFontMetrics fm(ui::fonts::font(-1));
        const QFontMetrics hfm(ui::fonts::font(-2, QFont::Bold));
        pad_ = std::max(6, fm.averageCharWidth());
        row_h_ = fm.height() + 6;
        const int cols = static_cast<int>(block_->header.size());
        widths_.assign(static_cast<size_t>(cols), 0);
        const int lo = fm.averageCharWidth() * 4;
        const int hi = fm.averageCharWidth() * 40;
        for (int c = 0; c < cols; ++c) {
            int w = hfm.horizontalAdvance(block_->header.at(c));
            const int scan = std::min(static_cast<int>(block_->rows.size()), 200);
            for (int r = 0; r < scan; ++r)
                w = std::max(w, fm.horizontalAdvance(block_->rows.at(r).value(c)));
            widths_[static_cast<size_t>(c)] = std::clamp(w + 2 * pad_, lo, hi);
        }
        int total = 0;
        for (int w : widths_)
            total += w;
        setFixedSize(std::max(1, total), row_h_ * (rows_ + 1) + 1);
    }

    int content_width() const { return width(); }

  protected:
    void paintEvent(QPaintEvent* e) override {
        if (!block_)
            return;
        QPainter p(this);
        const QFont font = ui::fonts::font(-1);
        const QFont hfont = ui::fonts::font(-2, QFont::Bold);
        const QColor line(ui::colors::BORDER_DIM());
        const QColor head_bg(ui::colors::BG_RAISED());
        const QColor alt(ui::colors::ROW_ALT());
        const QColor text(ui::colors::TEXT_PRIMARY());
        const QColor head_text(ui::colors::TEXT_SECONDARY());
        const int cols = static_cast<int>(widths_.size());
        // Header.
        p.fillRect(QRect(0, 0, width(), row_h_), head_bg);
        p.setFont(hfont);
        p.setPen(head_text);
        int x = 0;
        for (int c = 0; c < cols; ++c) {
            const int w = widths_[static_cast<size_t>(c)];
            const bool num = c < block_->numeric.size() && block_->numeric.at(c);
            const QRect cell(x + pad_, 0, w - 2 * pad_, row_h_);
            p.drawText(cell, (num ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(block_->header.at(c), Qt::ElideRight, cell.width()));
            x += w;
        }
        // Body rows in the exposed area only.
        p.setFont(font);
        const int first = std::max(0, e->rect().top() / row_h_ - 1);
        const int last = std::min(rows_ - 1, e->rect().bottom() / row_h_);
        for (int r = first; r <= last; ++r) {
            const int y = (r + 1) * row_h_;
            if (r % 2 == 1)
                p.fillRect(QRect(0, y, width(), row_h_), alt);
            p.setPen(text);
            x = 0;
            const QStringList& row = block_->rows.at(r);
            for (int c = 0; c < cols; ++c) {
                const int w = widths_[static_cast<size_t>(c)];
                const bool num = c < block_->numeric.size() && block_->numeric.at(c);
                const QRect cell(x + pad_, y, w - 2 * pad_, row_h_);
                p.drawText(cell, (num ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(row.value(c), Qt::ElideRight, cell.width()));
                x += w;
            }
            p.setPen(line);
            p.drawLine(0, y, width(), y);
        }
        p.setPen(line);
        p.drawLine(0, row_h_ * (rows_ + 1), width(), row_h_ * (rows_ + 1));
    }

  private:
    const md::Block* block_ = nullptr;
    std::vector<int> widths_;
    int rows_ = 0;
    int row_h_ = 20;
    int pad_ = 6;
};

class CtbTable : public Block {
  public:
    CtbTable(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(0);
        frame_ = new QFrame(this);
        frame_->setObjectName(QStringLiteral("ctbTable"));
        auto* fl = new QVBoxLayout(frame_);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->setSpacing(0);
        head_ = new QWidget(frame_);
        head_->setObjectName(QStringLiteral("ctbCodeHead"));
        auto* hl = new QHBoxLayout(head_);
        hl->setContentsMargins(10, 2, 4, 2);
        hl->setSpacing(4);
        cap_ = ctb_label(head_, "ctbCap", false);
        cap_->setWordWrap(false);
        more_ = ctb_mini(head_, QString(), QString());
        csv_ = ctb_mini(head_, ctb_tr("COPY AS CSV"), ctb_tr("Copy every row as CSV (spreadsheet-safe)"));
        hl->addWidget(cap_);
        hl->addStretch(1);
        hl->addWidget(more_);
        hl->addWidget(csv_);
        fl->addWidget(head_);
        scroll_ = new QScrollArea(frame_);
        scroll_->setObjectName(QStringLiteral("ctbTableScroll"));
        scroll_->setFrameShape(QFrame::NoFrame);
        scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll_->setWidgetResizable(false);
        grid_ = new CtbGrid(scroll_);
        scroll_->setWidget(grid_);
        fl->addWidget(scroll_);
        vl->addWidget(frame_);
        QObject::connect(csv_, &QToolButton::clicked, this, [this]() {
            al::ArtefactTable t;
            t.columns = item_->block.header;
            t.rows = item_->block.rows;
            ctb_copy(csv_, al::table_to_csv(t));
        });
        QObject::connect(more_, &QToolButton::clicked, this, [this]() {
            item_->show_all = !item_->show_all;
            sync();
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }

    void sync() override {
        const md::Block& b = item_->block;
        const int rows = static_cast<int>(b.rows.size());
        const int shown = item_->show_all ? std::min(rows, kCtbTableRowsAll) : std::min(rows, kCtbTableRows);
        cap_->setText(ctb_tr("TABLE  ·  %1 ROWS  ·  %2 COLUMNS").arg(QLocale().toString(rows)).arg(b.header.size()));
        more_->setVisible(rows > kCtbTableRows);
        more_->setText(item_->show_all ? ctb_tr("SHOW LESS") : ctb_tr("SHOW ALL %1 ROWS").arg(QLocale().toString(rows)));
        if (item_->show_all && rows > kCtbTableRowsAll)
            more_->setToolTip(ctb_tr("The first %1 rows are drawn here; COPY AS CSV has every row.")
                                  .arg(QLocale().toString(kCtbTableRowsAll)));
        grid_->set_table(&item_->block, shown);
        frame_->setAccessibleName(ctb_tr("Table, %1 rows, %2 columns: %3")
                                      .arg(rows)
                                      .arg(b.header.size())
                                      .arg(b.header.join(QStringLiteral(", "))));
    }

    void restyle() override {
        grid_->fit();
        grid_->update();
        sync();
    }

    int height_for(int width) override {
        ensurePolished();
        const bool hbar = grid_->content_width() > width - 2;
        return 2 + head_->sizeHint().height() + grid_->height() +
               (hbar ? scroll_->horizontalScrollBar()->sizeHint().height() : 0) + 2 + ctb_gap();
    }

  private:
    QFrame* frame_ = nullptr;
    QWidget* head_ = nullptr;
    QLabel* cap_ = nullptr;
    QToolButton* csv_ = nullptr;
    QToolButton* more_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    CtbGrid* grid_ = nullptr;
};

// ── Activity (tool call / delegation / check) ───────────────────────────────

class CtbActivity : public Block {
  public:
    CtbActivity(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 0, 0, 1);
        vl->setSpacing(0);
        row_ = new QPushButton(this);
        row_->setObjectName(QStringLiteral("ctbRow"));
        row_->setCursor(Qt::PointingHandCursor);
        row_->setFlat(true);
        auto* hl = new QHBoxLayout(row_);
        hl->setContentsMargins(6, 2, 6, 2);
        hl->setSpacing(8);
        glyph_ = ctb_label(row_, "ctbRowGlyph", false);
        name_ = ctb_label(row_, "ctbRowName", false);
        target_ = ctb_label(row_, "ctbRowTarget", false);
        meta_ = ctb_label(row_, "ctbRowMeta", false);
        state_ = ctb_label(row_, "ctbRowState", false);
        for (QLabel* l : {glyph_, name_, target_, meta_, state_}) {
            l->setWordWrap(false);
            l->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        target_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        hl->addWidget(glyph_);
        hl->addWidget(name_);
        hl->addWidget(target_, 1);
        hl->addWidget(meta_);
        hl->addWidget(state_);
        vl->addWidget(row_);
        QObject::connect(row_, &QPushButton::clicked, this, [this]() {
            item_->expanded = !item_->expanded;
            sync();
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }

    void sync() override {
        const QJsonObject& d = item_->data;
        const bool running = item_->state == 0;
        const bool ok = item_->state == 1;
        const char* tone = running ? "info" : ok ? "success" : "danger";
        glyph_->setText(running ? QStringLiteral("●") : ok ? QStringLiteral("✓") : QStringLiteral("✕"));
        ctb_set_tone(glyph_, tone);
        ctb_set_tone(state_, tone);
        name_->setText(item_->text.toUpper());
        target_full_ = item_->text2.simplified();
        elide_target();
        const qint64 ms = static_cast<qint64>(d.value(QStringLiteral("duration_ms")).toDouble(-1));
        meta_->setText(ms >= 0 ? rv::ActivityLog::format_duration_precise(ms) : QString());
        meta_->setVisible(ms >= 0);
        state_->setText(running ? ctb_tr("RUNNING") : ok ? ctb_tr("DONE") : ctb_tr("FAILED"));
        row_->setToolTip((item_->expanded ? ctb_tr("Hide the details") : ctb_tr("Show the arguments and the result")) +
                         (target_full_.isEmpty() ? QString() : QStringLiteral("\n") + target_full_.left(400)));
        row_->setAccessibleName(ctb_tr("%1 %2, %3").arg(item_->text, target_full_.left(120), state_->text()));
        rebuild_detail();
    }

    int height_for(int width) override { return ctb_layout_height(this, width); }

  protected:
    void resizeEvent(QResizeEvent* e) override {
        Block::resizeEvent(e);
        elide_target();
    }

  private:
    void elide_target() {
        const int w = std::max(20, target_->width());
        const QString shown = target_->fontMetrics().elidedText(target_full_, Qt::ElideRight, w);
        if (target_->text() != shown)
            target_->setText(shown);
    }

    QString args_pretty() const {
        const QString args = item_->data.value(QStringLiteral("args")).toString();
        const QJsonDocument doc = QJsonDocument::fromJson(args.toUtf8());
        if (!doc.isNull())
            return QString::fromUtf8(doc.toJson(QJsonDocument::Indented)).trimmed();
        return args;
    }

    QPlainTextEdit* text_box(QWidget* parent, const QString& text, int max_lines) {
        auto* t = new QPlainTextEdit(parent);
        t->setObjectName(QStringLiteral("ctbDetailText"));
        t->setReadOnly(true);
        t->setTabChangesFocus(true);
        t->setPlainText(text);
        t->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        const QFontMetrics fm(ui::fonts::font(-2));
        const int lines = std::clamp(static_cast<int>(text.count(QLatin1Char('\n'))) + 1, 1, max_lines);
        t->setFixedHeight(fm.lineSpacing() * lines + 12);
        return t;
    }

    void rebuild_detail() {
        if (detail_) {
            detail_->hide();
            detail_->deleteLater();
            detail_ = nullptr;
        }
        if (!item_->expanded)
            return;
        detail_ = new QFrame(this);
        detail_->setObjectName(QStringLiteral("ctbDetail"));
        auto* vl = new QVBoxLayout(detail_);
        vl->setContentsMargins(10, 4, 6, 6);
        vl->setSpacing(4);
        const QJsonObject& d = item_->data;
        const bool tool = d.value(QStringLiteral("kind")).toString() == QLatin1String("tool");
        // ARGUMENTS
        const QString args = tool ? args_pretty() : d.value(QStringLiteral("body")).toString();
        if (!args.trimmed().isEmpty()) {
            auto* cap_row = new QHBoxLayout;
            cap_row->setSpacing(4);
            auto* cap = ctb_label(detail_, "ctbCap", false);
            cap->setText(tool ? ctb_tr("ARGUMENTS") : ctb_tr("DETAILS"));
            auto* copy = ctb_mini(detail_, ctb_tr("COPY"), ctb_tr("Copy the arguments"));
            QObject::connect(copy, &QToolButton::clicked, copy, [copy, args]() { ctb_copy(copy, args); });
            cap_row->addWidget(cap);
            cap_row->addStretch(1);
            cap_row->addWidget(copy);
            vl->addLayout(cap_row);
            vl->addWidget(text_box(detail_, args, 8));
        }
        if (tool) {
            auto* cap_row = new QHBoxLayout;
            cap_row->setSpacing(4);
            auto* cap = ctb_label(detail_, "ctbCap", false);
            cap->setText(ctb_tr("RESULT"));
            cap_row->addWidget(cap);
            cap_row->addStretch(1);
            vl->addLayout(cap_row);
            if (item_->state == 0) {
                auto* l = ctb_label(detail_, "ctbNote");
                l->setText(ctb_tr("The tool is still running…"));
                vl->addWidget(l);
            } else if (!item_->result || !item_->result->ready) {
                auto* l = ctb_label(detail_, "ctbNote");
                l->setText(ctb_tr("Reading the result…"));
                vl->addWidget(l);
                if (host_ && host_->classify)
                    host_->classify(item_->id); // the worker re-syncs this item when done
            } else {
                add_result(vl, cap_row);
            }
        }
        static_cast<QVBoxLayout*>(layout())->addWidget(detail_);
        detail_->show();
    }

    void add_result(QVBoxLayout* vl, QHBoxLayout* cap_row) {
        const al::ToolResultView& view = item_->result->view;
        auto* copy = ctb_mini(detail_, ctb_tr("COPY"), ctb_tr("Copy the result (tables as CSV)"));
        const ItemPtr it = item_;
        QObject::connect(copy, &QToolButton::clicked, copy, [copy, it]() { ctb_copy(copy, result_text(*it)); });
        const bool can_save = item_->state == 1 && !item_->data.value(QStringLiteral("run_id")).toString().isEmpty();
        if (can_save) {
            const bool saved = item_->status == QLatin1String("saved");
            auto* save = ctb_mini(detail_, saved ? ctb_tr("SAVED") : ctb_tr("SAVE AS ARTEFACT"),
                                  ctb_tr("Keep this result in the run's FILES (table, JSON or text)"));
            save->setEnabled(!saved && item_->status != QLatin1String("saving"));
            QObject::connect(save, &QToolButton::clicked, this, [this]() {
                if (host_ && host_->act)
                    host_->act(Action{QStringLiteral("save_artefact"), item_->id, item_->turn, {}});
            });
            cap_row->addWidget(save);
        }
        const auto add_more = [this, cap_row](bool needed) {
            if (!needed)
                return;
            auto* more = ctb_mini(detail_, item_->show_all ? ctb_tr("SHOW LESS") : ctb_tr("SHOW MORE"), QString());
            QObject::connect(more, &QToolButton::clicked, this, [this]() {
                item_->show_all = !item_->show_all;
                sync();
                if (host_ && host_->relayout)
                    host_->relayout(item_->id);
            });
            cap_row->addWidget(more);
        };
        switch (view.shape) {
            case al::ResultShape::Table: {
                auto* table = new QTableView(detail_);
                table->setObjectName(QStringLiteral("ctbResultTable"));
                auto* model = new ArtefactTableModel(table);
                model->set_table(view.table, item_->show_all ? kCtbResultRowsAll : kCtbResultRows);
                table->setModel(model);
                table->setSortingEnabled(true);
                table->sortByColumn(-1, Qt::AscendingOrder);
                table->setAlternatingRowColors(true);
                table->setSelectionBehavior(QAbstractItemView::SelectRows);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->verticalHeader()->setVisible(false);
                table->horizontalHeader()->setStretchLastSection(true);
                table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
                table->setWordWrap(false);
                const QFontMetrics fm(ui::fonts::font(-2));
                const int row_h = fm.height() + 6;
                table->verticalHeader()->setDefaultSectionSize(row_h);
                for (int c = 0; c < model->columnCount(); ++c)
                    table->setColumnWidth(
                        c, std::clamp(fm.horizontalAdvance(model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()) + 28,
                                      fm.averageCharWidth() * 8, fm.averageCharWidth() * 28));
                const int rows = std::min(model->rowCount(), item_->show_all ? 30 : 12);
                table->setFixedHeight(row_h * (rows + 1) + 6);
                vl->addWidget(table);
                auto* note = ctb_label(detail_, "ctbNote");
                note->setText(model->is_cut() ? ctb_tr("Showing %1 of %2 rows — COPY or SAVE AS ARTEFACT keeps them all.")
                                                    .arg(QLocale().toString(model->rowCount()),
                                                         QLocale().toString(model->total_rows()))
                                              : ctb_tr("%n row(s) — click a column to sort", nullptr, model->rowCount()));
                vl->addWidget(note);
                add_more(model->total_rows() > kCtbResultRows);
                break;
            }
            case al::ResultShape::KeyValue: {
                auto* host = new QWidget(detail_);
                auto* grid = new QGridLayout(host);
                grid->setContentsMargins(0, 0, 0, 0);
                grid->setHorizontalSpacing(12);
                grid->setVerticalSpacing(2);
                const int cap = item_->show_all ? static_cast<int>(view.fields.size()) : kCtbResultFields;
                int row = 0;
                for (const auto& f : view.fields) {
                    if (row >= cap)
                        break;
                    auto* k = ctb_label(host, "ctbKey", false);
                    k->setText(f.first);
                    k->setWordWrap(false);
                    auto* v = ctb_label(host, "ctbValue");
                    v->setText(f.second);
                    grid->addWidget(k, row, 0, Qt::AlignTop);
                    grid->addWidget(v, row, 1);
                    ++row;
                }
                grid->setColumnStretch(1, 1);
                vl->addWidget(host);
                add_more(view.fields.size() > kCtbResultFields);
                break;
            }
            case al::ResultShape::Error: {
                auto* e = ctb_label(detail_, "ctbErrorText");
                e->setText(view.text);
                vl->addWidget(e);
                break;
            }
            case al::ResultShape::Text: {
                vl->addWidget(text_box(detail_, view.text, item_->show_all ? 40 : 10));
                add_more(view.text.count(QLatin1Char('\n')) > 10);
                if (item_->data.value(QStringLiteral("preview_only")).toBool()) {
                    auto* n = ctb_label(detail_, "ctbNote");
                    n->setText(ctb_tr("The server sends a shortened preview of the tool's output."));
                    vl->addWidget(n);
                }
                break;
            }
            case al::ResultShape::Empty: {
                auto* n = ctb_label(detail_, "ctbNote");
                n->setText(ctb_tr("The tool returned no output."));
                vl->addWidget(n);
                break;
            }
        }
        cap_row->addWidget(copy);
    }

    QPushButton* row_ = nullptr;
    QLabel* glyph_ = nullptr;
    QLabel* name_ = nullptr;
    QLabel* target_ = nullptr;
    QLabel* meta_ = nullptr;
    QLabel* state_ = nullptr;
    QFrame* detail_ = nullptr;
    QString target_full_;
};

// ── Fold rows: activity group, reasoning ────────────────────────────────────

class CtbFold : public Block {
  public:
    CtbFold(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 0, 0, 2);
        vl->setSpacing(2);
        button_ = ctb_button(this, QString(), "ctbFold");
        button_->setFlat(true);
        vl->addWidget(button_);
        if (item_->kind == ItemKind::Reasoning) {
            text_ = ctb_label(this, "ctbReason");
            vl->addWidget(text_);
        }
        QObject::connect(button_, &QPushButton::clicked, this, [this]() {
            if (item_->kind == ItemKind::ActivityGroup) {
                if (host_ && host_->toggle_group)
                    host_->toggle_group(item_->id);
                sync();
                return;
            }
            item_->expanded = !item_->expanded;
            sync();
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }

    void sync() override {
        const QString mark = item_->expanded ? QStringLiteral("▾  ") : QStringLiteral("▸  ");
        if (item_->kind == ItemKind::Reasoning) {
            button_->setText(mark + ctb_tr("REASONING  ·  %1 CHARACTERS").arg(QLocale().toString(item_->text.size())));
            button_->setToolTip(item_->expanded ? ctb_tr("Hide the model's reasoning") : ctb_tr("Show the model's reasoning"));
            text_->setText(item_->text);
            text_->setVisible(item_->expanded);
        } else {
            button_->setText(mark + item_->text);
            button_->setToolTip(item_->expanded ? ctb_tr("Fold the activity rows") : ctb_tr("Show every activity row"));
        }
        button_->setAccessibleName(button_->text().mid(3));
    }

    int height_for(int width) override { return ctb_layout_height(this, width); }

  private:
    QPushButton* button_ = nullptr;
    QLabel* text_ = nullptr;
};

// ── Plan strip ──────────────────────────────────────────────────────────────

/// Painted progress: one segment per step, toned by its state.
class CtbSegments : public QWidget {
  public:
    explicit CtbSegments(QWidget* parent) : QWidget(parent) {}
    void set_states(const QStringList& states) {
        states_ = states;
        update();
    }
    QSize sizeHint() const override { return QSize(120, 6); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const int n = std::max(1, static_cast<int>(states_.size()));
        const int gap = 2;
        const double w = (width() - gap * (n - 1)) / static_cast<double>(n);
        for (int i = 0; i < states_.size(); ++i) {
            const QString& s = states_.at(i);
            QColor c(ui::colors::BORDER_MED());
            if (s == QLatin1String("done") || s == QLatin1String("skipped"))
                c = QColor(ui::colors::SUCCESS());
            else if (s == QLatin1String("running"))
                c = QColor(ui::colors::AMBER());
            else if (s == QLatin1String("failed"))
                c = QColor(ui::colors::DANGER());
            p.fillRect(QRectF(i * (w + gap), 0, w, height()), c);
        }
    }

  private:
    QStringList states_;
};

class CtbPlan : public Block {
  public:
    CtbPlan(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(3);
        button_ = ctb_button(this, QString(), "ctbFold");
        button_->setFlat(true);
        bar_ = new CtbSegments(this);
        steps_ = ctb_label(this, "ctbSteps");
        vl->addWidget(button_);
        vl->addWidget(bar_);
        vl->addWidget(steps_);
        QObject::connect(button_, &QPushButton::clicked, this, [this]() {
            item_->expanded = !item_->expanded;
            sync();
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }

    void sync() override {
        const QJsonArray steps = item_->data.value(QStringLiteral("steps")).toArray();
        QStringList states;
        QStringList lines;
        int done = 0;
        for (int i = 0; i < steps.size(); ++i) {
            const QJsonObject s = steps.at(i).toObject();
            const QString st = s.value(QStringLiteral("status")).toString();
            states << st;
            if (st == QLatin1String("done") || st == QLatin1String("skipped"))
                ++done;
            const QString mark = st == QLatin1String("done")      ? QStringLiteral("✓")
                                 : st == QLatin1String("running") ? QStringLiteral("●")
                                 : st == QLatin1String("failed")  ? QStringLiteral("✕")
                                 : st == QLatin1String("skipped") ? QStringLiteral("–")
                                                                  : QStringLiteral("○");
            QString line = QStringLiteral("%1  %2. %3").arg(mark).arg(i + 1).arg(s.value(QStringLiteral("title")).toString());
            const qint64 ms = static_cast<qint64>(s.value(QStringLiteral("ms")).toDouble(-1));
            if (ms >= 0)
                line += QStringLiteral("  ·  ") + rv::format_duration(ms);
            const QString deliverable = s.value(QStringLiteral("deliverable")).toString().trimmed();
            if (!deliverable.isEmpty())
                line += QStringLiteral("\n      ") + deliverable;
            lines << line;
        }
        const QString title = item_->data.value(QStringLiteral("title")).toString();
        const QString mark = item_->expanded ? QStringLiteral("▾  ") : QStringLiteral("▸  ");
        button_->setText(mark + (title.isEmpty() ? ctb_tr("PLAN") : title.toUpper()) + QStringLiteral("  ·  ") +
                         ctb_tr("%1 / %2 STEPS").arg(done).arg(steps.size()));
        button_->setToolTip(item_->expanded ? ctb_tr("Hide the steps") : ctb_tr("Show the steps"));
        button_->setAccessibleName(button_->text().mid(3));
        bar_->set_states(states);
        bar_->setFixedHeight(std::max(4, ctb_metrics().lh3 / 3));
        steps_->setText(lines.join(QLatin1Char('\n')));
        steps_->setVisible(item_->expanded);
    }

    int height_for(int width) override { return ctb_layout_height(this, width); }

  private:
    QPushButton* button_ = nullptr;
    CtbSegments* bar_ = nullptr;
    QLabel* steps_ = nullptr;
};

// ── Cards: question, approval, credits; errors; follow-ups; sources ─────────

class CtbCard : public Block {
  public:
    CtbCard(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(0);
        card_ = new QFrame(this);
        card_->setObjectName(QStringLiteral("ctbCard"));
        body_ = new QVBoxLayout(card_);
        body_->setContentsMargins(12, 8, 12, 8);
        body_->setSpacing(5);
        title_ = ctb_label(card_, "ctbCardTitle", false);
        body_->addWidget(title_);
        vl->addWidget(card_);
    }
    int height_for(int width) override { return ctb_layout_height(this, width); }

  protected:
    QFrame* card_ = nullptr;
    QVBoxLayout* body_ = nullptr;
    QLabel* title_ = nullptr;
};

class CtbQuestion : public CtbCard {
  public:
    CtbQuestion(const ItemPtr& item, const BlockHost* host, QWidget* parent) : CtbCard(item, host, parent) {
        ctb_set_tone(card_, "warning");
        ctb_set_tone(title_, "warning");
        text_ = ctb_label(card_, "ctbCardText");
        hint_ = ctb_label(card_, "ctbCardHint");
        body_->addWidget(text_);
        body_->addWidget(hint_);
        sync();
    }
    void sync() override {
        title_->setText(ctb_tr("THE AGENT IS WAITING FOR YOUR INPUT"));
        text_->setText(item_->text.isEmpty() ? ctb_tr("The agent needs more information to continue.") : item_->text);
        hint_->setText(item_->text2);
        hint_->setVisible(!item_->text2.isEmpty());
    }

  private:
    QLabel* text_ = nullptr;
    QLabel* hint_ = nullptr;
};

class CtbApproval : public CtbCard {
  public:
    CtbApproval(const ItemPtr& item, const BlockHost* host, QWidget* parent) : CtbCard(item, host, parent) {
        what_ = ctb_label(card_, "ctbCardText");
        extra_ = ctb_label(card_, "ctbCardHint");
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        approve_ = ctb_button(card_, ctb_tr("APPROVE"), "ctbPrimary");
        // agents: approving executes a trade on the portfolio ledger — never without the user's scrutiny
        approve_->setProperty("fincept_agent_risk", "order");
        deny_ = ctb_button(card_, ctb_tr("DENY"), "ctbSecondary");
        row->addWidget(approve_);
        row->addWidget(deny_);
        row->addStretch(1);
        hint_ = ctb_label(card_, "ctbCardHint");
        body_->addWidget(what_);
        body_->addWidget(extra_);
        body_->addLayout(row);
        body_->addWidget(hint_);
        QObject::connect(approve_, &QPushButton::clicked, this, [this]() { act("approve"); });
        QObject::connect(deny_, &QPushButton::clicked, this, [this]() { act("deny"); });
        // The order is shown through the shared formatter, and again after a format change.
        const QPointer<CtbApproval> self(this);
        ui::formatting::on_format_changed(what_, [self]() {
            if (self)
                self->render_what();
        });
        sync();
    }

    void sync() override {
        const auto state = static_cast<ApprovalState>(item_->state);
        const bool pending = state == ApprovalState::Pending || state == ApprovalState::Busy;
        title_->setText(pending ? ctb_tr("TRADE APPROVAL REQUIRED") : ctb_tr("TRADE PROPOSAL"));
        ctb_set_tone(card_, pending ? "warning" : "neutral");
        ctb_set_tone(title_, pending ? "warning" : "neutral");
        render_what();
        const QJsonObject& p = item_->data;
        QStringList extra;
        const QString portfolio = p.value(QStringLiteral("portfolio_name")).toString().trimmed();
        const QString notes = p.value(QStringLiteral("notes")).toString().trimmed();
        if (!portfolio.isEmpty())
            extra << ctb_tr("Portfolio: %1").arg(portfolio);
        if (!notes.isEmpty())
            extra << ctb_tr("Notes: %1").arg(notes);
        extra_->setText(extra.join(QLatin1Char('\n')));
        extra_->setVisible(!extra.isEmpty());
        const bool show_buttons = state == ApprovalState::Pending || state == ApprovalState::Busy;
        approve_->setVisible(show_buttons);
        deny_->setVisible(show_buttons);
        const bool enabled = state == ApprovalState::Pending && host_ && host_->approvals_enabled && host_->approvals_enabled();
        approve_->setEnabled(enabled);
        deny_->setEnabled(enabled);
        if (state == ApprovalState::Pending && !enabled) {
            approve_->setToolTip(ctb_tr("Available when the agent's turn has ended"));
            deny_->setToolTip(approve_->toolTip());
        } else {
            approve_->setToolTip(ctb_tr("Execute this trade on the portfolio"));
            deny_->setToolTip(ctb_tr("Do not execute this trade"));
        }
        hint_->setText(item_->status);
        hint_->setVisible(!item_->status.isEmpty());
    }

  private:
    void act(const char* kind) {
        if (host_ && host_->act)
            host_->act(Action{QString::fromLatin1(kind), item_->id, item_->turn, item_->ref});
    }

    void render_what() {
        namespace fmt = fincept::ui::formatting;
        const QJsonObject& p = item_->data;
        const QString action = p.value(QStringLiteral("action")).toString().trimmed().toUpper();
        const QString symbol = p.value(QStringLiteral("symbol")).toString().trimmed();
        const double quantity = p.value(QStringLiteral("quantity")).toDouble();
        const double price = p.value(QStringLiteral("price")).toDouble();
        const double total = p.value(QStringLiteral("total_value")).toDouble(quantity * price);
        const QString ccy = p.value(QStringLiteral("currency")).toString().trimmed();
        const QString px = ccy.isEmpty() ? fmt::price(price) : fmt::price_with_symbol(price, fmt::InstrumentFmt{ccy});
        const QString tot = ccy.isEmpty() ? fmt::number(total, 2) : fmt::money(total, ccy);
        what_->setText(ctb_tr("%1 %2 %3 at %4 (total %5)")
                           .arg(action, QString::number(quantity, 'g', 12), symbol, px, tot));
        approve_->setAccessibleName(ctb_tr("Approve: %1").arg(what_->text()));
        deny_->setAccessibleName(ctb_tr("Deny: %1").arg(what_->text()));
    }

    QLabel* what_ = nullptr;
    QLabel* extra_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* approve_ = nullptr;
    QPushButton* deny_ = nullptr;
};

class CtbCredits : public CtbCard {
  public:
    CtbCredits(const ItemPtr& item, const BlockHost* host, QWidget* parent) : CtbCard(item, host, parent) {
        ctb_set_tone(card_, "accent");
        ctb_set_tone(title_, "accent");
        title_->setText(ctb_tr("OUT OF CREDITS"));
        auto* body = ctb_label(card_, "ctbCardText");
        body->setText(ctb_tr("Pro adds a monthly credit allowance, higher limits and priority support."));
        body_->addWidget(body);
        auto* row = new QHBoxLayout;
        // The Fincept plan price is a USD price: always labelled USD, never converted.
        auto* pro = ctb_button(card_, ctb_tr("GET PRO — %1/MO").arg(fincept::ui::formatting::plan_price(10)), "ctbPrimary");
        pro->setProperty("fincept_agent_risk", "spend"); // agents: opens a paid plan purchase
        QObject::connect(pro, &QPushButton::clicked, this, [this]() {
            if (host_ && host_->act)
                host_->act(Action{QStringLiteral("get_pro"), item_->id, item_->turn, {}});
        });
        row->addWidget(pro);
        row->addStretch(1);
        body_->addLayout(row);
        auto* ent = ui::enterprise::make_mention_label(card_);
        ui::enterprise::set_mention_text(ent, ctb_tr("Teams and firms: Fincept Terminal Enterprise includes AI credits."),
                                         ctb_tr("See Enterprise ↗"),
                                         ui::enterprise::url_with_ref(ui::enterprise::site_url(), QStringLiteral("chat")));
        body_->addWidget(ent);
    }
};

class CtbError : public Block {
  public:
    CtbError(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(0);
        frame_ = new QFrame(this);
        frame_->setObjectName(QStringLiteral("ctbError"));
        auto* hl = new QHBoxLayout(frame_);
        hl->setContentsMargins(12, 6, 8, 6);
        hl->setSpacing(10);
        text_ = ctb_label(frame_, "ctbErrorText");
        button_ = ctb_button(frame_, QString(), "ctbSecondary");
        button_->setProperty("fincept_agent_risk", "spend"); // agents: re-sends a billed model request
        hl->addWidget(text_, 1);
        hl->addWidget(button_, 0, Qt::AlignTop);
        vl->addWidget(frame_);
        QObject::connect(button_, &QPushButton::clicked, this, [this]() {
            if (host_ && host_->act)
                host_->act(Action{QStringLiteral("error_action"), item_->id, item_->turn, item_->action});
        });
        sync();
    }
    void sync() override {
        text_->setText(QStringLiteral("!  ") + item_->text);
        button_->setText(item_->text2.isEmpty() ? ctb_tr("RETRY") : item_->text2);
        button_->setAccessibleName(button_->text());
        button_->setVisible(!item_->action.isEmpty());
        button_->setEnabled(item_->state == 0);
    }
    int height_for(int width) override { return ctb_layout_height(this, width); }

  private:
    QFrame* frame_ = nullptr;
    QLabel* text_ = nullptr;
    QPushButton* button_ = nullptr;
};

class CtbFollowUps : public Block {
  public:
    CtbFollowUps(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* flow = new ui::FlowLayout(this);
        flow->setContentsMargins(0, 2, 0, ctb_gap());
        auto* cap = ctb_label(this, "ctbCap", false);
        cap->setWordWrap(false);
        cap->setText(ctb_tr("FOLLOW UP"));
        flow->addWidget(cap);
        for (int i = 0; i < item_->list.size() && i < 4; ++i) {
            const QString q = item_->list.at(i);
            const QString shown = q.simplified().size() > 70 ? q.simplified().left(69) + QChar(0x2026) : q.simplified();
            auto* b = ctb_button(this, shown, "ctbChip");
            b->setToolTip(ctb_tr("Put this question in the message box (nothing is sent): %1").arg(q));
            QObject::connect(b, &QPushButton::clicked, this, [this, q]() {
                if (host_ && host_->act)
                    host_->act(Action{QStringLiteral("follow_up"), item_->id, item_->turn, q});
            });
            flow->addWidget(b);
        }
    }
    int height_for(int width) override { return ctb_layout_height(this, width); }
};

class CtbSources : public Block {
  public:
    CtbSources(const ItemPtr& item, const BlockHost* host, QWidget* parent) : Block(item, host, parent) {
        auto* vl = new QVBoxLayout(this);
        vl->setContentsMargins(0, 2, 0, ctb_gap());
        vl->setSpacing(2);
        auto* top = new QHBoxLayout;
        cap_ = ctb_label(this, "ctbCap", false);
        more_ = ctb_mini(this, QString(), QString());
        top->addWidget(cap_);
        top->addStretch(1);
        top->addWidget(more_);
        vl->addLayout(top);
        list_ = ctb_rich(this, "ctbSources", host);
        vl->addWidget(list_);
        QObject::connect(more_, &QToolButton::clicked, this, [this]() {
            item_->show_all = !item_->show_all;
            sync();
            if (host_ && host_->relayout)
                host_->relayout(item_->id);
        });
        sync();
    }
    void sync() override {
        const QJsonArray links = item_->data.value(QStringLiteral("links")).toArray();
        cap_->setText(ctb_tr("SOURCES  ·  %1").arg(links.size()));
        const int cap = item_->show_all ? static_cast<int>(links.size()) : kCtbSources;
        QStringList rows;
        for (int i = 0; i < links.size() && i < cap; ++i) {
            const QJsonObject l = links.at(i).toObject();
            const QString url = l.value(QStringLiteral("url")).toString();
            const QUrl u(url);
            const QString host = u.host().isEmpty() ? url : u.host();
            const QString text = l.value(QStringLiteral("text")).toString().simplified();
            rows << QStringLiteral("[%1]&nbsp;<a href=\"%2\">%3</a>%4")
                        .arg(QString::number(i + 1), md::escape_html(url, false), md::escape_html(host, false),
                             text.isEmpty() || text == url ? QString()
                                                           : QStringLiteral(" — ") + md::escape_html(text.left(120), false));
        }
        ctb_set_rich(list_, rows.join(QStringLiteral("<br/>")));
        more_->setVisible(links.size() > kCtbSources);
        more_->setText(item_->show_all ? ctb_tr("SHOW LESS") : ctb_tr("SHOW ALL %1").arg(links.size()));
    }
    int height_for(int width) override { return ctb_layout_height(this, width); }

  private:
    QLabel* cap_ = nullptr;
    QToolButton* more_ = nullptr;
    QLabel* list_ = nullptr;
};

} // namespace transcript_blocks_detail

using namespace transcript_blocks_detail;

// ── Block ────────────────────────────────────────────────────────────────────

Block::Block(const ItemPtr& item, const BlockHost* host, QWidget* parent) : QWidget(parent), item_(item), host_(host) {
    setObjectName(QStringLiteral("ctbBlock"));
}

int Block::height_for(int width) {
    return ctb_layout_height(this, width);
}

Block* create_block(const ItemPtr& item, const BlockHost* host, QWidget* parent) {
    switch (item->kind) {
        case ItemKind::Header:
            return new CtbHeader(item, host, parent);
        case ItemKind::UserText:
            return new CtbUser(item, host, parent);
        case ItemKind::Markdown:
            switch (item->block.kind) {
                case md::BlockKind::Code:
                    return new CtbCode(item, host, parent);
                case md::BlockKind::Table:
                    return new CtbTable(item, host, parent);
                case md::BlockKind::Rule:
                    return new CtbRule(item, host, parent);
                default:
                    return new CtbRich(item, host, parent);
            }
        case ItemKind::Activity:
            return new CtbActivity(item, host, parent);
        case ItemKind::ActivityGroup:
        case ItemKind::Reasoning:
            return new CtbFold(item, host, parent);
        case ItemKind::Plan:
            return new CtbPlan(item, host, parent);
        case ItemKind::Question:
            return new CtbQuestion(item, host, parent);
        case ItemKind::Approval:
            return new CtbApproval(item, host, parent);
        case ItemKind::Error:
            return new CtbError(item, host, parent);
        case ItemKind::FollowUps:
            return new CtbFollowUps(item, host, parent);
        case ItemKind::CreditsCta:
            return new CtbCredits(item, host, parent);
        case ItemKind::Sources:
            return new CtbSources(item, host, parent);
        case ItemKind::Note:
            break;
    }
    return new CtbRich(item, host, parent);
}

void reset_metrics() {
    ctb_metrics().valid = false;
}

int estimate_height(Item& it, int width) {
    if (it.hidden)
        return 0;
    const CtbMetrics& m = ctb_metrics();
    const int gap = ctb_gap();
    const int w = std::max(80, width);
    switch (it.kind) {
        case ItemKind::Header:
            return (it.turn <= 0 ? 2 : std::max(10, m.lh0)) + m.lh3 + 8;
        case ItemKind::UserText:
            ctb_text_stats(it, it.text);
            return ctb_lines(it.est_chars, it.est_breaks, m.avg0, w - 24) * m.lh0 + 14 + (it.text2.isEmpty() ? 0 : m.lh3) + gap;
        case ItemKind::Markdown: {
            const md::Block& b = it.block;
            switch (b.kind) {
                case md::BlockKind::Heading:
                    return (b.level <= 1 ? m.lh_h1 : m.lh_h2) + gap * 2;
                case md::BlockKind::Rule:
                    return gap * 2 + 1;
                case md::BlockKind::Code: {
                    const int lines = static_cast<int>(b.plain.count(QLatin1Char('\n'))) + 1;
                    return std::min(lines, it.show_all ? lines : kCtbCodeLines) * m.lh1 + m.lh3 + 24 + gap;
                }
                case md::BlockKind::Table: {
                    const int rows = std::min(static_cast<int>(b.rows.size()), it.show_all ? kCtbTableRowsAll : kCtbTableRows);
                    return (rows + 1) * (m.lh1 + 6) + m.lh3 + 14 + gap;
                }
                default:
                    ctb_text_stats(it, b.plain);
                    return ctb_lines(it.est_chars, it.est_breaks, m.avg0, w) * m.lh0 + gap;
            }
        }
        case ItemKind::Activity:
            return m.lh2 + 8 + (it.expanded ? m.lh2 * 12 : 0);
        case ItemKind::ActivityGroup:
            return m.lh2 + 10;
        case ItemKind::Reasoning:
            ctb_text_stats(it, it.text);
            return m.lh2 + 10 + (it.expanded ? ctb_lines(it.est_chars, it.est_breaks, m.avg2, w) * m.lh2 : 0);
        case ItemKind::Plan: {
            const int steps = static_cast<int>(it.data.value(QStringLiteral("steps")).toArray().size());
            return m.lh2 + 16 + (it.expanded ? steps * m.lh2 * 2 : 0) + gap;
        }
        case ItemKind::Question:
            ctb_text_stats(it, it.text);
            return m.lh3 + ctb_lines(it.est_chars, it.est_breaks, m.avg1, w - 24) * m.lh1 + m.lh3 * 2 + 24 + gap;
        case ItemKind::Approval:
            return m.lh3 + m.lh1 * 2 + m.lh0 + m.lh3 * 2 + 40 + gap;
        case ItemKind::Note:
        case ItemKind::Error:
            ctb_text_stats(it, it.text);
            return ctb_lines(it.est_chars, it.est_breaks, m.avg2, w - 120) * m.lh2 + (it.kind == ItemKind::Error ? 14 : 0) + gap;
        case ItemKind::FollowUps:
            return m.lh2 + 14 + gap;
        case ItemKind::CreditsCta:
            return m.lh3 + m.lh1 * 2 + m.lh0 + m.lh2 + 50 + gap;
        case ItemKind::Sources: {
            const int n = static_cast<int>(it.data.value(QStringLiteral("links")).toArray().size());
            return m.lh3 + std::min(n, it.show_all ? n : kCtbSources) * m.lh1 + gap + 4;
        }
    }
    return m.lh0 + gap;
}

QString result_text(const Item& item) {
    if (!item.result || !item.result->ready)
        return item.data.value(QStringLiteral("preview")).toString();
    const al::ToolResultView& v = item.result->view;
    switch (v.shape) {
        case al::ResultShape::Table:
            return al::table_to_csv(v.table);
        case al::ResultShape::KeyValue: {
            QStringList lines;
            for (const auto& f : v.fields)
                lines << f.first + QStringLiteral(": ") + f.second;
            return lines.join(QLatin1Char('\n'));
        }
        case al::ResultShape::Text:
        case al::ResultShape::Error:
            return v.text;
        case al::ResultShape::Empty:
            break;
    }
    return {};
}

QString rich_css() {
    return QStringLiteral("a{color:%1;text-decoration:none;}"
                          "a.tk{color:%2;font-weight:600;}"
                          "code{font-family:%3;color:%4;background-color:%5;}"
                          "pre{font-family:%3;color:%6;background-color:%7;}"
                          "blockquote{color:%8;}"
                          "th{color:%8;font-weight:700;padding:2px 8px;border-bottom:1px solid %9;}"
                          "td{padding:2px 8px;}"
                          "h1,h2{color:%2;font-weight:700;}"
                          "h3,h4,h5,h6{color:%6;font-weight:700;}"
                          "li{margin-top:2px;}")
        .arg(ui::colors::CYAN(), ui::colors::AMBER(), QString(ui::fonts::DATA_FAMILY), ui::colors::CYAN(),
             ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
             ui::colors::BORDER_DIM());
}

QString transcript_sheet() {
    // One sheet for every block part (object-name selectors), set on the view before
    // any block exists; the central re-style pass keeps it in step with the theme.
    return QStringLiteral(
               "QAbstractScrollArea#chatTranscript{background:%1;border:none;}"
               "QWidget#chatTranscriptViewport{background:%1;}"
               "QWidget#ctbBlock,QWidget#ctbHeader,QWidget#ctbActs{background:transparent;}"
               "QLabel{background:transparent;border:none;font-family:%2;}"
               "QLabel#ctbWho{color:%3;font-size:%4px;font-weight:700;letter-spacing:1px;}"
               "QLabel#ctbWho[role=\"agent\"]{color:%5;}"
               "QLabel#ctbMeta{color:%6;font-size:%4px;}"
               "QToolButton#ctbAct,QToolButton#ctbMini{background:transparent;border:1px solid transparent;color:%7;"
               "font-size:%8px;font-weight:700;letter-spacing:0.5px;padding:1px 5px;}"
               "QToolButton#ctbAct[hot=\"true\"]{color:%3;}"
               "QToolButton#ctbAct:hover,QToolButton#ctbMini:hover{color:%5;border-color:%9;}"
               "QToolButton#ctbAct:focus,QToolButton#ctbMini:focus{border-color:%5;color:%10;}"
               "QToolButton#ctbMini{color:%3;}"
               "QToolButton#ctbMini:disabled{color:%7;}"
               "QFrame#ctbUser{background:%11;border:none;border-left:2px solid %12;}"
               "QLabel#ctbUserText{color:%10;font-size:%13px;}"
               "QLabel#ctbSentWith{color:%6;font-size:%4px;}"
               "QLabel#ctbText{color:%10;font-size:%13px;}"
               "QLabel#ctbH1{color:%5;font-size:%14px;font-weight:700;}"
               "QLabel#ctbH2{color:%5;font-size:%15px;font-weight:700;}"
               "QLabel#ctbH3{color:%10;font-size:%13px;font-weight:700;}"
               "QLabel#ctbQuote{color:%3;font-size:%13px;border-left:2px solid %9;padding-left:10px;}"
               "QLabel#ctbNote{color:%6;font-size:%16px;font-style:italic;}"
               "QLabel#ctbCap{color:%6;font-size:%8px;font-weight:700;letter-spacing:1px;}"
               "QFrame#ctbCode,QFrame#ctbTable{background:%11;border:1px solid %17;}"
               "QWidget#ctbCodeHead{background:%11;border-bottom:1px solid %17;}"
               "QPlainTextEdit#ctbCodeBody{background:%11;color:%10;border:none;font-family:%2;font-size:%18px;"
               "selection-background-color:%19;}"
               "QScrollArea#ctbTableScroll,QWidget#ctbGrid{background:%11;}"
               "QPushButton#ctbRow{background:transparent;border:none;border-left:2px solid %17;text-align:left;"
               "padding:0;}"
               "QPushButton#ctbRow:hover{background:%20;}"
               "QPushButton#ctbRow:focus{border-left:2px solid %5;}"
               "QLabel#ctbRowGlyph{color:%21;font-size:%16px;}"
               "QLabel#ctbRowGlyph[tone=\"success\"],QLabel#ctbRowState[tone=\"success\"]{color:%22;}"
               "QLabel#ctbRowGlyph[tone=\"danger\"],QLabel#ctbRowState[tone=\"danger\"]{color:%23;}"
               "QLabel#ctbRowName{color:%10;font-size:%16px;font-weight:700;letter-spacing:0.5px;}"
               "QLabel#ctbRowTarget{color:%3;font-size:%16px;}"
               "QLabel#ctbRowMeta{color:%6;font-size:%4px;}"
               "QLabel#ctbRowState{color:%21;font-size:%8px;font-weight:700;letter-spacing:1px;}"
               "QFrame#ctbDetail{background:%11;border:none;border-left:2px solid %17;}"
               "QPlainTextEdit#ctbDetailText{background:%1;color:%10;border:1px solid %17;font-family:%2;"
               "font-size:%16px;}"
               "QTableView#ctbResultTable{background:%1;alternate-background-color:%24;color:%10;gridline-color:%17;"
               "border:1px solid %17;font-size:%16px;}"
               "QTableView#ctbResultTable QHeaderView::section{background:%25;color:%3;border:none;"
               "border-bottom:1px solid %17;padding:2px 6px;font-size:%4px;font-weight:700;}"
               "QLabel#ctbKey{color:%6;font-size:%16px;}"
               "QLabel#ctbValue{color:%10;font-size:%16px;}"
               "QPushButton#ctbFold{background:transparent;border:none;color:%3;text-align:left;padding:3px 2px;"
               "font-size:%4px;font-weight:700;letter-spacing:1px;}"
               "QPushButton#ctbFold:hover{color:%10;}"
               "QPushButton#ctbFold:focus{color:%5;}"
               "QLabel#ctbReason{color:%6;font-size:%16px;font-style:italic;padding-left:14px;}"
               "QLabel#ctbSteps{color:%3;font-size:%16px;padding-left:14px;}"
               "QFrame#ctbCard{background:%11;border:1px solid %17;border-left:2px solid %26;}"
               "QFrame#ctbCard[tone=\"warning\"]{border-left:2px solid %27;}"
               "QFrame#ctbCard[tone=\"accent\"]{border-left:2px solid %5;}"
               "QLabel#ctbCardTitle{color:%3;font-size:%4px;font-weight:700;letter-spacing:1px;}"
               "QLabel#ctbCardTitle[tone=\"warning\"]{color:%27;}"
               "QLabel#ctbCardTitle[tone=\"accent\"]{color:%5;}"
               "QLabel#ctbCardText{color:%10;font-size:%18px;}"
               "QLabel#ctbCardHint{color:%6;font-size:%4px;}"
               "QPushButton#ctbPrimary{background:%5;color:%28;border:1px solid %5;padding:4px 14px;font-size:%16px;"
               "font-weight:700;}"
               "QPushButton#ctbPrimary:hover{background:%29;border-color:%29;}"
               "QPushButton#ctbPrimary:focus{border-color:%10;}"
               "QPushButton#ctbPrimary:disabled,QPushButton#ctbSecondary:disabled{background:transparent;color:%7;"
               "border-color:%17;}"
               "QPushButton#ctbSecondary{background:%25;color:%10;border:1px solid %9;padding:4px 14px;font-size:%16px;"
               "font-weight:700;}"
               "QPushButton#ctbSecondary:hover,QPushButton#ctbSecondary:focus{border-color:%5;}"
               "QFrame#ctbError{background:%30;border:none;border-left:2px solid %23;}"
               "QLabel#ctbErrorText{color:%10;font-size:%18px;}"
               "QPushButton#ctbChip{background:%25;color:%3;border:1px solid %17;padding:3px 10px;font-size:%16px;}"
               "QPushButton#ctbChip:hover,QPushButton#ctbChip:focus{color:%10;border-color:%5;}"
               "QLabel#ctbSources{color:%3;font-size:%16px;}"
               "QPushButton#ctbNewPill{background:%5;color:%28;border:none;padding:4px 12px;font-size:%4px;"
               "font-weight:700;letter-spacing:1px;}"
               "QPushButton#ctbNewPill:hover{background:%29;}")
        .arg(ui::colors::BG_BASE(), QString(ui::fonts::DATA_FAMILY), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3),
             ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_DIM(), ui::fonts::px(-4),
             ui::colors::BORDER_BRIGHT(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE(), ui::colors::AMBER_DIM(),
             ui::fonts::px(0), ui::fonts::px(4), ui::fonts::px(2), ui::fonts::px(-2), ui::colors::BORDER_DIM(),
             ui::fonts::px(-1), ui::colors::AMBER_DIM(), ui::colors::BG_HOVER(), ui::colors::INFO(),
             ui::colors::SUCCESS(), ui::colors::DANGER(), ui::colors::ROW_ALT(), ui::colors::BG_RAISED(),
             ui::colors::BORDER_MED(), ui::colors::WARNING(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(),
             ui::colors::tint(QColor(ui::colors::DANGER()), 0.10));
}

} // namespace fincept::chat_mode::transcript
