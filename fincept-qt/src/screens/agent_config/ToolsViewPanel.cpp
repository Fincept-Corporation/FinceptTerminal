// src/screens/agent_config/ToolsViewPanel.cpp
#include "screens/agent_config/ToolsViewPanel.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Stylesheet helpers ───────────────────────────────────────────────────────

static QString panel_style(const QString& border_side = "right") {
    return QString("background:%1;border-%2:1px solid %3;")
        .arg(ui::colors::BG_SURFACE(), border_side, ui::colors::BORDER_DIM());
}

static QString section_title_style() {
    return QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER());
}

static QString badge_style(const QString& color = ui::colors::CYAN) {
    return QString("color:%1;font-size:10px;background:%2;padding:1px 6px;"
                   "border-radius:2px;font-weight:600;")
        .arg(color, ui::colors::BG_RAISED());
}

static QString list_style() {
    return QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                   "QListWidget::item{padding:5px 8px;border-bottom:1px solid %2;}"
                   "QListWidget::item:selected{background:%4;color:%3;}"
                   "QListWidget::item:hover{background:%5;}")
        .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(),
             ui::colors::BG_HOVER());
}

static QString tree_style() {
    return QString("QTreeWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                   "QTreeWidget::item{padding:4px 6px;}"
                   "QTreeWidget::item:selected{background:%4;color:%3;}"
                   "QTreeWidget::item:hover{background:%5;}"
                   "QTreeWidget::branch{background:%1;}")
        .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER_DIM(),
             ui::colors::BG_HOVER());
}

static QString btn_primary_style() {
    return QString("QPushButton{background:%1;color:%2;border:none;padding:7px;"
                   "font-size:11px;font-weight:700;letter-spacing:1px;}"
                   "QPushButton:hover{background:%3;}"
                   "QPushButton:disabled{background:%4;color:%5;}")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::ORANGE(), ui::colors::BG_RAISED(), ui::colors::TEXT_DIM());
}

static QString btn_secondary_style() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                   "padding:5px 10px;font-size:10px;font-weight:600;}"
                   "QPushButton:hover{background:%3;color:%4;}"
                   "QPushButton:disabled{color:%5;border-color:%6;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(),
             ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM());
}

static QString btn_danger_style() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                   "padding:5px 10px;font-size:10px;font-weight:600;}"
                   "QPushButton:hover{background:%1;color:%2;}")
        .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY());
}

static QString input_style() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;"
                   "padding:6px 10px;font-size:12px;}"
                   "QLineEdit:focus{border-color:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER());
}

static QString radio_style() {
    return QString("QRadioButton{color:%1;font-size:11px;spacing:6px;}"
                   "QRadioButton::indicator{width:12px;height:12px;border:1px solid %2;"
                   "background:%3;}"
                   "QRadioButton::indicator:checked{background:%4;border-color:%4;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::BG_RAISED(), ui::colors::AMBER());
}

static QString combo_style() {
    return QString("QComboBox{background:%1;color:%2;border:1px solid %3;"
                   "padding:5px 8px;font-size:11px;}"
                   "QComboBox:focus{border-color:%4;}"
                   "QComboBox::drop-down{border:none;width:20px;}"
                   "QComboBox QAbstractItemView{background:%1;color:%2;"
                   "border:1px solid %3;selection-background-color:%5;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER(),
             ui::colors::AMBER_DIM());
}

static QString textedit_style() {
    return QString("QTextEdit{background:%1;color:%2;border:1px solid %3;"
                   "font-size:12px;padding:4px;}"
                   "QScrollBar:vertical{background:%1;width:6px;}"
                   "QScrollBar::handle:vertical{background:%3;}")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM());
}

static QFrame* h_line() {
    auto* f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet(QString("background:%1;border:none;max-height:1px;").arg(ui::colors::BORDER_DIM()));
    return f;
}

static QLabel* section_header(const QString& text) {
    auto* l = new QLabel(text);
    l->setStyleSheet(section_title_style());
    return l;
}

// ── Constructor ──────────────────────────────────────────────────────────────

