// ChatAgentPanel.cpp — Chat Mode's run inspector (WP2) and agent panel.
//
// Two kinds (AgentPanelPages, 2026-11-17 revamp): the right rail's Inspector —
// CONTEXT (ChatContextPage) | PLAN | ACTIVITY | FILES (RunArtefactsPanel) — and
// the left rail's Agent panel — MEMORY | MCP. Every page is an empty container
// until it is first shown; then its content is built into it. The run pages
// follow set_current_run(). The page picked last is remembered per kind
// (QSettings "chat_mode/inspector_page" / "chat_mode/agent_page", string ids;
// read once from the old index key "chat_mode/inspector_tab" when they are new).
//
// MEMORY and MCP keep their server calls (ChatModeService) but lost their modal
// flows: adding is an inline form, deleting asks in an inline strip, failures
// show in the tab's status line. The old Tasks tab (server background tasks,
// QInputDialog + nested dlg.exec() activity — plan defect B7) is removed: long
// tasks are runs (RUNS tab + this inspector).

#include "screens/chat_mode/ChatAgentPanel.h"

#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatContextPage.h"
#include "screens/chat_mode/ChatModeService.h"
#include "screens/chat_mode/ComposerContextBar.h"
#include "screens/chat_mode/RunActivityView.h"
#include "screens/chat_mode/RunArtefactsPanel.h"
#include "screens/chat_mode/RunPlanPanel.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>

namespace fincept::chat_mode {

namespace rv = fincept::chat_mode::run_view;

namespace chat_agent_panel_detail {

/// Every page a panel can carry; a panel carries the ones of its kind (cap_pages_of).
enum CapPage { kCapContext = 0, kCapPlan, kCapActivity, kCapFiles, kCapMemory, kCapMcp, kCapPageCount };

constexpr const char* kCapOldTabKey = "chat_mode/inspector_tab"; ///< before 2026-11-17: index of 5 tabs
constexpr const char* kCapInspectorKey = "chat_mode/inspector_page";
constexpr const char* kCapAgentKey = "chat_mode/agent_page";
constexpr int kCapStatusRole = Qt::UserRole + 2;

const char* cap_page_id(int page) {
    switch (page) {
        case kCapContext:
            return "context";
        case kCapPlan:
            return "plan";
        case kCapActivity:
            return "activity";
        case kCapFiles:
            return "files";
        case kCapMemory:
            return "memory";
        case kCapMcp:
            return "mcp";
        default:
            break;
    }
    return "";
}

int cap_page_from_id(const QString& id) {
    for (int p = 0; p < kCapPageCount; ++p)
        if (id == QLatin1String(cap_page_id(p)))
            return p;
    if (id == QLatin1String("artefacts"))
        return kCapFiles; // the page's old name
    return -1;
}

QVector<int> cap_pages_of(AgentPanelPages kind) {
    if (kind == AgentPanelPages::Agent)
        return {kCapMemory, kCapMcp};
    return {kCapContext, kCapPlan, kCapActivity, kCapFiles};
}

/// The page of the old index key (0 plan, 1 activity, 2 artefacts, 3 memory, 4 mcp)
/// for a panel of `kind`, -1 when it named a page the kind does not carry.
int cap_page_from_old_index(int index, AgentPanelPages kind) {
    static constexpr int kOld[] = {kCapPlan, kCapActivity, kCapFiles, kCapMemory, kCapMcp};
    if (index < 0 || index >= static_cast<int>(std::size(kOld)))
        return -1;
    const int page = kOld[index];
    return cap_pages_of(kind).contains(page) ? page : -1;
}

QString cap_style() {
    return QStringLiteral(
               "#chatAgentPanel{background:%1;border:none;}"
               "#chatAgentPanel[capBorder=\"true\"]{border-left:1px solid %2;}"
               "#chatAgentPanel QTabBar#capTabs{background:%1;border-bottom:1px solid %2;}"
               "#chatAgentPanel QTabBar#capTabs::tab{background:%1;color:%3;padding:6px 9px;border:none;"
               "border-right:1px solid %2;border-bottom:2px solid transparent;font-size:%9px;font-weight:700;}"
               "#chatAgentPanel QTabBar#capTabs::tab:last{border-right:none;}"
               "#chatAgentPanel QTabBar#capTabs QToolButton{background:%1;border:1px solid %2;color:%3;}"
               "#chatAgentPanel QTabBar#capTabs::tab:selected{color:%4;border-bottom:2px solid %4;}"
               "#chatAgentPanel QTabBar#capTabs::tab:hover{color:%5;}"
               "#chatAgentPanel QWidget#capPage{background:%1;}"
               "#chatAgentPanel QLabel{background:transparent;border:none;color:%3;font-size:%9px;}"
               "#chatAgentPanel QLabel#capTitle{color:%3;font-weight:700;letter-spacing:1px;}"
               "#chatAgentPanel QLabel#capHint{color:%6;}"
               "#chatAgentPanel QLabel#capStatus[error=\"true\"]{color:%10;}"
               "#chatAgentPanel QListWidget{background:%1;border:1px solid %2;color:%5;font-size:%11px;outline:none;}"
               "#chatAgentPanel QListWidget::item{padding:6px 8px;border-bottom:1px solid %7;}"
               "#chatAgentPanel QListWidget::item:selected{background:%7;color:%4;}"
               "#chatAgentPanel QListWidget::item:hover{background:%8;}"
               "#chatAgentPanel QPushButton{background:%7;color:%3;border:1px solid %2;padding:3px 8px;"
               "font-size:%9px;}"
               "#chatAgentPanel QPushButton:hover{background:%8;color:%5;border-color:%4;}"
               "#chatAgentPanel QPushButton:focus{border-color:%4;}"
               "#chatAgentPanel QPushButton:disabled{color:%12;border-color:%2;background:transparent;}"
               "#chatAgentPanel QPushButton#capDanger{background:%13;color:%10;border-color:%10;}"
               "#chatAgentPanel QFrame#capForm{background:%7;border:1px solid %2;}"
               "#chatAgentPanel QFrame#capConfirm{background:%13;border:1px solid %10;}"
               "#chatAgentPanel QLineEdit,#chatAgentPanel QComboBox{background:%1;color:%5;border:1px solid %2;"
               "padding:3px 6px;font-size:%11px;}"
               "#chatAgentPanel QLineEdit:focus,#chatAgentPanel QComboBox:focus{border-color:%14;}"
               "#chatAgentPanel QComboBox QAbstractItemView{background:%1;color:%5;border:1px solid %2;}"
               "#chatAgentPanel QScrollBar:vertical{background:transparent;width:6px;}"
               "#chatAgentPanel QScrollBar::handle:vertical{background:%15;min-height:20px;}"
               "#chatAgentPanel QScrollBar::add-line:vertical,#chatAgentPanel QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER(),
             ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_DIM(), ui::colors::BG_RAISED(), ui::colors::BG_HOVER(),
             ui::fonts::px(-3))
        .arg(ui::colors::DANGER(), ui::fonts::px(-2), ui::colors::TEXT_DIM(),
             ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::colors::BORDER_BRIGHT(),
             ui::colors::BORDER_MED());
}

QColor cap_mcp_color(const QString& status) {
    if (status == QLatin1String("connected"))
        return QColor(ui::colors::SUCCESS());
    if (status == QLatin1String("error"))
        return QColor(ui::colors::DANGER());
    return QColor(ui::colors::TEXT_SECONDARY());
}

QPushButton* cap_button(QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QLabel* cap_label(QWidget* parent, const char* object_name = nullptr) {
    auto* l = new QLabel(parent);
    if (object_name)
        l->setObjectName(QLatin1String(object_name));
    l->setWordWrap(true);
    return l;
}

/// A disabled placeholder row ("Loading…", "No entries yet.").
void cap_placeholder(QListWidget* list, const QString& text, const QString& tip = {}) {
    auto* item = new QListWidgetItem(text, list);
    item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    if (!tip.isEmpty())
        item->setToolTip(tip);
}

/// Inline "are you sure?" strip: text + a destructive and a keep button.
struct CapConfirm {
    QFrame* frame = nullptr;
    QLabel* text = nullptr;
    QPushButton* yes = nullptr;
    QPushButton* no = nullptr;
    std::function<void()> action;

    void build(QWidget* parent, QVBoxLayout* into) {
        frame = new QFrame(parent);
        frame->setObjectName(QStringLiteral("capConfirm"));
        auto* vl = new QVBoxLayout(frame);
        vl->setContentsMargins(8, 6, 8, 6);
        vl->setSpacing(4);
        text = cap_label(frame);
        vl->addWidget(text);
        auto* hl = new QHBoxLayout;
        hl->setSpacing(4);
        yes = cap_button(frame);
        yes->setObjectName(QStringLiteral("capDanger"));
        no = cap_button(frame);
        hl->addWidget(yes);
        hl->addWidget(no);
        hl->addStretch();
        vl->addLayout(hl);
        frame->setVisible(false);
        into->addWidget(frame);
        QObject::connect(no, &QPushButton::clicked, frame, [this]() { hide(); });
        QObject::connect(yes, &QPushButton::clicked, frame, [this]() {
            auto fn = std::move(action);
            hide();
            if (fn)
                fn();
        });
        for (QPushButton* b : {yes, no})
            new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
                if (ke->key() != Qt::Key_Escape)
                    return false;
                if (apply)
                    hide();
                return true;
            });
    }
    void ask(const QString& question, const QString& yes_text, const QString& no_text, std::function<void()> fn) {
        text->setText(question);
        yes->setText(yes_text);
        no->setText(no_text);
        action = std::move(fn);
        frame->setVisible(true);
        yes->setFocus();
    }
    void hide() {
        frame->setVisible(false);
        action = nullptr;
    }
};

void cap_set_status(QLabel* status, const QString& text, bool error) {
    status->setText(text);
    status->setVisible(!text.isEmpty());
    if (status->property("error").toBool() != error) {
        status->setProperty("error", error);
        status->style()->unpolish(status);
        status->style()->polish(status);
    }
}

} // namespace chat_agent_panel_detail

