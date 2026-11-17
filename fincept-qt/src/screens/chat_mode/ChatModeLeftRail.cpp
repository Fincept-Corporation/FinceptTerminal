// ChatModeLeftRail.cpp — Chat Mode's left rail (2026-11-17 revamp).
//
//   RUNS · 2   CHATS   MEMORY   MCP        (a segmented switch; Left / Right move,
//   ─────────────────────────────────          Space / Enter / click pick)
//   [RunListPanel | ChatSessionPanel | ChatAgentPanel(Agent: MEMORY / MCP)]
//
// The RUNS segment carries the count of runs waiting for the user
// (TaskRunService::attention_changed). The section picked last is remembered in
// QSettings "chat_mode/left_section". One selector stylesheet set before any child
// exists; the strip's height follows the font (re-fitted on appearance_settled).

#include "screens/chat_mode/ChatModeLeftRail.h"

#include "screens/chat_mode/ChatAgentPanel.h"
#include "screens/chat_mode/ChatSessionPanel.h"
#include "screens/chat_mode/RunListPanel.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <array>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace chat_mode_left_rail_detail {

constexpr const char* kLrSectionKey = "chat_mode/left_section";

enum LrSection { kLrRuns = 0, kLrChats, kLrMemory, kLrMcp, kLrCount };

const std::array<const char*, kLrCount> kLrIds = {"runs", "conversations", "memory", "mcp"};

int lr_index_of(const QString& id) {
    for (int i = 0; i < kLrCount; ++i)
        if (id == QLatin1String(kLrIds[static_cast<size_t>(i)]))
            return i;
    if (id == QLatin1String("chats") || id == QLatin1String("sessions"))
        return kLrChats;
    if (id == QLatin1String("memories"))
        return kLrMemory;
    return -1;
}

QString lr_style() {
    return QStringLiteral("#chatModeLeftRail{background:%1;border-right:1px solid %2;}"
                          "#chatModeLeftRail QWidget#lrHeader{background:%1;border:none;border-bottom:1px solid %2;}"
                          "#chatModeLeftRail QPushButton#lrSegment{background:transparent;color:%3;border:none;"
                          "border-bottom:2px solid transparent;padding:0 7px;font-size:%4px;font-weight:700;"
                          "letter-spacing:1px;}"
                          "#chatModeLeftRail QPushButton#lrSegment:hover{color:%5;}"
                          "#chatModeLeftRail QPushButton#lrSegment:checked{color:%5;border-bottom:2px solid %6;}"
                          "#chatModeLeftRail QPushButton#lrSegment:focus{color:%5;background:%7;}"
                          "#chatModeLeftRail QPushButton#lrSegment[lrAlert=\"true\"]{color:%8;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(), ui::fonts::px(-3),
             ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::BG_RAISED(), ui::colors::WARNING());
}

} // namespace chat_mode_left_rail_detail

using namespace chat_mode_left_rail_detail;

struct ChatModeLeftRail::Impl {
    ChatModeLeftRail* q = nullptr;
    QWidget* header = nullptr;
    QButtonGroup* group = nullptr;
    std::array<QPushButton*, kLrCount> segments{};
    QStackedWidget* stack = nullptr;
    RunListPanel* runs = nullptr;
    ChatSessionPanel* chats = nullptr;
    ChatAgentPanel* agent = nullptr;
    int current = -1;
    int attention = 0;

    void build();
    void retranslate();
    void fit();
    void select(int index, bool user);
    void update_runs_segment();
};

