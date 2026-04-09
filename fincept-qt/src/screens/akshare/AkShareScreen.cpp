#include "screens/akshare/AkShareScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPointer>
#include <QScrollArea>
#include <QVBoxLayout>

// ── Screen-level stylesheet ─────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

static const QString kStyle =
    QStringLiteral("#akScreen { background: %1; }"

                   "#akHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#akHeaderTitle { color: %4; font-size: 12px; font-weight: 700; background: transparent; }"
                   "#akHeaderSub { color: %5; font-size: 9px; letter-spacing: 0.5px; background: transparent; }"
                   "#akHeaderBadge { color: %6; font-size: 8px; font-weight: 700; "
                   "  background: rgba(22,163,74,0.2); padding: 2px 6px; }"

                   "#akSourceGrid { background: %1; }"
                   "#akSourceBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  font-size: 9px; font-weight: 700; padding: 6px 4px; text-align: center; }"
                   "#akSourceBtn:hover { color: %4; border-color: %9; }"
                   "#akSourceBtn[active=\"true\"] { background: rgba(217,119,6,0.15); "
                   "  color: %3; border-color: %3; }"

                   "#akEndpointPanel { background: %7; border-right: 1px solid %8; }"
                   "#akSearchInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: 11px; }"
                   "#akSearchInput:focus { border-color: %9; }"
                   "#akEndpointCount { color: %5; font-size: 9px; font-weight: 700; background: transparent; }"
                   "#akEndpointList { background: %1; border: none; outline: none; font-size: 11px; }"
                   "#akEndpointList::item { color: %5; padding: 4px 8px; border-bottom: 1px solid %8; }"
                   "#akEndpointList::item:hover { color: %4; background: %10; }"
                   "#akEndpointList::item:selected { color: %3; background: rgba(217,119,6,0.1); "
                   "  border-left: 2px solid %3; }"

                   "#akParamsPanel { background: %7; border-bottom: 1px solid %8; }"
                   "#akParamLabel { color: %5; font-size: 9px; font-weight: 700; background: transparent; }"
                   "#akParamInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 3px 6px; font-size: 11px; }"
                   "#akParamInput:focus { border-color: %9; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 3px 6px; font-size: 11px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; }"

                   "#akExecBtn { background: %3; color: %1; border: none; padding: 4px 16px; "
                   "  font-size: 9px; font-weight: 700; }"
                   "#akExecBtn:hover { background: #FF8800; }"
                   "#akExecBtn:disabled { background: %11; color: %12; }"

                   "#akViewToggle { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                   "#akViewToggle:hover { color: %4; }"

                   "#akRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: 9px; font-weight: 700; }"
                   "#akRefreshBtn:hover { color: %4; }"
                   "#akRefreshBtn:disabled { color: %12; }"

                   "#akDataPanel { background: %1; }"
                   "#akDataStatus { color: %5; font-size: 11px; background: transparent; }"
                   "#akRecordCount { color: %13; font-size: 9px; font-weight: 700; background: transparent; }"

                   "QTableWidget { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: 11px; }"
                   "QTableWidget::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QTableWidget::item:selected { background: rgba(217,119,6,0.1); color: %3; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: 10px; font-weight: 700; }"

                   "#akJsonView { background: %1; color: %13; border: none; font-size: 11px; }"

                   "#akErrorPanel { background: rgba(220,38,38,0.1); border: 1px solid %14; }"
                   "#akErrorText { color: %14; font-size: 10px; background: transparent; }"
                   "#akErrorLabel { color: %14; font-size: 9px; font-weight: 700; background: transparent; }"

                   "#akStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#akStatusText { color: %5; font-size: 9px; background: transparent; }"
                   "#akStatusHighlight { color: %13; font-size: 9px; background: transparent; }"

                   "#akEmptyState { color: %12; font-size: 13px; background: transparent; }"

                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { background: %1; height: 6px; }"
                   "QScrollBar::handle:horizontal { background: %8; min-width: 20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

                   "QSplitter::handle { background: %8; }")
        .arg(colors::BG_BASE)        // %1
        .arg(colors::BG_RAISED)      // %2
        .arg(colors::AMBER)          // %3
        .arg(colors::TEXT_PRIMARY)   // %4
        .arg(colors::TEXT_SECONDARY) // %5
        .arg(colors::POSITIVE)       // %6
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

