// src/screens/agent_config/ToolsViewPanel.cpp
#include "screens/agent_config/ToolsViewPanel.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Constructor ──────────────────────────────────────────────────────────────

ToolsViewPanel::ToolsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("ToolsViewPanel");
    build_ui();
    setup_connections();
}

// ── UI ───────────────────────────────────────────────────────────────────────

void ToolsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM));

    splitter->addWidget(build_selected_panel());
    splitter->addWidget(build_browser_panel());
    splitter->setSizes({240, 600});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter);
}

// ── Left: selected tools ─────────────────────────────────────────────────────

QWidget* ToolsViewPanel::build_selected_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(200);
    panel->setMaximumWidth(320);
    panel->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    // Header
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("SELECTED");
    title->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    header->addWidget(title);

    selected_count_ = new QLabel("0");
    selected_count_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                       .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    header->addWidget(selected_count_);
    header->addStretch();

    auto* clear_btn = new QPushButton("CLEAR");
    clear_btn->setCursor(Qt::PointingHandCursor);
    clear_btn->setStyleSheet(
        QString("QPushButton { background:transparent;color:%1;border:1px solid %1;padding:2px 8px;"
                "font-size:9px;font-weight:600; } QPushButton:hover { background:%2; }")
            .arg(ui::colors::NEGATIVE, ui::colors::BG_HOVER));
    connect(clear_btn, &QPushButton::clicked, this, [this]() {
        selected_tools_.clear();
        selected_list_->clear();
        selected_count_->setText("0");
        emit tools_selection_changed(selected_tools_);
    });
    header->addWidget(clear_btn);
    vl->addLayout(header);

    selected_list_ = new QListWidget;
    selected_list_->setStyleSheet(QString("QListWidget { background:%1;border:1px solid %2;color:%3;font-size:12px; }"
                                          "QListWidget::item { padding:4px 8px;border-bottom:1px solid %2; }"
                                          "QListWidget::item:selected { background:%4; }"
                                          "QListWidget::item:hover { background:%5; }")
                                      .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                           ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(selected_list_, 1);

    // Remove button
    auto* remove_btn = new QPushButton("REMOVE SELECTED");
    remove_btn->setCursor(Qt::PointingHandCursor);
    remove_btn->setStyleSheet(QString("QPushButton { background:%1;color:%2;border:1px solid %3;padding:5px;"
                                      "font-size:10px;font-weight:600; } QPushButton:hover { background:%3; }")
                                  .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    connect(remove_btn, &QPushButton::clicked, this, [this]() {
        auto* item = selected_list_->currentItem();
        if (item)
            remove_tool(item->text());
    });
    vl->addWidget(remove_btn);

    return panel;
}

// ── Center: tool browser ─────────────────────────────────────────────────────

QWidget* ToolsViewPanel::build_browser_panel() {
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Header
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("AVAILABLE TOOLS");
    title->setStyleSheet(
        QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    header->addWidget(title);

    total_count_ = new QLabel;
    total_count_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                    .arg(ui::colors::CYAN, ui::colors::BG_RAISED));
    header->addWidget(total_count_);
    header->addStretch();
    vl->addLayout(header);

    // Search
    search_edit_ = new QLineEdit;
    search_edit_->setPlaceholderText("Search tools...");
    search_edit_->setStyleSheet(
        QString("QLineEdit { background:%1;color:%2;border:1px solid %3;padding:6px 10px;font-size:12px; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    vl->addWidget(search_edit_);

    // Tree
    tool_tree_ = new QTreeWidget;
    tool_tree_->setHeaderHidden(true);
    tool_tree_->setRootIsDecorated(true);
    tool_tree_->setStyleSheet(QString("QTreeWidget { background:%1;border:1px solid %2;color:%3;font-size:12px; }"
                                      "QTreeWidget::item { padding:3px 4px; }"
                                      "QTreeWidget::item:selected { background:%4; }"
                                      "QTreeWidget::item:hover { background:%5; }"
                                      "QTreeWidget::branch:has-children:!has-siblings:closed,"
                                      "QTreeWidget::branch:closed:has-children:has-siblings { image:none; }")
                                  .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                       ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(tool_tree_, 1);

    // Add button
    auto* add_btn = new QPushButton("ADD TO SELECTED");
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setStyleSheet(QString("QPushButton { background:%1;color:%2;border:none;padding:8px;"
                                   "font-size:11px;font-weight:700;letter-spacing:1px; }"
                                   "QPushButton:hover { background:%3; }")
                               .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    connect(add_btn, &QPushButton::clicked, this, [this]() {
        auto* item = tool_tree_->currentItem();
        if (item && !item->childCount()) // leaf = tool, not category
            add_tool(item->text(0));
    });
    vl->addWidget(add_btn);

    // Copy button with feedback
    auto* btn_row = new QHBoxLayout;
    copy_btn_ = new QPushButton("COPY NAME");
    copy_btn_->setCursor(Qt::PointingHandCursor);
    copy_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:6px;"
                                     "font-size:10px;font-weight:600;}QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        auto* item = tool_tree_->currentItem();
        if (item && !item->childCount())
            copy_tool_name(item->text(0));
    });
    btn_row->addWidget(copy_btn_);
    btn_row->addStretch();
    vl->addLayout(btn_row);

    return panel;
}

// ── Connections ──────────────────────────────────────────────────────────────

void ToolsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::tools_loaded, this, &ToolsViewPanel::populate_tools);

    connect(search_edit_, &QLineEdit::textChanged, this, &ToolsViewPanel::filter_tools);

    // Double-click to add
    connect(tool_tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item && !item->childCount())
            add_tool(item->text(0));
    });
}

