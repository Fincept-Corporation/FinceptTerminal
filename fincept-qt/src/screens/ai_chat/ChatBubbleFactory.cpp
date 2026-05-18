// ChatBubbleFactory.cpp — see header.

#include "screens/ai_chat/ChatBubbleFactory.h"

#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

namespace fincept::ai_chat {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

// Horizontal CSS padding baked into bubble_style(): "padding:10px 14px".
static constexpr int kBubblePadX = 14;

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
        return "You";
    if (role == "system")
        return "System";
    return "AI";
}

static QString format_timestamp(const QString& timestamp) {
    QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(timestamp, "yyyy-MM-dd HH:mm:ss");
    if (!dt.isValid() && !timestamp.isEmpty())
        return timestamp;
    return (dt.isValid() ? dt : QDateTime::currentDateTime()).toString("HH:mm");
}

static QPushButton* make_copy_button(const QString& initial_text) {
    auto* btn = new QPushButton("Copy");
    btn->setFixedHeight(20);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                               "border-radius:0px;padding:0 8px;font-size:%3px;}"
                               "QPushButton:hover{background:%2;color:%4;}")
                           .arg(col::TEXT_DIM(), col::BORDER_MED())
                           .arg(fnt::TINY)
                           .arg(col::TEXT_PRIMARY()));
    QObject::connect(btn, &QPushButton::clicked, btn, [btn, initial_text]() {
        // initial_text is captured for the static-content variant; the streaming
        // variant overrides this connection at finalize_streaming() time.
        QApplication::clipboard()->setText(initial_text);
        btn->setText("Copied!");
        QTimer::singleShot(1500, btn, [btn]() { btn->setText("Copy"); });
    });
    return btn;
}

