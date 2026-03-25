// DataSourcesScreen.cpp — Fincept Data Sources, Obsidian design system.
#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QCheckBox>
#include <QFileDialog>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QMap>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTcpSocket>
#include <QTextEdit>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens::datasources {

namespace col = fincept::ui::colors;
namespace fnt = fincept::ui::fonts;

namespace {

static const QString TAG = "DataSources";
static const QStringList kCategoryLabels = {
    "All Connectors", "Databases", "APIs", "Files", "Streaming",
    "Cloud", "Time Series", "Market Data", "Search", "Warehouses"
};

const ConnectorConfig* find_connector_config(const QString& key) {
    if (const auto* cfg = ConnectorRegistry::instance().get(key))
        return cfg;
    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (cfg.type == key)
            return &cfg;
    }
    return nullptr;
}

QString normalized_provider_key(const DataSource& ds) {
    if (const auto* cfg = find_connector_config(ds.provider))
        return cfg->id;
    return ds.provider;
}

bool connection_matches_connector(const DataSource& ds, const QString& connector_id) {
    return !connector_id.isEmpty() && normalized_provider_key(ds) == connector_id;
}

bool connector_matches_text(const ConnectorConfig& cfg, const QString& filter) {
    if (filter.isEmpty()) return true;
    const QString haystack =
        QString("%1 %2 %3 %4 %5").arg(cfg.name, cfg.id, cfg.type, cfg.description, category_label(cfg.category));
    return haystack.toLower().contains(filter);
}

QString connector_code(const ConnectorConfig& cfg) {
    const QString icon = cfg.icon.trimmed().toUpper();
    if (!icon.isEmpty()) return icon.left(2);
    return cfg.name.left(2).toUpper();
}

QString connector_transport(const ConnectorConfig& cfg) {
    switch (cfg.category) {
        case Category::Database:        return "SQL";
        case Category::Api:             return "API";
        case Category::File:            return "FILE";
        case Category::Streaming:       return "STREAM";
        case Category::Cloud:           return "CLOUD";
        case Category::TimeSeries:      return "TSDB";
        case Category::MarketData:      return "MARKET";
        case Category::SearchAnalytics: return "SEARCH";
        case Category::DataWarehouse:   return "WAREHOUSE";
    }
    return "API";
}

QString persistence_type(const ConnectorConfig& cfg) {
    return cfg.category == Category::Streaming ? "websocket" : "rest_api";
}

QString field_type_label(FieldType type) {
    switch (type) {
        case FieldType::Text:     return "Text";
        case FieldType::Password: return "Secret";
        case FieldType::Number:   return "Number";
        case FieldType::Url:      return "URL";
        case FieldType::Select:   return "Select";
        case FieldType::Textarea: return "Textarea";
        case FieldType::Checkbox: return "Bool";
        case FieldType::File:     return "File";
    }
    return "Text";
}

QString compact_value(const QString& value, int max_len = 22) {
    const QString trimmed = value.trimmed();
    if (trimmed.size() <= max_len) return trimmed;
    return trimmed.left(max_len - 3) + "...";
}

int total_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (connection_matches_connector(ds, connector_id)) ++count;
    return count;
}

int enabled_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (ds.enabled && connection_matches_connector(ds, connector_id)) ++count;
    return count;
}

QTableWidgetItem* make_item(const QString& text,
                            const QColor& color = TEXT_PRIMARY,
                            Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
    auto* item = new QTableWidgetItem(text);
    item->setForeground(color);
    item->setTextAlignment(alignment);
    return item;
}

QWidget* create_stat_box(const QString& label, QLabel** value_label) {
    auto* box = new QFrame;
    box->setObjectName("dsStatBox");
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);

    auto* value = new QLabel("--");
    value->setObjectName("dsStatValue");
    layout->addWidget(value);

    auto* title = new QLabel(label);
    title->setObjectName("dsStatLabel");
    layout->addWidget(title);

    *value_label = value;
    return box;
}

QWidget* create_metric_row(const QString& label, QLabel** value_label) {
    auto* row = new QWidget;
    row->setObjectName("dsInfoRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(label);
    title->setObjectName("dsMetricLabel");
    layout->addWidget(title);
    layout->addStretch();

    auto* value = new QLabel("--");
    value->setObjectName("dsMetricValue");
    layout->addWidget(value);

    *value_label = value;
    return row;
}

} // namespace

// ── Constructor ──────────────────────────────────────────────────────────────

DataSourcesScreen::DataSourcesScreen(QWidget* parent) : QWidget(parent) {
    register_all_connectors();
    setup_ui();
    refresh_connections();
    LOG_INFO(TAG, QString("Loaded %1 connector definitions").arg(ConnectorRegistry::instance().count()));
}

void DataSourcesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_timer_) { update_clock(); clock_timer_->start(); }
    if (poll_timer_)  poll_timer_->start();
    refresh_connections();
}

void DataSourcesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_timer_) clock_timer_->stop();
    if (poll_timer_)  poll_timer_->stop();
}

void DataSourcesScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (search_edit_) {
        const int target_width = std::clamp(width() / 4, 180, 340);
        search_edit_->setFixedWidth(target_width);
    }
    update_stats_strip();
}

bool DataSourcesScreen::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        const QVariant idx_var = obj->property("statIndex");
        if (idx_var.isValid()) {
            on_stat_box_clicked(idx_var.toInt());
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Styles ───────────────────────────────────────────────────────────────────

void DataSourcesScreen::apply_screen_styles() {
    setObjectName("dsScreen");
    setStyleSheet(QString(R"(
        #dsScreen { background: %1; }
        #dsHeaderBar { background: %3; border-bottom: 1px solid %4; }
        #dsHeaderTitle { color: %5; font-size: 15px; font-weight: 700; background: transparent; }
        #dsHeaderSub { color: %6; font-size: 12px; background: transparent; }
        #dsHeaderMeta { color: %8; font-size: 12px; background: transparent; }
        #dsSearch { background: %1; border: 1px solid %4; color: %10; padding: 4px 12px;
                    font-size: 13px; border-radius: 4px; }
        #dsSearch:focus { border-color: %5; }
        #dsClearBtn { background: transparent; color: %6; border: 1px solid %4;
                      padding: 4px 10px; font-size: 12px; font-weight: 600; border-radius: 3px; }
        #dsClearBtn:hover { color: %10; background: %9; }
        #dsStatStrip { background: %1; border-bottom: 1px solid %4; }
        #dsStatBox { background: %3; border: 1px solid %4; border-radius: 6px; }
        #dsStatValue { color: %10; font-size: 22px; font-weight: 700; background: transparent; }
        #dsStatLabel { color: %6; font-size: 11px; background: transparent; }
        #dsSidebarPanel { background: %2; border-right: 1px solid %4; }
        #dsSidebarTitle { color: %5; font-size: 13px; font-weight: 700; background: transparent; }
        #dsSidebarSectionLabel { color: %6; font-size: 11px; font-weight: 600;
                                  letter-spacing: 0.5px; background: transparent; }
        #dsCategoryList { background: transparent; border: none; outline: none; }
        #dsCategoryList::item { padding: 9px 16px; color: %6; font-size: 13px;
                                 border-radius: 4px; margin: 1px 6px; }
        #dsCategoryList::item:selected { background: rgba(217,119,6,0.12);
                                          color: %5; border-left: 3px solid %5; }
        #dsCategoryList::item:hover:!selected { background: %9; color: %10; }
        #dsProviderLadder { background: transparent; border: none; outline: none; }
        #dsProviderLadder::item { padding: 7px 16px; color: %6; font-size: 13px;
                                   border-radius: 3px; margin: 1px 6px; }
        #dsProviderLadder::item:selected { background: %9; color: %10; }
        #dsProviderLadder::item:hover:!selected { background: %9; }
        #dsPanel { background: %1; border: none; }
        #dsPanelHeader { background: %3; border-bottom: 1px solid %4; }
        #dsPanelTitle { color: %10; font-size: 13px; font-weight: 700; background: transparent; }
        #dsPanelMeta { color: %6; font-size: 12px; background: transparent; }
        #dsConnectorTable { background: %1; border: none; color: %10; font-size: 13px; gridline-color: %4; }
        #dsConnectorTable::item { padding: 0 8px; border-bottom: 1px solid %4; }
        #dsConnectorTable::item:selected { background: rgba(217,119,6,0.10); color: %10; }
        #dsConnectorTable QHeaderView::section { background: %3; color: %6; font-size: 11px;
            font-weight: 700; border: none; border-bottom: 1px solid %4; padding: 0 8px; }
        #dsConnectionsTable { background: %1; border: none; color: %10; font-size: 13px; gridline-color: %4; }
        #dsConnectionsTable::item { padding: 0 8px; border-bottom: 1px solid %4; }
        #dsConnectionsTable::item:selected { background: rgba(217,119,6,0.10); color: %10; }
        #dsConnectionsTable QHeaderView::section { background: %3; color: %6; font-size: 11px;
            font-weight: 700; border: none; border-bottom: 1px solid %4; padding: 0 8px; }
        #dsFieldTable { background: %2; border: none; color: %10; font-size: 12px; gridline-color: %4; }
        #dsFieldTable::item { padding: 0 6px; border-bottom: 1px solid %4; }
        #dsFieldTable QHeaderView::section { background: %3; color: %6; font-size: 11px;
            font-weight: 600; border: none; border-bottom: 1px solid %4; padding: 0 6px; }
        #dsDetailConnList { background: transparent; border: none; outline: none; }
        #dsDetailConnList::item { padding: 8px 10px; color: %6; font-size: 13px;
                                   border-radius: 3px; margin: 1px 4px; }
        #dsDetailConnList::item:selected { background: %9; color: %10; }
        #dsDetailConnList::item:hover:!selected { background: %9; }
        #dsInspectorSymbol { min-width: 44px; max-width: 44px; min-height: 44px; max-height: 44px;
            font-size: 16px; font-weight: 700; color: %10; background: %3;
            border: 1px solid %4; border-radius: 6px; }
        #dsInspectorTitle { color: %10; font-size: 14px; font-weight: 700; background: transparent; }
        #dsInspectorKey { color: %6; font-size: 11px; font-weight: 600; background: transparent; }
        #dsInspectorDesc { color: %6; font-size: 13px; background: transparent; line-height: 1.4; }
        #dsInfoRow { background: transparent; border-bottom: 1px solid %4; }
        #dsMetricLabel { color: %6; font-size: 12px; background: transparent; }
        #dsMetricValue { color: %10; font-size: 12px; font-weight: 600; background: transparent; }
        #dsSectionLabel { color: %6; font-size: 11px; font-weight: 700;
                           letter-spacing: 0.5px; background: transparent; }
        #dsActionBtn { background: %9; color: %6; border: 1px solid %4; padding: 6px 14px;
                        font-size: 12px; font-weight: 600; border-radius: 4px; }
        #dsActionBtn:hover:enabled { background: %11; color: %10; }
        #dsActionBtn[accent="true"] { background: rgba(217,119,6,0.10); color: %5;
                                       border: 1px solid %12; }
        #dsActionBtn[accent="true"]:hover:enabled { background: %5; color: %1; }
        #dsActionBtn:disabled { background: %3; color: %8; }
        #dsInlineAction { background: transparent; color: %6; border: 1px solid %4;
                           padding: 3px 8px; font-size: 11px; font-weight: 600; border-radius: 3px; }
        #dsInlineAction:hover { color: %10; background: %9; }
        #dsInlineAction[accent="true"] { color: %5; border-color: %12; background: rgba(217,119,6,0.08); }
        #dsInlineAction[test="true"] { color: %13; border-color: #14532d; background: rgba(22,163,74,0.08); }
        #dsInlineAction[danger="true"] { color: %14; border-color: #7f1d1d; background: rgba(220,38,38,0.08); }
        #dsConnSearch { background: %1; border: 1px solid %4; color: %10; padding: 3px 10px;
                        font-size: 12px; border-radius: 3px; }
        #dsConnSearch:focus { border-color: %5; }
        #dsConnToolbar { background: %3; border-bottom: 1px solid %4; }
        #dsViewModeBtn { background: transparent; color: %6; border: 1px solid %4;
                         padding: 4px 10px; font-size: 11px; font-weight: 600; border-radius: 3px; }
        #dsViewModeBtn:hover { color: %10; background: %9; }
        #dsViewModeBtn[active="true"] { color: %5; border-color: %12; background: rgba(217,119,6,0.10); }
        #dsImportExportBtn { background: transparent; color: %6; border: 1px solid %4;
                             padding: 4px 10px; font-size: 11px; font-weight: 600; border-radius: 3px; }
        #dsImportExportBtn:hover { color: %10; background: %9; }
        #dsBulkBtn { background: transparent; color: %6; border: 1px solid %4;
                     padding: 3px 10px; font-size: 11px; font-weight: 600; border-radius: 3px; }
        #dsBulkBtn:hover:enabled { color: %10; background: %9; }
        #dsBulkBtn:disabled { color: %8; }
        #dsBulkBtn[danger="true"]:hover:enabled { color: %14; background: rgba(220,38,38,0.08); }
        #dsSelectAll { color: %6; font-size: 12px; }
        #dsStatusDot { font-size: 11px; font-weight: 700; background: transparent; }
        #dsEnableToggle { color: %10; background: transparent; }
        #dsEnableToggle::indicator { width: 14px; height: 14px; border: 1px solid %7; background: %3; border-radius: 2px; }
        #dsEnableToggle::indicator:checked { background: %13; border-color: %13; }
        #dsWorkspaceSplitter::handle, #dsMainSplitter::handle { background: %4; width: 1px; height: 1px; }
        QScrollBar:vertical { background: transparent; width: 4px; margin: 2px; }
        QScrollBar::handle:vertical { background: %7; border-radius: 2px; min-height: 24px; }
        QScrollBar::handle:vertical:hover { background: %11; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )")
        .arg(col::BG_BASE)           // %1
        .arg(col::BG_SURFACE)        // %2
        .arg(col::BG_RAISED)         // %3
        .arg(col::BORDER_DIM)        // %4
        .arg(col::AMBER)             // %5
        .arg(col::TEXT_SECONDARY)    // %6
        .arg(col::BORDER_MED)        // %7
        .arg(col::TEXT_TERTIARY)     // %8
        .arg(col::BG_HOVER)          // %9
        .arg(col::TEXT_PRIMARY)      // %10
        .arg(col::BORDER_BRIGHT)     // %11
        .arg(col::AMBER_DIM)         // %12
        .arg(col::POSITIVE)          // %13
        .arg(col::NEGATIVE));        // %14
}

