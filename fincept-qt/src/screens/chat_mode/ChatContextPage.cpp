// ChatContextPage.cpp — the CONTEXT page of Chat Mode's inspector (2026-11-17).
//
// Five sections under uppercase PanelHeader strips in one vertical scroll area:
// CONTEXT (the composer's chips with DETACH / ATTACH, and what the shown run was
// sent with), TOOLS (the terminal tool bridge and the cloud agent's MCP servers),
// FILES (the run's artefacts; a click opens the FILES page), BUDGET & LIMITS and
// RUN STATS. Widgets are built once; refreshes only change texts (the chip and file
// rows, a handful of widgets, are rebuilt when their data changes). A hidden page
// only marks itself dirty and catches up when shown.

#include "screens/chat_mode/ChatContextPage.h"

#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ComposerContextBar.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "screens/chat_mode/TerminalToolBridge.h"
#include "services/agent_tasks/ArtefactService.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace chat_context_page_detail {

constexpr int kCcpTickMs = 1000;
constexpr int kCcpMaxFileRows = 30;
constexpr int kCcpMaxMcpRows = 12;

QString ccp_style() {
    return QStringLiteral(
               "#chatContextPage{background:%1;}"
               "#chatContextPage QScrollArea{background:%1;border:none;}"
               "#chatContextPage QWidget#ccpContent{background:%1;}"
               "#chatContextPage QWidget#ccpBody{background:%1;}"
               "#chatContextPage QLabel{background:transparent;border:none;color:%2;font-size:%8px;}"
               "#chatContextPage QLabel#ccpHint{color:%3;}"
               "#chatContextPage QLabel#ccpSub{color:%3;font-size:%9px;font-weight:700;letter-spacing:1px;}"
               "#chatContextPage QLabel#ccpKey{color:%3;}"
               "#chatContextPage QLabel#ccpValue{color:%4;}"
               "#chatContextPage QLabel[tone=\"ok\"]{color:%10;}"
               "#chatContextPage QLabel[tone=\"warn\"]{color:%11;}"
               "#chatContextPage QLabel[tone=\"danger\"]{color:%12;}"
               "#chatContextPage QLabel[tone=\"info\"]{color:%13;}"
               "#chatContextPage QLabel[tone=\"dim\"]{color:%3;}"
               "#chatContextPage QFrame#ccpChip{background:%5;border:1px solid %6;}"
               "#chatContextPage QFrame#ccpChip QLabel{color:%4;}"
               "#chatContextPage QFrame#ccpChip[sent=\"true\"]{background:transparent;}"
               "#chatContextPage QToolButton#ccpDetach{background:transparent;border:none;color:%2;padding:0 3px;}"
               "#chatContextPage QToolButton#ccpDetach:hover{color:%12;}"
               "#chatContextPage QToolButton#ccpDetach:focus{color:%12;border:1px solid %7;}"
               "#chatContextPage QPushButton#ccpAction{background:%5;color:%2;border:1px solid %6;padding:2px 8px;"
               "font-size:%9px;font-weight:700;}"
               "#chatContextPage QPushButton#ccpAction:hover{color:%4;border-color:%7;}"
               "#chatContextPage QPushButton#ccpAction:focus{border-color:%7;}"
               "#chatContextPage QPushButton#ccpAction:disabled{color:%3;border-color:%6;background:transparent;}"
               "#chatContextPage QPushButton#ccpFile{background:transparent;color:%4;border:none;"
               "border-bottom:1px solid %6;text-align:left;padding:3px 2px;font-size:%8px;}"
               "#chatContextPage QPushButton#ccpFile:hover{background:%14;}"
               "#chatContextPage QPushButton#ccpFile:focus{border:1px solid %7;}"
               "#chatContextPage QScrollBar:vertical{background:transparent;width:6px;}"
               "#chatContextPage QScrollBar::handle:vertical{background:%6;min-height:20px;}"
               "#chatContextPage QScrollBar::add-line:vertical,#chatContextPage QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_TERTIARY(),
             ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
             ui::fonts::px(-3), ui::fonts::px(-4))
        .arg(ui::colors::SUCCESS(), ui::colors::WARNING(), ui::colors::DANGER(), ui::colors::INFO(),
             ui::colors::BG_HOVER());
}

/// A tone as the stylesheet's [tone="…"] value.
const char* ccp_tone_key(rv::Tone t) {
    switch (t) {
        case rv::Tone::Success:
            return "ok";
        case rv::Tone::Warning:
            return "warn";
        case rv::Tone::Danger:
            return "danger";
        case rv::Tone::Info:
            return "info";
        case rv::Tone::Neutral:
            break;
    }
    return "";
}

