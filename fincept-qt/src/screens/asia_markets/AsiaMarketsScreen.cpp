#include "screens/asia_markets/AsiaMarketsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

// ── Screen-level stylesheet ─────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle =
    QStringLiteral("#asiaScreen { background: %1; }"

                   "#asiaHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#asiaHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#asiaHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"

                   "#asiaRegionBtn { background: transparent; color: %5; border: 1px solid %8; "
                   "  font-size: 9px; font-weight: 700; padding: 3px 10px; }"
                   "#asiaRegionBtn:hover { color: %4; border-color: %9; }"
                   "#asiaRegionBtn[active=\"true\"] { background: %3; color: %1; border-color: %3; }"

                   "#asiaCatBar { background: %1; border-bottom: 1px solid %8; }"
                   "#asiaCatBtn { background: transparent; color: %5; border: none; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 14px; border-bottom: 2px solid transparent; }"
                   "#asiaCatBtn:hover { color: %4; }"
                   "#asiaCatBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"

                   "#asiaLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#asiaSearchInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "#asiaSearchInput:focus { border-color: %9; }"
                   "#asiaSymbolInput { background: %1; color: %13; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; font-weight: 700; }"
                   "#asiaSymbolInput:focus { border-color: %9; }"
                   "#asiaEndpointCount { color: %5; font-size: 9px; font-weight: 700; background: transparent; }"

                   "#asiaEndpointList { background: %1; border: none; outline: none; font-size: 11px; }"
                   "#asiaEndpointList::item { color: %5; padding: 4px 8px; border-bottom: 1px solid %8; }"
                   "#asiaEndpointList::item:hover { color: %4; background: %10; }"
                   "#asiaEndpointList::item:selected { color: %3; background: rgba(217,119,6,0.1); "
                   "  border-left: 2px solid %3; }"

                   "#asiaDataPanel { background: %1; }"
                   "#asiaToolbar { background: %2; border-bottom: 1px solid %8; }"
                   "#asiaDataStatus { color: %5; font-size: 11px; background: transparent; }"
                   "#asiaRecordCount { color: %13; font-size: 9px; font-weight: 700; background: transparent; }"

                   "#asiaExecBtn { background: %3; color: %1; border: none; padding: 4px 14px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#asiaExecBtn:hover { background: #FF8800; }"
                   "#asiaExecBtn:disabled { background: %11; color: %12; }"

                   "#asiaViewToggle { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                   "#asiaViewToggle:hover { color: %4; }"

                   "#asiaRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                   "#asiaRefreshBtn:hover { color: %4; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QTableWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "#asiaJsonView { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#asiaErrorPanel { background: rgba(220,38,38,0.1); border: 1px solid %14; padding: 8px 12px; }"
                   "#asiaErrorText { color: %14; font-size: 10px; background: transparent; }"

                   "#asiaStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#asiaStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#asiaStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "#asiaEmptyState { color: %12; font-size: 13px; background: transparent; }"
                   "#asiaParamLabel { color: %5; font-size: 9px; font-weight: 700; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { background: %1; height: 6px; }"
                   "QScrollBar::handle:horizontal { background: %8; min-width: 20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }")
        .arg(colors::BG_BASE)        // %1
        .arg(colors::BG_RAISED)      // %2
        .arg(colors::AMBER)          // %3
        .arg(colors::TEXT_PRIMARY)   // %4
        .arg(colors::TEXT_SECONDARY) // %5
        .arg(colors::POSITIVE)       // %6  (unused but reserved)
        .arg(colors::BG_SURFACE)     // %7
        .arg(colors::BORDER_DIM)     // %8
        .arg(colors::BORDER_BRIGHT)  // %9
        .arg(colors::BG_HOVER)       // %10
        .arg(colors::AMBER_DIM)      // %11
        .arg(colors::TEXT_DIM)       // %12
        .arg(colors::CYAN)           // %13
        .arg(colors::NEGATIVE)       // %14
    ;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;

// ── Category definitions ────────────────────────────────────────────────────