// ── Header bar ───────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_header_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("dsHeaderBar");
    bar->setFixedHeight(56);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(20, 0, 16, 0);
    hl->setSpacing(10);

    // Icon + title block
    auto* icon = new QLabel("DS");
    icon->setStyleSheet(QString("color:%1;font-size:20px;background:transparent;").arg(col::AMBER));
    hl->addWidget(icon);

    auto* title_col = new QVBoxLayout;
    title_col->setContentsMargins(0, 0, 0, 0);
    title_col->setSpacing(1);

    auto* title = new QLabel("Data Sources");
    title->setObjectName("dsHeaderTitle");
    title_col->addWidget(title);

    auto* sub = new QLabel("Connect and manage data connectors");
    sub->setObjectName("dsHeaderSub");
    title_col->addWidget(sub);

    hl->addLayout(title_col);
    hl->addSpacing(24);

    // Search (centered)
    search_edit_ = new QLineEdit;
    search_edit_->setObjectName("dsSearch");
    search_edit_->setPlaceholderText("Search connectors, categories, providers…");
    search_edit_->setFixedHeight(34);
    search_edit_->setFixedWidth(300);
    connect(search_edit_, &QLineEdit::textChanged, this, &DataSourcesScreen::on_search_changed);
    hl->addWidget(search_edit_);

    clear_search_btn_ = new QPushButton("X");
    clear_search_btn_->setObjectName("dsClearBtn");
    clear_search_btn_->setFixedSize(28, 28);
    clear_search_btn_->setCursor(Qt::PointingHandCursor);
    clear_search_btn_->setToolTip("Clear search");
    connect(clear_search_btn_, &QPushButton::clicked, search_edit_, &QLineEdit::clear);
    hl->addWidget(clear_search_btn_);

    hl->addStretch();

    // View mode toggle
    view_mode_btn_ = new QPushButton("Gallery");
    view_mode_btn_->setObjectName("dsViewModeBtn");
    view_mode_btn_->setProperty("active", true);
    view_mode_btn_->setCursor(Qt::PointingHandCursor);
    view_mode_btn_->setToolTip("Switch between Gallery and Connections view");
    connect(view_mode_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_view_mode_toggle);
    hl->addWidget(view_mode_btn_);

    // Import / Export
    import_btn_ = new QPushButton("Import");
    import_btn_->setObjectName("dsImportExportBtn");
    import_btn_->setCursor(Qt::PointingHandCursor);
    import_btn_->setToolTip("Import connections from JSON file");
    connect(import_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_import_connections);
    hl->addWidget(import_btn_);

    export_btn_ = new QPushButton("Export");
    export_btn_->setObjectName("dsImportExportBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    export_btn_->setToolTip("Export all connections to JSON file");
    connect(export_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_export_connections);
    hl->addWidget(export_btn_);

    hl->addSpacing(8);

    // Count + clock
    count_label_ = new QLabel;
    count_label_->setObjectName("dsHeaderMeta");
    count_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(count_label_);

    hl->addSpacing(16);

    clock_label_ = new QLabel("--:--:--");
    clock_label_->setObjectName("dsHeaderMeta");
    hl->addWidget(clock_label_);

    return bar;
}

// ── Command bar (kept for layout compatibility, minimal) ─────────────────────

QWidget* DataSourcesScreen::build_command_bar() {
    // Search moved to header — return invisible placeholder
    auto* placeholder = new QWidget;
    placeholder->setFixedHeight(0);
    placeholder->setVisible(false);
    selection_status_label_ = new QLabel(placeholder); // keep pointer valid
    return placeholder;
}

// ── Stats strip ──────────────────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_stats_strip() {
    auto* strip = new QWidget;
    strip->setObjectName("dsStatStrip");
    strip->setFixedHeight(76);

    auto* hl = new QHBoxLayout(strip);
    hl->setContentsMargins(16, 8, 16, 8);
    hl->setSpacing(10);

    // Create clickable stat boxes — index maps to on_stat_box_clicked(i)
    const QStringList labels = {"Total Connectors", "Configured", "Active", "Auth Required"};
    QLabel** value_ptrs[] = {&universe_stat_value_, &configured_stat_value_,
                              &active_stat_value_,   &auth_stat_value_};
    for (int i = 0; i < 4; ++i) {
        auto* box = create_stat_box(labels[i], value_ptrs[i]);
        box->setCursor(Qt::PointingHandCursor);
        box->setToolTip(i == 0 ? "Click to clear filter"
                      : i == 1 ? "Click to show only configured connectors"
                      : i == 2 ? "Click to show only active connections"
                               : "Click to show only auth-required connectors");
        // Install event filter via lambda — use a child QLabel to detect click
        const int idx = i;
        // Wrap in a QPushButton-like response via MousePressEvent on the frame
        box->installEventFilter(this);
        box->setProperty("statIndex", idx);
        hl->addWidget(box);
    }

    return strip;
}

// ── Category sidebar panel ───────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_category_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dsSidebarPanel");
    panel->setFixedWidth(260);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Categories header
    auto* cat_hdr = new QWidget;
    cat_hdr->setFixedHeight(44);
    cat_hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(col::BG_RAISED, col::BORDER_DIM));
    auto* cat_hdr_l = new QHBoxLayout(cat_hdr);
    cat_hdr_l->setContentsMargins(16, 0, 14, 0);

    auto* cat_title = new QLabel("Categories");
    cat_title->setObjectName("dsSidebarTitle");
    cat_hdr_l->addWidget(cat_title, 1);
    vl->addWidget(cat_hdr);

    // Category list
    category_list_ = new QListWidget;
    category_list_->setObjectName("dsCategoryList");
    category_list_->setFrameShape(QFrame::NoFrame);
    category_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    category_list_->setSpacing(1);
    connect(category_list_, &QListWidget::currentRowChanged,
            this, &DataSourcesScreen::on_category_selection_changed);
    vl->addWidget(category_list_);

    // Divider
    auto* div = new QFrame;
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM));
    div->setFixedHeight(1);
    vl->addWidget(div);

    // Connected providers header
    auto* prov_hdr = new QWidget;
    prov_hdr->setFixedHeight(36);
    prov_hdr->setStyleSheet(QString("background:%1;").arg(col::BG_RAISED));
    auto* prov_hdr_l = new QHBoxLayout(prov_hdr);
    prov_hdr_l->setContentsMargins(16, 0, 14, 0);

    auto* prov_title = new QLabel("Connected Providers");
    prov_title->setObjectName("dsSidebarSectionLabel");
    prov_hdr_l->addWidget(prov_title, 1);
    vl->addWidget(prov_hdr);

    // Provider ladder
    provider_ladder_ = new QListWidget;
    provider_ladder_->setObjectName("dsProviderLadder");
    provider_ladder_->setFrameShape(QFrame::NoFrame);
    provider_ladder_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(provider_ladder_, &QListWidget::itemClicked,
            this, &DataSourcesScreen::on_provider_ladder_activated);
    vl->addWidget(provider_ladder_, 1);

    return panel;
}

// ── Connector panel (main table) ─────────────────────────────────────────────

QWidget* DataSourcesScreen::build_connector_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dsPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget;
    hdr->setObjectName("dsPanelHeader");
    hdr->setFixedHeight(44);
    auto* hdr_l = new QHBoxLayout(hdr);
    hdr_l->setContentsMargins(16, 0, 16, 0);
    hdr_l->setSpacing(8);

    auto* hdr_title = new QLabel("Connectors");
    hdr_title->setObjectName("dsPanelTitle");
    hdr_l->addWidget(hdr_title, 1);

    auto* hint = new QLabel("Double-click to configure");
    hint->setObjectName("dsPanelMeta");
    hdr_l->addWidget(hint);

    vl->addWidget(hdr);

    // Connector table
    connector_table_ = new QTableWidget;
    connector_table_->setObjectName("dsConnectorTable");
    connector_table_->setColumnCount(7);
    connector_table_->setHorizontalHeaderLabels({"", "Connector", "Category", "Auth", "Fields", "Active", "Total"});
    connector_table_->verticalHeader()->setVisible(false);
    connector_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connector_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    connector_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connector_table_->setShowGrid(false);
    connector_table_->setAlternatingRowColors(false);
    connector_table_->setFrameShape(QFrame::NoFrame);
    connector_table_->setWordWrap(false);
    connector_table_->horizontalHeader()->setStretchLastSection(false);
    connector_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connector_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connector_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connector_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    connector_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    connector_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    connector_table_->setColumnWidth(0, 48);
    connect(connector_table_, &QTableWidget::itemSelectionChanged,
            this, &DataSourcesScreen::on_connector_selection_changed);
    connect(connector_table_, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) {
                auto* item = connector_table_->item(row, 1);
                if (item) on_connector_clicked(item->data(Qt::UserRole).toString());
            });
    vl->addWidget(connector_table_, 1);

    return panel;
}

