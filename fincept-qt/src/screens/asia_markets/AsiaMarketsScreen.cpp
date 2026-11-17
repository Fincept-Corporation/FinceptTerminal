#include "screens/asia_markets/AsiaMarketsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/asia_markets/AsiaJsonView.h"
#include "screens/asia_markets/AsiaResultTable.h"
#include "screens/asia_markets/AsiaSymbolLinks.h"
#include "screens/common/CommandRows.h"
#include "services/asia_markets/AsiaMarketsService.h"
#include "storage/cache/CacheManager.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QFuture>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

// ── Screen-level stylesheet ─────────────────────────────────────────────────

namespace {
using namespace fincept::ui;

/// The screen stylesheet, built from the active appearance at each call (never cached).
QString asia_style() {
    return
        QString("#asiaScreen { background: %1; }"

                   "#asiaHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#asiaHeaderTitle { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
                   "#asiaHeaderSub { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; letter-spacing: 0.5px; background: transparent; }"

                   "#asiaRegionBtn { background: transparent; color: %5; border: 1px solid %8; "
                   "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; padding: 3px 10px; }"
                   "#asiaRegionBtn:hover { color: %4; border-color: %9; }"
                   "#asiaRegionBtn[active=\"true\"] { background: %3; color: %6; border-color: %3; }"

                   "#asiaCatScroll { background: %1; border: none; border-bottom: 1px solid %8; }"
                   "#asiaCatBar { background: %1; }"
                   "#asiaCatBtn { background: transparent; color: %5; border: none; "
                   "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; padding: 6px 14px; border-bottom: 2px solid transparent; }"
                   "#asiaCatBtn:hover { color: %4; }"
                   "#asiaCatBtn[active=\"true\"] { color: %4; border-bottom-color: %3; }"

                   "#asiaLeftPanel { background: %7; border-right: 1px solid %8; }"
                   "#asiaSearchInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "#asiaSearchInput:focus { border-color: %9; }"
                   "#asiaSymbolInput { background: %1; color: %13; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: 700; }"
                   "#asiaSymbolInput:focus { border-color: %9; }"
                   "#asiaEndpointCount { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"

                   "#asiaEndpointList { background: %1; border: none; outline: none; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "#asiaEndpointList::item { color: %5; padding: 4px 8px; border-bottom: 1px solid %8; }"
                   "#asiaEndpointList::item:hover { color: %4; background: %10; }"
                   "#asiaEndpointList::item:selected { color: %3; background: %14; "
                   "  border-left: 2px solid %3; }"

                   "#asiaDataPanel { background: %1; }"
                   "#asiaToolbar { background: %2; border-bottom: 1px solid %8; }"
                   "#asiaDataStatus { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
                   "#asiaRecordCount { color: %13; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"
                   "#asiaAsOf { color: %12; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"

                   "#asiaExecBtn { background: %3; color: %6; border: none; padding: 4px 14px; "
                   "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#asiaExecBtn:hover { background: %11; }"
                   "#asiaExecBtn:disabled { background: %7; color: %12; border: 1px solid %8; }"

                   "#asiaViewToggle { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#asiaViewToggle:hover { color: %4; }"

                   "#asiaRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#asiaRefreshBtn:hover { color: %4; }"
                   "#asiaRefreshBtn:disabled { color: %12; }"

                   "QTableView { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "QTableView::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QTableView::item:selected { background: %14; color: %3; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; }"

                   "#asiaJsonView { background: %1; color: %13; border: none; font-size: " + fincept::ui::fonts::px(-3) + "px; }"

                   "#asiaStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#asiaStatusText { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"
                   "#asiaStatusHighlight { color: %13; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"

                   "#asiaParamLabel { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"

                   "QSplitter::handle { background: %8; }"
                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { background: %1; height: 6px; }"
                   "QScrollBar::handle:horizontal { background: %8; min-width: 20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::TEXT_ON_ACCENT()) // %6 text on accent fills
        .arg(colors::BG_SURFACE())    // %7
        .arg(colors::BORDER_DIM())    // %8
        .arg(colors::BORDER_BRIGHT()) // %9
        .arg(colors::BG_HOVER())      // %10
        .arg(colors::AMBER_DIM())     // %11
        .arg(colors::TEXT_TERTIARY()) // %12 disabled text, "as of"
        .arg(colors::CYAN())          // %13
        .arg(colors::ACCENT_BG());    // %14
}

/// Item colour of the endpoint list's category headers.
QColor asia_header_color() {
    return QColor(colors::AMBER());
}

/// Most rows written into the JSON view (the table shows every row).
constexpr int kAsiaJsonRows = 1000;
/// Remembered results: refreshed quietly when older than this on a later visit — the pane's default;
/// each view gets its own (asia_links::result_max_age_s: 5 min for quotes, 6 h for daily data).
constexpr int kAsiaResultMaxAgeS = 15 * 60;
/// Largest result kept across restarts (compact JSON).
constexpr int kAsiaPersistBytes = 2 * 1024 * 1024;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;
namespace asia_svc = fincept::services::asia_markets;

// ── Category definitions ────────────────────────────────────────────────────

static QList<StockCategory> build_categories() {
    return {
        {"realtime", "REALTIME", "akshare_stocks_realtime.py"},
        {"historical", "HISTORICAL", "akshare_stocks_historical.py"},
        {"financial", "FINANCIAL", "akshare_stocks_financial.py"},
        {"holders", "HOLDERS", "akshare_stocks_holders.py"},
        {"fund_flow", "FUND FLOW", "akshare_stocks_funds.py"},
        {"boards", "BOARDS", "akshare_stocks_board.py"},
        {"margin", "MARGIN/HSGT", "akshare_stocks_margin.py"},
        {"hot_news", "HOT & NEWS", "akshare_stocks_hot.py"},
    };
}

// ── Constructor ─────────────────────────────────────────────────────────────

AsiaMarketsScreen::AsiaMarketsScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("asiaScreen");
    setStyleSheet(asia_style());
    categories_ = build_categories();
    setup_ui();
    // Item brushes / fonts of the list's category headers, the table row height and the category bar
    // height are not reached by stylesheets: refit once the new appearance has settled. Table cell
    // colours come from the model (live tokens), so nothing walks the cells.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, &AsiaMarketsScreen::apply_appearance);
    LOG_DEBUG("AsiaMarkets", "Screen constructed");
}

// ── Show / hide ─────────────────────────────────────────────────────────────

void AsiaMarketsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!first_show_done_) {
        // One event-loop turn later: the router shows a new screen before it restores its state, so
        // the remembered category opens directly (no load of category 0 first).
        QTimer::singleShot(0, this, [this]() {
            if (isVisible())
                first_show();
        });
    } else if (!endpoints_loading_ && active_category_ >= 0 && endpoint_list_->count() == 0 &&
               list_state_kind_ == ListState::Loading) {
        // The endpoint list was interrupted by leaving the screen: load it again.
        load_endpoints(active_category_);
    }
}

void AsiaMarketsScreen::hideEvent(QHideEvent* e) {
    // Leaving the screen stops an endpoint-list run (the result pane cancels its own default run).
    if (list_cancel_) {
        list_cancel_->store(true);
        list_cancel_.reset();
        endpoints_loading_ = false;
    }
    QWidget::hideEvent(e);
}