using namespace chat_agent_panel_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ChatAgentPanel::Impl {
    ChatAgentPanel* q = nullptr;
    AgentPanelPages kind = AgentPanelPages::Inspector;
    QVector<int> order; ///< CapPage of each tab (tab index == stack index)
    QString run_id;
    QPointer<ComposerContextBar> context_bar;
    QTabBar* tabs = nullptr;
    QStackedWidget* stack = nullptr;
    std::array<QWidget*, kCapPageCount> pages{};
    std::array<bool, kCapPageCount> built{};

    ChatContextPage* context = nullptr;
    RunPlanPanel* plan = nullptr;
    RunActivityView* activity = nullptr;
    RunArtefactsPanel* artefacts = nullptr;

    // MEMORY
    QLabel* mem_title = nullptr;
    QLabel* mem_hint = nullptr;
    QListWidget* mem_list = nullptr;
    QPushButton* mem_add = nullptr;
    QPushButton* mem_del = nullptr;
    QPushButton* mem_clear = nullptr;
    QPushButton* mem_refresh = nullptr;
    QFrame* mem_form = nullptr;
    QLineEdit* mem_key = nullptr;
    QLineEdit* mem_value = nullptr;
    QComboBox* mem_type = nullptr;
    QPushButton* mem_save = nullptr;
    QPushButton* mem_cancel = nullptr;
    CapConfirm mem_confirm;
    QLabel* mem_status = nullptr;
    bool mem_busy = false;

    // MCP
    QLabel* mcp_title = nullptr;
    QLabel* mcp_hint = nullptr;
    QListWidget* mcp_list = nullptr;
    QPushButton* mcp_add = nullptr;
    QPushButton* mcp_del = nullptr;
    QPushButton* mcp_refresh = nullptr;
    QFrame* mcp_form = nullptr;
    QLabel* mcp_form_hint = nullptr;
    QLineEdit* mcp_name = nullptr;
    QComboBox* mcp_transport = nullptr;
    QLineEdit* mcp_url = nullptr;
    QPushButton* mcp_save = nullptr;
    QPushButton* mcp_cancel = nullptr;
    CapConfirm mcp_confirm;
    QLabel* mcp_status = nullptr;
    bool mcp_busy = false;
    int mcp_total_tools = -1;
    QVector<McpServer> mcp_servers;

    void build();
    void retranslate();
    void fit_width();
    int page_at(int tab) const { return order.value(tab, -1); }
    int tab_of(int page) const { return static_cast<int>(order.indexOf(page)); }
    const char* settings_key() const { return kind == AgentPanelPages::Agent ? kCapAgentKey : kCapInspectorKey; }
    int initial_page() const;
    void ensure_page(int page);
    void show_tab(int index);
    void build_memory(QWidget* page);
    void build_mcp(QWidget* page);
    void update_memory_buttons();
    void update_mcp_buttons();
    void fill_memory(bool ok, const QVector<AgentMemory>& memories, const QString& err);
    void fill_mcp(bool ok, const QVector<McpServer>& servers, int total_tools, const QString& err);
    void save_memory();
    void save_mcp();
};

