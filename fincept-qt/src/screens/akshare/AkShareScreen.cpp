#include "screens/akshare/AkShareScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "screens/asia_markets/AsiaJsonView.h"
#include "screens/asia_markets/AsiaResultTable.h"
#include "screens/asia_markets/AsiaSymbolLinks.h"
#include "screens/common/CommandRows.h"
#include "services/akshare/AkShareService.h"
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
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QResizeEvent>
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
QString ak_style() {
    return
        QString("#akScreen { background: %1; }"

                   "#akHeader { background: %2; border-bottom: 2px solid %3; }"
                   "#akHeaderTitle { color: %4; font-size: " + fincept::ui::fonts::px(-2) + "px; font-weight: 700; background: transparent; }"
                   "#akHeaderSub { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; letter-spacing: 0.5px; background: transparent; }"
                   "#akHeaderBadge { color: %6; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; "
                   "  background: %16; padding: 2px 6px; }"

                   "#akSourceGrid { background: %1; border-bottom: 1px solid %8; }"
                   "#akSourceBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; padding: 6px 4px; text-align: center; }"
                   "#akSourceBtn:hover { color: %4; border-color: %9; }"
                   "#akSourceBtn[active=\"true\"] { background: %15; "
                   "  color: %3; border-color: %3; }"

                   "#akEndpointPanel { background: %7; border-right: 1px solid %8; }"
                   "#akSearchInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 4px 8px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "#akSearchInput:focus { border-color: %9; }"
                   "#akEndpointCount { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"
                   "#akEndpointList { background: %1; border: none; outline: none; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "#akEndpointList::item { color: %5; padding: 4px 8px; border-bottom: 1px solid %8; }"
                   "#akEndpointList::item:hover { color: %4; background: %10; }"
                   "#akEndpointList::item:selected { color: %3; background: %15; "
                   "  border-left: 2px solid %3; }"

                   "#akParamsPanel { background: %7; border-bottom: 1px solid %8; }"
                   "#akParamLabel { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"
                   "#akParamInput { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 3px 6px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "#akParamInput:focus { border-color: %9; }"
                   "QComboBox { background: %1; color: %4; border: 1px solid %8; "
                   "  padding: 3px 6px; font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "QComboBox::drop-down { border: none; width: 16px; }"
                   "QComboBox QAbstractItemView { background: %2; color: %4; border: 1px solid %8; "
                   "  selection-background-color: %3; selection-color: %14; }"

                   "#akExecBtn { background: %3; color: %14; border: none; padding: 4px 16px; "
                   "  font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#akExecBtn:hover { background: %11; }"
                   "#akExecBtn:disabled { background: %7; color: %12; border: 1px solid %8; }"

                   "#akViewToggle { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#akViewToggle:hover { color: %4; }"

                   "#akRefreshBtn { background: %7; color: %5; border: 1px solid %8; "
                   "  padding: 4px 10px; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; }"
                   "#akRefreshBtn:hover { color: %4; }"
                   "#akRefreshBtn:disabled { color: %12; }"

                   "#akDataPanel { background: %1; }"
                   "#akDataToolbar { background: %2; border-bottom: 1px solid %8; }"
                   "#akDataStatus { color: %5; font-size: " + fincept::ui::fonts::px(-3) + "px; background: transparent; }"
                   "#akRecordCount { color: %13; font-size: " + fincept::ui::fonts::px(-5) + "px; font-weight: 700; background: transparent; }"
                   "#akAsOf { color: %12; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"

                   "QTableView { background: %1; color: %4; border: none; gridline-color: %8; "
                   "  font-size: " + fincept::ui::fonts::px(-3) + "px; }"
                   "QTableView::item { padding: 2px 6px; border-bottom: 1px solid %8; }"
                   "QTableView::item:selected { background: %15; color: %3; }"
                   "QHeaderView::section { background: %2; color: %5; border: none; "
                   "  border-bottom: 1px solid %8; border-right: 1px solid %8; "
                   "  padding: 4px 6px; font-size: " + fincept::ui::fonts::px(-4) + "px; font-weight: 700; }"

                   "#akJsonView { background: %1; color: %13; border: none; font-size: " + fincept::ui::fonts::px(-3) + "px; }"

                   "#akStatusBar { background: %2; border-top: 1px solid %8; }"
                   "#akStatusText { color: %5; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"
                   "#akStatusHighlight { color: %13; font-size: " + fincept::ui::fonts::px(-5) + "px; background: transparent; }"

                   "QScrollBar:vertical { background: %1; width: 6px; }"
                   "QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                   "QScrollBar:horizontal { background: %1; height: 6px; }"
                   "QScrollBar::handle:horizontal { background: %8; min-width: 20px; }"
                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

                   "QSplitter::handle { background: %8; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::SUCCESS())        // %6 "FREE API" badge
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::BG_HOVER())       // %10
        .arg(colors::AMBER_DIM())      // %11
        .arg(colors::TEXT_TERTIARY())  // %12 disabled text, "as of"
        .arg(colors::CYAN())           // %13
        .arg(colors::TEXT_ON_ACCENT()) // %14 text on accent fills
        .arg(colors::ACCENT_BG())      // %15
        .arg(colors::tint(colors::SUCCESS, 0.2)); // %16
}

/// Item colour of the endpoint list's category headers.
QColor ak_header_color() {
    return QColor(colors::AMBER());
}

/// Most rows written into the JSON view (the table shows every row).
constexpr int kAkJsonRows = 1000;
/// Remembered results: refreshed quietly when older than this on a later visit — the pane's default;
/// each view gets its own (asia_links::result_max_age_s: 5 min for quotes, 6 h for daily / macro data).
constexpr int kAkResultMaxAgeS = 15 * 60;
/// Largest result kept across restarts (compact JSON).
constexpr int kAkPersistBytes = 2 * 1024 * 1024;
} // namespace

namespace fincept::screens {

using namespace fincept::ui;
namespace akshare_svc = fincept::services::akshare;

// ── Data source definitions ─────────────────────────────────────────────────

/// List position the removed "Global Economics" source used to occupy. Saved states from before its
/// removal carry an integer "source" index; positions after it moved up by one.
static constexpr int kAkShareRemovedSourceIndex = 21;

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
        // "Global Economics" (akshare_economics_global.py) was removed: the script was a stub that
        // always exited with an error, and the global-macro endpoints it meant to wrap are served by
        // "Macro Global" (akshare_macro.py) below. kAkShareRemovedSourceIndex keeps old saved states
        // (which stored the source by list position) pointing at the right source.
        {"funds_expanded", "Funds Expanded", "akshare_funds_expanded.py", "Fd"},
        {"company_info", "Company Info", "akshare_company_info.py", "Co"},
        {"macro", "Macro Global", "akshare_macro.py", "Mc"},
        {"misc", "Miscellaneous", "akshare_misc.py", ".."},
    };
}