void AsiaMarketsScreen::first_show() {
    if (first_show_done_)
        return;
    first_show_done_ = true;
    // Endpoint lists already cached (1 h) give the header its real count without running anything.
    for (const auto& cat : std::as_const(categories_)) {
        if (endpoint_cache_.contains(cat.script))
            continue;
        const QVariant cached = fincept::CacheManager::instance().get("asia:endpoints:" + cat.script);
        if (cached.isNull())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isObject())
            endpoint_cache_[cat.script] = doc.object();
    }
    const int cat = (restored_category_ >= 0 && restored_category_ < categories_.size()) ? restored_category_ : 0;
    select_category(cat, std::exchange(pending_endpoint_, QString()));
}

// ── Live language switch ─────────────────────────────────────────────────────

void AsiaMarketsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AsiaMarketsScreen::retranslateUi() {
    // Header
    if (header_title_)
        header_title_->setText(tr("ASIA MARKETS TERMINAL"));
    update_header_count();

    // Left panel
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search endpoints..."));
    if (sym_label_)
        sym_label_->setText(tr("SYMBOL"));
    if (symbol_input_)
        symbol_input_->setPlaceholderText(tr("e.g. 000001"));

    // Data panel toolbar
    if (exec_btn_)
        exec_btn_->setText(tr("EXECUTE"));
    if (view_toggle_btn_)
        view_toggle_btn_->setText(is_table_view_ ? tr("JSON") : tr("TABLE"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));

    for (auto* btn : region_btns_)
        btn->setToolTip(tr("Show only %1 endpoints (click again to show all)").arg(btn->text()));

    // Status bar — category/region names are data values; only the labels translate.
    if (status_left_)
        status_left_->setText(tr("ASIA MARKETS"));
    if (status_category_ && active_category_ >= 0 && active_category_ < categories_.size())
        status_category_->setText(tr("CATEGORY: %1").arg(categories_[active_category_].name));
    if (status_region_)
        status_region_->setText(tr("REGION: %1").arg(active_region_ >= 0 && active_region_ < regions_.size()
                                                         ? regions_[active_region_]
                                                         : tr("ALL")));
}

void AsiaMarketsScreen::apply_appearance() {
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        QListWidgetItem* item = endpoint_list_->item(i);
        if (item->flags() & Qt::ItemIsSelectable)
            continue; // only the category headers carry their own colour and font
        item->setForeground(asia_header_color());
        item->setFont(fonts::font(-3, QFont::Bold));
    }
    data_table_->verticalHeader()->setDefaultSectionSize(asia_links::result_row_height());
    if (model_->rowCount() > 0)
        asia_links::fit_columns(data_table_, model_);
    fit_category_bar();
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
    left->setMinimumWidth(200);
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
    bar->setMinimumHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    // Title
    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    header_title_ = new QLabel(tr("ASIA MARKETS TERMINAL"));
    header_title_->setObjectName("asiaHeaderTitle");
    // The real endpoint count of what is loaded (update_header_count) — never a literal.
    header_sub_ = new QLabel;
    header_sub_->setObjectName("asiaHeaderSub");
    header_sub_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title_col->addWidget(header_title_);
    title_col->addWidget(header_sub_);
    hl->addLayout(title_col, 1);

    // Region buttons filter the endpoint list. AKShare endpoints are region-specific by name
    // (stock_hk_*, stock_us_*, stock_zh_b_*), so this is a client-side filter of the list; the
    // scripts take no region argument. Clicking the active region again shows every endpoint.
    for (int i = 0; i < regions_.size(); ++i) {
        auto* btn = new QPushButton(region_labels_[i]);
        btn->setObjectName("asiaRegionBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr("Show only %1 endpoints (click again to show all)").arg(region_labels_[i]));
        btn->setProperty("active", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_region_changed(active_region_ == i ? -1 : i); });
        hl->addWidget(btn);
        region_btns_.append(btn);
    }

    update_header_count();
    return bar;
}

QWidget* AsiaMarketsScreen::create_category_bar() {
    // A sideways-scrolling row: at 1280 px with a large font the eight tabs no longer fit, and a plain
    // row would force the whole screen wider than the window.
    cat_scroll_ = new QScrollArea(this);
    cat_scroll_->setObjectName("asiaCatScroll");
    cat_scroll_->setWidgetResizable(true);
    cat_scroll_->setFrameShape(QFrame::NoFrame);
    cat_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    cat_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* bar = new QWidget(cat_scroll_);
    bar->setObjectName("asiaCatBar");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(0);

    for (int i = 0; i < categories_.size(); ++i) {
        // A button reads a single "&" as a keyboard-shortcut marker: "HOT & NEWS" was drawn "HOT _NEWS".
        auto* btn = new QPushButton(QString(categories_[i].name).replace(QLatin1Char('&'), QLatin1String("&&")), bar);
        btn->setObjectName("asiaCatBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_category_changed(i); });
        hl->addWidget(btn);
        cat_btns_.append(btn);
    }
    hl->addStretch(1);

    cat_scroll_->setWidget(bar);
    fit_category_bar();
    return cat_scroll_;
}

void AsiaMarketsScreen::fit_category_bar() {
    if (!cat_scroll_)
        return;
    // One row of tabs plus room for the sideways scroll bar (6 px, see the stylesheet).
    const int tab_h = QFontMetrics(fonts::font(-5, QFont::Bold)).height() + 14; // padding 6+6, border 2
    cat_scroll_->setFixedHeight(std::max(32, tab_h + 8));
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
    search_input_->setPlaceholderText(tr("Search endpoints..."));
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::textChanged, this, &AsiaMarketsScreen::on_search_changed);

    // Symbol input row
    auto* sym_row = new QWidget(this);
    auto* srl = new QHBoxLayout(sym_row);
    srl->setContentsMargins(0, 0, 0, 0);
    srl->setSpacing(6);
    sym_label_ = new QLabel(tr("SYMBOL"));
    sym_label_->setObjectName("asiaParamLabel");
    symbol_input_ = new QLineEdit;
    symbol_input_->setObjectName("asiaSymbolInput");
    symbol_input_->setPlaceholderText(tr("e.g. 000001"));
    symbol_input_->setMaxLength(20);
    connect(symbol_input_, &QLineEdit::returnPressed, this, &AsiaMarketsScreen::on_execute);
    // The symbol is the result's input: a remembered result made from another says so.
    connect(symbol_input_, &QLineEdit::textChanged, this, &AsiaMarketsScreen::update_signature);
    srl->addWidget(sym_label_);
    srl->addWidget(symbol_input_, 1);

    endpoint_count_label_ = new QLabel(tr("%1 endpoints").arg(0));
    endpoint_count_label_->setObjectName("asiaEndpointCount");

    il->addWidget(search_input_);
    il->addWidget(sym_row);
    il->addWidget(endpoint_count_label_);
    vl->addWidget(input_area);

    // Endpoint list | loading skeleton | designed state (failed / nothing listed / filtered out)
    list_stack_ = new QStackedWidget(panel);

    endpoint_list_ = new QListWidget(list_stack_);
    endpoint_list_->setObjectName("asiaEndpointList");
    endpoint_list_->setAccessibleName(tr("Available endpoints"));
    connect(endpoint_list_, &QListWidget::itemClicked, this, &AsiaMarketsScreen::on_endpoint_clicked);
    list_stack_->addWidget(endpoint_list_);

    list_skeleton_ = new LoadingSkeleton(LoadingSkeleton::Shape::List, list_stack_);
    list_skeleton_->set_row_count(12);
    list_stack_->addWidget(list_skeleton_);

    list_state_ = new EmptyState(list_stack_);
    list_state_->set_compact(true);
    connect(list_state_, &EmptyState::primary_clicked, this, [this]() {
        if (list_state_kind_ == ListState::Filtered) {
            if (list_filtered_by_region_)
                on_region_changed(-1);
            else
                search_input_->clear();
        } else if (list_state_kind_ == ListState::Failed && active_category_ >= 0) {
            ++request_seq_;
            load_endpoints(active_category_);
        }
    });
    list_stack_->addWidget(list_state_);
    list_stack_->setCurrentWidget(list_skeleton_); // the first show opens a category at once

    vl->addWidget(list_stack_, 1);
    return panel;
}

