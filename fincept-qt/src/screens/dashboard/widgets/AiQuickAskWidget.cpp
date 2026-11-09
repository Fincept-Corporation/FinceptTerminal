#include "screens/dashboard/widgets/AiQuickAskWidget.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/llm/LlmErrors.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPointer>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens::widgets {

namespace {

// Prefix of the per-question chat_session_id (see the header).
const QString kQuickAskSessionPrefix = QStringLiteral("dashboard-quick-ask-");
// LlmService::chat_streaming's answer when no provider is set at all.
const QString kQuickAskNoProviderError = QStringLiteral("No LLM provider configured");
// In-band sentinel: the model switched to tool calls, drop what streamed so far.
const QString kQuickAskToolCallClear = QStringLiteral("\x01__TOOL_CALL_CLEAR__");
constexpr int kQuickAskMaxQuestionChars = 2000;

void quick_ask_open_screen(const QString& screen_id) {
    EventBus::instance().publish(QStringLiteral("nav.switch_screen"), QVariantMap{{"screen_id", screen_id}});
}

} // namespace

AiQuickAskWidget::AiQuickAskWidget(const QJsonObject& /*cfg*/, QWidget* parent)
    : BaseWidget(tr("AI QUICK ASK"), parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // One stylesheet on this root (objectName selectors), set in apply_styles().
    root_ = new QWidget(this);
    root_->setObjectName(QStringLiteral("quickAskRoot"));
    auto* rl = new QVBoxLayout(root_);
    rl->setContentsMargins(6, 6, 6, 6);
    rl->setSpacing(6);

    auto* ask_row = new QHBoxLayout;
    ask_row->setContentsMargins(0, 0, 0, 0);
    ask_row->setSpacing(4);
    input_ = new QLineEdit(root_);
    input_->setObjectName(QStringLiteral("quickAskInput"));
    input_->setMaxLength(kQuickAskMaxQuestionChars);
    input_->setClearButtonEnabled(true);
    connect(input_, &QLineEdit::returnPressed, this, &AiQuickAskWidget::on_ask);
    ask_row->addWidget(input_, 1);

    ask_btn_ = new QPushButton(root_);
    ask_btn_->setObjectName(QStringLiteral("quickAskButton"));
    ask_btn_->setCursor(Qt::PointingHandCursor);
    connect(ask_btn_, &QPushButton::clicked, this, &AiQuickAskWidget::on_ask);
    ask_row->addWidget(ask_btn_);
    rl->addLayout(ask_row);

    answer_ = new QTextBrowser(root_);
    answer_->setObjectName(QStringLiteral("quickAskAnswer"));
    // Model output is untrusted text: never navigate on a link click.
    answer_->setOpenLinks(false);
    answer_->setOpenExternalLinks(false);
    rl->addWidget(answer_, 1);

    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(0, 0, 0, 0);
    foot->setSpacing(6);
    status_label_ = new QLabel(root_);
    status_label_->setObjectName(QStringLiteral("quickAskStatus"));
    status_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    foot->addWidget(status_label_, 1);

    settings_btn_ = new QPushButton(root_);
    settings_btn_->setObjectName(QStringLiteral("quickAskLink"));
    settings_btn_->setCursor(Qt::PointingHandCursor);
    settings_btn_->setFlat(true);
    settings_btn_->hide();
    connect(settings_btn_, &QPushButton::clicked, this, []() { quick_ask_open_screen(QStringLiteral("settings")); });
    foot->addWidget(settings_btn_);

    // No API hands a prompt to the AI Chat screen, so this just opens it.
    continue_btn_ = new QPushButton(root_);
    continue_btn_->setObjectName(QStringLiteral("quickAskLink"));
    continue_btn_->setCursor(Qt::PointingHandCursor);
    continue_btn_->setFlat(true);
    connect(continue_btn_, &QPushButton::clicked, this, []() { quick_ask_open_screen(QStringLiteral("ai_chat")); });
    foot->addWidget(continue_btn_);
    rl->addLayout(foot);

    vl->addWidget(root_, 1);

    // finished_streaming() is process-wide; on_finished() keeps only our reply.
    auto& llm = ai_chat::LlmService::instance();
    connect(&llm, &ai_chat::LlmService::finished_streaming, this, &AiQuickAskWidget::on_finished);
    connect(&llm, &ai_chat::LlmService::config_changed, this, [this]() { refresh_config_state(); });
    // Title-bar refresh: nothing to re-fetch — re-check the LLM configuration.
    connect(this, &BaseWidget::refresh_requested, this, [this]() { refresh_config_state(); });

    AiQuickAskWidget::retranslateUi(); // qualified: no dynamic dispatch inside a constructor
    apply_styles();
}

AiQuickAskWidget::~AiQuickAskWidget() {
    cancel_in_flight();
}