void ChatModeLeftRail::Impl::build() {
    q->setObjectName(QStringLiteral("chatModeLeftRail"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(lr_style()); // before any child exists

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(0, 0, 1, 0); // the right hairline
    vl->setSpacing(0);

    header = new QWidget(q);
    header->setObjectName(QStringLiteral("lrHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(4, 0, 4, 0);
    hl->setSpacing(0);
    group = new QButtonGroup(q);
    group->setExclusive(true);
    for (int i = 0; i < kLrCount; ++i) {
        auto* b = new QPushButton(header);
        b->setObjectName(QStringLiteral("lrSegment"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::StrongFocus);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        group->addButton(b, i);
        hl->addWidget(b);
        segments[static_cast<size_t>(i)] = b;
        // Left / Right move along the switch and pick the segment (a tab strip's keys).
        new rv::RvKeyHook(b, [this, i](const QKeyEvent* ke, bool apply) {
            if (ke->modifiers() != Qt::NoModifier || (ke->key() != Qt::Key_Left && ke->key() != Qt::Key_Right))
                return false;
            if (apply) {
                const int next = (i + (ke->key() == Qt::Key_Right ? 1 : kLrCount - 1)) % kLrCount;
                select(next, true);
                segments[static_cast<size_t>(next)]->setFocus(Qt::TabFocusReason);
            }
            return true;
        });
    }
    hl->addStretch(1);
    vl->addWidget(header);

    stack = new QStackedWidget(q);
    runs = new RunListPanel(stack);
    chats = new ChatSessionPanel(stack);
    agent = new ChatAgentPanel(stack, AgentPanelPages::Agent);
    agent->set_tabs_visible(false); // the switch above picks MEMORY / MCP
    stack->addWidget(runs);
    stack->addWidget(chats);
    stack->addWidget(agent);
    vl->addWidget(stack, 1);

    QObject::connect(group, &QButtonGroup::idClicked, q, [this](int id) { select(id, true); });
    QObject::connect(&agt::TaskRunService::instance(), &agt::TaskRunService::attention_changed, q,
                     [this](int, int needs) {
                         attention = needs;
                         update_runs_segment();
                     });
    // Height and minimum width follow the font, once a new appearance has settled.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() { fit(); });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange)
            retranslate();
    });
}

void ChatModeLeftRail::Impl::fit() {
    const int h = ui::PanelHeader::strip_height();
    if (header->minimumHeight() != h || header->maximumHeight() != h)
        header->setFixedHeight(h);
    // Narrow enough to collapse cleanly in a splitter, wide enough for a run's title
    // beside its time: about 24 average characters of the list font.
    const int min_w = QFontMetrics(ui::fonts::font(-2)).averageCharWidth() * 24 + 24;
    if (q->minimumWidth() != min_w)
        q->setMinimumWidth(min_w);
}

void ChatModeLeftRail::Impl::retranslate() {
    const std::array<QString, kLrCount> tips = {
        ChatModeLeftRail::tr("Every run: messages, tasks, scheduled runs (Alt+1)"),
        ChatModeLeftRail::tr("Your cloud conversations"),
        ChatModeLeftRail::tr("What the cloud agent remembers about you"),
        ChatModeLeftRail::tr("Remote MCP tool servers of the cloud agent")};
    const std::array<QString, kLrCount> names = {ChatModeLeftRail::tr("Runs"), ChatModeLeftRail::tr("Conversations"),
                                                 ChatModeLeftRail::tr("Agent memory"),
                                                 ChatModeLeftRail::tr("MCP servers")};
    segments[kLrChats]->setText(ChatModeLeftRail::tr("CHATS"));
    segments[kLrMemory]->setText(ChatModeLeftRail::tr("MEMORY"));
    segments[kLrMcp]->setText(ChatModeLeftRail::tr("MCP"));
    for (int i = 0; i < kLrCount; ++i) {
        segments[static_cast<size_t>(i)]->setToolTip(tips[static_cast<size_t>(i)]);
        segments[static_cast<size_t>(i)]->setAccessibleName(names[static_cast<size_t>(i)]);
        segments[static_cast<size_t>(i)]->setAccessibleDescription(
            ChatModeLeftRail::tr("Left and Right arrows move between the sections"));
    }
    update_runs_segment();
}

void ChatModeLeftRail::Impl::update_runs_segment() {
    QPushButton* b = segments[kLrRuns];
    b->setText(attention > 0 ? ChatModeLeftRail::tr("RUNS · %1").arg(attention) : ChatModeLeftRail::tr("RUNS"));
    b->setAccessibleName(attention > 0 ? ChatModeLeftRail::tr("Runs, %n waiting for you", nullptr, attention)
                                       : ChatModeLeftRail::tr("Runs"));
    const bool alert = attention > 0;
    if (b->property("lrAlert").toBool() != alert) {
        // The tone is a stylesheet selector ([lrAlert="true"]): re-polish on a change.
        b->setProperty("lrAlert", alert);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

void ChatModeLeftRail::Impl::select(int index, bool user) {
    if (index < 0 || index >= kLrCount)
        return;
    {
        const QSignalBlocker block(group);
        segments[static_cast<size_t>(index)]->setChecked(true);
    }
    stack->setCurrentIndex(index >= kLrMemory ? 2 : index);
    if (index == kLrMemory || index == kLrMcp)
        agent->show_page(QLatin1String(kLrIds[static_cast<size_t>(index)]));
    if (index == current)
        return;
    current = index;
    if (user)
        QSettings().setValue(QLatin1String(kLrSectionKey), QLatin1String(kLrIds[static_cast<size_t>(index)]));
    emit q->section_changed(QLatin1String(kLrIds[static_cast<size_t>(index)]));
}

ChatModeLeftRail::ChatModeLeftRail(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->attention = agt::TaskRunService::instance().attention_count();
    d_->retranslate();
    d_->fit();
    const int saved = lr_index_of(QSettings().value(QLatin1String(kLrSectionKey)).toString());
    d_->select(saved >= 0 ? saved : kLrRuns, /*user=*/false);
}

ChatModeLeftRail::~ChatModeLeftRail() = default;

RunListPanel* ChatModeLeftRail::runs() const {
    return d_->runs;
}

ChatSessionPanel* ChatModeLeftRail::conversations() const {
    return d_->chats;
}

ChatAgentPanel* ChatModeLeftRail::agent() const {
    return d_->agent;
}

bool ChatModeLeftRail::show_section(const QString& id) {
    const int index = lr_index_of(id.trimmed().toLower());
    if (index < 0)
        return false;
    d_->select(index, /*user=*/true);
    return true;
}

QString ChatModeLeftRail::section() const {
    return d_->current >= 0 ? QString::fromLatin1(kLrIds[static_cast<size_t>(d_->current)]) : QString();
}

void ChatModeLeftRail::focus_section() {
    switch (d_->current) {
        case kLrRuns:
            d_->runs->focus_list();
            break;
        case kLrChats:
            d_->chats->focus_list();
            break;
        case kLrMemory:
        case kLrMcp:
            d_->agent->focus_page();
            break;
        default:
            break;
    }
}

} // namespace fincept::chat_mode
