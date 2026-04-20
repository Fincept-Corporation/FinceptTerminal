// DataSourcesScreen.cpp — Fincept Data Sources, Obsidian design system v2.
#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidgetItem>
#include <QMap>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
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
    "All Connectors", "Databases",   "APIs",   "Files",      "Streaming",        "Cloud",
    "Time Series",    "Market Data", "Search", "Warehouses", "Alternative Data", "Open Banking"};

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
    if (filter.isEmpty())
        return true;
    const QString haystack =
        QString("%1 %2 %3 %4 %5").arg(cfg.name, cfg.id, cfg.type, cfg.description, category_label(cfg.category));
    return haystack.toLower().contains(filter);
}

QString connector_code(const ConnectorConfig& cfg) {
    const QString icon = cfg.icon.trimmed().toUpper();
    if (!icon.isEmpty())
        return icon.left(2);
    return cfg.name.left(2).toUpper();
}

QString connector_transport(const ConnectorConfig& cfg) {
    switch (cfg.category) {
        case Category::Database:
            return "SQL";
        case Category::Api:
            return "REST";
        case Category::File:
            return "FILE";
        case Category::Streaming:
            return "WS";
        case Category::Cloud:
            return "CLOUD";
        case Category::TimeSeries:
            return "TSDB";
        case Category::MarketData:
            return "MKT";
        case Category::SearchAnalytics:
            return "SRCH";
        case Category::DataWarehouse:
            return "DW";
        case Category::AlternativeData:
            return "ALT";
        case Category::OpenBanking:
            return "OB";
    }
    return "API";
}

QString persistence_type(const ConnectorConfig& cfg) {
    return cfg.category == Category::Streaming ? "websocket" : "rest_api";
}

QString field_type_label(FieldType type) {
    switch (type) {
        case FieldType::Text:
            return "Text";
        case FieldType::Password:
            return "Secret";
        case FieldType::Number:
            return "Number";
        case FieldType::Url:
            return "URL";
        case FieldType::Select:
            return "Select";
        case FieldType::Textarea:
            return "Textarea";
        case FieldType::Checkbox:
            return "Bool";
        case FieldType::File:
            return "File";
    }
    return "Text";
}

int total_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (connection_matches_connector(ds, connector_id))
            ++count;
    return count;
}

int enabled_connections_for_provider(const QVector<DataSource>& rows, const QString& connector_id) {
    int count = 0;
    for (const auto& ds : rows)
        if (ds.enabled && connection_matches_connector(ds, connector_id))
            ++count;
    return count;
}

QTableWidgetItem* make_item(const QString& text, const QColor& color = QColor(col::TEXT_PRIMARY.get()),
                            Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
    auto* item = new QTableWidgetItem(text);
    item->setForeground(color);
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

// ── Shared stylesheet for all tab views ──────────────────────────────────────
// Uses arg() substitution — call with .arg(col::BG_BASE) etc.
static const QString kScreenStylesheet = R"(
    * { font-family: 'Consolas','Courier New',monospace; }

    /* ── Root ── */
    #dsScreen { background: %1; }

    /* ── Screen header ── */
    #dsScreenHeader {
        background: %1;
        border-bottom: 1px solid %4;
    }
    #dsScreenTitle {
        color: %5; font-size: 13px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsScreenSubtitle {
        color: %8; font-size: 11px; background: transparent;
    }
    #dsClock {
        color: %5; font-size: 11px; font-weight: 700;
        background: transparent; letter-spacing: 0.5px;
    }
    #dsSearch {
        background: %2; border: 1px solid %4; color: %10;
        padding: 0 10px; font-size: 12px;
    }
    #dsSearch:focus { border-color: %7; }
    #dsSearch::selection { background: %5; color: %1; }

    /* ── Tab bar ── */
    #dsTabBar { background: %2; border-bottom: 1px solid %4; }
    #dsTab {
        background: transparent; color: %6; border: none;
        border-bottom: 2px solid transparent;
        padding: 0 18px; font-size: 12px; font-weight: 700;
        letter-spacing: 0.5px;
    }
    #dsTab:hover { color: %10; background: %9; }
    #dsTabActive {
        background: transparent; color: %5; border: none;
        border-bottom: 2px solid %5;
        padding: 0 18px; font-size: 12px; font-weight: 700;
        letter-spacing: 0.5px;
    }

    /* ── Stat boxes ── */
    #dsStatRow { background: %3; border-bottom: 1px solid %4; }
    #dsStatBox {
        background: %2; border: 1px solid %4;
        border-right: none;
    }
    #dsStatBox:last-child { border-right: 1px solid %4; }
    #dsStatValue {
        color: %10; font-size: 22px; font-weight: 700;
        background: transparent;
    }
    #dsStatLabel {
        color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; background: transparent;
    }

    /* ── Left sidebar ── */
    #dsSidebar { background: %2; border-right: 1px solid %4; }
    #dsSidebarHeader {
        background: %3; border-bottom: 1px solid %4;
    }
    #dsSidebarTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsCategoryList { background: transparent; border: none; outline: none; }
    #dsCategoryList::item {
        padding: 0 14px; height: 30px; color: %6;
        font-size: 12px; border-bottom: 1px solid %4;
    }
    #dsCategoryList::item:selected {
        background: rgba(217,119,6,0.1); color: %5;
        border-left: 2px solid %5;
    }
    #dsCategoryList::item:hover:!selected { background: %9; color: %10; }

    /* ── Provider ladder ── */
    #dsProviderHeader { background: %3; border-bottom: 1px solid %4; border-top: 1px solid %4; }
    #dsProviderLadder { background: transparent; border: none; outline: none; }
    #dsProviderLadder::item {
        padding: 0 14px; height: 26px; color: %6;
        font-size: 11px; border-bottom: 1px solid %4;
    }
    #dsProviderLadder::item:selected { background: %9; color: %10; }
    #dsProviderLadder::item:hover:!selected { background: %9; }

    /* ── Connector table ── */
    #dsConnPanel { background: %1; border: none; }
    #dsConnHeader { background: %3; border-bottom: 1px solid %4; }
    #dsConnPanelTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsConnCount {
        color: %8; font-size: 11px; background: transparent;
    }
    #dsConnectorTable {
        background: %1; border: none; color: %10;
        font-size: 12px; gridline-color: transparent;
        selection-background-color: rgba(217,119,6,0.1);
        selection-color: %10;
    }
    #dsConnectorTable::item {
        padding: 0 8px; border-bottom: 1px solid %4; height: 26px;
    }
    #dsConnectorTable::item:hover:!selected { background: %9; }
    #dsConnectorTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; border: none;
        border-bottom: 1px solid %4; border-right: 1px solid %4;
        padding: 0 8px; height: 26px;
    }
    #dsConnectorTable QHeaderView::section:last { border-right: none; }

    /* ── Detail panel ── */
    #dsDetail { background: %2; border-left: 1px solid %4; }
    #dsDetailHeader { background: %3; border-bottom: 1px solid %4; }
    #dsDetailTitle {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }
    #dsDetailSymbol {
        min-width: 38px; max-width: 38px;
        min-height: 38px; max-height: 38px;
        font-size: 13px; font-weight: 700;
        color: %5; background: %1; border: 1px solid %5;
    }
    #dsDetailName {
        color: %10; font-size: 13px; font-weight: 700;
        background: transparent;
    }
    #dsDetailDesc {
        color: %6; font-size: 11px; background: transparent;
    }
    #dsDetailEmpty {
        color: %8; font-size: 12px; background: transparent;
    }
    #dsInfoRow { background: transparent; border-bottom: 1px solid %4; }
    #dsInfoLabel {
        color: %8; font-size: 11px; font-weight: 700;
        letter-spacing: 0.5px; background: transparent;
    }
    #dsInfoValue {
        color: %10; font-size: 11px; font-weight: 700;
        background: transparent;
    }
    #dsSectionSep {
        color: %5; font-size: 10px; font-weight: 700;
        letter-spacing: 1px; background: transparent;
    }

    /* ── Field table ── */
    #dsFieldTable {
        background: %2; border: none; color: %10;
        font-size: 11px; gridline-color: transparent;
        selection-background-color: %9;
    }
    #dsFieldTable::item {
        padding: 0 6px; border-bottom: 1px solid %4; height: 24px;
    }
    #dsFieldTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        border: none; border-bottom: 1px solid %4;
        border-right: 1px solid %4; padding: 0 6px; height: 24px;
    }

    /* ── Detail connection list ── */
    #dsDetailConnList { background: transparent; border: none; outline: none; }
    #dsDetailConnList::item {
        padding: 0 10px; height: 26px; color: %6;
        font-size: 11px; border-bottom: 1px solid %4;
    }
    #dsDetailConnList::item:selected { background: %9; color: %10; }
    #dsDetailConnList::item:hover:!selected { background: %9; }

    /* ── Connections panel ── */
    #dsConnsPanel { background: %1; border: none; }
    #dsConnsHeader { background: %3; border-bottom: 1px solid %4; }
    #dsConnsToolbar { background: %2; border-bottom: 1px solid %4; }
    #dsConnectionsTable {
        background: %1; border: none; color: %10;
        font-size: 12px; gridline-color: transparent;
        selection-background-color: rgba(217,119,6,0.1);
        selection-color: %10;
    }
    #dsConnectionsTable::item {
        padding: 0 8px; border-bottom: 1px solid %4; height: 26px;
    }
    #dsConnectionsTable::item:hover:!selected { background: %9; }
    #dsConnectionsTable QHeaderView::section {
        background: %2; color: %8; font-size: 10px; font-weight: 700;
        letter-spacing: 0.5px; border: none;
        border-bottom: 1px solid %4; border-right: 1px solid %4;
        padding: 0 8px; height: 26px;
    }

    /* ── Buttons ── */
    #dsBtn {
        background: %2; color: %6; border: 1px solid %4;
        padding: 0 10px; font-size: 11px; font-weight: 700;
        letter-spacing: 0.5px;
    }
    #dsBtn:hover:enabled { background: %9; color: %10; border-color: %7; }
    #dsBtn:disabled { color: %8; opacity: 0.4; }
    #dsBtnAccent {
        background: rgba(217,119,6,0.08); color: %5;
        border: 1px solid %12; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnAccent:hover:enabled { background: %5; color: %1; }
    #dsBtnAccent:disabled { opacity: 0.4; }
    #dsBtnDanger {
        background: rgba(220,38,38,0.08); color: %14;
        border: 1px solid %15; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnDanger:hover:enabled { background: rgba(220,38,38,0.18); border-color: %14; }
    #dsBtnDanger:disabled { opacity: 0.4; }
    #dsBtnGreen {
        background: rgba(22,163,74,0.08); color: %13;
        border: 1px solid %16; padding: 0 10px;
        font-size: 11px; font-weight: 700; letter-spacing: 0.5px;
    }
    #dsBtnGreen:hover:enabled { background: rgba(22,163,74,0.18); border-color: %13; }
    #dsBtnGreen:disabled { opacity: 0.4; }
    #dsSearchConn {
        background: %2; border: 1px solid %4; color: %10;
        padding: 0 8px; font-size: 11px;
    }
    #dsSearchConn:focus { border-color: %7; }
    #dsEnableToggle { color: %10; background: transparent; }
    #dsEnableToggle::indicator {
        width: 13px; height: 13px;
        border: 1px solid %7; background: %2;
    }
    #dsEnableToggle::indicator:checked { background: %13; border-color: %13; }
    #dsStatusDot { font-size: 11px; font-weight: 700; background: transparent; }

    /* ── Splitters ── */
    #dsBrowseSplitter::handle, #dsConnsSplitter::handle {
        background: %4; width: 1px; height: 1px;
    }

    /* ── Scrollbars ── */
    QScrollBar:vertical { background: transparent; width: 5px; margin: 0; }
    QScrollBar::handle:vertical { background: %7; min-height: 20px; }
    QScrollBar::handle:vertical:hover { background: %11; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QScrollBar:horizontal { background: transparent; height: 5px; margin: 0; }
    QScrollBar::handle:horizontal { background: %7; min-width: 20px; }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
)";

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Constructor & lifecycle
// ─────────────────────────────────────────────────────────────────────────────

