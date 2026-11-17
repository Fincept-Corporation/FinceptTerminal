// AiChatBubble.cpp — "Quick Chat" floating bubble.
//
// Stays explicitly separate from the AI Chat tab:
//   • No ChatRepository writes — chat history is in-memory only.
//   • Conversations never appear in the AI Chat tab's session list.
//
//   ┌─ ▌QUICK CHAT  not saved · separate from AI Chat     [VOICE] [CLEAR] [✕] ─┐
//   │  ● model ▾                                                                │
//   │  transcript (same renderer as the tab, compact) / welcome                 │
//   │  ───────────────────────────────────────────────────────────────────────  │
//   │  [ input box .................................. ] [MIC] [SEND]          │
//   │  status chip (hidden when idle)                                [STOP]     │
//   └───────────────────────────────────────────────────────────────────────────┘

#include "screens/ai_chat/AiChatBubble.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/ai_chat/AiChatStyle.h"
#include "screens/ai_chat/AiChatTranscript.h"
#include "services/llm/LlmErrors.h"
#include "services/llm/LlmToolActivity.h"
#include "services/llm/ModelRouter.h"
#include "services/stt/SpeechService.h"
#include "services/tts/TtsService.h"
#include "services/voice_trigger/ClapDetectorService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/ModelBadge.h"
#include "ui/widgets/PlanHint.h"
#include "ui/widgets/Requirements.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QStyle>

#include <algorithm>
#include <memory>

namespace fincept {

namespace col = fincept::ui::colors;
namespace st = fincept::screens::aichat::style;
namespace aic = fincept::screens::aichat;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int PANEL_W = 400;
static constexpr int PANEL_H = 560;
static constexpr int BTN_SIZE = 42;
static constexpr int MARGIN = 16;

// ── Constructor ───────────────────────────────────────────────────────────────
AiChatBubble::AiChatBubble(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    build_bubble_button();
    build_chat_panel();
    chat_panel_->hide();

    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::finished_streaming, this,
            &AiChatBubble::on_streaming_done, Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::route_notice, this, &AiChatBubble::on_route_notice,
            Qt::UniqueConnection);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::request_started, this,
            &AiChatBubble::on_request_started);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::request_chunk, this,
            &AiChatBubble::on_request_chunk);
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::ui_operating, this,
            &AiChatBubble::on_ui_operating);
    // A model set up (or removed) while the panel is open updates its welcome at once; a
    // closed panel re-checks when it opens.
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this, [this]() {
        if (is_open_)
            refresh_welcome_setup();
    });

    // STT wiring
    auto& stt = services::SpeechService::instance();
    connect(&stt, &services::SpeechService::transcription_ready, this, &AiChatBubble::on_transcription);
    connect(&stt, &services::SpeechService::listening_changed, this, &AiChatBubble::on_stt_listening_changed);
    connect(&stt, &services::SpeechService::error_occurred, this, &AiChatBubble::on_stt_error);

    // TTS wiring — drives the "Speaking" status + auto-resume in voice mode
    auto& tts = services::TtsService::instance();
    connect(&tts, &services::TtsService::speaking_started, this, [this]() {
        is_speaking_ = true;
        error_msg_.clear();
        render_status();
    });
    connect(&tts, &services::TtsService::speaking_finished, this, [this]() {
        is_speaking_ = false;
        render_status();
        if (voice_mode_ && is_open_ && !is_listening_ && !streaming_)
            QTimer::singleShot(400, this, &AiChatBubble::start_listening);
    });
    connect(&tts, &services::TtsService::error_occurred, this, [this](const QString& msg) {
        LOG_WARN("AiChatBubble", QString("TTS error: %1").arg(msg));
        is_speaking_ = false;
        error_msg_ = msg;
        render_status();
        if (voice_mode_ && is_open_ && !is_listening_)
            QTimer::singleShot(600, this, &AiChatBubble::start_listening);
    });

    // Clap-to-start trigger. The service is idle by default — only started when AppConfig
    // "voice/clap_to_start/enabled" is true. A clap opens the panel and starts the mic.
    auto& clap = services::ClapDetectorService::instance();
    connect(&clap, &services::ClapDetectorService::clap_detected, this, [this]() {
        LOG_DEBUG("AiChatBubble", "Clap detected — opening panel + starting mic");
        if (!is_open_)
            open_panel();
        if (!is_listening_ && !is_speaking_)
            start_listening();
    });
    // The detector's failures are shown in the status strip, not only the log.
    connect(&clap, &services::ClapDetectorService::error_occurred, this, [this](const QString& msg) {
        LOG_WARN("AiChatBubble", QString("Clap-to-start error: %1").arg(msg));
        error_msg_ = tr("Clap-to-start: %1").arg(msg);
        render_status();
    });
    if (services::ClapDetectorService::is_enabled_in_config())
        clap.start();

    if (parent)
        parent->installEventFilter(this);

    refresh_theme(); // text-derived sizes
    reposition();
    raise();

    // Never inside theme_changed (restyling / resizing there stalls a theme switch): once the
    // appearance has settled, and each sheet only when its text changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
}

// ── Theme refresh ─────────────────────────────────────────────────────────────
void AiChatBubble::refresh_theme() {
    // setStyleSheet() re-styles a widget even for an identical sheet: only on change.
    const auto put = [](QWidget* w, const QString& sheet) {
        if (w && w->styleSheet() != sheet)
            w->setStyleSheet(sheet);
    };
    put(bubble_btn_, QString("QWidget#aicQuickButton{background:%1;border:1px solid %2;}"
                             "QLabel#aicQuickGlyph{color:%3;background:transparent;font-size:%5px;}"
                             "QLabel#aicQuickBadge{background:%4;color:%6;font-weight:700;font-size:%7px;}")
                         .arg(col::BG_SURFACE(), col::BORDER_BRIGHT(), col::AMBER(), col::DANGER())
                         .arg(fincept::ui::fonts::px(8))
                         .arg(col::TEXT_ON_ACCENT())
                         .arg(fincept::ui::fonts::px(-5)));
    // The shared ai_chat sheet (same look as the tab) plus the panel's own frame.
    put(chat_panel_, st::sheet() + QString("QWidget#aicQuickPanel{background:%1;border:1px solid %2;}")
                                       .arg(col::BG_BASE(), col::BORDER_MED()));

    // Sizes that hold text follow the user's font size so nothing clips at large sizes.
    if (unread_badge_) {
        const int d = std::max(18, st::text_h(-5) + 2);
        unread_badge_->setFixedSize(d, d);
        unread_badge_->move(BTN_SIZE - d + 2, -2);
    }
    if (header_)
        header_->setFixedHeight(std::max(st::strip_h() + st::u(2), st::text_h(-3) * 2 + st::button_h() + st::u(3)));
    if (input_box_ && mic_btn_ && send_btn_) {
        const int input_h = std::max(40, QFontMetrics(st::ui_font(0)).lineSpacing() * 2 + st::u(3));
        input_box_->setFixedHeight(input_h);
        const int btn_h = std::max(st::button_h(), input_h);
        mic_btn_->setFixedHeight(btn_h);
        send_btn_->setFixedHeight(btn_h);
        if (input_row_)
            input_row_->setFixedHeight(input_h + st::u(4));
    }
    if (status_strip_)
        status_strip_->setFixedHeight(st::button_h() + st::u(2));
}