// ── Data source definitions ─────────────────────────────────────────────────

static QList<AkShareSource> build_sources() {
    return {
        {"stocks_realtime", "Stocks: Realtime", "akshare_stocks_realtime.py", "A"},
        {"stocks_historical", "Stocks: Historical", "akshare_stocks_historical.py", "H"},
        {"stocks_financial", "Stocks: Financial", "akshare_stocks_financial.py", "F"},
        {"stocks_holders", "Stocks: Shareholders", "akshare_stocks_holders.py", "S"},
        {"stocks_funds", "Stocks: Fund Flows", "akshare_stocks_funds.py", "$"},
        {"stocks_board", "Stocks: Boards", "akshare_stocks_board.py", "B"},
        {"stocks_margin", "Stocks: Margin", "akshare_stocks_margin.py", "M"},
        {"stocks_hot", "Stocks: Hot & News", "akshare_stocks_hot.py", "N"},
        {"index", "Index Data", "akshare_index.py", "I"},
        {"futures", "Futures", "akshare_futures.py", "Ft"},
        {"bonds", "Bonds", "akshare_bonds.py", "Bd"},
        {"currency", "Currency / Forex", "akshare_currency.py", "FX"},
        {"energy", "Energy", "akshare_energy.py", "En"},
        {"crypto", "Crypto", "akshare_crypto.py", "Cr"},
        {"news", "News", "akshare_news.py", "Nw"},
        {"reits", "REITs", "akshare_reits.py", "RE"},
        {"data", "Market Data", "akshare_data.py", "Mk"},
        {"alternative", "Alternative Data", "akshare_alternative.py", "Alt"},
        {"analysis", "Market Analysis", "akshare_analysis.py", "An"},
        {"derivatives", "Derivatives", "akshare_derivatives.py", "Dv"},
        {"economics_china", "China Economics", "akshare_economics_china.py", "CN"},
        {"economics_global", "Global Economics", "akshare_economics_global.py", "GL"},
        {"funds_expanded", "Funds Expanded", "akshare_funds_expanded.py", "Fd"},
        {"company_info", "Company Info", "akshare_company_info.py", "Co"},
        {"macro", "Macro Global", "akshare_macro.py", "Mc"},
        {"misc", "Miscellaneous", "akshare_misc.py", ".."},
    };
}

// ── Constructor ─────────────────────────────────────────────────────────────

AkShareScreen::AkShareScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("akScreen");
    setStyleSheet(kStyle);
    sources_ = build_sources();
    setup_ui();
    LOG_INFO("AkShare", "Screen constructed");
}

// ── UI Setup ────────────────────────────────────────────────────────────────

void AkShareScreen::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    root->addWidget(create_header());

    // Source grid
    root->addWidget(create_source_grid());

    // Main content: splitter with endpoint list | data panel
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName("akSplitter");
    splitter->setHandleWidth(1);

    // Left: endpoint panel
    auto* left = new QWidget(this);
    auto* left_vl = new QVBoxLayout(left);
    left_vl->setContentsMargins(0, 0, 0, 0);
    left_vl->setSpacing(0);
    left_vl->addWidget(create_endpoint_panel());
    left->setMinimumWidth(220);
    left->setMaximumWidth(350);

    // Right: params + data
    auto* right = new QWidget(this);
    auto* right_vl = new QVBoxLayout(right);
    right_vl->setContentsMargins(0, 0, 0, 0);
    right_vl->setSpacing(0);
    right_vl->addWidget(create_params_panel());
    right_vl->addWidget(create_data_panel(), 1);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 800});

    root->addWidget(splitter, 1);

    // Status bar
    root->addWidget(create_status_bar());
}

QWidget* AkShareScreen::create_header() {
    auto* bar = new QWidget(this);
    bar->setObjectName("akHeader");
    bar->setFixedHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    auto* title = new QLabel("AKSHARE DATA EXPLORER");
    title->setObjectName("akHeaderTitle");
    auto* sub = new QLabel("1000+ CHINESE & GLOBAL FINANCIAL DATA ENDPOINTS");
    sub->setObjectName("akHeaderSub");
    title_col->addWidget(title);
    title_col->addWidget(sub);
    hl->addLayout(title_col);
    hl->addStretch(1);

    auto* badge = new QLabel("FREE API");
    badge->setObjectName("akHeaderBadge");
    hl->addWidget(badge);

    return bar;
}