void ChatAgentPanel::Impl::build() {
    order = cap_pages_of(kind);
    const bool inspector = kind == AgentPanelPages::Inspector;
    q->setObjectName(QStringLiteral("chatAgentPanel"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    // The right rail draws a rule on its left edge; the left rail's agent panel none.
    q->setProperty("capBorder", inspector);
    q->setStyleSheet(cap_style());
    // In a QSplitter now: no fixed width, a minimum the tabs and the run views need
    // (from the font; re-fitted once a new appearance has settled).
    fit_width();

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(inspector ? 1 : 0, 0, 0, 0); // the left border line
    vl->setSpacing(0);

    tabs = new QTabBar(q);
    tabs->setObjectName(QStringLiteral("capTabs"));
    // Several tabs in a narrow column: each gets its full name, padding and a divider
    // while they fit; when they do not (large fonts, a narrow rail), names are
    // elided to a few letters (the tooltip has the whole name) and only then do
    // scroll arrows appear — never words run together ("ACTIVITY FILES").
    tabs->setExpanding(true);
    tabs->setDrawBase(false);
    tabs->setElideMode(Qt::ElideRight);
    tabs->setUsesScrollButtons(true);
    for (int i = 0; i < order.size(); ++i)
        tabs->addTab(QString());
    vl->addWidget(tabs);

    stack = new QStackedWidget(q);
    for (int p : std::as_const(order)) {
        auto* page = new QWidget(stack);
        page->setObjectName(QStringLiteral("capPage"));
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(0);
        stack->addWidget(page);
        pages[static_cast<size_t>(p)] = page;
    }
    vl->addWidget(stack, 1);

    QObject::connect(tabs, &QTabBar::currentChanged, q, [this](int index) {
        // A page is built (and memory / MCP read) only once the panel is on screen;
        // a pick made while it is hidden (the rail's switch) is built by showEvent.
        if (q->isVisible())
            show_tab(index);
        else
            stack->setCurrentIndex(index);
        const QString id = QString::fromLatin1(cap_page_id(page_at(index)));
        QSettings().setValue(QLatin1String(settings_key()), id);
        emit q->page_changed(id);
    });
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() { fit_width(); });
}

void ChatAgentPanel::Impl::fit_width() {
    // Room for the tab names and a run view's two-column rows at the current font.
    const QFontMetrics fm(ui::fonts::font(-3, QFont::Bold));
    const int w = std::max(200, fm.averageCharWidth() * 30);
    if (q->minimumWidth() != w)
        q->setMinimumWidth(w);
}

int ChatAgentPanel::Impl::initial_page() const {
    QSettings s;
    const QString key = QLatin1String(settings_key());
    if (s.contains(key)) {
        const int p = cap_page_from_id(s.value(key).toString());
        if (order.contains(p))
            return p;
    } else if (s.contains(QLatin1String(kCapOldTabKey))) {
        // Before 2026-11-17 one panel had all five tabs, remembered by index: read it
        // once (the new key is written on the first pick).
        const int p = cap_page_from_old_index(s.value(QLatin1String(kCapOldTabKey)).toInt(), kind);
        if (p >= 0)
            return p;
    }
    return order.value(0, kCapContext);
}

void ChatAgentPanel::Impl::retranslate() {
    for (int i = 0; i < order.size(); ++i) {
        QString name;
        QString tip;
        switch (order.at(i)) {
            case kCapContext:
                name = ChatAgentPanel::tr("CONTEXT");
                tip = ChatAgentPanel::tr("What the run can see and use: context, tools, files, budget and run stats");
                break;
            case kCapPlan:
                name = ChatAgentPanel::tr("PLAN");
                tip = ChatAgentPanel::tr("The run's steps, budget and controls");
                break;
            case kCapActivity:
                name = ChatAgentPanel::tr("ACTIVITY");
                tip = ChatAgentPanel::tr("Everything the run did: steps, tool calls, questions, results");
                break;
            case kCapFiles:
                name = ChatAgentPanel::tr("FILES");
                tip = ChatAgentPanel::tr("Reports, tables and files the run produced");
                break;
            case kCapMemory:
                name = ChatAgentPanel::tr("MEMORY");
                tip = ChatAgentPanel::tr("What the cloud agent remembers about you");
                break;
            case kCapMcp:
                name = ChatAgentPanel::tr("MCP");
                tip = ChatAgentPanel::tr("Remote MCP tool servers of the cloud agent");
                break;
            default:
                break;
        }
        tabs->setTabText(i, name);
        tabs->setTabToolTip(i, tip);
    }
    tabs->setAccessibleName(kind == AgentPanelPages::Agent ? ChatAgentPanel::tr("Agent memory and MCP servers")
                                                           : ChatAgentPanel::tr("Run inspector"));

    if (mem_title) {
        mem_title->setText(ChatAgentPanel::tr("AGENT MEMORY"));
        mem_hint->setText(ChatAgentPanel::tr("Facts and preferences the cloud agent keeps across conversations."));
        mem_add->setText(ChatAgentPanel::tr("+ ADD"));
        mem_add->setToolTip(ChatAgentPanel::tr("Add a memory entry"));
        mem_del->setText(ChatAgentPanel::tr("DELETE"));
        mem_clear->setText(ChatAgentPanel::tr("CLEAR ALL"));
        mem_refresh->setText(ChatAgentPanel::tr("REFRESH"));
        mem_refresh->setToolTip(ChatAgentPanel::tr("Read the memory again"));
        mem_key->setPlaceholderText(ChatAgentPanel::tr("Key, e.g. risk_tolerance"));
        mem_key->setAccessibleName(ChatAgentPanel::tr("Memory key"));
        mem_value->setPlaceholderText(ChatAgentPanel::tr("Value, e.g. moderate"));
        mem_value->setAccessibleName(ChatAgentPanel::tr("Memory value"));
        mem_type->setItemText(0, ChatAgentPanel::tr("Fact"));
        mem_type->setItemText(1, ChatAgentPanel::tr("Preference"));
        mem_type->setItemText(2, ChatAgentPanel::tr("Entity"));
        mem_type->setAccessibleName(ChatAgentPanel::tr("Memory type"));
        mem_save->setText(ChatAgentPanel::tr("SAVE"));
        mem_cancel->setText(ChatAgentPanel::tr("CANCEL"));
        mem_list->setAccessibleName(ChatAgentPanel::tr("Agent memory entries"));
        update_memory_buttons();
    }
    if (mcp_title) {
        mcp_title->setText(ChatAgentPanel::tr("MCP SERVERS"));
        mcp_add->setText(ChatAgentPanel::tr("+ ADD"));
        mcp_add->setToolTip(ChatAgentPanel::tr("Add a remote MCP server (https)"));
        mcp_del->setText(ChatAgentPanel::tr("DELETE"));
        mcp_refresh->setText(ChatAgentPanel::tr("REFRESH"));
        mcp_refresh->setToolTip(ChatAgentPanel::tr("Reconnect every server and list their tools again"));
        mcp_form_hint->setText(ChatAgentPanel::tr("The cloud agent can use remote MCP servers reachable over https. "
                                                  "Local (stdio) servers, such as uvx / npx commands, belong in the "
                                                  "terminal's MCP Servers screen."));
        mcp_name->setPlaceholderText(ChatAgentPanel::tr("Name: 2-48 of a-z, 0-9 and -"));
        mcp_name->setAccessibleName(ChatAgentPanel::tr("Server name"));
        mcp_transport->setItemText(0, ChatAgentPanel::tr("Streamable HTTP (recommended)"));
        mcp_transport->setItemText(1, ChatAgentPanel::tr("SSE (Server-Sent Events)"));
        mcp_transport->setAccessibleName(ChatAgentPanel::tr("Transport"));
        mcp_url->setPlaceholderText(QStringLiteral("https://mcp.example.com/mcp"));
        mcp_url->setAccessibleName(ChatAgentPanel::tr("Server URL"));
        mcp_save->setText(ChatAgentPanel::tr("ADD SERVER"));
        mcp_cancel->setText(ChatAgentPanel::tr("CANCEL"));
        mcp_list->setAccessibleName(ChatAgentPanel::tr("MCP servers"));
        if (mcp_total_tools < 0)
            mcp_hint->setText(ChatAgentPanel::tr("Model Context Protocol tool servers."));
        else
            mcp_hint->setText(ChatAgentPanel::tr("%1 server(s) | %2 tools").arg(mcp_servers.size()).arg(mcp_total_tools));
        update_mcp_buttons();
    }
}

void ChatAgentPanel::Impl::ensure_page(int index) {
    if (index < 0 || index >= kCapPageCount || built[static_cast<size_t>(index)] || !pages[static_cast<size_t>(index)])
        return;
    built[static_cast<size_t>(index)] = true;
    QWidget* page = pages[static_cast<size_t>(index)];
    auto* layout = static_cast<QVBoxLayout*>(page->layout());
    switch (index) {
        case kCapContext:
            context = new ChatContextPage(page);
            layout->addWidget(context);
            context->set_context_bar(context_bar);
            context->set_run(run_id);
            // A file row: the FILES page (it lists, previews and acts on every artefact).
            QObject::connect(context, &ChatContextPage::files_requested, q,
                             [this](const QString&) { q->show_page(QStringLiteral("files")); });
            QObject::connect(context, &ChatContextPage::mcp_state_changed, q, &ChatAgentPanel::mcp_state_changed);
            break;
        case kCapPlan:
            plan = new RunPlanPanel(page);
            layout->addWidget(plan);
            QObject::connect(plan, &RunPlanPanel::run_created, q, &ChatAgentPanel::run_selection_requested);
            QObject::connect(plan, &RunPlanPanel::conversation_requested, q, &ChatAgentPanel::run_open_requested);
            plan->set_run(run_id);
            break;
        case kCapActivity:
            activity = new RunActivityView(page);
            layout->addWidget(activity);
            activity->set_run(run_id);
            break;
        case kCapFiles:
            artefacts = new RunArtefactsPanel(page);
            layout->addWidget(artefacts);
            QObject::connect(artefacts, &RunArtefactsPanel::new_run_requested, q, &ChatAgentPanel::new_run_requested);
            QObject::connect(artefacts, &RunArtefactsPanel::open_screen_requested, q,
                             &ChatAgentPanel::open_screen_requested);
            artefacts->set_run(run_id);
            break;
        case kCapMemory:
            build_memory(page);
            retranslate();
            q->refresh_memory();
            break;
        case kCapMcp:
            build_mcp(page);
            retranslate();
            q->refresh_mcp_servers();
            break;
        default:
            break;
    }
}

void ChatAgentPanel::Impl::show_tab(int index) {
    if (index < 0 || index >= order.size())
        return;
    ensure_page(page_at(index));
    stack->setCurrentIndex(index);
}

// ── MEMORY ───────────────────────────────────────────────────────────────────

void ChatAgentPanel::Impl::build_memory(QWidget* page) {
    auto* layout = static_cast<QVBoxLayout*>(page->layout());
    auto* body = new QWidget(page);
    layout->addWidget(body);
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    mem_title = cap_label(body, "capTitle");
    mem_hint = cap_label(body, "capHint");
    vl->addWidget(mem_title);
    vl->addWidget(mem_hint);

    mem_list = new QListWidget(body);
    mem_list->setProperty("fincept_agent_risk_keys", "Delete=delete"); // agents: Delete deletes the selected memories
    mem_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mem_list->setWordWrap(true);
    vl->addWidget(mem_list, 1);

    mem_form = new QFrame(body);
    mem_form->setObjectName(QStringLiteral("capForm"));
    auto* fl = new QVBoxLayout(mem_form);
    fl->setContentsMargins(8, 6, 8, 6);
    fl->setSpacing(4);
    mem_key = new QLineEdit(mem_form);
    mem_key->setMaxLength(120);
    mem_value = new QLineEdit(mem_form);
    mem_value->setMaxLength(2000);
    mem_type = new QComboBox(mem_form);
    // Memory types are stored keys sent to the service — labels are translated, data is not.
    mem_type->addItem(QString(), QStringLiteral("fact"));
    mem_type->addItem(QString(), QStringLiteral("preference"));
    mem_type->addItem(QString(), QStringLiteral("entity"));
    auto* fb = new QHBoxLayout;
    fb->setSpacing(4);
    mem_save = cap_button(mem_form);
    mem_cancel = cap_button(mem_form);
    fb->addWidget(mem_save);
    fb->addWidget(mem_cancel);
    fb->addStretch();
    fl->addWidget(mem_key);
    fl->addWidget(mem_value);
    fl->addWidget(mem_type);
    fl->addLayout(fb);
    mem_form->setVisible(false);
    vl->addWidget(mem_form);

    mem_confirm.build(body, vl);

    mem_status = cap_label(body, "capStatus");
    mem_status->setVisible(false);
    vl->addWidget(mem_status);

    auto* buttons = new QWidget(body);
    auto* flow = new rv::RvFlowLayout(buttons, 4, 4);
    mem_add = cap_button(buttons);
    mem_del = cap_button(buttons);
    mem_del->setProperty("fincept_agent_risk", "delete"); // agents: deletes agent memories
    mem_clear = cap_button(buttons);
    mem_clear->setProperty("fincept_agent_risk", "delete"); // agents: deletes every agent memory
    mem_refresh = cap_button(buttons);
    for (QPushButton* b : {mem_add, mem_del, mem_clear, mem_refresh})
        flow->addWidget(b);
    vl->addWidget(buttons);

    QObject::connect(mem_list, &QListWidget::itemSelectionChanged, q, [this]() { update_memory_buttons(); });
    QObject::connect(mem_add, &QPushButton::clicked, q, [this]() {
        mem_form->setVisible(true);
        mem_key->clear();
        mem_value->clear();
        mem_type->setCurrentIndex(0);
        mem_key->setFocus();
        update_memory_buttons();
    });
    QObject::connect(mem_cancel, &QPushButton::clicked, q, [this]() {
        mem_form->setVisible(false);
        update_memory_buttons();
    });
    QObject::connect(mem_key, &QLineEdit::textChanged, q, [this]() { update_memory_buttons(); });
    QObject::connect(mem_value, &QLineEdit::textChanged, q, [this]() { update_memory_buttons(); });
    QObject::connect(mem_value, &QLineEdit::returnPressed, q, [this]() {
        if (mem_save->isEnabled())
            save_memory();
    });
    QObject::connect(mem_save, &QPushButton::clicked, q, [this]() { save_memory(); });
    QObject::connect(mem_refresh, &QPushButton::clicked, q, [this]() { q->refresh_memory(); });
    QObject::connect(mem_del, &QPushButton::clicked, q, [this]() {
        QListWidgetItem* item = mem_list->currentItem();
        const QString key = item ? item->data(Qt::UserRole).toString() : QString();
        if (key.isEmpty())
            return;
        QPointer<ChatAgentPanel> self(q);
        mem_confirm.ask(ChatAgentPanel::tr("Delete the memory entry “%1”?").arg(key),
                        ChatAgentPanel::tr("DELETE"), ChatAgentPanel::tr("KEEP"), [this, self, key]() {
                            if (!self)
                                return;
                            mem_busy = true;
                            update_memory_buttons();
                            ChatModeService::instance().delete_memory(key, [this, self](bool ok, QString err) {
                                if (!self)
                                    return;
                                mem_busy = false;
                                if (!ok) {
                                    LOG_WARN("ChatAgentPanel", "Delete memory failed: " + err);
                                    cap_set_status(mem_status, ChatAgentPanel::tr("Could not delete: %1").arg(err), true);
                                }
                                q->refresh_memory();
                            });
                        });
    });
    QObject::connect(mem_clear, &QPushButton::clicked, q, [this]() {
        QPointer<ChatAgentPanel> self(q);
        mem_confirm.ask(ChatAgentPanel::tr("Delete ALL memory entries? The agent forgets everything it learned about "
                                           "you."),
                        ChatAgentPanel::tr("CLEAR ALL"), ChatAgentPanel::tr("KEEP"), [this, self]() {
                            if (!self)
                                return;
                            mem_busy = true;
                            update_memory_buttons();
                            ChatModeService::instance().clear_all_memory([this, self](bool ok, QString err) {
                                if (!self)
                                    return;
                                mem_busy = false;
                                if (!ok) {
                                    LOG_WARN("ChatAgentPanel", "Clear memory failed: " + err);
                                    cap_set_status(mem_status, ChatAgentPanel::tr("Could not clear: %1").arg(err), true);
                                }
                                q->refresh_memory();
                            });
                        });
    });
    new rv::RvKeyHook(mem_list, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() != Qt::Key_Delete || !mem_del->isEnabled())
            return false;
        if (apply)
            mem_del->click();
        return true;
    });
    for (QWidget* w : {static_cast<QWidget*>(mem_key), static_cast<QWidget*>(mem_value), static_cast<QWidget*>(mem_type)})
        new rv::RvKeyHook(w, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply) {
                mem_form->setVisible(false);
                mem_add->setFocus();
                update_memory_buttons();
            }
            return true;
        });
}