// ── Detail / inspector panel ─────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_detail_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dsPanel");
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(340);

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget;
    hdr->setObjectName("dsPanelHeader");
    hdr->setFixedHeight(44);
    auto* hdr_l = new QHBoxLayout(hdr);
    hdr_l->setContentsMargins(16, 0, 16, 0);

    auto* hdr_title = new QLabel("Inspector");
    hdr_title->setObjectName("dsPanelTitle");
    hdr_l->addWidget(hdr_title, 1);

    auto* hdr_meta = new QLabel("Config & Status");
    hdr_meta->setObjectName("dsPanelMeta");
    hdr_l->addWidget(hdr_meta);
    vl->addWidget(hdr);

    // Scrollable body
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* body = new QWidget;
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(14, 14, 14, 14);
    body_vl->setSpacing(10);

    // Hero (icon + title)
    auto* hero = new QWidget;
    auto* hero_hl = new QHBoxLayout(hero);
    hero_hl->setContentsMargins(0, 0, 0, 0);
    hero_hl->setSpacing(12);

    detail_symbol_ = new QLabel("--");
    detail_symbol_->setAlignment(Qt::AlignCenter);
    detail_symbol_->setObjectName("dsInspectorSymbol");
    hero_hl->addWidget(detail_symbol_);

    auto* hero_text_vl = new QVBoxLayout;
    hero_text_vl->setContentsMargins(0, 0, 0, 0);
    hero_text_vl->setSpacing(3);

    detail_title_ = new QLabel("Select a connector");
    detail_title_->setObjectName("dsInspectorTitle");
    hero_text_vl->addWidget(detail_title_);

    auto* detail_key = new QLabel("Connector Profile");
    detail_key->setObjectName("dsInspectorKey");
    hero_text_vl->addWidget(detail_key);
    hero_text_vl->addStretch();

    hero_hl->addLayout(hero_text_vl, 1);
    body_vl->addWidget(hero);

    // Description
    detail_description_ = new QLabel("Select a connector to inspect its fields and manage saved connections.");
    detail_description_->setWordWrap(true);
    detail_description_->setObjectName("dsInspectorDesc");
    body_vl->addWidget(detail_description_);

    // Divider
    auto* info_div = new QFrame;
    info_div->setFrameShape(QFrame::HLine);
    info_div->setStyleSheet(QString("background:%1;").arg(col::BORDER_DIM));
    info_div->setFixedHeight(1);
    body_vl->addWidget(info_div);

    // Metric rows
    body_vl->addWidget(create_metric_row("Category",    &detail_category_value_));
    body_vl->addWidget(create_metric_row("Transport",   &detail_transport_value_));
    body_vl->addWidget(create_metric_row("Auth",        &detail_auth_value_));
    body_vl->addWidget(create_metric_row("Testable",    &detail_test_value_));
    body_vl->addWidget(create_metric_row("Fields",      &detail_fields_value_));
    body_vl->addWidget(create_metric_row("Configured",  &detail_configured_value_));
    body_vl->addWidget(create_metric_row("Active",      &detail_enabled_value_));
    body_vl->addWidget(create_metric_row("Last Tested", &detail_last_tested_value_));
    body_vl->addWidget(create_metric_row("Last Status", &detail_last_status_value_));

    // Action buttons
    auto* actions = new QWidget;
    auto* act_hl = new QHBoxLayout(actions);
    act_hl->setContentsMargins(0, 4, 0, 4);
    act_hl->setSpacing(6);

    new_connection_btn_ = new QPushButton("+ New Connection");
    new_connection_btn_->setObjectName("dsActionBtn");
    new_connection_btn_->setProperty("accent", true);
    new_connection_btn_->setCursor(Qt::PointingHandCursor);
    connect(new_connection_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_connection_add);
    act_hl->addWidget(new_connection_btn_);

    edit_connection_btn_ = new QPushButton("Edit");
    edit_connection_btn_->setObjectName("dsActionBtn");
    edit_connection_btn_->setCursor(Qt::PointingHandCursor);
    connect(edit_connection_btn_, &QPushButton::clicked, this, [this]() {
        const QString conn_id = effective_detail_connection_id();
        if (!conn_id.isEmpty()) on_connection_edit(conn_id);
    });
    act_hl->addWidget(edit_connection_btn_);

    test_connection_btn_ = new QPushButton("Test");
    test_connection_btn_->setObjectName("dsActionBtn");
    test_connection_btn_->setCursor(Qt::PointingHandCursor);
    connect(test_connection_btn_, &QPushButton::clicked, this, [this]() {
        const QString conn_id = effective_detail_connection_id();
        if (!conn_id.isEmpty()) on_connection_test(conn_id);
    });
    act_hl->addWidget(test_connection_btn_);

    auto* dup_btn = new QPushButton("Duplicate");
    dup_btn->setObjectName("dsActionBtn");
    dup_btn->setCursor(Qt::PointingHandCursor);
    dup_btn->setToolTip("Clone this connection with a new name");
    connect(dup_btn, &QPushButton::clicked, this, [this]() {
        const QString conn_id = effective_detail_connection_id();
        if (!conn_id.isEmpty()) on_connection_duplicate(conn_id);
    });
    act_hl->addWidget(dup_btn);

    body_vl->addWidget(actions);

    // Fields section
    auto* fields_lbl = new QLabel("Field Map");
    fields_lbl->setObjectName("dsSectionLabel");
    body_vl->addWidget(fields_lbl);

    field_table_ = new QTableWidget;
    field_table_->setObjectName("dsFieldTable");
    field_table_->setColumnCount(4);
    field_table_->setHorizontalHeaderLabels({"Field", "Type", "Req", "Default"});
    field_table_->verticalHeader()->setVisible(false);
    field_table_->setSelectionMode(QAbstractItemView::NoSelection);
    field_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    field_table_->setShowGrid(false);
    field_table_->setFrameShape(QFrame::NoFrame);
    field_table_->horizontalHeader()->setStretchLastSection(true);
    field_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    field_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    field_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    field_table_->setFixedHeight(160);
    body_vl->addWidget(field_table_);

    // Connections section
    auto* saved_lbl = new QLabel("Saved Connections");
    saved_lbl->setObjectName("dsSectionLabel");
    body_vl->addWidget(saved_lbl);

    detail_connections_list_ = new QListWidget;
    detail_connections_list_->setObjectName("dsDetailConnList");
    detail_connections_list_->setFrameShape(QFrame::NoFrame);
    detail_connections_list_->setMinimumHeight(80);
    connect(detail_connections_list_, &QListWidget::itemClicked,
            this, &DataSourcesScreen::on_detail_connection_activated);
    body_vl->addWidget(detail_connections_list_, 1);

    scroll->setWidget(body);
    vl->addWidget(scroll, 1);

    return panel;
}

// ── Connections bottom panel ─────────────────────────────────────────────────

QWidget* DataSourcesScreen::build_connections_panel() {
    auto* panel = new QWidget;
    panel->setObjectName("dsPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Panel header
    auto* hdr = new QWidget;
    hdr->setObjectName("dsPanelHeader");
    hdr->setFixedHeight(44);
    auto* hdr_l = new QHBoxLayout(hdr);
    hdr_l->setContentsMargins(16, 0, 16, 0);
    hdr_l->setSpacing(8);

    auto* hdr_title = new QLabel("Saved Connections");
    hdr_title->setObjectName("dsPanelTitle");
    hdr_l->addWidget(hdr_title, 1);

    auto* hdr_meta = new QLabel("Enable | Edit | Test | Duplicate | Delete");
    hdr_meta->setObjectName("dsPanelMeta");
    hdr_l->addWidget(hdr_meta);
    vl->addWidget(hdr);

    // Connections toolbar: search + bulk actions
    auto* toolbar = new QWidget;
    toolbar->setObjectName("dsConnToolbar");
    toolbar->setFixedHeight(38);
    auto* tb_hl = new QHBoxLayout(toolbar);
    tb_hl->setContentsMargins(10, 4, 10, 4);
    tb_hl->setSpacing(6);

    // Select-all checkbox
    select_all_check_ = new QCheckBox;
    select_all_check_->setObjectName("dsSelectAll");
    select_all_check_->setToolTip("Select / deselect all visible connections");
    connect(select_all_check_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!connections_table_) return;
        connections_table_->setSelectionMode(
            checked ? QAbstractItemView::MultiSelection : QAbstractItemView::SingleSelection);
        if (checked) connections_table_->selectAll();
        else         connections_table_->clearSelection();
    });
    tb_hl->addWidget(select_all_check_);

    // Connection search
    conn_search_edit_ = new QLineEdit;
    conn_search_edit_->setObjectName("dsConnSearch");
    conn_search_edit_->setPlaceholderText("Filter connections…");
    conn_search_edit_->setFixedHeight(28);
    conn_search_edit_->setFixedWidth(200);
    connect(conn_search_edit_, &QLineEdit::textChanged,
            this, &DataSourcesScreen::on_connections_search_changed);
    tb_hl->addWidget(conn_search_edit_);

    tb_hl->addStretch();

    // Bulk buttons
    bulk_enable_btn_ = new QPushButton("Enable All");
    bulk_enable_btn_->setObjectName("dsBulkBtn");
    bulk_enable_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_enable_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_enable_all);
    tb_hl->addWidget(bulk_enable_btn_);

    bulk_disable_btn_ = new QPushButton("Disable All");
    bulk_disable_btn_->setObjectName("dsBulkBtn");
    bulk_disable_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_disable_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_disable_all);
    tb_hl->addWidget(bulk_disable_btn_);

    bulk_delete_btn_ = new QPushButton("Delete Selected");
    bulk_delete_btn_->setObjectName("dsBulkBtn");
    bulk_delete_btn_->setProperty("danger", true);
    bulk_delete_btn_->setCursor(Qt::PointingHandCursor);
    connect(bulk_delete_btn_, &QPushButton::clicked, this, &DataSourcesScreen::on_bulk_delete_selected);
    tb_hl->addWidget(bulk_delete_btn_);

    vl->addWidget(toolbar);

    // Connections table — extended to 9 cols (added Live Status)
    connections_table_ = new QTableWidget;
    connections_table_->setObjectName("dsConnectionsTable");
    connections_table_->setColumnCount(9);
    connections_table_->setHorizontalHeaderLabels(
        {"On", "Name", "Provider", "Category", "Type", "Live", "Tags", "Updated", "Actions"});
    connections_table_->verticalHeader()->setVisible(false);
    connections_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connections_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    connections_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connections_table_->setShowGrid(false);
    connections_table_->setAlternatingRowColors(false);
    connections_table_->setFrameShape(QFrame::NoFrame);
    connections_table_->horizontalHeader()->setStretchLastSection(false);
    connections_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connections_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    connections_table_->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    connections_table_->setColumnWidth(0, 44);
    connections_table_->setColumnWidth(8, 200);
    connect(connections_table_, &QTableWidget::itemSelectionChanged,
            this, &DataSourcesScreen::on_connection_selection_changed);
    vl->addWidget(connections_table_, 1);

    return panel;
}

// ── setup_ui ─────────────────────────────────────────────────────────────────

void DataSourcesScreen::setup_ui() {
    apply_screen_styles();

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &DataSourcesScreen::update_clock);

    // Live status poll — 30s interval, only runs while screen is visible
    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(30000);
    connect(poll_timer_, &QTimer::timeout, this, &DataSourcesScreen::on_poll_timer);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_header_bar());
    root->addWidget(build_command_bar()); // zero-height placeholder
    root->addWidget(build_stats_strip());

    auto* main_splitter = new QSplitter(Qt::Vertical);
    main_splitter->setObjectName("dsMainSplitter");
    main_splitter->setHandleWidth(1);
    main_splitter->setChildrenCollapsible(false);

    auto* workspace_splitter = new QSplitter(Qt::Horizontal);
    workspace_splitter->setObjectName("dsWorkspaceSplitter");
    workspace_splitter->setHandleWidth(1);
    workspace_splitter->setChildrenCollapsible(false);
    workspace_splitter->addWidget(build_category_panel());
    workspace_splitter->addWidget(build_connector_panel());
    workspace_splitter->addWidget(build_detail_panel());
    workspace_splitter->setStretchFactor(0, 0);
    workspace_splitter->setStretchFactor(1, 1);
    workspace_splitter->setStretchFactor(2, 0);
    workspace_splitter->setSizes({260, 560, 300});

    main_splitter->addWidget(workspace_splitter);
    main_splitter->addWidget(build_connections_panel());
    main_splitter->setStretchFactor(0, 3);
    main_splitter->setStretchFactor(1, 2);
    main_splitter->setSizes({520, 280});
    main_splitter->setCollapsible(0, true);
    main_splitter->setCollapsible(1, true);

    root->addWidget(main_splitter, 1);
}

// ── Config dialog ─────────────────────────────────────────────────────────────

