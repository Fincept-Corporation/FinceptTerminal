// AgentRail.cpp — the agents screen's left rail (see the header).

#include "screens/agent_config/AgentRail.h"

#include "screens/agent_config/AgentRunCommon.h"
#include "screens/agent_config/AgentRunListLogic.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agents/AgentService.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHash>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QSet>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <functional>

namespace fincept::screens {

namespace ca = services::coding_agents;

namespace agent_rail_detail {

enum Tone { kDim = 0, kOk = 1, kWarn = 2, kDanger = 3, kBusy = 4, kAccent = 5 };

QColor tone_color(int tone) {
    switch (tone) {
        case kOk:
            return QColor(ui::colors::SUCCESS());
        case kWarn:
            return QColor(ui::colors::WARNING());
        case kDanger:
            return QColor(ui::colors::DANGER());
        case kBusy:
            return QColor(ui::colors::INFO());
        case kAccent:
            return QColor(ui::colors::AMBER());
        default:
            break;
    }
    return QColor(ui::colors::TEXT_TERTIARY());
}

int harness_tone(AwbHarnessTone t) {
    switch (t) {
        case AwbHarnessTone::Ready:
            return kOk;
        case AwbHarnessTone::Warning:
            return kWarn;
        case AwbHarnessTone::Danger:
            return kDanger;
        case AwbHarnessTone::Busy:
            return kBusy;
        case AwbHarnessTone::Absent:
            break;
    }
    return kDim;
}

QStandardItem* rail_item(const QString& text, const QString& kind, const QString& id, const QString& chip = {},
                         int tone = kDim) {
    auto* it = new QStandardItem(text);
    it->setEditable(false);
    it->setData(kind, AgentRail::kKindRole);
    it->setData(id, AgentRail::kIdRole);
    it->setData(chip, AgentRail::kChipRole);
    it->setData(tone, AgentRail::kToneRole);
    return it;
}

QString rail_css() {
    using namespace ui::colors;
    return QStringLiteral("QTreeView#agentRailTree { background:%1; color:%2; border:none; outline:none; }"
                          "QTreeView#agentRailTree::item { border:none; }"
                          "QTreeView#agentRailTree::branch { background:%1; }"
                          "QWidget#agentRail { background:%1; border-right:1px solid %3; }"
                          "QLabel#agentRailTitle { color:%4; font-weight:700; letter-spacing:1px; background:transparent; }")
        .arg(BG_SURFACE(), TEXT_PRIMARY(), BORDER_MED(), TEXT_SECONDARY());
}

} // namespace agent_rail_detail

using namespace agent_rail_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Delegate
// ═════════════════════════════════════════════════════════════════════════════

QSize AgentRailDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(s.height(), QFontMetrics(ui::fonts::font(-1)).height() + 8));
    return s;
}

void AgentRailDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString kind = index.data(AgentRail::kKindRole).toString();
    const QString chip = index.data(AgentRail::kChipRole).toString();
    const int tone = index.data(AgentRail::kToneRole).toInt();
    const QRect r = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);
    if (selected)
        painter->fillRect(r, QColor(ui::colors::ACCENT_BG()));
    else if (hovered && kind != QLatin1String("group"))
        painter->fillRect(r, QColor(ui::colors::BG_HOVER()));
    if (selected)
        painter->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::AMBER()));

    // Chip on the right.
    int right = r.right() - 6;
    if (!chip.isEmpty()) {
        QFont cf = ui::fonts::font(-5, QFont::Bold);
        painter->setFont(cf);
        const QFontMetrics fm(cf);
        const int w = qMin(fm.horizontalAdvance(chip) + 10, r.width() / 2);
        const int h = fm.height() + 2;
        const QRect box(right - w, r.center().y() - h / 2, w, h);
        const QColor c = tone_color(tone);
        painter->setPen(c);
        painter->drawRect(box.adjusted(0, 0, -1, -1));
        painter->drawText(box, Qt::AlignCenter, fm.elidedText(chip, Qt::ElideRight, w - 4));
        right = box.left() - 6;
    }

    // Text.
    QFont f = ui::fonts::font(-1);
    QColor fg(ui::colors::TEXT_PRIMARY());
    if (kind == QLatin1String("group")) {
        f = ui::fonts::font(-3, QFont::Bold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        fg = QColor(ui::colors::AMBER());
    } else if (kind == QLatin1String("category")) {
        f = ui::fonts::font(-3, QFont::Bold);
        fg = QColor(ui::colors::TEXT_SECONDARY());
    } else if (kind == QLatin1String("state")) {
        f = ui::fonts::font(-2);
        fg = QColor(ui::colors::TEXT_SECONDARY());
    } else if (kind == QLatin1String("action")) {
        f = ui::fonts::font(-2, QFont::Bold);
        fg = QColor(ui::colors::CYAN());
    }
    painter->setFont(f);
    painter->setPen(fg);
    const QRect text_rect(r.left() + 4, r.top(), qMax(10, right - r.left() - 4), r.height());
    painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(f).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, text_rect.width()));
    painter->restore();
}