void ChatAgentPanel::Impl::update_memory_buttons() {
    if (!mem_list)
        return;
    QListWidgetItem* item = mem_list->currentItem();
    const bool has = item && !item->data(Qt::UserRole).toString().isEmpty() && item->isSelected();
    mem_del->setEnabled(has && !mem_busy);
    mem_del->setToolTip(has ? ChatAgentPanel::tr("Delete the selected entry")
                            : ChatAgentPanel::tr("Select an entry to delete it"));
    bool any = false;
    for (int i = 0; i < mem_list->count() && !any; ++i)
        any = !mem_list->item(i)->data(Qt::UserRole).toString().isEmpty();
    mem_clear->setEnabled(any && !mem_busy);
    mem_clear->setToolTip(any ? ChatAgentPanel::tr("Delete every memory entry")
                              : ChatAgentPanel::tr("There is nothing to clear"));
    mem_add->setEnabled(!mem_form->isVisible());
    mem_add->setToolTip(mem_form->isVisible() ? ChatAgentPanel::tr("Finish the entry below first")
                                              : ChatAgentPanel::tr("Add a memory entry"));
    const bool complete = !mem_key->text().trimmed().isEmpty() && !mem_value->text().trimmed().isEmpty();
    mem_save->setEnabled(complete && !mem_busy);
    mem_save->setToolTip(complete ? ChatAgentPanel::tr("Save the entry")
                                  : ChatAgentPanel::tr("Fill in both the key and the value"));
    mem_refresh->setEnabled(!mem_busy);
}