// ── Constructor ─────────────────────────────────────────────────────────────

AkShareScreen::AkShareScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("akScreen");
    setStyleSheet(ak_style());
    sources_ = build_sources();
    setup_ui();
    // Item brushes / fonts of the list's category headers, the table row height and the source grid's
    // column count are not reached by stylesheets: refit once the new appearance has settled. Table
    // cell colours come from the model (live tokens), so nothing walks the cells.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, &AkShareScreen::apply_appearance);
    LOG_DEBUG("AkShare", "Screen constructed");
}

// ── Show / hide ──────────────────────────────────────────────────────────────

void AkShareScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!first_show_done_) {
        // One event-loop turn later: the router shows a new screen before it restores its state.
        QTimer::singleShot(0, this, [this]() {
            if (isVisible())
                first_show();
        });
    } else if (!endpoints_loading_ && active_source_ >= 0 && endpoint_list_->count() == 0 &&
               list_state_kind_ == ListState::Loading) {
        // The endpoint list was interrupted by leaving the screen: load it again.
        load_endpoints(sources_[active_source_]);
    }
}

void AkShareScreen::hideEvent(QHideEvent* event) {
    // Leaving the screen stops an endpoint-list run (the result pane cancels its own default run).
    if (list_cancel_) {
        list_cancel_->store(true);
        list_cancel_.reset();
        endpoints_loading_ = false;
    }
    QWidget::hideEvent(event);
}

void AkShareScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (event->size().width() != event->oldSize().width() && !ThemeManager::instance().notifying())
        reflow_sources();
}

void AkShareScreen::first_show() {
    if (first_show_done_)
        return;
    first_show_done_ = true;
    int src = 0;
    for (int i = 0; i < sources_.size() && !restored_source_id_.isEmpty(); ++i) {
        if (sources_[i].id == restored_source_id_) {
            src = i;
            break;
        }
    }
    const QString endpoint = std::exchange(pending_endpoint_, QString());
    select_source(src, endpoint);
}

// ── Live language switch ─────────────────────────────────────────────────────

void AkShareScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AkShareScreen::retranslateUi() {
    // Header
    if (header_title_)
        header_title_->setText(tr("AKSHARE DATA EXPLORER"));
    if (header_sub_)
        header_sub_->setText(tr("1000+ CHINESE & GLOBAL FINANCIAL DATA ENDPOINTS"));
    if (header_badge_)
        header_badge_->setText(tr("FREE API"));

    // Endpoint panel — endpoint_count_ / data_status_ carry data-derived text
    // refreshed on the next source load, so we only re-apply static chrome here.
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search endpoints..."));

    // Parameter labels
    if (sym_label_)
        sym_label_->setText(tr("SYMBOL"));
    if (start_label_)
        start_label_->setText(tr("START"));
    if (end_label_)
        end_label_->setText(tr("END"));
    if (period_label_)
        period_label_->setText(tr("PERIOD"));
    if (param_symbol_)
        param_symbol_->setPlaceholderText(tr("e.g. 000001"));
    if (param_period_) {
        param_period_->setItemText(0, tr("Default"));
        param_period_->setToolTip(tr("Bar size for endpoints that take a daily / weekly / monthly period"));
    }
    if (exec_btn_)
        exec_btn_->setText(tr("EXECUTE"));

    // Data panel toolbar
    if (view_toggle_btn_)
        view_toggle_btn_->setText(is_table_view_ ? tr("JSON") : tr("TABLE"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));

    // Status bar
    if (status_left_)
        status_left_->setText(tr("AKSHARE DATA"));
    if (status_source_ && active_source_ < 0)
        status_source_->setText(tr("SOURCE: --"));
}

void AkShareScreen::apply_appearance() {
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        QListWidgetItem* item = endpoint_list_->item(i);
        if (item->flags() & Qt::ItemIsSelectable)
            continue; // only the category headers carry their own colour and font
        item->setForeground(ak_header_color());
        item->setFont(fonts::font(-3, QFont::Bold));
    }
    data_table_->verticalHeader()->setDefaultSectionSize(asia_links::result_row_height());
    if (model_->rowCount() > 0)
        asia_links::fit_columns(data_table_, model_);
    source_cols_ = 0; // button widths changed with the font: lay the grid out again
    reflow_sources();
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
    left->setMinimumWidth(200);
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
    bar->setMinimumHeight(42);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    auto* title_col = new QVBoxLayout;
    title_col->setSpacing(0);
    header_title_ = new QLabel(tr("AKSHARE DATA EXPLORER"));
    header_title_->setObjectName("akHeaderTitle");
    header_sub_ = new QLabel(tr("1000+ CHINESE & GLOBAL FINANCIAL DATA ENDPOINTS"));
    header_sub_->setObjectName("akHeaderSub");
    title_col->addWidget(header_title_);
    title_col->addWidget(header_sub_);
    hl->addLayout(title_col);
    hl->addStretch(1);

    header_badge_ = new QLabel(tr("FREE API"));
    header_badge_->setObjectName("akHeaderBadge");
    hl->addWidget(header_badge_);

    return bar;
}

