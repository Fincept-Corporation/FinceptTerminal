// ChatBubbleFactory.cpp — see header.

#include "screens/ai_chat/ChatBubbleFactory.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QVector>
#include <QWidget>

namespace fincept::ai_chat {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

// Horizontal CSS padding baked into bubble_style(): "padding:10px 14px".
static constexpr int kBubblePadX = 14;

// Dynamic properties carried by a streaming body label.
static constexpr const char* kPropAcc = "acc";                 // painted text
static constexpr const char* kPropPending = "pending";         // buffered, not yet painted
static constexpr const char* kPropFlushArmed = "flush_armed";  // a flush timer is queued
static constexpr const char* kPropPlaceholder = "placeholder"; // acc is a placeholder, not answer text
static constexpr const char* kPropCopyBtn = "copy_btn";
static constexpr const char* kPropFooter = "footer_widget";
static constexpr const char* kPropTimeLbl = "time_lbl";

static int bubble_inner_width(bool is_user, const ChatBubbleFactory::Options& opts) {
    return (is_user ? opts.user_col_max_width : opts.ai_col_max_width) - 2 * kBubblePadX;
}

static QString bubble_style(const QString& role) {
    if (role == "user")
        return "background:rgba(120,53,15,0.45);border:1px solid rgba(217,119,6,0.28);"
               "border-radius:0px;padding:10px 14px;";
    if (role == "system")
        return "background:rgba(50,12,12,0.85);border:1px solid rgba(220,38,38,0.22);"
               "border-radius:0px;padding:10px 14px;";
    return QString("background:%1;border:1px solid %2;border-radius:0px;padding:10px 14px;")
        .arg(col::BG_SURFACE(), col::BORDER_DIM());
}

static QString body_color(const QString& role) {
    if (role == "user")
        return "#fff7ed";
    if (role == "system")
        return "#fee2e2";
    return col::TEXT_PRIMARY();
}

static QString role_color(const QString& role) {
    if (role == "system")
        return col::NEGATIVE();
    return col::AMBER();
}

static QString role_label_text(const QString& role) {
    if (role == "user")
        return QObject::tr("You");
    if (role == "system")
        return QObject::tr("System");
    return QObject::tr("AI");
}

static QString format_timestamp(const QString& timestamp) {
    // chat_messages.timestamp is SQLite datetime('now'): UTC, "yyyy-MM-dd HH:mm:ss".
    // It used to be parsed as LOCAL time, so every reloaded message showed a
    // clock time off by the user's UTC offset.
    QDateTime dt;
    const QDateTime sqlite_utc = QDateTime::fromString(timestamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (sqlite_utc.isValid())
        dt = QDateTime(sqlite_utc.date(), sqlite_utc.time(), QTimeZone(QTimeZone::UTC));
    else
        dt = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!dt.isValid())
        return timestamp.isEmpty() ? QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")) : timestamp;
    const QDateTime local = dt.toLocalTime();
    // A bare HH:mm is ambiguous for anything older than today.
    return local.date() == QDate::currentDate() ? local.toString(QStringLiteral("HH:mm"))
                                                : local.toString(QStringLiteral("MMM d, HH:mm"));
}

static QString footer_text(const QString& time, const QString& meta) {
    if (meta.isEmpty())
        return time;
    return time.isEmpty() ? meta : time + QStringLiteral("  ·  ") + meta;
}

static QLabel* make_time_label(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color:%1;font-size:%2px;background:transparent;").arg(col::TEXT_DIM()).arg(fnt::TINY));
    return lbl;
}

QPushButton* ChatBubbleFactory::make_footer_button(const QString& text, const QString& tooltip) {
    auto* btn = new QPushButton(text);
    btn->setFixedHeight(20);
    btn->setCursor(Qt::PointingHandCursor);
    if (!tooltip.isEmpty())
        btn->setToolTip(tooltip);
    btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                               "border-radius:0px;padding:0 8px;font-size:%3px;}"
                               "QPushButton:hover{background:%2;color:%4;}")
                           .arg(col::TEXT_DIM(), col::BORDER_MED())
                           .arg(fnt::TINY)
                           .arg(col::TEXT_PRIMARY()));
    return btn;
}

