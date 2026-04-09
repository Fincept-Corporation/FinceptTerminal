// src/screens/relationship_map/RelationshipMapScreen.cpp
#include "screens/relationship_map/RelationshipMapScreen.h"

#include "core/session/ScreenStateManager.h"
#include "screens/relationship_map/RelationshipGraphScene.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QGraphicsItem>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;
using namespace fincept::relmap;

static inline QString MF() {
    return QStringLiteral("font-family:'Consolas','Courier New',monospace;");
}

// ── Constructor ──────────────────────────────────────────────────────────────

RelationshipMapScreen::RelationshipMapScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    auto& svc = services::RelationshipMapService::instance();
    connect(&svc, &services::RelationshipMapService::data_ready, this, &RelationshipMapScreen::on_data_ready);
    connect(&svc, &services::RelationshipMapService::fetch_failed, this, &RelationshipMapScreen::on_fetch_failed);
    connect(&svc, &services::RelationshipMapService::progress_changed, this, &RelationshipMapScreen::on_progress);
}

void RelationshipMapScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
}

void RelationshipMapScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void RelationshipMapScreen::build_ui() {
    setStyleSheet(QString("QWidget#RelMapRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("RelMapRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header Bar ───────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(44);
    header->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(colors::BG_RAISED, colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(10);

    auto* title = new QLabel("CORPORATE INTELLIGENCE MAP");
    title->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::AMBER, MF()));
    hhl->addWidget(title);

    // Search
    search_input_ = new QLineEdit;
    search_input_->setPlaceholderText("Enter ticker (AAPL, MSFT, TSLA...)");
    search_input_->setFixedWidth(280);
    search_input_->setStyleSheet(
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                "padding: 4px 10px; font-size: 12px; %4 }"
                "QLineEdit:focus { border-color: %5; }")
            .arg(colors::BG_SURFACE, colors::TEXT_PRIMARY, colors::BORDER_DIM, MF(), colors::AMBER));
    connect(search_input_, &QLineEdit::returnPressed, this, &RelationshipMapScreen::on_search);
    hhl->addWidget(search_input_);

    auto* search_btn = new QPushButton("ANALYZE");
    search_btn->setCursor(Qt::PointingHandCursor);
    search_btn->setFixedHeight(28);
    search_btn->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.15); color: %1; border: 1px solid %3; "
                "padding: 0 14px; font-size: 11px; font-weight: 700; %2 }"
                "QPushButton:hover { background: %1; color: %4; }")
            .arg(colors::AMBER, MF(), colors::AMBER_DIM, colors::BG_BASE));
    connect(search_btn, &QPushButton::clicked, this, &RelationshipMapScreen::on_search);
    hhl->addWidget(search_btn);

    hhl->addStretch();

    // Layout selector
    layout_combo_ = new QComboBox;
    layout_combo_->addItem("LAYERED", (int)LayoutMode::Layered);
    layout_combo_->addItem("RADIAL", (int)LayoutMode::Radial);
    layout_combo_->addItem("FORCE", (int)LayoutMode::Force);
    layout_combo_->setStyleSheet(QString("QComboBox { background: %1; color: %2; border: 1px solid %3; "
                                         "padding: 3px 8px; font-size: 10px; %4 }")
                                     .arg(colors::BG_SURFACE, colors::TEXT_SECONDARY, colors::BORDER_DIM, MF()));
    connect(layout_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        layout_mode_ = static_cast<LayoutMode>(layout_combo_->itemData(idx).toInt());
        if (has_data_)
            rebuild_graph();
        ScreenStateManager::instance().notify_changed(this);
    });
    hhl->addWidget(layout_combo_);

    // Filter toggle
    filter_btn_ = new QPushButton("FILTERS");
    filter_btn_->setCursor(Qt::PointingHandCursor);
    filter_btn_->setCheckable(true);
    filter_btn_->setFixedHeight(28);
    filter_btn_->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid %2; "
                "padding: 0 10px; font-size: 10px; %3 }"
                "QPushButton:hover { color: %5; } "
                "QPushButton:checked { background: rgba(217,119,6,0.1); color: %4; border-color: %6; }")
            .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM, MF(), colors::AMBER, colors::TEXT_PRIMARY,
                 colors::AMBER_DIM));
    connect(filter_btn_, &QPushButton::toggled, this, [this](bool checked) { filter_panel_->setVisible(checked); });
    hhl->addWidget(filter_btn_);

    root->addWidget(header);

    // ── Progress bar ─────────────────────────────────────────────────────
    auto* prog_row = new QWidget(this);
    prog_row->setFixedHeight(20);
    prog_row->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    auto* phl = new QHBoxLayout(prog_row);
    phl->setContentsMargins(12, 2, 12, 2);

    progress_label_ = new QLabel("Ready");
    progress_label_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM, MF()));
    phl->addWidget(progress_label_);

    progress_bar_ = new QProgressBar;
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(false);
    progress_bar_->setFixedHeight(4);
    progress_bar_->setStyleSheet(QString("QProgressBar { background: %1; border: none; }"
                                         "QProgressBar::chunk { background: %2; }")
                                     .arg(colors::BG_RAISED, colors::AMBER));
    progress_bar_->hide();
    phl->addWidget(progress_bar_, 1);

    root->addWidget(prog_row);

    // ── Main content area ────────────────────────────────────────────────
    auto* content = new QWidget(this);
    auto* chl = new QHBoxLayout(content);
    chl->setContentsMargins(0, 0, 0, 0);
    chl->setSpacing(0);

    // Filter panel (left, hidden by default)
    filter_panel_ = build_filter_panel();
    filter_panel_->setVisible(false);
    chl->addWidget(filter_panel_);

    // Graph view (center)
    scene_ = new RelationshipGraphScene(this);
    view_ = new RelationshipGraphView(scene_, this);
    chl->addWidget(view_, 1);

    // Detail panel (right, always visible when node selected)
    detail_panel_ = build_detail_panel();
    detail_panel_->setVisible(false);
    chl->addWidget(detail_panel_);

    root->addWidget(content, 1);

    // Legend (floating over graph)
    legend_widget_ = build_legend();
    legend_widget_->setParent(view_);
    legend_widget_->move(10, 10);
    legend_widget_->hide();

    // ── Status bar ───────────────────────────────────────────────────────
    auto* status = new QWidget(this);
    status->setFixedHeight(24);
    status->setStyleSheet(
        QString("background: %1; border-top: 1px solid %2;").arg(colors::BG_RAISED, colors::BORDER_DIM));
    auto* shl = new QHBoxLayout(status);
    shl->setContentsMargins(12, 0, 12, 0);

    status_nodes_ = new QLabel("READY");
    status_nodes_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM, MF()));
    shl->addWidget(status_nodes_);

    shl->addStretch();

    status_quality_ = new QLabel("");
    status_quality_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::TEXT_DIM, MF()));
    shl->addWidget(status_quality_);

    shl->addStretch();

    status_brand_ = new QLabel("FINCEPT TERMINAL");
    status_brand_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: 700; %2").arg(colors::AMBER, MF()));
    shl->addWidget(status_brand_);

    root->addWidget(status);

    // Connect scene selection
    connect(scene_, &QGraphicsScene::selectionChanged, this, &RelationshipMapScreen::on_node_selected);
}