void DataSourcesScreen::show_config_dialog(const ConnectorConfig& config,
                                           const QString& edit_id,
                                           bool           duplicate) {
    const bool editing = !edit_id.isEmpty() && !duplicate;

    DataSource existing;
    QJsonObject existing_cfg;
    bool existing_loaded = false;

    if (editing) {
        auto result = DataSourceRepository::instance().get(edit_id);
        if (result.is_ok()) {
            existing = result.value();
            existing_cfg = QJsonDocument::fromJson(existing.config.toUtf8()).object();
            existing_loaded = true;
        }
    }

    QString saved_id;

    QDialog dlg(this);
    dlg.setWindowTitle(editing    ? QString("Edit — %1").arg(config.name)
                      : duplicate ? QString("Clone — %1").arg(config.name)
                                  : QString("Configure — %1").arg(config.name));
    dlg.resize(560, 620);
    dlg.setModal(true);
    dlg.setStyleSheet(QString(
        "QDialog { background:%1; color:%2; }"
        "QLabel { color:%3; font-size:12px; background:transparent; }"
        "QLineEdit, QTextEdit, QComboBox { background:%4; border:1px solid %5; color:%2;"
        "  padding:6px 10px; font-size:13px; border-radius:3px; }"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border-color:%6; }"
        "QCheckBox { color:%2; font-size:13px; }"
        "QPushButton { padding:7px 18px; font-size:12px; font-weight:700; border-radius:4px; }")
        .arg(col::BG_SURFACE, col::TEXT_PRIMARY, col::TEXT_SECONDARY,
             col::BG_BASE, col::BORDER_DIM, col::AMBER));

    auto* root_vl = new QVBoxLayout(&dlg);
    root_vl->setContentsMargins(0, 0, 0, 0);
    root_vl->setSpacing(0);

    // Dialog header
    auto* dlg_hdr = new QWidget;
    dlg_hdr->setFixedHeight(56);
    dlg_hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_RAISED, col::BORDER_DIM));
    auto* dlg_hdr_hl = new QHBoxLayout(dlg_hdr);
    dlg_hdr_hl->setContentsMargins(16, 0, 16, 0);
    dlg_hdr_hl->setSpacing(12);

    auto* code_lbl = new QLabel(connector_code(config));
    code_lbl->setAlignment(Qt::AlignCenter);
    code_lbl->setFixedSize(36, 36);
    code_lbl->setStyleSheet(
        QString("background:%1;color:%2;border:1px solid %3;font-size:14px;"
                "font-weight:700;border-radius:6px;")
            .arg(config.color, col::TEXT_PRIMARY, col::BORDER_DIM));
    dlg_hdr_hl->addWidget(code_lbl);

    auto* title_vl = new QVBoxLayout;
    title_vl->setContentsMargins(0, 0, 0, 0);
    title_vl->setSpacing(2);

    auto* dlg_title = new QLabel(editing    ? QString("Edit  %1").arg(config.name)
                                : duplicate ? QString("Clone  %1").arg(config.name)
                                            : config.name);
    dlg_title->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;").arg(col::AMBER));
    title_vl->addWidget(dlg_title);

    auto* dlg_sub = new QLabel(config.description);
    dlg_sub->setWordWrap(true);
    dlg_sub->setStyleSheet(
        QString("color:%1;font-size:11px;background:transparent;").arg(col::TEXT_SECONDARY));
    title_vl->addWidget(dlg_sub);

    dlg_hdr_hl->addLayout(title_vl, 1);
    root_vl->addWidget(dlg_hdr);

    // Scrollable form body
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(
        QString("QScrollArea { background:%1; border:none; }").arg(col::BG_SURFACE));

    auto* body = new QWidget;
    auto* body_vl = new QVBoxLayout(body);
    body_vl->setContentsMargins(20, 20, 20, 20);
    body_vl->setSpacing(16);

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setColumnStretch(1, 1);
    form->setColumnMinimumWidth(0, 130);

    int row = 0;

    auto* name_lbl = new QLabel("Connection Name");
    name_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(name_lbl, row, 0);

    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText(config.name + " Connection");
    name_edit->setText(existing_loaded
        ? (duplicate ? QString("Copy of %1").arg(existing.display_name) : existing.display_name)
        : "");
    name_edit->setFixedHeight(34);
    form->addWidget(name_edit, row, 1);
    ++row;

    auto* enabled_lbl = new QLabel("Enable Connection");
    enabled_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(enabled_lbl, row, 0);

    auto* enabled_check = new QCheckBox("Active");
    enabled_check->setChecked(existing_loaded ? existing.enabled : true);
    form->addWidget(enabled_check, row, 1);
    ++row;

    QMap<QString, QWidget*> field_widgets;

    for (const auto& field : config.fields) {
        auto* lbl = new QLabel(field.label + (field.required ? " *" : ""));
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->addWidget(lbl, row, 0);

        QWidget* input = nullptr;
        if (field.type == FieldType::Checkbox) {
            auto* check = new QCheckBox(field.required ? "Required" : "Optional");
            const bool value = existing_cfg.contains(field.name)
                ? existing_cfg.value(field.name).toBool()
                : (field.default_value == "true");
            check->setChecked(value);
            input = check;
        } else if (field.type == FieldType::Select) {
            auto* combo = new QComboBox;
            for (const auto& option : field.options)
                combo->addItem(option.label, option.value);
            const QString current = existing_cfg.contains(field.name)
                ? existing_cfg.value(field.name).toString()
                : field.default_value;
            const int index = combo->findData(current);
            if (index >= 0) combo->setCurrentIndex(index);
            input = combo;
        } else if (field.type == FieldType::Textarea) {
            auto* edit = new QTextEdit;
            edit->setMaximumHeight(90);
            edit->setPlaceholderText(field.placeholder);
            edit->setPlainText(existing_cfg.contains(field.name)
                ? existing_cfg.value(field.name).toString()
                : field.default_value);
            input = edit;
        } else {
            auto* edit = new QLineEdit;
            edit->setPlaceholderText(field.placeholder);
            edit->setFixedHeight(34);
            if (field.type == FieldType::Password)
                edit->setEchoMode(QLineEdit::Password);
            if (field.type == FieldType::Number)
                edit->setValidator(new QIntValidator(0, 999999999, edit));
            const QString text = existing_cfg.contains(field.name)
                ? existing_cfg.value(field.name).toVariant().toString()
                : field.default_value;
            edit->setText(text);
            input = edit;
        }

        field_widgets[field.name] = input;
        form->addWidget(input, row, 1);
        ++row;
    }

    // Tags field (generic — appended after connector-specific fields)
    auto* tags_lbl = new QLabel("Tags");
    tags_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->addWidget(tags_lbl, row, 0);

    auto* tags_edit = new QLineEdit;
    tags_edit->setPlaceholderText("Comma-separated tags, e.g. prod, live, trading");
    tags_edit->setFixedHeight(34);
    tags_edit->setText(existing_loaded ? existing.tags : "");
    form->addWidget(tags_edit, row, 1);
    ++row;

    body_vl->addLayout(form);

    auto* note = new QLabel("Fields marked with * are required.");
    note->setWordWrap(true);
    note->setStyleSheet(
        QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
            .arg(col::TEXT_TERTIARY));
    body_vl->addWidget(note);
    body_vl->addStretch();

    scroll->setWidget(body);
    root_vl->addWidget(scroll, 1);

    // Dialog footer
    auto* footer = new QWidget;
    footer->setFixedHeight(54);
    footer->setStyleSheet(
        QString("background:%1;border-top:1px solid %2;").arg(col::BG_RAISED, col::BORDER_DIM));
    auto* footer_hl = new QHBoxLayout(footer);
    footer_hl->setContentsMargins(16, 0, 16, 0);
    footer_hl->setSpacing(8);

    auto* status = new QLabel;
    status->setStyleSheet(
        QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY));
    footer_hl->addWidget(status, 1);

    auto* cancel = new QPushButton("Cancel");
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;}"
                "QPushButton:hover{background:%3;color:%4;}")
            .arg(col::BG_BASE, col::TEXT_SECONDARY, col::BORDER_MED, col::TEXT_PRIMARY));
    footer_hl->addWidget(cancel);

    auto* save = new QPushButton(editing ? "Update Connection" : "Save Connection");
    save->setCursor(Qt::PointingHandCursor);
    save->setDefault(true);
    save->setAutoDefault(true);
    save->setStyleSheet(
        QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;border:1px solid %2;}"
                "QPushButton:hover{background:%1;color:%3;}")
            .arg(col::AMBER, col::AMBER_DIM, col::BG_BASE));
    footer_hl->addWidget(save);

    root_vl->addWidget(footer);

    name_edit->setFocus();

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dlg, [&, existing_loaded]() {
        QJsonObject cfg_json;

        for (const auto& field : config.fields) {
            QWidget* widget = field_widgets.value(field.name, nullptr);
            QString text_value;

            if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
                text_value = edit->text().trimmed();
                if (field.type == FieldType::Number)
                    cfg_json[field.name] = text_value.toInt();
                else
                    cfg_json[field.name] = text_value;
            } else if (auto* edit = qobject_cast<QTextEdit*>(widget)) {
                text_value = edit->toPlainText().trimmed();
                cfg_json[field.name] = text_value;
            } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
                text_value = combo->currentData().toString();
                cfg_json[field.name] = text_value;
            } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
                cfg_json[field.name] = check->isChecked();
            }

            if (field.required && field.type != FieldType::Checkbox && text_value.isEmpty()) {
                status->setText("Missing required field: " + field.label);
                status->setStyleSheet(
                    QString("color:%1;font-size:12px;font-weight:700;background:transparent;")
                        .arg(col::NEGATIVE));
                return;
            }
        }

        DataSource ds = (existing_loaded && !duplicate) ? existing : DataSource{};
        ds.id = (existing_loaded && !duplicate)
            ? existing.id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias = (existing_loaded && !duplicate) && !existing.alias.isEmpty()
            ? existing.alias : (config.id + "_" + ds.id.left(8));
        ds.display_name = name_edit->text().trimmed().isEmpty()
            ? config.name : name_edit->text().trimmed();
        ds.description  = config.description;
        ds.type         = persistence_type(config);
        ds.provider     = config.id;
        ds.category     = category_str(config.category);
        ds.config       = QJsonDocument(cfg_json).toJson(QJsonDocument::Compact);
        ds.enabled      = enabled_check->isChecked();
        ds.tags         = tags_edit->text().trimmed();

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_err()) {
            status->setText("Failed to save: " + QString::fromStdString(result.error()));
            status->setStyleSheet(
                QString("color:%1;font-size:12px;font-weight:700;background:transparent;")
                    .arg(col::NEGATIVE));
            return;
        }

        saved_id = ds.id;
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted && !saved_id.isEmpty()) {
        selected_connector_id_ = config.id;
        selected_connection_id_ = saved_id;
        refresh_connections();
    }
}

// ── Data functions (unchanged) ────────────────────────────────────────────────

QVector<ConnectorConfig> DataSourcesScreen::filtered_connectors() const {
    QVector<ConnectorConfig> filtered;
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    // Pre-build set of configured/active connector IDs for stat filter
    QSet<QString> configured_ids, active_ids;
    if (stat_filter_ == 1 || stat_filter_ == 2) {
        for (const auto& ds : connections_cache_) {
            configured_ids.insert(normalized_provider_key(ds));
            if (ds.enabled) active_ids.insert(normalized_provider_key(ds));
        }
    }

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!show_all_categories_ && cfg.category != active_category_) continue;
        if (!connector_matches_text(cfg, filter)) continue;
        if (stat_filter_ == 1 && !configured_ids.contains(cfg.id)) continue;
        if (stat_filter_ == 2 && !active_ids.contains(cfg.id)) continue;
        if (stat_filter_ == 3 && !cfg.requires_auth) continue;
        filtered.append(cfg);
    }
    return filtered;
}

void DataSourcesScreen::build_category_ladder() {
    if (!category_list_) return;

    QVector<int> counts(10, 0);
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!connector_matches_text(cfg, filter)) continue;
        ++counts[0];
        ++counts[static_cast<int>(cfg.category) + 1];
    }

    QSignalBlocker blocker(category_list_);
    category_list_->clear();

    for (int i = 0; i < kCategoryLabels.size(); ++i) {
        const QString text = QString("%1  (%2)").arg(kCategoryLabels[i]).arg(counts[i]);
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        item->setForeground(i == 0 ? QColor(col::TEXT_PRIMARY) : QColor(col::TEXT_SECONDARY));
        category_list_->addItem(item);
    }

    const int active_row = show_all_categories_ ? 0 : (static_cast<int>(active_category_) + 1);
    if (category_list_->count() > active_row)
        category_list_->setCurrentRow(active_row);
}