QWidget* AkShareScreen::create_source_grid() {
    auto* container = new QWidget(this);
    container->setObjectName("akSourceGrid");

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(80);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* inner = new QWidget(this);
    auto* grid = new QGridLayout(inner);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setSpacing(4);

    int cols = 13;
    for (int i = 0; i < sources_.size(); ++i) {
        auto* btn = new QPushButton(sources_[i].icon + " " + sources_[i].name);
        btn->setObjectName("akSourceBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_source_clicked(i); });
        grid->addWidget(btn, i / cols, i % cols);
        source_btns_.append(btn);
    }

    scroll->setWidget(inner);

    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->addWidget(scroll);

    return container;
}

QWidget* AkShareScreen::create_endpoint_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("akEndpointPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Search bar
    auto* search_bar = new QWidget(this);
    auto* sbl = new QHBoxLayout(search_bar);
    sbl->setContentsMargins(8, 6, 8, 6);
    sbl->setSpacing(6);

    search_input_ = new QLineEdit;
    search_input_->setObjectName("akSearchInput");
    search_input_->setPlaceholderText("Search endpoints...");
    connect(search_input_, &QLineEdit::textChanged, this, &AkShareScreen::on_search_changed);

    endpoint_count_ = new QLabel("0 endpoints");
    endpoint_count_->setObjectName("akEndpointCount");

    sbl->addWidget(search_input_, 1);
    sbl->addWidget(endpoint_count_);
    vl->addWidget(search_bar);

    // Endpoint list
    endpoint_list_ = new QListWidget;
    endpoint_list_->setObjectName("akEndpointList");
    connect(endpoint_list_, &QListWidget::itemClicked, this, &AkShareScreen::on_endpoint_clicked);
    vl->addWidget(endpoint_list_, 1);

    // Empty state
    auto* empty = new QLabel("Select a data source above\nto load available endpoints");
    empty->setObjectName("akEmptyState");
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    vl->addWidget(empty);

    return panel;
}

QWidget* AkShareScreen::create_params_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("akParamsPanel");
    panel->setFixedHeight(38);

    auto* hl = new QHBoxLayout(panel);
    hl->setContentsMargins(12, 4, 12, 4);
    hl->setSpacing(8);

    // Symbol
    auto* sym_label = new QLabel("SYMBOL");
    sym_label->setObjectName("akParamLabel");
    param_symbol_ = new QLineEdit("000001");
    param_symbol_->setObjectName("akParamInput");
    param_symbol_->setFixedWidth(90);

    // Start date
    auto* start_label = new QLabel("START");
    start_label->setObjectName("akParamLabel");
    param_start_ = new QLineEdit;
    param_start_->setObjectName("akParamInput");
    param_start_->setPlaceholderText("YYYY-MM-DD");
    param_start_->setFixedWidth(100);

    // End date
    auto* end_label = new QLabel("END");
    end_label->setObjectName("akParamLabel");
    param_end_ = new QLineEdit;
    param_end_->setObjectName("akParamInput");
    param_end_->setPlaceholderText("YYYY-MM-DD");
    param_end_->setFixedWidth(100);

    // Period
    auto* period_label = new QLabel("PERIOD");
    period_label->setObjectName("akParamLabel");
    param_period_ = new QComboBox;
    param_period_->addItems({"daily", "weekly", "monthly"});
    param_period_->setFixedWidth(90);

    // Execute button
    exec_btn_ = new QPushButton("EXECUTE");
    exec_btn_->setObjectName("akExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setFixedWidth(80);
    connect(exec_btn_, &QPushButton::clicked, this, &AkShareScreen::on_execute);

    hl->addWidget(sym_label);
    hl->addWidget(param_symbol_);
    hl->addSpacing(4);
    hl->addWidget(start_label);
    hl->addWidget(param_start_);
    hl->addSpacing(4);
    hl->addWidget(end_label);
    hl->addWidget(param_end_);
    hl->addSpacing(4);
    hl->addWidget(period_label);
    hl->addWidget(param_period_);
    hl->addStretch(1);
    hl->addWidget(exec_btn_);

    return panel;
}

QWidget* AkShareScreen::create_data_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("akDataPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar row
    auto* toolbar = new QWidget(this);
    toolbar->setFixedHeight(30);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    tbl->setSpacing(8);

    data_status_ = new QLabel("Ready");
    data_status_->setObjectName("akDataStatus");

    record_count_ = new QLabel;
    record_count_->setObjectName("akRecordCount");
    record_count_->hide();

    view_toggle_btn_ = new QPushButton("JSON");
    view_toggle_btn_->setObjectName("akViewToggle");
    view_toggle_btn_->setCursor(Qt::PointingHandCursor);
    view_toggle_btn_->setFixedWidth(50);
    connect(view_toggle_btn_, &QPushButton::clicked, this, &AkShareScreen::on_view_toggle);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setObjectName("akRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setFixedWidth(70);
    refresh_btn_->setEnabled(false);
    connect(refresh_btn_, &QPushButton::clicked, this, &AkShareScreen::on_refresh);

    tbl->addWidget(data_status_);
    tbl->addWidget(record_count_);
    tbl->addStretch(1);
    tbl->addWidget(view_toggle_btn_);
    tbl->addWidget(refresh_btn_);
    vl->addWidget(toolbar);

    // View stack: table | json
    view_stack_ = new QStackedWidget;

    data_table_ = new QTableWidget;
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->setAlternatingRowColors(false);
    data_table_->verticalHeader()->setVisible(false);
    data_table_->horizontalHeader()->setStretchLastSection(true);
    data_table_->horizontalHeader()->setSortIndicatorShown(true);
    data_table_->setSortingEnabled(true);
    data_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    json_view_ = new QTextEdit;
    json_view_->setObjectName("akJsonView");
    json_view_->setReadOnly(true);

    view_stack_->addWidget(data_table_);
    view_stack_->addWidget(json_view_);
    vl->addWidget(view_stack_, 1);

    return panel;
}

QWidget* AkShareScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("akStatusBar");
    bar->setFixedHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* left = new QLabel("AKSHARE DATA");
    left->setObjectName("akStatusText");
    hl->addWidget(left);
    hl->addStretch(1);

    status_source_ = new QLabel("SOURCE: --");
    status_source_->setObjectName("akStatusText");
    hl->addWidget(status_source_);

    hl->addSpacing(16);
    status_endpoint_ = new QLabel;
    status_endpoint_->setObjectName("akStatusHighlight");
    hl->addWidget(status_endpoint_);

    return bar;
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AkShareScreen::on_source_clicked(int index) {
    if (index == active_source_)
        return;
    active_source_ = index;
    active_endpoint_.clear();

    // Update button states
    for (int i = 0; i < source_btns_.size(); ++i) {
        source_btns_[i]->setProperty("active", i == index);
        source_btns_[i]->style()->unpolish(source_btns_[i]);
        source_btns_[i]->style()->polish(source_btns_[i]);
    }

    status_source_->setText("SOURCE: " + sources_[index].name.toUpper());
    status_endpoint_->clear();
    search_input_->clear();

    LOG_INFO("AkShare", "Source selected: " + sources_[index].name);

    // Check cache first
    if (endpoint_cache_.contains(sources_[index].script)) {
        populate_endpoint_list(endpoint_cache_[sources_[index].script]);
    } else {
        load_endpoints(sources_[index]);
    }
}

void AkShareScreen::on_endpoint_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    active_endpoint_ = item->data(Qt::UserRole).toString();
    status_endpoint_->setText(active_endpoint_);
    refresh_btn_->setEnabled(true);

    // Auto-execute
    on_execute();
}

void AkShareScreen::on_search_changed(const QString& text) {
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive) ||
                     item->data(Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void AkShareScreen::on_execute() {
    if (loading_ || active_source_ < 0 || active_endpoint_.isEmpty())
        return;

    QStringList args;
    // Some endpoints need parameters — pass them if non-empty
    if (!param_symbol_->text().trimmed().isEmpty()) {
        args << param_symbol_->text().trimmed();
    }
    if (!param_start_->text().trimmed().isEmpty()) {
        args << param_start_->text().trimmed();
    }
    if (!param_end_->text().trimmed().isEmpty()) {
        args << param_end_->text().trimmed();
    }

    execute_query(sources_[active_source_].script, active_endpoint_, args);
}

void AkShareScreen::on_view_toggle() {
    is_table_view_ = !is_table_view_;
    view_stack_->setCurrentIndex(is_table_view_ ? 0 : 1);
    view_toggle_btn_->setText(is_table_view_ ? "JSON" : "TABLE");
}

void AkShareScreen::on_refresh() {
    on_execute();
}

// ── Data loading ────────────────────────────────────────────────────────────

void AkShareScreen::load_endpoints(const AkShareSource& source) {
    const QString cache_key = "akshare:endpoints:" + source.script;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            auto obj = doc.object();
            endpoint_cache_[source.script] = obj;
            populate_endpoint_list(obj);
            return;
        }
    }

    set_loading(true);
    endpoint_list_->clear();
    data_status_->setText("Loading endpoints...");

    QPointer<AkShareScreen> self = this;

    python::PythonRunner::instance().run(
        source.script, {"get_all_endpoints"},
        [self, script = source.script, cache_key](const python::PythonResult& result) {
            if (!self)
                return;

            self->set_loading(false);

            if (!result.success) {
                self->data_status_->setText("Failed to load endpoints");
                LOG_ERROR("AkShare", "Endpoint load failed: " + result.error);
                return;
            }

            QString json_str = python::extract_json(result.output);
            if (json_str.isEmpty()) {
                self->data_status_->setText("No endpoint data returned");
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
                "akshare");
            self->endpoint_cache_[script] = obj;
            self->populate_endpoint_list(obj);
        });
}