// ── Filter Panel ─────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_filter_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(200);
    panel->setStyleSheet(
        QString("background: %1; border-right: 1px solid %2;").arg(colors::BG_SURFACE, colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* title = new QLabel("FILTERS");
    title->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::AMBER, MF()));
    vl->addWidget(title);

    auto make_check = [&](const QString& label, bool& state, NodeCategory cat) {
        auto* cb = new QCheckBox(label);
        cb->setChecked(state);
        QColor col = category_color(cat);
        cb->setStyleSheet(QString("QCheckBox { color: %1; font-size: 11px; %2; spacing: 6px; }"
                                  "QCheckBox::indicator { width: 12px; height: 12px; "
                                  "border: 1px solid %3; background: transparent; }"
                                  "QCheckBox::indicator:checked { background: %3; }")
                              .arg(colors::TEXT_SECONDARY, MF(), col.name()));
        connect(cb, &QCheckBox::toggled, this, [this, &state](bool checked) {
            state = checked;
            if (has_data_)
                rebuild_graph();
        });
        vl->addWidget(cb);
    };

    make_check("Peers", filters_.show_peers, NodeCategory::Peer);
    make_check("Institutional", filters_.show_institutional, NodeCategory::Institutional);
    make_check("Insiders", filters_.show_insiders, NodeCategory::Insider);
    make_check("Events", filters_.show_events, NodeCategory::Event);
    make_check("Metrics", filters_.show_metrics, NodeCategory::Metrics);
    make_check("Supply Chain", filters_.show_supply_chain, NodeCategory::SupplyChain);

    vl->addStretch();
    return panel;
}