void ChatAgentPanel::Impl::save_memory() {
    const QString key = mem_key->text().trimmed();
    const QString value = mem_value->text().trimmed();
    const QString type = mem_type->currentData().toString();
    if (key.isEmpty() || value.isEmpty())
        return;
    mem_busy = true;
    update_memory_buttons();
    cap_set_status(mem_status, ChatAgentPanel::tr("Saving…"), false);
    QPointer<ChatAgentPanel> self(q);
    ChatModeService::instance().save_memory(key, value, type, [this, self](bool ok, QString err) {
        if (!self)
            return;
        mem_busy = false;
        if (!ok) {
            LOG_WARN("ChatAgentPanel", "Save memory failed: " + err);
            cap_set_status(mem_status, ChatAgentPanel::tr("Could not save: %1").arg(err), true);
            update_memory_buttons();
            return;
        }
        mem_form->setVisible(false);
        cap_set_status(mem_status, QString(), false);
        q->refresh_memory();
    });
}

void ChatAgentPanel::Impl::fill_memory(bool ok, const QVector<AgentMemory>& memories, const QString& err) {
    mem_list->clear();
    if (!ok) {
        LOG_WARN("ChatAgentPanel", "Memory load failed: " + err);
        cap_placeholder(mem_list, ChatAgentPanel::tr("Not available."), err);
        cap_set_status(mem_status, ChatAgentPanel::tr("The memory could not be read: %1").arg(err), true);
    } else if (memories.isEmpty()) {
        cap_placeholder(mem_list, ChatAgentPanel::tr("No entries yet."));
    } else {
        for (const AgentMemory& m : memories) {
            auto* item = new QListWidgetItem(QStringLiteral("[%1] %2: %3").arg(m.memory_type, m.key, m.value), mem_list);
            item->setData(Qt::UserRole, m.key);
            item->setToolTip(m.value);
        }
    }
    update_memory_buttons();
}