static void flash_copied(QPushButton* btn, const QString& restore_text) {
    btn->setText(QObject::tr("Copied!"));
    QTimer::singleShot(1500, btn, [btn, restore_text]() { btn->setText(restore_text); });
}

static void connect_copy(QPushButton* btn, const QString& text) {
    QObject::disconnect(btn, &QPushButton::clicked, nullptr, nullptr);
    QObject::connect(btn, &QPushButton::clicked, btn, [btn, text]() {
        QApplication::clipboard()->setText(text);
        flash_copied(btn, QObject::tr("Copy"));
    });
}

// ── Fenced code blocks ───────────────────────────────────────────────────────
// A QLabel renders a whole answer as one rich-text document, so there is no
// per-block widget to hang a button on. Code is what professionals copy most
// (SQL, Python, formulas), and selecting it by mouse out of a wrapped label
// drops indentation — the footer offers it raw instead.

struct ChatCodeBlock {
    QString lang;
    QString code;
};

static QVector<ChatCodeBlock> extract_code_blocks(const QString& md) {
    QVector<ChatCodeBlock> out;
    bool in_block = false;
    QChar fence_char;
    int fence_len = 0;
    ChatCodeBlock cur;
    QStringList buf;
    const QStringList lines = md.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        // CommonMark: a fence may be indented by up to three spaces.
        int indent = 0;
        while (indent < raw.size() && indent < 4 && raw.at(indent) == QLatin1Char(' '))
            ++indent;
        const QString t = indent < 4 ? raw.mid(indent) : raw;
        if (!in_block) {
            if (t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"))) {
                fence_char = t.at(0);
                fence_len = 0;
                while (fence_len < t.size() && t.at(fence_len) == fence_char)
                    ++fence_len;
                cur = {};
                cur.lang = t.mid(fence_len).trimmed().section(QLatin1Char(' '), 0, 0);
                buf.clear();
                in_block = true;
            }
            continue;
        }
        const QString tt = t.trimmed();
        bool closes = tt.size() >= fence_len && !tt.isEmpty();
        for (const QChar c : tt)
            if (c != fence_char) {
                closes = false;
                break;
            }
        if (closes) {
            cur.code = buf.join(QLatin1Char('\n'));
            out.append(cur);
            in_block = false;
        } else {
            buf.append(raw);
        }
    }
    if (in_block && !buf.isEmpty()) { // unterminated fence (cut-off answer): still copyable
        cur.code = buf.join(QLatin1Char('\n'));
        out.append(cur);
    }
    return out;
}