QWidget* AsiaMarketsScreen::create_data_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("asiaDataPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar
    auto* toolbar = new QWidget(panel);
    toolbar->setObjectName("asiaToolbar");
    toolbar->setMinimumHeight(32);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    tbl->setSpacing(8);

    data_status_ = new QLabel(tr("Loading…"));
    data_status_->setObjectName("asiaDataStatus");
    // A long status ("… — not used by this endpoint: SYMBOL") is clipped, never allowed to widen
    // the screen past the window (it pushed the layout beyond 1280 px); the full text is the tooltip.
    data_status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    data_status_->setMinimumWidth(0);

    record_count_ = new QLabel;
    record_count_->setObjectName("asiaRecordCount");
    record_count_->hide();

    as_of_label_ = new QLabel;
    as_of_label_->setObjectName("asiaAsOf");

    // Natural (font-sized) widths: nothing in this row is a fixed pixel width.
    exec_btn_ = new QPushButton(tr("EXECUTE"));
    exec_btn_->setObjectName("asiaExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setEnabled(false);
    connect(exec_btn_, &QPushButton::clicked, this, &AsiaMarketsScreen::on_execute);

    view_toggle_btn_ = new QPushButton(tr("JSON"));
    view_toggle_btn_->setObjectName("asiaViewToggle");
    view_toggle_btn_->setCursor(Qt::PointingHandCursor);
    connect(view_toggle_btn_, &QPushButton::clicked, this, &AsiaMarketsScreen::on_view_toggle);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setObjectName("asiaRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setEnabled(false);
    connect(refresh_btn_, &QPushButton::clicked, this, &AsiaMarketsScreen::on_refresh);

    tbl->addWidget(data_status_, 1);
    tbl->addWidget(record_count_);
    tbl->addWidget(as_of_label_);
    tbl->addWidget(exec_btn_);
    tbl->addWidget(view_toggle_btn_);
    tbl->addWidget(refresh_btn_);
    vl->addWidget(toolbar);

    // The result pane: remembered result / skeleton / designed failure pages around the table | JSON.
    pane_ = new AutoLoadPane(AutoLoadPane::Policy::AutoRun, panel);
    pane_->setObjectName("asiaResultPane");

    view_stack_ = new QStackedWidget(pane_->content_parent());

    data_table_ = new QTableView(view_stack_);
    model_ = new asia_links::ResultModel(data_table_);
    data_table_->setModel(model_);
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->setWordWrap(false);
    data_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    data_table_->verticalHeader()->setVisible(false);
    data_table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    data_table_->verticalHeader()->setDefaultSectionSize(asia_links::result_row_height());
    data_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    data_table_->horizontalHeader()->setStretchLastSection(true);
    data_table_->horizontalHeader()->setSortIndicatorShown(true);
    // Source order until a header is clicked (the script's order is often a ranking).
    data_table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    data_table_->setSortingEnabled(true);
    // Security rows lead somewhere: link-group publish on selection, "Open in…" on right-click.
    data_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(data_table_, &QTableView::customContextMenuRequested, this, &AsiaMarketsScreen::show_row_menu);
    connect(data_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { publish_selection_to_group(); });

    json_view_ = new QPlainTextEdit(view_stack_);
    json_view_->setObjectName("asiaJsonView");
    json_view_->setReadOnly(true);
    json_view_->setLineWrapMode(QPlainTextEdit::NoWrap);

    view_stack_->addWidget(data_table_);
    view_stack_->addWidget(json_view_);
    pane_->set_content(view_stack_);

    pane_->skeleton()->set_shape(LoadingSkeleton::Shape::Table);
    pane_->skeleton()->set_row_count(12);
    pane_->set_cache_key(QStringLiteral("asia_markets/result"), 7);
    pane_->set_persist_limit(kAsiaPersistBytes);
    pane_->set_max_age(kAsiaResultMaxAgeS);
    // Results over the persist limit come back only by running again, so the pane keeps more than its
    // default 32 MB; sizes come from the decode thread's estimate in the payload (O(1) here — the
    // default estimator walks the whole result on the GUI thread).
    pane_->set_memory_budget(asia_links::kResultMemoryBudgetBytes);
    pane_->set_size_estimator(&asia_links::result_payload_bytes);
    pane_->set_auto_delay(300); // users flick through categories and endpoints
    pane_->set_requirements({Requirement::python()});
    pane_->set_status_label(as_of_label_);
    pane_->set_start([this](quint64 id) { start_query(id); });
    pane_->set_cancel([this](quint64 id) { cancel_query(id); });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_result(payload); });

    vl->addWidget(pane_, 1);
    return panel;
}

QWidget* AsiaMarketsScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("asiaStatusBar");
    bar->setMinimumHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    status_left_ = new QLabel(tr("ASIA MARKETS"));
    status_left_->setObjectName("asiaStatusText");
    hl->addWidget(status_left_);
    hl->addStretch(1);

    status_category_ = new QLabel(tr("CATEGORY: %1").arg(QStringLiteral("REALTIME")));
    status_category_->setObjectName("asiaStatusText");
    hl->addWidget(status_category_);

    hl->addSpacing(16);
    status_region_ = new QLabel(tr("REGION: %1").arg(tr("ALL")));
    status_region_->setObjectName("asiaStatusHighlight");
    hl->addWidget(status_region_);

    return bar;
}

// ── Header count ─────────────────────────────────────────────────────────────

QStringList AsiaMarketsScreen::endpoint_names(const QJsonObject& result) {
    const QJsonObject data_obj =
        (result.contains("data") && result["data"].isObject()) ? result["data"].toObject() : result;
    QStringList names;
    QSet<QString> placed;
    const auto add = [&names, &placed](const QString& name) {
        if (!name.isEmpty() && !placed.contains(name)) {
            placed.insert(name);
            names << name;
        }
    };
    if (data_obj.value("categories").isObject()) {
        const QJsonObject cats = data_obj["categories"].toObject();
        for (auto it = cats.begin(); it != cats.end(); ++it)
            for (const auto& ep : it.value().toArray())
                add(ep.toString());
    }
    for (const auto& ep : data_obj.value("available_endpoints").toArray())
        add(ep.toString());
    return names;
}