// ── Reposition ────────────────────────────────────────────────────────────────
void AiChatBubble::reposition() {
    place();
    // The screen under the bubble may still be a placeholder that is built a moment later
    // (lazy materialisation after a navigation); look again then.
    if (!replace_pending_) {
        replace_pending_ = true;
        QTimer::singleShot(300, this, [this]() {
            replace_pending_ = false;
            place();
        });
    }
}

int AiChatBubble::bottom_strip_height() const {
    QWidget* host = parentWidget();
    if (!host || host->width() <= 0 || host->height() <= 0)
        return 0;
    const int h = host->height();
    constexpr int kEdge = 8; // a strip may sit inside a dock area's frame / margin
    // A strip is short (a few text lines at the current font) and wide; the first tall
    // ancestor is the screen's content, where the walk stops.
    const int max_strip_h = std::max(48, QFontMetrics(ui::fonts::font(0)).height() * 3);
    int strip_top = h;
    // Probe just above the bottom edge, under the bubble's centre (a frame pixel may be what
    // the very bottom row hits, hence two depths).
    for (const int depth : {3, kEdge - 1}) {
        QWidget* hit = host->childAt(QPoint(host->width() - MARGIN - BTN_SIZE / 2, h - depth));
        for (QWidget* cur = hit; cur && cur != host; cur = cur->parentWidget()) {
            if (cur == this || cur == chat_panel_)
                break;
            const QRect r(cur->mapTo(host, QPoint(0, 0)), cur->size());
            if (r.bottom() < h - kEdge || r.height() > max_strip_h)
                break;
            if (r.width() >= BTN_SIZE * 3) // a label / button inside the strip: keep walking up
                strip_top = std::min(strip_top, r.top());
        }
    }
    return h - strip_top;
}

int AiChatBubble::lift_over_controls(int lift) {
    QWidget* host = parentWidget();
    if (!host || host->width() <= 0 || host->height() <= 0)
        return lift;
    const int w = host->width();
    const int h = host->height();
    const int max_lift = h / 3;
    constexpr int kGap = 4;
    // childAt() skips widgets that are transparent for mouse events: look through the
    // bubble and its panel at the screen underneath.
    const bool self_transparent = testAttribute(Qt::WA_TransparentForMouseEvents);
    const bool panel_transparent = chat_panel_ && chat_panel_->testAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    if (chat_panel_)
        chat_panel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    int result = lift;
    for (int pass = 0; pass < 3; ++pass) { // a second row of controls may sit above the first
        const QRect rest(w - BTN_SIZE - MARGIN, h - BTN_SIZE - MARGIN - result, BTN_SIZE, BTN_SIZE);
        int top = h;
        const QPoint probes[] = {rest.center(), rest.topLeft() + QPoint(3, 3), rest.topRight() + QPoint(-3, 3),
                                 rest.bottomLeft() + QPoint(3, -3), rest.bottomRight() + QPoint(-3, -3)};
        for (const QPoint& p : probes) {
            for (QWidget* cur = host->childAt(p); cur && cur != host; cur = cur->parentWidget()) {
                if (qobject_cast<QAbstractButton*>(cur) || qobject_cast<QLineEdit*>(cur) ||
                    qobject_cast<QComboBox*>(cur)) {
                    top = std::min(top, cur->mapTo(host, QPoint(0, 0)).y());
                    break;
                }
            }
        }
        if (top >= h)
            break; // nothing interactive under the bubble
        const int wanted = h - MARGIN - top + kGap; // the bubble ends kGap px above that control
        if (wanted <= result || wanted > max_lift) {
            if (wanted > max_lift)
                result = lift; // a tall form: stay in the corner rather than float mid-screen
            break;
        }
        result = wanted;
    }

    setAttribute(Qt::WA_TransparentForMouseEvents, self_transparent);
    if (chat_panel_)
        chat_panel_->setAttribute(Qt::WA_TransparentForMouseEvents, panel_transparent);
    return result;
}

void AiChatBubble::place() {
    if (!parentWidget())
        return;
    // Keep clear of a status strip along the bottom of the screen under the bubble, and of
    // a button or input in that corner (the Agent Workbench's FOLLOW UP).
    const int lift = lift_over_controls(bottom_strip_height());
    const QRect pr = parentWidget()->rect();
    const int bx = pr.width() - BTN_SIZE - MARGIN;
    const int by = pr.height() - BTN_SIZE - MARGIN - lift;
    setGeometry(bx, by, BTN_SIZE, BTN_SIZE);
    bubble_btn_->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);

    // The panel never exceeds the host (small windows): narrower / shorter as needed.
    const int pw = std::min(PANEL_W, std::max(280, pr.width() - 2 * MARGIN));
    const int ph = std::min(PANEL_H, std::max(320, pr.height() - BTN_SIZE - 2 * MARGIN - 10 - lift));
    const int px = pr.width() - pw - MARGIN;
    const int py = pr.height() - ph - BTN_SIZE - MARGIN - 10 - lift;
    if (chat_panel_->parentWidget() != parentWidget())
        chat_panel_->setParent(parentWidget());
    chat_panel_->setGeometry(std::max(4, px), (py < 4 ? 4 : py), pw, ph);

    if (is_open_) {
        chat_panel_->show();
        chat_panel_->raise();
    }
    raise();
}