void DataSourcesScreen::build_connector_table() {
    if (!connector_table_) return;

    const auto filtered = filtered_connectors();
    int preferred_row = -1;

    {
        QSignalBlocker blocker(connector_table_);
        connector_table_->setRowCount(filtered.size());

        for (int row = 0; row < filtered.size(); ++row) {
            const auto& cfg = filtered[row];
            const int total_saved = total_connections_for_provider(connections_cache_, cfg.id);
            const int live_saved  = enabled_connections_for_provider(connections_cache_, cfg.id);

            connector_table_->setItem(row, 0, make_item(connector_code(cfg), QColor(cfg.color), Qt::AlignCenter));

            auto* name_item = make_item(cfg.name);
            name_item->setData(Qt::UserRole, cfg.id);
            connector_table_->setItem(row, 1, name_item);

            connector_table_->setItem(row, 2, make_item(category_label(cfg.category), TEXT_SECONDARY));
            connector_table_->setItem(row, 3, make_item(cfg.requires_auth ? "Key" : "Open",
                cfg.requires_auth ? COLOR_WARNING : COLOR_BUY, Qt::AlignCenter));
            connector_table_->setItem(row, 4, make_item(QString::number(cfg.fields.size()),
                TEXT_PRIMARY, Qt::AlignCenter));
            connector_table_->setItem(row, 5, make_item(QString::number(live_saved),
                live_saved > 0 ? COLOR_BUY : TEXT_TERTIARY, Qt::AlignCenter));
            connector_table_->setItem(row, 6, make_item(QString::number(total_saved),
                total_saved > 0 ? COLOR_ACCENT : TEXT_TERTIARY, Qt::AlignCenter));
            connector_table_->setRowHeight(row, 32);

            if (cfg.id == selected_connector_id_)
                preferred_row = row;
        }

        if (preferred_row < 0 && !filtered.isEmpty()) {
            selected_connector_id_ = filtered.first().id;
            preferred_row = 0;
        } else if (filtered.isEmpty()) {
            selected_connector_id_.clear();
        }

        if (preferred_row >= 0) connector_table_->selectRow(preferred_row);
        else connector_table_->clearSelection();
    }

    update_detail_panel();
}

void DataSourcesScreen::build_connections_table() {
    if (!connections_table_) return;

    const auto visible_connections = filtered_connection_rows();
    int preferred_row = -1;

    {
        QSignalBlocker blocker(connections_table_);
        connections_table_->setRowCount(visible_connections.size());

        for (int row = 0; row < visible_connections.size(); ++row) {
            const auto& ds = visible_connections[row];
            const QString connector_id = normalized_provider_key(ds);
            const auto* cfg = find_connector_config(connector_id);

            auto* toggle = new QCheckBox;
            toggle->setObjectName("dsEnableToggle");
            toggle->setChecked(ds.enabled);
            toggle->setCursor(Qt::PointingHandCursor);
            connect(toggle, &QCheckBox::toggled, this,
                [this, conn_id = ds.id](bool enabled) {
                    on_connection_enabled_changed(conn_id, enabled);
                });
            connections_table_->setCellWidget(row, 0, toggle);

            auto* name_item = make_item(ds.display_name);
            name_item->setData(Qt::UserRole, ds.id);
            connections_table_->setItem(row, 1, name_item);

            connections_table_->setItem(row, 2,
                make_item(cfg ? cfg->name : ds.provider, cfg ? QColor(cfg->color) : TEXT_SECONDARY));
            connections_table_->setItem(row, 3,
                make_item(cfg ? category_label(cfg->category) : ds.category, TEXT_SECONDARY));
            connections_table_->setItem(row, 4,
                make_item(ds.type == "websocket" ? "WS" : "REST",
                    ds.type == "websocket" ? COLOR_ACCENT : TEXT_PRIMARY, Qt::AlignCenter));

            // Col 5: Live status (from cache or pending)
            {
                const auto it = live_status_cache_.find(ds.id);
                QString dot_text = "-";
                QColor  dot_col  = TEXT_TERTIARY;
                if (it != live_status_cache_.end()) {
                    dot_text = it->first ? "OK" : "ERR";
                    dot_col  = it->first ? COLOR_BUY : COLOR_SELL;
                }
                auto* status_lbl = new QLabel(dot_text);
                status_lbl->setObjectName("dsStatusDot");
                status_lbl->setAlignment(Qt::AlignCenter);
                status_lbl->setStyleSheet(
                    QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
                        .arg(dot_col.name()));
                status_lbl->setToolTip(it != live_status_cache_.end() ? it->second : "Not yet tested");
                connections_table_->setCellWidget(row, 5, status_lbl);
            }

            // Col 6: Tags
            connections_table_->setItem(row, 6,
                make_item(ds.tags.isEmpty() ? "—" : ds.tags, TEXT_TERTIARY));

            connections_table_->setItem(row, 7, make_item(ds.updated_at, TEXT_TERTIARY));

            auto* act_widget = new QWidget;
            auto* act_hl = new QHBoxLayout(act_widget);
            act_hl->setContentsMargins(4, 2, 4, 2);
            act_hl->setSpacing(4);

            auto* edit_btn = new QPushButton("Edit");
            edit_btn->setObjectName("dsInlineAction");
            edit_btn->setProperty("accent", true);
            edit_btn->setCursor(Qt::PointingHandCursor);
            connect(edit_btn, &QPushButton::clicked, this,
                [this, conn_id = ds.id]() { on_connection_edit(conn_id); });
            act_hl->addWidget(edit_btn);

            auto* test_btn = new QPushButton("Test");
            test_btn->setObjectName("dsInlineAction");
            test_btn->setProperty("test", true);
            test_btn->setCursor(Qt::PointingHandCursor);
            connect(test_btn, &QPushButton::clicked, this,
                [this, conn_id = ds.id]() { on_connection_test(conn_id); });
            act_hl->addWidget(test_btn);

            auto* dup_btn2 = new QPushButton("Clone");
            dup_btn2->setObjectName("dsInlineAction");
            dup_btn2->setCursor(Qt::PointingHandCursor);
            connect(dup_btn2, &QPushButton::clicked, this,
                [this, conn_id = ds.id]() { on_connection_duplicate(conn_id); });
            act_hl->addWidget(dup_btn2);

            auto* del_btn = new QPushButton("Delete");
            del_btn->setObjectName("dsInlineAction");
            del_btn->setProperty("danger", true);
            del_btn->setCursor(Qt::PointingHandCursor);
            connect(del_btn, &QPushButton::clicked, this,
                [this, conn_id = ds.id]() { on_connection_delete(conn_id); });
            act_hl->addWidget(del_btn);

            connections_table_->setCellWidget(row, 8, act_widget);
            connections_table_->setRowHeight(row, 32);

            if (ds.id == selected_connection_id_) preferred_row = row;
            if (preferred_row < 0 && connector_id == selected_connector_id_) preferred_row = row;
        }

        if (preferred_row >= 0) connections_table_->selectRow(preferred_row);
        else connections_table_->clearSelection();
    }

    on_connection_selection_changed();
}

void DataSourcesScreen::update_provider_ladder() {
    if (!provider_ladder_) return;

    const auto visible_connectors = filtered_connectors();
    QSet<QString> allowed_connectors;
    for (const auto& cfg : visible_connectors)
        allowed_connectors.insert(cfg.id);

    struct ProviderSummary { QString id, name, color; int total = 0, live = 0; };
    QMap<QString, ProviderSummary> grouped;

    for (const auto& ds : connections_cache_) {
        const QString connector_id = normalized_provider_key(ds);
        if (!allowed_connectors.contains(connector_id)) continue;
        auto& s = grouped[connector_id];
        s.id = connector_id;
        if (const auto* cfg = find_connector_config(connector_id)) {
            s.name = cfg->name; s.color = cfg->color;
        } else {
            s.name = connector_id; s.color = col::TEXT_PRIMARY;
        }
        ++s.total;
        if (ds.enabled) ++s.live;
    }

    QVector<ProviderSummary> summaries;
    for (const auto& s : grouped.values()) summaries.append(s);
    std::sort(summaries.begin(), summaries.end(), [](const ProviderSummary& a, const ProviderSummary& b) {
        return a.total != b.total ? a.total > b.total : a.name.toLower() < b.name.toLower();
    });

    QSignalBlocker blocker(provider_ladder_);
    provider_ladder_->clear();

    if (summaries.isEmpty()) {
        auto* empty = new QListWidgetItem("No configured providers");
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(col::TEXT_TERTIARY));
        provider_ladder_->addItem(empty);
        return;
    }

    for (const auto& s : summaries) {
        const QString text = QString("%1  —  %2/%3 live").arg(s.name).arg(s.live).arg(s.total);
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, s.id);
        item->setToolTip(QString("%1 live / %2 configured").arg(s.live).arg(s.total));
        item->setForeground(s.live > 0 ? QColor(s.color) : QColor(col::TEXT_SECONDARY));
        provider_ladder_->addItem(item);
    }
}

void DataSourcesScreen::update_stats_strip() {
    const auto visible_connectors = filtered_connectors();
    const int total_connectors = ConnectorRegistry::instance().count();
    const int visible_count    = visible_connectors.size();

    QSet<QString> visible_ids;
    int auth_required = 0;
    for (const auto& cfg : visible_connectors) {
        visible_ids.insert(cfg.id);
        if (cfg.requires_auth) ++auth_required;
    }

    int configured = 0, active = 0;
    for (const auto& ds : connections_cache_) {
        if (!visible_ids.contains(normalized_provider_key(ds))) continue;
        ++configured;
        if (ds.enabled) ++active;
    }

    const QString active_style =
        QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::AMBER);
    const QString normal_style =
        QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::TEXT_PRIMARY);

    if (universe_stat_value_) {
        universe_stat_value_->setText(QString("%1/%2").arg(visible_count).arg(total_connectors));
        universe_stat_value_->setStyleSheet(stat_filter_ == 0 ? active_style : normal_style);
    }
    if (configured_stat_value_) {
        configured_stat_value_->setText(QString::number(configured));
        configured_stat_value_->setStyleSheet(stat_filter_ == 1 ? active_style : normal_style);
    }
    if (active_stat_value_) {
        active_stat_value_->setText(QString::number(active));
        active_stat_value_->setStyleSheet(stat_filter_ == 2 ? active_style : normal_style);
    }
    if (auth_stat_value_) {
        auth_stat_value_->setText(QString::number(auth_required));
        auth_stat_value_->setStyleSheet(stat_filter_ == 3 ? active_style : normal_style);
    }

    if (count_label_)
        count_label_->setText(QString("%1/%2 visible  |  %3 cfg  |  %4 live")
            .arg(visible_count).arg(total_connectors).arg(configured).arg(active));

    if (selection_status_label_) {
        QString focus = "All categories";
        if (!selected_connector_id_.isEmpty()) {
            const auto* cfg = find_connector_config(selected_connector_id_);
            focus = cfg ? cfg->name : selected_connector_id_;
        }
        selection_status_label_->setText(focus);
    }

    if (clear_search_btn_)
        clear_search_btn_->setEnabled(search_edit_ && !search_edit_->text().trimmed().isEmpty());
}