static QList<StockCategory> build_categories() {
    return {
        {"realtime", "REALTIME", "akshare_stocks_realtime.py", "#00D66F", 49},
        {"historical", "HISTORICAL", "akshare_stocks_historical.py", "#0088FF", 52},
        {"financial", "FINANCIAL", "akshare_stocks_financial.py", "#9D4EDD", 45},
        {"holders", "HOLDERS", "akshare_stocks_holders.py", "#FFD700", 49},
        {"fund_flow", "FUND FLOW", "akshare_stocks_funds.py", "#FF3B3B", 56},
        {"boards", "BOARDS", "akshare_stocks_board.py", "#00E5FF", 49},
        {"margin", "MARGIN/HSGT", "akshare_stocks_margin.py", "#FF8800", 46},
        {"hot_news", "HOT & NEWS", "akshare_stocks_hot.py", "#FF6B6B", 55},
    };
}

// ── Constructor ─────────────────────────────────────────────────────────────

AsiaMarketsScreen::AsiaMarketsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("asiaScreen");
    setStyleSheet(kStyle);
    categories_ = build_categories();
    setup_ui();
    LOG_INFO("AsiaMarkets", "Screen constructed");
}

// ── Show event — auto-load first category on first visit ────────────────────

void AsiaMarketsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (first_show_) {
        first_show_ = false;
        on_category_changed(0);
    }
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void AsiaMarketsScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(create_header());
    root->addWidget(create_category_bar());

    // Main content: splitter
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    auto* left = create_left_panel();
    left->setMinimumWidth(240);
    left->setMaximumWidth(360);

    auto* right = create_data_panel();

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 800});

    root->addWidget(splitter, 1);
    root->addWidget(create_status_bar());
}

QWidget* AsiaMarketsScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("asiaHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    // Title
    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("ASIA MARKETS TERMINAL");
    title->setObjectName("asiaHeaderTitle");
    auto* sub = new QLabel("398+ STOCK ENDPOINTS | CN A/B, HK, US");
    sub->setObjectName("asiaHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);
    hl->addLayout(title_col);
    hl->addStretch(1);

    // Region buttons
    const QStringList region_labels = {"CN A", "CN B", "HK", "US"};
    for (int i = 0; i < regions_.size(); ++i) {
        auto* btn = new QPushButton(region_labels[i]);
        btn->setObjectName("asiaRegionBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_region_changed(i); });
        hl->addWidget(btn);
        region_btns_.append(btn);
    }

    return bar;
}

QWidget* AsiaMarketsScreen::create_category_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("asiaCatBar");
    bar->setFixedHeight(32);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(0);

    for (int i = 0; i < categories_.size(); ++i) {
        auto* btn = new QPushButton(categories_[i].name);
        btn->setObjectName("asiaCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", i == 0);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_changed(i); });
        hl->addWidget(btn);
        cat_btns_.append(btn);
    }

    hl->addStretch(1);
    return bar;
}

QWidget* AsiaMarketsScreen::create_left_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("asiaLeftPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Search + symbol row
    auto* input_area = new QWidget(this);
    auto* il = new QVBoxLayout(input_area);
    il->setContentsMargins(8, 6, 8, 6);
    il->setSpacing(6);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("asiaSearchInput");
    search_input_->setPlaceholderText("Search endpoints...");
    connect(search_input_, &QLineEdit::textChanged, this, &AsiaMarketsScreen::on_search_changed);

    // Symbol input row
    auto* sym_row = new QWidget(this);
    auto* srl = new QHBoxLayout(sym_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);
    auto* sym_label = new QLabel("SYMBOL");
    sym_label->setObjectName("asiaParamLabel");
    symbol_input_ = new QLineEdit;
    symbol_input_->setObjectName("asiaSymbolInput");
    symbol_input_->setPlaceholderText("e.g. 000001");
    symbol_input_->setMaxLength(20);
    srl->addWidget(sym_label);
    srl->addWidget(symbol_input_, 1);

    endpoint_count_label_ = new QLabel("0 endpoints");
    endpoint_count_label_->setObjectName("asiaEndpointCount");

    il->addWidget(search_input_);
    il->addWidget(sym_row);
    il->addWidget(endpoint_count_label_);
    vl->addWidget(input_area);

    // Endpoint list
    endpoint_list_ = new QListWidget;
    endpoint_list_->setObjectName("asiaEndpointList");
    connect(endpoint_list_, &QListWidget::itemClicked, this, &AsiaMarketsScreen::on_endpoint_clicked);
    vl->addWidget(endpoint_list_, 1);

    return panel;
}