void AsiaMarketsScreen::update_header_count() {
    if (!header_sub_)
        return;
    int total = 0;
    int known = 0;
    for (const auto& cat : std::as_const(categories_)) {
        const auto it = endpoint_cache_.constFind(cat.script);
        if (it == endpoint_cache_.cend())
            continue;
        total += static_cast<int>(endpoint_names(it.value()).size());
        ++known;
    }
    QString text;
    if (known == categories_.size() && total > 0) {
        text = tr("%1 STOCK ENDPOINTS IN %2 CATEGORIES | CN A/B, HK, US")
                   .arg(ui::formatting::number(total, 0))
                   .arg(categories_.size());
    } else if (active_category_ >= 0 && endpoint_cache_.contains(categories_[active_category_].script)) {
        const int n = static_cast<int>(endpoint_names(endpoint_cache_[categories_[active_category_].script]).size());
        text = tr("%1 ENDPOINTS IN %2 | CN A/B, HK, US")
                   .arg(ui::formatting::number(n, 0), categories_[active_category_].name);
    } else {
        text = tr("STOCK ENDPOINTS | CN A/B, HK, US");
    }
    header_sub_->setText(text);
    header_sub_->setToolTip(text);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AsiaMarketsScreen::on_category_changed(int index) {
    // Re-clicking the active category is a no-op, unless its endpoint list is empty (the load
    // failed) — then it is the way to retry.
    if (index == active_category_ && endpoint_list_->count() > 0)
        return;
    select_category(index, QString());
}

void AsiaMarketsScreen::select_category(int index, const QString& endpoint) {
    if (index < 0 || index >= categories_.size())
        return;
    const StockCategory& cat = categories_[index];
    const bool same = index == active_category_;
    active_category_ = index;

    for (int i = 0; i < cat_btns_.size(); ++i) {
        if (cat_btns_[i]->property("active").toBool() == (i == index))
            continue;
        cat_btns_[i]->setProperty("active", i == index);
        cat_btns_[i]->style()->unpolish(cat_btns_[i]);
        cat_btns_[i]->style()->polish(cat_btns_[i]);
    }
    if (cat_scroll_ && index < cat_btns_.size())
        cat_scroll_->ensureWidgetVisible(cat_btns_[index], 24, 0);
    status_category_->setText(tr("CATEGORY: %1").arg(cat.name));
    if (!same) {
        const QSignalBlocker block(search_input_);
        search_input_->clear();
    }
    LOG_DEBUG("AsiaMarkets", "Category: " + cat.name);

    // The endpoint to open: the one asked for, else the one last opened in this category, else the
    // category's default (keyless, quick — asia_links::default_endpoint) when the region filter
    // shows it. Known before the list arrives, so the result does not wait for the list.
    QString ep = endpoint;
    if (ep.isEmpty())
        ep = last_endpoint_by_category_.value(cat.id);
    if (ep.isEmpty())
        ep = asia_links::default_endpoint(cat.script);
    if (endpoint.isEmpty() && !endpoint_in_region(ep, active_region_))
        ep.clear(); // the first endpoint the region filter shows, once the list is there
    active_endpoint_ = ep;

    if (!same || endpoint_list_->count() == 0) {
        ++request_seq_; // anything still in flight belongs to the previous category
        if (endpoint_cache_.contains(cat.script))
            populate_endpoint_list(endpoint_cache_[cat.script]);
        else
            load_endpoints(index);
    }
    select_endpoint(active_endpoint_);
    update_header_count();
    ScreenStateManager::instance().notify_changed(this);
}

void AsiaMarketsScreen::select_endpoint(const QString& endpoint) {
    active_endpoint_ = endpoint;
    const bool have = active_category_ >= 0 && !endpoint.isEmpty();
    if (have)
        last_endpoint_by_category_.insert(categories_[active_category_].id, endpoint);
    highlight_endpoint(endpoint);
    exec_btn_->setEnabled(have);
    refresh_btn_->setEnabled(have);
    if (active_category_ >= 0) {
        pane_->set_loading_text(have ? tr("Querying %1…").arg(endpoint)
                                     : tr("Loading the %1 endpoint list…").arg(categories_[active_category_].name));
    }
    if (!have) {
        data_status_->setText(tr("Loading endpoints…"));
        data_status_->setToolTip(QString());
    }
    // Freshness per endpoint (quotes 5 min, daily data 6 h), and the inputs signature set before the
    // switch so the view's first look ("Inputs changed", its default run) already uses it.
    const QString view = view_key();
    if (have)
        pane_->set_view_max_age(view, asia_links::result_max_age_s(categories_[active_category_].id, endpoint));
    pane_->set_view(view, AutoLoadPane::Policy::AutoRun, args_signature());
    ScreenStateManager::instance().notify_changed(this);
}

void AsiaMarketsScreen::on_region_changed(int index) {
    // -1 = no region filter (every endpoint of the category is listed).
    active_region_ = (index >= 0 && index < regions_.size()) ? index : -1;

    for (int i = 0; i < region_btns_.size(); ++i) {
        if (region_btns_[i]->property("active").toBool() == (i == active_region_))
            continue;
        region_btns_[i]->setProperty("active", i == active_region_);
        region_btns_[i]->style()->unpolish(region_btns_[i]);
        region_btns_[i]->style()->polish(region_btns_[i]);
    }

    const QString region = active_region_ >= 0 ? regions_[active_region_] : tr("ALL");
    status_region_->setText(tr("REGION: %1").arg(region));
    LOG_DEBUG("AsiaMarkets", "Region: " + region);
    apply_list_filter();
    // Nothing on screen yet for this category (the region hid its default): open the first endpoint
    // the new filter shows.
    if (active_endpoint_.isEmpty() && active_category_ >= 0 && !endpoints_loading_) {
        for (int i = 0; i < endpoint_list_->count(); ++i) {
            auto* item = endpoint_list_->item(i);
            const QString ep = item->data(Qt::UserRole).toString();
            if (!ep.isEmpty() && !item->isHidden()) {
                select_endpoint(ep);
                break;
            }
        }
    }
    ScreenStateManager::instance().notify_changed(this);
}

bool AsiaMarketsScreen::endpoint_in_region(const QString& endpoint, int region) {
    // Endpoint names carry the market: stock_hk_* / *_hk_* (HK), stock_us_* / *_us_* (US),
    // stock_zh_b_* (CN B shares); everything else is a mainland A-share / market-wide endpoint.
    const QString ep = endpoint.toLower();
    const bool hk = ep.contains(QLatin1String("_hk"));
    const bool us = ep.contains(QLatin1String("_us"));
    const bool b_share = ep.contains(QLatin1String("zh_b_"));
    switch (region) {
    case 0:
        return !hk && !us && !b_share; // CN A
    case 1:
        return b_share; // CN B
    case 2:
        return hk;
    case 3:
        return us;
    default:
        return true; // no region filter
    }
}

int AsiaMarketsScreen::apply_list_filter() {
    const QString text = search_input_ ? search_input_->text() : QString();
    // Endpoints first; a category header is shown only while one of its endpoints is.
    QListWidgetItem* header = nullptr;
    bool header_has_visible = false;
    int visible = 0;
    int in_region = 0;
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        const QString ep = item->data(Qt::UserRole).toString();
        if (ep.isEmpty()) { // category header
            if (header)
                header->setHidden(!header_has_visible);
            header = item;
            header_has_visible = false;
            continue;
        }
        const bool region_ok = endpoint_in_region(ep, active_region_);
        const bool match = region_ok && (text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive) ||
                                         ep.contains(text, Qt::CaseInsensitive));
        item->setHidden(!match);
        header_has_visible = header_has_visible || match;
        visible += match ? 1 : 0;
        in_region += region_ok ? 1 : 0;
    }
    if (header)
        header->setHidden(!header_has_visible);

    if (list_state_kind_ == ListState::List || list_state_kind_ == ListState::Filtered) {
        if (visible == 0 && endpoint_list_->count() > 0) {
            list_filtered_by_region_ = in_region == 0;
            show_list_state(ListState::Filtered, text);
        } else {
            show_list_state(ListState::List);
        }
    }
    return visible;
}