// ── Bubble (collapsed) button ────────────────────────────────────────────────
void AiChatBubble::build_bubble_button() {
    bubble_btn_ = new QWidget(this);
    bubble_btn_->setObjectName(QStringLiteral("aicQuickButton"));
    bubble_btn_->setAttribute(Qt::WA_StyledBackground, true);
    bubble_btn_->setFixedSize(BTN_SIZE, BTN_SIZE);
    bubble_btn_->setCursor(Qt::PointingHandCursor);
    bubble_btn_->setToolTip(tr("Quick Chat (separate from AI Chat tab)"));

    bubble_glyph_ = new QLabel(QStringLiteral("⬡"), bubble_btn_);
    bubble_glyph_->setObjectName(QStringLiteral("aicQuickGlyph"));
    bubble_glyph_->setAlignment(Qt::AlignCenter);
    bubble_glyph_->setGeometry(0, 0, BTN_SIZE, BTN_SIZE);

    unread_badge_ = new QLabel(bubble_btn_);
    unread_badge_->setObjectName(QStringLiteral("aicQuickBadge"));
    unread_badge_->setAlignment(Qt::AlignCenter); // size + position: refresh_theme()
    unread_badge_->hide();

    bubble_btn_->installEventFilter(this);
}

// ── Chat panel ────────────────────────────────────────────────────────────────
void AiChatBubble::build_chat_panel() {
    chat_panel_ = new QWidget(nullptr);
    chat_panel_->setObjectName(QStringLiteral("aicQuickPanel"));
    chat_panel_->setAttribute(Qt::WA_StyledBackground, true);
    // The shared selector sheet goes on before any child exists (refresh_theme keeps it).
    chat_panel_->setStyleSheet(st::sheet() + QString("QWidget#aicQuickPanel{background:%1;border:1px solid %2;}")
                                                 .arg(col::BG_BASE(), col::BORDER_MED()));
    chat_panel_->resize(PANEL_W, PANEL_H);

    auto* vl = new QVBoxLayout(chat_panel_);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);

    vl->addWidget(build_panel_header());

    transcript_ = new screens::aichat::AicTranscriptView(screens::aichat::AicTranscriptView::Density::Compact,
                                                         chat_panel_);
    transcript_->setObjectName(QString::fromLatin1(st::kTranscript));
    transcript_->set_report_source(tr("Quick Chat"));
    connect(transcript_, &screens::aichat::AicTranscriptView::action_requested, this,
            [this](quint64, aic::TurnAction action) {
                switch (action) {
                    case aic::TurnAction::ModelSettings:
                        ui::open_llm_settings();
                        break;
                    case aic::TurnAction::LlmSettings:
                        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                                     QVariantMap{{"screen_id", QStringLiteral("settings")}});
                        break;
                    case aic::TurnAction::ViewPlans:
                        ui::billing::open_plans(this);
                        break;
                    case aic::TurnAction::OpenSettings: {
                        const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::llm()});
                        if (now.link.valid())
                            ui::open_setup_link(now.link, this);
                        else
                            ui::open_llm_settings();
                        break;
                    }
                    default:
                        break; // COPY is done by the transcript; nothing else is offered here
                }
            });
    vl->addWidget(transcript_, 1);

    build_welcome_widget();
    vl->addWidget(welcome_widget_, 1);
    show_welcome_if_empty();

    auto* sep = new QFrame(chat_panel_);
    sep->setObjectName(QString::fromLatin1(st::kHairline));
    sep->setFixedHeight(1);
    vl->addWidget(sep);

    vl->addWidget(build_input_row());
    vl->addWidget(build_status_strip());
}

QWidget* AiChatBubble::build_panel_header() {
    header_ = new QWidget(chat_panel_);
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(st::u(3), st::u(2), st::u(2), st::u(2));
    hl->setSpacing(st::u(2));

    // Title block — title + subtitle + the model badge stacked
    auto* title_col = new QWidget(header_);
    auto* tl = new QVBoxLayout(title_col);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);

    hdr_title_lbl_ = new QLabel(tr("QUICK CHAT"), title_col);
    hdr_title_lbl_->setObjectName(QString::fromLatin1(st::kSectionTitle));
    tl->addWidget(hdr_title_lbl_);

    hdr_subtitle_lbl_ = new QLabel(tr("Not saved · separate from AI Chat tab"), title_col);
    hdr_subtitle_lbl_->setObjectName(QString::fromLatin1(st::kDim));
    tl->addWidget(hdr_subtitle_lbl_);

    // Which model answers Quick Chat (ModelRouter "quick_chat"); click = picker.
    model_badge_ = new ui::ModelBadge(QString::fromLatin1(ai_chat::kFeatureQuickChat), title_col);
    model_badge_->set_compact(true);
    tl->addSpacing(st::u(1));
    tl->addWidget(model_badge_, 0, Qt::AlignLeft);

    hl->addWidget(title_col, 1);

    // Voice mode toggle (continuous mic + speak)
    voice_mode_btn_ = new QPushButton(tr("VOICE"), header_);
    voice_mode_btn_->setObjectName(QString::fromLatin1(st::kBtn));
    voice_mode_btn_->setFixedHeight(st::button_h());
    voice_mode_btn_->setCheckable(true);
    voice_mode_btn_->setCursor(Qt::PointingHandCursor);
    voice_mode_btn_->setAccessibleName(tr("Voice mode"));
    voice_mode_btn_->setToolTip(tr("Toggle continuous voice conversation (auto-listen + auto-speak)"));
    connect(voice_mode_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_voice_mode);
    hl->addWidget(voice_mode_btn_, 0, Qt::AlignTop);

    // Clear chat (does NOT touch ChatRepository — wipes in-memory only)
    new_btn_ = new QPushButton(tr("CLEAR"), header_);
    new_btn_->setObjectName(QString::fromLatin1(st::kBtn));
    new_btn_->setFixedHeight(st::button_h());
    new_btn_->setCursor(Qt::PointingHandCursor);
    new_btn_->setAccessibleName(tr("Clear Quick Chat"));
    new_btn_->setToolTip(tr("Clear this chat (Ctrl+L)"));
    connect(new_btn_, &QPushButton::clicked, this, &AiChatBubble::on_clear_chat);
    hl->addWidget(new_btn_, 0, Qt::AlignTop);

    // Close (Esc)
    close_btn_ = new QPushButton(QStringLiteral("✕"), header_);
    close_btn_->setObjectName(QString::fromLatin1(st::kIconBtn));
    close_btn_->setFixedSize(st::button_h(), st::button_h());
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setAccessibleName(tr("Close Quick Chat"));
    close_btn_->setToolTip(tr("Close (Esc)"));
    connect(close_btn_, &QPushButton::clicked, this, &AiChatBubble::close_panel);
    hl->addWidget(close_btn_, 0, Qt::AlignTop);

    return header_;
}

