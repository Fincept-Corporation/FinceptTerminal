// DataSourcesScreen_Layout.cpp
//
// UI construction: setup_ui, header, stats strip, tab bar, browse + connections
// pages, category / connector / detail panels, command bar, and apply_screen_styles.
//
// Part of the partial-class split of DataSourcesScreen.cpp.

#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/data_sources/ConnectionConfigDialog.h"
#include "screens/data_sources/ConnectionTester.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "screens/data_sources/DataSourcesStyles.h"
#include "screens/data_sources/ImportExportConnections.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

void DataSourcesScreen::setup_ui() {
    setObjectName("dsScreen");

    // Apply stylesheet
    const QString style = screen_stylesheet()
                              .arg(col::BG_BASE())        // %1
                              .arg(col::BG_SURFACE())     // %2
                              .arg(col::BG_RAISED())      // %3
                              .arg(col::BORDER_DIM())     // %4
                              .arg(col::AMBER())          // %5
                              .arg(col::TEXT_SECONDARY()) // %6
                              .arg(col::BORDER_MED())     // %7
                              .arg(col::TEXT_TERTIARY())  // %8
                              .arg(col::BG_HOVER())       // %9
                              .arg(col::TEXT_PRIMARY())   // %10
                              .arg(col::BORDER_BRIGHT())  // %11
                              .arg(col::AMBER_DIM())      // %12
                              .arg(col::POSITIVE())       // %13
                              .arg(col::NEGATIVE())       // %14
                              .arg(col::NEGATIVE_DIM())   // %15
                              .arg(col::POSITIVE_DIM());  // %16
    setStyleSheet(style);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 1. Screen header
    root->addWidget(build_screen_header());

    // 2. Stats row
    root->addWidget(build_stats_strip());

    // 3. Tab bar
    root->addWidget(build_tab_bar());

    // 4. Page stack (Browse | Connections)
    page_stack_ = new QStackedWidget;
    page_stack_->addWidget(build_browse_page());      // index 0
    page_stack_->addWidget(build_connections_page()); // index 1
    root->addWidget(page_stack_, 1);

    // Timers
    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &DataSourcesScreen::update_clock);

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(30000);
    connect(poll_timer_, &QTimer::timeout, this, &DataSourcesScreen::on_poll_timer);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen header  (title + search + clock + io buttons)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_screen_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dsScreenHeader");
    bar->setFixedHeight(38);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(0);

    auto* title = new QLabel("DATA SOURCES");
    title->setObjectName("dsScreenTitle");
    hl->addWidget(title);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setStyleSheet(QString("background:%1;margin:8px 12px;").arg(col::BORDER_DIM()));
    hl->addWidget(sep);

    auto* subtitle = new QLabel(QString("%1 CONNECTORS").arg(ConnectorRegistry::instance().count()));
    subtitle->setObjectName("dsScreenSubtitle");
    hl->addWidget(subtitle);

    hl->addStretch();

    // Search
    search_edit_ = new QLineEdit;
    search_edit_->setObjectName("dsSearch");
    search_edit_->setPlaceholderText("search connectors...");
    search_edit_->setFixedSize(240, 26);
    connect(search_edit_, &QLineEdit::textChanged, this, &DataSourcesScreen::on_search_changed);
    hl->addWidget(search_edit_);

    hl->addSpacing(12);

    // IO buttons (right side)
    auto make_io_btn = [&](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label);
        btn->setObjectName("dsBtn");
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    auto* import_btn = make_io_btn("IMPORT");
    connect(import_btn, &QPushButton::clicked, this, &DataSourcesScreen::on_import_connections);
    hl->addWidget(import_btn);
    hl->addSpacing(4);

    auto* export_btn = make_io_btn("EXPORT");
    connect(export_btn, &QPushButton::clicked, this, &DataSourcesScreen::on_export_connections);
    hl->addWidget(export_btn);
    hl->addSpacing(4);

    auto* tpl_btn = make_io_btn("TEMPLATE");
    connect(tpl_btn, &QPushButton::clicked, this, &DataSourcesScreen::on_download_template);
    hl->addWidget(tpl_btn);

    hl->addSpacing(16);

    // Separator
    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1);
    sep2->setStyleSheet(QString("background:%1;margin:8px 0;").arg(col::BORDER_DIM()));
    hl->addWidget(sep2);
    hl->addSpacing(12);

    clock_label_ = new QLabel;
    clock_label_->setObjectName("dsClock");
    update_clock();
    hl->addWidget(clock_label_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Stats strip  (4 clickable stat boxes)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_stats_strip() {
    auto* strip = new QWidget(this);
    strip->setObjectName("dsStatRow");
    strip->setFixedHeight(58);

    auto* hl = new QHBoxLayout(strip);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    auto make_stat = [&](const QString& label, QLabel** out, int idx) -> QWidget* {
        auto* box = new QFrame;
        box->setObjectName("dsStatBox");
        box->setCursor(Qt::PointingHandCursor);
        box->installEventFilter(this);
        box->setProperty("statIndex", idx);

        auto* vl = new QVBoxLayout(box);
        vl->setContentsMargins(0, 6, 0, 6);
        vl->setSpacing(2);

        auto* val = new QLabel("--");
        val->setObjectName("dsStatValue");
        val->setAlignment(Qt::AlignCenter);
        val->setAttribute(Qt::WA_TransparentForMouseEvents);
        vl->addWidget(val);

        auto* lbl = new QLabel(label);
        lbl->setObjectName("dsStatLabel");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        vl->addWidget(lbl);

        *out = val;
        return box;
    };

    hl->addWidget(make_stat("UNIVERSE", &universe_stat_value_, 0));
    hl->addWidget(make_stat("CONFIGURED", &configured_stat_value_, 1));
    hl->addWidget(make_stat("ACTIVE", &active_stat_value_, 2));
    hl->addWidget(make_stat("AUTH REQ", &auth_stat_value_, 3));

    return strip;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab bar  (BROWSE | CONNECTIONS)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_tab_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("dsTabBar");
    bar->setFixedHeight(32);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(0);

    auto make_tab = [&](const QString& label, int page_idx) -> QPushButton* {
        auto* btn = new QPushButton(label);
        btn->setFixedHeight(32);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName(page_idx == 0 ? "dsTabActive" : "dsTab");
        connect(btn, &QPushButton::clicked, this, [this, btn, page_idx]() {
            // Update tab styles
            for (auto* sibling : btn->parentWidget()->findChildren<QPushButton*>()) {
                sibling->setObjectName("dsTab");
                sibling->style()->unpolish(sibling);
                sibling->style()->polish(sibling);
            }
            btn->setObjectName("dsTabActive");
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);

            page_stack_->setCurrentIndex(page_idx);
            view_mode_ = (page_idx == 0) ? ViewMode::Gallery : ViewMode::Connections;
        });
        return btn;
    };

    auto* browse_tab = make_tab("BROWSE", 0);
    auto* conns_tab = make_tab("CONNECTIONS", 1);

    hl->addWidget(browse_tab);
    hl->addWidget(conns_tab);
    hl->addStretch();

    // Count label
    count_label_ = new QLabel;
    count_label_->setObjectName("dsScreenSubtitle");
    hl->addWidget(count_label_);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Browse page  (sidebar | connector table | detail inspector)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_browse_page() {
    auto* page = new QWidget(this);
    auto* hl = new QHBoxLayout(page);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // Left: category sidebar
    hl->addWidget(build_category_panel());

    // Center + Right: splitter
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName("dsBrowseSplitter");
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);

    splitter->addWidget(build_connector_panel());
    splitter->addWidget(build_detail_panel());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({600, 280});

    hl->addWidget(splitter, 1);
    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Category sidebar
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_category_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dsSidebar");
    panel->setMinimumWidth(150);
    panel->setMaximumWidth(220);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("dsSidebarHeader");
    hdr->setFixedHeight(30);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(14, 0, 14, 0);
    auto* hdr_title = new QLabel("CATEGORY");
    hdr_title->setObjectName("dsSidebarTitle");
    hdr_hl->addWidget(hdr_title);
    vl->addWidget(hdr);

    // Category list
    category_list_ = new QListWidget;
    category_list_->setObjectName("dsCategoryList");
    category_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(category_list_, &QListWidget::currentRowChanged, this, &DataSourcesScreen::on_category_selection_changed);
    vl->addWidget(category_list_, 1);

    // Provider ladder section
    auto* prov_hdr = new QWidget(this);
    prov_hdr->setObjectName("dsProviderHeader");
    prov_hdr->setFixedHeight(28);
    auto* prov_hl = new QHBoxLayout(prov_hdr);
    prov_hl->setContentsMargins(14, 0, 14, 0);
    auto* prov_title = new QLabel("TOP PROVIDERS");
    prov_title->setObjectName("dsSidebarTitle");
    prov_hl->addWidget(prov_title);
    vl->addWidget(prov_hdr);

    provider_ladder_ = new QListWidget;
    provider_ladder_->setObjectName("dsProviderLadder");
    provider_ladder_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    provider_ladder_->setFixedHeight(120);
    connect(provider_ladder_, &QListWidget::itemClicked, this, &DataSourcesScreen::on_provider_ladder_activated);
    vl->addWidget(provider_ladder_);

    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connector table panel (center)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_connector_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dsConnPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("dsConnHeader");
    hdr->setFixedHeight(30);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(12, 0, 12, 0);
    hdr_hl->setSpacing(8);

    auto* panel_title = new QLabel("CONNECTORS");
    panel_title->setObjectName("dsConnPanelTitle");
    hdr_hl->addWidget(panel_title);

    hdr_hl->addStretch();

    count_label_ = new QLabel; // reuse for connector count display
    count_label_->setObjectName("dsConnCount");
    hdr_hl->addWidget(count_label_);

    vl->addWidget(hdr);

    // Table
    connector_table_ = new QTableWidget;
    connector_table_->setObjectName("dsConnectorTable");
    connector_table_->setColumnCount(7);
    connector_table_->setHorizontalHeaderLabels({"", "CONNECTOR", "CATEGORY", "AUTH", "TYPE", "ACTIVE", "TOTAL"});
    connector_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    connector_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connector_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connector_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    connector_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    connector_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    connector_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    connector_table_->setColumnWidth(0, 38);
    connector_table_->setColumnWidth(3, 52);
    connector_table_->setColumnWidth(4, 56);
    connector_table_->setColumnWidth(5, 52);
    connector_table_->setColumnWidth(6, 52);
    connector_table_->verticalHeader()->setVisible(false);
    connector_table_->verticalHeader()->setDefaultSectionSize(26);
    connector_table_->setShowGrid(false);
    connector_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connector_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    connector_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connector_table_->horizontalHeader()->setHighlightSections(false);
    connector_table_->setAlternatingRowColors(true);
    connector_table_->setStyleSheet(connector_table_->styleSheet() +
                                    QString("QTableWidget { alternate-background-color: %1; }").arg(col::ROW_ALT()));
    connect(connector_table_, &QTableWidget::itemSelectionChanged, this,
            &DataSourcesScreen::on_connector_selection_changed);
    connect(connector_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto* item = connector_table_->item(row, 1);
        if (item)
            on_connector_clicked(item->data(Qt::UserRole).toString());
    });

    vl->addWidget(connector_table_, 1);
    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// Detail inspector panel (right)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_detail_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("dsDetail");
    panel->setMinimumWidth(240);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("dsDetailHeader");
    hdr->setFixedHeight(30);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(12, 0, 12, 0);
    auto* hdr_title = new QLabel("INSPECTOR");
    hdr_title->setObjectName("dsDetailTitle");
    hdr_hl->addWidget(hdr_title);
    vl->addWidget(hdr);

    // Scrollable content
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background:%1; border:none; }").arg(col::BG_SURFACE()));

    auto* body = new QWidget(this);
    body->setStyleSheet(QString("background:%1;").arg(col::BG_SURFACE()));
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(0, 0, 0, 0);
    body_vl->setSpacing(0);

    // Identity block (symbol + name + description)
    auto* id_block = new QWidget(this);
    id_block->setObjectName("dsInfoRow");
    id_block->setFixedHeight(64);
    auto* id_hl = new QHBoxLayout(id_block);
    id_hl->setContentsMargins(12, 10, 12, 10);
    id_hl->setSpacing(10);

    detail_symbol_ = new QLabel("--");
    detail_symbol_->setObjectName("dsDetailSymbol");
    detail_symbol_->setAlignment(Qt::AlignCenter);
    id_hl->addWidget(detail_symbol_);

    auto* id_text = new QVBoxLayout;
    id_text->setSpacing(2);
    detail_title_ = new QLabel("Select a connector");
    detail_title_->setObjectName("dsDetailName");
    id_text->addWidget(detail_title_);
    detail_description_ = new QLabel("Double-click to configure");
    detail_description_->setObjectName("dsDetailDesc");
    detail_description_->setWordWrap(true);
    id_text->addWidget(detail_description_);
    id_hl->addLayout(id_text, 1);

    body_vl->addWidget(id_block);

    // ── Metadata rows ────────────────────────────────────────────────────────
    auto make_info_row = [&](const QString& label, QLabel** value_out) -> QWidget* {
        auto* row = new QWidget(this);
        row->setObjectName("dsInfoRow");
        row->setFixedHeight(28);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(12, 0, 12, 0);
        hl->setSpacing(0);
        auto* lbl = new QLabel(label);
        lbl->setObjectName("dsInfoLabel");
        hl->addWidget(lbl);
        hl->addStretch();
        auto* val = new QLabel("--");
        val->setObjectName("dsInfoValue");
        hl->addWidget(val);
        *value_out = val;
        return row;
    };

    body_vl->addWidget(make_info_row("CATEGORY", &detail_category_value_));
    body_vl->addWidget(make_info_row("TYPE", &detail_transport_value_));
    body_vl->addWidget(make_info_row("AUTH", &detail_auth_value_));
    body_vl->addWidget(make_info_row("TESTABLE", &detail_test_value_));
    body_vl->addWidget(make_info_row("FIELDS", &detail_fields_value_));
    body_vl->addWidget(make_info_row("CONFIGURED", &detail_configured_value_));
    body_vl->addWidget(make_info_row("ACTIVE", &detail_enabled_value_));
    body_vl->addWidget(make_info_row("LAST STATUS", &detail_last_status_value_));

    // ── Fields section label ─────────────────────────────────────────────────
    auto* fields_hdr = new QWidget(this);
    fields_hdr->setObjectName("dsConnHeader");
    fields_hdr->setFixedHeight(26);
    auto* fh_hl = new QHBoxLayout(fields_hdr);
    fh_hl->setContentsMargins(12, 0, 12, 0);
    auto* fh_lbl = new QLabel("CONFIG FIELDS");
    fh_lbl->setObjectName("dsSectionSep");
    fh_hl->addWidget(fh_lbl);
    body_vl->addWidget(fields_hdr);

    field_table_ = new QTableWidget;
    field_table_->setObjectName("dsFieldTable");
    field_table_->setColumnCount(3);
    field_table_->setHorizontalHeaderLabels({"FIELD", "TYPE", "REQ"});
    field_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    field_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    field_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    field_table_->setColumnWidth(1, 58);
    field_table_->setColumnWidth(2, 38);
    field_table_->verticalHeader()->setVisible(false);
    field_table_->verticalHeader()->setDefaultSectionSize(24);
    field_table_->setShowGrid(false);
    field_table_->setSelectionMode(QAbstractItemView::NoSelection);
    field_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    field_table_->horizontalHeader()->setHighlightSections(false);
    field_table_->setMaximumHeight(120);
    body_vl->addWidget(field_table_);

    // ── Saved connections section ────────────────────────────────────────────
    auto* conns_hdr = new QWidget(this);
    conns_hdr->setObjectName("dsConnHeader");
    conns_hdr->setFixedHeight(26);
    auto* ch_hl = new QHBoxLayout(conns_hdr);
    ch_hl->setContentsMargins(12, 0, 12, 0);
    auto* ch_lbl = new QLabel("SAVED CONNECTIONS");
    ch_lbl->setObjectName("dsSectionSep");
    ch_hl->addWidget(ch_lbl);
    body_vl->addWidget(conns_hdr);

    detail_connections_list_ = new QListWidget;
    detail_connections_list_->setObjectName("dsDetailConnList");
    detail_connections_list_->setMaximumHeight(110);
    detail_connections_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(detail_connections_list_, &QListWidget::itemClicked, this,
            &DataSourcesScreen::on_detail_connection_activated);
    body_vl->addWidget(detail_connections_list_);

    // ── Action buttons ───────────────────────────────────────────────────────
    auto* actions = new QWidget(this);
    actions->setObjectName("dsConnHeader");
    auto* act_vl = new QVBoxLayout(actions);
    act_vl->setContentsMargins(12, 10, 12, 10);
    act_vl->setSpacing(6);

    new_connection_btn_ = new QPushButton("+ ADD CONNECTION");
    new_connection_btn_->setObjectName("dsBtnAccent");
    new_connection_btn_->setFixedHeight(28);
    new_connection_btn_->setCursor(Qt::PointingHandCursor);
    new_connection_btn_->setEnabled(false);
    connect(new_connection_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_connection_add);
    act_vl->addWidget(new_connection_btn_);

    auto* edit_test_hl = new QHBoxLayout;
    edit_test_hl->setSpacing(6);

    edit_connection_btn_ = new QPushButton("EDIT");
    edit_connection_btn_->setObjectName("dsBtn");
    edit_connection_btn_->setFixedHeight(26);
    edit_connection_btn_->setCursor(Qt::PointingHandCursor);
    edit_connection_btn_->setEnabled(false);
    connect(edit_connection_btn_, &QPushButton::clicked, this,
            [this]() { on_connection_edit(effective_detail_connection_id()); });
    edit_test_hl->addWidget(edit_connection_btn_);

    test_connection_btn_ = new QPushButton("TEST");
    test_connection_btn_->setObjectName("dsBtnGreen");
    test_connection_btn_->setFixedHeight(26);
    test_connection_btn_->setCursor(Qt::PointingHandCursor);
    test_connection_btn_->setEnabled(false);
    connect(test_connection_btn_, &QPushButton::clicked, this,
            [this]() { on_connection_test(effective_detail_connection_id()); });
    edit_test_hl->addWidget(test_connection_btn_);

    act_vl->addLayout(edit_test_hl);
    body_vl->addWidget(actions);

    body_vl->addStretch();
    scroll->setWidget(body);
    vl->addWidget(scroll, 1);

    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connections page  (full-screen connections table)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_connections_page() {
    auto* page = new QWidget(this);
    page->setObjectName("dsConnsPanel");

    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("dsConnsHeader");
    hdr->setFixedHeight(30);
    auto* hdr_hl = new QHBoxLayout(hdr);
    hdr_hl->setContentsMargins(12, 0, 12, 0);
    hdr_hl->setSpacing(8);

    auto* hdr_title = new QLabel("SAVED CONNECTIONS");
    hdr_title->setObjectName("dsConnPanelTitle");
    hdr_hl->addWidget(hdr_title);
    hdr_hl->addStretch();

    auto* add_conn_btn = new QPushButton("+ ADD");
    add_conn_btn->setObjectName("dsBtnAccent");
    add_conn_btn->setFixedHeight(24);
    add_conn_btn->setCursor(Qt::PointingHandCursor);
    connect(add_conn_btn, &QPushButton::clicked, this, &DataSourcesScreen::on_connection_add);
    hdr_hl->addWidget(add_conn_btn);

    vl->addWidget(hdr);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("dsConnsToolbar");
    toolbar->setFixedHeight(32);
    auto* tb_hl = new QHBoxLayout(toolbar);
    tb_hl->setContentsMargins(10, 0, 10, 0);
    tb_hl->setSpacing(6);

    // Search
    conn_search_edit_ = new QLineEdit;
    conn_search_edit_->setObjectName("dsSearchConn");
    conn_search_edit_->setPlaceholderText("filter connections...");
    conn_search_edit_->setFixedSize(220, 22);
    connect(conn_search_edit_, &QLineEdit::textChanged, this, &DataSourcesScreen::on_connections_search_changed);
    tb_hl->addWidget(conn_search_edit_);

    tb_hl->addStretch();

    // Bulk buttons
    bulk_enable_btn_ = new QPushButton("ENABLE ALL");
    bulk_enable_btn_->setObjectName("dsBtnGreen");
    bulk_enable_btn_->setFixedHeight(22);
    bulk_enable_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_enable_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_enable_all);
    tb_hl->addWidget(bulk_enable_btn_);

    bulk_disable_btn_ = new QPushButton("DISABLE ALL");
    bulk_disable_btn_->setObjectName("dsBtn");
    bulk_disable_btn_->setFixedHeight(22);
    bulk_disable_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_disable_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_disable_all);
    tb_hl->addWidget(bulk_disable_btn_);

    bulk_delete_btn_ = new QPushButton("DELETE SEL");
    bulk_delete_btn_->setObjectName("dsBtnDanger");
    bulk_delete_btn_->setFixedHeight(22);
    bulk_delete_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_delete_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_delete_selected);
    tb_hl->addWidget(bulk_delete_btn_);

    vl->addWidget(toolbar);

    // Connections table
    connections_table_ = new QTableWidget;
    connections_table_->setObjectName("dsConnectionsTable");
    connections_table_->setColumnCount(8);
    connections_table_->setHorizontalHeaderLabels(
        {"", "NAME", "PROVIDER", "CATEGORY", "TYPE", "STATUS", "TAGS", "UPDATED"});
    connections_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    connections_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connections_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    connections_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    connections_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    connections_table_->setColumnWidth(0, 32);
    connections_table_->setColumnWidth(4, 54);
    connections_table_->setColumnWidth(5, 54);
    connections_table_->setColumnWidth(7, 140);
    connections_table_->verticalHeader()->setVisible(false);
    connections_table_->verticalHeader()->setDefaultSectionSize(26);
    connections_table_->setShowGrid(false);
    connections_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connections_table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connections_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connections_table_->horizontalHeader()->setHighlightSections(false);
    connections_table_->setAlternatingRowColors(true);
    connections_table_->setStyleSheet(connections_table_->styleSheet() +
                                      QString("QTableWidget { alternate-background-color: %1; }").arg(col::ROW_ALT()));
    connect(connections_table_, &QTableWidget::itemSelectionChanged, this,
            &DataSourcesScreen::on_connection_selection_changed);

    vl->addWidget(connections_table_, 1);
    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Command bar stub (unused — kept for ABI compat)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_command_bar() {
    auto* w = new QWidget(this);
    w->setFixedHeight(0);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// apply_screen_styles — no-op (styles applied inline in setup_ui)
// ─────────────────────────────────────────────────────────────────────────────

void DataSourcesScreen::apply_screen_styles() {}

// ─────────────────────────────────────────────────────────────────────────────
// Config dialog (thin wrapper — implementation in ConnectionConfigDialog.cpp)
// ─────────────────────────────────────────────────────────────────────────────
} // namespace fincept::screens::datasources