void AkShareScreen::populate_endpoint_list(const QJsonObject& result) {
    endpoint_list_->clear();

    // Try to find endpoints in various response formats
    QJsonObject data_obj;
    if (result.contains("data") && result["data"].isObject()) {
        data_obj = result["data"].toObject();
    } else {
        data_obj = result;
    }

    // Look for categories -> endpoint lists
    QStringList all_endpoints;

    if (data_obj.contains("categories") && data_obj["categories"].isObject()) {
        auto categories = data_obj["categories"].toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            // Add category header (non-selectable)
            auto* header = new QListWidgetItem(it.key().toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(QColor(colors::AMBER()));
            auto f = header->font();
            f.setBold(true);
            f.setPointSize(8);
            header->setFont(f);
            endpoint_list_->addItem(header);

            // Add endpoints under this category
            if (it.value().isArray()) {
                for (auto ep : it.value().toArray()) {
                    QString ep_name = ep.toString();
                    auto* item = new QListWidgetItem("  " + ep_name);
                    item->setData(Qt::UserRole, ep_name);
                    endpoint_list_->addItem(item);
                    all_endpoints << ep_name;
                }
            }
        }
    } else if (data_obj.contains("available_endpoints") && data_obj["available_endpoints"].isArray()) {
        for (auto ep : data_obj["available_endpoints"].toArray()) {
            QString ep_name = ep.toString();
            auto* item = new QListWidgetItem(ep_name);
            item->setData(Qt::UserRole, ep_name);
            endpoint_list_->addItem(item);
            all_endpoints << ep_name;
        }
    }

    endpoint_count_->setText(QString::number(all_endpoints.size()) + " endpoints");
    data_status_->setText("Select an endpoint");
    LOG_INFO("AkShare", "Loaded " + QString::number(all_endpoints.size()) + " endpoints");
}

