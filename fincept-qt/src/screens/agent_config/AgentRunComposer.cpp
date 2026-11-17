// AgentRunComposer.cpp — NEW RUN section (see the header).

#include "screens/agent_config/AgentRunComposer.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentPromptTemplates.h"
#include "screens/agent_config/HarnessRunOptionsWidget.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace agent_run_composer_detail {

/// The strings came from the Workbench's RUN tab: keep their translation context.
QString arc_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::AgentRunPanel", text);
}

} // namespace agent_run_composer_detail

using namespace agent_run_composer_detail;

AgentRunComposer::AgentRunComposer(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // Narrower than its content, the composer scrolls sideways instead of hiding RUN.
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    root->addWidget(scroll);

    auto* box = new QWidget(scroll);
    box->setObjectName(QStringLiteral("awbComposer"));
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(6);

    title_ = new QLabel(box);
    title_->setObjectName(QStringLiteral("awbSection"));
    v->addWidget(title_);

    options_ = new HarnessRunOptionsWidget(HarnessRunOptionsWidget::Purpose::Composer, box);
    connect(options_, &HarnessRunOptionsWidget::run_requested, this, &AgentRunComposer::on_run);
    connect(options_, &HarnessRunOptionsWidget::harness_changed, this, [this](const QString&) { refresh_run_button(); });
    connect(options_, &HarnessRunOptionsWidget::options_changed, this, &AgentRunComposer::state_changed);
    v->addWidget(options_, 1);

    auto* run_row = new QHBoxLayout;
    hint_ = new QLabel(box);
    hint_->setObjectName(QStringLiteral("awbHint"));
    hint_->setWordWrap(true);
    run_row->addWidget(hint_, 1);
    run_btn_ = new QPushButton(box);
    run_btn_->setObjectName(QStringLiteral("awbPrimaryBtn"));
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setProperty("fincept_agent_risk", "spend"); // agents: starts a coding-agent harness run (model quota)
    connect(run_btn_, &QPushButton::clicked, this, &AgentRunComposer::on_run);
    run_row->addWidget(run_btn_, 0, Qt::AlignBottom);
    v->addLayout(run_row);
    scroll->setWidget(box);

    retranslate();
    // Harness state decides whether RUN is enabled.
    connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::status_changed, this,
            [this](const QString&) { refresh_run_button(); });
}

void AgentRunComposer::retranslate() {
    title_->setText(arc_tr("NEW AGENT RUN"));
    run_btn_->setText(arc_tr("RUN  ▶"));
    refresh_run_button();
    refresh_hint();
}

void AgentRunComposer::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentRunComposer::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (conns_.isEmpty()) {
        auto& svc = ca::CodingAgentService::instance();
        auto refresh = [this]() { refresh_hint(); };
        conns_ << connect(&svc, &ca::CodingAgentService::run_added, this, refresh);
        conns_ << connect(&svc, &ca::CodingAgentService::run_finished, this, refresh);
        conns_ << connect(&svc, &ca::CodingAgentService::runs_reset, this, refresh);
    }
    refresh_run_button();
    refresh_hint();
}

void AgentRunComposer::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentRunComposer::refresh_run_button() {
    QString why;
    const bool ok = options_->harness_runnable(&why);
    run_btn_->setEnabled(ok);
    run_btn_->setToolTip(ok ? arc_tr("Start the agent (Ctrl+Enter)") : why);
}

void AgentRunComposer::refresh_hint() {
    auto& svc = ca::CodingAgentService::instance();
    hint_->setText(arc_tr("Ctrl+Enter to run · %1 running · %2 queued · max %3 at once")
                       .arg(svc.running_count())
                       .arg(svc.queued_count())
                       .arg(svc.max_concurrent()));
}

void AgentRunComposer::set_prompt(const QString& prompt) {
    options_->set_prompt(prompt);
}

void AgentRunComposer::open_templates() {
    options_->open_templates();
}

void AgentRunComposer::insert_template(int index) {
    options_->insert_template(index);
}

void AgentRunComposer::focus_prompt() {
    options_->prompt_edit()->setFocus();
}

void AgentRunComposer::on_run() {
    QString why;
    if (!options_->harness_runnable(&why)) {
        QMessageBox::information(this, tr("New agent run"), why);
        return;
    }
    const HarnessRunOptions o = options_->options();
    if (o.prompt.isEmpty()) {
        options_->prompt_edit()->setFocus();
        return;
    }
    if (o.prompt.contains(agent_placeholder_regex())) {
        const auto answer = QMessageBox::question(
            this, tr("New agent run"), arc_tr("The prompt still contains placeholders such as <TICKER>. Run it anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            options_->select_first_placeholder();
            options_->prompt_edit()->setFocus();
            return;
        }
    }
    ca::RunRequest req;
    req.harness = o.harness;
    req.prompt = o.prompt;
    req.model = o.model;
    req.effort = o.effort;
    req.permission = o.permission;
    req.use_fincept_tools = o.tools;
    req.max_budget_usd = o.budget_usd;
    req.time_limit_s = o.time_limit_s;
    req.max_turns = o.max_turns;
    req.workdir = o.workdir;
    req.origin = QStringLiteral("user");

    QString err;
    const QString id = ca::CodingAgentService::instance().start_run(req, &err);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("New agent run"), err);
        return;
    }
    LOG_INFO("AgentConfig", QString("Harness run started from the composer run=%1 harness=%2 prompt_chars=%3")
                                .arg(id.left(8), o.harness)
                                .arg(o.prompt.size()));
    emit run_started(id);
}

QVariantMap AgentRunComposer::save_state() const {
    return options_->save_state();
}

void AgentRunComposer::restore_state(const QVariantMap& state) {
    options_->restore_state(state);
    refresh_run_button();
}

} // namespace fincept::screens