void DataSourcesScreen::update_detail_panel() {
    if (!detail_title_ || !field_table_ || !detail_connections_list_) return;

    const auto* cfg = find_connector_config(selected_connector_id_);
    if (!cfg) {
        detail_symbol_->setText("--");
        detail_symbol_->setStyleSheet("");
        detail_title_->setText("Select a connector");
        detail_description_->setText("Select a connector from the list to inspect its fields and manage saved connections.");
        detail_category_value_->setText("--");
        detail_transport_value_->setText("--");
        detail_auth_value_->setText("--");
        detail_auth_value_->setStyleSheet("");
        detail_test_value_->setText("--");
        detail_fields_value_->setText("--");
        detail_configured_value_->setText("--");
        detail_enabled_value_->setText("--");
        if (detail_last_tested_value_) detail_last_tested_value_->setText("--");
        if (detail_last_status_value_) detail_last_status_value_->setText("--");
        field_table_->setRowCount(0);
        detail_connections_list_->clear();
        auto* empty = new QListWidgetItem("No connector selected");
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(col::TEXT_TERTIARY));
        detail_connections_list_->addItem(empty);
        update_action_states();
        return;
    }

    detail_symbol_->setText(connector_code(*cfg));
    detail_symbol_->setStyleSheet(
        QString("background:%1;color:%2;border:1px solid %3;"
                "font-size:15px;font-weight:700;border-radius:6px;")
            .arg(cfg->color, col::TEXT_PRIMARY, col::BORDER_DIM));
    detail_title_->setText(cfg->name);
    detail_description_->setText(cfg->description);
    detail_category_value_->setText(category_label(cfg->category));
    detail_transport_value_->setText(connector_transport(*cfg));
    detail_auth_value_->setText(cfg->requires_auth ? "Requires credentials" : "Open access");
    detail_auth_value_->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
            .arg(cfg->requires_auth ? col::WARNING : col::POSITIVE));
    detail_test_value_->setText(cfg->testable ? "Yes" : "No");
    detail_fields_value_->setText(QString::number(cfg->fields.size()));

    const int total_saved = total_connections_for_provider(connections_cache_, cfg->id);
    const int live_saved  = enabled_connections_for_provider(connections_cache_, cfg->id);
    detail_configured_value_->setText(QString::number(total_saved));
    detail_enabled_value_->setText(QString("%1 / %2").arg(live_saved).arg(total_saved));

    // Last tested / status from live cache — use preferred connection
    const QString pref_conn = preferred_connection_for_connector(cfg->id);
    const auto it = live_status_cache_.find(pref_conn);
    if (detail_last_tested_value_) {
        detail_last_tested_value_->setText(it != live_status_cache_.end() ? "Recently" : "Never");
    }
    if (detail_last_status_value_) {
        if (it != live_status_cache_.end()) {
            detail_last_status_value_->setText(it->first ? "OK" : "Failed");
            detail_last_status_value_->setStyleSheet(
                QString("color:%1;font-size:12px;font-weight:600;background:transparent;")
                    .arg(it->first ? col::POSITIVE : col::NEGATIVE));
        } else {
            detail_last_status_value_->setText("—");
            detail_last_status_value_->setStyleSheet("");
        }
    }

    {
        QSignalBlocker blocker(field_table_);
        field_table_->setRowCount(cfg->fields.size());
        for (int row = 0; row < cfg->fields.size(); ++row) {
            const auto& field = cfg->fields[row];
            field_table_->setItem(row, 0, make_item(field.label, TEXT_PRIMARY));
            field_table_->setItem(row, 1, make_item(field_type_label(field.type), TEXT_SECONDARY, Qt::AlignCenter));
            field_table_->setItem(row, 2, make_item(field.required ? "Req" : "Opt",
                field.required ? COLOR_WARNING : TEXT_TERTIARY, Qt::AlignCenter));
            QString preview = !field.default_value.isEmpty() ? field.default_value : field.placeholder;
            if (field.type == FieldType::Password && !preview.isEmpty())
                preview = "<secret>";
            field_table_->setItem(row, 3, make_item(compact_value(preview), TEXT_TERTIARY));
            field_table_->setRowHeight(row, 26);
        }
    }

    QVector<DataSource> connector_connections;
    for (const auto& ds : connections_cache_)
        if (connection_matches_connector(ds, cfg->id))
            connector_connections.append(ds);

    std::sort(connector_connections.begin(), connector_connections.end(),
        [](const DataSource& a, const DataSource& b) { return a.updated_at > b.updated_at; });

    QSignalBlocker blocker(detail_connections_list_);
    detail_connections_list_->clear();

    if (connector_connections.isEmpty()) {
        auto* empty = new QListWidgetItem("No saved connections");
        empty->setFlags(Qt::NoItemFlags);
        empty->setForeground(QColor(col::TEXT_TERTIARY));
        detail_connections_list_->addItem(empty);
    } else {
        bool current_set = false;
        for (const auto& ds : connector_connections) {
            const QString text = QString("%1  %2")
                .arg(ds.enabled ? "[Live]" : "[Standby]", ds.display_name);
            auto* item = new QListWidgetItem(text);
            item->setData(Qt::UserRole, ds.id);
            item->setToolTip(ds.description);
            item->setForeground(ds.enabled ? QColor(col::POSITIVE) : QColor(col::TEXT_SECONDARY));
            detail_connections_list_->addItem(item);
            if (ds.id == selected_connection_id_) {
                detail_connections_list_->setCurrentItem(item);
                current_set = true;
            }
        }
        if (!current_set && detail_connections_list_->count() > 0)
            detail_connections_list_->setCurrentRow(0);
    }

    update_action_states();
}

void DataSourcesScreen::update_action_states() {
    const bool has_connector  = !selected_connector_id_.isEmpty();
    const bool has_connection = !effective_detail_connection_id().isEmpty();
    if (new_connection_btn_)  new_connection_btn_->setEnabled(has_connector);
    if (edit_connection_btn_) edit_connection_btn_->setEnabled(has_connection);
    if (test_connection_btn_) test_connection_btn_->setEnabled(has_connection);
}

QString DataSourcesScreen::effective_detail_connection_id() const {
    if (!selected_connection_id_.isEmpty()) {
        for (const auto& ds : connections_cache_)
            if (ds.id == selected_connection_id_ && connection_matches_connector(ds, selected_connector_id_))
                return ds.id;
    }
    QString first_match;
    for (const auto& ds : connections_cache_) {
        if (!connection_matches_connector(ds, selected_connector_id_)) continue;
        if (ds.enabled) return ds.id;
        if (first_match.isEmpty()) first_match = ds.id;
    }
    return first_match;
}

QString DataSourcesScreen::preferred_connection_for_connector(const QString& connector_id) const {
    if (connector_id.isEmpty()) return {};
    if (!selected_connection_id_.isEmpty()) {
        for (const auto& ds : connections_cache_)
            if (ds.id == selected_connection_id_ && connection_matches_connector(ds, connector_id))
                return ds.id;
    }
    QString first_match;
    for (const auto& ds : connections_cache_) {
        if (!connection_matches_connector(ds, connector_id)) continue;
        if (ds.enabled) return ds.id;
        if (first_match.isEmpty()) first_match = ds.id;
    }
    return first_match;
}

void DataSourcesScreen::select_connector_by_id(const QString& connector_id) {
    const auto* cfg = find_connector_config(connector_id);
    if (!cfg) return;
    selected_connector_id_ = cfg->id;
    if (!connector_table_) return;
    for (int row = 0; row < connector_table_->rowCount(); ++row) {
        auto* item = connector_table_->item(row, 1);
        if (item && item->data(Qt::UserRole).toString() == selected_connector_id_) {
            connector_table_->selectRow(row);
            break;
        }
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void DataSourcesScreen::on_category_filter(int idx) {
    show_all_categories_ = (idx == 0);
    if (idx > 0) active_category_ = static_cast<Category>(idx - 1);
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
    update_action_states();
}

void DataSourcesScreen::on_search_changed(const QString& /*text*/) {
    build_category_ladder();
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
    update_action_states();
}

void DataSourcesScreen::on_connector_clicked(const QString& connector_id) {
    if (const auto* cfg = find_connector_config(connector_id)) {
        selected_connector_id_ = cfg->id;
        const QString existing_connection = preferred_connection_for_connector(cfg->id);
        show_config_dialog(*cfg, existing_connection);
    }
}

void DataSourcesScreen::on_connection_add() {
    if (selected_connector_id_.isEmpty()) {
        const auto filtered = filtered_connectors();
        if (filtered.isEmpty()) return;
        selected_connector_id_ = filtered.first().id;
    }
    if (const auto* cfg = find_connector_config(selected_connector_id_))
        show_config_dialog(*cfg);
}

void DataSourcesScreen::on_connection_edit(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err()) return;
    const auto ds = result.value();
    if (const auto* cfg = find_connector_config(ds.provider)) {
        selected_connector_id_ = cfg->id;
        selected_connection_id_ = ds.id;
        show_config_dialog(*cfg, ds.id);
    }
}

void DataSourcesScreen::on_connection_delete(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().remove(conn_id);
    if (result.is_err()) {
        LOG_ERROR(TAG, QString("Failed to remove connection %1").arg(conn_id));
        return;
    }
    if (selected_connection_id_ == conn_id) selected_connection_id_.clear();
    refresh_connections();
}

void DataSourcesScreen::on_connection_enabled_changed(const QString& conn_id, bool enabled) {
    const auto result = DataSourceRepository::instance().set_enabled(conn_id, enabled);
    if (result.is_err()) {
        LOG_ERROR(TAG, QString("Failed to update connection state for %1").arg(conn_id));
        return;
    }
    LOG_INFO(TAG, QString("%1 connection %2").arg(enabled ? "Enabled" : "Disabled", conn_id));
    refresh_connections();
}

// ── Known base-URL map for API-key-only providers ─────────────────────────
// Returns a URL that can be hit to verify the endpoint is reachable.
// For authenticated providers we use an endpoint that returns 200/401/403
// (any of those means the server is up and the key was at least transmitted).
static QString provider_probe_url(const QString& provider_id, const QJsonObject& cfg) {
    // Market data — simple ping / time / status endpoints (no side effects)
    if (provider_id == "yahoo-finance")   return "https://query1.finance.yahoo.com/v8/finance/spark?symbols=AAPL&range=1d&interval=1d";
    if (provider_id == "alpha-vantage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=IBM&interval=5min&apikey=%1").arg(key.isEmpty() ? "demo" : key);
    }
    if (provider_id == "finnhub") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://finnhub.io/api/v1/stock/profile2?symbol=AAPL&token=%1").arg(key);
    }
    if (provider_id == "iex-cloud") {
        const QString token = cfg.value("token").toString().trimmed();
        const bool sandbox = cfg.value("sandbox").toBool();
        const QString host = sandbox ? "https://sandbox.iexapis.com" : "https://cloud.iexapis.com";
        return QString("%1/stable/stock/AAPL/quote?token=%2").arg(host, token);
    }
    if (provider_id == "twelve-data") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.twelvedata.com/time_series?symbol=AAPL&interval=1min&outputsize=1&apikey=%1").arg(key);
    }
    if (provider_id == "quandl") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://data.nasdaq.com/api/v3/datasets/WIKI/AAPL.json?rows=1&api_key=%1").arg(key);
    }
    if (provider_id == "binance")        return "https://api.binance.com/api/v3/ping";
    if (provider_id == "coinbase")       return "https://api.coinbase.com/v2/time";
    if (provider_id == "kraken")         return "https://api.kraken.com/0/public/Time";
    if (provider_id == "coinmarketcap") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://pro-api.coinmarketcap.com/v1/cryptocurrency/listings/latest?limit=1&CMC_PRO_API_KEY=%1").arg(key);
    }
    if (provider_id == "coingecko")      return "https://api.coingecko.com/api/v3/ping";
    if (provider_id == "eod-historical") {
        const QString token = cfg.value("apiToken").toString().trimmed();
        return QString("https://eodhd.com/api/eod/AAPL.US?api_token=%1&fmt=json&limit=1").arg(token);
    }
    if (provider_id == "tiingo") {
        const QString token = cfg.value("apiToken").toString().trimmed();
        return QString("https://api.tiingo.com/api/test?token=%1").arg(token);
    }
    if (provider_id == "intrinio") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api-v2.intrinio.com/securities/AAPL?api_key=%1").arg(key);
    }
    if (provider_id == "reuters") {
        const QString appKey = cfg.value("appKey").toString().trimmed();
        return QString("https://api.refinitiv.com/user-framework/machine-id/v1/health?appKey=%1").arg(appKey);
    }
    if (provider_id == "marketstack") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        const bool https = cfg.value("https").toBool();
        const QString scheme = https ? "https" : "http";
        return QString("%1://api.marketstack.com/v1/tickers/AAPL?access_key=%2").arg(scheme, key);
    }
    if (provider_id == "finage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.finage.co.uk/agg/stock/prev-close/AAPL?apikey=%1").arg(key);
    }
    if (provider_id == "tradier") {
        const bool sandbox = cfg.value("sandbox").toBool();
        return sandbox ? "https://sandbox.tradier.com/v1/markets/clock"
                       : "https://api.tradier.com/v1/markets/clock";
    }
    // Search & Analytics
    if (provider_id == "elasticsearch") {
        QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty()) return node.endsWith('/') ? node + "_cluster/health" : node + "/_cluster/health";
    }
    if (provider_id == "opensearch") {
        QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty()) return node.endsWith('/') ? node + "_cluster/health" : node + "/_cluster/health";
    }
    if (provider_id == "solr") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty()) return (host.startsWith("http") ? host : "http://" + host) + "/solr/admin/info/system";
    }
    if (provider_id == "meilisearch") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty()) return (host.startsWith("http") ? host : "http://" + host) + "/health";
    }
    // Time series
    if (provider_id == "influxdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return url + (url.endsWith('/') ? "health" : "/health");
    }
    if (provider_id == "prometheus") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return url + (url.endsWith('/') ? "-/healthy" : "/-/healthy");
    }
    if (provider_id == "victoriametrics") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return url + (url.endsWith('/') ? "health" : "/health");
    }
    if (provider_id == "opentsdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty()) return url + (url.endsWith('/') ? "api/version" : "/api/version");
    }
    // Cloud storage
    if (provider_id == "minio") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty()) return ep.startsWith("http") ? ep + "/minio/health/live" : "http://" + ep + "/minio/health/live";
    }
    if (provider_id == "ibm-cloud-storage") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty()) return ep;  // TCP-probe the endpoint host
    }
    // Search
    if (provider_id == "algolia") {
        const QString appId = cfg.value("appId").toString().trimmed();
        if (!appId.isEmpty())
            return QString("https://%1-dsn.algolia.net/1/isalive").arg(appId);
    }
    // Data warehouses
    if (provider_id == "databricks") {
        const QString host = cfg.value("serverHostname").toString().trimmed();
        if (!host.isEmpty()) return QString("https://%1/api/2.0/clusters/list").arg(host);
    }
    if (provider_id == "bigquery")   return "https://bigquery.googleapis.com/";
    if (provider_id == "redshift") {
        // Redshift is Postgres-compatible — TCP probe handled by host+port fallback
        return {};
    }
    if (provider_id == "synapse") {
        const QString server = cfg.value("server").toString().trimmed();
        if (!server.isEmpty()) return QString("https://%1").arg(server);
    }
    return {};
}