// ── Detail Panel ─────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_detail_panel() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(260);
    panel->setStyleSheet(
        QString("background: %1; border-left: 1px solid %2;").arg(colors::BG_SURFACE, colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* header = new QHBoxLayout;
    detail_title_ = new QLabel("SELECT A NODE");
    detail_title_->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: 700; %2").arg(colors::TEXT_PRIMARY, MF()));
    header->addWidget(detail_title_);
    header->addStretch();

    auto* close_btn = new QPushButton("X");
    close_btn->setFixedSize(20, 20);
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                     "font-size: 11px; %2 } QPushButton:hover { color: %3; }")
                                 .arg(colors::TEXT_DIM, MF(), colors::TEXT_PRIMARY));
    connect(close_btn, &QPushButton::clicked, this, [this]() {
        detail_panel_->hide();
        scene_->clearSelection();
    });
    header->addWidget(close_btn);
    vl->addLayout(header);

    detail_category_ = new QLabel("");
    detail_category_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: 700; "
                                            "letter-spacing: 0.5px; %2")
                                        .arg(colors::TEXT_TERTIARY, MF()));
    vl->addWidget(detail_category_);

    // Scrollable properties
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    detail_props_container_ = new QWidget(this);
    detail_props_container_->setStyleSheet("background: transparent;");
    new QVBoxLayout(detail_props_container_);

    scroll->setWidget(detail_props_container_);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Legend ────────────────────────────────────────────────────────────────────