ToolsViewPanel::ToolsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("ToolsViewPanel");
    build_ui();
    setup_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });

    // Seed from cache immediately so the combo is populated without waiting
    // for the agents_discovered signal (handles lazy construction race).
    auto cached = services::AgentService::instance().cached_agents();
    if (!cached.isEmpty()) {
        agent_configs_ = cached;
        load_assign_targets();
    }
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void ToolsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));

    splitter->addWidget(build_left_panel());
    splitter->addWidget(build_center_panel());
    splitter->addWidget(build_right_panel());
    splitter->setSizes({220, 460, 280});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);

    root->addWidget(splitter);
}

// ── Left panel ───────────────────────────────────────────────────────────────

QWidget* ToolsViewPanel::build_left_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(180);
    panel->setMaximumWidth(280);
    panel->setStyleSheet(panel_style("right"));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Assign To ────────────────────────────────────────────────────────────
    vl->addWidget(section_header("ASSIGN TO"));

    auto* radio_row = new QHBoxLayout;
    radio_row->setSpacing(12);
    radio_agent_ = new QRadioButton("Agent");
    radio_team_ = new QRadioButton("Team");
    radio_agent_->setChecked(true);
    radio_agent_->setStyleSheet(radio_style());
    radio_team_->setStyleSheet(radio_style());
    radio_row->addWidget(radio_agent_);
    radio_row->addWidget(radio_team_);
    radio_row->addStretch();
    vl->addLayout(radio_row);

    target_combo_ = new QComboBox;
    target_combo_->setStyleSheet(combo_style());
    target_combo_->setPlaceholderText("Select target...");
    vl->addWidget(target_combo_);

    target_status_ = new QLabel("No target selected");
    target_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    target_status_->setWordWrap(true);
    vl->addWidget(target_status_);

    vl->addWidget(h_line());

    // ── Selected Tools ────────────────────────────────────────────────────────
    auto* sel_header = new QHBoxLayout;
    sel_header->addWidget(section_header("SELECTED TOOLS"));
    selected_count_ = new QLabel("0");
    selected_count_->setStyleSheet(badge_style());
    sel_header->addWidget(selected_count_);
    sel_header->addStretch();
    vl->addLayout(sel_header);

    selected_list_ = new QListWidget;
    selected_list_->setStyleSheet(list_style());
    selected_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    vl->addWidget(selected_list_, 1);

    // ── Actions ───────────────────────────────────────────────────────────────
    remove_btn_ = new QPushButton("REMOVE");
    remove_btn_->setCursor(Qt::PointingHandCursor);
    remove_btn_->setStyleSheet(btn_secondary_style());
    remove_btn_->setEnabled(false);
    vl->addWidget(remove_btn_);

    auto* bottom_row = new QHBoxLayout;
    bottom_row->setSpacing(6);

    clear_btn_ = new QPushButton("CLEAR");
    clear_btn_->setCursor(Qt::PointingHandCursor);
    clear_btn_->setStyleSheet(btn_danger_style());
    bottom_row->addWidget(clear_btn_);

    assign_btn_ = new QPushButton("ASSIGN →");
    assign_btn_->setCursor(Qt::PointingHandCursor);
    assign_btn_->setStyleSheet(btn_primary_style());
    assign_btn_->setEnabled(false);
    assign_btn_->setToolTip("Save selected tools to the chosen agent or team");
    bottom_row->addWidget(assign_btn_);

    vl->addLayout(bottom_row);

    return panel;
}

// ── Center panel ─────────────────────────────────────────────────────────────

QWidget* ToolsViewPanel::build_center_panel() {
    auto* panel = new QWidget(this);
    panel->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(6);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("AVAILABLE TOOLS");
    title->setStyleSheet(section_title_style());
    header->addWidget(title);

    total_count_ = new QLabel("0");
    total_count_->setStyleSheet(badge_style(ui::colors::TEXT_SECONDARY));
    header->addWidget(total_count_);
    header->addStretch();

    auto* hint = new QLabel("● = already assigned");
    hint->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    header->addWidget(hint);
    vl->addLayout(header);

    // ── Search ────────────────────────────────────────────────────────────────
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search tools...");
    search_edit_->setStyleSheet(input_style());
    search_edit_->setClearButtonEnabled(true);
    vl->addWidget(search_edit_);

    // ── Tree ─────────────────────────────────────────────────────────────────
    tool_tree_ = new QTreeWidget;
    tool_tree_->setHeaderHidden(true);
    tool_tree_->setRootIsDecorated(true);
    tool_tree_->setStyleSheet(tree_style());
    tool_tree_->setUniformRowHeights(true);
    vl->addWidget(tool_tree_, 1);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(6);

    add_btn_ = new QPushButton("+ ADD TO SELECTION");
    add_btn_->setCursor(Qt::PointingHandCursor);
    add_btn_->setStyleSheet(btn_primary_style());
    btn_row->addWidget(add_btn_, 1);

    copy_btn_ = new QPushButton("COPY NAME");
    copy_btn_->setCursor(Qt::PointingHandCursor);
    copy_btn_->setStyleSheet(btn_secondary_style());
    btn_row->addWidget(copy_btn_);

    vl->addLayout(btn_row);

    return panel;
}