void AsiaMarketsScreen::on_endpoint_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    const QString ep = item->data(Qt::UserRole).toString();
    if (ep.isEmpty())
        return; // category header
    // Opens the endpoint's result: the remembered one at once (refreshed when old), else a run.
    // Picking another endpoint while a query runs supersedes it.
    select_endpoint(ep);
}

void AsiaMarketsScreen::on_search_changed(const QString& text) {
    Q_UNUSED(text); // apply_list_filter() reads the search box itself (it also runs on region changes)
    apply_list_filter();
}

void AsiaMarketsScreen::on_execute() {
    if (active_endpoint_.isEmpty())
        return;
    pane_->run();
}

void AsiaMarketsScreen::on_refresh() {
    // A fresh run of the endpoint on screen (the remembered result stays, dimmed, until it lands).
    if (active_endpoint_.isEmpty())
        return;
    pane_->run();
}

void AsiaMarketsScreen::on_view_toggle() {
    is_table_view_ = !is_table_view_;
    view_stack_->setCurrentIndex(is_table_view_ ? 0 : 1);
    view_toggle_btn_->setText(is_table_view_ ? tr("JSON") : tr("TABLE"));
    // Populate JSON view lazily on first switch
    if (!is_table_view_)
        fill_json_view();
}

// ── Endpoint list ───────────────────────────────────────────────────────────

void AsiaMarketsScreen::show_list_state(ListState state, const QString& detail) {
    list_state_kind_ = state;
    const QString cat_name = active_category_ >= 0 ? categories_[active_category_].name : QString();
    switch (state) {
        case ListState::List:
            list_stack_->setCurrentWidget(endpoint_list_);
            return;
        case ListState::Loading:
            list_skeleton_->set_message(tr("Loading endpoints…"));
            list_stack_->setCurrentWidget(list_skeleton_);
            return;
        case ListState::Failed:
            list_state_->apply(EmptyState::error_spec(tr("The %1 endpoint list did not load.").arg(cat_name),
                                                      tr("Could not list endpoints"), tr("RETRY"), detail));
            break;
        case ListState::Empty:
            list_state_->apply(
                EmptyState::no_data_spec(tr("No endpoints"), tr("%1 publishes no endpoints.").arg(cat_name)));
            break;
        case ListState::Filtered:
            if (list_filtered_by_region_) {
                const QString region = active_region_ >= 0 ? region_labels_[active_region_] : QString();
                list_state_->apply(EmptyState::no_data_spec(
                    tr("No %1 endpoints here").arg(region),
                    tr("%1 has no %2 endpoints. Show every region, or pick another category.").arg(cat_name, region),
                    tr("SHOW ALL REGIONS")));
            } else {
                list_state_->apply(EmptyState::no_data_spec(
                    tr("No match"), tr("No endpoint matches “%1”.").arg(detail), tr("CLEAR SEARCH")));
            }
            break;
    }
    list_stack_->setCurrentWidget(list_state_);
}

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

    if (list_cancel_)
        list_cancel_->store(true); // the list of the previous category is no longer wanted
    list_cancel_ = std::make_shared<std::atomic<bool>>(false);
    endpoints_loading_ = true;
    endpoint_list_->clear();
    endpoint_count_label_->setText(tr("%1 endpoints").arg(0));
    show_list_state(ListState::Loading);

    QPointer<AsiaMarketsScreen> self = this;
    const int seq = request_seq_;
    const auto token = list_cancel_;

    asia_svc::AsiaMarketsService::instance().fetch_endpoints(
        script,
        [self, seq, script, cache_key, token](const asia_svc::EndpointsResult& r) {
            if (!self || r.cancelled)
                return;

            // Keep a good listing even if the user has moved on to another category...
            if (r.success) {
                fincept::CacheManager::instance().put(
                    cache_key, QVariant(QString::fromUtf8(QJsonDocument(r.data).toJson(QJsonDocument::Compact))),
                    60 * 60, "asia_markets");
                self->endpoint_cache_[script] = r.data;
                self->update_header_count();
            }
            // ...but never paint it (or auto-run one of its endpoints) over the current category.
            if (seq != self->request_seq_)
                return;
            if (self->list_cancel_ == token)
                self->list_cancel_.reset();
            self->endpoints_loading_ = false;

            if (!r.success) {
                const QString why = r.error.isEmpty() ? AsiaMarketsScreen::tr("Failed to load endpoints") : r.error;
                self->show_list_state(ListState::Failed, why.left(600));
                LOG_ERROR("AsiaMarkets", "Endpoint load failed for " + script + ": " + why.left(300));
                if (self->waiting_id_ != 0) {
                    // The result pane was waiting for an endpoint to run: same failure, same RETRY.
                    const quint64 id = std::exchange(self->waiting_id_, 0);
                    QPointer<AsiaMarketsScreen> s2 = self;
                    self->pane_->fail(id,
                                      EmptyState::error_spec(AsiaMarketsScreen::tr("The endpoint list did not load, "
                                                                                   "so there is nothing to run yet."),
                                                             AsiaMarketsScreen::tr("Could not list endpoints"),
                                                             AsiaMarketsScreen::tr("RETRY"), why.left(600)),
                                      [s2]() {
                                          if (!s2 || s2->active_category_ < 0)
                                              return;
                                          ++s2->request_seq_;
                                          s2->load_endpoints(s2->active_category_);
                                          s2->pane_->run();
                                      });
                }
                return;
            }

            self->populate_endpoint_list(r.data);
        },
        list_cancel_);
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
        auto add_header = [this](const QString& title) {
            // Category header
            auto* header = new QListWidgetItem(title.toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(asia_header_color());
            header->setFont(fonts::font(-3, QFont::Bold));
            endpoint_list_->addItem(header);
        };
        QSet<QString> placed;
        auto categories = data_obj["categories"].toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            add_header(it.key());

            if (it.value().isArray()) {
                for (auto ep : it.value().toArray()) {
                    QString name = ep.toString();
                    auto* item = new QListWidgetItem("  " + name);
                    item->setData(Qt::UserRole, name);
                    endpoint_list_->addItem(item);
                    if (!placed.contains(name)) {
                        placed.insert(name);
                        all_endpoints << name;
                    }
                }
            }
        }

        // Endpoints in the full list that no category placed stay reachable under OTHER.
        QStringList rest;
        for (const auto& ep : data_obj.value("available_endpoints").toArray()) {
            const QString name = ep.toString();
            if (!name.isEmpty() && !placed.contains(name)) {
                placed.insert(name);
                rest << name;
            }
        }
        if (!rest.isEmpty()) {
            add_header(tr("Other"));
            for (const QString& name : rest) {
                auto* item = new QListWidgetItem("  " + name);
                item->setData(Qt::UserRole, name);
                endpoint_list_->addItem(item);
                all_endpoints << name;
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

    endpoint_count_label_->setText(tr("%1 endpoints").arg(all_endpoints.size()));
    LOG_DEBUG("AsiaMarkets", "Loaded " + QString::number(all_endpoints.size()) + " endpoints");
    update_header_count();
    if (all_endpoints.isEmpty()) {
        show_list_state(ListState::Empty);
        if (waiting_id_ != 0) {
            const quint64 id = std::exchange(waiting_id_, 0);
            pane_->fail(id,
                        EmptyState::no_data_spec(tr("No endpoints"),
                                                 tr("%1 publishes no endpoints to query.")
                                                     .arg(active_category_ >= 0 ? categories_[active_category_].name
                                                                                : QString())),
                        []() {});
        }
        return;
    }
    show_list_state(ListState::List);
    apply_list_filter(); // keep the active region filter (and any search text) on the new list

    if (!active_endpoint_.isEmpty()) {
        highlight_endpoint(active_endpoint_);
        return;
    }
    // No endpoint chosen yet (the region filter hid the default): the first one the filter shows.
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        const QString ep = item->data(Qt::UserRole).toString();
        if (!ep.isEmpty() && !item->isHidden()) {
            select_endpoint(ep);
            return;
        }
    }
    // The region filter hides every endpoint of this category: say so where the result would be.
    if (waiting_id_ != 0)
        fail_region_empty(std::exchange(waiting_id_, 0));
}