DataSourcesScreen::DataSourcesScreen(QWidget* parent) : QWidget(parent) {
    register_all_connectors();
    setup_ui();
    refresh_connections();
    LOG_INFO(TAG, QString("Loaded %1 connector definitions").arg(ConnectorRegistry::instance().count()));
}

void DataSourcesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (clock_timer_) {
        update_clock();
        clock_timer_->start();
    }
    if (poll_timer_)
        poll_timer_->start();
    refresh_connections();
}

void DataSourcesScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (clock_timer_)
        clock_timer_->stop();
    if (poll_timer_)
        poll_timer_->stop();
}

void DataSourcesScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
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

// ─────────────────────────────────────────────────────────────────────────────
// setup_ui — assembles the complete screen
// ─────────────────────────────────────────────────────────────────────────────

void DataSourcesScreen::setup_ui() {
    setObjectName("dsScreen");

    // Apply stylesheet
    const QString style = kScreenStylesheet
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
// Config dialog
// ─────────────────────────────────────────────────────────────────────────────

void DataSourcesScreen::show_config_dialog(const ConnectorConfig& config, const QString& edit_id, bool duplicate) {
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
    dlg.setWindowTitle(editing     ? QString("Edit — %1").arg(config.name)
                       : duplicate ? QString("Clone — %1").arg(config.name)
                                   : QString("Configure — %1").arg(config.name));
    dlg.resize(560, 620);
    dlg.setModal(true);
    dlg.setStyleSheet(
        QString("QDialog { background:%1; color:%2; }"
                "QLabel { color:%3; font-size:12px; background:transparent; font-family:'Consolas','Courier "
                "New',monospace; }"
                "QLineEdit, QTextEdit, QComboBox { background:%4; border:1px solid %5; color:%2;"
                "  padding:6px 10px; font-size:13px; font-family:'Consolas','Courier New',monospace; }"
                "QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border-color:%6; }"
                "QCheckBox { color:%2; font-size:13px; font-family:'Consolas','Courier New',monospace; }"
                "QPushButton { padding:7px 18px; font-size:12px; font-weight:700;"
                "  font-family:'Consolas','Courier New',monospace; }")
            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::TEXT_SECONDARY(), col::BG_BASE(), col::BORDER_DIM(), col::AMBER()));

    auto* root_vl = new QVBoxLayout(&dlg);
    root_vl->setContentsMargins(0, 0, 0, 0);
    root_vl->setSpacing(0);

    // Dialog header
    auto* dlg_hdr = new QWidget(this);
    dlg_hdr->setFixedHeight(56);
    dlg_hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* dlg_hdr_hl = new QHBoxLayout(dlg_hdr);
    dlg_hdr_hl->setContentsMargins(16, 0, 16, 0);
    dlg_hdr_hl->setSpacing(12);

    auto* code_lbl = new QLabel(connector_code(config));
    code_lbl->setAlignment(Qt::AlignCenter);
    code_lbl->setFixedSize(36, 36);
    code_lbl->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;font-size:14px;"
                                    "font-weight:700;")
                                .arg(config.color, col::TEXT_PRIMARY(), col::BORDER_DIM()));
    dlg_hdr_hl->addWidget(code_lbl);

    auto* title_vl = new QVBoxLayout;
    title_vl->setContentsMargins(0, 0, 0, 0);
    title_vl->setSpacing(2);

    auto* dlg_title = new QLabel(editing     ? QString("Edit  %1").arg(config.name)
                                 : duplicate ? QString("Clone  %1").arg(config.name)
                                             : config.name);
    dlg_title->setStyleSheet(
        QString("color:%1;font-size:14px;font-weight:700;background:transparent;").arg(col::AMBER()));
    title_vl->addWidget(dlg_title);

    auto* dlg_sub = new QLabel(config.description);
    dlg_sub->setWordWrap(true);
    dlg_sub->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;").arg(col::TEXT_SECONDARY()));
    title_vl->addWidget(dlg_sub);

    dlg_hdr_hl->addLayout(title_vl, 1);
    root_vl->addWidget(dlg_hdr);

    // Scrollable form body
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background:%1; border:none; }").arg(col::BG_SURFACE()));

    auto* body = new QWidget(this);
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
    name_edit->setText(
        existing_loaded ? (duplicate ? QString("Copy of %1").arg(existing.display_name) : existing.display_name) : "");
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
            const bool value = existing_cfg.contains(field.name) ? existing_cfg.value(field.name).toBool()
                                                                 : (field.default_value == "true");
            check->setChecked(value);
            input = check;
        } else if (field.type == FieldType::Select) {
            auto* combo = new QComboBox;
            for (const auto& option : field.options)
                combo->addItem(option.label, option.value);
            const QString current =
                existing_cfg.contains(field.name) ? existing_cfg.value(field.name).toString() : field.default_value;
            const int index = combo->findData(current);
            if (index >= 0)
                combo->setCurrentIndex(index);
            input = combo;
        } else if (field.type == FieldType::Textarea) {
            auto* edit = new QTextEdit;
            edit->setMaximumHeight(90);
            edit->setPlaceholderText(field.placeholder);
            edit->setPlainText(existing_cfg.contains(field.name) ? existing_cfg.value(field.name).toString()
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

    // Tags field
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
        QString("color:%1;font-size:11px;font-style:italic;background:transparent;").arg(col::TEXT_TERTIARY()));
    body_vl->addWidget(note);
    body_vl->addStretch();

    scroll->setWidget(body);
    root_vl->addWidget(scroll, 1);

    // Dialog footer
    auto* footer = new QWidget(this);
    footer->setFixedHeight(54);
    footer->setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(col::BG_RAISED(), col::BORDER_DIM()));
    auto* footer_hl = new QHBoxLayout(footer);
    footer_hl->setContentsMargins(16, 0, 16, 0);
    footer_hl->setSpacing(8);

    auto* status = new QLabel;
    status->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY()));
    footer_hl->addWidget(status, 1);

    auto* cancel = new QPushButton("Cancel");
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;}"
                                  "QPushButton:hover{background:%3;color:%4;}")
                              .arg(col::BG_BASE(), col::TEXT_SECONDARY(), col::BORDER_MED(), col::TEXT_PRIMARY()));
    footer_hl->addWidget(cancel);

    auto* save = new QPushButton(editing ? "Update Connection" : "Save Connection");
    save->setCursor(Qt::PointingHandCursor);
    save->setDefault(true);
    save->setAutoDefault(true);
    save->setStyleSheet(QString("QPushButton{background:rgba(217,119,6,0.12);color:%1;border:1px solid %2;}"
                                "QPushButton:hover{background:%1;color:%3;}")
                            .arg(col::AMBER(), col::AMBER_DIM(), col::BG_BASE()));
    footer_hl->addWidget(save);

    root_vl->addWidget(footer);

    name_edit->setFocus();

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dlg, [&, existing_loaded]() {
        QJsonObject cfg_json;

        for (const auto& field : config.fields) {
            QWidget* widget = field_widgets.value(field.name, nullptr);
            QString text_value;

            if (auto* line_edit = qobject_cast<QLineEdit*>(widget)) {
                text_value = line_edit->text().trimmed();
                if (field.type == FieldType::Number)
                    cfg_json[field.name] = text_value.toInt();
                else
                    cfg_json[field.name] = text_value;
            } else if (auto* text_edit = qobject_cast<QTextEdit*>(widget)) {
                text_value = text_edit->toPlainText().trimmed();
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
                    QString("color:%1;font-size:12px;font-weight:700;background:transparent;").arg(col::NEGATIVE()));
                return;
            }
        }

        DataSource ds = (existing_loaded && !duplicate) ? existing : DataSource{};
        ds.id = (existing_loaded && !duplicate) ? existing.id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias = (existing_loaded && !duplicate) && !existing.alias.isEmpty() ? existing.alias
                                                                                : (config.id + "_" + ds.id.left(8));
        ds.display_name = name_edit->text().trimmed().isEmpty() ? config.name : name_edit->text().trimmed();
        ds.description = config.description;
        ds.type = persistence_type(config);
        ds.provider = config.id;
        ds.category = category_str(config.category);
        ds.config = QJsonDocument(cfg_json).toJson(QJsonDocument::Compact);
        ds.enabled = enabled_check->isChecked();
        ds.tags = tags_edit->text().trimmed();

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_err()) {
            status->setText("Failed to save: " + QString::fromStdString(result.error()));
            status->setStyleSheet(
                QString("color:%1;font-size:12px;font-weight:700;background:transparent;").arg(col::NEGATIVE()));
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

// ─────────────────────────────────────────────────────────────────────────────
// Data functions
// ─────────────────────────────────────────────────────────────────────────────

QVector<ConnectorConfig> DataSourcesScreen::filtered_connectors() const {
    QVector<ConnectorConfig> filtered;
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    QSet<QString> configured_ids, active_ids;
    if (stat_filter_ == 1 || stat_filter_ == 2) {
        for (const auto& ds : connections_cache_) {
            configured_ids.insert(normalized_provider_key(ds));
            if (ds.enabled)
                active_ids.insert(normalized_provider_key(ds));
        }
    }

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!show_all_categories_ && cfg.category != active_category_)
            continue;
        if (!connector_matches_text(cfg, filter))
            continue;
        if (stat_filter_ == 1 && !configured_ids.contains(cfg.id))
            continue;
        if (stat_filter_ == 2 && !active_ids.contains(cfg.id))
            continue;
        if (stat_filter_ == 3 && !cfg.requires_auth)
            continue;
        filtered.append(cfg);
    }
    return filtered;
}