// ── Right panel ───────────────────────────────────────────────────────────────

QWidget* ToolsViewPanel::build_right_panel() {
    auto* panel = new QWidget(this);
    panel->setMinimumWidth(220);
    panel->setMaximumWidth(320);
    panel->setStyleSheet(panel_style("left"));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    // ── Tool name ─────────────────────────────────────────────────────────────
    vl->addWidget(section_header("TOOL DETAIL"));

    detail_name_ = new QLabel("Select a tool");
    detail_name_->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:700;padding:2px 0;").arg(ui::colors::TEXT_PRIMARY()));
    detail_name_->setWordWrap(true);
    vl->addWidget(detail_name_);

    detail_category_ = new QLabel();
    detail_category_->setStyleSheet(badge_style(ui::colors::CYAN));
    detail_category_->hide();
    vl->addWidget(detail_category_);

    vl->addWidget(h_line());

    // ── Description ───────────────────────────────────────────────────────────
    vl->addWidget(section_header("DESCRIPTION"));
    detail_desc_ = new QTextEdit;
    detail_desc_->setReadOnly(true);
    detail_desc_->setStyleSheet(textedit_style());
    detail_desc_->setFixedHeight(90);
    detail_desc_->setPlaceholderText("Click a tool to see details...");
    vl->addWidget(detail_desc_);

    vl->addWidget(h_line());

    // ── Parameters ────────────────────────────────────────────────────────────
    vl->addWidget(section_header("PARAMETERS"));
    detail_params_ = new QTextEdit;
    detail_params_->setReadOnly(true);
    detail_params_->setStyleSheet(textedit_style());
    detail_params_->setFixedHeight(110);
    detail_params_->setPlaceholderText("—");
    vl->addWidget(detail_params_);

    vl->addWidget(h_line());

    // ── Used By ───────────────────────────────────────────────────────────────
    vl->addWidget(section_header("USED BY"));
    detail_used_by_ = new QTextEdit;
    detail_used_by_->setReadOnly(true);
    detail_used_by_->setStyleSheet(textedit_style());
    detail_used_by_->setPlaceholderText("No agents or teams use this tool yet.");
    vl->addWidget(detail_used_by_, 1);

    return panel;
}

// ── Connections ───────────────────────────────────────────────────────────────

void ToolsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::tools_loaded, this, &ToolsViewPanel::populate_tools);

    connect(&svc, &services::AgentService::agents_discovered, this, [this](const QVector<services::AgentInfo>& agents) {
        agent_configs_ = agents;
        load_assign_targets();
        update_assigned_dots();
    });

    connect(search_edit_, &QLineEdit::textChanged, this, &ToolsViewPanel::filter_tools);

    // Tool tree — single click = detail, double click = add
    connect(tool_tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
        if (item && !item->childCount())
            show_tool_detail(item->text(0).split(' ').first());
    });

    connect(tool_tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item && !item->childCount())
            add_tool(item->text(0).split(' ').first());
    });

    // Selected list — enable/disable remove button
    connect(selected_list_, &QListWidget::currentRowChanged, this,
            [this](int row) { remove_btn_->setEnabled(row >= 0); });

    // Add button
    connect(add_btn_, &QPushButton::clicked, this, [this]() {
        auto* item = tool_tree_->currentItem();
        if (item && !item->childCount())
            add_tool(item->text(0).split(' ').first());
    });

    // Remove button
    connect(remove_btn_, &QPushButton::clicked, this, [this]() {
        auto* item = selected_list_->currentItem();
        if (item)
            remove_tool(item->text());
    });

    // Clear button
    connect(clear_btn_, &QPushButton::clicked, this, &ToolsViewPanel::clear_selection);

    // Assign button
    connect(assign_btn_, &QPushButton::clicked, this, &ToolsViewPanel::assign_to_target);

    // Copy button
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        auto* item = tool_tree_->currentItem();
        if (item && !item->childCount())
            copy_tool_name(item->text(0).split(' ').first());
    });

    // Radio buttons — reload combo instantly from cached data when switching agent/team
    connect(radio_agent_, &QRadioButton::toggled, this, [this](bool) {
        load_assign_targets();
        update_assigned_dots();
    });

    // Target combo — update status label
    connect(target_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx < 0) {
            target_status_->setText("No target selected");
            assign_btn_->setEnabled(false);
            return;
        }
        QString name = target_combo_->currentText();
        QString type = radio_agent_->isChecked() ? "Agent" : "Team";
        target_status_->setText(QString("Assigning to: %1 (%2)").arg(name, type));
        assign_btn_->setEnabled(!selected_tools_.isEmpty());
        update_assigned_dots();
    });
}