QWidget* AiChatBubble::build_input_row() {
    input_row_ = new QWidget(chat_panel_);
    auto* hl = new QHBoxLayout(input_row_);
    hl->setContentsMargins(st::u(2), st::u(2), st::u(2), st::u(2));
    hl->setSpacing(st::u(1) + 2);

    input_box_ = new QPlainTextEdit(input_row_); // height: refresh_theme()
    input_box_->setObjectName(QString::fromLatin1(st::kComposerEdit));
    input_box_->setPlaceholderText(tr("Ask anything…  (Enter to send, Shift+Enter for newline)"));
    input_box_->installEventFilter(this);
    input_box_->setAccessibleName(tr("Quick Chat message"));
    input_box_->setProperty("fincept_agent_risk", "spend"); // Enter sends a billed LLM request
    input_box_->setAccessibleDescription(tr("Enter sends, Shift+Enter inserts a new line, Esc closes, Ctrl+L clears."));
    hl->addWidget(input_box_, 1);

    mic_btn_ = new QPushButton(tr("MIC"), input_row_); // height: refresh_theme()
    mic_btn_->setObjectName(QString::fromLatin1(st::kBtn));
    mic_btn_->setCursor(Qt::PointingHandCursor);
    mic_btn_->setToolTip(tr("Push-to-talk: click to dictate one message"));
    // No setCheckable — listening state is rendered via the "active" property.
    mic_btn_->setProperty("active", false);
    mic_btn_->setAccessibleName(tr("Dictate message"));
    connect(mic_btn_, &QPushButton::clicked, this, &AiChatBubble::on_toggle_mic);
    hl->addWidget(mic_btn_);

    send_btn_ = new QPushButton(tr("SEND"), input_row_); // height: refresh_theme()
    send_btn_->setObjectName(QString::fromLatin1(st::kBtnPrimary));
    send_btn_->setProperty("fincept_agent_risk", "spend"); // billed LLM request
    send_btn_->setCursor(Qt::PointingHandCursor);
    send_btn_->setToolTip(tr("Send (Enter)"));
    send_btn_->setAccessibleName(tr("Send message"));
    connect(send_btn_, &QPushButton::clicked, this, &AiChatBubble::on_send);
    hl->addWidget(send_btn_);

    setTabOrder(input_box_, mic_btn_);
    setTabOrder(mic_btn_, send_btn_);

    return input_row_;
}

QWidget* AiChatBubble::build_status_strip() {
    status_strip_ = new QWidget(chat_panel_);
    auto* hl = new QHBoxLayout(status_strip_);
    hl->setContentsMargins(st::u(2), st::u(1), st::u(2), st::u(1));
    hl->setSpacing(st::u(2));

    status_dot_ = new QLabel(QStringLiteral("●"), status_strip_);
    status_dot_->setObjectName(QString::fromLatin1(st::kDim));
    hl->addWidget(status_dot_);

    status_lbl_ = new QLabel(status_strip_);
    status_lbl_->setObjectName(QString::fromLatin1(st::kChip));
    status_lbl_->setWordWrap(false);
    status_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hl->addWidget(status_lbl_, 1);

    stop_speech_btn_ = new QPushButton(tr("STOP"), status_strip_);
    stop_speech_btn_->setObjectName(QString::fromLatin1(st::kBtnDanger));
    stop_speech_btn_->setFixedHeight(st::button_h());
    stop_speech_btn_->setCursor(Qt::PointingHandCursor);
    stop_speech_btn_->setAccessibleName(tr("Stop speaking"));
    stop_speech_btn_->setToolTip(tr("Stop speaking"));
    stop_speech_btn_->hide();
    connect(stop_speech_btn_, &QPushButton::clicked, this, &AiChatBubble::on_stop_speech);
    hl->addWidget(stop_speech_btn_);

    status_strip_->hide(); // hidden when status is Idle
    return status_strip_;
}

// ── Welcome (shown instead of the transcript while the chat is empty) ───────────
void AiChatBubble::build_welcome_widget() {
    welcome_widget_ = new QWidget(chat_panel_);
    auto* wvl = new QVBoxLayout(welcome_widget_);
    wvl->setContentsMargins(st::u(4), st::u(6), st::u(4), st::u(2));
    wvl->setSpacing(st::u(2));

    welcome_glyph_ = new QLabel(QStringLiteral("⬡"), welcome_widget_);
    welcome_glyph_->setObjectName(QString::fromLatin1(st::kSectionTitle));
    welcome_glyph_->setAlignment(Qt::AlignCenter);
    wvl->addWidget(welcome_glyph_);

    welcome_heading_ = new QLabel(tr("How can I help?"), welcome_widget_);
    welcome_heading_->setObjectName(QString::fromLatin1(st::kHeading));
    welcome_heading_->setAlignment(Qt::AlignCenter);
    wvl->addWidget(welcome_heading_);

    welcome_sub_ = new QLabel(tr("Ask a quick question. Nothing here is saved.\n"
                                 "For long-form chats use the AI Chat tab."),
                              welcome_widget_);
    welcome_sub_->setObjectName(QString::fromLatin1(st::kDim));
    welcome_sub_->setAlignment(Qt::AlignCenter);
    welcome_sub_->setWordWrap(true);
    wvl->addWidget(welcome_sub_);

    wvl->addStretch();
}

void AiChatBubble::show_welcome_if_empty() {
    if (!welcome_widget_ || !transcript_ || !chat_history_.empty() || transcript_->turn_count() > 0)
        return;
    transcript_->hide();
    welcome_widget_->show();
}

void AiChatBubble::refresh_welcome_setup() {
    if (!welcome_widget_)
        return;
    const ui::RequirementStatus st_req = ui::check_requirements({ui::Requirement::llm()});
    const int state = st_req.ok ? 1 : 0;
    if (state == welcome_llm_state_)
        return;
    welcome_llm_state_ = state;
    for (QLabel* part : {welcome_glyph_, welcome_heading_, welcome_sub_})
        if (part)
            part->setVisible(st_req.ok);
    if (st_req.ok) {
        if (welcome_setup_)
            welcome_setup_->hide();
        return;
    }
    if (!welcome_setup_) {
        welcome_setup_ = new ui::EmptyState(welcome_widget_);
        welcome_setup_->set_compact(true);
        welcome_setup_->set_alignment(Qt::AlignTop);
        connect(welcome_setup_, &ui::EmptyState::primary_clicked, this, [this]() {
            const ui::RequirementStatus now = ui::check_requirements({ui::Requirement::llm()});
            if (now.link.valid())
                ui::open_setup_link(now.link, this);
            else
                ui::open_llm_settings();
        });
        if (auto* wvl = qobject_cast<QVBoxLayout*>(welcome_widget_->layout()))
            wvl->insertWidget(wvl->count() - 1, welcome_setup_); // above the trailing stretch
    }
    ui::EmptyState::Spec spec = ui::EmptyState::needs_setup_spec(st_req);
    spec.message = tr("Choose a provider and a model in Settings > LLM Config. Quick Chat sends what you type to "
                      "that model — nothing is sent until you press Send.");
    welcome_setup_->apply(spec);
    welcome_setup_->show();
}