QWidget* AsiaMarketsScreen::create_data_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("asiaDataPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("asiaToolbar");
    toolbar->setFixedHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    tbl->setSpacing(8);

    data_status_ = new QLabel("Select a category to begin");
    data_status_->setObjectName("asiaDataStatus");

    record_count_ = new QLabel;
    record_count_->setObjectName("asiaRecordCount");
    record_count_->hide();

    exec_btn_ = new QPushButton("EXECUTE");
    exec_btn_->setObjectName("asiaExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setFixedWidth(70);
    exec_btn_->setEnabled(false);
    connect(exec_btn_, &QPushButton::clicked, this, &AsiaMarketsScreen::on_execute);

    view_toggle_btn_ = new QPushButton("JSON");
    view_toggle_btn_->setObjectName("asiaViewToggle");
    view_toggle_btn_->setCursor(Qt::PointingHandCursor);
    view_toggle_btn_->setFixedWidth(50);
    connect(view_toggle_btn_, &QPushButton::clicked, this, &AsiaMarketsScreen::on_view_toggle);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setObjectName("asiaRefreshBtn");
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setFixedWidth(65);
    connect(refresh_btn, &QPushButton::clicked, this, &AsiaMarketsScreen::on_execute);

    tbl->addWidget(data_status_);
    tbl->addWidget(record_count_);
    tbl->addStretch(1);
    tbl->addWidget(exec_btn_);
    tbl->addWidget(view_toggle_btn_);
    tbl->addWidget(refresh_btn);
    vl->addWidget(toolbar);

    // View stack
    view_stack_ = new QStackedWidget;

    data_table_ = new QTableWidget;
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->verticalHeader()->setVisible(false);
    data_table_->horizontalHeader()->setStretchLastSection(true);
    data_table_->horizontalHeader()->setSortIndicatorShown(true);
    data_table_->setSortingEnabled(true);
    data_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    json_view_ = new QTextEdit;
    json_view_->setObjectName("asiaJsonView");
    json_view_->setReadOnly(true);

    view_stack_->addWidget(data_table_);
    view_stack_->addWidget(json_view_);
    vl->addWidget(view_stack_, 1);

    return panel;
}

QWidget* AsiaMarketsScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("asiaStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("ASIA MARKETS");
    left->setObjectName("asiaStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_category_ = new QLabel("CATEGORY: REALTIME");
    status_category_->setObjectName("asiaStatusText");
    hl->addWidget(status_category_);

    hl->addSpacing(16);
    status_region_ = new QLabel("REGION: CN_A");
    status_region_->setObjectName("asiaStatusHighlight");
    hl->addWidget(status_region_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AsiaMarketsScreen::on_category_changed(int index) {
    if (index == active_category_ && endpoint_list_->count() > 0)
        return;
    active_category_ = index;
    active_endpoint_.clear();
    exec_btn_->setEnabled(false);

    // Update button states
    for (int i = 0; i < cat_btns_.size(); ++i) {
        cat_btns_[i]->setProperty("active", i == index);
        cat_btns_[i]->style()->unpolish(cat_btns_[i]);
        cat_btns_[i]->style()->polish(cat_btns_[i]);
    }

    status_category_->setText("CATEGORY: " + categories_[index].name);
    search_input_->clear();

    LOG_INFO("AsiaMarkets", "Category: " + categories_[index].name);
    ScreenStateManager::instance().notify_changed(this);

    if (endpoint_cache_.contains(categories_[index].script)) {
        populate_endpoint_list(endpoint_cache_[categories_[index].script]);
    } else {
        load_endpoints(index);
    }
}

void AsiaMarketsScreen::on_region_changed(int index) {
    active_region_ = index;

    for (int i = 0; i < region_btns_.size(); ++i) {
        region_btns_[i]->setProperty("active", i == index);
        region_btns_[i]->style()->unpolish(region_btns_[i]);
        region_btns_[i]->style()->polish(region_btns_[i]);
    }

    status_region_->setText("REGION: " + regions_[index]);
    LOG_INFO("AsiaMarkets", "Region: " + regions_[index]);
    ScreenStateManager::instance().notify_changed(this);
}

void AsiaMarketsScreen::on_endpoint_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    QString ep = item->data(Qt::UserRole).toString();
    if (ep.isEmpty())
        return; // category header
    active_endpoint_ = ep;
    exec_btn_->setEnabled(true);

    // Auto-execute
    on_execute();
}

void AsiaMarketsScreen::on_search_changed(const QString& text) {
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        bool is_header = item->data(Qt::UserRole).toString().isEmpty();
        if (is_header) {
            item->setHidden(false); // always show headers (they'll be hidden if all children hidden)
            continue;
        }
        bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive) ||
                     item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void AsiaMarketsScreen::on_execute() {
    if (loading_ || active_endpoint_.isEmpty())
        return;

    QStringList extra;
    QString sym = symbol_input_->text().trimmed();
    if (!sym.isEmpty()) {
        extra << sym;
    }

    execute_query(active_endpoint_, extra);
}

void AsiaMarketsScreen::on_view_toggle() {
    is_table_view_ = !is_table_view_;
    view_stack_->setCurrentIndex(is_table_view_ ? 0 : 1);
    view_toggle_btn_->setText(is_table_view_ ? "JSON" : "TABLE");
    // Populate JSON view lazily on first switch
    if (!is_table_view_ && json_view_->toPlainText().isEmpty() && !last_data_.isEmpty())
        display_json(last_data_);
}

// ── Data loading ────────────────────────────────────────────────────────────

void AsiaMarketsScreen::load_endpoints(int cat_index) {
    const QString script = categories_[cat_index].script;

    // Check CacheManager first — endpoint lists are static, cache 1 hour
    const QString cache_key = "asia:endpoints:" + script;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            auto obj = doc.object();
            endpoint_cache_[script] = obj;
            populate_endpoint_list(obj);
            return;
        }
    }

    set_loading(true);
    endpoint_list_->clear();
    data_status_->setText("Loading endpoints...");

    QPointer<AsiaMarketsScreen> self = this;

    python::PythonRunner::instance().run(
        script, {"get_all_endpoints"}, [self, script, cache_key](const python::PythonResult& result) {
            if (!self)
                return;
            self->set_loading(false);

            if (!result.success) {
                self->data_status_->setText("Failed to load endpoints");
                LOG_ERROR("AsiaMarkets", "Endpoint load failed: " + result.error);
                return;
            }

            QString json_str = python::extract_json(result.output);
            if (json_str.isEmpty()) {
                self->data_status_->setText("No endpoint data");
                return;
            }

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull() || !doc.isObject()) {
                self->data_status_->setText("Invalid endpoint data");
                return;
            }

            auto obj = doc.object();
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))), 60 * 60,
                "asia_markets");
            self->endpoint_cache_[script] = obj;
            self->populate_endpoint_list(obj);
        });
}