QWidget* AkShareScreen::create_source_grid() {
    // The buttons are laid out in as many columns as the width allows (reflow_sources()), so the grid
    // wraps instead of forcing the window wider at 1280 px or with a large font.
    source_grid_host_ = new QWidget(this);
    source_grid_host_->setObjectName("akSourceGrid");
    source_grid_ = new QGridLayout(source_grid_host_);
    source_grid_->setContentsMargins(8, 6, 8, 6);
    source_grid_->setSpacing(4);

    for (int i = 0; i < sources_.size(); ++i) {
        // A button reads a single "&" as a keyboard-shortcut marker ("Hot & News").
        QString text = sources_[i].icon + " " + sources_[i].name;
        text.replace(QLatin1Char('&'), QLatin1String("&&"));
        auto* btn = new QPushButton(text, source_grid_host_);
        btn->setObjectName("akSourceBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("active", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_source_clicked(i); });
        source_btns_.append(btn);
    }
    reflow_sources();
    return source_grid_host_;
}

void AkShareScreen::reflow_sources() {
    if (!source_grid_ || source_btns_.isEmpty())
        return;
    int cell = 0;
    for (auto* b : std::as_const(source_btns_))
        cell = std::max(cell, b->sizeHint().width());
    const int n = static_cast<int>(source_btns_.size());
    const int avail = width() - 16;
    // Before the first layout (width 0) start narrow: the grid must not make the screen wider.
    int cols = avail > 0 ? (avail + 4) / std::max(1, cell + 4) : 5;
    cols = std::clamp(cols, 2, 13);
    const int rows = (n + cols - 1) / cols;
    cols = (n + rows - 1) / rows; // same number of rows, columns as even as possible
    if (cols == source_cols_)
        return;
    source_cols_ = cols;
    for (auto* b : std::as_const(source_btns_))
        source_grid_->removeWidget(b);
    for (int i = 0; i < n; ++i)
        source_grid_->addWidget(source_btns_[i], i / cols, i % cols);
    for (int c = 0; c < 13; ++c)
        source_grid_->setColumnStretch(c, c < cols ? 1 : 0);
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
    search_input_->setPlaceholderText(tr("Search endpoints..."));
    search_input_->setAccessibleName(tr("Filter endpoints"));
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::textChanged, this, &AkShareScreen::on_search_changed);

    endpoint_count_ = new QLabel(tr("%1 endpoints").arg(0));
    endpoint_count_->setObjectName("akEndpointCount");

    sbl->addWidget(search_input_, 1);
    sbl->addWidget(endpoint_count_);
    vl->addWidget(search_bar);

    // Endpoint list | loading skeleton | designed state (failed / nothing listed / no match)
    list_stack_ = new QStackedWidget(panel);

    endpoint_list_ = new QListWidget(list_stack_);
    endpoint_list_->setObjectName("akEndpointList");
    endpoint_list_->setAccessibleName(tr("Available endpoints"));
    connect(endpoint_list_, &QListWidget::itemClicked, this, &AkShareScreen::on_endpoint_clicked);
    list_stack_->addWidget(endpoint_list_);

    list_skeleton_ = new LoadingSkeleton(LoadingSkeleton::Shape::List, list_stack_);
    list_skeleton_->set_row_count(12);
    list_stack_->addWidget(list_skeleton_);

    list_state_ = new EmptyState(list_stack_);
    list_state_->set_compact(true);
    connect(list_state_, &EmptyState::primary_clicked, this, [this]() {
        if (list_state_kind_ == ListState::Filtered) {
            search_input_->clear();
        } else if (list_state_kind_ == ListState::Failed && active_source_ >= 0) {
            ++request_seq_;
            load_endpoints(sources_[active_source_]);
        }
    });
    list_stack_->addWidget(list_state_);
    list_stack_->setCurrentWidget(list_skeleton_); // the first show opens a source at once
    list_state_kind_ = ListState::Loading;

    vl->addWidget(list_stack_, 1);
    return panel;
}

QWidget* AkShareScreen::create_params_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("akParamsPanel");
    panel->setMinimumHeight(38);

    auto* hl = new QHBoxLayout(panel);
    hl->setContentsMargins(12, 4, 12, 4);
    hl->setSpacing(8);

    // Inputs take their natural (font-sized) width; nothing here is a fixed pixel width, so the row
    // fits at 1280 px with a large font.

    // Symbol
    sym_label_ = new QLabel(tr("SYMBOL"));
    sym_label_->setObjectName("akParamLabel");
    // Empty by default: a pre-filled code was sent to every endpoint, including those whose
    // parameter is a date or a board name. Each endpoint uses its own default when this is empty.
    param_symbol_ = new QLineEdit;
    param_symbol_->setPlaceholderText(tr("e.g. 000001"));
    param_symbol_->setObjectName("akParamInput");
    param_symbol_->setAccessibleName(tr("Symbol parameter"));
    connect(param_symbol_, &QLineEdit::returnPressed, this, &AkShareScreen::on_execute);

    // Start date
    start_label_ = new QLabel(tr("START"));
    start_label_->setObjectName("akParamLabel");
    param_start_ = new QLineEdit;
    param_start_->setObjectName("akParamInput");
    param_start_->setPlaceholderText("YYYY-MM-DD");

    // End date
    end_label_ = new QLabel(tr("END"));
    end_label_->setObjectName("akParamLabel");
    param_end_ = new QLineEdit;
    param_end_->setObjectName("akParamInput");
    param_end_->setPlaceholderText("YYYY-MM-DD");

    // Period
    period_label_ = new QLabel(tr("PERIOD"));
    period_label_->setObjectName("akParamLabel");
    param_period_ = new QComboBox;
    // Item data is the value sent as period=<value>; the first entry sends nothing (endpoint default).
    // The scripts apply it only to endpoints whose `period` is a daily/weekly/monthly bar size.
    param_period_->addItem(tr("Default"), QString());
    param_period_->addItem(QStringLiteral("daily"), QStringLiteral("daily"));
    param_period_->addItem(QStringLiteral("weekly"), QStringLiteral("weekly"));
    param_period_->addItem(QStringLiteral("monthly"), QStringLiteral("monthly"));
    param_period_->setToolTip(tr("Bar size for endpoints that take a daily / weekly / monthly period"));

    // Execute button
    exec_btn_ = new QPushButton(tr("EXECUTE"));
    exec_btn_->setObjectName("akExecBtn");
    exec_btn_->setCursor(Qt::PointingHandCursor);
    exec_btn_->setEnabled(false);
    connect(exec_btn_, &QPushButton::clicked, this, &AkShareScreen::on_execute);

    hl->addWidget(sym_label_);
    hl->addWidget(param_symbol_);
    hl->addSpacing(4);
    hl->addWidget(start_label_);
    hl->addWidget(param_start_);
    hl->addSpacing(4);
    hl->addWidget(end_label_);
    hl->addWidget(param_end_);
    hl->addSpacing(4);
    hl->addWidget(period_label_);
    hl->addWidget(param_period_);
    hl->addStretch(1);
    hl->addWidget(exec_btn_);

    param_start_->setAccessibleName(tr("Start date parameter"));
    param_end_->setAccessibleName(tr("End date parameter"));
    param_period_->setAccessibleName(tr("Period parameter"));
    exec_btn_->setAccessibleName(tr("Execute query"));
    connect(param_start_, &QLineEdit::returnPressed, this, &AkShareScreen::on_execute);
    connect(param_end_, &QLineEdit::returnPressed, this, &AkShareScreen::on_execute);

    // The parameters are the result's inputs: a remembered result made from others says so.
    for (QLineEdit* e : {param_symbol_, param_start_, param_end_})
        connect(e, &QLineEdit::textChanged, this, &AkShareScreen::update_signature);
    connect(param_period_, &QComboBox::currentIndexChanged, this, &AkShareScreen::update_signature);

    // Explicit left-to-right tab order through the parameter form.
    QWidget::setTabOrder(param_symbol_, param_start_);
    QWidget::setTabOrder(param_start_, param_end_);
    QWidget::setTabOrder(param_end_, param_period_);
    QWidget::setTabOrder(param_period_, exec_btn_);

    return panel;
}