// ── Tool population ──────────────────────────────────────────────────────────

void ToolsViewPanel::populate_tools(const services::AgentToolsInfo& info) {
    all_tools_ = info;
    total_count_->setText(QString::number(info.total_count));
    filter_tools(search_edit_->text());
}

void ToolsViewPanel::filter_tools(const QString& query) {
    tool_tree_->clear();
    QString q = query.toLower().trimmed();

    for (const auto& cat : all_tools_.categories) {
        QJsonArray tools = all_tools_.tools[cat].toArray();

        QList<QTreeWidgetItem*> matching;
        for (const auto& t : tools) {
            QString name = t.toString();
            if (q.isEmpty() || name.toLower().contains(q)) {
                auto* child = new QTreeWidgetItem({name});
                child->setForeground(0, QColor(ui::colors::TEXT_PRIMARY));
                matching.append(child);
            }
        }

        if (matching.isEmpty())
            continue;

        auto* cat_item = new QTreeWidgetItem({QString("%1 (%2)").arg(cat.toUpper()).arg(matching.size())});
        cat_item->setForeground(0, QColor(ui::colors::AMBER));
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);
        cat_item->addChildren(matching);
        tool_tree_->addTopLevelItem(cat_item);

        if (!q.isEmpty())
            cat_item->setExpanded(true); // auto-expand when searching
    }

    // Also merge MCP internal tools
    auto mcp_tools = mcp::McpService::instance().get_all_tools();
    if (!mcp_tools.empty()) {
        QList<QTreeWidgetItem*> mcp_matching;
        for (const auto& tool : mcp_tools) {
            if (q.isEmpty() || tool.name.toLower().contains(q)) {
                auto* child = new QTreeWidgetItem({tool.name});
                child->setToolTip(0, tool.description);
                child->setForeground(0, QColor(ui::colors::TEXT_PRIMARY));
                mcp_matching.append(child);
            }
        }
        if (!mcp_matching.isEmpty()) {
            auto* mcp_cat = new QTreeWidgetItem({QString("MCP TOOLS (%1)").arg(mcp_matching.size())});
            mcp_cat->setForeground(0, QColor(ui::colors::CYAN));
            mcp_cat->setFlags(mcp_cat->flags() & ~Qt::ItemIsSelectable);
            mcp_cat->addChildren(mcp_matching);
            tool_tree_->addTopLevelItem(mcp_cat);
            if (!q.isEmpty())
                mcp_cat->setExpanded(true);
        }
    }
}

// ── Selection management ─────────────────────────────────────────────────────

void ToolsViewPanel::add_tool(const QString& tool_name) {
    if (selected_tools_.contains(tool_name))
        return;

    selected_tools_.append(tool_name);
    auto* item = new QListWidgetItem(tool_name);
    selected_list_->addItem(item);
    selected_count_->setText(QString::number(selected_tools_.size()));
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
    emit tools_selection_changed(selected_tools_);
}

// ── Copy to clipboard ────────────────────────────────────────────────────────

void ToolsViewPanel::copy_tool_name(const QString& tool_name) {
    QApplication::clipboard()->setText(tool_name);
    copy_btn_->setText("COPIED!");
    QTimer::singleShot(1500, this, [this]() { copy_btn_->setText("COPY NAME"); });
}

// ── Visibility ───────────────────────────────────────────────────────────────

void ToolsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
        services::AgentService::instance().list_tools();
    }
}

} // namespace fincept::screens