void AiQuickAskWidget::apply_styles() {
    root_->setStyleSheet(
        QString("QWidget#quickAskRoot{background:transparent;}"
                "QLineEdit#quickAskInput{background:%1;color:%2;border:1px solid %3;padding:3px 6px;"
                "font-size:11px;selection-background-color:%4;selection-color:%5;}"
                "QLineEdit#quickAskInput:focus{border-color:%6;}"
                "QPushButton#quickAskButton{background:%7;color:%4;border:1px solid %8;padding:3px 12px;"
                "font-size:10px;font-weight:bold;}"
                "QPushButton#quickAskButton:hover{background:%4;color:%5;}"
                "QTextBrowser#quickAskAnswer{background:%1;color:%2;border:1px solid %3;padding:4px;"
                "font-size:11px;}"
                "QLabel#quickAskStatus{color:%9;font-size:9px;background:transparent;}"
                "QLabel#quickAskStatus[error=\"true\"]{color:%10;}"
                "QPushButton#quickAskLink{color:%4;background:transparent;border:none;padding:0px 2px;"
                "font-size:9px;font-weight:bold;}"
                "QPushButton#quickAskLink:hover{color:%2;text-decoration:underline;}"
                "QScrollBar:vertical{width:5px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%6;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
                 ui::colors::TEXT_ON_ACCENT(), ui::colors::BORDER_MED(), ui::colors::ACCENT_BG(),
                 ui::colors::AMBER_DIM(), ui::colors::TEXT_TERTIARY())
            .arg(ui::colors::NEGATIVE()));
}

void AiQuickAskWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    refresh_config_state();
}

void AiQuickAskWidget::refresh_config_state() {
    const bool configured = ai_chat::LlmService::instance().is_configured();
    if (!configured) {
        // A reply already in flight still lands; only an idle tile shows the notice.
        if (pending_session_.isEmpty())
            set_state(AskState::NotConfigured);
        return;
    }
    if (state_ == AskState::NotConfigured) {
        answer_->clear();
        set_state(AskState::Idle);
    } else {
        update_status_text(); // the active model may have changed
    }
}

void AiQuickAskWidget::on_ask() {
    const QString question = input_->text().trimmed();
    if (question.isEmpty())
        return;
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        set_state(AskState::NotConfigured);
        return;
    }

    cancel_in_flight(); // a new question supersedes the one in flight
    const quint64 gen = request_gen_;
    pending_session_ = kQuickAskSessionPrefix + QUuid::createUuid().toString(QUuid::WithoutBraces);
    req_provider_ = llm.active_provider();
    req_model_ = llm.active_model();
    last_tokens_ = 0;
    last_error_.clear();
    stream_buffer_.clear();
    rendered_len_ = 0;
    answer_->clear();
    set_state(AskState::Thinking);
    LOG_DEBUG("AiQuickAsk", QString("Question sent (%1 chars)").arg(question.size())); // never the text (P14)

    // Plain chat: ToolPolicy::None — no MCP tools, no UI control. on_chunk runs
    // on a worker thread; hop to the GUI thread and drop it if this tile is gone
    // or the question was superseded (P8).
    QPointer<AiQuickAskWidget> self = this;
    llm.chat_streaming(
        question, {},
        [self, gen](const QString& chunk, bool /*done*/) {
            QMetaObject::invokeMethod(
                qApp,
                [self, gen, chunk]() {
                    if (self)
                        self->on_chunk(gen, chunk);
                },
                Qt::QueuedConnection);
        },
        ai_chat::LlmService::ToolPolicy::None, pending_session_);
}

void AiQuickAskWidget::cancel_in_flight() {
    ++request_gen_; // chunks of the old request are dropped from here on
    if (pending_session_.isEmpty())
        return;
    // Only ever our own session id — never cancels the AI Chat tab's request.
    ai_chat::LlmService::instance().cancel_active_request(pending_session_);
    pending_session_.clear();
}

void AiQuickAskWidget::on_chunk(quint64 gen, const QString& chunk) {
    if (gen != request_gen_ || pending_session_.isEmpty() || chunk.isEmpty())
        return;
    // Reasoning and tool-progress channels are not part of the answer.
    if (chunk.startsWith(ai_chat::think_stream_prefix()) || chunk.startsWith(ai_chat::tool_stream_prefix()))
        return;
    if (chunk.startsWith(kQuickAskToolCallClear)) {
        stream_buffer_.clear();
        rendered_len_ = 0;
        answer_->clear();
        return;
    }
    stream_buffer_ += chunk;
    // Coalesce a burst of chunks into one append on the next event-loop turn.
    schedule_render([this]() { render_stream(); });
}