// ── MCP ──────────────────────────────────────────────────────────────────────

void ChatAgentPanel::Impl::build_mcp(QWidget* page) {
    auto* layout = static_cast<QVBoxLayout*>(page->layout());
    auto* body = new QWidget(page);
    layout->addWidget(body);
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    mcp_title = cap_label(body, "capTitle");
    mcp_hint = cap_label(body, "capHint");
    vl->addWidget(mcp_title);
    vl->addWidget(mcp_hint);

    mcp_list = new QListWidget(body);
    mcp_list->setProperty("fincept_agent_risk_keys", "Delete=account"); // agents: Delete removes an MCP server from the agents' tools
    mcp_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mcp_list->setWordWrap(true);
    vl->addWidget(mcp_list, 1);

    mcp_form = new QFrame(body);
    mcp_form->setObjectName(QStringLiteral("capForm"));
    auto* fl = new QVBoxLayout(mcp_form);
    fl->setContentsMargins(8, 6, 8, 6);
    fl->setSpacing(4);
    mcp_form_hint = cap_label(mcp_form, "capHint");
    mcp_name = new QLineEdit(mcp_form);
    mcp_name->setMaxLength(48);
    mcp_transport = new QComboBox(mcp_form);
    // Labels translated; the data values are protocol identifiers.
    mcp_transport->addItem(QString(), QStringLiteral("streamable-http"));
    mcp_transport->addItem(QString(), QStringLiteral("sse"));
    mcp_url = new QLineEdit(mcp_form);
    auto* fb = new QHBoxLayout;
    fb->setSpacing(4);
    mcp_save = cap_button(mcp_form);
    mcp_save->setProperty("fincept_agent_risk", "account"); // agents: adds / changes an MCP server the agents may call
    mcp_cancel = cap_button(mcp_form);
    fb->addWidget(mcp_save);
    fb->addWidget(mcp_cancel);
    fb->addStretch();
    fl->addWidget(mcp_form_hint);
    fl->addWidget(mcp_name);
    fl->addWidget(mcp_transport);
    fl->addWidget(mcp_url);
    fl->addLayout(fb);
    mcp_form->setVisible(false);
    vl->addWidget(mcp_form);

    mcp_confirm.build(body, vl);

    mcp_status = cap_label(body, "capStatus");
    mcp_status->setVisible(false);
    vl->addWidget(mcp_status);

    auto* buttons = new QWidget(body);
    auto* flow = new rv::RvFlowLayout(buttons, 4, 4);
    mcp_add = cap_button(buttons);
    mcp_refresh = cap_button(buttons);
    mcp_del = cap_button(buttons);
    mcp_del->setProperty("fincept_agent_risk", "account"); // agents: removes an MCP server from the agents' tools
    for (QPushButton* b : {mcp_add, mcp_refresh, mcp_del})
        flow->addWidget(b);
    vl->addWidget(buttons);

    QObject::connect(mcp_list, &QListWidget::itemSelectionChanged, q, [this]() { update_mcp_buttons(); });
    QObject::connect(mcp_add, &QPushButton::clicked, q, [this]() {
        mcp_form->setVisible(true);
        mcp_name->clear();
        mcp_url->setText(QStringLiteral("https://"));
        mcp_transport->setCurrentIndex(0);
        mcp_name->setFocus();
        update_mcp_buttons();
    });
    QObject::connect(mcp_cancel, &QPushButton::clicked, q, [this]() {
        mcp_form->setVisible(false);
        update_mcp_buttons();
    });
    QObject::connect(mcp_name, &QLineEdit::textChanged, q, [this]() { update_mcp_buttons(); });
    QObject::connect(mcp_url, &QLineEdit::textChanged, q, [this]() { update_mcp_buttons(); });
    QObject::connect(mcp_url, &QLineEdit::returnPressed, q, [this]() {
        if (mcp_save->isEnabled())
            save_mcp();
    });
    QObject::connect(mcp_save, &QPushButton::clicked, q, [this]() { save_mcp(); });
    QObject::connect(mcp_refresh, &QPushButton::clicked, q, [this]() {
        mcp_busy = true;
        update_mcp_buttons();
        cap_set_status(mcp_status, ChatAgentPanel::tr("Reconnecting the servers…"), false);
        QPointer<ChatAgentPanel> self(q);
        ChatModeService::instance().refresh_mcp_servers(
            [this, self](bool ok, QVector<McpServer> servers, int total_tools, QString err) {
                if (!self)
                    return;
                mcp_busy = false;
                if (!ok) {
                    LOG_WARN("ChatAgentPanel", "Refresh MCP failed: " + err);
                    cap_set_status(mcp_status, ChatAgentPanel::tr("Could not refresh: %1").arg(err), true);
                    update_mcp_buttons();
                    return;
                }
                cap_set_status(mcp_status, QString(), false);
                fill_mcp(true, servers, total_tools, {});
            });
    });
    QObject::connect(mcp_del, &QPushButton::clicked, q, [this]() {
        QListWidgetItem* item = mcp_list->currentItem();
        if (!item || item->data(Qt::UserRole + 1).toString() == QLatin1String("global"))
            return; // shared server: not the user's to remove (the button is disabled for it)
        const QString name = item->data(Qt::UserRole).toString();
        if (name.isEmpty())
            return;
        QPointer<ChatAgentPanel> self(q);
        mcp_confirm.ask(ChatAgentPanel::tr("Remove the server “%1”? The cloud agent loses its tools.").arg(name),
                        ChatAgentPanel::tr("REMOVE"), ChatAgentPanel::tr("KEEP"), [this, self, name]() {
                            if (!self)
                                return;
                            mcp_busy = true;
                            update_mcp_buttons();
                            ChatModeService::instance().delete_mcp_server(name, [this, self](bool ok, QString err) {
                                if (!self)
                                    return;
                                mcp_busy = false;
                                if (!ok) {
                                    LOG_WARN("ChatAgentPanel", "Delete MCP server failed: " + err);
                                    cap_set_status(mcp_status, ChatAgentPanel::tr("Could not remove: %1").arg(err),
                                                   true);
                                }
                                q->refresh_mcp_servers();
                            });
                        });
    });
    new rv::RvKeyHook(mcp_list, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() != Qt::Key_Delete || !mcp_del->isEnabled())
            return false;
        if (apply)
            mcp_del->click();
        return true;
    });
    for (QWidget* w : {static_cast<QWidget*>(mcp_name), static_cast<QWidget*>(mcp_transport),
                       static_cast<QWidget*>(mcp_url)})
        new rv::RvKeyHook(w, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply) {
                mcp_form->setVisible(false);
                mcp_add->setFocus();
                update_mcp_buttons();
            }
            return true;
        });
}