void DataSourcesScreen::build_category_ladder() {
    if (!category_list_)
        return;

    QVector<int> counts(12, 0);
    const QString filter = search_edit_ ? search_edit_->text().trimmed().toLower() : "";

    for (const auto& cfg : ConnectorRegistry::instance().all()) {
        if (!connector_matches_text(cfg, filter))
            continue;
        ++counts[0];
        ++counts[static_cast<int>(cfg.category) + 1];
    }

    QSignalBlocker blocker(category_list_);
    category_list_->clear();

    for (int i = 0; i < kCategoryLabels.size(); ++i) {
        const QString text = QString("%1  (%2)").arg(kCategoryLabels[i]).arg(counts[i]);
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, i);
        item->setForeground(i == 0 ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_SECONDARY()));
        category_list_->addItem(item);
    }

    const int active_row = show_all_categories_ ? 0 : (static_cast<int>(active_category_) + 1);
    if (category_list_->count() > active_row)
        category_list_->setCurrentRow(active_row);
}

void DataSourcesScreen::build_connector_table() {
    if (!connector_table_)
        return;

    const auto filtered = filtered_connectors();
    int preferred_row = -1;

    {
        QSignalBlocker blocker(connector_table_);
        connector_table_->setRowCount(filtered.size());

        for (int row = 0; row < filtered.size(); ++row) {
            const auto& cfg = filtered[row];
            const int total_saved = total_connections_for_provider(connections_cache_, cfg.id);
            const int live_saved = enabled_connections_for_provider(connections_cache_, cfg.id);

            // Col 0: code badge
            auto* code_item = make_item(connector_code(cfg), QColor(cfg.color), Qt::AlignCenter);
            connector_table_->setItem(row, 0, code_item);

            // Col 1: name (carries connector ID in UserRole)
            auto* name_item = make_item(cfg.name);
            name_item->setData(Qt::UserRole, cfg.id);
            connector_table_->setItem(row, 1, name_item);

            // Col 2: category
            connector_table_->setItem(row, 2, make_item(category_label(cfg.category), QColor(col::TEXT_SECONDARY())));

            // Col 3: auth
            connector_table_->setItem(row, 3,
                                      make_item(cfg.requires_auth ? "KEY" : "OPEN",
                                                cfg.requires_auth ? QColor(col::WARNING()) : QColor(col::POSITIVE()),
                                                Qt::AlignCenter));

            // Col 4: transport type
            connector_table_->setItem(
                row, 4, make_item(connector_transport(cfg), QColor(col::TEXT_TERTIARY()), Qt::AlignCenter));

            // Col 5: active connections
            connector_table_->setItem(row, 5,
                                      make_item(live_saved > 0 ? QString::number(live_saved) : "-",
                                                live_saved > 0 ? QColor(col::POSITIVE()) : QColor(col::TEXT_TERTIARY()),
                                                Qt::AlignCenter));

            // Col 6: total connections
            connector_table_->setItem(
                row, 6,
                make_item(total_saved > 0 ? QString::number(total_saved) : "-",
                          total_saved > 0 ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_TERTIARY()),
                          Qt::AlignCenter));

            if (cfg.id == selected_connector_id_)
                preferred_row = row;
        }
    }

    if (count_label_)
        count_label_->setText(QString("%1 / %2").arg(filtered.size()).arg(ConnectorRegistry::instance().count()));

    if (preferred_row >= 0) {
        connector_table_->selectRow(preferred_row);
    } else if (!selected_connector_id_.isEmpty()) {
        connector_table_->clearSelection();
    }

    update_detail_panel();
}

void DataSourcesScreen::build_connections_table() {
    if (!connections_table_)
        return;

    const auto rows = filtered_connection_rows();

    QSignalBlocker blocker(connections_table_);
    connections_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        const auto& ds = rows[r];
        const auto* cfg = find_connector_config(ds.provider);

        // Col 0: enable checkbox
        auto* toggle = new QCheckBox;
        toggle->setObjectName("dsEnableToggle");
        toggle->setChecked(ds.enabled);
        toggle->setProperty("conn_id", ds.id);
        toggle->setCursor(Qt::PointingHandCursor);
        connect(toggle, &QCheckBox::toggled, this,
                [this, id = ds.id](bool checked) { on_connection_enabled_changed(id, checked); });
        connections_table_->setCellWidget(r, 0, toggle);

        // Col 1: name (carries conn ID in UserRole)
        auto* name_item = make_item(ds.display_name);
        name_item->setData(Qt::UserRole, ds.id);
        connections_table_->setItem(r, 1, name_item);

        // Col 2: provider
        const QString prov_color = cfg ? cfg->color : col::TEXT_SECONDARY();
        connections_table_->setItem(r, 2, make_item(cfg ? cfg->name : ds.provider, QColor(prov_color)));

        // Col 3: category
        connections_table_->setItem(r, 3, make_item(ds.category, QColor(col::TEXT_SECONDARY())));

        // Col 4: type
        connections_table_->setItem(
            r, 4, make_item(cfg ? connector_transport(*cfg) : ds.type, QColor(col::TEXT_TERTIARY()), Qt::AlignCenter));

        // Col 5: live status
        const bool has_status = live_status_cache_.contains(ds.id);
        const bool ok = has_status ? live_status_cache_[ds.id].first : false;
        auto* status_lbl = new QLabel(has_status ? (ok ? "OK" : "ERR") : "--");
        status_lbl->setAlignment(Qt::AlignCenter);
        status_lbl->setObjectName("dsStatusDot");
        status_lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                      .arg(!has_status ? col::TEXT_TERTIARY()
                                           : ok        ? col::POSITIVE()
                                                       : col::NEGATIVE()));
        if (has_status)
            status_lbl->setToolTip(live_status_cache_[ds.id].second);
        connections_table_->setCellWidget(r, 5, status_lbl);

        // Col 6: tags
        connections_table_->setItem(r, 6, make_item(ds.tags, QColor(col::TEXT_TERTIARY())));

        // Col 7: updated_at
        connections_table_->setItem(r, 7, make_item(ds.updated_at.left(16), QColor(col::TEXT_TERTIARY())));
    }
}