// ── Show event — lazy load ────────────────────────────────────────────────────

void ToolsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        services::AgentService::instance().list_tools();
        services::AgentService::instance().discover_agents();
    }
}

// ── Populate tools ────────────────────────────────────────────────────────────

void ToolsViewPanel::populate_tools(const services::AgentToolsInfo& info) {
    all_tools_ = info;
    total_count_->setText(QString::number(info.total_count));
    filter_tools(search_edit_->text());
}

void ToolsViewPanel::filter_tools(const QString& query) {
    tool_tree_->clear();
    QString q = query.toLower().trimmed();

    QStringList assigned = tools_of_target();
    int total = 0;

    // Agent service tools grouped by category
    for (const auto& cat : all_tools_.categories) {
        QJsonArray tools = all_tools_.tools[cat].toArray();
        QList<QTreeWidgetItem*> matching;

        for (const auto& t : tools) {
            QString name = t.toString();
            if (!q.isEmpty() && !name.toLower().contains(q))
                continue;

            QString display = assigned.contains(name) ? QString("%1 ●").arg(name) : name;

            auto* child = new QTreeWidgetItem({display});
            child->setForeground(0,
                                 QColor(assigned.contains(name) ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()));
            child->setToolTip(0, name);
            matching.append(child);
        }

        if (matching.isEmpty())
            continue;

        auto* cat_item = new QTreeWidgetItem({QString("%1  [%2]").arg(cat.toUpper()).arg(matching.size())});
        cat_item->setForeground(0, QColor(ui::colors::AMBER()));
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);
        cat_item->addChildren(matching);
        tool_tree_->addTopLevelItem(cat_item);
        if (!q.isEmpty())
            cat_item->setExpanded(true);
        total += matching.size();
    }

    // MCP internal tools
    auto mcp_tools = mcp::McpService::instance().get_all_tools();
    if (!mcp_tools.empty()) {
        // Group by category
        QMap<QString, QList<QTreeWidgetItem*>> by_cat;
        for (const auto& tool : mcp_tools) {
            if (!q.isEmpty() && !tool.name.toLower().contains(q))
                continue;
            // Skip duplicates already shown from agent service
            bool already_shown = false;
            for (const auto& cat : all_tools_.categories) {
                if (all_tools_.tools[cat].toArray().contains(QJsonValue(tool.name))) {
                    already_shown = true;
                    break;
                }
            }
            if (already_shown)
                continue;

            QString cat = tool.is_internal ? "internal-mcp" : tool.server_name;
            QString display = assigned.contains(tool.name) ? QString("%1 ●").arg(tool.name) : tool.name;

            auto* child = new QTreeWidgetItem({display});
            child->setForeground(
                0, QColor(assigned.contains(tool.name) ? ui::colors::POSITIVE() : ui::colors::TEXT_PRIMARY()));
            child->setToolTip(0, tool.description.isEmpty() ? tool.name : tool.description);
            by_cat[cat].append(child);
        }

        for (auto it = by_cat.begin(); it != by_cat.end(); ++it) {
            auto* cat_item = new QTreeWidgetItem({QString("%1  [%2]").arg(it.key().toUpper()).arg(it.value().size())});
            cat_item->setForeground(0, QColor(ui::colors::CYAN()));
            cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);
            cat_item->addChildren(it.value());
            tool_tree_->addTopLevelItem(cat_item);
            if (!q.isEmpty())
                cat_item->setExpanded(true);
            total += it.value().size();
        }
    }

    total_count_->setText(QString::number(total));
}