void AiChatBubble::hide_welcome() {
    if (welcome_widget_)
        welcome_widget_->hide();
    if (transcript_)
        transcript_->show();
}

// ── Open / Close ──────────────────────────────────────────────────────────────
void AiChatBubble::open_panel() {
    if (is_open_)
        return;
    is_open_ = true;
    unread_count_ = 0;
    unread_badge_->hide();
    refresh_welcome_setup(); // "no AI model configured" said up front, not after a send
    reposition();
    chat_panel_->show();
    chat_panel_->raise();
    raise();
    input_box_->setFocus();
    render_status();
}

void AiChatBubble::close_panel() {
    is_open_ = false;
    // Voice mode running while the panel is hidden is confusing — turn it off.
    if (voice_mode_) {
        voice_mode_ = false;
        if (voice_mode_btn_)
            voice_mode_btn_->setChecked(false);
    }
    if (is_listening_)
        stop_listening();
    if (is_speaking_)
        stop_tts();
    chat_panel_->hide();
    update_breathing(); // minimised while the AI works: the button breathes
    // Closing the panel doesn't disable clap-to-start — that is a global wake-up trigger,
    // not panel state.
}

// ── Clear chat (in-memory only — never touches ChatRepository) ────────────────
void AiChatBubble::on_clear_chat() {
    LOG_INFO("AiChatBubble", "Clearing chat (in-memory only)");
    if (is_listening_)
        stop_listening();
    if (is_speaking_)
        stop_tts();
    transcript_->clear();
    chat_history_.clear();
    streaming_turn_ = 0;
    tool_steps_.clear();
    streaming_ = false;
    error_msg_.clear();
    show_welcome_if_empty();
    set_input_enabled(true);
    render_status();
}

// ── Turns ─────────────────────────────────────────────────────────────────────
void AiChatBubble::add_turn(aic::TurnRole role, const QString& text, aic::NoticeKind notice) {
    aic::Turn t;
    t.role = role;
    t.notice = notice;
    t.text = text;
    t.when = QDateTime::currentDateTime();
    hide_welcome();
    transcript_->append_turn(t);
    transcript_->scroll_to_bottom();
}

// ── Send ──────────────────────────────────────────────────────────────────────
void AiChatBubble::on_send() {
    const QString text = input_box_->toPlainText().trimmed();
    if (text.isEmpty() || streaming_)
        return;

    // Fail fast if LLM is not configured — inline notice beats a delayed network failure.
    if (!ai_chat::LlmService::instance().is_configured()) {
        add_turn(aic::TurnRole::Notice,
                 tr("AI chat is not configured. Open **Settings → LLM Config** and add an API key or pick the "
                    "Fincept provider."),
                 aic::NoticeKind::Setup);
        return;
    }

    input_box_->clear();
    add_turn(aic::TurnRole::User, text);
    tool_steps_.clear();
    // Snapshot the PRIOR turns — chat_streaming appends `text` as the trailing user message
    // itself, so appending here first duplicated it on the wire.
    std::vector<ai_chat::ConversationMessage> hist_copy = chat_history_;
    chat_history_.push_back({"user", text});

    // The answer's block exists from the start (it names the model and says STREAMING).
    const ai_chat::ModelChoice route =
        ai_chat::ModelRouter::instance().resolve(QString::fromLatin1(ai_chat::kFeatureQuickChat));
    aic::Turn a;
    a.role = aic::TurnRole::Assistant;
    a.state = aic::TurnState::Streaming;
    a.provider = route.provider_id();
    a.model = route.model;
    a.route_label = route.label;
    a.when = QDateTime::currentDateTime();
    streaming_turn_ = transcript_->append_turn(a);
    request_started_ms_ = QDateTime::currentMSecsSinceEpoch();

    streaming_ = true;
    error_msg_.clear();
    set_input_enabled(false);
    render_status();

    QPointer<AiChatBubble> self = this;
    // ToolPolicy::NoNavigation — the floating bubble lets the model use tools (so "add SPGI
    // to my watchlist" works), but hides the `navigation` category so the model can't yank
    // the user out of their current screen.
    ai_chat::LlmService::instance().chat_streaming(
        text, hist_copy,
        [self](const QString& chunk, bool done) {
            QMetaObject::invokeMethod(
                qApp,
                [self, chunk, done]() {
                    if (self)
                        self->on_stream_chunk(chunk, done);
                },
                Qt::QueuedConnection);
        },
        ai_chat::LlmService::ToolPolicy::NoNavigation, QString(),
        ai_chat::ChatRoute{QString::fromLatin1(ai_chat::kFeatureQuickChat), QString(), QString()});
}

// Mirror of an AI Chat tab turn that operates the terminal.
void AiChatBubble::on_request_started(const QString& chat_session_id, const QString& user_message) {
    if (chat_session_id.isEmpty() || mirror_engaged_)
        return;
    mirror_session_ = chat_session_id;
    mirror_user_text_ = user_message;
    mirror_chunks_.clear();
}

void AiChatBubble::on_request_chunk(const QString& chat_session_id, const QString& chunk) {
    if (chat_session_id.isEmpty() || chat_session_id != mirror_session_)
        return;
    if (mirror_engaged_)
        on_stream_chunk(chunk, false);
    else if (mirror_chunks_.size() < 5000)
        mirror_chunks_.append(chunk);
}