void ccp_set_tone(QWidget* w, const char* tone) {
    const QString t = QString::fromLatin1(tone);
    if (w->property("tone").toString() == t)
        return;
    w->setProperty("tone", t);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QLabel* ccp_label(QWidget* parent, const char* name = nullptr) {
    auto* l = new QLabel(parent);
    if (name)
        l->setObjectName(QLatin1String(name));
    l->setTextFormat(Qt::PlainText);
    return l;
}

QPushButton* ccp_action(QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QStringLiteral("ccpAction"));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

/// A time in the user's locale, "—" when unknown.
QString ccp_when(qint64 ms) {
    if (ms <= 0)
        return QStringLiteral("—");
    return QLocale().toString(QDateTime::fromMSecsSinceEpoch(ms), QLocale::ShortFormat);
}

QString ccp_engine_text(const agt::TaskRun& r) {
    if (r.engine == QLatin1String(agt::kEngineLocal))
        return ChatContextPage::tr("Local agent");
    if (r.engine == QLatin1String(rv::kHarnessEngineId)) {
        const rv::HarnessEngineOption h = rv::harness_engine_option_now();
        return h.enabled ? ChatContextPage::tr("Harness · %1").arg(h.label)
                         : ChatContextPage::tr("Harness (Claude Code / Codex)");
    }
    const QString base = r.executor_id() == QLatin1String(agt::kExecutorCloudPlan)
                             ? ChatContextPage::tr("Cloud · multi-step task")
                             : ChatContextPage::tr("Cloud · one reply");
    return r.mode.isEmpty() ? base : base + QStringLiteral(" · ") + r.mode.toUpper();
}

struct CcpTools {
    QString text;
    QString tip;
    rv::Tone tone = rv::Tone::Neutral;
};

/// The terminal tool bridge as one line (same wording as the status bar).
CcpTools ccp_tools(const ToolBridgeStatus& s) {
    using State = ToolBridgeStatus::State;
    CcpTools t;
    switch (s.state) {
        case State::Off:
            t.text = ChatContextPage::tr("off");
            t.tip = ChatContextPage::tr("The terminal's tools are not offered to the cloud agent right now. They are "
                                        "registered while Chat Mode is open or a cloud run is going.");
            break;
        case State::Usable:
            t.text = ChatContextPage::tr("%n ready", nullptr, s.offered);
            t.tip = ChatContextPage::tr("The cloud agent can call %n terminal tool(s).", nullptr, s.offered);
            t.tone = rv::Tone::Success;
            break;
        case State::Unconfirmed:
            t.text = ChatContextPage::tr("unconfirmed");
            t.tip = ChatContextPage::tr("The tools were offered, but the server cannot say whether the agent sees them.");
            break;
        case State::AgentUnreachable:
            t.text = ChatContextPage::tr("agent offline");
            t.tip = ChatContextPage::tr("The tools are registered, but the cloud agent service is not running right now.");
            t.tone = rv::Tone::Warning;
            break;
        case State::NotRegistered:
            t.text = ChatContextPage::tr("not registered");
            t.tip = ChatContextPage::tr("The tools are not registered with the cloud agent; the terminal retries on its own.");
            if (!s.detail.isEmpty())
                t.tip += QLatin1Char('\n') + s.detail;
            t.tone = rv::Tone::Danger;
            break;
        case State::Unavailable:
            t.text = ChatContextPage::tr("unavailable");
            t.tip = ChatContextPage::tr("The server's tool bridge is temporarily unavailable.");
            if (!s.detail.isEmpty())
                t.tip += QLatin1Char('\n') + s.detail;
            t.tone = rv::Tone::Warning;
            break;
    }
    if (!s.rejected.isEmpty()) {
        constexpr int kShown = 12;
        QStringList lines;
        for (int i = 0; i < s.rejected.size() && i < kShown; ++i)
            lines << QStringLiteral("- %1: %2").arg(s.rejected[i].name, s.rejected[i].reason);
        if (s.rejected.size() > kShown)
            lines << ChatContextPage::tr("... and %1 more").arg(s.rejected.size() - kShown);
        t.tip += QStringLiteral("\n\n") + ChatContextPage::tr("Not accepted by the server:") + QLatin1Char('\n') +
                 lines.join(QLatin1Char('\n'));
    }
    return t;
}

QString ccp_kind_label(const QString& kind) {
    if (kind == QLatin1String(agt::kArtefactReport))
        return ChatContextPage::tr("REPORT");
    if (kind == QLatin1String(agt::kArtefactTable))
        return ChatContextPage::tr("TABLE");
    if (kind == QLatin1String(agt::kArtefactSeries))
        return ChatContextPage::tr("SERIES");
    if (kind == QLatin1String(agt::kArtefactJson))
        return ChatContextPage::tr("JSON");
    if (kind == QLatin1String(agt::kArtefactText))
        return ChatContextPage::tr("TEXT");
    if (kind == QLatin1String(agt::kArtefactImage))
        return ChatContextPage::tr("IMAGE");
    return ChatContextPage::tr("FILE");
}

/// A thin bar for "spent / budget", painted from the live theme.
class CcpFractionBar : public QWidget {
  public:
    explicit CcpFractionBar(QWidget* parent) : QWidget(parent) { setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); }
    void set_fraction(double f) {
        if (f == fraction_)
            return;
        fraction_ = f;
        update();
    }
    QSize sizeHint() const override { return QSize(80, bar_height()); }
    QSize minimumSizeHint() const override { return QSize(20, bar_height()); }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect r = rect();
        p.fillRect(r, QColor(ui::colors::BG_RAISED()));
        if (fraction_ < 0)
            return;
        const double f = std::clamp(fraction_, 0.0, 1.0);
        const QColor fill = fraction_ >= 1.0  ? QColor(ui::colors::DANGER())
                            : fraction_ >= 0.8 ? QColor(ui::colors::WARNING())
                                               : QColor(ui::colors::AMBER());
        p.fillRect(QRect(r.left(), r.top(), static_cast<int>(r.width() * f), r.height()), fill);
    }

  private:
    int bar_height() const { return std::max(3, QFontMetrics(ui::fonts::font(-3)).height() / 5); }
    double fraction_ = -1;
};

} // namespace chat_context_page_detail