void AsiaMarketsScreen::populate_endpoint_list(const QJsonObject& result) {
    endpoint_list_->clear();

    QJsonObject data_obj;
    if (result.contains("data") && result["data"].isObject()) {
        data_obj = result["data"].toObject();
    } else {
        data_obj = result;
    }

    QStringList all_endpoints;

    if (data_obj.contains("categories") && data_obj["categories"].isObject()) {
        auto categories = data_obj["categories"].toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            // Category header
            auto* header = new QListWidgetItem(it.key().toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(QColor(colors::AMBER()));
            auto f = header->font();
            f.setBold(true);
            f.setPointSize(8);
            header->setFont(f);
            endpoint_list_->addItem(header);

            if (it.value().isArray()) {
                for (auto ep : it.value().toArray()) {
                    QString name = ep.toString();
                    auto* item = new QListWidgetItem("  " + name);
                    item->setData(Qt::UserRole, name);
                    endpoint_list_->addItem(item);
                    all_endpoints << name;
                }
            }
        }
    } else if (data_obj.contains("available_endpoints") && data_obj["available_endpoints"].isArray()) {
        for (auto ep : data_obj["available_endpoints"].toArray()) {
            QString name = ep.toString();
            auto* item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, name);
            endpoint_list_->addItem(item);
            all_endpoints << name;
        }
    }

    endpoint_count_label_->setText(QString::number(all_endpoints.size()) + " endpoints");
    data_status_->setText("Select an endpoint");
    LOG_INFO("AsiaMarkets", "Loaded " + QString::number(all_endpoints.size()) + " endpoints");

    // Auto-select: prefer a _sina endpoint (reliable), fallback to first real endpoint
    QListWidgetItem* first_real = nullptr;
    QListWidgetItem* first_sina = nullptr;
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        QString ep = item->data(Qt::UserRole).toString();
        if (ep.isEmpty())
            continue;
        if (!first_real)
            first_real = item;
        if (!first_sina && ep.contains("sina", Qt::CaseInsensitive)) {
            first_sina = item;
            break;
        }
    }
    QListWidgetItem* to_select = first_sina ? first_sina : first_real;
    if (to_select) {
        endpoint_list_->setCurrentItem(to_select);
        on_endpoint_clicked(to_select);
    }
}