// ═════════════════════════════════════════════════════════════════════════════
// Rail
// ═════════════════════════════════════════════════════════════════════════════

AgentRail::AgentRail(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("agentRail"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(rail_css()); // once, before any child exists

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 6, 0, 0);
    v->setSpacing(4);
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("agentRailTitle"));
    title_->setContentsMargins(8, 0, 8, 0);
    v->addWidget(title_);

    model_ = new QStandardItemModel(this);
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setRecursiveFilteringEnabled(true);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    tree_ = new QTreeView(this);
    tree_->setObjectName(QStringLiteral("agentRailTree"));
    tree_->setModel(proxy_);
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(true);
    tree_->setIndentation(QFontMetrics(ui::fonts::font()).averageCharWidth() * 2);
    tree_->setUniformRowHeights(true);
    tree_->setMouseTracking(true);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setItemDelegate(new AgentRailDelegate(tree_));
    tree_->setAccessibleName(tr("Agents, teams, coding agents and active runs"));
    v->addWidget(tree_, 1);

    connect(tree_, &QTreeView::clicked, this, [this](const QModelIndex& idx) {
        const QString kind = idx.data(kKindRole).toString();
        if (kind == QLatin1String("group") || kind == QLatin1String("category") || kind == QLatin1String("state")) {
            if (kind != QLatin1String("state"))
                tree_->setExpanded(idx, !tree_->isExpanded(idx));
            return;
        }
        current_kind_ = kind;
        current_id_ = idx.data(kIdRole).toString();
        emit item_activated(kind, current_id_);
    });
    connect(tree_, &QTreeView::activated, this, [this](const QModelIndex& idx) {
        const QString kind = idx.data(kKindRole).toString();
        if (kind == QLatin1String("group") || kind == QLatin1String("category") || kind == QLatin1String("state"))
            return;
        current_kind_ = kind;
        current_id_ = idx.data(kIdRole).toString();
        emit item_activated(kind, current_id_);
    });

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setSingleShot(true);
    refresh_timer_->setInterval(300);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (isVisible())
            rebuild();
        else
            dirty_ = true;
    });

    // Every source the rail shows; each change only arms the coalescing timer.
    auto& agents = services::AgentService::instance();
    connect(&agents, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo>, QVector<services::AgentCategory>) {
                discovering_ = false;
                discover_error_.clear();
                request_refresh();
            });
    connect(&agents, &services::AgentService::config_saved, this, [this]() { request_refresh(); });
    connect(&agents, &services::AgentService::config_deleted, this, [this]() { request_refresh(); });
    auto& registry = harness::HarnessRegistry::instance();
    connect(&registry, &harness::HarnessRegistry::status_changed, this, [this](const QString&) { request_refresh(); });
    connect(&registry, &harness::HarnessRegistry::defaults_changed, this, [this]() { request_refresh(); });
    auto& runs = ca::CodingAgentService::instance();
    connect(&runs, &ca::CodingAgentService::run_added, this, [this](const QString&) { request_refresh(); });
    connect(&runs, &ca::CodingAgentService::run_finished, this, [this](const QString&) { request_refresh(); });
    connect(&runs, &ca::CodingAgentService::runs_reset, this, [this]() { request_refresh(); });
    auto& tasks = agent_tasks::TaskRunService::instance();
    connect(&tasks, &agent_tasks::TaskRunService::run_added, this, [this](const QString&) { request_refresh(); });
    connect(&tasks, &agent_tasks::TaskRunService::run_finished, this, [this](const QString&) { request_refresh(); });
    connect(&tasks, &agent_tasks::TaskRunService::runs_reset, this, [this]() { request_refresh(); });

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &AgentRail::fit);
    title_->setText(tr("AGENTS · TEAMS · HARNESSES"));
    fit();
}

void AgentRail::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        title_->setText(tr("AGENTS · TEAMS · HARNESSES"));
        request_refresh();
    }
    QWidget::changeEvent(event);
}

void AgentRail::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (dirty_)
        rebuild();
}