void DataSourcesScreen::update_stats_strip() {
    if (!universe_stat_value_)
        return;

    const auto& all = ConnectorRegistry::instance().all();
    const int universe = all.size();

    QSet<QString> configured_providers;
    int active_count = 0, auth_count = 0;
    for (const auto& ds : connections_cache_) {
        configured_providers.insert(normalized_provider_key(ds));
        if (ds.enabled)
            ++active_count;
    }
    for (const auto& cfg : all) {
        if (cfg.requires_auth)
            ++auth_count;
    }

    universe_stat_value_->setText(QString::number(universe));
    configured_stat_value_->setText(QString::number(configured_providers.size()));
    active_stat_value_->setText(QString::number(active_count));
    auth_stat_value_->setText(QString::number(auth_count));

    // Highlight active stat filter
    auto reset_color = [](QLabel* v) {
        v->setStyleSheet(
            QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::TEXT_PRIMARY()));
    };
    auto set_amber = [](QLabel* v) {
        v->setStyleSheet(QString("color:%1;font-size:22px;font-weight:700;background:transparent;").arg(col::AMBER()));
    };

    reset_color(universe_stat_value_);
    reset_color(configured_stat_value_);
    reset_color(active_stat_value_);
    reset_color(auth_stat_value_);

    switch (stat_filter_) {
        case 0:
            set_amber(universe_stat_value_);
            break;
        case 1:
            set_amber(configured_stat_value_);
            break;
        case 2:
            set_amber(active_stat_value_);
            break;
        case 3:
            set_amber(auth_stat_value_);
            break;
        default:
            break;
    }
}

void DataSourcesScreen::update_provider_ladder() {
    if (!provider_ladder_)
        return;

    // Build connector -> total_connections map
    QMap<QString, int> conn_counts;
    for (const auto& ds : connections_cache_) {
        const QString key = normalized_provider_key(ds);
        conn_counts[key]++;
    }

    // Top 5 by count
    QVector<QPair<int, QString>> ranked;
    for (auto it = conn_counts.begin(); it != conn_counts.end(); ++it)
        ranked.append({it.value(), it.key()});
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    ranked = ranked.mid(0, 8);

    QSignalBlocker blocker(provider_ladder_);
    provider_ladder_->clear();

    for (const auto& [count, id] : ranked) {
        const auto* cfg = find_connector_config(id);
        const QString name = cfg ? cfg->name : id;
        auto* item = new QListWidgetItem(QString("%1  (%2)").arg(name).arg(count));
        item->setData(Qt::UserRole, id);
        item->setForeground(QColor(col::TEXT_SECONDARY()));
        provider_ladder_->addItem(item);
    }

    if (ranked.isEmpty()) {
        auto* empty = new QListWidgetItem("no connections yet");
        empty->setForeground(QColor(col::TEXT_TERTIARY()));
        empty->setFlags(Qt::NoItemFlags);
        provider_ladder_->addItem(empty);
    }
}

void DataSourcesScreen::update_detail_panel() {
    if (!detail_title_)
        return;

    const auto* cfg = find_connector_config(selected_connector_id_);

    if (!cfg) {
        detail_symbol_->setText("--");
        detail_symbol_->setStyleSheet(QString("min-width:38px;max-width:38px;min-height:38px;max-height:38px;"
                                              "font-size:13px;font-weight:700;color:%1;background:%2;"
                                              "border:1px solid %1;")
                                          .arg(col::AMBER(), col::BG_BASE()));
        detail_title_->setText("Select a connector");
        detail_description_->setText("Double-click any row to configure");
        detail_category_value_->setText("--");
        detail_transport_value_->setText("--");
        detail_auth_value_->setText("--");
        detail_test_value_->setText("--");
        detail_fields_value_->setText("--");
        detail_configured_value_->setText("--");
        detail_enabled_value_->setText("--");
        detail_last_status_value_->setText("--");
        if (field_table_)
            field_table_->setRowCount(0);
        if (detail_connections_list_)
            detail_connections_list_->clear();
        if (new_connection_btn_)
            new_connection_btn_->setEnabled(false);
        if (edit_connection_btn_)
            edit_connection_btn_->setEnabled(false);
        if (test_connection_btn_)
            test_connection_btn_->setEnabled(false);
        return;
    }

    // Identity
    detail_symbol_->setText(connector_code(*cfg));
    detail_symbol_->setStyleSheet(QString("min-width:38px;max-width:38px;min-height:38px;max-height:38px;"
                                          "font-size:13px;font-weight:700;color:%1;background:%2;border:1px solid %1;")
                                      .arg(cfg->color, col::BG_BASE()));
    detail_title_->setText(cfg->name);
    detail_description_->setText(cfg->description);

    // Metadata
    detail_category_value_->setText(category_label(cfg->category));
    detail_transport_value_->setText(connector_transport(*cfg));
    detail_auth_value_->setText(cfg->requires_auth ? "Required" : "None");
    detail_auth_value_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                          .arg(cfg->requires_auth ? col::WARNING() : col::POSITIVE()));
    detail_test_value_->setText(cfg->testable ? "Yes" : "No");

    const int total = total_connections_for_provider(connections_cache_, cfg->id);
    const int active = enabled_connections_for_provider(connections_cache_, cfg->id);
    detail_fields_value_->setText(QString::number(cfg->fields.size()));
    detail_configured_value_->setText(QString::number(total));
    detail_enabled_value_->setText(QString::number(active));
    detail_enabled_value_->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                             .arg(active > 0 ? col::POSITIVE() : col::TEXT_TERTIARY()));

    // Last test status
    const QString conn_id = effective_detail_connection_id();
    if (!conn_id.isEmpty() && live_status_cache_.contains(conn_id)) {
        const bool ok = live_status_cache_[conn_id].first;
        detail_last_status_value_->setText(ok ? "OK" : "ERR");
        detail_last_status_value_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                .arg(ok ? col::POSITIVE() : col::NEGATIVE()));
    } else {
        detail_last_status_value_->setText("--");
        detail_last_status_value_->setStyleSheet(
            QString("color:%1;font-size:11px;font-weight:700;background:transparent;").arg(col::TEXT_TERTIARY()));
    }

    // Fields table
    if (field_table_) {
        QSignalBlocker fb(field_table_);
        field_table_->setRowCount(cfg->fields.size());
        for (int i = 0; i < cfg->fields.size(); ++i) {
            const auto& f = cfg->fields[i];
            field_table_->setItem(i, 0, make_item(f.label));
            field_table_->setItem(i, 1,
                                  make_item(field_type_label(f.type), QColor(col::TEXT_SECONDARY()), Qt::AlignCenter));
            field_table_->setItem(i, 2,
                                  make_item(f.required ? "Y" : "N",
                                            f.required ? QColor(col::WARNING()) : QColor(col::TEXT_TERTIARY()),
                                            Qt::AlignCenter));
        }
        // Shrink to content (max 120px = ~5 rows)
        const int row_h = 24;
        const int hdr_h = 24;
        field_table_->setFixedHeight(std::min(hdr_h + static_cast<int>(cfg->fields.size()) * row_h, 120));
    }

    // Saved connections list
    if (detail_connections_list_) {
        QSignalBlocker lb(detail_connections_list_);
        detail_connections_list_->clear();
        for (const auto& ds : connections_cache_) {
            if (!connection_matches_connector(ds, cfg->id))
                continue;
            const QString status_str = ds.enabled ? "ACTIVE" : "OFF";
            const QString label = QString("%1  [%2]").arg(ds.display_name).arg(status_str);
            auto* item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, ds.id);
            item->setForeground(ds.enabled ? QColor(col::TEXT_PRIMARY()) : QColor(col::TEXT_SECONDARY()));
            detail_connections_list_->addItem(item);
            if (ds.id == selected_connection_id_) {
                detail_connections_list_->setCurrentItem(item);
            }
        }
        if (detail_connections_list_->count() == 0) {
            auto* empty = new QListWidgetItem("No connections configured");
            empty->setForeground(QColor(col::TEXT_TERTIARY()));
            empty->setFlags(Qt::NoItemFlags);
            detail_connections_list_->addItem(empty);
        }
    }

    update_action_states();
}