void ChatAgentPanel::Impl::update_mcp_buttons() {
    if (!mcp_list)
        return;
    QListWidgetItem* item = mcp_list->currentItem();
    const bool has = item && item->isSelected() && !item->data(Qt::UserRole).toString().isEmpty();
    const bool shared = has && item->data(Qt::UserRole + 1).toString() == QLatin1String("global");
    mcp_del->setEnabled(has && !shared && !mcp_busy);
    mcp_del->setToolTip(!has     ? ChatAgentPanel::tr("Select a server to remove it")
                        : shared ? ChatAgentPanel::tr("Shared server, managed by Fincept: it cannot be removed")
                                 : ChatAgentPanel::tr("Remove the selected server"));
    mcp_add->setEnabled(!mcp_form->isVisible() && !mcp_busy);
    mcp_refresh->setEnabled(!mcp_busy);
    static const QRegularExpression kName(QStringLiteral("^[a-z0-9][a-z0-9-]{1,47}$"));
    const bool name_ok = kName.match(mcp_name->text().trimmed()).hasMatch();
    const QUrl url(mcp_url->text().trimmed(), QUrl::StrictMode);
    const bool url_ok = url.isValid() && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 &&
                        !url.host().isEmpty();
    mcp_save->setEnabled(name_ok && url_ok && !mcp_busy);
    mcp_save->setToolTip(!name_ok  ? ChatAgentPanel::tr("Name: 2-48 characters of lowercase letters, digits and '-'")
                         : !url_ok ? ChatAgentPanel::tr("Enter a full https:// URL, e.g. https://mcp.example.com/mcp")
                                   : ChatAgentPanel::tr("Add the server"));
}

void ChatAgentPanel::Impl::save_mcp() {
    QJsonObject config;
    config[QStringLiteral("transport")] = mcp_transport->currentData().toString();
    config[QStringLiteral("url")] = mcp_url->text().trimmed();
    const QString name = mcp_name->text().trimmed();
    mcp_busy = true;
    update_mcp_buttons();
    cap_set_status(mcp_status, ChatAgentPanel::tr("Adding and connecting the server…"), false);
    QPointer<ChatAgentPanel> self(q);
    ChatModeService::instance().add_mcp_server(name, config, [this, self](bool ok, McpServer srv, QString err) {
        if (!self)
            return;
        mcp_busy = false;
        if (!ok) {
            cap_set_status(mcp_status, err, true); // the backend's own explanation
            update_mcp_buttons();
            return;
        }
        mcp_form->setVisible(false);
        if (srv.status == QLatin1String("error"))
            cap_set_status(mcp_status,
                           ChatAgentPanel::tr("'%1' was added but could not be connected: %2")
                               .arg(srv.name, srv.error.isEmpty() ? ChatAgentPanel::tr("connection failed") : srv.error),
                           true);
        else
            cap_set_status(mcp_status, ChatAgentPanel::tr("'%1' was added.").arg(srv.name), false);
        q->refresh_mcp_servers();
    });
}

void ChatAgentPanel::Impl::fill_mcp(bool ok, const QVector<McpServer>& servers, int total_tools, const QString& err) {
    mcp_list->clear();
    if (!ok) {
        LOG_WARN("ChatAgentPanel", "MCP load failed: " + err);
        mcp_total_tools = -1;
        mcp_servers.clear();
        cap_placeholder(mcp_list, ChatAgentPanel::tr("Not available."), err);
        mcp_hint->setText(ChatAgentPanel::tr("MCP not available."));
        update_mcp_buttons();
        emit q->mcp_state_changed(-1, -1);
        return;
    }
    mcp_servers = servers;
    mcp_total_tools = total_tools;
    mcp_hint->setText(ChatAgentPanel::tr("%1 server(s) | %2 tools").arg(servers.size()).arg(total_tools));
    if (servers.isEmpty())
        cap_placeholder(mcp_list, ChatAgentPanel::tr("No servers connected."));
    for (const McpServer& s : servers) {
        auto* item = new QListWidgetItem(mcp_list);
        QString name = s.name;
        if (s.is_shared())
            name += ChatAgentPanel::tr(" (shared)");
        item->setText(ChatAgentPanel::tr("%1\n  %2 | %3 tools").arg(name, s.status).arg(s.tools_count));
        item->setData(Qt::UserRole, s.name);
        item->setData(Qt::UserRole + 1, s.scope);
        item->setData(kCapStatusRole, s.status);
        QString tip = s.is_shared() ? ChatAgentPanel::tr("Shared server, managed by Fincept") : QString();
        if (!s.error.isEmpty())
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + s.error;
        item->setToolTip(tip);
        item->setForeground(cap_mcp_color(s.status));
    }
    update_mcp_buttons();
    int connected = 0;
    for (const McpServer& s : servers)
        if (s.status == QLatin1String("connected"))
            ++connected;
    emit q->mcp_state_changed(connected, static_cast<int>(servers.size()));
}