void AsiaMarketsScreen::fail_region_empty(quint64 request_id) {
    const QString region = active_region_ >= 0 ? region_labels_[active_region_] : QString();
    const QString cat_name = active_category_ >= 0 ? categories_[active_category_].name : QString();
    QPointer<AsiaMarketsScreen> self = this;
    pane_->fail(request_id,
                EmptyState::no_data_spec(tr("No %1 endpoints in %2").arg(region, cat_name),
                                         tr("The region filter hides every endpoint of this category. Show every "
                                            "region, or pick another category above."),
                                         tr("SHOW ALL REGIONS")),
                [self]() {
                    if (self)
                        self->on_region_changed(-1);
                });
}

void AsiaMarketsScreen::highlight_endpoint(const QString& endpoint) {
    const QSignalBlocker block(endpoint_list_);
    for (int i = 0; i < endpoint_list_->count() && !endpoint.isEmpty(); ++i) {
        auto* item = endpoint_list_->item(i);
        if (item && item->data(Qt::UserRole).toString() == endpoint) {
            endpoint_list_->setCurrentItem(item);
            endpoint_list_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            return;
        }
    }
    endpoint_list_->clearSelection();
}

// ── Result pane ─────────────────────────────────────────────────────────────

QString AsiaMarketsScreen::view_key() const {
    if (active_category_ < 0 || active_category_ >= categories_.size())
        return {};
    return categories_[active_category_].id + QLatin1Char('/') + active_endpoint_;
}

QStringList AsiaMarketsScreen::current_args() const {
    // By name, not position: the endpoints' first parameter is often a date, an indicator or a
    // board name, not a stock code. The script applies symbol= only where the endpoint has a
    // symbol (or stock) parameter and reports it back as unused otherwise.
    QStringList extra;
    const QString sym = symbol_input_->text().trimmed();
    if (!sym.isEmpty())
        extra << QStringLiteral("symbol=") + sym;
    return extra;
}

QString AsiaMarketsScreen::args_signature() const {
    return autoload::signature_of(QVariantMap{{QStringLiteral("args"), current_args()}});
}

void AsiaMarketsScreen::update_signature() {
    if (pane_)
        pane_->set_signature(args_signature());
    ScreenStateManager::instance().notify_changed(this);
}

void AsiaMarketsScreen::start_query(quint64 request_id) {
    if (active_category_ < 0 || active_endpoint_.isEmpty()) {
        if (endpoints_loading_) {
            // The list is still loading; its arrival picks an endpoint (switching the view cancels this).
            waiting_id_ = request_id;
            return;
        }
        if (endpoint_list_->count() > 0 && active_region_ >= 0 && apply_list_filter() == 0) {
            fail_region_empty(request_id);
            return;
        }
        pane_->fail(request_id, tr("Pick an endpoint in the list on the left."), tr("No endpoint selected"));
        return;
    }
    const QString script = categories_[active_category_].script;
    const QString endpoint = active_endpoint_;
    const auto token = std::make_shared<std::atomic<bool>>(false);
    query_tokens_.insert(request_id, token);

    QPointer<AsiaMarketsScreen> self = this;
    asia_svc::AsiaMarketsService::instance().query(
        script, endpoint, current_args(),
        [self, request_id, endpoint](const asia_svc::QueryResult& r) {
            if (!self)
                return;
            self->query_tokens_.remove(request_id);
            if (r.cancelled || !self->pane_->is_current(request_id))
                return; // superseded, cancelled, or its view was left
            if (!r.success) {
                self->fail_query(request_id, endpoint, r);
                return;
            }
            if (r.rows.isEmpty()) {
                self->pane_->fail(
                    request_id,
                    EmptyState::no_data_spec(AsiaMarketsScreen::tr("No data for these inputs"),
                                             AsiaMarketsScreen::tr("%1 answered with an empty table — the market may "
                                                                   "be closed for the day, or the symbol matches "
                                                                   "nothing. Try another symbol.")
                                                 .arg(endpoint),
                                             AsiaMarketsScreen::tr("RUN AGAIN")),
                    {});
                return;
            }
            // approx_bytes: the decode thread's size estimate, read by the pane's size estimator
            // (asia_links::result_payload_bytes) instead of walking the rows here.
            QJsonObject payload{{QStringLiteral("endpoint"), endpoint},
                                {QStringLiteral("rows"), r.rows},
                                {QStringLiteral("columns"), QJsonArray::fromStringList(r.columns)},
                                {QStringLiteral("ignored"), QJsonArray::fromStringList(r.ignored_params)},
                                {QStringLiteral("approx_bytes"), static_cast<double>(r.approx_bytes)}};
            self->pane_->deliver(request_id, payload);
            LOG_DEBUG("AsiaMarkets", endpoint + ": " + QString::number(r.rows.size()) + " records");
        },
        token);
}

void AsiaMarketsScreen::cancel_query(quint64 request_id) {
    if (waiting_id_ == request_id)
        waiting_id_ = 0;
    if (auto token = query_tokens_.take(request_id))
        token->store(true); // the runner kills the script (or never starts it)
}