void DataSourcesScreen::update_action_states() {
    const bool has_connector = !selected_connector_id_.isEmpty();
    const QString conn_id = effective_detail_connection_id();
    const bool has_conn = !conn_id.isEmpty();

    if (new_connection_btn_)
        new_connection_btn_->setEnabled(has_connector);
    if (edit_connection_btn_)
        edit_connection_btn_->setEnabled(has_conn);
    if (test_connection_btn_)
        test_connection_btn_->setEnabled(has_conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

QString DataSourcesScreen::effective_detail_connection_id() const {
    if (!selected_connection_id_.isEmpty())
        return selected_connection_id_;
    return preferred_connection_for_connector(selected_connector_id_);
}

QString DataSourcesScreen::preferred_connection_for_connector(const QString& connector_id) const {
    QString first_match;
    for (const auto& ds : connections_cache_) {
        if (!connection_matches_connector(ds, connector_id))
            continue;
        if (ds.enabled)
            return ds.id;
        if (first_match.isEmpty())
            first_match = ds.id;
    }
    return first_match;
}

void DataSourcesScreen::select_connector_by_id(const QString& connector_id) {
    const auto* cfg = find_connector_config(connector_id);
    if (!cfg)
        return;
    selected_connector_id_ = cfg->id;
    if (!connector_table_)
        return;
    for (int row = 0; row < connector_table_->rowCount(); ++row) {
        auto* item = connector_table_->item(row, 1);
        if (item && item->data(Qt::UserRole).toString() == selected_connector_id_) {
            connector_table_->selectRow(row);
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void DataSourcesScreen::on_category_filter(int idx) {
    show_all_categories_ = (idx == 0);
    if (idx > 0)
        active_category_ = static_cast<Category>(idx - 1);
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
        ScreenStateManager::instance().notify_changed(this);
        const QString existing_connection = preferred_connection_for_connector(cfg->id);
        show_config_dialog(*cfg, existing_connection);
    }
}

void DataSourcesScreen::on_connection_add() {
    if (selected_connector_id_.isEmpty()) {
        const auto filtered = filtered_connectors();
        if (filtered.isEmpty())
            return;
        selected_connector_id_ = filtered.first().id;
    }
    if (const auto* cfg = find_connector_config(selected_connector_id_))
        show_config_dialog(*cfg);
}

void DataSourcesScreen::on_connection_edit(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err())
        return;
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
    if (selected_connection_id_ == conn_id)
        selected_connection_id_.clear();
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

// ── Provider probe URLs ───────────────────────────────────────────────────────

// Returns a URL whose host:port we TCP-probe to verify reachability.
// For connectors that have no URL at all we return {} — the host+port fallback
// in on_connection_test() will pick up host/port fields directly.
// Connectors with testable=false never reach this function.
static QString provider_probe_url(const QString& provider_id, const QJsonObject& cfg) {
    // ── Market data ───────────────────────────────────────────────────────────
    if (provider_id == "yahoo-finance")
        return "https://query1.finance.yahoo.com/v8/finance/spark?symbols=AAPL&range=1d&interval=1d";
    if (provider_id == "alpha-vantage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY"
                       "&symbol=IBM&interval=5min&apikey=%1")
            .arg(key.isEmpty() ? "demo" : key);
    }
    if (provider_id == "finnhub") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://finnhub.io/api/v1/stock/profile2?symbol=AAPL&token=%1").arg(key);
    }
    if (provider_id == "iex-cloud") {
        const QString token = cfg.value("token").toString().trimmed();
        const bool sandbox = cfg.value("sandbox").toBool();
        const QString base = sandbox ? "https://sandbox.iexapis.com" : "https://cloud.iexapis.com";
        return QString("%1/stable/stock/AAPL/quote?token=%2").arg(base, token);
    }
    if (provider_id == "twelve-data") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.twelvedata.com/time_series?symbol=AAPL&interval=1min&outputsize=1&apikey=%1")
            .arg(key);
    }
    if (provider_id == "quandl") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://data.nasdaq.com/api/v3/datasets/WIKI/AAPL.json?rows=1&api_key=%1").arg(key);
    }
    if (provider_id == "polygon-io")
        return "https://api.polygon.io/v2/aggs/ticker/AAPL/range/1/day/2023-01-09/2023-01-09";
    if (provider_id == "factset")
        return "https://api.factset.com/health/alive";
    if (provider_id == "refinitiv")
        return "https://api.refinitiv.com/";
    if (provider_id == "pitchbook")
        return "https://api.pitchbook.com/";
    if (provider_id == "nasdaq-totalview")
        return "https://api.nasdaq.com/api/quote/AAPL/info?assetClass=stocks";
    if (provider_id == "fincept") {
        // host+port handled by TCP fallback — no HTTP probe
        return {};
    }

    // ── Crypto ────────────────────────────────────────────────────────────────
    if (provider_id == "binance")
        return "https://api.binance.com/api/v3/ping";
    if (provider_id == "coinbase")
        return "https://api.coinbase.com/v2/time";
    if (provider_id == "kraken")
        return "https://api.kraken.com/0/public/Time";
    if (provider_id == "coinmarketcap") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://pro-api.coinmarketcap.com/v1/cryptocurrency/listings/latest?limit=1&CMC_PRO_API_KEY=%1")
            .arg(key);
    }
    if (provider_id == "coingecko")
        return "https://api.coingecko.com/api/v3/ping";

    // ── EOD / Tiingo / Intrinio ───────────────────────────────────────────────
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

    // ── Reuters / Refinitiv (legacy) ──────────────────────────────────────────
    if (provider_id == "reuters") {
        const QString appKey = cfg.value("appKey").toString().trimmed();
        return QString("https://api.refinitiv.com/user-framework/machine-id/v1/health?appKey=%1").arg(appKey);
    }
    if (provider_id == "marketstack") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        const bool use_https = cfg.value("https").toBool();
        return QString("%1://api.marketstack.com/v1/tickers/AAPL?access_key=%2").arg(use_https ? "https" : "http", key);
    }
    if (provider_id == "finage") {
        const QString key = cfg.value("apiKey").toString().trimmed();
        return QString("https://api.finage.co.uk/agg/stock/prev-close/AAPL?apikey=%1").arg(key);
    }
    if (provider_id == "tradier") {
        const bool sandbox = cfg.value("sandbox").toBool();
        return sandbox ? "https://sandbox.tradier.com/v1/markets/clock" : "https://api.tradier.com/v1/markets/clock";
    }

    // ── Alternative data ─────────────────────────────────────────────────────
    if (provider_id == "ravenpack")
        return "https://api.ravenpack.com/user-text";
    if (provider_id == "bloomberg-second-measure")
        return "https://app.secondmeasure.com/";
    if (provider_id == "safegraph")
        return "https://api.safegraph.com/v1/healthcheck";
    if (provider_id == "orbital-insight")
        return "https://api.orbitalinsight.com/";
    if (provider_id == "refinitiv-tick-history")
        return "https://selectapi.datascope.refinitiv.com/RestApi/v1/Authentication/RequestToken";
    if (provider_id == "earnest-research")
        return "https://api.earnestresearch.com/";
    if (provider_id == "thinknum")
        return "https://data.thinknum.com/";
    if (provider_id == "revelio-labs")
        return "https://api.reveliolabs.com/";
    if (provider_id == "adanos-market-sentiment")
        return "https://api.adanos.org/news/stocks/v1/health";

    // ── Open banking ─────────────────────────────────────────────────────────
    if (provider_id == "plaid")
        return "https://production.plaid.com/";
    if (provider_id == "mx")
        return "https://int-api.mx.com/";
    if (provider_id == "finicity")
        return "https://api.finicity.com/";
    if (provider_id == "truelayer")
        return "https://auth.truelayer.com/";
    if (provider_id == "fdx-api") {
        const QString base = cfg.value("baseUrl").toString().trimmed();
        if (!base.isEmpty())
            return base;
        return "https://api.financial-data-exchange.org/";
    }

    // ── Generic protocol connectors ───────────────────────────────────────────
    // REST API: probe baseUrl directly (any non-refused response = reachable)
    if (provider_id == "rest-api") {
        const QString base = cfg.value("baseUrl").toString().trimmed();
        if (!base.isEmpty())
            return base;
        return {};
    }
    // GraphQL: probe the endpoint (TCP connect suffices; POST introspection would need QNetworkAccessManager)
    if (provider_id == "graphql") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty())
            return ep;
        return {};
    }
    // WebSocket: extract host:port from ws:// or wss:// URL for TCP probe
    if (provider_id == "websocket") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty())
            return url; // QUrl parser in test code handles ws/wss → port 80/443
        return {};
    }
    // gRPC: host+port TCP probe handled by fallback (host/port fields present)
    // rabbitmq: host+port TCP fallback handles it
    // fix-protocol: host+port TCP fallback handles it
    // SOAP: probe WSDL URL
    if (provider_id == "soap") {
        const QString wsdl = cfg.value("wsdlUrl").toString().trimmed();
        if (!wsdl.isEmpty())
            return wsdl;
        return {};
    }
    // OData: probe mandatory $metadata endpoint
    if (provider_id == "odata") {
        const QString root = cfg.value("serviceRoot").toString().trimmed();
        if (!root.isEmpty())
            return (root.endsWith('/') ? root : root + "/") + "$metadata";
        return {};
    }
    // MQTT: broker field is host, port field is port — TCP fallback via host+port handles it
    // Kafka: brokers field "host:port" handled by brokers fallback in on_connection_test
    // NATS: servers field "nats://host:port" handled by servers fallback in on_connection_test
    // Redpanda: same as Kafka — brokers fallback
    // Apache Flink: probe /overview REST endpoint
    if (provider_id == "apache-flink") {
        const QString jm = cfg.value("jobManagerUrl").toString().trimmed();
        if (!jm.isEmpty())
            return (jm.endsWith('/') ? jm : jm + "/") + "overview";
        return "http://localhost:8081/overview";
    }
    // Apache Iceberg REST catalog: probe /v1/config
    if (provider_id == "apache-iceberg") {
        const QString catalog = cfg.value("catalog").toString().trimmed().toLower();
        const QString uri = cfg.value("catalogUri").toString().trimmed();
        if (catalog == "rest" && !uri.isEmpty())
            return (uri.endsWith('/') ? uri : uri + "/") + "v1/config";
        return {}; // non-REST catalogs (Hive, Glue, JDBC) have no HTTP endpoint
    }

    // ── Streaming / messaging (host+port via TCP fallback) ────────────────────
    // rabbitmq, fix-protocol: host+port fields → TCP fallback handles them
    // No probe URL needed here.

    // ── Cloud storage (no URL-based health endpoints — TCP to S3/GCS/Azure) ──
    if (provider_id == "aws-s3")
        return "https://s3.amazonaws.com/";
    if (provider_id == "google-cloud-storage")
        return "https://storage.googleapis.com/";
    if (provider_id == "azure-blob")
        return "https://blob.core.windows.net/";
    if (provider_id == "digitalocean-spaces")
        return "https://nyc3.digitaloceanspaces.com/";
    if (provider_id == "backblaze-b2")
        return "https://api.backblazeb2.com/b2api/v2/b2_authorize_account";
    if (provider_id == "wasabi")
        return "https://s3.wasabisys.com/";
    if (provider_id == "cloudflare-r2")
        return "https://api.cloudflare.com/client/v4/user/tokens/verify";
    if (provider_id == "oracle-cloud-storage")
        return "https://objectstorage.us-ashburn-1.oraclecloud.com/";
    if (provider_id == "minio") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty())
            return (ep.startsWith("http") ? ep : "http://" + ep) + "/minio/health/live";
        return {};
    }
    if (provider_id == "ibm-cloud-storage") {
        const QString ep = cfg.value("endpoint").toString().trimmed();
        if (!ep.isEmpty())
            return ep;
        return "https://s3.us.cloud-object-storage.appdomain.cloud/";
    }

    // ── Search & analytics ────────────────────────────────────────────────────
    if (provider_id == "elasticsearch") {
        const QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty())
            return (node.endsWith('/') ? node : node + "/") + "_cluster/health";
        return {};
    }
    if (provider_id == "opensearch") {
        const QString node = cfg.value("node").toString().trimmed();
        if (!node.isEmpty())
            return (node.endsWith('/') ? node : node + "/") + "_cluster/health";
        return {};
    }
    if (provider_id == "solr") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty())
            return (host.startsWith("http") ? host : "http://" + host) + "/solr/admin/info/system";
        return {};
    }
    if (provider_id == "meilisearch") {
        const QString host = cfg.value("host").toString().trimmed();
        if (!host.isEmpty())
            return (host.startsWith("http") ? host : "http://" + host) + "/health";
        return {};
    }
    if (provider_id == "algolia") {
        const QString appId = cfg.value("appId").toString().trimmed();
        if (!appId.isEmpty())
            return QString("https://%1-dsn.algolia.net/1/isalive").arg(appId);
        return {};
    }

    // ── Time series ───────────────────────────────────────────────────────────
    if (provider_id == "influxdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty())
            return (url.endsWith('/') ? url : url + "/") + "health";
        return {};
    }
    if (provider_id == "prometheus") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty())
            return (url.endsWith('/') ? url : url + "/") + "-/healthy";
        return {};
    }
    if (provider_id == "victoriametrics") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty())
            return (url.endsWith('/') ? url : url + "/") + "health";
        return {};
    }
    if (provider_id == "opentsdb") {
        const QString url = cfg.value("url").toString().trimmed();
        if (!url.isEmpty())
            return (url.endsWith('/') ? url : url + "/") + "api/version";
        return {};
    }
    // questdb, timescaledb, kdb, clickhouse: host+port TCP fallback

    // ── Data warehouses ───────────────────────────────────────────────────────
    if (provider_id == "bigquery")
        return "https://bigquery.googleapis.com/";
    if (provider_id == "firestore")
        return "https://firestore.googleapis.com/";
    if (provider_id == "databricks") {
        const QString h = cfg.value("serverHostname").toString().trimmed();
        if (!h.isEmpty())
            return QString("https://%1/api/2.0/clusters/list").arg(h);
        return {};
    }
    if (provider_id == "synapse") {
        const QString server = cfg.value("server").toString().trimmed();
        if (!server.isEmpty())
            return QString("https://%1").arg(server);
        return {};
    }
    // redshift, snowflake: host+port TCP fallback

    return {};
}