// Add (or refresh) the footer's "Copy code" action for `md`. One block copies
// directly; several open a menu listing each block plus "all".
static void attach_code_copy(QWidget* footer, const QString& md) {
    if (!footer)
        return;
    auto* fhl = qobject_cast<QHBoxLayout*>(footer->layout());
    if (!fhl)
        return;
    const QVector<ChatCodeBlock> blocks = extract_code_blocks(md);
    auto* btn = footer->findChild<QPushButton*>(QStringLiteral("aiCodeCopyBtn"));
    if (blocks.isEmpty()) {
        if (btn)
            btn->hide();
        return;
    }
    if (!btn) {
        btn = ChatBubbleFactory::make_footer_button(QString(), QObject::tr("Copy the code from this answer"));
        btn->setObjectName(QStringLiteral("aiCodeCopyBtn"));
        // Before the trailing Copy button when there is one.
        auto* copy = footer->findChild<QPushButton*>(QStringLiteral("aiCopyBtn"));
        const int at = copy ? fhl->indexOf(copy) : -1;
        if (at >= 0)
            fhl->insertWidget(at, btn);
        else
            fhl->addWidget(btn);
    }
    btn->show();
    QObject::disconnect(btn, &QPushButton::clicked, nullptr, nullptr);
    if (blocks.size() == 1) {
        const QString label = QObject::tr("Copy code");
        btn->setText(label);
        const QString code = blocks.first().code;
        QObject::connect(btn, &QPushButton::clicked, btn, [btn, code, label]() {
            QApplication::clipboard()->setText(code);
            flash_copied(btn, label);
        });
        return;
    }
    const QString label = QObject::tr("Copy code (%1) ▾").arg(blocks.size());
    btn->setText(label);
    QObject::connect(btn, &QPushButton::clicked, btn, [btn, blocks, label]() {
        auto* menu = new QMenu(btn);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QStringList all;
        for (int i = 0; i < blocks.size(); ++i) {
            const auto& b = blocks[i];
            const int n_lines = b.code.isEmpty() ? 0 : int(b.code.count(QLatin1Char('\n'))) + 1;
            const QString title = QObject::tr("Block %1 · %2 · %n line(s)", "", n_lines)
                                      .arg(i + 1)
                                      .arg(b.lang.isEmpty() ? QObject::tr("code") : b.lang);
            const QString code = b.code;
            QObject::connect(menu->addAction(title), &QAction::triggered, btn, [btn, code, label]() {
                QApplication::clipboard()->setText(code);
                flash_copied(btn, label);
            });
            all.append(b.code);
        }
        menu->addSeparator();
        const QString joined = all.join(QStringLiteral("\n\n"));
        QObject::connect(menu->addAction(QObject::tr("All code blocks")), &QAction::triggered, btn,
                         [btn, joined, label]() {
                             QApplication::clipboard()->setText(joined);
                             flash_copied(btn, label);
                         });
        menu->popup(btn->mapToGlobal(QPoint(0, btn->height())));
    });
}

// Markdown links in a reply (sources, filings, docs) were rendered as links but did
// nothing when clicked: a QLabel only opens them if told to. The model controls the link
// target, so only web links are followed — a file:// or custom-scheme URL would let a
// reply launch a local program with one click.
static void enable_web_links(QLabel* body) {
    QObject::connect(body, &QLabel::linkActivated, body, [](const QString& link) {
        const QUrl url(link);
        const QString scheme = url.scheme().toLower();
        if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
            QDesktopServices::openUrl(url);
    });
}

// Build the row + column scaffold shared by static and streaming bubbles.
// `body` is added to the bubble frame; the column + row layouts handle role
// label and alignment. The footer is built only when show_footer is true.
struct Scaffold {
    QWidget* row = nullptr;
    QWidget* column = nullptr;
    QVBoxLayout* col_vl = nullptr;
    QFrame* frame = nullptr;
    QLabel* role_lbl = nullptr;
};

static Scaffold build_scaffold(const ChatBubbleFactory::Options& opts) {
    const bool is_user = (opts.role == "user");

    auto* row = new QWidget;
    row->setStyleSheet("background:transparent;");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    if (is_user)
        rl->addStretch();

    auto* col_widget = new QWidget;
    col_widget->setStyleSheet("background:transparent;");
    col_widget->setMaximumWidth(is_user ? opts.user_col_max_width : opts.ai_col_max_width);
    auto* cvl = new QVBoxLayout(col_widget);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(4);

    auto* role_lbl = new QLabel(role_label_text(opts.role));
    role_lbl->setAlignment(is_user ? Qt::AlignRight : Qt::AlignLeft);
    role_lbl->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:600;background:transparent;")
                                .arg(role_color(opts.role))
                                .arg(fnt::TINY));
    cvl->addWidget(role_lbl);

    auto* frame = new QFrame;
    frame->setStyleSheet(bubble_style(opts.role));
    auto* bvl = new QVBoxLayout(frame);
    bvl->setContentsMargins(0, 0, 0, 0);
    cvl->addWidget(frame);

    rl->addWidget(col_widget);
    if (!is_user)
        rl->addStretch();

    return {row, col_widget, cvl, frame, role_lbl};
}