// ── Load assign targets ───────────────────────────────────────────────────────

void ToolsViewPanel::load_assign_targets() {
    target_combo_->clear();
    bool use_agents = radio_agent_->isChecked();

    if (use_agents) {
        for (const auto& info : agent_configs_)
            target_combo_->addItem(info.name, info.id);
    } else {
        // Teams are stored as AgentConfig rows with category == "team"
        auto result = AgentConfigRepository::instance().list_by_category("team");
        if (result.is_ok()) {
            for (const auto& cfg : result.value())
                target_combo_->addItem(cfg.name, cfg.id);
        }
    }

    if (target_combo_->count() == 0) {
        target_status_->setText(use_agents ? "No agents found — create one first"
                                           : "No teams found — create one first");
        assign_btn_->setEnabled(false);
    }
}

// ── Show tool detail ──────────────────────────────────────────────────────────

void ToolsViewPanel::show_tool_detail(const QString& tool_name) {
    if (tool_name.isEmpty())
        return;
    current_tool_ = tool_name;

    detail_name_->setText(tool_name);

    // Find description and params from MCP
    QString description;
    QString category;
    QString params_text;

    auto mcp_tools = mcp::McpService::instance().get_all_tools();
    for (const auto& t : mcp_tools) {
        if (t.name == tool_name) {
            description = t.description;
            category = t.is_internal ? "internal-mcp" : t.server_name;

            // Format parameters
            QJsonObject schema = t.input_schema;
            QJsonObject props = schema.value("properties").toObject();
            QJsonArray req = schema.value("required").toArray();

            if (!props.isEmpty()) {
                QStringList lines;
                for (auto it = props.begin(); it != props.end(); ++it) {
                    QJsonObject p = it.value().toObject();
                    QString type = p.value("type").toString("string");
                    QString desc = p.value("description").toString();
                    bool required = req.contains(QJsonValue(it.key()));
                    lines << QString("%1%2  %3  %4").arg(it.key()).arg(required ? "*" : "").arg(type).arg(desc);
                }
                params_text = lines.join("\n");
            } else {
                params_text = "No parameters";
            }
            break;
        }
    }

    // Also check agent service tools
    if (description.isEmpty()) {
        for (const auto& cat : all_tools_.categories) {
            if (all_tools_.tools[cat].toArray().contains(QJsonValue(tool_name))) {
                category = cat;
                break;
            }
        }
    }

    if (category.isEmpty())
        category = "—";
    detail_category_->setText(category.toUpper());
    detail_category_->setVisible(!category.isEmpty());
    detail_desc_->setPlainText(description.isEmpty() ? "No description available." : description);
    detail_params_->setPlainText(params_text.isEmpty() ? "—" : params_text);

    refresh_used_by(tool_name);
}

// ── Used by ───────────────────────────────────────────────────────────────────

void ToolsViewPanel::refresh_used_by(const QString& tool_name) {
    QStringList lines;
    for (const auto& info : agent_configs_) {
        QJsonArray tools = info.config.value("tools").toArray();
        for (const auto& t : tools) {
            if (t.toString() == tool_name) {
                lines << QString("● Agent: %1").arg(info.name);
                break;
            }
        }
    }
    detail_used_by_->setPlainText(lines.isEmpty() ? "Not assigned to any agent or team yet." : lines.join("\n"));
}

// ── Assigned dots ─────────────────────────────────────────────────────────────

void ToolsViewPanel::update_assigned_dots() {
    filter_tools(search_edit_->text());
}

// ── Selection management ──────────────────────────────────────────────────────

void ToolsViewPanel::add_tool(const QString& tool_name) {
    if (tool_name.isEmpty() || selected_tools_.contains(tool_name))
        return;

    selected_tools_.append(tool_name);
    selected_list_->addItem(tool_name);
    selected_count_->setText(QString::number(selected_tools_.size()));
    assign_btn_->setEnabled(target_combo_->currentIndex() >= 0);
    update_assigned_dots();
    emit tools_selection_changed(selected_tools_);
}