void DataSourcesScreen::on_connection_test(const QString& conn_id) {
    const auto get_result = DataSourceRepository::instance().get(conn_id);
    if (get_result.is_err())
        return;

    const auto ds = get_result.value();
    const auto cfg_doc = QJsonDocument::fromJson(ds.config.toUtf8());
    const auto cfg_obj = cfg_doc.object();

    const auto* connector_cfg = find_connector_config(ds.provider);
    if (connector_cfg && !connector_cfg->testable) {
        QDialog result_dlg(this);
        result_dlg.setWindowTitle(QString("Test: %1").arg(ds.display_name));
        result_dlg.resize(420, 160);
        result_dlg.setModal(true);
        result_dlg.setStyleSheet(
            QString("QDialog{background:%1;color:%2;font-family:'Consolas','Courier New',monospace;}"
                    "QLabel{font-size:13px;background:transparent;}"
                    "QPushButton{background:%3;color:%2;border:1px solid %4;"
                    "padding:6px 18px;font-size:12px;font-weight:700;}"
                    "QPushButton:hover{background:%4;}")
                .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM()));
        auto* vl = new QVBoxLayout(&result_dlg);
        vl->setContentsMargins(24, 20, 24, 16);
        vl->setSpacing(10);
        auto* lbl = new QLabel("This connector does not support connectivity testing.");
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;").arg(col::TEXT_SECONDARY()));
        vl->addWidget(lbl);
        vl->addStretch();
        auto* btn = new QPushButton("Close");
        btn->setCursor(Qt::PointingHandCursor);
        QObject::connect(btn, &QPushButton::clicked, &result_dlg, &QDialog::accept);
        auto* row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(btn);
        vl->addLayout(row);
        result_dlg.exec();
        return;
    }

    QPointer<DataSourcesScreen> self = this;
    const QString display = ds.display_name;
    const QString provider = ds.provider;

    const QString probe_url = provider_probe_url(provider, cfg_obj);

    QString explicit_url;
    if (probe_url.isEmpty()) {
        const QStringList url_keys = {"url", "baseUrl", "endpoint", "wsdlUrl", "serviceRoot"};
        for (const auto& key : url_keys) {
            const QString v = cfg_obj.value(key).toString().trimmed();
            if (!v.isEmpty()) {
                explicit_url = v;
                break;
            }
        }
    }

    const QString test_url = probe_url.isEmpty() ? explicit_url : probe_url;

    QString host = cfg_obj.value("host").toString().trimmed();
    int port = cfg_obj.value("port").toVariant().toInt();
    if (port <= 0)
        port = cfg_obj.value("port").toString().trimmed().toInt();

    if (host.isEmpty()) {
        const QString brokers = cfg_obj.value("brokers").toString().trimmed();
        if (!brokers.isEmpty()) {
            const QString first = brokers.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
            }
        }
    }
    if (host.isEmpty()) {
        for (const auto& key : {"servers", "hosts"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (val.isEmpty())
                continue;
            const QString first = val.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const QUrl u(first);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0)
                    port = u.port();
            } else {
                const int idx = first.lastIndexOf(':');
                if (idx > 0) {
                    host = first.left(idx);
                    port = first.mid(idx + 1).toInt();
                } else {
                    host = first;
                }
            }
            if (!host.isEmpty())
                break;
        }
    }
    if (host.isEmpty()) {
        const QString cs = cfg_obj.value("connectionString").toString().trimmed();
        if (!cs.isEmpty()) {
            const QUrl u(cs);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0)
                    port = u.port(27017);
            }
        }
    }
    if (host.isEmpty()) {
        const QString uri = cfg_obj.value("uri").toString().trimmed();
        if (!uri.isEmpty()) {
            const QUrl u(uri);
            if (u.isValid() && !u.host().isEmpty()) {
                host = u.host();
                if (port <= 0)
                    port = u.port(7687);
            }
        }
    }
    if (host.isEmpty()) {
        const QString cp = cfg_obj.value("contactPoints").toString().trimmed();
        if (!cp.isEmpty()) {
            const QString first = cp.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
            }
            if (port <= 0)
                port = cfg_obj.value("port").toVariant().toInt();
            if (port <= 0)
                port = 9042;
        }
    }
    if (host.isEmpty()) {
        const QString zk = cfg_obj.value("zkQuorum").toString().trimmed();
        if (!zk.isEmpty()) {
            const QString first = zk.split(',', Qt::SkipEmptyParts).value(0).trimmed();
            const int idx = first.lastIndexOf(':');
            if (idx > 0) {
                host = first.left(idx);
                port = first.mid(idx + 1).toInt();
            } else {
                host = first;
                port = 2181;
            }
        }
    }
    if (host.isEmpty()) {
        for (const auto& key : {"serverHostname", "server", "account"}) {
            const QString val = cfg_obj.value(key).toString().trimmed();
            if (!val.isEmpty()) {
                host = val;
                break;
            }
        }
    }
    if (host.isEmpty() && !test_url.isEmpty()) {
        const QUrl u(test_url);
        if (u.isValid() && !u.host().isEmpty()) {
            host = u.host();
            if (port <= 0) {
                const QString s = u.scheme().toLower();
                port = u.port((s == "https" || s == "wss") ? 443 : 80);
            }
        }
    }

    if (test_url.isEmpty() && (host.isEmpty() || port <= 0)) {
        QDialog result_dlg(this);
        result_dlg.setWindowTitle(QString("Test: %1").arg(display));
        result_dlg.resize(420, 160);
        result_dlg.setModal(true);
        result_dlg.setStyleSheet(
            QString("QDialog{background:%1;color:%2;font-family:'Consolas','Courier New',monospace;}"
                    "QLabel{font-size:13px;background:transparent;}"
                    "QPushButton{background:%3;color:%2;border:1px solid %4;"
                    "padding:6px 18px;font-size:12px;font-weight:700;}"
                    "QPushButton:hover{background:%4;}")
                .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM()));
        auto* vl = new QVBoxLayout(&result_dlg);
        vl->setContentsMargins(24, 20, 24, 16);
        vl->setSpacing(10);
        auto* lbl = new QLabel("No testable endpoint found in the saved configuration.\n"
                               "Ensure required fields (URL, host, or API key) are filled in.");
        lbl->setWordWrap(true);
        lbl->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;").arg(col::TEXT_SECONDARY()));
        vl->addWidget(lbl);
        vl->addStretch();
        auto* btn = new QPushButton("Close");
        btn->setCursor(Qt::PointingHandCursor);
        QObject::connect(btn, &QPushButton::clicked, &result_dlg, &QDialog::accept);
        auto* row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(btn);
        vl->addLayout(row);
        result_dlg.exec();
        return;
    }

    const QString captured_test_url = test_url;
    const QString captured_host = host;
    const int captured_port = port;

    const auto test_future =
        QtConcurrent::run([self, conn_id, display, captured_test_url, captured_host, captured_port]() {
            bool success = false;
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
                const QUrl url(captured_test_url);
                if (!url.isValid() || url.host().isEmpty()) {
                    message = QString("Invalid URL: %1").arg(captured_test_url);
                } else {
                    const QString url_host = url.host();
                    const QString scheme = url.scheme().toLower();
                    const int default_port = (scheme == "https" || scheme == "wss") ? 443 : 80;
                    const int url_port = url.port(default_port);
                    auto [ok, msg] = tcp_probe(url_host, url_port, 5000);
                    success = ok;
                    message = ok ? QString("Endpoint reachable: %1").arg(captured_test_url) : msg;
                }
            } else if (!captured_host.isEmpty() && captured_port > 0) {
                auto [ok, msg] = tcp_probe(captured_host, captured_port, 3000);
                success = ok;
                message = msg;
            }

            QMetaObject::invokeMethod(
                qApp,
                [self, conn_id, display, success, message]() {
                    if (!self)
                        return;
                    LOG_INFO(TAG, QString("Test %1: %2 — %3").arg(display, success ? "OK" : "FAIL", message));
                    self->live_status_cache_[conn_id] = {success, message};
                    self->update_connection_status_cell(conn_id, success, message);
                    self->update_detail_panel();

                    QDialog result_dlg(self);
                    result_dlg.setWindowTitle(QString("Test: %1").arg(display));
                    result_dlg.resize(440, 190);
                    result_dlg.setModal(true);
                    result_dlg.setStyleSheet(
                        QString("QDialog{background:%1;color:%2;font-family:'Consolas','Courier New',monospace;}"
                                "QLabel{font-size:13px;background:transparent;}"
                                "QPushButton{background:%3;color:%2;border:1px solid %4;"
                                "padding:6px 18px;font-size:12px;font-weight:700;}"
                                "QPushButton:hover{background:%4;}")
                            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM()));

                    auto* vl = new QVBoxLayout(&result_dlg);
                    vl->setContentsMargins(24, 20, 24, 16);
                    vl->setSpacing(10);

                    auto* status_lbl = new QLabel(success ? "Connection successful" : "Connection failed");
                    status_lbl->setStyleSheet(
                        QString("color:%1;font-size:14px;font-weight:700;background:transparent;")
                            .arg(success ? col::POSITIVE.operator QString() : col::NEGATIVE.operator QString()));
                    vl->addWidget(status_lbl);

                    auto* msg_lbl = new QLabel(message);
                    msg_lbl->setWordWrap(true);
                    msg_lbl->setStyleSheet(
                        QString("color:%1;font-size:12px;background:transparent;").arg(col::TEXT_SECONDARY()));
                    vl->addWidget(msg_lbl);

                    if (success) {
                        auto* note =
                            new QLabel("Note: TCP reachability confirmed. API key validity is not verified here.");
                        note->setWordWrap(true);
                        note->setStyleSheet(QString("color:%1;font-size:11px;font-style:italic;background:transparent;")
                                                .arg(col::TEXT_TERTIARY()));
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
                },
                Qt::QueuedConnection);
        });
    Q_UNUSED(test_future);
}