// ── ChatAgentPanel ───────────────────────────────────────────────────────────

ChatAgentPanel::ChatAgentPanel(QWidget* parent, AgentPanelPages pages)
    : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->kind = pages;
    d_->build();
    d_->retranslate();
    // Server rows carry their status colour as an item brush. (The panel's own
    // stylesheet is kept in step by the theme manager's re-style pass: no
    // setStyleSheet() in a theme_changed handler.)
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        if (!d_->mcp_list)
            return;
        for (int i = 0; i < d_->mcp_list->count(); ++i) {
            QListWidgetItem* item = d_->mcp_list->item(i);
            const QVariant status = item->data(kCapStatusRole);
            if (status.isValid())
                item->setForeground(cap_mcp_color(status.toString()));
        }
    });
    // The page picked last (any window) — only the shown page is built, on show.
    const int last = std::max(0, d_->tab_of(d_->initial_page()));
    {
        const QSignalBlocker block(d_->tabs);
        d_->tabs->setCurrentIndex(last);
    }
    d_->stack->setCurrentIndex(last);
}

ChatAgentPanel::~ChatAgentPanel() = default;

AgentPanelPages ChatAgentPanel::pages() const {
    return d_->kind;
}

void ChatAgentPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Nothing loads before the panel is on screen (no request before login).
    const int index = d_->tabs->currentIndex();
    const int page = d_->page_at(index);
    if (page < 0)
        return;
    if (!d_->built[static_cast<size_t>(page)]) {
        d_->show_tab(index); // builds it (and loads memory / MCP for those pages)
        return;
    }
    if (page == kCapMemory)
        refresh_memory();
    else if (page == kCapMcp)
        refresh_mcp_servers();
}

void ChatAgentPanel::set_context_bar(ComposerContextBar* bar) {
    d_->context_bar = bar;
    if (d_->context)
        d_->context->set_context_bar(bar);
}

bool ChatAgentPanel::show_page(const QString& id) {
    const int tab = d_->tab_of(cap_page_from_id(id.trimmed().toLower()));
    if (tab < 0)
        return false;
    if (d_->tabs->currentIndex() != tab) {
        d_->tabs->setCurrentIndex(tab); // currentChanged: builds, remembers, page_changed()
    } else {
        // Already the current tab: make sure it is built (a panel not shown yet builds on show).
        if (isVisible())
            d_->show_tab(tab);
    }
    return true;
}

QString ChatAgentPanel::current_page() const {
    return QString::fromLatin1(cap_page_id(d_->page_at(d_->tabs->currentIndex())));
}

void ChatAgentPanel::set_tabs_visible(bool visible) {
    if (d_->tabs->isVisibleTo(this) != visible)
        d_->tabs->setVisible(visible);
}

void ChatAgentPanel::focus_page() {
    const int index = d_->tabs->currentIndex();
    const int page = d_->page_at(index);
    if (page < 0)
        return;
    d_->show_tab(index);
    switch (page) {
        case kCapContext:
            if (d_->context) {
                d_->context->focus_first();
                return;
            }
            break;
        case kCapMemory:
            if (d_->mem_list) {
                d_->mem_list->setFocus(Qt::OtherFocusReason);
                return;
            }
            break;
        case kCapMcp:
            if (d_->mcp_list) {
                d_->mcp_list->setFocus(Qt::OtherFocusReason);
                return;
            }
            break;
        default:
            break;
    }
    // PLAN / ACTIVITY / FILES: the first control that takes the Tab key — a list first.
    QWidget* host = d_->pages[static_cast<size_t>(page)];
    QWidget* first = nullptr;
    for (QWidget* w : host->findChildren<QWidget*>()) {
        if (!w->isVisibleTo(host) || !w->isEnabled() || !(w->focusPolicy() & Qt::TabFocus))
            continue;
        if (qobject_cast<QAbstractItemView*>(w)) {
            first = w;
            break;
        }
        if (!first)
            first = w;
    }
    if (first)
        first->setFocus(Qt::OtherFocusReason);
    else
        d_->tabs->setFocus(Qt::OtherFocusReason);
}

void ChatAgentPanel::refresh_memory() {
    if (!d_->mem_list)
        return; // the tab was never opened
    cap_set_status(d_->mem_status, QString(), false);
    QPointer<ChatAgentPanel> self = this;
    ChatModeService::instance().list_memory([self](bool ok, QVector<AgentMemory> memories, QString err) {
        if (self)
            self->d_->fill_memory(ok, memories, err);
    });
}

void ChatAgentPanel::refresh_mcp_servers() {
    if (!d_->mcp_list)
        return;
    QPointer<ChatAgentPanel> self = this;
    ChatModeService::instance().list_mcp_servers(
        [self](bool ok, QVector<McpServer> servers, int total_tools, QString err) {
            if (self)
                self->d_->fill_mcp(ok, servers, total_tools, err);
        });
}

void ChatAgentPanel::set_current_run(const QString& run_id) {
    d_->run_id = run_id;
    if (d_->context)
        d_->context->set_run(run_id);
    if (d_->plan)
        d_->plan->set_run(run_id);
    if (d_->activity)
        d_->activity->set_run(run_id);
    if (d_->artefacts)
        d_->artefacts->set_run(run_id);
}

QString ChatAgentPanel::current_run() const {
    return d_->run_id;
}

void ChatAgentPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        d_->retranslate();
    QWidget::changeEvent(event);
}

} // namespace fincept::chat_mode