void AkShareScreen::execute_query(const QString& script, const QString& endpoint, const QStringList& args) {
    QStringList full_args;
    full_args << endpoint << args;

    const QString cache_key = "akshare:query:" + script + ":" + full_args.join(":");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isArray() && !doc.array().isEmpty()) {
            const QJsonArray data_array = doc.array();
            record_count_->setText(QString::number(data_array.size()) + " records");
            record_count_->show();
            data_status_->setText(endpoint + " (cached)");
            display_table_data(data_array);
            display_json_data(data_array);
            return;
        }
    }

    set_loading(true);
    data_status_->setText("Querying " + endpoint + "...");
    record_count_->hide();

    QPointer<AkShareScreen> self = this;

    python::PythonRunner::instance().run(
        script, full_args, [self, endpoint, cache_key](const python::PythonResult& result) {
            if (!self)
                return;

            self->set_loading(false);

            if (!result.success) {
                self->display_error(result.error.isEmpty() ? "Query failed" : result.error);
                return;
            }

            QString json_str = python::extract_json(result.output);
            if (json_str.isEmpty()) {
                self->display_error("No data returned from " + endpoint);
                return;
            }

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull()) {
                self->display_error("JSON parse error: " + err.errorString());
                return;
            }

            auto obj = doc.isObject() ? doc.object() : QJsonObject();

            // Check for error in response
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
                    // Single object result — wrap in array
                    data_array.append(obj["data"]);
                } else {
                    // Scalar value
                    QJsonObject wrapper;
                    wrapper["value"] = obj["data"];
                    data_array.append(wrapper);
                }
            } else if (doc.isArray()) {
                data_array = doc.array();
            } else {
                // Treat entire response as single-row data
                data_array.append(obj);
            }

            int count = data_array.size();
            self->record_count_->setText(QString::number(count) + " records");
            self->record_count_->show();
            self->data_status_->setText(endpoint);

            if (!data_array.isEmpty()) {
                fincept::CacheManager::instance().put(
                    cache_key, QVariant(QString::fromUtf8(QJsonDocument(data_array).toJson(QJsonDocument::Compact))),
                    2 * 60, "akshare");
            }

            self->display_table_data(data_array);
            self->display_json_data(data_array);

            LOG_INFO("AkShare", "Query " + endpoint + ": " + QString::number(count) + " records");
        });
}