using namespace chat_context_page_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ChatContextPage::Impl {
    ChatContextPage* q = nullptr;
    QString run_id;
    QPointer<ComposerContextBar> bar;
    QScrollArea* scroll = nullptr;

    // CONTEXT
    ui::PanelHeader* ctx_header = nullptr;
    QPushButton* add_symbol = nullptr;
    QPushButton* add_file = nullptr;
    QPushButton* add_more = nullptr;
    QLabel* ctx_hint = nullptr;
    QWidget* chips = nullptr;
    rv::RvFlowLayout* chips_flow = nullptr;
    QLabel* sent_title = nullptr;
    QWidget* sent_chips = nullptr;
    rv::RvFlowLayout* sent_flow = nullptr;

    // TOOLS
    ui::PanelHeader* tools_header = nullptr;
    QPushButton* mcp_refresh = nullptr;
    QLabel* bridge_key = nullptr;
    QLabel* bridge_value = nullptr;
    QLabel* mcp_key = nullptr;
    QLabel* mcp_value = nullptr;
    QWidget* mcp_rows = nullptr;
    QVBoxLayout* mcp_rows_layout = nullptr;
    QLabel* mcp_error = nullptr;
    bool mcp_loaded_once = false;
    bool mcp_loading = false;
    int mcp_seq = 0;

    // FILES
    ui::PanelHeader* files_header = nullptr;
    QWidget* file_rows = nullptr;
    QVBoxLayout* file_rows_layout = nullptr;
    QLabel* files_empty = nullptr;

    // BUDGET & LIMITS
    ui::PanelHeader* budget_header = nullptr;
    QLabel* budget_credits = nullptr;
    CcpFractionBar* budget_bar = nullptr;
    QLabel* budget_steps = nullptr;
    QLabel* budget_time = nullptr;
    QLabel* budget_next = nullptr;
    QLabel* paper_note = nullptr;

    // RUN STATS
    ui::PanelHeader* stats_header = nullptr;
    QLabel* stats_empty = nullptr;
    QWidget* stats_grid_host = nullptr;
    struct StatRow {
        QLabel* key = nullptr;
        QLabel* value = nullptr;
    };
    StatRow st_state, st_engine, st_turns, st_tokens, st_credits, st_elapsed, st_started, st_finished, st_steps;

    QTimer* tick = nullptr;
    bool dirty_context = true;
    bool dirty_files = true;
    bool dirty_run = true;

    void build();
    void wire();
    void retranslate();
    QWidget* section_body(QVBoxLayout* content, ui::PanelHeader* header, QVBoxLayout** body_layout);
    void clear_flow(rv::RvFlowLayout* flow);
    void add_chip(rv::RvFlowLayout* flow, QWidget* host, const QString& text, const QString& tip,
                  std::function<void()> detach);
    void refresh_all();
    void refresh_context();
    void refresh_tools();
    void refresh_files();
    void refresh_run();
    void fill_mcp(bool ok, const QVector<McpServer>& servers, int total_tools, const QString& err);
    void update_tick();
};

QWidget* ChatContextPage::Impl::section_body(QVBoxLayout* content, ui::PanelHeader* header, QVBoxLayout** body_layout) {
    content->addWidget(header);
    auto* body = new QWidget(header->parentWidget());
    body->setObjectName(QStringLiteral("ccpBody"));
    auto* vl = new QVBoxLayout(body);
    const int pad = std::max(6, QFontMetrics(ui::fonts::font(-3)).averageCharWidth());
    vl->setContentsMargins(pad + 4, pad / 2 + 2, pad + 2, pad);
    vl->setSpacing(std::max(3, pad / 2));
    content->addWidget(body);
    *body_layout = vl;
    return body;
}