void AgentRail::fit() {
    const QFontMetrics fm(ui::fonts::font());
    setMinimumWidth(fm.averageCharWidth() * 20);
    setMaximumWidth(fm.averageCharWidth() * 40);
    tree_->setIndentation(fm.averageCharWidth() * 2);
    tree_->doItemsLayout();
}

void AgentRail::request_refresh() {
    if (!refresh_timer_->isActive())
        refresh_timer_->start();
}

void AgentRail::set_discovery_state(bool discovering, const QString& error) {
    discovering_ = discovering;
    discover_error_ = error;
    request_refresh();
}

void AgentRail::set_filter_text(const QString& text) {
    proxy_->setFilterFixedString(text.trimmed());
    tree_->expandAll();
}

void AgentRail::set_current(const QString& kind, const QString& id) {
    current_kind_ = kind;
    current_id_ = id;
    std::function<QModelIndex(const QModelIndex&)> find = [&](const QModelIndex& parent) -> QModelIndex {
        for (int r = 0; r < proxy_->rowCount(parent); ++r) {
            const QModelIndex idx = proxy_->index(r, 0, parent);
            if (idx.data(kKindRole).toString() == kind && idx.data(kIdRole).toString() == id)
                return idx;
            const QModelIndex child = find(idx);
            if (child.isValid())
                return child;
        }
        return {};
    };
    const QModelIndex idx = find(QModelIndex());
    const QSignalBlocker block(tree_->selectionModel());
    if (idx.isValid()) {
        tree_->setCurrentIndex(idx);
        tree_->scrollTo(idx);
    } else {
        tree_->clearSelection();
    }
}