void AiChatBubble::on_ui_operating(const QString& chat_session_id) {
    if (chat_session_id.isEmpty() || chat_session_id != mirror_session_ || mirror_engaged_ || streaming_)
        return;
    // One bubble follows the turn: the one in the window the user is working in.
    if (QWidget* active = QApplication::activeWindow(); active && active != window())
        return;
    mirror_engaged_ = true;
    add_turn(aic::TurnRole::User, mirror_user_text_);
    tool_steps_.clear();
    chat_history_.push_back({"user", mirror_user_text_}); // a follow-up typed here continues it

    const ai_chat::ModelChoice route =
        ai_chat::ModelRouter::instance().resolve(QString::fromLatin1(ai_chat::kFeatureAiChat));
    aic::Turn a;
    a.role = aic::TurnRole::Assistant;
    a.state = aic::TurnState::Streaming;
    a.provider = route.provider_id();
    a.model = route.model;
    a.route_label = route.label;
    a.when = QDateTime::currentDateTime();
    streaming_turn_ = transcript_->append_turn(a);
    request_started_ms_ = QDateTime::currentMSecsSinceEpoch();
    streaming_ = true;
    error_msg_.clear();
    set_input_enabled(false);

    // What the turn did before it started operating the screens.
    const QStringList backlog = std::exchange(mirror_chunks_, {});
    for (const QString& chunk : backlog)
        on_stream_chunk(chunk, false);

    if (!is_open_)
        open_panel();
    transcript_->scroll_to_bottom();
    render_status();
    LOG_INFO("AiChatBubble", "Following an AI Chat turn that is operating the terminal");
}

void AiChatBubble::update_breathing() {
    if (!bubble_btn_)
        return;
    const bool breathe = streaming_ && !is_open_;
    if (breathe) {
        if (!breath_fx_) {
            breath_fx_ = new QGraphicsOpacityEffect(bubble_btn_);
            bubble_btn_->setGraphicsEffect(breath_fx_);
            breath_anim_ = new QPropertyAnimation(breath_fx_, "opacity", this);
            breath_anim_->setDuration(1800);
            breath_anim_->setKeyValueAt(0.0, 1.0);
            breath_anim_->setKeyValueAt(0.5, 0.35);
            breath_anim_->setKeyValueAt(1.0, 1.0);
            breath_anim_->setEasingCurve(QEasingCurve::InOutSine);
            breath_anim_->setLoopCount(-1);
        }
        breath_fx_->setEnabled(true);
        if (breath_anim_->state() != QAbstractAnimation::Running)
            breath_anim_->start();
        bubble_btn_->setToolTip(tr("The AI is working. Click to follow it."));
    } else if (breath_fx_ && breath_fx_->isEnabled()) {
        breath_anim_->stop();
        breath_fx_->setOpacity(1.0);
        breath_fx_->setEnabled(false); // drawn directly again: no effect cost while idle
        bubble_btn_->setToolTip(tr("Quick Chat (separate from AI Chat tab)"));
    }
}

void AiChatBubble::on_route_notice(const QString& origin_session_id, const QString& feature, const QString& notice) {
    // Ours: the bubble's requests carry no session id and the quick_chat feature.
    if (!streaming_ || !origin_session_id.isEmpty() || feature != QLatin1String(ai_chat::kFeatureQuickChat))
        return;
    add_route_notice(notice);
}

void AiChatBubble::add_route_notice(const QString& text) {
    if (text.isEmpty())
        return;
    add_turn(aic::TurnRole::Notice, text, aic::NoticeKind::Route);
}

void AiChatBubble::append_tool_chunk(const QString& text) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QString& raw : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (const auto rec = ai_chat::decode_tool_done(raw)) {
            for (auto& s : tool_steps_) {
                if (s.state != aic::ToolStep::State::Running || s.label != rec->label)
                    continue;
                s.elapsed_ms = rec->elapsed_ms;
                s.parallel = rec->parallel;
                s.detail = rec->detail;
                s.state = rec->outcome == ai_chat::ToolDoneRecord::Outcome::Failed ? aic::ToolStep::State::Failed
                          : rec->outcome == ai_chat::ToolDoneRecord::Outcome::Backgrounded
                              ? aic::ToolStep::State::Job
                              : aic::ToolStep::State::Ok;
                break;
            }
            continue;
        }
        const QString line = raw.simplified();
        if (line.isEmpty())
            continue;
        aic::ToolStep s;
        s.label = line;
        s.base = line.section(QStringLiteral(" · "), 0, 0);
        s.started_ms = now;
        tool_steps_.append(s);
    }
    if (streaming_turn_ != 0)
        transcript_->set_tool_steps(streaming_turn_, tool_steps_);
}

void AiChatBubble::on_stream_chunk(const QString& chunk, bool done) {
    Q_UNUSED(done)
    if (!streaming_ || streaming_turn_ == 0)
        return;
    // The compact bubble doesn't surface chain-of-thought — reasoning chunks are dropped so
    // the sentinel never leaks into the visible text.
    if (chunk.startsWith(ai_chat::think_stream_prefix()))
        return;
    // "The model moved on to tool calls" is a routing marker, not text: the compact bubble
    // keeps what streamed and lists the tools in its activity line.
    if (chunk.startsWith(QStringLiteral("\x01__TOOL_CALL_CLEAR__")))
        return;
    const QString tool_prefix = ai_chat::tool_stream_prefix();
    if (chunk.startsWith(tool_prefix)) {
        append_tool_chunk(chunk.mid(tool_prefix.size()));
        return;
    }
    if (!chunk.isEmpty())
        transcript_->stream_append(streaming_turn_, chunk);
}