void ChatContextPage::Impl::build() {
    q->setObjectName(QStringLiteral("chatContextPage"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(ccp_style()); // one sheet, before any child exists

    auto* outer = new QVBoxLayout(q);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    scroll = new QScrollArea(q);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("ccpContent"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    // CONTEXT
    {
        ctx_header = new ui::PanelHeader(QString(), content);
        add_symbol = ccp_action(ctx_header->controls());
        add_file = ccp_action(ctx_header->controls());
        add_more = ccp_action(ctx_header->controls());
        ctx_header->add_control(add_symbol);
        ctx_header->add_control(add_file);
        ctx_header->add_control(add_more);
        QVBoxLayout* vl = nullptr;
        QWidget* body = section_body(cl, ctx_header, &vl);
        ctx_hint = ccp_label(body, "ccpHint");
        ctx_hint->setWordWrap(true);
        vl->addWidget(ctx_hint);
        chips = new QWidget(body);
        chips_flow = new rv::RvFlowLayout(chips, 4, 4);
        vl->addWidget(chips);
        sent_title = ccp_label(body, "ccpSub");
        vl->addWidget(sent_title);
        sent_chips = new QWidget(body);
        sent_flow = new rv::RvFlowLayout(sent_chips, 4, 4);
        vl->addWidget(sent_chips);
    }
    // TOOLS
    {
        tools_header = new ui::PanelHeader(QString(), content);
        mcp_refresh = ccp_action(tools_header->controls());
        tools_header->add_control(mcp_refresh);
        QVBoxLayout* vl = nullptr;
        QWidget* body = section_body(cl, tools_header, &vl);
        auto* grid_host = new QWidget(body);
        auto* grid = new QGridLayout(grid_host);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(3);
        bridge_key = ccp_label(grid_host, "ccpKey");
        bridge_value = ccp_label(grid_host, "ccpValue");
        mcp_key = ccp_label(grid_host, "ccpKey");
        mcp_value = ccp_label(grid_host, "ccpValue");
        mcp_value->setWordWrap(true);
        grid->addWidget(bridge_key, 0, 0, Qt::AlignTop);
        grid->addWidget(bridge_value, 0, 1);
        grid->addWidget(mcp_key, 1, 0, Qt::AlignTop);
        grid->addWidget(mcp_value, 1, 1);
        grid->setColumnStretch(1, 1);
        vl->addWidget(grid_host);
        mcp_rows = new QWidget(body);
        mcp_rows_layout = new QVBoxLayout(mcp_rows);
        mcp_rows_layout->setContentsMargins(0, 0, 0, 0);
        mcp_rows_layout->setSpacing(2);
        vl->addWidget(mcp_rows);
        mcp_error = ccp_label(body, "ccpHint");
        mcp_error->setWordWrap(true);
        mcp_error->setVisible(false);
        vl->addWidget(mcp_error);
    }
    // FILES
    {
        files_header = new ui::PanelHeader(QString(), content);
        QVBoxLayout* vl = nullptr;
        QWidget* body = section_body(cl, files_header, &vl);
        file_rows = new QWidget(body);
        file_rows_layout = new QVBoxLayout(file_rows);
        file_rows_layout->setContentsMargins(0, 0, 0, 0);
        file_rows_layout->setSpacing(0);
        vl->addWidget(file_rows);
        files_empty = ccp_label(body, "ccpHint");
        files_empty->setWordWrap(true);
        vl->addWidget(files_empty);
    }
    // BUDGET & LIMITS
    {
        budget_header = new ui::PanelHeader(QString(), content);
        QVBoxLayout* vl = nullptr;
        QWidget* body = section_body(cl, budget_header, &vl);
        budget_credits = ccp_label(body, "ccpValue");
        budget_bar = new CcpFractionBar(body);
        budget_steps = ccp_label(body, "ccpValue");
        budget_time = ccp_label(body, "ccpValue");
        budget_next = ccp_label(body, "ccpValue");
        budget_next->setWordWrap(true);
        paper_note = ccp_label(body, "ccpHint");
        paper_note->setWordWrap(true);
        for (QWidget* w : {static_cast<QWidget*>(budget_credits), static_cast<QWidget*>(budget_bar),
                           static_cast<QWidget*>(budget_steps), static_cast<QWidget*>(budget_time),
                           static_cast<QWidget*>(budget_next), static_cast<QWidget*>(paper_note)})
            vl->addWidget(w);
    }
    // RUN STATS
    {
        stats_header = new ui::PanelHeader(QString(), content);
        QVBoxLayout* vl = nullptr;
        QWidget* body = section_body(cl, stats_header, &vl);
        stats_empty = ccp_label(body, "ccpHint");
        stats_empty->setWordWrap(true);
        vl->addWidget(stats_empty);
        stats_grid_host = new QWidget(body);
        auto* grid = new QGridLayout(stats_grid_host);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(2);
        int row = 0;
        for (StatRow* sr : {&st_state, &st_engine, &st_turns, &st_tokens, &st_credits, &st_elapsed, &st_started,
                            &st_finished, &st_steps}) {
            sr->key = ccp_label(stats_grid_host, "ccpKey");
            sr->value = ccp_label(stats_grid_host, "ccpValue");
            sr->value->setWordWrap(true);
            sr->value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            grid->addWidget(sr->key, row, 0, Qt::AlignTop);
            grid->addWidget(sr->value, row, 1);
            ++row;
        }
        grid->setColumnStretch(1, 1);
        vl->addWidget(stats_grid_host);
    }
    cl->addStretch(1);
    scroll->setWidget(content);

    tick = new QTimer(q);
    tick->setInterval(kCcpTickMs);
}

void ChatContextPage::Impl::wire() {
    auto& svc = agt::TaskRunService::instance();
    const auto on_run = [this](const QString& id) {
        if (id != run_id)
            return;
        dirty_run = true;
        if (q->isVisible())
            refresh_run();
    };
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, on_run);
    QObject::connect(&svc, &agt::TaskRunService::run_finished, q, on_run);
    QObject::connect(&svc, &agt::TaskRunService::steps_changed, q, on_run);
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() {
        dirty_run = dirty_files = dirty_context = true;
        if (q->isVisible())
            refresh_all();
    });
    QObject::connect(&svc, &agt::TaskRunService::artefact_added, q, [this](const QString& rid, const QString&) {
        if (rid != run_id)
            return;
        dirty_files = true;
        if (q->isVisible())
            refresh_files();
    });
    QObject::connect(&agt::ArtefactService::instance(), &agt::ArtefactService::artefacts_changed, q,
                     [this](const QString& rid) {
                         if (!rid.isEmpty() && rid != run_id)
                             return;
                         dirty_files = true;
                         if (q->isVisible())
                             refresh_files();
                     });
    QObject::connect(&TerminalToolBridge::instance(), &TerminalToolBridge::tools_status_changed, q,
                     [this](const ToolBridgeStatus&) {
                         if (q->isVisible())
                             refresh_tools();
                     });
    QObject::connect(tick, &QTimer::timeout, q, [this]() { refresh_run(); });

    QObject::connect(add_symbol, &QPushButton::clicked, q, [this]() {
        if (bar)
            bar->begin_symbol_entry();
    });
    QObject::connect(add_file, &QPushButton::clicked, q, [this]() {
        if (bar)
            bar->open_attach_dialog();
    });
    QObject::connect(add_more, &QPushButton::clicked, q, [this]() {
        if (bar)
            bar->popup_add_menu(add_more->mapToGlobal(QPoint(0, add_more->height())));
    });
    QObject::connect(mcp_refresh, &QPushButton::clicked, q, [this]() { q->refresh_mcp(); });

    new rv::RvEventHook(q, [this](QEvent::Type t) {
        switch (t) {
            case QEvent::LanguageChange:
                retranslate();
                break;
            case QEvent::Show:
                if (!mcp_loaded_once && !mcp_loading)
                    q->refresh_mcp(); // once, on the first show (never per tick)
                refresh_all();
                break;
            case QEvent::Hide:
                tick->stop();
                break;
            default:
                break;
        }
    });
}

void ChatContextPage::Impl::retranslate() {
    ctx_header->set_title(ChatContextPage::tr("Context"));
    add_symbol->setText(ChatContextPage::tr("+ SYMBOL"));
    add_symbol->setToolTip(ChatContextPage::tr("Add a symbol the agent should focus on (the composer's picker)"));
    add_symbol->setAccessibleName(ChatContextPage::tr("Attach a symbol"));
    add_file->setText(ChatContextPage::tr("+ FILE"));
    add_file->setToolTip(ChatContextPage::tr("Attach a text file to your next message"));
    add_file->setAccessibleName(ChatContextPage::tr("Attach a text file"));
    add_more->setText(ChatContextPage::tr("+ MORE"));
    add_more->setToolTip(ChatContextPage::tr("Portfolio, investment profile, clipboard text…"));
    add_more->setAccessibleName(ChatContextPage::tr("More context to attach"));
    sent_title->setText(ChatContextPage::tr("SENT WITH THIS RUN"));
    tools_header->set_title(ChatContextPage::tr("Tools"));
    mcp_refresh->setText(ChatContextPage::tr("REFRESH"));
    mcp_refresh->setToolTip(ChatContextPage::tr("Read the cloud agent's MCP servers again"));
    mcp_refresh->setAccessibleName(ChatContextPage::tr("Refresh the MCP servers"));
    bridge_key->setText(ChatContextPage::tr("Terminal tools"));
    mcp_key->setText(ChatContextPage::tr("MCP servers"));
    files_header->set_title(ChatContextPage::tr("Files"));
    budget_header->set_title(ChatContextPage::tr("Budget & limits"));
    paper_note->setText(ChatContextPage::tr("Paper only: a run never places a live or broker order. Trades the agent "
                                            "proposes stay approval cards for the Fincept portfolio ledger, and "
                                            "scheduled runs refuse money actions outright."));
    stats_header->set_title(ChatContextPage::tr("Run stats"));
    st_state.key->setText(ChatContextPage::tr("State"));
    st_engine.key->setText(ChatContextPage::tr("Engine"));
    st_turns.key->setText(ChatContextPage::tr("Turns"));
    st_tokens.key->setText(ChatContextPage::tr("Tokens"));
    st_tokens.key->setToolTip(ChatContextPage::tr("The input / output split is not reported by the service."));
    st_credits.key->setText(ChatContextPage::tr("Credits"));
    st_elapsed.key->setText(ChatContextPage::tr("Working time"));
    st_elapsed.key->setToolTip(ChatContextPage::tr("Pauses, questions to you and the queue do not count."));
    st_started.key->setText(ChatContextPage::tr("Started"));
    st_finished.key->setText(ChatContextPage::tr("Finished"));
    st_steps.key->setText(ChatContextPage::tr("Steps"));
    refresh_all();
}

void ChatContextPage::Impl::clear_flow(rv::RvFlowLayout* flow) {
    while (QLayoutItem* item = flow->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
}

void ChatContextPage::Impl::add_chip(rv::RvFlowLayout* flow, QWidget* host, const QString& text, const QString& tip,
                                     std::function<void()> detach) {
    auto* chip = new QFrame(host);
    chip->setObjectName(QStringLiteral("ccpChip"));
    if (!detach)
        chip->setProperty("sent", true);
    auto* hl = new QHBoxLayout(chip);
    hl->setContentsMargins(6, 1, detach ? 2 : 6, 1);
    hl->setSpacing(2);
    auto* label = ccp_label(chip);
    label->setText(text);
    hl->addWidget(label);
    if (detach) {
        auto* x = new QToolButton(chip);
        x->setObjectName(QStringLiteral("ccpDetach"));
        x->setText(QStringLiteral("×"));
        x->setCursor(Qt::PointingHandCursor);
        x->setToolTip(ChatContextPage::tr("Detach: no longer sent with your messages"));
        x->setAccessibleName(ChatContextPage::tr("Detach %1").arg(text));
        hl->addWidget(x);
        QObject::connect(x, &QToolButton::clicked, q, [fn = std::move(detach)]() { fn(); });
    }
    chip->setToolTip(tip);
    chip->setAccessibleName(text);
    flow->addWidget(chip);
}

void ChatContextPage::Impl::refresh_all() {
    refresh_context();
    refresh_tools();
    refresh_files();
    refresh_run();
}

void ChatContextPage::Impl::refresh_context() {
    dirty_context = false;
    clear_flow(chips_flow);
    const bool has_bar = !bar.isNull();
    int n = 0;
    if (has_bar) {
        for (const QString& s : bar->symbols()) {
            QPointer<ComposerContextBar> b = bar;
            add_chip(chips_flow, chips, s, ChatContextPage::tr("Symbol %1 goes with every message").arg(s), [b, s]() {
                if (b)
                    b->remove_symbol(s);
            });
            ++n;
        }
        if (!bar->portfolio_name().isEmpty()) {
            QPointer<ComposerContextBar> b = bar;
            add_chip(chips_flow, chips, ChatContextPage::tr("Portfolio: %1").arg(bar->portfolio_name()),
                     ChatContextPage::tr("The portfolio's name goes with every message"), [b]() {
                         if (b)
                             b->clear_portfolio();
                     });
            ++n;
        }
        if (!bar->profile_name().isEmpty()) {
            QPointer<ComposerContextBar> b = bar;
            add_chip(chips_flow, chips, ChatContextPage::tr("Profile: %1").arg(bar->profile_name()),
                     ChatContextPage::tr("The investment profile's name goes with every message"), [b]() {
                         if (b)
                             b->clear_profile();
                     });
            ++n;
        }
        if (!bar->attachment_name().isEmpty()) {
            QPointer<ComposerContextBar> b = bar;
            add_chip(chips_flow, chips,
                     ChatContextPage::tr("Attached: %1 · %2 chars")
                         .arg(bar->attachment_name(), QLocale().toString(bar->attachment_chars())),
                     ChatContextPage::tr("Inlined into your next message only"), [b]() {
                         if (b)
                             b->clear_attachment();
                     });
            ++n;
        }
    }
    chips->setVisible(n > 0);
    ctx_header->set_badge(n > 0 ? QString::number(n) : QString());
    ctx_hint->setText(!has_bar ? ChatContextPage::tr("The composer is not available.")
                      : n == 0 ? ChatContextPage::tr("Nothing attached: the agent sees only what you type, the "
                                                     "conversation and the terminal tools below.")
                               : ChatContextPage::tr("Goes with every message until detached; an attached text goes "
                                                     "with the next message only."));
    for (QPushButton* b : {add_symbol, add_file, add_more}) {
        b->setEnabled(has_bar);
        if (!has_bar)
            b->setToolTip(ChatContextPage::tr("The composer is not available."));
    }

    // What the shown run was started with (read-only).
    clear_flow(sent_flow);
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(run_id);
    int sent = 0;
    if (r) {
        const QJsonObject& c = r->context;
        QStringList syms;
        for (const QJsonValue& v : c.value(QStringLiteral("symbols")).toArray())
            if (!v.toString().isEmpty() && !syms.contains(v.toString()))
                syms << v.toString();
        const QString one = c.value(QStringLiteral("symbol")).toString();
        if (!one.isEmpty() && !syms.contains(one))
            syms << one;
        for (const QString& s : std::as_const(syms)) {
            add_chip(sent_flow, sent_chips, s, ChatContextPage::tr("This run was started with %1").arg(s), {});
            ++sent;
        }
        const QString pf = c.value(QStringLiteral("portfolio_name")).toString().isEmpty()
                               ? c.value(QStringLiteral("portfolio_id")).toVariant().toString()
                               : c.value(QStringLiteral("portfolio_name")).toString();
        if (!pf.isEmpty()) {
            add_chip(sent_flow, sent_chips, ChatContextPage::tr("Portfolio: %1").arg(pf), {}, {});
            ++sent;
        }
        const QString prof = c.value(QStringLiteral("profile_name")).toString();
        if (!prof.isEmpty()) {
            add_chip(sent_flow, sent_chips, ChatContextPage::tr("Profile: %1").arg(prof), {}, {});
            ++sent;
        }
        const QJsonObject att = c.value(QStringLiteral("attachment")).toObject();
        if (!att.isEmpty()) {
            add_chip(sent_flow, sent_chips,
                     ChatContextPage::tr("Attached: %1 · %2 chars")
                         .arg(att.value(QStringLiteral("name")).toString(),
                              QLocale().toString(att.value(QStringLiteral("chars")).toInt())),
                     att.value(QStringLiteral("truncated")).toBool()
                         ? ChatContextPage::tr("The attachment was cut to fit the message.")
                         : QString(),
                     {});
            ++sent;
        }
    }
    sent_title->setVisible(sent > 0);
    sent_chips->setVisible(sent > 0);
}

void ChatContextPage::Impl::refresh_tools() {
    const CcpTools t = ccp_tools(TerminalToolBridge::instance().status());
    bridge_value->setText(t.text);
    bridge_value->setToolTip(t.tip);
    bridge_value->setAccessibleName(ChatContextPage::tr("Terminal tools: %1").arg(t.text));
    ccp_set_tone(bridge_value, ccp_tone_key(t.tone));
    if (mcp_loading && !mcp_loaded_once)
        mcp_value->setText(ChatContextPage::tr("Reading the servers…"));
    mcp_refresh->setEnabled(!mcp_loading);
}

void ChatContextPage::Impl::fill_mcp(bool ok, const QVector<McpServer>& servers, int total_tools, const QString& err) {
    while (QLayoutItem* item = mcp_rows_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    mcp_loaded_once = true;
    mcp_loading = false;
    mcp_refresh->setEnabled(true);
    if (!ok) {
        LOG_DEBUG("ChatContextPage", QStringLiteral("MCP servers could not be read")); // ChatModeService logs the cause
        mcp_value->setText(ChatContextPage::tr("not available"));
        ccp_set_tone(mcp_value, "dim");
        mcp_error->setText(err.isEmpty() ? ChatContextPage::tr("The server did not answer.") : err);
        mcp_error->setVisible(true);
        mcp_rows->setVisible(false);
        emit q->mcp_state_changed(-1, -1);
        return;
    }
    mcp_error->setVisible(false);
    int connected = 0;
    for (const McpServer& s : servers)
        if (s.status == QLatin1String("connected"))
            ++connected;
    const int total = static_cast<int>(servers.size());
    mcp_value->setText(total == 0 ? ChatContextPage::tr("none connected")
                                  : ChatContextPage::tr("%1 of %2 connected · %n tool(s)", nullptr, total_tools)
                                        .arg(connected)
                                        .arg(total));
    ccp_set_tone(mcp_value, total > 0 && connected == total ? "ok" : connected < total ? "warn" : "");
    int shown = 0;
    for (const McpServer& s : servers) {
        if (shown >= kCcpMaxMcpRows)
            break;
        auto* row = new QWidget(mcp_rows);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto* dot = ccp_label(row);
        dot->setText(QStringLiteral("●"));
        ccp_set_tone(dot, s.status == QLatin1String("connected") ? "ok" : s.status == QLatin1String("error") ? "danger"
                                                                                                             : "dim");
        auto* text = ccp_label(row, "ccpValue");
        QString name = s.name;
        if (s.is_shared())
            name += ChatContextPage::tr(" (shared)");
        text->setText(ChatContextPage::tr("%1 · %2 · %n tool(s)", nullptr, s.tools_count).arg(name, s.status));
        text->setToolTip(s.error.isEmpty() ? s.tool_names.mid(0, 20).join(QStringLiteral(", ")) : s.error);
        text->setWordWrap(true);
        hl->addWidget(dot, 0, Qt::AlignTop);
        hl->addWidget(text, 1);
        mcp_rows_layout->addWidget(row);
        ++shown;
    }
    if (servers.size() > kCcpMaxMcpRows) {
        auto* more = ccp_label(mcp_rows, "ccpHint");
        more->setText(ChatContextPage::tr("%n more in the MCP section of the left rail", nullptr,
                                          static_cast<int>(servers.size()) - kCcpMaxMcpRows));
        mcp_rows_layout->addWidget(more);
    }
    mcp_rows->setVisible(shown > 0);
    emit q->mcp_state_changed(connected, total);
}

void ChatContextPage::Impl::refresh_files() {
    dirty_files = false;
    while (QLayoutItem* item = file_rows_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    if (run_id.isEmpty()) {
        files_header->set_badge(QString());
        files_empty->setText(ChatContextPage::tr("Reports, tables and files a run produces show here."));
        files_empty->setVisible(true);
        file_rows->setVisible(false);
        return;
    }
    const QVector<agt::RunArtefact> list = agt::ArtefactService::instance().artefacts(run_id);
    files_header->set_badge(list.isEmpty() ? QString() : QString::number(list.size()));
    if (list.isEmpty()) {
        files_empty->setText(ChatContextPage::tr("This run has produced no files yet."));
        files_empty->setVisible(true);
        file_rows->setVisible(false);
        return;
    }
    files_empty->setVisible(false);
    // Newest first, capped: the FILES page lists them all.
    int shown = 0;
    for (int i = static_cast<int>(list.size()) - 1; i >= 0 && shown < kCcpMaxFileRows; --i, ++shown) {
        const agt::RunArtefact& a = list.at(i);
        auto* b = new QPushButton(file_rows);
        b->setObjectName(QStringLiteral("ccpFile"));
        b->setCursor(Qt::PointingHandCursor);
        const QString size = a.bytes > 0 ? QLocale().formattedDataSize(a.bytes) : QString();
        QString title = a.title.simplified();
        title.replace(QLatin1Char('&'), QStringLiteral("&&")); // a button label: '&' would be a mnemonic
        b->setText(size.isEmpty() ? QStringLiteral("%1   %2").arg(ccp_kind_label(a.kind), title)
                                  : QStringLiteral("%1   %2   %3").arg(ccp_kind_label(a.kind), title, size));
        b->setToolTip(ChatContextPage::tr("Open in FILES: preview, save, add to a report or send to Notes"));
        b->setAccessibleName(ChatContextPage::tr("File %1, %2").arg(a.title, ccp_kind_label(a.kind)));
        const QString id = a.id;
        QObject::connect(b, &QPushButton::clicked, q, [this, id]() { emit q->files_requested(id); });
        file_rows_layout->addWidget(b);
    }
    if (list.size() > kCcpMaxFileRows) {
        auto* more = new QPushButton(file_rows);
        more->setObjectName(QStringLiteral("ccpFile"));
        more->setCursor(Qt::PointingHandCursor);
        more->setText(ChatContextPage::tr("+ %n more in FILES", nullptr, static_cast<int>(list.size()) - kCcpMaxFileRows));
        QObject::connect(more, &QPushButton::clicked, q, [this]() { emit q->files_requested(QString()); });
        file_rows_layout->addWidget(more);
    }
    file_rows->setVisible(true);
}

void ChatContextPage::Impl::refresh_run() {
    dirty_run = false;
    auto& svc = agt::TaskRunService::instance();
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : svc.find_run(run_id);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // BUDGET & LIMITS
    if (!r) {
        budget_credits->setVisible(false);
        budget_bar->setVisible(false);
        budget_steps->setVisible(false);
        budget_time->setVisible(false);
        const QString next = bar ? bar->budget_summary() : QString();
        budget_next->setText(next.isEmpty() ? ChatContextPage::tr("No run selected.")
                                            : ChatContextPage::tr("Next message: %1 · %2").arg(bar->engine_label(), next));
        budget_next->setVisible(true);
    } else {
        const QVector<agt::RunStep> steps = svc.steps(run_id);
        const rv::BudgetLine b = rv::budget_line(*r, steps, now);
        budget_next->setVisible(false);
        if (!rv::run_uses_credits(*r)) {
            budget_credits->setText(r->engine == QLatin1String(agt::kEngineLocal)
                                        ? ChatContextPage::tr("Your own model: no Fincept credits")
                                        : ChatContextPage::tr("Billed to your own API key: no Fincept credits"));
            budget_bar->set_fraction(-1);
            budget_bar->setVisible(false);
        } else {
            budget_credits->setText(b.credits);
            budget_bar->set_fraction(b.credits_fraction);
            budget_bar->setVisible(b.credits_fraction >= 0);
        }
        budget_credits->setVisible(true);
        ccp_set_tone(budget_credits, b.stopped ? "danger" : b.credits_fraction >= 0.8 ? "warn" : "");
        budget_steps->setText(b.steps);
        budget_steps->setVisible(!b.steps.isEmpty());
        budget_time->setText(b.time.isEmpty() ? ChatContextPage::tr("No time limit") : b.time);
        budget_time->setVisible(true);
        if (b.stopped)
            budget_credits->setToolTip(ChatContextPage::tr("The run stopped at its budget."));
        else
            budget_credits->setToolTip(QString());
    }

    // RUN STATS
    stats_empty->setVisible(!r);
    stats_grid_host->setVisible(r != nullptr);
    if (!r) {
        stats_header->set_badge(QString());
        stats_empty->setText(run_id.isEmpty() ? ChatContextPage::tr("No run selected. Send a message or pick a run in "
                                                                    "RUNS: its numbers show here.")
                                              : ChatContextPage::tr("This run was deleted."));
        tick->stop();
        return;
    }
    stats_header->set_badge(rv::run_status_pill(r->status));
    st_state.value->setText(agt::run_status_label(r->status) +
                            (agt::is_terminal(r->status) || r->activity.isEmpty() ? QString()
                                                                                  : QStringLiteral(" · ") + r->activity));
    ccp_set_tone(st_state.value, ccp_tone_key(rv::run_status_tone(r->status)));
    st_engine.value->setText(ccp_engine_text(*r));
    st_turns.value->setText(QLocale().toString(r->turns));
    st_tokens.value->setText(r->tokens_used > 0 ? QLocale().toString(r->tokens_used) : QStringLiteral("—"));
    st_tokens.value->setToolTip(ChatContextPage::tr("The input / output split is not reported by the service."));
    if (rv::run_uses_credits(*r))
        st_credits.value->setText(
            rv::credits_text(*r, ChatContextPage::tr("%1 credits").arg(rv::format_credits(r->credits_spent))));
    else
        st_credits.value->setText(ChatContextPage::tr("none (not billed in Fincept credits)"));
    st_elapsed.value->setText(r->started_at_ms > 0 ? rv::format_duration(rv::run_elapsed_ms(*r, now)) : QStringLiteral("—"));
    st_started.value->setText(ccp_when(r->started_at_ms > 0 ? r->started_at_ms : r->created_at_ms));
    st_finished.value->setText(agt::is_terminal(r->status) ? ccp_when(r->finished_at_ms) : QStringLiteral("—"));
    const QVector<agt::RunStep> steps = svc.steps(run_id);
    const bool single = rv::is_single_reply(*r, steps) || steps.isEmpty();
    st_steps.key->setVisible(!single);
    st_steps.value->setVisible(!single);
    if (!single) {
        const rv::StepProgress p = rv::step_progress(steps);
        st_steps.value->setText(p.failed > 0 ? ChatContextPage::tr("%1 / %2 done · %3 failed").arg(p.done).arg(p.total).arg(p.failed)
                                             : ChatContextPage::tr("%1 / %2 done").arg(p.done).arg(p.total));
    }
    update_tick();
}

void ChatContextPage::Impl::update_tick() {
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(run_id);
    // Only a working run's clock moves; nothing ticks while the page is hidden.
    const bool want = q->isVisible() && r && agt::holds_slot(r->status) && r->started_at_ms > 0;
    if (want && !tick->isActive())
        tick->start();
    else if (!want && tick->isActive())
        tick->stop();
}

// ── ChatContextPage ──────────────────────────────────────────────────────────

ChatContextPage::ChatContextPage(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
}

ChatContextPage::~ChatContextPage() = default;

void ChatContextPage::set_run(const QString& run_id) {
    if (run_id == d_->run_id)
        return;
    d_->run_id = run_id;
    d_->dirty_context = d_->dirty_files = d_->dirty_run = true;
    if (isVisible())
        d_->refresh_all();
}

QString ChatContextPage::run_id() const {
    return d_->run_id;
}

void ChatContextPage::set_context_bar(ComposerContextBar* bar) {
    if (d_->bar == bar)
        return;
    if (d_->bar)
        disconnect(d_->bar, nullptr, this, nullptr);
    d_->bar = bar;
    if (bar) {
        const auto changed = [this]() {
            d_->dirty_context = d_->dirty_run = true;
            if (isVisible()) {
                d_->refresh_context();
                d_->refresh_run();
            }
        };
        connect(bar, &ComposerContextBar::context_changed, this, changed);
        connect(bar, &ComposerContextBar::engine_changed, this, changed);
        connect(bar, &ComposerContextBar::run_as_task_toggled, this, changed);
    }
    d_->refresh_context();
    d_->refresh_run();
}

void ChatContextPage::refresh_mcp() {
    if (d_->mcp_loading)
        return;
    d_->mcp_loading = true;
    d_->mcp_refresh->setEnabled(false);
    if (!d_->mcp_loaded_once)
        d_->mcp_value->setText(tr("Reading the servers…"));
    const int seq = ++d_->mcp_seq;
    QPointer<ChatContextPage> self(this);
    ChatModeService::instance().list_mcp_servers(
        [self, seq](bool ok, QVector<McpServer> servers, int total_tools, QString err) {
            if (!self || seq != self->d_->mcp_seq)
                return;
            self->d_->fill_mcp(ok, servers, total_tools, err);
        });
}

void ChatContextPage::focus_first() {
    if (d_->add_symbol->isEnabled())
        d_->add_symbol->setFocus(Qt::OtherFocusReason);
    else
        d_->mcp_refresh->setFocus(Qt::OtherFocusReason);
}

} // namespace fincept::chat_mode