void AsiaMarketsScreen::fail_query(quint64 request_id, const QString& endpoint, const asia_svc::QueryResult& r) {
    const QString error = r.error.isEmpty() ? tr("The query failed.") : r.error;

    // The endpoint needs a value that was not given — an input page, not an error. The scripts name
    // the parameters (r.missing, akshare_screen_args.py missing_param()); answers without the list
    // carry only the sentence "<endpoint> needs a value for: symbol (SYMBOL box)".
    const bool structured = !r.missing.isEmpty();
    const QString missing =
        structured ? asia_links::missing_parameters_text(r.missing) : asia_links::missing_parameter(error);
    if (!missing.isEmpty()) {
        EmptyState::Spec spec;
        spec.glyph = QStringLiteral("?");
        spec.glyph_tone = EmptyState::Tone::Info;
        spec.title = tr("%1 needs an input").arg(endpoint);
        spec.message = tr("It needs a value for: %1. Type it in SYMBOL on the left and press EXECUTE.").arg(missing);
        const bool needs_symbol = structured ? asia_links::missing_needs_symbol(r.missing)
                                             : missing.contains(QLatin1String("SYMBOL"));
        const bool symbol = needs_symbol && symbol_input_->text().trimmed().isEmpty();
        if (symbol) {
            spec.primary = tr("RUN WITH %1").arg(asia_links::sample_symbol());
            spec.details = {{tr("Sample symbol"), asia_links::sample_symbol()}};
            spec.details_heading = tr("TRY IT WITH");
        }
        QPointer<AsiaMarketsScreen> self = this;
        pane_->fail(request_id, spec, [self, symbol]() {
            if (!self)
                return;
            if (symbol) {
                self->symbol_input_->setText(asia_links::sample_symbol());
                self->pane_->run();
            } else {
                self->symbol_input_->setFocus();
            }
        });
        return;
    }

    if (error.contains(QLatin1String("Missing dependency"))) {
        EmptyState::Spec spec = EmptyState::error_spec(
            tr("The AKShare Python package is not installed in the terminal's Python environment."),
            tr("AKShare is not installed"), tr("OPEN SETTINGS > PYTHON"), error);
        spec.glyph = QStringLiteral("⚙");
        spec.glyph_tone = EmptyState::Tone::Warning;
        QPointer<AsiaMarketsScreen> self = this;
        pane_->fail(request_id, spec, [self]() {
            if (self)
                open_setup_link({QStringLiteral("settings"), QStringLiteral("python_env"), QString()}, self.data());
        });
        return;
    }

    // The scripts' failure classes (akshare_screen_args.py: unavailable / network / usage; the shared
    // connector wrapper: not_supported / upstream_down / bad_args / no_data).
    const QString type = r.error_type;
    if (type == QLatin1String("unavailable") || type == QLatin1String("not_supported")) {
        EmptyState::Spec spec = EmptyState::no_data_spec(tr("%1 is not available here").arg(endpoint), error);
        spec.glyph = QStringLiteral("⊘"); // ⊘
        pane_->fail(request_id, spec, []() {});
        return;
    }

    if (type == QLatin1String("no_data")) {
        pane_->fail(request_id,
                    EmptyState::no_data_spec(tr("No data for these inputs"), error, tr("RUN AGAIN")), {});
        return;
    }

    if (type == QLatin1String("bad_args") || type == QLatin1String("usage")) {
        EmptyState::Spec spec = EmptyState::error_spec(error, tr("Check the inputs"), tr("RUN AGAIN"));
        spec.glyph = QStringLiteral("?");
        spec.glyph_tone = EmptyState::Tone::Info;
        pane_->fail(request_id, spec, {});
        return;
    }

    if (type == QLatin1String("network") || type == QLatin1String("upstream_down")) {
        const bool limited = error.contains(QLatin1String("rate-limit"), Qt::CaseInsensitive) ||
                             error.contains(QLatin1String("refusing"), Qt::CaseInsensitive);
        EmptyState::Spec spec = EmptyState::error_spec(
            error, limited ? tr("The source is limiting requests") : tr("The data source did not answer"), tr("RETRY"));
        if (limited) {
            spec.glyph_tone = EmptyState::Tone::Warning;
            spec.note = tr("Retrying at once extends the block — wait a few minutes.");
            spec.note_tone = EmptyState::Tone::Warning;
        }
        pane_->fail(request_id, spec, {});
        return;
    }

    pane_->fail(request_id, EmptyState::error_spec(error.simplified().left(400), tr("Could not load %1").arg(endpoint),
                                                   tr("RETRY"), error.size() > 400 ? error : QString()),
                {});
}

void AsiaMarketsScreen::render_result(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QJsonArray rows = p.value(QStringLiteral("rows")).toArray();
    QStringList columns;
    for (const auto& c : p.value(QStringLiteral("columns")).toArray())
        columns << (c.isString() ? c.toString() : c.toVariant().toString());
    QStringList ignored;
    for (const auto& c : p.value(QStringLiteral("ignored")).toArray())
        ignored << c.toString();
    const QString endpoint = p.value(QStringLiteral("endpoint")).toString();

    // One pass: the model takes the rows as they are (no per-cell items), columns are fitted once.
    {
        const QSignalBlocker block(data_table_->horizontalHeader());
        data_table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    }
    data_table_->setUpdatesEnabled(false);
    model_->set_result(rows, columns);
    symbol_col_ = asia_links::symbol_column(model_->columns());
    name_col_ = asia_links::name_column(model_->columns());
    asia_links::fit_columns(data_table_, model_);
    data_table_->setUpdatesEnabled(true);

    record_count_->setText(tr("%1 records").arg(ui::formatting::number(rows.size(), 0)));
    record_count_->show();
    // Say so when the SYMBOL box was not applied (this endpoint has no symbol parameter).
    const QString status =
        ignored.isEmpty() ? endpoint
                          : tr("%1 — not used by this endpoint: %2").arg(endpoint, ignored.join(QStringLiteral(", ")));
    data_status_->setText(status);
    data_status_->setToolTip(status);

    json_rows_ = rows;
    json_dirty_ = true;
    ++json_seq_; // a JSON document still being built is for the previous result
    if (!is_table_view_)
        fill_json_view();
}

void AsiaMarketsScreen::fill_json_view() {
    if (!json_dirty_)
        return;
    json_dirty_ = false;
    const quint64 seq = ++json_seq_;
    QString header;
    if (json_rows_.size() > kAsiaJsonRows) {
        header = tr("// The first %1 of %2 rows — the TABLE view shows them all.")
                     .arg(kAsiaJsonRows)
                     .arg(json_rows_.size()) +
                 QLatin1Char('\n');
    }
    // The indented text of up to 1,000 rows (~0.7 MB, ~21,000 lines for a wide table) and its text
    // document are built on a worker; this thread only attaches the finished document.
    asia_links::set_json_text(json_view_, tr("Preparing the JSON view…"));
    QPointer<AsiaMarketsScreen> self = this;
    (void)QtConcurrent::run(asia_links::json_view_pool(),
                            [rows = json_rows_, header, target = thread()]() {
                                return asia_links::build_json_document(rows, kAsiaJsonRows, header, target);
                            })
        .then(this, [self, seq](asia_links::JsonViewDocumentPtr built) {
            if (!self || seq != self->json_seq_ || !built || !built->doc)
                return; // a newer result (or the screen) replaced it: the holder deletes the document
            asia_links::install_json_document(self->json_view_, std::exchange(built->doc, nullptr));
        });
}

// ── Symbol linking ───────────────────────────────────────────────────────────

QString AsiaMarketsScreen::yf_symbol_at_row(int row) const {
    if (symbol_col_ < 0 || row < 0 || row >= model_->rowCount())
        return {};
    return asia_links::yf_symbol_for_code(model_->text(row, symbol_col_));
}

QString AsiaMarketsScreen::name_at_row(int row) const {
    if (name_col_ < 0 || row < 0 || row >= model_->rowCount())
        return {};
    return model_->text(row, name_col_).trimmed();
}

void AsiaMarketsScreen::publish_selection_to_group() {
    if (link_group_ == fincept::SymbolGroup::None || suppress_publish_)
        return;
    const QString yf = yf_symbol_at_row(data_table_->currentIndex().row());
    if (yf.isEmpty())
        return;
    fincept::SymbolContext::instance().set_group_symbol(
        link_group_, fincept::SymbolRef::equity(yf, asia_links::exchange_for_yf(yf)), this);
}

void AsiaMarketsScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // A-share / HK symbols become the SYMBOL parameter, so the next per-security query follows the
    // group (nothing is fetched here — a follower must stay cheap).
    const QString native = asia_links::native_code_for(ref.symbol);
    if (!native.isEmpty() && symbol_input_)
        symbol_input_->setText(native);

    if (symbol_col_ < 0)
        return;
    QString target = asia_links::yf_symbol_for_code(ref.symbol);
    if (target.isEmpty())
        target = ref.symbol.trimmed();
    for (int r = 0; r < model_->rowCount(); ++r) {
        if (yf_symbol_at_row(r).compare(target, Qt::CaseInsensitive) == 0) {
            suppress_publish_ = true; // a follower must not re-publish
            data_table_->selectRow(r);
            data_table_->scrollTo(model_->index(r, symbol_col_), QAbstractItemView::PositionAtCenter);
            suppress_publish_ = false;
            return;
        }
    }
}

fincept::SymbolRef AsiaMarketsScreen::current_symbol() const {
    const QString yf = yf_symbol_at_row(data_table_ ? data_table_->currentIndex().row() : -1);
    if (yf.isEmpty())
        return {};
    return fincept::SymbolRef::equity(yf, asia_links::exchange_for_yf(yf));
}

void AsiaMarketsScreen::show_row_menu(const QPoint& pos) {
    const QModelIndex idx = data_table_->indexAt(pos);
    if (!idx.isValid())
        return;
    const int row = idx.row();
    const QString yf = yf_symbol_at_row(row);
    if (yf.isEmpty())
        return; // this result has no security-code column
    data_table_->selectRow(row); // act on the row under the cursor (publishes to the group, if linked)
    QMenu menu(this);
    asia_links::add_symbol_actions(menu, this, data_table_, yf, name_at_row(row));
    menu.exec(data_table_->viewport()->mapToGlobal(pos));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap AsiaMarketsScreen::save_state() const {
    // Before the first show the remembered selection is still pending: keep it as it was restored.
    QVariantMap state{
        {"category", active_category_ >= 0 ? active_category_ : std::max(0, restored_category_)},
        // New key: the old "region" was always 0 (the buttons were disabled) and must not turn
        // into a CN A filter when an old state is restored.
        {"region_filter", active_region_},
        {"endpoint", active_category_ >= 0 ? active_endpoint_ : pending_endpoint_},
    };
    if (search_input_)
        state["search"] = search_input_->text();
    if (symbol_input_)
        state["symbol"] = symbol_input_->text();
    // The result itself is remembered by the result pane (with its "as of" time), not here.
    return state;
}

void AsiaMarketsScreen::restore_state(const QVariantMap& state) {
    const int region = state.value("region_filter", -1).toInt();
    if (region != active_region_)
        on_region_changed(region);

    // Symbol first: it is the input of the run the category switch starts.
    if (symbol_input_ && state.contains("symbol"))
        symbol_input_->setText(state.value("symbol").toString());

    // Category and endpoint are opened on the first show (nothing loads while the screen is hidden).
    restored_category_ = std::clamp(state.value("category", 0).toInt(), 0, static_cast<int>(categories_.size()) - 1);
    pending_endpoint_ = state.value("endpoint").toString();
    if (first_show_done_)
        select_category(restored_category_, std::exchange(pending_endpoint_, QString()));

    if (search_input_ && state.contains("search"))
        search_input_->setText(state.value("search").toString());
}

bool AsiaMarketsScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed();
    if (!key.isEmpty()) {
        QString rest;
        if (command_rows::split_kind(key, "region", &rest)) {
            // The region filter buttons (a client-side filter of the endpoint list); "all" clears it.
            int region = rest.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0 ? -1 : -2;
            for (int i = 0; i < regions_.size() && region == -2; ++i)
                if (regions_[i].compare(rest, Qt::CaseInsensitive) == 0)
                    region = i;
            if (region == -2)
                return false;
            if (region != active_region_)
                on_region_changed(region);
        } else {
            if (command_rows::split_kind(key, "category", &rest))
                key = rest;
            int index = -1;
            for (int i = 0; i < categories_.size() && index < 0; ++i)
                if (categories_[i].id == key)
                    index = i;
            for (int i = 0; i < categories_.size() && index < 0; ++i)
                if (categories_[i].id.compare(key, Qt::CaseInsensitive) == 0)
                    index = i;
            if (index < 0)
                return false;
            if (first_show_done_) {
                on_category_changed(index); // the category button's click path
            } else {
                // The first show (one event-loop turn after showEvent) opens this category instead of
                // the remembered one; a remembered endpoint belongs to the remembered category only.
                if (index != restored_category_)
                    pending_endpoint_.clear();
                restored_category_ = index;
            }
        }
    }
    const QString filter = focus.trimmed();
    if (!filter.isEmpty()) {
        // The endpoint filter, set after the first show / category switch (both clear it). Filters
        // the list only: nothing runs.
        QTimer::singleShot(0, this, [this, filter]() {
            if (search_input_)
                search_input_->setText(filter);
        });
    }
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

struct AsiaSectionRow {
    const char* section;
    const char* title;
    const char* keywords;
};

const AsiaSectionRow kAsiaSectionRows[] = {
    {"realtime", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian stock quotes"),
     "realtime, live quotes, spot, a-shares, hong kong stocks, china stocks"},
    {"historical", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian stock price history"),
     "historical, history, ohlc, daily bars, kline, candles"},
    {"financial", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian company financials"),
     "financial statements, balance sheet, income statement, cash flow, earnings"},
    {"holders", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian stock shareholders"),
     "holders, shareholders, ownership, top holders"},
    {"fund_flow", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian fund flows"),
     "fund flow, money flow, capital flow, inflows, outflows"},
    {"boards", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian sector boards"),
     "boards, sectors, industries, concepts"},
    {"margin", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Margin & Stock Connect"),
     "margin, hsgt, stock connect, northbound, southbound, short selling"},
    {"hot_news", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Asian hot stocks & news"),
     "hot stocks, trending, popular, stock news, headlines"},
    {"region:cn_a", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "China A-shares"),
     "a-shares, a shares, mainland china, shanghai, shenzhen, cn a"},
    {"region:cn_b", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "China B-shares"),
     "b-shares, b shares, cn b"},
    {"region:hk", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "Hong Kong stocks"),
     "hong kong, hkex, hang seng, hk stocks"},
    {"region:us", QT_TRANSLATE_NOOP("fincept::screens::AsiaMarketsScreen", "US-listed Chinese stocks"),
     "us listed, adr, chinese adrs, us stocks"},
};

QList<command::CommandItem> asia_command_rows() {
    namespace rows = command_rows;
    const QString where = AsiaMarketsScreen::tr("Asia Markets");
    QList<command::CommandItem> out;
    for (const AsiaSectionRow& r : kAsiaSectionRows) {
        QStringList keywords;
        for (const QString& w : QString::fromLatin1(r.keywords).split(QLatin1Char(','), Qt::SkipEmptyParts))
            keywords.append(w.trimmed());
        keywords << QStringLiteral("asia") << QStringLiteral("akshare");
        out.append(rows::section(QStringLiteral("asia_markets"), QString::fromLatin1(r.section),
                                 QCoreApplication::translate("fincept::screens::AsiaMarketsScreen", r.title), keywords,
                                 where));
    }
    return out;
}

void asia_install_commands() {
    command_rows::install(QStringLiteral("asia_markets"), &asia_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(asia_install_commands)

} // namespace fincept::screens