QWidget* AkShareScreen::create_data_panel() {
    auto* panel = new QWidget(this);
    panel->setObjectName("akDataPanel");

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar row
    auto* toolbar = new QWidget(panel);
    toolbar->setObjectName("akDataToolbar");
    toolbar->setMinimumHeight(30);
    auto* tbl = new QHBoxLayout(toolbar);
    tbl->setContentsMargins(12, 0, 12, 0);
    tbl->setSpacing(8);

    data_status_ = new QLabel(tr("Ready"));
    data_status_->setObjectName("akDataStatus");
    // Long statuses ("… — not used by this endpoint: SYMBOL, START") are clipped, not allowed to
    // widen the screen; the full text is the tooltip.
    data_status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    data_status_->setMinimumWidth(0);

    record_count_ = new QLabel;
    record_count_->setObjectName("akRecordCount");
    record_count_->hide();

    as_of_label_ = new QLabel;
    as_of_label_->setObjectName("akAsOf");

    view_toggle_btn_ = new QPushButton(tr("JSON"));
    view_toggle_btn_->setObjectName("akViewToggle");
    view_toggle_btn_->setCursor(Qt::PointingHandCursor);
    connect(view_toggle_btn_, &QPushButton::clicked, this, &AkShareScreen::on_view_toggle);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setObjectName("akRefreshBtn");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setEnabled(false);
    connect(refresh_btn_, &QPushButton::clicked, this, &AkShareScreen::on_refresh);

    tbl->addWidget(data_status_, 1);
    tbl->addWidget(record_count_);
    tbl->addWidget(as_of_label_);
    tbl->addWidget(view_toggle_btn_);
    tbl->addWidget(refresh_btn_);
    vl->addWidget(toolbar);

    // The result pane: remembered result / skeleton / designed failure pages around the table | JSON.
    pane_ = new AutoLoadPane(AutoLoadPane::Policy::AutoRun, panel);
    pane_->setObjectName("akResultPane");

    view_stack_ = new QStackedWidget(pane_->content_parent());

    data_table_ = new QTableView(view_stack_);
    model_ = new asia_links::ResultModel(data_table_);
    data_table_->setModel(model_);
    data_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    data_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    data_table_->setAlternatingRowColors(false);
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
    connect(data_table_, &QTableView::customContextMenuRequested, this, &AkShareScreen::show_row_menu);
    connect(data_table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { publish_selection_to_group(); });

    json_view_ = new QPlainTextEdit(view_stack_);
    json_view_->setObjectName("akJsonView");
    json_view_->setReadOnly(true);
    json_view_->setLineWrapMode(QPlainTextEdit::NoWrap);

    view_stack_->addWidget(data_table_);
    view_stack_->addWidget(json_view_);
    pane_->set_content(view_stack_);

    pane_->skeleton()->set_shape(LoadingSkeleton::Shape::Table);
    pane_->skeleton()->set_row_count(12);
    pane_->set_cache_key(QStringLiteral("akshare/result"), 7);
    pane_->set_persist_limit(kAkPersistBytes);
    pane_->set_max_age(kAkResultMaxAgeS);
    // Results over the persist limit come back only by running again (macro_shipping_bdi: 110 s), so
    // the pane keeps more than its default 32 MB; sizes come from the decode thread's estimate in the
    // payload (O(1) here — the default estimator walks the whole result on the GUI thread).
    pane_->set_memory_budget(asia_links::kResultMemoryBudgetBytes);
    pane_->set_size_estimator(&asia_links::result_payload_bytes);
    pane_->set_auto_delay(300); // users flick through sources and endpoints
    pane_->set_requirements({Requirement::python()});
    pane_->set_status_label(as_of_label_);
    pane_->set_start([this](quint64 id) { start_query(id); });
    pane_->set_cancel([this](quint64 id) { cancel_query(id); });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_result(payload); });

    vl->addWidget(pane_, 1);
    return panel;
}

QWidget* AkShareScreen::create_status_bar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("akStatusBar");
    bar->setMinimumHeight(26);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(16, 0, 16, 0);

    status_left_ = new QLabel(tr("AKSHARE DATA"));
    status_left_->setObjectName("akStatusText");
    hl->addWidget(status_left_);
    hl->addStretch(1);

    status_source_ = new QLabel(tr("SOURCE: --"));
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
    // Re-clicking the active source is a no-op, unless its endpoint list is empty (the load
    // failed) — then it is the only way to retry.
    if (index == active_source_ && endpoint_list_->count() > 0)
        return;
    select_source(index, QString());
}

void AkShareScreen::select_source(int index, const QString& endpoint) {
    if (index < 0 || index >= sources_.size())
        return;
    const AkShareSource& src = sources_[index];
    const bool same = index == active_source_;
    active_source_ = index;

    for (int i = 0; i < source_btns_.size(); ++i) {
        if (source_btns_[i]->property("active").toBool() == (i == index))
            continue;
        source_btns_[i]->setProperty("active", i == index);
        source_btns_[i]->style()->unpolish(source_btns_[i]);
        source_btns_[i]->style()->polish(source_btns_[i]);
    }
    status_source_->setText(tr("SOURCE: %1").arg(src.name.toUpper()));
    if (!same) {
        const QSignalBlocker block(search_input_);
        search_input_->clear();
    }
    LOG_DEBUG("AkShare", "Source selected: " + src.name);

    // The endpoint to open: the one asked for, else the one last opened in this source, else the
    // source's default (keyless, quick — asia_links::default_endpoint). Known before the list
    // arrives, so the result (remembered or fetched) does not wait for it.
    QString ep = endpoint;
    if (ep.isEmpty())
        ep = last_endpoint_by_source_.value(src.id);
    if (ep.isEmpty())
        ep = asia_links::default_endpoint(src.script);
    active_endpoint_ = ep;

    if (!same || endpoint_list_->count() == 0) {
        ++request_seq_; // anything still in flight belongs to the previous source
        if (endpoint_cache_.contains(src.script))
            populate_endpoint_list(endpoint_cache_[src.script]);
        else
            load_endpoints(src);
    }
    select_endpoint(active_endpoint_);
}