void AsiaMarketsScreen::execute_query(const QString& endpoint, const QStringList& extra_args) {
    const QString script = categories_[active_category_].script;
    QStringList args;
    args << endpoint << extra_args;

    // Cache market data for 2 minutes
    const QString cache_key = "asia:query:" + script + ":" + args.join(":");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isArray() && !doc.array().isEmpty()) {
            const QJsonArray data_array = doc.array();
            record_count_->setText(QString::number(data_array.size()) + " records");
            record_count_->show();
            data_status_->setText(endpoint + " (cached)");
            last_data_ = data_array;
            json_view_->clear();
            display_table(data_array);
            if (!is_table_view_)
                display_json(data_array);
            return;
        }
    }

    set_loading(true);
    data_status_->setText("Querying " + endpoint + "...");
    record_count_->hide();

    QPointer<AsiaMarketsScreen> self = this;

    python::PythonRunner::instance().run(script, args, [self, endpoint, cache_key](const python::PythonResult& result) {
        if (!self)
            return;
        self->set_loading(false);

        if (!result.success) {
            self->display_error(result.error.isEmpty() ? "Query failed" : result.error);
            return;
        }

        QString json_str = python::extract_json(result.output);
        if (json_str.isEmpty()) {
            self->display_error("No data from " + endpoint);
            return;
        }

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
        if (doc.isNull()) {
            self->display_error("JSON parse error: " + err.errorString());
            return;
        }

        auto obj = doc.isObject() ? doc.object() : QJsonObject();

        if (obj.contains("error")) {
            self->display_error(obj["error"].toString());
            return;
        }
        if (obj.contains("success") && !obj["success"].toBool()) {
            self->display_error(obj.value("error").toString("Query returned failure"));
            return;
        }

        // Extract data array
        QJsonArray data_array;
        if (obj.contains("data")) {
            if (obj["data"].isArray()) {
                data_array = obj["data"].toArray();
            } else if (obj["data"].isObject()) {
                data_array.append(obj["data"]);
            } else {
                QJsonObject wrapper;
                wrapper["value"] = obj["data"];
                data_array.append(wrapper);
            }
        } else if (doc.isArray()) {
            data_array = doc.array();
        } else {
            data_array.append(obj);
        }

        int count = data_array.size();
        self->record_count_->setText(QString::number(count) + " records");
        self->record_count_->show();
        self->data_status_->setText(endpoint);

        if (!data_array.isEmpty()) {
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(data_array).toJson(QJsonDocument::Compact))),
                2 * 60, "asia_markets");
        }

        self->last_data_ = data_array;
        self->json_view_->clear();
        self->display_table(data_array);
        if (!self->is_table_view_)
            self->display_json(data_array);

        LOG_INFO("AsiaMarkets", endpoint + ": " + QString::number(count) + " records");
    });
}