// Build the row + column scaffold shared by static and streaming bubbles.
// `body` is added to the bubble frame; the column + row layouts handle role
// label and alignment. The footer is built only when show_footer is true.
struct Scaffold {
    QWidget*     row      = nullptr;
    QWidget*     column   = nullptr;
    QVBoxLayout* col_vl   = nullptr;
    QFrame*      frame    = nullptr;
    QLabel*      role_lbl = nullptr;
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

ChatBubbleFactory::Bubble ChatBubbleFactory::build(const Options& opts) {
    const bool is_user = (opts.role == "user");
    const bool is_system = (opts.role == "system");

    Scaffold s = build_scaffold(opts);

    auto* body = new QLabel;
    body->setTextFormat(Qt::MarkdownText);
    body->setText(opts.content);
    body->setWordWrap(true);
    body->setMaximumWidth(bubble_inner_width(is_user, opts));
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->setStyleSheet(QString("QLabel{background:transparent;color:%1;font-size:%2px;}")
                            .arg(body_color(opts.role))
                            .arg(fnt::BODY));
    qobject_cast<QVBoxLayout*>(s.frame->layout())->addWidget(body);

    Bubble out;
    out.row      = s.row;
    out.column   = s.column;
    out.frame    = s.frame;
    out.role_lbl = s.role_lbl;
    out.body     = body;

    if (!opts.show_footer)
        return out;

    auto* footer = new QWidget;
    footer->setStyleSheet("background:transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 0, 0, 0);
    fhl->setSpacing(6);

    QLabel* time_lbl = nullptr;
    if (!opts.timestamp_iso.isEmpty()) {
        time_lbl = new QLabel(format_timestamp(opts.timestamp_iso));
        time_lbl->setStyleSheet(
            QString("color:%1;font-size:%2px;background:transparent;").arg(col::TEXT_DIM()).arg(fnt::TINY));
        fhl->addWidget(time_lbl);
    }

    QPushButton* copy_btn = nullptr;
    if (!is_user && !is_system) {
        if (time_lbl)
            fhl->addStretch();
        copy_btn = make_copy_button(opts.content);
        fhl->addWidget(copy_btn);
    }

    if (is_user)
        fhl->insertStretch(0);
    s.col_vl->addWidget(footer);

    out.time_lbl = time_lbl;
    out.copy_btn = copy_btn;
    return out;
}

ChatBubbleFactory::Bubble ChatBubbleFactory::build_streaming(const Options& opts) {
    const bool is_user = (opts.role == "user");
    const bool is_system = (opts.role == "system");

    Scaffold s = build_scaffold(opts);

    // Streaming body — PlainText during streaming (appending chunks as markdown
    // would re-parse on every token; waste). finalize_streaming() switches to
    // MarkdownText and re-renders. Accumulated text lives on the widget as
    // dynamic property "acc" so chunk handlers don't need external state.
    auto* body = new QLabel;
    body->setTextFormat(Qt::PlainText);
    body->setText({});
    body->setProperty("acc", QString{});
    body->setWordWrap(true);
    body->setMaximumWidth(bubble_inner_width(is_user, opts));
    body->setTextInteractionFlags(Qt::TextBrowserInteraction);
    body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    body->setStyleSheet(QString("QLabel{background:transparent;color:%1;font-size:%2px;}")
                            .arg(body_color(opts.role))
                            .arg(fnt::BODY));
    qobject_cast<QVBoxLayout*>(s.frame->layout())->addWidget(body);

    Bubble out;
    out.row      = s.row;
    out.column   = s.column;
    out.frame    = s.frame;
    out.role_lbl = s.role_lbl;
    out.body     = body;

    if (!opts.show_footer)
        return out;

    auto* footer = new QWidget;
    footer->setStyleSheet("background:transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 0, 0, 0);
    fhl->setSpacing(6);

    // Streaming bubbles intentionally have no timestamp at build time — there
    // is no meaningful timestamp until the stream completes. Callers can read
    // the body's "acc" property when they're done.

    QPushButton* copy_btn = nullptr;
    if (!is_user && !is_system) {
        copy_btn = make_copy_button({});
        copy_btn->hide();   // shown by finalize_streaming
        fhl->addStretch();
        fhl->addWidget(copy_btn);
        // Stash on the body so finalize_streaming() can find it without an
        // external map.
        body->setProperty("copy_btn", QVariant::fromValue(static_cast<QObject*>(copy_btn)));
    }

    if (is_user)
        fhl->insertStretch(0);
    s.col_vl->addWidget(footer);

    out.copy_btn = copy_btn;
    return out;
}

void ChatBubbleFactory::append_streaming_chunk(QLabel* body, const QString& chunk) {
    if (!body || chunk.isEmpty())
        return;
    QString acc = body->property("acc").toString();
    if (acc == "Calling tool...")
        acc = chunk;
    else
        acc += chunk;
    body->setProperty("acc", acc);
    body->setText(acc);
}

void ChatBubbleFactory::replace_streaming_text(QLabel* body, const QString& text) {
    if (!body)
        return;
    body->setProperty("acc", text);
    body->setText(text);
}

void ChatBubbleFactory::finalize_streaming(QLabel* body, const QString& final_text) {
    if (!body)
        return;
    QString text = final_text;
    if (text.isEmpty())
        text = body->property("acc").toString();
    if (!text.isEmpty()) {
        body->setTextFormat(Qt::MarkdownText);
        body->setText(text);
        body->setProperty("acc", text);
    }
    auto* copy_obj = body->property("copy_btn").value<QObject*>();
    if (auto* copy_btn = qobject_cast<QPushButton*>(copy_obj)) {
        // Reconnect with the final text so Copy uses what's actually shown.
        QObject::disconnect(copy_btn, &QPushButton::clicked, nullptr, nullptr);
        QObject::connect(copy_btn, &QPushButton::clicked, copy_btn, [copy_btn, text]() {
            QApplication::clipboard()->setText(text);
            copy_btn->setText("Copied!");
            QTimer::singleShot(1500, copy_btn, [copy_btn]() { copy_btn->setText("Copy"); });
        });
        copy_btn->show();
    }
}

} // namespace fincept::ai_chat