void AgentRail::rebuild() {
    dirty_ = false;
    QSet<QString> collapsed; // keep what the user folded
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        const QModelIndex g = proxy_->index(r, 0);
        if (!tree_->isExpanded(g))
            collapsed.insert(g.data(kIdRole).toString());
        for (int c = 0; c < proxy_->rowCount(g); ++c) {
            const QModelIndex sub = proxy_->index(c, 0, g);
            if (proxy_->rowCount(sub) > 0 && !tree_->isExpanded(sub))
                collapsed.insert(sub.data(kIdRole).toString());
        }
    }
    model_->clear();
    QStandardItem* root = model_->invisibleRootItem();

    // ── AGENTS (by category) ─────────────────────────────────────────────────
    const QVector<services::AgentInfo> agents = services::AgentService::instance().cached_agents();
    auto* agents_group = rail_item(tr("AGENTS"), QStringLiteral("group"), QStringLiteral("g:agents"),
                                   agents.isEmpty() ? QString() : QString::number(agents.size()), kDim);
    root->appendRow(agents_group);
    if (agents.isEmpty()) {
        if (discovering_)
            agents_group->appendRow(rail_item(tr("Discovering agents…"), QStringLiteral("state"), {}, {}, kBusy));
        else if (!discover_error_.isEmpty()) {
            auto* err = rail_item(tr("Agent discovery failed"), QStringLiteral("state"), {}, tr("ERROR"), kDanger);
            err->setToolTip(discover_error_);
            agents_group->appendRow(err);
            agents_group->appendRow(rail_item(tr("Retry discovery"), QStringLiteral("action"),
                                              QStringLiteral("retry_discovery")));
        } else {
            agents_group->appendRow(rail_item(tr("Create your first agent"), QStringLiteral("action"),
                                              QStringLiteral("new_agent")));
        }
        agents_group->appendRow(rail_item(tr("Start a run from a template"), QStringLiteral("action"),
                                          QStringLiteral("templates")));
    } else {
        QMap<QString, QStandardItem*> categories;
        for (const services::AgentInfo& a : agents) {
            const QString cat = a.category.isEmpty() ? tr("Other") : a.category;
            QStandardItem* parent = categories.value(cat, nullptr);
            if (!parent) {
                parent = rail_item(cat.toUpper(), QStringLiteral("category"), QStringLiteral("c:") + cat);
                categories.insert(cat, parent);
            }
            auto* it = rail_item(a.name, QStringLiteral("agent"), a.id, a.provider.toUpper(), kDim);
            it->setToolTip(a.description);
            parent->appendRow(it);
        }
        for (auto it = categories.cbegin(); it != categories.cend(); ++it) {
            it.value()->setData(QString::number(it.value()->rowCount()), kChipRole);
            agents_group->appendRow(it.value());
        }
    }

    // ── TEAMS ────────────────────────────────────────────────────────────────
    auto* teams_group = rail_item(tr("TEAMS"), QStringLiteral("group"), QStringLiteral("g:teams"));
    root->appendRow(teams_group);
    const auto teams = AgentConfigRepository::instance().list_by_category(QStringLiteral("team"));
    if (teams.is_ok() && !teams.value().isEmpty()) {
        teams_group->setData(QString::number(teams.value().size()), kChipRole);
        for (const auto& t : teams.value())
            teams_group->appendRow(rail_item(t.name, QStringLiteral("team"), t.id));
    } else {
        teams_group->appendRow(
            rail_item(tr("Build a team of agents"), QStringLiteral("action"), QStringLiteral("teams")));
    }

    // ── HARNESSES (coding agents) ────────────────────────────────────────────
    auto& registry = harness::HarnessRegistry::instance();
    auto& runs = ca::CodingAgentService::instance();
    QHash<QString, int> running_by_harness;
    QStringList active_harness_runs;
    for (const QString& id : runs.run_ids()) {
        const ca::CodingAgentRun* r = runs.find_run(id);
        if (!r || ca::is_terminal_status(r->status))
            continue;
        active_harness_runs << id;
        if (r->status == ca::RunStatus::Running)
            running_by_harness[r->harness] += 1;
    }
    const QStringList installed = registry.installed_ids();
    auto* harness_group = rail_item(tr("CODING AGENTS"), QStringLiteral("group"), QStringLiteral("g:harnesses"),
                                    tr("%1/%2").arg(installed.size()).arg(registry.ids().size()), kDim);
    root->appendRow(harness_group);
    for (const QString& id : registry.ids()) {
        const AwbHarnessState state = awb_harness_state(id);
        QString chip = state.word;
        int tone = harness_tone(state.tone);
        if (const int n = running_by_harness.value(id); n > 0) {
            chip = tr("RUNNING %1").arg(n);
            tone = kAccent;
        }
        QString name = registry.display_name(id);
        if (const auto* a = registry.adapter(id); a && a->descriptor().beta)
            name += QStringLiteral(" · ") + tr("BETA");
        auto* it = rail_item(name, QStringLiteral("harness"), id, chip, tone);
        it->setToolTip(state.sentence);
        harness_group->appendRow(it);
    }
    if (installed.isEmpty())
        harness_group->appendRow(rail_item(tr("Connect a coding agent ›"), QStringLiteral("action"),
                                           QStringLiteral("agent_connect")));

    // ── ACTIVE RUNS (harness + Chat Mode; agentic tasks are in the run list) ──
    auto* active_group = rail_item(tr("ACTIVE RUNS"), QStringLiteral("group"), QStringLiteral("g:active"));
    root->appendRow(active_group);
    int active = 0;
    for (const QString& id : std::as_const(active_harness_runs)) {
        const ca::CodingAgentRun* r = runs.find_run(id);
        if (!r)
            continue;
        const bool running = r->status == ca::RunStatus::Running;
        auto* it = rail_item(r->title, QStringLiteral("run"), agent_runs::run_key(agent_runs::Engine::Harness, id),
                             running ? awb_status_text(ca::RunStatus::Running) : awb_status_text(r->status),
                             running ? kAccent : kBusy);
        it->setToolTip(awb_harness_name(r->harness));
        active_group->appendRow(it);
        ++active;
    }
    auto& tasks = agent_tasks::TaskRunService::instance();
    for (const QString& id : tasks.active_run_ids()) {
        const agent_tasks::TaskRun* r = tasks.find_run(id);
        if (!r)
            continue;
        auto* it = rail_item(r->title.isEmpty() ? r->prompt.left(80) : r->title, QStringLiteral("run"),
                             agent_runs::run_key(agent_runs::Engine::Chat, id), tr("CHAT"), kBusy);
        active_group->appendRow(it);
        ++active;
    }
    if (active > 0)
        active_group->setData(QString::number(active), kChipRole);
    else
        active_group->appendRow(rail_item(tr("Open the run list"), QStringLiteral("action"), QStringLiteral("runs")));

    tree_->expandAll();
    std::function<void(const QModelIndex&)> fold = [&](const QModelIndex& parent) {
        for (int r = 0; r < proxy_->rowCount(parent); ++r) {
            const QModelIndex idx = proxy_->index(r, 0, parent);
            if (collapsed.contains(idx.data(kIdRole).toString()))
                tree_->setExpanded(idx, false);
            fold(idx);
        }
    };
    fold(QModelIndex());
    if (!current_kind_.isEmpty())
        set_current(current_kind_, current_id_);
}

} // namespace fincept::screens