void DataSourcesScreen::on_connector_selection_changed() {
    if (!connector_table_)
        return;
    const auto selected = connector_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connector_id_.clear();
    } else {
        const int row = connector_table_->currentRow();
        auto* item = connector_table_->item(row, 1);
        if (item)
            selected_connector_id_ = item->data(Qt::UserRole).toString();
    }
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_connection_selection_changed() {
    if (!connections_table_)
        return;
    const auto selected = connections_table_->selectedItems();
    if (selected.isEmpty()) {
        selected_connection_id_.clear();
    } else {
        const int row = connections_table_->currentRow();
        auto* item = connections_table_->item(row, 1);
        if (item)
            selected_connection_id_ = item->data(Qt::UserRole).toString();
    }
    update_action_states();
}

void DataSourcesScreen::on_category_selection_changed(int row) {
    on_category_filter(row);
}

void DataSourcesScreen::on_provider_ladder_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QString connector_id = item->data(Qt::UserRole).toString();
    if (connector_id.isEmpty())
        return;
    select_connector_by_id(connector_id);
    update_detail_panel();
    update_stats_strip();
}

void DataSourcesScreen::on_detail_connection_activated(QListWidgetItem* item) {
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QString conn_id = item->data(Qt::UserRole).toString();
    if (conn_id.isEmpty())
        return;
    selected_connection_id_ = conn_id;
    update_action_states();
}

QVector<DataSource> DataSourcesScreen::filtered_connection_rows() const {
    const QString filter = conn_search_text_.trimmed().toLower();
    QVector<DataSource> result;
    for (const auto& ds : connections_cache_) {
        if (stat_filter_ == 2 && !ds.enabled)
            continue;
        if (stat_filter_ == 3) {
            const auto* cfg = find_connector_config(ds.provider);
            if (!cfg || !cfg->requires_auth)
                continue;
        }
        if (!filter.isEmpty()) {
            const QString hay =
                QString("%1 %2 %3 %4").arg(ds.display_name, ds.provider, ds.category, ds.tags).toLower();
            if (!hay.contains(filter))
                continue;
        }
        result.append(ds);
    }
    return result;
}

void DataSourcesScreen::update_connection_status_cell(const QString& conn_id, bool ok, const QString& msg) {
    if (!connections_table_)
        return;
    for (int row = 0; row < connections_table_->rowCount(); ++row) {
        auto* name_item = connections_table_->item(row, 1);
        if (!name_item || name_item->data(Qt::UserRole).toString() != conn_id)
            continue;
        auto* lbl = qobject_cast<QLabel*>(connections_table_->cellWidget(row, 5));
        if (lbl) {
            lbl->setText(ok ? "OK" : "ERR");
            lbl->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                   .arg(ok ? col::POSITIVE() : col::NEGATIVE()));
            lbl->setToolTip(msg);
        }
        break;
    }
}

void DataSourcesScreen::on_view_mode_toggle() {
    // Legacy — now handled by tab bar
    view_mode_ = (view_mode_ == ViewMode::Gallery) ? ViewMode::Connections : ViewMode::Gallery;
    if (page_stack_)
        page_stack_->setCurrentIndex(view_mode_ == ViewMode::Gallery ? 0 : 1);
}

void DataSourcesScreen::on_connections_search_changed(const QString& text) {
    conn_search_text_ = text;
    build_connections_table();
    update_stats_strip();
}

void DataSourcesScreen::on_stat_box_clicked(int stat_index) {
    apply_stat_filter(stat_index);
}

void DataSourcesScreen::apply_stat_filter(int stat_index) {
    stat_filter_ = (stat_filter_ == stat_index) ? -1 : stat_index;
    build_connector_table();
    build_connections_table();
    update_stats_strip();
}

void DataSourcesScreen::on_connection_duplicate(const QString& conn_id) {
    const auto result = DataSourceRepository::instance().get(conn_id);
    if (result.is_err())
        return;
    const auto ds = result.value();
    if (const auto* cfg = find_connector_config(ds.provider)) {
        selected_connector_id_ = cfg->id;
        show_config_dialog(*cfg, ds.id, /*duplicate=*/true);
    }
}