static QLabel* make_body(const ChatBubbleFactory::Options& opts, Qt::TextFormat format, const QString& text) {
    const bool is_user = (opts.role == "user");
    auto* body = new QLabel;
    body->setTextFormat(format);
    body->setText(text);
    body->setWordWrap(true);
    body->setMaximumWidth(bubble_inner_width(is_user, opts));
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->setStyleSheet(
        QString("QLabel{background:transparent;color:%1;font-size:%2px;}").arg(body_color(opts.role)).arg(fnt::BODY));
    enable_web_links(body);
    return body;
}

// Footer: [time · meta] … [Copy code] [Copy]  (user rows right-aligned).
static void build_footer(ChatBubbleFactory::Bubble& out, QVBoxLayout* col_vl, const ChatBubbleFactory::Options& opts,
                         const QString& time_text, bool streaming) {
    const bool is_user = (opts.role == "user");
    const bool is_system = (opts.role == "system");

    auto* footer = new QWidget;
    footer->setStyleSheet("background:transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 0, 0, 0);
    fhl->setSpacing(6);

    // Streaming bubbles get their time at finalize — there is no meaningful
    // timestamp until the answer is complete.
    QLabel* time_lbl = nullptr;
    const QString ft = footer_text(time_text, opts.footer_meta);
    if (streaming || !ft.isEmpty()) {
        time_lbl = make_time_label(ft);
        time_lbl->setVisible(!streaming && !ft.isEmpty());
        fhl->addWidget(time_lbl);
    }
    if (!is_user)
        fhl->addStretch();

    QPushButton* copy_btn = nullptr;
    if (!is_system) {
        copy_btn = ChatBubbleFactory::make_footer_button(QObject::tr("Copy"), QObject::tr("Copy this message"));
        copy_btn->setObjectName(QStringLiteral("aiCopyBtn"));
        connect_copy(copy_btn, opts.content);
        fhl->addWidget(copy_btn);
        if (streaming)
            copy_btn->hide(); // shown by finalize_streaming
    }
    if (is_user)
        fhl->insertStretch(0);
    col_vl->addWidget(footer);

    if (!streaming && !is_user && !is_system)
        attach_code_copy(footer, opts.content);

    out.footer = footer;
    out.footer_layout = fhl;
    out.time_lbl = time_lbl;
    out.copy_btn = copy_btn;
    if (out.body) {
        out.body->setProperty(kPropFooter, QVariant::fromValue(static_cast<QObject*>(footer)));
        if (copy_btn)
            out.body->setProperty(kPropCopyBtn, QVariant::fromValue(static_cast<QObject*>(copy_btn)));
        if (time_lbl)
            out.body->setProperty(kPropTimeLbl, QVariant::fromValue(static_cast<QObject*>(time_lbl)));
    }
}

ChatBubbleFactory::Bubble ChatBubbleFactory::build(const Options& opts) {
    Scaffold s = build_scaffold(opts);
    auto* body = make_body(opts, Qt::MarkdownText, opts.content);
    qobject_cast<QVBoxLayout*>(s.frame->layout())->addWidget(body);

    Bubble out;
    out.row = s.row;
    out.column = s.column;
    out.frame = s.frame;
    out.role_lbl = s.role_lbl;
    out.body = body;
    if (opts.show_footer)
        build_footer(out, s.col_vl, opts, opts.timestamp_iso.isEmpty() ? QString() : format_timestamp(opts.timestamp_iso),
                     /*streaming=*/false);
    return out;
}

ChatBubbleFactory::Bubble ChatBubbleFactory::build_streaming(const Options& opts) {
    Scaffold s = build_scaffold(opts);

    // Streaming body — PlainText during streaming (appending chunks as markdown
    // would re-parse on every paint; waste). finalize_streaming() switches to
    // MarkdownText and re-renders. Accumulated text lives on the widget as
    // dynamic properties so chunk handlers don't need external state.
    auto* body = make_body(opts, Qt::PlainText, QString());
    body->setProperty(kPropAcc, QString{});
    body->setProperty(kPropPending, QString{});
    qobject_cast<QVBoxLayout*>(s.frame->layout())->addWidget(body);

    Bubble out;
    out.row = s.row;
    out.column = s.column;
    out.frame = s.frame;
    out.role_lbl = s.role_lbl;
    out.body = body;
    if (opts.show_footer)
        build_footer(out, s.col_vl, opts, QString(), /*streaming=*/true);
    return out;
}

// Paint whatever is buffered. Runs from the coalescing timer and before any read.
static void flush_streaming(QLabel* body) {
    body->setProperty(kPropFlushArmed, false);
    const QString pending = body->property(kPropPending).toString();
    if (pending.isEmpty())
        return;
    body->setProperty(kPropPending, QString());
    QString acc = body->property(kPropAcc).toString();
    if (body->property(kPropPlaceholder).toBool()) {
        acc = pending; // the real answer replaces "Calling tool…"
        body->setProperty(kPropPlaceholder, false);
    } else {
        acc += pending;
    }
    body->setProperty(kPropAcc, acc);
    body->setText(acc);
}

void ChatBubbleFactory::append_streaming_chunk(QLabel* body, const QString& chunk) {
    if (!body || chunk.isEmpty())
        return;
    body->setProperty(kPropPending, body->property(kPropPending).toString() + chunk);
    if (body->property(kPropFlushArmed).toBool())
        return;
    body->setProperty(kPropFlushArmed, true);
    // Context = body: the timer dies with the label.
    QTimer::singleShot(kStreamFlushMs, body, [body]() { flush_streaming(body); });
}

QString ChatBubbleFactory::streaming_text(QLabel* body) {
    if (!body)
        return {};
    flush_streaming(body);
    if (body->property(kPropPlaceholder).toBool())
        return {};
    return body->property(kPropAcc).toString();
}

void ChatBubbleFactory::replace_streaming_text(QLabel* body, const QString& text) {
    if (!body)
        return;
    body->setProperty(kPropPending, QString());
    // Replacement text is a status placeholder ("Calling tool…", "Stopped."),
    // never answer text: the next streamed chunk takes its place, and
    // streaming_text() does not report it as a partial answer.
    body->setProperty(kPropPlaceholder, true);
    body->setProperty(kPropAcc, text);
    body->setText(text);
}

QWidget* ChatBubbleFactory::footer_of(QLabel* body) {
    if (!body)
        return nullptr;
    return qobject_cast<QWidget*>(body->property(kPropFooter).value<QObject*>());
}

void ChatBubbleFactory::finalize_streaming(QLabel* body, const QString& final_text, const QString& footer_meta) {
    if (!body)
        return;
    flush_streaming(body);
    QString text = final_text;
    if (text.isEmpty())
        text = body->property(kPropAcc).toString();
    if (!text.isEmpty()) {
        body->setTextFormat(Qt::MarkdownText);
        body->setText(text);
        body->setProperty(kPropAcc, text);
    }
    body->setProperty(kPropPlaceholder, false);
    if (auto* copy_btn = qobject_cast<QPushButton*>(body->property(kPropCopyBtn).value<QObject*>())) {
        // Reconnect with the final text so Copy uses what's actually shown.
        connect_copy(copy_btn, text);
        copy_btn->show();
    }
    if (auto* time_lbl = qobject_cast<QLabel*>(body->property(kPropTimeLbl).value<QObject*>())) {
        time_lbl->setText(footer_text(QDateTime::currentDateTime().toString("HH:mm"), footer_meta));
        time_lbl->show();
    }
    attach_code_copy(footer_of(body), text);
}

} // namespace fincept::ai_chat