void AkShareScreen::select_endpoint(const QString& endpoint) {
    active_endpoint_ = endpoint;
    const bool have = active_source_ >= 0 && !endpoint.isEmpty();
    if (have)
        last_endpoint_by_source_.insert(sources_[active_source_].id, endpoint);
    status_endpoint_->setText(endpoint);
    highlight_endpoint(endpoint);
    exec_btn_->setEnabled(have);
    refresh_btn_->setEnabled(have);
    if (active_source_ >= 0) {
        pane_->set_loading_text(have ? tr("Querying %1 · %2…").arg(sources_[active_source_].name, endpoint)
                                     : tr("Loading the endpoint list of %1…").arg(sources_[active_source_].name));
    }
    if (!have) {
        data_status_->setText(tr("Loading endpoints…"));
        data_status_->setToolTip(QString());
    }
    // Freshness per endpoint (quotes 5 min, daily / macro data 6 h), and the inputs signature set
    // before the switch so the view's first look ("Inputs changed", its default run) already uses it.
    const QString view = view_key();
    if (have)
        pane_->set_view_max_age(view, asia_links::result_max_age_s(sources_[active_source_].id, endpoint));
    pane_->set_view(view, AutoLoadPane::Policy::AutoRun, args_signature());
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AkShareScreen::on_endpoint_clicked(QListWidgetItem* item) {
    if (!item)
        return;
    const QString ep = item->data(Qt::UserRole).toString();
    if (ep.isEmpty())
        return; // category header — keep the active endpoint
    // Opens the endpoint's result: the remembered one at once (refreshed when old), else a run.
    // Picking another endpoint while a query runs supersedes it.
    select_endpoint(ep);
}

void AkShareScreen::on_search_changed(const QString& text) {
    // Endpoints first; a category header is shown only while one of its endpoints is.
    QListWidgetItem* header = nullptr;
    bool header_has_visible = false;
    int visible = 0;
    for (int i = 0; i < endpoint_list_->count(); ++i) {
        auto* item = endpoint_list_->item(i);
        const QString ep = item->data(Qt::UserRole).toString();
        if (ep.isEmpty()) {
            if (header)
                header->setHidden(!header_has_visible);
            header = item;
            header_has_visible = false;
            continue;
        }
        const bool match = text.isEmpty() || item->text().contains(text, Qt::CaseInsensitive) ||
                           ep.contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
        header_has_visible = header_has_visible || match;
        visible += match ? 1 : 0;
    }
    if (header)
        header->setHidden(!header_has_visible);
    if (list_state_kind_ == ListState::List || list_state_kind_ == ListState::Filtered) {
        if (visible == 0 && endpoint_list_->count() > 0)
            show_list_state(ListState::Filtered, text);
        else
            show_list_state(ListState::List);
    }
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AkShareScreen::on_execute() {
    if (active_endpoint_.isEmpty())
        return;
    pane_->run();
}

void AkShareScreen::on_view_toggle() {
    is_table_view_ = !is_table_view_;
    view_stack_->setCurrentIndex(is_table_view_ ? 0 : 1);
    view_toggle_btn_->setText(is_table_view_ ? tr("JSON") : tr("TABLE"));
    if (!is_table_view_)
        fill_json_view();
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AkShareScreen::on_refresh() {
    // A fresh run of the endpoint on screen (the remembered result stays, dimmed, until it lands).
    if (active_endpoint_.isEmpty())
        return;
    pane_->run();
}

QString AkShareScreen::query_status_text(const QString& endpoint, const QStringList& ignored) {
    if (ignored.isEmpty())
        return endpoint;
    return tr("%1 — not used by this endpoint: %2").arg(endpoint, ignored.join(QStringLiteral(", ")));
}

// ── Endpoint list ───────────────────────────────────────────────────────────

void AkShareScreen::show_list_state(ListState state, const QString& detail) {
    list_state_kind_ = state;
    switch (state) {
        case ListState::List:
            list_stack_->setCurrentWidget(endpoint_list_);
            return;
        case ListState::Loading:
            list_skeleton_->set_message(tr("Loading endpoints…"));
            list_stack_->setCurrentWidget(list_skeleton_);
            return;
        case ListState::Failed:
            list_state_->apply(EmptyState::error_spec(tr("The endpoint list of %1 did not load.")
                                                          .arg(active_source_ >= 0 ? sources_[active_source_].name
                                                                                   : QString()),
                                                      tr("Could not list endpoints"), tr("RETRY"), detail));
            break;
        case ListState::Empty:
            list_state_->apply(EmptyState::no_data_spec(
                tr("No endpoints"), tr("%1 publishes no endpoints.")
                                        .arg(active_source_ >= 0 ? sources_[active_source_].name : QString())));
            break;
        case ListState::Filtered:
            list_state_->apply(EmptyState::no_data_spec(tr("No match"),
                                                        tr("No endpoint matches “%1”.").arg(detail),
                                                        tr("CLEAR SEARCH")));
            break;
    }
    list_stack_->setCurrentWidget(list_state_);
}

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

    if (list_cancel_)
        list_cancel_->store(true); // the list of the previous source is no longer wanted
    list_cancel_ = std::make_shared<std::atomic<bool>>(false);
    endpoints_loading_ = true;
    endpoint_list_->clear();
    endpoint_count_->setText(tr("%1 endpoints").arg(0));
    show_list_state(ListState::Loading);

    QPointer<AkShareScreen> self = this;
    const int seq = request_seq_;
    const auto token = list_cancel_;

    akshare_svc::AkShareService::instance().fetch_endpoints(
        source.script,
        [self, seq, script = source.script, cache_key, token](const akshare_svc::EndpointsResult& r) {
            if (!self || r.cancelled)
                return;

            // Cache a good listing even when the user has already moved on to another source...
            if (r.success) {
                fincept::CacheManager::instance().put(
                    cache_key, QVariant(QString::fromUtf8(QJsonDocument(r.data).toJson(QJsonDocument::Compact))),
                    60 * 60, "akshare");
                self->endpoint_cache_[script] = r.data;
            }
            // ...but never paint a stale listing over the current source's list.
            if (seq != self->request_seq_)
                return;
            if (self->list_cancel_ == token)
                self->list_cancel_.reset();
            self->endpoints_loading_ = false;

            if (!r.success) {
                const QString why = r.error.isEmpty() ? AkShareScreen::tr("Failed to load endpoints") : r.error;
                self->endpoint_count_->setText(AkShareScreen::tr("%1 endpoints").arg(0));
                self->show_list_state(ListState::Failed, why.left(600));
                LOG_ERROR("AkShare", "Endpoint load failed for " + script + ": " + why.left(300));
                if (self->waiting_id_ != 0) {
                    // The result pane was waiting for an endpoint to run: same failure, same RETRY.
                    const quint64 id = std::exchange(self->waiting_id_, 0);
                    QPointer<AkShareScreen> s2 = self;
                    self->pane_->fail(id,
                                      EmptyState::error_spec(AkShareScreen::tr("The endpoint list did not load, so "
                                                                               "there is nothing to run yet."),
                                                             AkShareScreen::tr("Could not list endpoints"),
                                                             AkShareScreen::tr("RETRY"), why.left(600)),
                                      [s2]() {
                                          if (!s2 || s2->active_source_ < 0)
                                              return;
                                          ++s2->request_seq_;
                                          s2->load_endpoints(s2->sources_[s2->active_source_]);
                                          s2->pane_->run();
                                      });
                }
                return;
            }

            self->populate_endpoint_list(r.data);
        },
        list_cancel_);
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
    // Flat lists: "available_endpoints" (most scripts) or "endpoints" (akshare_alternative).
    const char* flat_key = data_obj.contains("available_endpoints") ? "available_endpoints" : "endpoints";

    if (data_obj.contains("categories") && data_obj["categories"].isObject()) {
        auto add_header = [this](const QString& title) {
            // Category header (non-selectable)
            auto* header = new QListWidgetItem(title.toUpper());
            header->setFlags(header->flags() & ~Qt::ItemIsSelectable);
            header->setForeground(ak_header_color());
            header->setFont(fonts::font(-3, QFont::Bold));
            endpoint_list_->addItem(header);
        };
        QSet<QString> placed;
        auto categories = data_obj["categories"].toObject();
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            add_header(it.key());

            // Add endpoints under this category
            if (it.value().isArray()) {
                for (auto ep : it.value().toArray()) {
                    QString ep_name = ep.toString();
                    auto* item = new QListWidgetItem("  " + ep_name);
                    item->setData(Qt::UserRole, ep_name);
                    endpoint_list_->addItem(item);
                    if (!placed.contains(ep_name)) {
                        placed.insert(ep_name);
                        all_endpoints << ep_name;
                    }
                }
            }
        }

        // Several scripts' categories are a hand-picked sample, not a partition of the full list:
        // whatever the full list has that no category placed goes under OTHER, so it stays reachable.
        QStringList rest;
        for (const auto& ep : data_obj.value(flat_key).toArray()) {
            const QString ep_name = ep.toString();
            if (!ep_name.isEmpty() && !placed.contains(ep_name)) {
                placed.insert(ep_name);
                rest << ep_name;
            }
        }
        if (!rest.isEmpty()) {
            add_header(tr("Other"));
            for (const QString& ep_name : rest) {
                auto* item = new QListWidgetItem("  " + ep_name);
                item->setData(Qt::UserRole, ep_name);
                endpoint_list_->addItem(item);
                all_endpoints << ep_name;
            }
        }
    } else {
        if (data_obj.value(flat_key).isArray()) {
            for (auto ep : data_obj.value(flat_key).toArray()) {
                QString ep_name = ep.toString();
                auto* item = new QListWidgetItem(ep_name);
                item->setData(Qt::UserRole, ep_name);
                endpoint_list_->addItem(item);
                all_endpoints << ep_name;
            }
        }
    }

    endpoint_count_->setText(tr("%1 endpoints").arg(all_endpoints.size()));
    LOG_DEBUG("AkShare", "Loaded " + QString::number(all_endpoints.size()) + " endpoints");
    if (all_endpoints.isEmpty()) {
        show_list_state(ListState::Empty);
        if (waiting_id_ != 0) {
            const quint64 id = std::exchange(waiting_id_, 0);
            pane_->fail(id, EmptyState::no_data_spec(tr("No endpoints"),
                                                     tr("%1 publishes no endpoints to query.")
                                                         .arg(active_source_ >= 0 ? sources_[active_source_].name
                                                                                  : QString())),
                        []() {});
        }
        return;
    }
    show_list_state(ListState::List);
    on_search_changed(search_input_->text()); // keep a typed filter on the new list

    if (active_endpoint_.isEmpty())
        select_endpoint(all_endpoints.first()); // no default known for this source: its first endpoint
    else
        highlight_endpoint(active_endpoint_);
}

void AkShareScreen::highlight_endpoint(const QString& endpoint) {
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

QString AkShareScreen::view_key() const {
    if (active_source_ < 0 || active_source_ >= sources_.size())
        return {};
    return sources_[active_source_].id + QLatin1Char('/') + active_endpoint_;
}

QStringList AkShareScreen::current_args() const {
    // Pass the filled boxes by name, not position: endpoints do not share one parameter order
    // (stock_zh_a_hist is symbol, period, start_date, ...; stock_zt_pool_em takes only a date).
    // The scripts' dispatcher (akshare_screen_args.py) applies each value only where the endpoint
    // has a matching parameter and reports the rest back as "ignored_params".
    QStringList args;
    const QString symbol = param_symbol_->text().trimmed();
    const QString start = param_start_->text().trimmed();
    const QString end = param_end_->text().trimmed();
    const QString period = param_period_->currentData().toString();
    if (!symbol.isEmpty())
        args << QStringLiteral("symbol=") + symbol;
    if (!start.isEmpty())
        args << QStringLiteral("start_date=") + start;
    if (!end.isEmpty())
        args << QStringLiteral("end_date=") + end;
    if (!period.isEmpty())
        args << QStringLiteral("period=") + period;
    return args;
}

QString AkShareScreen::args_signature() const {
    return autoload::signature_of(QVariantMap{{QStringLiteral("args"), current_args()}});
}

void AkShareScreen::update_signature() {
    if (pane_)
        pane_->set_signature(args_signature());
    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AkShareScreen::start_query(quint64 request_id) {
    if (active_source_ < 0 || active_endpoint_.isEmpty()) {
        // No endpoint yet: the list is still loading, and its arrival picks one (switching the view
        // cancels this wait).
        if (endpoints_loading_) {
            waiting_id_ = request_id;
            return;
        }
        pane_->fail(request_id, tr("Pick an endpoint in the list on the left."), tr("No endpoint selected"));
        return;
    }
    const QString script = sources_[active_source_].script;
    const QString endpoint = active_endpoint_;
    QStringList args = current_args();
    // Scripts whose every endpoint needs a SYMBOL open on a sample one while the box is empty (the
    // status line says so), instead of on a "needs an input" page.
    QString sample;
    if (param_symbol_->text().trimmed().isEmpty()) {
        sample = asia_links::default_symbol(script, endpoint);
        if (!sample.isEmpty())
            args << QStringLiteral("symbol=") + sample;
    }
    const auto token = std::make_shared<std::atomic<bool>>(false);
    query_tokens_.insert(request_id, token);

    QPointer<AkShareScreen> self = this;
    akshare_svc::AkShareService::instance().query(
        script, endpoint, args,
        [self, request_id, endpoint, sample](const akshare_svc::QueryResult& r) {
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
                    EmptyState::no_data_spec(AkShareScreen::tr("No data for these inputs"),
                                             AkShareScreen::tr("%1 answered with an empty table — the market may be "
                                                               "closed for the day, or the parameters match "
                                                               "nothing. Try another date or symbol.")
                                                 .arg(endpoint),
                                             AkShareScreen::tr("RUN AGAIN")),
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
            if (!sample.isEmpty())
                payload.insert(QStringLiteral("sample_symbol"), sample);
            self->pane_->deliver(request_id, payload);
            LOG_DEBUG("AkShare", "Query " + endpoint + ": " + QString::number(r.rows.size()) + " records");
        },
        token);
}

void AkShareScreen::cancel_query(quint64 request_id) {
    if (waiting_id_ == request_id)
        waiting_id_ = 0;
    if (auto token = query_tokens_.take(request_id))
        token->store(true); // the runner kills the script (or never starts it)
}

void AkShareScreen::fail_query(quint64 request_id, const QString& endpoint, const akshare_svc::QueryResult& r) {
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
        spec.message = tr("It needs a value for: %1. Type it in the box above and press EXECUTE.").arg(missing);
        const bool needs_symbol = structured ? asia_links::missing_needs_symbol(r.missing)
                                             : missing.contains(QLatin1String("SYMBOL"));
        const bool symbol = needs_symbol && param_symbol_->text().trimmed().isEmpty();
        if (symbol) {
            spec.primary = tr("RUN WITH %1").arg(asia_links::sample_symbol());
            spec.details = {{tr("Sample symbol"), asia_links::sample_symbol()}};
            spec.details_heading = tr("TRY IT WITH");
        }
        // The box to type into: the one filling the first missing parameter (SYMBOL when unknown).
        const QString first_box =
            structured ? asia_links::parameter_box(r.missing.first()) : missing.section(QLatin1Char('('), 1);
        QWidget* box = param_symbol_;
        if (!needs_symbol && first_box.contains(QLatin1String("START")))
            box = param_start_;
        else if (!needs_symbol && first_box.contains(QLatin1String("END")))
            box = param_end_;
        else if (!needs_symbol && first_box.contains(QLatin1String("PERIOD")))
            box = param_period_;
        QPointer<AkShareScreen> self = this;
        QPointer<QWidget> focus_box = box;
        pane_->fail(request_id, spec, [self, symbol, focus_box]() {
            if (!self)
                return;
            if (symbol) {
                self->param_symbol_->setText(asia_links::sample_symbol());
                self->pane_->run();
            } else if (focus_box) {
                focus_box->setFocus();
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
        QPointer<AkShareScreen> self = this;
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

void AkShareScreen::render_result(const QJsonValue& payload) {
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
    QString status = query_status_text(endpoint, ignored);
    const QString sample = p.value(QStringLiteral("sample_symbol")).toString();
    if (!sample.isEmpty())
        status = tr("%1 — sample symbol %2 (type another in SYMBOL)").arg(status, sample);
    data_status_->setText(status);
    data_status_->setToolTip(status);

    json_rows_ = rows;
    json_dirty_ = true;
    ++json_seq_; // a JSON document still being built is for the previous result
    if (!is_table_view_)
        fill_json_view();
}

void AkShareScreen::fill_json_view() {
    if (!json_dirty_)
        return;
    json_dirty_ = false;
    const quint64 seq = ++json_seq_;
    QString header;
    if (json_rows_.size() > kAkJsonRows) {
        header = tr("// The first %1 of %2 rows — the TABLE view shows them all.")
                     .arg(kAkJsonRows)
                     .arg(json_rows_.size()) +
                 QLatin1Char('\n');
    }
    // The indented text of up to 1,000 rows (~0.7 MB, ~21,000 lines for a wide table) and its text
    // document are built on a worker; this thread only attaches the finished document.
    asia_links::set_json_text(json_view_, tr("Preparing the JSON view…"));
    QPointer<AkShareScreen> self = this;
    (void)QtConcurrent::run(asia_links::json_view_pool(),
                            [rows = json_rows_, header, target = thread()]() {
                                return asia_links::build_json_document(rows, kAkJsonRows, header, target);
                            })
        .then(this, [self, seq](asia_links::JsonViewDocumentPtr built) {
            if (!self || seq != self->json_seq_ || !built || !built->doc)
                return; // a newer result (or the screen) replaced it: the holder deletes the document
            asia_links::install_json_document(self->json_view_, std::exchange(built->doc, nullptr));
        });
}

// ── Symbol linking ───────────────────────────────────────────────────────────

QString AkShareScreen::yf_symbol_at_row(int row) const {
    if (symbol_col_ < 0 || row < 0 || row >= model_->rowCount())
        return {};
    return asia_links::yf_symbol_for_code(model_->text(row, symbol_col_));
}

QString AkShareScreen::name_at_row(int row) const {
    if (name_col_ < 0 || row < 0 || row >= model_->rowCount())
        return {};
    return model_->text(row, name_col_).trimmed();
}

void AkShareScreen::publish_selection_to_group() {
    if (link_group_ == fincept::SymbolGroup::None || suppress_publish_)
        return;
    const QString yf = yf_symbol_at_row(data_table_->currentIndex().row());
    if (yf.isEmpty())
        return;
    fincept::SymbolContext::instance().set_group_symbol(
        link_group_, fincept::SymbolRef::equity(yf, asia_links::exchange_for_yf(yf)), this);
}

void AkShareScreen::on_group_symbol_changed(const fincept::SymbolRef& ref) {
    if (!ref.is_valid())
        return;
    // A-share / HK symbols become the SYMBOL parameter, so the next per-security query follows the
    // group (nothing is fetched here — a follower must stay cheap).
    const QString native = asia_links::native_code_for(ref.symbol);
    if (!native.isEmpty() && param_symbol_)
        param_symbol_->setText(native);

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

fincept::SymbolRef AkShareScreen::current_symbol() const {
    const QString yf = yf_symbol_at_row(data_table_ ? data_table_->currentIndex().row() : -1);
    if (yf.isEmpty())
        return {};
    return fincept::SymbolRef::equity(yf, asia_links::exchange_for_yf(yf));
}

void AkShareScreen::show_row_menu(const QPoint& pos) {
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

// ── IStatefulScreen ──────────────────────────────────────────────────────────

QVariantMap AkShareScreen::save_state() const {
    // Before the first show the remembered selection is still pending: keep it as it was restored.
    const QString source_id = (active_source_ >= 0 && active_source_ < sources_.size()) ? sources_[active_source_].id
                                                                                        : restored_source_id_;
    QVariantMap state{
        // By id, not list position: positions shift whenever a source is added or removed.
        {"source_id", source_id},
        {"endpoint", active_source_ >= 0 ? active_endpoint_ : pending_endpoint_},
        {"search", search_input_ ? search_input_->text() : QString()},
        {"table_view", is_table_view_},
    };
    if (param_symbol_)
        state["param_symbol"] = param_symbol_->text();
    if (param_start_)
        state["param_start"] = param_start_->text();
    if (param_end_)
        state["param_end"] = param_end_->text();
    if (param_period_)
        state["param_period"] = param_period_->currentData().toString();
    // The result itself is remembered by the result pane (with its "as of" time), not here.
    return state;
}

void AkShareScreen::restore_state(const QVariantMap& state) {
    // Only recorded here: the first show opens the source (nothing loads while the screen is hidden).
    QString src_id = state.value("source_id").toString();
    QString endpoint = state.value("endpoint").toString();
    if (src_id.isEmpty() && state.contains("source")) {
        // Legacy state: an index into the list as it was before "Global Economics" was removed.
        int src = state.value("source", -1).toInt();
        if (src == kAkShareRemovedSourceIndex) {
            src = -1; // the removed source has no replacement endpoint list to restore into
            endpoint.clear();
        } else if (src > kAkShareRemovedSourceIndex) {
            --src;
        }
        if (src >= 0 && src < sources_.size())
            src_id = sources_[src].id;
    }

    // Parameters first: they are the inputs of the run the source switch may start.
    if (param_symbol_ && state.contains("param_symbol"))
        param_symbol_->setText(state.value("param_symbol").toString());
    if (param_start_ && state.contains("param_start"))
        param_start_->setText(state.value("param_start").toString());
    if (param_end_ && state.contains("param_end"))
        param_end_->setText(state.value("param_end").toString());
    if (param_period_ && state.contains("param_period")) {
        const int idx = param_period_->findData(state.value("param_period").toString());
        if (idx >= 0)
            param_period_->setCurrentIndex(idx);
    }

    const bool table_view = state.value("table_view", true).toBool();
    if (table_view != is_table_view_)
        on_view_toggle();

    restored_source_id_ = src_id;
    pending_endpoint_ = endpoint;
    if (first_show_done_) {
        // Restored after the screen was already opened: switch now.
        for (int i = 0; i < sources_.size(); ++i) {
            if (sources_[i].id == src_id) {
                select_source(i, std::exchange(pending_endpoint_, QString()));
                break;
            }
        }
    }

    const QString search = state.value("search").toString();
    if (search_input_ && !search.isEmpty())
        search_input_->setText(search);
}

bool AkShareScreen::open_section(const QString& section, const QString& focus) {
    QString key = section.trimmed();
    int index = -1;
    if (!key.isEmpty()) {
        QString rest;
        if (command_rows::split_kind(key, "source", &rest))
            key = rest;
        for (int i = 0; i < sources_.size() && index < 0; ++i)
            if (sources_[i].id == key)
                index = i;
        for (int i = 0; i < sources_.size() && index < 0; ++i)
            if (sources_[i].id.compare(key, Qt::CaseInsensitive) == 0)
                index = i;
        if (index < 0)
            return false;
        if (first_show_done_) {
            on_source_clicked(index); // the source button's click path
        } else {
            // The first show (one event-loop turn after showEvent) opens this source instead of the
            // remembered one; a remembered endpoint belongs to the remembered source only.
            if (sources_[index].id != restored_source_id_)
                pending_endpoint_.clear();
            restored_source_id_ = sources_[index].id;
        }
    }
    const QString filter = focus.trimmed();
    if (!filter.isEmpty()) {
        // The endpoint filter, set after the first show / source switch (both clear it). Filters the
        // list only: nothing runs.
        QTimer::singleShot(0, this, [this, filter]() {
            if (search_input_)
                search_input_->setText(filter);
        });
    }
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

struct AksSourceRow {
    const char* id;
    const char* title;
    const char* keywords;
};

const AksSourceRow kAksSourceRows[] = {
    {"stocks_realtime", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China stock quotes"),
     "realtime, live quotes, spot, a-shares, a shares, china stocks, shanghai, shenzhen"},
    {"stocks_historical", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China stock price history"),
     "historical, history, ohlc, daily bars, kline, candles, a-shares"},
    {"stocks_financial", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China company financials"),
     "financial statements, balance sheet, income statement, cash flow, earnings, a-shares"},
    {"stocks_holders", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China shareholders"),
     "holders, shareholders, ownership, top holders, institutional holdings"},
    {"stocks_funds", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China fund flows"),
     "fund flows, money flow, capital flow, northbound, southbound, stock connect"},
    {"stocks_board", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China sector boards"),
     "boards, sectors, industries, concepts, themes"},
    {"stocks_margin", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China margin trading"),
     "margin, margin financing, short selling, securities lending"},
    {"stocks_hot", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China hot stocks"),
     "hot stocks, trending, popular, most active, stock news"},
    {"index", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China indices"),
     "index, indices, csi 300, shanghai composite, shenzhen component, hang seng"},
    {"futures", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China futures"),
     "futures, commodities, commodity futures, contracts"},
    {"bonds", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China bonds"),
     "bonds, yields, treasury, convertible bonds, fixed income"},
    {"currency", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Currencies"),
     "currency, forex, fx, exchange rates, cny, rmb, yuan"},
    {"energy", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Energy data"),
     "energy, oil, coal, natural gas, power, carbon"},
    {"crypto", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Crypto data"),
     "crypto, bitcoin, cryptocurrency, digital assets"},
    {"news", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China financial news"),
     "news, headlines, announcements, china news"},
    {"reits", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "REITs"),
     "reits, real estate investment trusts, property"},
    {"data", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China market data"),
     "market data, market overview, exchange statistics"},
    {"alternative", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Alternative data"),
     "alternative data, sentiment, search trends, other data"},
    {"analysis", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Market analysis"),
     "analysis, technical analysis, rankings, screens"},
    {"derivatives", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China options"),
     "derivatives, options, option chain, warrants"},
    {"economics_china", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China economy"),
     "china economics, macro, gdp, cpi, pmi, inflation, money supply"},
    {"funds_expanded", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China funds"),
     "funds, mutual funds, etf, fund nav, fund rankings"},
    {"company_info", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "China company profiles"),
     "company info, company profile, business, management"},
    {"macro", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Global macro data"),
     "global macro, global economics, world economy, central banks"},
    {"misc", QT_TRANSLATE_NOOP("fincept::screens::AkShareScreen", "Other AkShare data"),
     "miscellaneous, misc, other"},
};

QList<command::CommandItem> aks_command_rows() {
    namespace rows = command_rows;
    const QString where = AkShareScreen::tr("AkShare Data");
    QList<command::CommandItem> out;
    for (const AksSourceRow& r : kAksSourceRows) {
        QStringList keywords;
        for (const QString& w : QString::fromLatin1(r.keywords).split(QLatin1Char(','), Qt::SkipEmptyParts))
            keywords.append(w.trimmed());
        keywords << QStringLiteral("akshare") << QStringLiteral("china");
        out.append(rows::section(QStringLiteral("akshare"), QStringLiteral("source:") + QString::fromLatin1(r.id),
                                 QCoreApplication::translate("fincept::screens::AkShareScreen", r.title), keywords,
                                 where));
    }
    return out;
}

void aks_install_commands() {
    command_rows::install(QStringLiteral("akshare"), &aks_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(aks_install_commands)

} // namespace fincept::screens