QWidget* RelationshipMapScreen::build_legend() {
    auto* panel = new QWidget(this);
    panel->setFixedWidth(160);
    panel->setStyleSheet(QString("background: rgba(10,10,10,0.9); border: 1px solid %1; "
                                 "border-radius: 2px;")
                             .arg(colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(3);

    auto* title = new QLabel("LEGEND");
    title->setStyleSheet(QString("color: %1; font-size: 8px; font-weight: 700; "
                                 "letter-spacing: 0.5px; %2")
                             .arg(colors::TEXT_DIM, MF()));
    vl->addWidget(title);

    auto add_entry = [&](NodeCategory cat) {
        auto* row = new QWidget(this);
        row->setStyleSheet("background: transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);

        auto* dot = new QLabel;
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("background: %1; border-radius: 4px;").arg(category_color(cat).name()));
        hl->addWidget(dot);

        auto* lbl = new QLabel(category_label(cat));
        lbl->setStyleSheet(QString("color: %1; font-size: 8px; %2").arg(colors::TEXT_TERTIARY, MF()));
        hl->addWidget(lbl);
        hl->addStretch();
        vl->addWidget(row);
    };

    add_entry(NodeCategory::Company);
    add_entry(NodeCategory::Peer);
    add_entry(NodeCategory::Institutional);
    add_entry(NodeCategory::Insider);
    add_entry(NodeCategory::Metrics);
    add_entry(NodeCategory::Event);
    add_entry(NodeCategory::SupplyChain);

    return panel;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void RelationshipMapScreen::on_search() {
    QString ticker = search_input_->text().trimmed().toUpper();
    if (ticker.isEmpty())
        return;

    search_input_->setText(ticker);
    progress_bar_->show();
    progress_bar_->setValue(0);
    detail_panel_->hide();
    legend_widget_->hide();
    has_data_ = false;

    services::RelationshipMapService::instance().fetch(ticker);
    ScreenStateManager::instance().notify_changed(this);
}

void RelationshipMapScreen::on_progress(int percent, const QString& message) {
    progress_bar_->setValue(percent);
    progress_label_->setText(message);

    if (percent >= 100) {
        progress_bar_->hide();
    }
}

void RelationshipMapScreen::on_data_ready(const RelationshipData& data) {
    current_data_ = data;
    has_data_ = true;
    progress_bar_->hide();
    progress_label_->setText("Complete");
    legend_widget_->show();
    rebuild_graph();
    update_status_bar();

    // Fit view to content
    view_->fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void RelationshipMapScreen::on_fetch_failed(const QString& error) {
    progress_bar_->hide();
    progress_label_->setText("Error: " + error);
    progress_label_->setStyleSheet(QString("color: %1; font-size: 9px; %2").arg(colors::NEGATIVE, MF()));
}

void RelationshipMapScreen::rebuild_graph() {
    scene_->build_graph(current_data_, filters_, layout_mode_);
    update_status_bar();
}

void RelationshipMapScreen::on_node_selected() {
    auto items = scene_->selectedItems();
    if (items.isEmpty()) {
        detail_panel_->hide();
        return;
    }

    // Find the GraphNodeItem
    for (auto* item : items) {
        // Dynamic cast to our custom type
        auto* rect = dynamic_cast<QGraphicsRectItem*>(item);
        if (!rect)
            continue;

        // Access node data through scene items — we stored it in the item
        // Since GraphNodeItem is defined in the .cpp, we use a different approach:
        // Get the data from item's tooltip or child text items
        auto children = rect->childItems();
        QString label, category_text, sublabel;
        QMap<QString, QString> props;

        if (children.size() >= 2) {
            auto* badge_item = dynamic_cast<QGraphicsTextItem*>(children[0]);
            auto* label_item = dynamic_cast<QGraphicsTextItem*>(children[1]);
            if (badge_item)
                category_text = badge_item->toPlainText();
            if (label_item)
                label = label_item->toPlainText();
            if (children.size() >= 3) {
                auto* sub_item = dynamic_cast<QGraphicsTextItem*>(children[2]);
                if (sub_item)
                    sublabel = sub_item->toPlainText();
            }
        }

        detail_title_->setText(label.isEmpty() ? "NODE" : label);
        detail_category_->setText(category_text);
        detail_category_->setStyleSheet(
            QString("color: %1; font-size: 9px; font-weight: 700; letter-spacing: 0.5px; %2").arg(colors::AMBER, MF()));

        // Clear and repopulate properties
        auto* layout = detail_props_container_->layout();
        QLayoutItem* child;
        while ((child = layout->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }

        if (!sublabel.isEmpty()) {
            auto* sub = new QLabel(sublabel);
            sub->setWordWrap(true);
            sub->setStyleSheet(QString("color: %1; font-size: 11px; %2").arg(colors::TEXT_SECONDARY, MF()));
            layout->addWidget(sub);
        }

        // Look up full node data from current_data_ to show rich properties
        // (match by label/ticker)
        if (!label.isEmpty() && has_data_) {
            auto* sep = new QWidget(this);
            sep->setFixedHeight(1);
            sep->setStyleSheet(QString("background: %1;").arg(colors::BORDER_DIM));
            layout->addWidget(sep);

            // Try to match with company
            if (label == current_data_.company.ticker) {
                auto add_prop = [&](const QString& key, const QString& val) {
                    auto* row = new QLabel(QString("%1:  %2").arg(key, val));
                    row->setStyleSheet(QString("color: %1; font-size: 10px; %2").arg(colors::TEXT_SECONDARY, MF()));
                    layout->addWidget(row);
                };
                add_prop("Sector", current_data_.company.sector);
                add_prop("Industry", current_data_.company.industry);
                add_prop("Mkt Cap", QString("$%1B").arg(current_data_.company.market_cap / 1e9, 0, 'f', 1));
                add_prop("Price", QString("$%1").arg(current_data_.company.current_price, 0, 'f', 2));
                add_prop("P/E", QString::number(current_data_.company.pe_ratio, 'f', 1));
                add_prop("ROE", QString("%1%").arg(current_data_.company.roe * 100, 0, 'f', 1));
                add_prop("Growth", QString("%1%").arg(current_data_.company.revenue_growth * 100, 0, 'f', 1));
                add_prop("Margins", QString("%1%").arg(current_data_.company.profit_margins * 100, 0, 'f', 1));
                add_prop("Employees", QString::number(current_data_.company.employees));
                add_prop("Signal", current_data_.valuation.action);
            }

            // Try to match peer
            for (const auto& p : current_data_.peers) {
                if (p.ticker == label) {
                    auto add_prop = [&](const QString& key, const QString& val) {
                        auto* row = new QLabel(QString("%1:  %2").arg(key, val));
                        row->setStyleSheet(QString("color: %1; font-size: 10px; %2").arg(colors::TEXT_SECONDARY, MF()));
                        layout->addWidget(row);
                    };
                    add_prop("Mkt Cap", QString("$%1B").arg(p.market_cap / 1e9, 0, 'f', 1));
                    add_prop("Price", QString("$%1").arg(p.current_price, 0, 'f', 2));
                    add_prop("P/E", QString::number(p.pe_ratio, 'f', 1));
                    add_prop("ROE", QString("%1%").arg(p.roe * 100, 0, 'f', 1));
                    add_prop("Growth", QString("%1%").arg(p.revenue_growth * 100, 0, 'f', 1));
                    break;
                }
            }
        }

        detail_panel_->show();
        break;
    }
}

void RelationshipMapScreen::update_status_bar() {
    if (!has_data_) {
        status_nodes_->setText("READY");
        status_quality_->setText("");
        return;
    }
    int node_count = scene_->items().size(); // approximate
    status_nodes_->setText(QString("%1 ITEMS | %2 PEERS | %3 HOLDERS")
                               .arg(node_count)
                               .arg(current_data_.peers.size())
                               .arg(current_data_.institutional_holders.size()));
    status_quality_->setText(QString("QUALITY: %1%").arg(current_data_.data_quality));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap RelationshipMapScreen::save_state() const {
    return {
        {"ticker", search_input_ ? search_input_->text().trimmed() : QString{}},
        {"layout_mode", static_cast<int>(layout_mode_)},
    };
}

void RelationshipMapScreen::restore_state(const QVariantMap& state) {
    const QString ticker = state.value("ticker").toString();
    const int layout = state.value("layout_mode", 0).toInt();

    if (layout_combo_) {
        // Find the combo item whose data matches layout_mode
        for (int i = 0; i < layout_combo_->count(); ++i) {
            if (layout_combo_->itemData(i).toInt() == layout) {
                layout_combo_->setCurrentIndex(i);
                break;
            }
        }
    }

    if (!ticker.isEmpty() && search_input_) {
        search_input_->setText(ticker);
        on_search();
    }
}

} // namespace fincept::screens