// ── Display ─────────────────────────────────────────────────────────────────

void AkShareScreen::display_table_data(const QJsonArray& data) {
    data_table_->setSortingEnabled(false);
    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    if (data.isEmpty()) {
        data_status_->setText("No data returned");
        return;
    }

    // Extract columns from first row
    QStringList columns;
    auto first = data[0].toObject();
    for (auto it = first.begin(); it != first.end(); ++it) {
        columns << it.key();
    }

    data_table_->setColumnCount(columns.size());
    data_table_->setHorizontalHeaderLabels(columns);

    // Populate rows (cap at 5000 to avoid UI freeze)
    int max_rows = qMin(data.size(), 5000);
    data_table_->setRowCount(max_rows);

    for (int row = 0; row < max_rows; ++row) {
        auto obj = data[row].toObject();
        for (int col = 0; col < columns.size(); ++col) {
            auto val = obj.value(columns[col]);
            QString text;
            if (val.isDouble()) {
                text = QString::number(val.toDouble(), 'g', 10);
            } else if (val.isNull()) {
                text = "--";
            } else {
                text = val.toVariant().toString();
            }

            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            // Color numbers in cyan, negative in red
            if (val.isDouble()) {
                double v = val.toDouble();
                if (v < 0) {
                    item->setForeground(QColor(colors::NEGATIVE()));
                } else {
                    item->setForeground(QColor(colors::CYAN()));
                }
            }

            data_table_->setItem(row, col, item);
        }
    }

    // Resize columns to content (limit to avoid freezing)
    if (columns.size() <= 20) {
        data_table_->resizeColumnsToContents();
    }
    data_table_->setSortingEnabled(true);

    if (data.size() > max_rows) {
        data_status_->setText(QString("Showing %1 of %2 records").arg(max_rows).arg(data.size()));
    }
}

void AkShareScreen::display_json_data(const QJsonArray& data) {
    QJsonDocument doc(data);
    json_view_->setPlainText(doc.toJson(QJsonDocument::Indented));
}

void AkShareScreen::display_error(const QString& error) {
    data_status_->setText("Error");
    record_count_->hide();

    // Clear table
    data_table_->clear();
    data_table_->setRowCount(0);
    data_table_->setColumnCount(0);

    // Show error in JSON view
    QJsonObject err_obj;
    err_obj["error"] = error;
    json_view_->setPlainText(QJsonDocument(err_obj).toJson(QJsonDocument::Indented));

    LOG_ERROR("AkShare", "Query error: " + error);
}

void AkShareScreen::set_loading(bool loading) {
    loading_ = loading;
    exec_btn_->setEnabled(!loading);
    refresh_btn_->setEnabled(!loading && !active_endpoint_.isEmpty());
    if (loading) {
        data_status_->setText("Loading...");
    }
}

} // namespace fincept::screens