void AiChatBubble::on_streaming_done(ai_chat::LlmResponse response) {
    // finished_streaming() is process-wide: it also fires for the AI Chat tab's requests
    // (which carry a session id; the bubble sends none). Only react to a response that is
    // ours: we are waiting for one and it has no session id.
    // The exception is the tab turn this bubble is following (see "Mirror").
    const bool watched = !response.origin_session_id.isEmpty() && response.origin_session_id == mirror_session_;
    const bool mirrored = watched && mirror_engaged_;
    if (watched) {
        mirror_session_.clear();
        mirror_user_text_.clear();
        mirror_chunks_.clear();
        mirror_engaged_ = false;
    }
    if (!mirrored && (!streaming_ || !response.origin_session_id.isEmpty()))
        return;

    streaming_ = false;
    set_input_enabled(true);
    const quint64 turn_id = streaming_turn_;
    streaming_turn_ = 0;
    for (auto& s : tool_steps_)
        if (s.state == aic::ToolStep::State::Running)
            s.state = aic::ToolStep::State::Ok;

    aic::Turn t;
    if (const aic::Turn* cur = transcript_->turn(turn_id))
        t = *cur;
    t.tools = tool_steps_;
    t.duration_ms = request_started_ms_ > 0 ? QDateTime::currentMSecsSinceEpoch() - request_started_ms_ : -1;
    if (!response.provider.isEmpty()) {
        t.provider = response.provider;
        t.model = response.model;
    }
    if (!response.route_label.isEmpty())
        t.route_label = response.route_label;
    const QString partial = transcript_->stream_text(turn_id).trimmed();

    // The harness was unavailable and the setting is "ask" / "fail": say so, with the way
    // to Settings, instead of "Error: …".
    if (!response.success && !response.cancelled && !response.unavailable_notice.isEmpty()) {
        if (partial.isEmpty() && t.tools.isEmpty()) {
            transcript_->remove_turn(turn_id);
        } else {
            t.state = aic::TurnState::Stopped;
            t.text = partial;
            transcript_->update_turn(turn_id, t);
        }
        add_route_notice(response.unavailable_notice + QStringLiteral("\n\n") +
                         tr("Choose another model with the badge above, or set “When a harness is unavailable” to "
                            "fall back automatically."));
        if (!is_open_)
            update_unread(1);
        if (voice_mode_)
            start_listening();
        render_status();
        return;
    }
    // Out of Fincept credits / sign-in rejected: an actionable notice instead of "Error: …".
    if (!response.success && ai_chat::classify_llm_error(response.error) != ai_chat::LlmErrorKind::Other) {
        if (partial.isEmpty() && t.tools.isEmpty()) {
            transcript_->remove_turn(turn_id);
        } else {
            t.state = aic::TurnState::Stopped;
            t.text = partial;
            transcript_->update_turn(turn_id, t);
        }
        add_fincept_notice(response.error);
        if (!is_open_)
            update_unread(1);
        if (voice_mode_)
            start_listening();
        render_status();
        return;
    }

    const QString content = !response.content.trimmed().isEmpty() ? response.content : partial;
    if (response.success) {
        if (content.trimmed().isEmpty()) {
            t.state = aic::TurnState::Empty;
            t.text.clear();
        } else {
            t.state = aic::TurnState::Done;
            t.text = content;
        }
        t.tokens = response.total_tokens;
        t.cost_usd = response.cost_usd;
        // A harness answer: who answered and who pays (never Fincept credits).
        if (ai_chat::provider_is_harness(response.provider))
            t.billing_note = response.billing_note;
    } else {
        t.state = response.cancelled ? aic::TurnState::Stopped : aic::TurnState::Failed;
        t.text = partial;
        t.error = response.error.isEmpty() ? tr("The request failed with no reason given.") : response.error;
    }
    transcript_->update_turn(turn_id, t);
    transcript_->scroll_to_bottom();

    // Only a real answer belongs in the history that is re-sent with the next question —
    // "Error: HTTP 401…" is not something the model said.
    if (response.success && !content.trimmed().isEmpty())
        chat_history_.push_back({"assistant", content});

    if (!is_open_)
        update_unread(1);

    if (voice_mode_ && response.success)
        speak_text(content);
    else if (voice_mode_)
        start_listening();
    render_status();
}

// ── Fincept credits / sign-in notice ──────────────────────────────────────────
void AiChatBubble::add_fincept_notice(const QString& error) {
    const bool credits = ai_chat::classify_llm_error(error) == ai_chat::LlmErrorKind::FinceptOutOfCredits;
    QString text = credits ? tr("**You're out of Fincept credits.** Top up, or add your own provider API key in "
                                "**Settings → LLM Configuration**.")
                           : tr("**Fincept did not accept your sign-in.** Sign in again, or add your own provider "
                                "API key in **Settings → LLM Configuration**.");
    if (credits) {
        // What the plan would change, from the server's plan table (empty without one).
        const QString plan_note = ui::plan::out_of_credits_note(QStringLiteral("ai"));
        if (!plan_note.isEmpty())
            text += QStringLiteral("\n\n") + plan_note;
    }
    add_turn(aic::TurnRole::Notice, text, credits ? aic::NoticeKind::Credits : aic::NoticeKind::SignIn);
}

void AiChatBubble::set_input_enabled(bool enabled) {
    send_btn_->setEnabled(enabled);
    input_box_->setEnabled(enabled);
    // Mic stays clickable so the user can cancel a stuck listen.
    mic_btn_->setEnabled(enabled || is_listening_);
    send_btn_->setText(enabled ? tr("SEND") : QStringLiteral("…"));
}

void AiChatBubble::update_unread(int delta) {
    unread_count_ += delta;
    if (unread_count_ > 0) {
        unread_badge_->setText(unread_count_ > 9 ? QStringLiteral("9+") : QString::number(unread_count_));
        unread_badge_->show();
    } else {
        unread_badge_->hide();
    }
}

// ── Voice input (STT) ────────────────────────────────────────────────────────
void AiChatBubble::on_toggle_mic() {
    LOG_DEBUG("AiChatBubble", QString("Mic toggled — listening=%1 speaking=%2 voice_mode=%3")
                                  .arg(is_listening_)
                                  .arg(is_speaking_)
                                  .arg(voice_mode_));
    if (is_listening_)
        stop_listening();
    else
        start_listening();
}

void AiChatBubble::start_listening() {
    LOG_DEBUG("AiChatBubble",
              QString("start_listening — listening=%1 speaking=%2").arg(is_listening_).arg(is_speaking_));
    if (is_listening_ || is_speaking_)
        return;
    // Pause the clap detector while STT owns the mic.
    if (services::ClapDetectorService::instance().is_active())
        services::ClapDetectorService::instance().stop();
    services::SpeechService::instance().start_listening();
}

void AiChatBubble::stop_listening() {
    LOG_DEBUG("AiChatBubble", "stop_listening — forwarding to SpeechService");
    services::SpeechService::instance().stop_listening();
}

void AiChatBubble::on_stt_listening_changed(bool active) {
    is_listening_ = active;
    set_mic_listening_visual(active);
    if (active)
        error_msg_.clear();
    render_status();

    // STT released the mic — bring the clap detector back if still enabled.
    if (!active && !is_speaking_ && !streaming_ && services::ClapDetectorService::is_enabled_in_config() &&
        !services::ClapDetectorService::instance().is_active()) {
        services::ClapDetectorService::instance().start();
    }
}

void AiChatBubble::on_transcription(const QString& text) {
    LOG_DEBUG("AiChatBubble", QString("Transcription: len=%1 voice_mode=%2").arg(text.length()).arg(voice_mode_));
    if (text.isEmpty())
        return;
    input_box_->setPlainText(text);
    if (!voice_mode_)
        stop_listening();
    else
        on_send();
    render_status();
}

void AiChatBubble::on_stt_error(const QString& message) {
    LOG_WARN("AiChatBubble", QString("STT error: %1").arg(message));
    error_msg_ = message;
    render_status();
}