// ── Display ─────────────────────────────────────────────────────────────────

void AsiaMarketsScreen::display_table(const QJsonArray& data) {
    if (data.isEmpty()) {
        data_status_->setText("No data returned");
        return;
    }

    // Columns from first row
    QStringList columns;
    auto first = data[0].toObject();
    for (auto it = first.begin(); it != first.end(); ++it)
        columns << it.key();

    int max_rows = qMin(data.size(), 2000);

    // Block all signals + updates during population
    data_table_->setSortingEnabled(false);
    data_table_->setUpdatesEnabled(false);
    data_table_->clearContents();
    data_table_->setRowCount(max_rows);
    data_table_->setColumnCount(columns.size());
    data_table_->setHorizontalHeaderLabels(columns);

    // Pre-build color lookup to avoid repeated QColor construction
    const QColor col_pos(colors::CYAN());
    const QColor col_neg(colors::NEGATIVE());

    for (int row = 0; row < max_rows; ++row) {
        auto obj = data[row].toObject();
        for (int col = 0; col < columns.size(); ++col) {
            auto val = obj.value(columns[col]);
            QString text;
            bool is_neg = false;
            if (val.isDouble()) {
                double v = val.toDouble();
                text = QString::number(v, 'g', 8);
                is_neg = v < 0;
            } else if (val.isNull()) {
                text = "--";
            } else {
                text = val.toVariant().toString();
            }

            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (val.isDouble())
                item->setForeground(is_neg ? col_neg : col_pos);
            data_table_->setItem(row, col, item);
        }
    }

    // Fixed column width — much faster than resizeColumnsToContents
    data_table_->horizontalHeader()->setDefaultSectionSize(110);
    data_table_->verticalHeader()->setDefaultSectionSize(22);
    data_table_->setUpdatesEnabled(true);
    data_table_->setSortingEnabled(true);

    if (data.size() > max_rows)
        data_status_->setText(QString("Showing %1 of %2").arg(max_rows).arg(data.size()));
}

void AsiaMarketsScreen::display_json(const QJsonArray& data) {
    QJsonDocument doc(data);
    json_view_->setPlainText(doc.toJson(QJsonDocument::Indented));
}

void AsiaMarketsScreen::display_error(const QString& error) {
    data_status_->setText("Error");
    record_count_->hide();

    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    QJsonObject err_obj;
    err_obj["error"] = error;
    json_view_->setPlainText(QJsonDocument(err_obj).toJson(QJsonDocument::Indented));

    LOG_ERROR("AsiaMarkets", error);
}

void AsiaMarketsScreen::set_loading(bool loading) {
    loading_ = loading;
    exec_btn_->setEnabled(!loading && !active_endpoint_.isEmpty());
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AsiaMarketsScreen::save_state() const {
    return {
        {"category", active_category_},
        {"region", active_region_},
        {"endpoint", active_endpoint_},
    };
}

void AsiaMarketsScreen::restore_state(const QVariantMap& state) {
    const int region = state.value("region", 0).toInt();
    if (region != active_region_)
        on_region_changed(region);

    const int cat = state.value("category", 0).toInt();
    if (cat != active_category_)
        on_category_changed(cat);
    // active_endpoint_ is restored via on_category_changed populating the list;
    // exact endpoint row selection would need list rebuilding — skip for simplicity.
}

} // namespace fincept::screens