void DataSourcesScreen::on_connection_test(const QString& conn_id) {
    const auto get_result = DataSourceRepository::instance().get(conn_id);
    if (get_result.is_err()) return;

    const auto ds = get_result.value();
    const auto cfg_doc = QJsonDocument::fromJson(ds.config.toUtf8());
    const auto cfg_obj = cfg_doc.object();

    // Check testable flag
    const auto* connector_cfg = find_connector_config(ds.provider);
    if (connector_cfg && !connector_cfg->testable) {
        QDialog result_dlg(this);
        result_dlg.setWindowTitle(QString("Test: %1").arg(ds.display_name));
        result_dlg.resize(420, 160);
        result_dlg.setModal(true);
        result_dlg.setStyleSheet(
            "QDialog{background:#0a0a0a;color:#e5e5e5;}"
            "QLabel{font-size:13px;background:transparent;}"
            "QPushButton{background:#111111;color:#e5e5e5;border:1px solid #1a1a1a;"
            "padding:6px 18px;font-size:12px;font-weight:700;border-radius:4px;}"
            "QPushButton:hover{background:#1a1a1a;}");
        auto* vl = new QVBoxLayout(&result_dlg);
        vl->setContentsMargins(24, 20, 24, 16); vl->setSpacing(10);
        auto* lbl = new QLabel("This connector does not support connectivity testing.");
        lbl->setWordWrap(true);
        lbl->setStyleSheet("color:#808080;font-size:13px;background:transparent;");
        vl->addWidget(lbl); vl->addStretch();
        auto* btn = new QPushButton("Close");
        btn->setCursor(Qt::PointingHandCursor);
        QObject::connect(btn, &QPushButton::clicked, &result_dlg, &QDialog::accept);
        auto* row = new QHBoxLayout; row->addStretch(); row->addWidget(btn);
        vl->addLayout(row);
        result_dlg.exec();
        return;
    }

    QPointer<DataSourcesScreen> self = this;
    const QString display  = ds.display_name;
    const QString provider = ds.provider;

    // Resolve what to test
    // Priority: 1) known probe URL per provider, 2) explicit url/endpoint field, 3) host+port TCP
    const QString probe_url = provider_probe_url(provider, cfg_obj);

    QString explicit_url;
    if (probe_url.isEmpty()) {
        const QStringList url_keys = {"url", "baseUrl", "endpoint", "wsdlUrl", "serviceRoot"};
        for (const auto& key : url_keys) {
            const QString v = cfg_obj.value(key).toString().trimmed();
            if (!v.isEmpty()) { explicit_url = v; break; }
        }
    }

    const QString test_url = probe_url.isEmpty() ? explicit_url : probe_url;

    // Resolve host/port for TCP fallback.
    // Handles both plain "host" fields and compound/URL fields from various connector types.
    QString host = cfg_obj.value("host").toString().trimmed();
    int port     = cfg_obj.value("port").toVariant().toInt();
    if (port <= 0) port = cfg_obj.value("port").toString().trimmed().toInt();

    // Kafka: "brokers" = "host1:port1,host2:port2"
    if (host.isEmpty()) {
        const QString brokers = cfg_obj.value("brokers").toString().trimmed();
        if (!brokers.isEmpty()) {
            const QString first = brokers.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) { host = first.left(idx); port = first.mid(idx + 1).toInt(); }
            else          { host = first; }
        }
    }
    // NATS / Memcached: "servers" or "hosts" = "nats://host:port" or "host:port"
    if (host.isEmpty()) {
        for (const auto& key : {"servers", "hosts"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (val.isEmpty()) continue;
            const QString first = val.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const QUrl u(first);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port();
            } else {
                // plain "host:port" without scheme
                const int idx = first.lastIndexOf(':');
                if (idx > 0) { host = first.left(idx); port = first.mid(idx + 1).toInt(); }
                else          { host = first; }
            }
            if (!host.isEmpty()) break;
        }
    }
    // MongoDB: "connectionString" = "mongodb://host:port"
    if (host.isEmpty()) {
        const QString cs = cfg_obj.value("connectionString").toString().trimmed();
        if (!cs.isEmpty()) {
            const QUrl u(cs);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port(27017);
            }
        }
    }
    // Neo4j: "uri" = "bolt://host:port"
    if (host.isEmpty()) {
        const QString uri = cfg_obj.value("uri").toString().trimmed();
        if (!uri.isEmpty()) {
            const QUrl u(uri);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0) port = u.port(7687);
            }
        }
    }
    // Cassandra / ScyllaDB: "contactPoints" = "host1,host2"
    if (host.isEmpty()) {
        const QString cp = cfg_obj.value("contactPoints").toString().trimmed();
        if (!cp.isEmpty()) {
            const QString first = cp.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) { host = first.left(idx); port = first.mid(idx + 1).toInt(); }
            else          { host = first; }
            if (port <= 0) port = cfg_obj.value("port").toVariant().toInt();
            if (port <= 0) port = 9042;  // Cassandra default
        }
    }
    // HBase: "zkQuorum" = "host:port"
    if (host.isEmpty()) {
        const QString zk = cfg_obj.value("zkQuorum").toString().trimmed();
        if (!zk.isEmpty()) {
            const QString first = zk.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) { host = first.left(idx); port = first.mid(idx + 1).toInt(); }
            else          { host = first; port = 2181; }
        }
    }
    // Databricks / Synapse / others: "serverHostname" or "server"
    if (host.isEmpty()) {
        for (const auto& key : {"serverHostname", "server", "account"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (!val.isEmpty()) { host = val; break; }
        }
    }
    // For URL-based hosts (Elasticsearch node, InfluxDB url, CouchDB url etc.)
    // extract host+port for TCP fallback
    if (host.isEmpty() && !test_url.isEmpty()) {
        const QUrl u(test_url);
        if (u.isValid() && !u.host().isEmpty()) {
            host = u.host();
            if (port <= 0) port = u.port(u.scheme() == "https" ? 443 : 80);
        }
    }

    if (test_url.isEmpty() && (host.isEmpty() || port <= 0)) {
        // Nothing to test — show informational dialog on main thread immediately
        QDialog result_dlg(this);
        result_dlg.setWindowTitle(QString("Test: %1").arg(display));
        result_dlg.resize(420, 160);
        result_dlg.setModal(true);
        result_dlg.setStyleSheet(
            "QDialog{background:#0a0a0a;color:#e5e5e5;}"
            "QLabel{font-size:13px;background:transparent;}"
            "QPushButton{background:#111111;color:#e5e5e5;border:1px solid #1a1a1a;"
            "padding:6px 18px;font-size:12px;font-weight:700;border-radius:4px;}"
            "QPushButton:hover{background:#1a1a1a;}");
        auto* vl = new QVBoxLayout(&result_dlg);
        vl->setContentsMargins(24, 20, 24, 16); vl->setSpacing(10);
        auto* lbl = new QLabel("No testable endpoint found in the saved configuration.\n"
                               "Ensure required fields (URL, host, or API key) are filled in.");
        lbl->setWordWrap(true);
        lbl->setStyleSheet("color:#808080;font-size:13px;background:transparent;");
        vl->addWidget(lbl); vl->addStretch();
        auto* btn = new QPushButton("Close");
        btn->setCursor(Qt::PointingHandCursor);
        QObject::connect(btn, &QPushButton::clicked, &result_dlg, &QDialog::accept);
        auto* row = new QHBoxLayout; row->addStretch(); row->addWidget(btn);
        vl->addLayout(row);
        result_dlg.exec();
        return;
    }

    // Run connectivity test on a worker thread.
    // IMPORTANT: QNetworkAccessManager must NOT be used on a non-main thread without its own
    // event loop. We use only QTcpSocket (blocking, safe on worker threads) here. For HTTP
    // we open a raw TCP connection to the resolved host:port — this validates network reachability
    // without touching QNetworkAccessManager off-thread.
    const QString captured_test_url = test_url;
    const QString captured_host     = host;
    const int     captured_port     = port;

    const auto test_future = QtConcurrent::run([self, conn_id, display, captured_test_url, captured_host, captured_port]() {
        bool    success = false;
        QString message = "No testable endpoint found";

        auto tcp_probe = [](const QString& h, int p, int timeout_ms) -> std::pair<bool, QString> {
            QTcpSocket socket;
            socket.connectToHost(h, static_cast<quint16>(p));
            if (socket.waitForConnected(timeout_ms)) {
                socket.disconnectFromHost();
                return {true, QString("TCP connected to %1:%2").arg(h).arg(p)};
            }
            return {false, QString("Cannot connect to %1:%2 — %3").arg(h).arg(p).arg(socket.errorString())};
        };

        if (!captured_test_url.isEmpty()) {
            // Derive host/port from the URL and do a TCP probe — avoids QNetworkAccessManager off-thread
            const QUrl url(captured_test_url);
            if (!url.isValid() || url.host().isEmpty()) {
                message = QString("Invalid URL: %1").arg(captured_test_url);
            } else {
                const QString url_host = url.host();
                const int url_port = url.port(url.scheme() == "https" ? 443 : 80);
                auto [ok, msg] = tcp_probe(url_host, url_port, 5000);
                success = ok;
                // Give a more meaningful message: distinguish reachable-but-auth-required from unreachable
                message = ok ? QString("Endpoint reachable: %1").arg(captured_test_url) : msg;
            }
        } else if (!captured_host.isEmpty() && captured_port > 0) {
            auto [ok, msg] = tcp_probe(captured_host, captured_port, 3000);
            success = ok;
            message = msg;
        }

        QMetaObject::invokeMethod(qApp, [self, conn_id, display, success, message]() {
            if (!self) return;
            LOG_INFO(TAG, QString("Test %1: %2 — %3").arg(display, success ? "OK" : "FAIL", message));
            // Update live cache and refresh status cell in-place
            self->live_status_cache_[conn_id] = {success, message};
            self->update_connection_status_cell(conn_id, success, message);
            self->update_detail_panel();

            QDialog result_dlg(self);
            result_dlg.setWindowTitle(QString("Test: %1").arg(display));
            result_dlg.resize(440, 190);
            result_dlg.setModal(true);
            result_dlg.setStyleSheet(
                "QDialog{background:#0a0a0a;color:#e5e5e5;}"
                "QLabel{font-size:13px;background:transparent;}"
                "QPushButton{background:#111111;color:#e5e5e5;border:1px solid #1a1a1a;"
                "padding:6px 18px;font-size:12px;font-weight:700;border-radius:4px;}"
                "QPushButton:hover{background:#1a1a1a;}");

            auto* vl = new QVBoxLayout(&result_dlg);
            vl->setContentsMargins(24, 20, 24, 16);
            vl->setSpacing(10);

            auto* status_lbl = new QLabel(success ? "Connection successful" : "Connection failed");
            status_lbl->setStyleSheet(
                QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
                    .arg(success ? "#16a34a" : "#dc2626"));
            vl->addWidget(status_lbl);

            auto* msg_lbl = new QLabel(message);
            msg_lbl->setWordWrap(true);
            msg_lbl->setStyleSheet("color:#808080;font-size:12px;background:transparent;");
            vl->addWidget(msg_lbl);

            if (success) {
                auto* note = new QLabel("Note: TCP reachability confirmed. API key validity is not verified here.");
                note->setWordWrap(true);
                note->setStyleSheet("color:#525252;font-size:11px;font-style:italic;background:transparent;");
                vl->addWidget(note);
            }
            vl->addStretch();

            auto* close_btn = new QPushButton("Close");
            close_btn->setCursor(Qt::PointingHandCursor);
            QObject::connect(close_btn, &QPushButton::clicked, &result_dlg, &QDialog::accept);
            auto* btn_row = new QHBoxLayout;
            btn_row->addStretch();
            btn_row->addWidget(close_btn);
            vl->addLayout(btn_row);

            result_dlg.exec();
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(test_future);
}

void DataSourcesScreen::on_connector_selection_changed() {
    if (!connector_table_) return;
    const auto selected = connector_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connector_id_.clear();
    } else {
        const int row = connector_table_->currentRow();
        auto* item = connector_table_->item(row, 1);
        if (item) selected_connector_id_ = item->data(Qt::UserRole).toString();
    }
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_connection_selection_changed() {
    if (!connections_table_) return;
    const auto selected = connections_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connection_id_.clear();
    } else {
        const int row = connections_table_->currentRow();
        auto* item = connections_table_->item(row, 1);
        if (item) selected_connection_id_ = item->data(Qt::UserRole).toString();
    }
    update_action_states();
}

void DataSourcesScreen::on_category_selection_changed(int row) {
    on_category_filter(row);
}

void DataSourcesScreen::on_provider_ladder_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    const QString connector_id = item->data(Qt::UserRole).toString();
    if (connector_id.isEmpty()) return;
    select_connector_by_id(connector_id);
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_detail_connection_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    const QString conn_id = item->data(Qt::UserRole).toString();
    if (conn_id.isEmpty()) return;
    selected_connection_id_ = conn_id;
    update_action_states();
}

// ── filtered_connection_rows — applies conn_search + stat_filter ──────────────

QVector<DataSource> DataSourcesScreen::filtered_connection_rows() const {
    const QString filter = conn_search_text_.trimmed().toLower();
    QVector<DataSource> result;
    for (const auto& ds : connections_cache_) {
        // stat_filter: 1=configured(any), 2=active, 3=auth-required connector
        if (stat_filter_ == 2 && !ds.enabled) continue;
        if (stat_filter_ == 3) {
            const auto* cfg = find_connector_config(ds.provider);
            if (!cfg || !cfg->requires_auth) continue;
        }
        if (!filter.isEmpty()) {
            const QString hay = QString("%1 %2 %3 %4")
                .arg(ds.display_name, ds.provider, ds.category, ds.tags).toLower();
            if (!hay.contains(filter)) continue;
        }
        result.append(ds);
    }
    return result;
}

// ── update_connection_status_cell — update Live column in-place ───────────────

void DataSourcesScreen::update_connection_status_cell(const QString& conn_id, bool ok, const QString& msg) {
    if (!connections_table_) return;
    for (int row = 0; row < connections_table_->rowCount(); ++row) {
        auto* name_item = connections_table_->item(row, 1);
        if (!name_item || name_item->data(Qt::UserRole).toString() != conn_id) continue;
        auto* lbl = qobject_cast<QLabel*>(connections_table_->cellWidget(row, 5));
        if (lbl) {
            lbl->setText(ok ? "OK" : "ERR");
            lbl->setStyleSheet(
                QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
                    .arg(ok ? col::POSITIVE : col::NEGATIVE));
            lbl->setToolTip(msg);
        }
        break;
    }
}

// ── on_view_mode_toggle ───────────────────────────────────────────────────────

void DataSourcesScreen::on_view_mode_toggle() {
    view_mode_ = (view_mode_ == ViewMode::Gallery) ? ViewMode::Connections : ViewMode::Gallery;
    const bool gallery = (view_mode_ == ViewMode::Gallery);

    if (view_mode_btn_) {
        view_mode_btn_->setText(gallery ? "Gallery" : "Connections");
        view_mode_btn_->setProperty("active", gallery);
        view_mode_btn_->style()->unpolish(view_mode_btn_);
        view_mode_btn_->style()->polish(view_mode_btn_);
    }

    auto* main_splitter   = findChild<QSplitter*>("dsMainSplitter");
    auto* work_splitter   = findChild<QSplitter*>("dsWorkspaceSplitter");

    if (gallery) {
        // Gallery: show full connector browser + inspector, hide connections table
        if (work_splitter) work_splitter->setVisible(true);
        if (main_splitter) {
            // Restore normal split — workspace takes all space, connections panel hidden
            main_splitter->setSizes({10000, 0});
        }
    } else {
        // Connections: hide the top browser entirely, give full height to connections table
        if (work_splitter) work_splitter->setVisible(false);
        if (main_splitter) {
            main_splitter->setSizes({0, 10000});
        }
    }
}

// ── on_connections_search_changed ────────────────────────────────────────────

void DataSourcesScreen::on_connections_search_changed(const QString& text) {
    conn_search_text_ = text;
    build_connections_table();
    update_stats_strip();
}

// ── on_stat_box_clicked ───────────────────────────────────────────────────────

void DataSourcesScreen::on_stat_box_clicked(int stat_index) {
    apply_stat_filter(stat_index);
}

void DataSourcesScreen::apply_stat_filter(int stat_index) {
    // Toggle: clicking the active filter clears it
    stat_filter_ = (stat_filter_ == stat_index) ? -1 : stat_index;
    build_connector_table();
    build_connections_table();
    update_stats_strip();
}

// ── on_connection_duplicate ───────────────────────────────────────────────────

void DataSourcesScreen::on_connection_duplicate(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err()) return;
    const auto ds = result.value();
    if (const auto* cfg = find_connector_config(ds.provider)) {
        selected_connector_id_ = cfg->id;
        show_config_dialog(*cfg, ds.id, /*duplicate=*/true);
    }
}

// ── on_bulk_enable_all ────────────────────────────────────────────────────────

void DataSourcesScreen::on_bulk_enable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (!ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, true);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-enabled %1 connections").arg(rows.size()));
}