void AiQuickAskWidget::render_stream() {
    if (state_ != AskState::Thinking || rendered_len_ >= stream_buffer_.size())
        return;
    QTextCursor cursor(answer_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(stream_buffer_.mid(rendered_len_));
    rendered_len_ = stream_buffer_.size();
    answer_->verticalScrollBar()->setValue(answer_->verticalScrollBar()->maximum());
}

void AiQuickAskWidget::on_finished(const ai_chat::LlmResponse& response) {
    if (pending_session_.isEmpty() || response.origin_session_id != pending_session_)
        return; // the AI Chat tab's, the Quick Chat bubble's, or a superseded question's
    pending_session_.clear();
    ++request_gen_; // nothing more may append to this answer

    if (response.cancelled) {
        set_state(AskState::Cancelled);
        return;
    }
    if (!response.success) {
        if (response.error == kQuickAskNoProviderError) {
            set_state(AskState::NotConfigured);
            return;
        }
        last_error_ = response.error;
        render_failure();
        set_state(AskState::Failed);
        return;
    }

    QString text = stream_buffer_.trimmed().isEmpty() ? response.content : stream_buffer_;
    if (text.trimmed().isEmpty())
        text = tr("*The model returned an empty answer.*");
    // Final render as Markdown; raw HTML in model output is not interpreted.
    answer_->document()->setMarkdown(
        text, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub | QTextDocument::MarkdownNoHTML));
    last_tokens_ = response.total_tokens;
    set_state(AskState::Answered);
}

void AiQuickAskWidget::set_state(AskState state) {
    state_ = state;
    if (state == AskState::NotConfigured)
        render_not_configured();
    const bool needs_settings =
        state == AskState::NotConfigured ||
        (state == AskState::Failed && ai_chat::classify_llm_error(last_error_) != ai_chat::LlmErrorKind::Other);
    settings_btn_->setVisible(needs_settings);
    update_status_text();
}

void AiQuickAskWidget::update_status_text() {
    QString text;
    bool error = false;
    const auto model_meta = [](const QString& provider, const QString& model) {
        if (provider.isEmpty())
            return QString();
        return model.isEmpty() ? provider : provider + QStringLiteral(" / ") + model;
    };
    switch (state_) {
        case AskState::Idle: {
            auto& llm = ai_chat::LlmService::instance();
            const QString meta = model_meta(llm.active_provider(), llm.active_model());
            text = meta.isEmpty() ? QString() : tr("Model: %1").arg(meta);
            break;
        }
        case AskState::NotConfigured:
            text = tr("AI not configured");
            error = true;
            break;
        case AskState::Thinking:
            text = tr("Thinking…");
            break;
        case AskState::Answered:
            text = model_meta(req_provider_, req_model_);
            if (last_tokens_ > 0)
                text += (text.isEmpty() ? QString() : QStringLiteral("  ·  ")) + tr("%1 tok").arg(last_tokens_);
            break;
        case AskState::Failed:
            text = tr("Request failed");
            error = true;
            break;
        case AskState::Cancelled:
            text = tr("Cancelled");
            break;
    }
    status_label_->setText(text);
    status_label_->setToolTip(text);
    if (status_label_->property("error").toBool() != error) {
        status_label_->setProperty("error", error);
        status_label_->style()->unpolish(status_label_);
        status_label_->style()->polish(status_label_);
    }
}

void AiQuickAskWidget::render_not_configured() {
    answer_->setPlainText(tr("No AI model is configured yet.\n\n"
                             "Open Settings › LLM Configuration to add a provider and its API key "
                             "(or choose the Fincept provider), then ask again."));
}

void AiQuickAskWidget::render_failure() {
    QString msg;
    switch (ai_chat::classify_llm_error(last_error_)) {
        case ai_chat::LlmErrorKind::FinceptOutOfCredits:
            msg = tr("You're out of Fincept credits. Top up, or add your own provider API key in "
                     "Settings › LLM Configuration.");
            break;
        case ai_chat::LlmErrorKind::FinceptAuth:
            msg = tr("Fincept did not accept your sign-in. Sign in again, or add your own provider API key in "
                     "Settings › LLM Configuration.");
            break;
        case ai_chat::LlmErrorKind::Other:
            msg = tr("The AI request failed: %1").arg(last_error_);
            break;
    }
    // Keep whatever had streamed before the failure; put the error below it.
    const QString partial = stream_buffer_.trimmed();
    answer_->setPlainText(partial.isEmpty() ? msg : partial + QStringLiteral("\n\n— ") + msg);
}

void AiQuickAskWidget::on_theme_changed() {
    apply_styles();
}

void AiQuickAskWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("AI QUICK ASK"));
    input_->setPlaceholderText(tr("Ask a quick question… (Enter to send)"));
    input_->setAccessibleName(tr("Question for the AI"));
    ask_btn_->setText(tr("ASK"));
    ask_btn_->setToolTip(tr("Send the question to the configured AI model"));
    ask_btn_->setAccessibleName(tr("Ask the AI"));
    answer_->setAccessibleName(tr("AI answer"));
    settings_btn_->setText(tr("Open Settings"));
    settings_btn_->setToolTip(tr("Configure an AI provider in Settings › LLM Configuration"));
    settings_btn_->setAccessibleName(tr("Open Settings"));
    continue_btn_->setText(tr("Continue in AI Chat →"));
    continue_btn_->setToolTip(tr("Open the AI Chat screen"));
    continue_btn_->setAccessibleName(tr("Continue in AI Chat"));
    if (state_ == AskState::NotConfigured)
        render_not_configured();
    else if (state_ == AskState::Failed)
        render_failure();
    update_status_text();
}

} // namespace fincept::screens::widgets