void DataSourcesScreen::on_bulk_enable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (!ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, true);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-enabled %1 connections").arg(rows.size()));
}

void DataSourcesScreen::on_bulk_disable_all() {
    const auto rows = filtered_connection_rows();
    for (const auto& ds : rows) {
        if (ds.enabled)
            DataSourceRepository::instance().set_enabled(ds.id, false);
    }
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-disabled %1 connections").arg(rows.size()));
}

void DataSourcesScreen::on_bulk_delete_selected() {
    if (!connections_table_)
        return;

    QSet<QString> selected_ids;
    const auto selected_items = connections_table_->selectedItems();
    for (auto* item : selected_items) {
        if (item->column() == 1)
            selected_ids.insert(item->data(Qt::UserRole).toString());
    }
    if (selected_ids.isEmpty())
        return;

    QMessageBox confirm(this);
    confirm.setWindowTitle("Delete Connections");
    confirm.setText(QString("Delete %1 selected connection(s)?").arg(selected_ids.size()));
    confirm.setInformativeText("This cannot be undone.");
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.setStyleSheet(
        QString("QMessageBox { background:%1; color:%2; font-family:'Consolas','Courier New',monospace; }"
                "QLabel { color:%2; font-size:13px; background:transparent; }"
                "QPushButton { background:%3; color:%2; border:1px solid %4;"
                " padding:6px 18px; font-size:12px; font-weight:700; }"
                "QPushButton:hover { background:%4; }")
            .arg(col::BG_SURFACE(), col::TEXT_PRIMARY(), col::BG_RAISED(), col::BORDER_DIM()));
    if (confirm.exec() != QMessageBox::Yes)
        return;

    for (const auto& id : selected_ids) {
        DataSourceRepository::instance().remove(id);
        live_status_cache_.remove(id);
    }
    if (selected_ids.contains(selected_connection_id_))
        selected_connection_id_.clear();
    refresh_connections();
    LOG_INFO(TAG, QString("Bulk-deleted %1 connections").arg(selected_ids.size()));
}

void DataSourcesScreen::on_export_connections() {
    const QString path = QFileDialog::getSaveFileName(this, "Export Connections", "fincept_connections.json",
                                                      "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return;

    const auto all = DataSourceRepository::instance().list_all();
    if (all.is_err())
        return;

    QJsonArray arr;
    for (const auto& ds : all.value()) {
        QJsonObject obj;
        obj["id"] = ds.id;
        obj["alias"] = ds.alias;
        obj["display_name"] = ds.display_name;
        obj["description"] = ds.description;
        obj["type"] = ds.type;
        obj["provider"] = ds.provider;
        obj["category"] = ds.category;
        obj["config"] = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        obj["enabled"] = ds.enabled;
        obj["tags"] = ds.tags;
        obj["created_at"] = ds.created_at;
        obj["updated_at"] = ds.updated_at;
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Export failed: cannot write to %1").arg(path));
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Exported %1 connections to %2").arg(arr.size()).arg(path));

    services::FileManagerService::instance().import_file(path, "data_sources");
}

void DataSourcesScreen::on_import_connections() {
    const QString path =
        QFileDialog::getOpenFileName(this, "Import Connections", "", "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(TAG, QString("Import failed: cannot open %1").arg(path));
        return;
    }
    const QByteArray payload = file.readAll();
    QJsonParseError parse_err;
    const auto doc = QJsonDocument::fromJson(payload, &parse_err);
    if (parse_err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR(TAG, QString("Import failed: invalid JSON — %1").arg(parse_err.errorString()));
        return;
    }

    int imported = 0, skipped = 0;
    for (const auto& val : doc.array()) {
        const auto obj = val.toObject();
        const QString provider = obj["provider"].toString();
        if (provider.isEmpty()) {
            ++skipped;
            continue;
        }

        DataSource ds;
        ds.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ds.alias = obj["alias"].toString();
        ds.display_name = obj["display_name"].toString();
        ds.description = obj["description"].toString();
        ds.type = obj["type"].toString();
        ds.provider = provider;
        ds.category = obj["category"].toString();
        ds.config = QJsonDocument(obj["config"].toObject()).toJson(QJsonDocument::Compact);
        ds.enabled = obj["enabled"].toBool(false);
        ds.tags = obj["tags"].toString();

        if (ds.display_name.isEmpty())
            ds.display_name = provider;
        if (ds.alias.isEmpty())
            ds.alias = provider + "_" + ds.id.left(8);

        const auto result = DataSourceRepository::instance().save(ds);
        if (result.is_ok())
            ++imported;
        else
            ++skipped;
    }

    refresh_connections();
    LOG_INFO(TAG, QString("Import complete: %1 imported, %2 skipped").arg(imported).arg(skipped));
    services::FileManagerService::instance().import_file(path, "data_sources");
}

void DataSourcesScreen::on_download_template() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Connector Template", "fincept_connections_template.json", "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return;

    const auto& all = ConnectorRegistry::instance().all();

    QMap<QString, QJsonArray> by_category;
    for (const auto& cfg : all) {
        QJsonObject config_obj;
        for (const auto& field : cfg.fields) {
            QString val = !field.default_value.isEmpty() ? field.default_value
                                                         : (!field.placeholder.isEmpty() ? field.placeholder : "");
            config_obj[field.name] = val;
        }

        QJsonObject entry;
        entry["provider"] = cfg.id;
        entry["display_name"] = cfg.name;
        entry["alias"] = cfg.id + "_1";
        entry["description"] = cfg.description;
        entry["type"] = cfg.type;
        entry["category"] = category_str(cfg.category);
        entry["enabled"] = false;
        entry["tags"] = "";
        entry["config"] = config_obj;
        entry["_help"] = QString("Fill in the 'config' fields and set 'enabled' to true. "
                                 "Remove this '_help' key before importing.");

        by_category[category_label(cfg.category)].append(entry);
    }

    QStringList cat_order = {"Databases",   "APIs",        "Streaming",          "File Sources",   "Cloud Storage",
                             "Time Series", "Market Data", "Search & Analytics", "Data Warehouses"};
    QJsonArray out;
    for (const auto& cat : cat_order) {
        if (by_category.contains(cat)) {
            for (const auto& entry : by_category[cat])
                out.append(entry);
        }
    }
    for (auto it = by_category.begin(); it != by_category.end(); ++it) {
        if (!cat_order.contains(it.key())) {
            for (const auto& entry : it.value())
                out.append(entry);
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(TAG, QString("Template write failed: %1").arg(path));
        return;
    }
    file.write(QJsonDocument(out).toJson(QJsonDocument::Indented));
    file.close();
    LOG_INFO(TAG, QString("Template written: %1 connectors to %2").arg(out.size()).arg(path));
}

void DataSourcesScreen::on_poll_timer() {
    for (const auto& ds : connections_cache_) {
        if (!ds.enabled)
            continue;
        const auto* connector_cfg = find_connector_config(ds.provider);
        if (!connector_cfg || !connector_cfg->testable)
            continue;

        const auto cfg_obj = QJsonDocument::fromJson(ds.config.toUtf8()).object();
        const QString probe_url = provider_probe_url(ds.provider, cfg_obj);

        QString host = cfg_obj.value("host").toString().trimmed();
        int port = cfg_obj.value("port").toVariant().toInt();

        if (host.isEmpty() && !probe_url.isEmpty()) {
            const QUrl u(probe_url);
            host = u.host();
            if (port <= 0) {
                const QString s = u.scheme().toLower();
                port = u.port((s == "https" || s == "wss") ? 443 : 80);
            }
        }
        if (host.isEmpty() || port <= 0)
            continue;

        QPointer<DataSourcesScreen> self = this;
        const QString conn_id = ds.id;
        const QString cap_host = host;
        const int cap_port = port;
        const QString cap_probe = probe_url;

        (void)QtConcurrent::run([self, conn_id, cap_host, cap_port, cap_probe]() {
            QTcpSocket socket;
            socket.connectToHost(cap_host, static_cast<quint16>(cap_port));
            const bool ok = socket.waitForConnected(3000);
            const QString msg = ok ? QString("Reachable: %1").arg(cap_probe.isEmpty() ? cap_host : cap_probe)
                                   : QString("Unreachable: %1").arg(socket.errorString());
            if (ok)
                socket.disconnectFromHost();

            QMetaObject::invokeMethod(
                qApp,
                [self, conn_id, ok, msg]() {
                    if (!self)
                        return;
                    self->live_status_cache_[conn_id] = {ok, msg};
                    self->update_connection_status_cell(conn_id, ok, msg);
                    self->update_detail_panel();
                },
                Qt::QueuedConnection);
        });
    }
}

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

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap DataSourcesScreen::save_state() const {
    return {
        {"connector_id", selected_connector_id_},
        {"category", static_cast<int>(active_category_)},
    };
}

void DataSourcesScreen::restore_state(const QVariantMap& state) {
    const QString id = state.value("connector_id").toString();
    if (!id.isEmpty())
        select_connector_by_id(id);
}

} // namespace fincept::screens::datasources