// ── on_bulk_disable_all ───────────────────────────────────────────────────────

void DataSourcesScreen::on_bulk_disable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, false);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-disabled %1 connections").arg(rows.size()));
}

// ── on_bulk_delete_selected ───────────────────────────────────────────────────

void DataSourcesScreen::on_bulk_delete_selected() {
    if (!connections_table_) return;

    // Collect selected connection IDs
    QSet<QString> selected_ids;
    const auto selected_items = connections_table_->selectedItems();
    for (auto* item : selected_items) {
        if (item->column() == 1)
            selected_ids.insert(item->data(Qt::UserRole).toString());
    }
    if (selected_ids.isEmpty()) return;

    // Confirm
    QMessageBox confirm(this);
    confirm.setWindowTitle("Delete Connections");
    confirm.setText(QString("Delete %1 selected connection(s)?").arg(selected_ids.size()));
    confirm.setInformativeText("This cannot be undone.");
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.setStyleSheet(
        "QMessageBox { background:#0a0a0a; color:#e5e5e5; }"
        "QLabel { color:#e5e5e5; font-size:13px; background:transparent; }"
        "QPushButton { background:#111111; color:#e5e5e5; border:1px solid #1a1a1a;"
        " padding:6px 18px; font-size:12px; font-weight:700; border-radius:4px; }"
        "QPushButton:hover { background:#1a1a1a; }");
    if (confirm.exec() != QMessageBox::Yes) return;

    for (const auto& id : selected_ids) {
        DataSourceRepository::instance().remove(id);
        live_status_cache_.remove(id);
    }
    if (selected_ids.contains(selected_connection_id_)) selected_connection_id_.clear();
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-deleted %1 connections").arg(selected_ids.size()));
}

// ── on_export_connections ─────────────────────────────────────────────────────

void DataSourcesScreen::on_export_connections() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Connections", "fincept_connections.json",
        "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    const auto all = DataSourceRepository::instance().list_all();
    if (all.is_err()) return;

    QJsonArray arr;
    for (const auto& ds : all.value()) {
        QJsonObject obj;
        obj["id"]           = ds.id;
        obj["alias"]        = ds.alias;
        obj["display_name"] = ds.display_name;
        obj["description"]  = ds.description;
        obj["type"]         = ds.type;
        obj["provider"]     = ds.provider;
        obj["category"]     = ds.category;
        obj["config"]       = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        obj["enabled"]      = ds.enabled;
        obj["tags"]         = ds.tags;
        obj["created_at"]   = ds.created_at;
        obj["updated_at"]   = ds.updated_at;
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Export failed: cannot write to %1").arg(path));
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    LOG_INFO(TAG, QString("Exported %1 connections to %2").arg(arr.size()).arg(path));
}

// ── on_import_connections ─────────────────────────────────────────────────────

void DataSourcesScreen::on_import_connections() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Import Connections", "",
        "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(TAG, QString("Import failed: cannot open %1").arg(path));
        return;
    }
    const QByteArray data = file.readAll();
    QJsonParseError parse_err;
    const auto doc = QJsonDocument::fromJson(data, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR(TAG, QString("Import failed: invalid JSON — %1").arg(parse_err.errorString()));
        return;
    }

    int imported = 0, skipped = 0;
    for (const auto& val : doc.array()) {
        const auto obj = val.toObject();
        const QString provider = obj["provider"].toString();
        if (provider.isEmpty()) { ++skipped; continue; }

        DataSource ds;
        // Assign a fresh UUID so imports never collide with existing IDs
        ds.id           = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias        = obj["alias"].toString();
        ds.display_name = obj["display_name"].toString();
        ds.description  = obj["description"].toString();
        ds.type         = obj["type"].toString();
        ds.provider     = provider;
        ds.category     = obj["category"].toString();
        ds.config       = QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact);
        ds.enabled      = obj["enabled"].toBool(false);
        ds.tags         = obj["tags"].toString();

        if (ds.display_name.isEmpty()) ds.display_name = provider;
        if (ds.alias.isEmpty()) ds.alias = provider + "_" + ds.id.left(8);

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_ok()) ++imported; else ++skipped;
    }

    refresh_connections();
    LOG_INFO(TAG, QString("Import complete: %1 imported, %2 skipped").arg(imported).arg(skipped));
}

// ── on_poll_timer — background status poll for enabled connections ─────────────

void DataSourcesScreen::on_poll_timer() {
    // Poll only enabled connections that have a testable probe URL or host+port
    for (const auto& ds : connections_cache_) {
        if (!ds.enabled) continue;

        const auto* connector_cfg = find_connector_config(ds.provider);
        if (!connector_cfg || !connector_cfg->testable) continue;

        // Run the same probe logic as on_connection_test but silently (no dialog)
        const auto cfg_obj = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        const QString probe_url = provider_probe_url(ds.provider, cfg_obj);

        QString host = cfg_obj.value("host").toString().trimmed();
        int port = cfg_obj.value("port").toVariant().toInt();

        if (host.isEmpty() && !probe_url.isEmpty()) {
            const QUrl u(probe_url);
            host = u.host();
            if (port <= 0) port = u.port(u.scheme() == "https" ? 443 : 80);
        }
        if (host.isEmpty() || port <= 0) continue;

        QPointer<DataSourcesScreen> self = this;
        const QString conn_id    = ds.id;
        const QString cap_host   = host;
        const int     cap_port   = port;
        const QString cap_probe  = probe_url;

        QtConcurrent::run([self, conn_id, cap_host, cap_port, cap_probe]() {
            QTcpSocket socket;
            socket.connectToHost(cap_host, static_cast<quint16>(cap_port));
            const bool ok = socket.waitForConnected(3000);
            const QString msg = ok
                ? QString("Reachable: %1").arg(cap_probe.isEmpty() ? cap_host : cap_probe)
                : QString("Unreachable: %1").arg(socket.errorString());
            if (ok) socket.disconnectFromHost();

            QMetaObject::invokeMethod(qApp, [self, conn_id, ok, msg]() {
                if (!self) return;
                self->live_status_cache_[conn_id] = {ok, msg};
                self->update_connection_status_cell(conn_id, ok, msg);
                self->update_detail_panel();
            }, Qt::QueuedConnection);
        });
    }
}

// ── refresh_connections ───────────────────────────────────────────────────────

void DataSourcesScreen::refresh_connections() {
    const auto result = DataSourceRepository::instance().list_all();
    connections_cache_ = result.is_ok() ? result.value() : QVector<DataSource>{};
    build_category_ladder();
    build_connector_table();
    build_connections_table();
    update_provider_ladder();
    update_stats_strip();
}

void DataSourcesScreen::update_clock() {
    if (clock_label_)
        clock_label_->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

} // namespace fincept::screens::datasources