void ToolsViewPanel::remove_tool(const QString& tool_name) {
    selected_tools_.removeAll(tool_name);
    for (int i = 0; i < selected_list_->count(); ++i) {
        if (selected_list_->item(i)->text() == tool_name) {
            delete selected_list_->takeItem(i);
            break;
        }
    }
    selected_count_->setText(QString::number(selected_tools_.size()));
    assign_btn_->setEnabled(!selected_tools_.isEmpty() && target_combo_->currentIndex() >= 0);
    update_assigned_dots();
    emit tools_selection_changed(selected_tools_);
}

void ToolsViewPanel::clear_selection() {
    selected_tools_.clear();
    selected_list_->clear();
    selected_count_->setText("0");
    assign_btn_->setEnabled(false);
    update_assigned_dots();
    emit tools_selection_changed(selected_tools_);
}

// ── Assign to target ─────────────────────────────────────────────────────────

void ToolsViewPanel::assign_to_target() {
    if (selected_tools_.isEmpty() || target_combo_->currentIndex() < 0)
        return;

    QString id = target_combo_->currentData().toString();
    QString name = target_combo_->currentText();

    // Load existing config
    auto result = AgentConfigRepository::instance().get(id);
    if (!result.is_ok()) {
        target_status_->setText(QString("Error: could not load %1").arg(name));
        return;
    }

    AgentConfig cfg = result.value();
    QJsonDocument doc = QJsonDocument::fromJson(cfg.config_json.toUtf8());
    QJsonObject obj = doc.object();

    // Merge tools — keep existing, add new ones
    QJsonArray existing = obj.value("tools").toArray();
    QStringList all_tools;
    for (const auto& t : existing)
        all_tools << t.toString();
    for (const auto& t : selected_tools_) {
        if (!all_tools.contains(t))
            all_tools << t;
    }

    QJsonArray merged;
    for (const auto& t : all_tools)
        merged.append(t);
    obj["tools"] = merged;
    cfg.config_json = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    auto save_result = AgentConfigRepository::instance().save(cfg);
    if (save_result.is_ok()) {
        const QString msg = QString("✓ %1 tool(s) added to %2").arg(selected_tools_.size()).arg(name);
        target_status_->setText(msg);
        target_status_->setStyleSheet(
            QString("color:%1;font-size:11px;padding:2px 0;font-weight:600;").arg(ui::colors::POSITIVE()));
        LOG_INFO("ToolsViewPanel", msg);

        // Notify other panels to reload this agent's tool list immediately
        emit tool_assigned(id, all_tools);

        // Reload agent cache so "Used By" reflects the change
        services::AgentService::instance().discover_agents();

        // Restore neutral status after 5 seconds
        QTimer::singleShot(5000, this, [this]() {
            if (target_combo_->currentIndex() >= 0) {
                target_status_->setText(
                    QString("Target: %1 (%2)")
                        .arg(target_combo_->currentText(), radio_agent_->isChecked() ? "Agent" : "Team"));
                target_status_->setStyleSheet(
                    QString("color:%1;font-size:11px;padding:2px 0;").arg(ui::colors::TEXT_SECONDARY()));
            }
        });
    } else {
        target_status_->setText(QString("Error saving to %1").arg(name));
        target_status_->setStyleSheet(QString("color:%1;font-size:11px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ToolsViewPanel::current_target_name() const {
    return target_combo_->currentText();
}

QStringList ToolsViewPanel::tools_of_target() const {
    if (target_combo_->currentIndex() < 0)
        return {};

    QString id = target_combo_->currentData().toString();
    auto result = AgentConfigRepository::instance().get(id);
    if (!result.is_ok())
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(result.value().config_json.toUtf8());
    QJsonArray arr = doc.object().value("tools").toArray();
    QStringList out;
    for (const auto& t : arr)
        out << t.toString();
    return out;
}

void ToolsViewPanel::copy_tool_name(const QString& name) {
    QApplication::clipboard()->setText(name);
    copy_btn_->setText("COPIED!");
    QTimer::singleShot(1500, this, [this]() { copy_btn_->setText("COPY NAME"); });
}

} // namespace fincept::screens