// ── Voice mode (continuous loop) ─────────────────────────────────────────────
void AiChatBubble::on_toggle_voice_mode() {
    voice_mode_ = voice_mode_btn_->isChecked();
    LOG_DEBUG("AiChatBubble", QString("Voice mode -> %1").arg(voice_mode_));
    if (voice_mode_) {
        if (!is_open_)
            open_panel();
        QTimer::singleShot(300, this, &AiChatBubble::start_listening);
    } else {
        stop_listening();
        stop_tts();
    }
    render_status();
}

// ── TTS ───────────────────────────────────────────────────────────────────────
void AiChatBubble::speak_text(const QString& text) {
    LOG_DEBUG("AiChatBubble", QString("speak_text len=%1").arg(text.length()));
    if (text.isEmpty())
        return;
    // Strip markdown for TTS — engines speak raw text.
    QString clean = text;
    clean.remove(QRegularExpression(R"(\*\*|__|~~|```[^`]*```|`[^`]*`)"));
    clean.remove(QRegularExpression(R"(#{1,6} )"));
    clean = clean.trimmed();
    if (clean.isEmpty()) {
        if (voice_mode_)
            start_listening();
        return;
    }
    services::TtsService::instance().speak(clean.left(1200));
}

void AiChatBubble::stop_tts() {
    LOG_DEBUG("AiChatBubble", "stop_tts");
    services::TtsService::instance().stop();
    is_speaking_ = false;
    render_status();
}

void AiChatBubble::on_stop_speech() {
    stop_tts();
    if (voice_mode_ && !is_listening_ && !streaming_)
        QTimer::singleShot(200, this, &AiChatBubble::start_listening);
}

// ── Status strip rendering (single source of truth) ──────────────────────────
void AiChatBubble::render_status() {
    update_breathing();
    if (!status_strip_)
        return;
    Status s = Status::Idle;
    if (!error_msg_.isEmpty())
        s = Status::Error;
    else if (is_speaking_)
        s = Status::Speaking;
    else if (is_listening_)
        s = Status::Listening;
    else if (streaming_)
        s = Status::Thinking;

    QString tone, text;
    bool show_stop = false;
    switch (s) {
        case Status::Idle:
            status_strip_->setVisible(false);
            return;
        case Status::Listening:
            tone = QStringLiteral("ok");
            text = tr("Listening — speak now");
            break;
        case Status::Thinking:
            tone = QStringLiteral("accent");
            text = mirror_engaged_ ? tr("AI is working in the terminal. Follow it here.") : tr("AI is thinking…");
            break;
        case Status::Speaking:
            tone = QStringLiteral("warn");
            text = tr("AI speaking…");
            show_stop = true;
            break;
        case Status::Error:
            tone = QStringLiteral("danger");
            text = error_msg_;
            break;
    }
    status_strip_->setVisible(true);
    // Tone through a property of the shared sheet (no per-state stylesheet).
    if (status_lbl_->property("tone").toString() != tone) {
        status_lbl_->setProperty("tone", tone);
        status_lbl_->style()->unpolish(status_lbl_);
        status_lbl_->style()->polish(status_lbl_);
    }
    status_lbl_->setText(text);
    status_lbl_->setToolTip(text);
    stop_speech_btn_->setVisible(show_stop);
}

void AiChatBubble::set_mic_listening_visual(bool on) {
    if (!mic_btn_)
        return;
    mic_btn_->setProperty("active", on);
    // Re-polish so the [active="true"] selector takes effect.
    mic_btn_->style()->unpolish(mic_btn_);
    mic_btn_->style()->polish(mic_btn_);
    mic_btn_->update();
}

// ── i18n ─────────────────────────────────────────────────────────────────────
void AiChatBubble::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AiChatBubble::retranslateUi() {
    if (bubble_btn_)
        bubble_btn_->setToolTip(tr("Quick Chat (separate from AI Chat tab)"));
    if (hdr_title_lbl_)
        hdr_title_lbl_->setText(tr("QUICK CHAT"));
    if (hdr_subtitle_lbl_)
        hdr_subtitle_lbl_->setText(tr("Not saved · separate from AI Chat tab"));
    if (voice_mode_btn_) {
        voice_mode_btn_->setText(tr("VOICE"));
        voice_mode_btn_->setToolTip(tr("Toggle continuous voice conversation (auto-listen + auto-speak)"));
    }
    if (new_btn_) {
        new_btn_->setText(tr("CLEAR"));
        new_btn_->setToolTip(tr("Clear this chat (Ctrl+L)"));
    }
    if (close_btn_)
        close_btn_->setToolTip(tr("Close (Esc)"));
    if (input_box_)
        input_box_->setPlaceholderText(tr("Ask anything…  (Enter to send, Shift+Enter for newline)"));
    if (mic_btn_) {
        mic_btn_->setText(tr("MIC"));
        mic_btn_->setToolTip(tr("Push-to-talk: click to dictate one message"));
    }
    if (send_btn_)
        send_btn_->setToolTip(tr("Send (Enter)"));
    if (stop_speech_btn_) {
        stop_speech_btn_->setText(tr("STOP"));
        stop_speech_btn_->setToolTip(tr("Stop speaking"));
    }
    if (welcome_heading_)
        welcome_heading_->setText(tr("How can I help?"));
    if (welcome_sub_)
        welcome_sub_->setText(tr("Ask a quick question. Nothing here is saved.\n"
                                 "For long-form chats use the AI Chat tab."));
    // Re-run helpers that compose live-state strings so they pick up the new language.
    set_input_enabled(!streaming_);
    render_status();
    welcome_llm_state_ = -1; // the setup texts are re-applied on the next check
    if (is_open_)
        refresh_welcome_setup();
}

// ── Event filter (bubble click, Enter/Shift+Enter, Esc, Ctrl+L) ──────────────
bool AiChatBubble::eventFilter(QObject* obj, QEvent* e) {
    if (obj == bubble_btn_ && e->type() == QEvent::MouseButtonRelease) {
        if (is_open_)
            close_panel();
        else
            open_panel();
        return true;
    }
    if (obj == input_box_ && e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        // Enter sends (main and numpad keys); Shift+Enter inserts a newline.
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & Qt::ShiftModifier)) {
            on_send();
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            close_panel();
            return true;
        }
        if (ke->key() == Qt::Key_L && (ke->modifiers() & Qt::ControlModifier)) {
            on_clear_chat();
            return true;
        }
    }
    if (obj == parentWidget() && e->type() == QEvent::Resize) {
        reposition();
        return false;
    }
    return QWidget::eventFilter(obj, e);
}

} // namespace fincept
