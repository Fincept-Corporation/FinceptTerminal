#include "screens/screener/ScreenerScreen.h"

#include "core/events/EventBus.h"
#include "core/session/ScreenStateManager.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/screener/ScreenerCriteriaBuilder.h"
#include "screens/screener/ScreenerResultsModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTableView>
#include <QTextStream>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

// The screen's original view: a fixed large-cap basket streamed from `market:quote:<sym>`.
const QStringList kScreenerBasket = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "NFLX", "AMD",   "INTC", "AVGO", "ORCL", "CRM",
    "ADBE", "CSCO", "QCOM",  "TXN",  "JPM",  "GS",   "BAC",  "WFC",  "MS",    "C",    "BRK-B", "V",    "MA",
    "AXP",  "PYPL", "WMT",   "COST", "TGT",  "HD",   "LOW",  "NKE",  "MCD",   "SBUX", "AMGN", "PFE",  "JNJ",
    "MRK",  "ABBV", "LLY",   "UNH",  "XOM",  "CVX",  "SLB",  "COP",  "NEE",   "DUK",  "SO",   "CAT",  "GE",
    "HON",  "RTX",  "BA",    "DE",   "PLTR", "COIN", "SOFI", "SNAP", "UBER",  "ABNB", "SHOP", "SQ"};

const QString kBasketView = QStringLiteral("basket");
const QString kCustomView = QStringLiteral("custom");
const QString kPredefinedPrefix = QStringLiteral("pre:");
const QString kSavedPrefix = QStringLiteral("saved:");
const QString kDefaultPredefined = QStringLiteral("most_actives");
constexpr int kPageSizes[] = {25, 50, 100, 250};
constexpr int kDefaultPageSize = 100;
constexpr int kNameColumnMaxWidth = 260;

/// Identity of a screen regardless of paging / page size: a refresh or page turn of the same
/// screen keeps the user's column sort; a different screen resets to Yahoo's ranking.
QString screen_identity(const services::ScreenerQuery& q) {
    if (q.is_predefined())
        return kPredefinedPrefix + q.predefined_id;
    return kCustomView + QLatin1Char(':') + QString::fromUtf8(QJsonDocument(q.criteria).toJson(QJsonDocument::Compact)) +
           QLatin1Char('|') + q.sort_field + (q.sort_asc ? QStringLiteral("|asc") : QStringLiteral("|desc"));
}

QString thousands(qint64 v) {
    static const QLocale loc(QLocale::English, QLocale::UnitedStates);
    return loc.toString(v);
}

/// CSV cell for text. Cells starting with = + - @ are executed as formulas by spreadsheet apps,
/// so they are neutralised with a leading apostrophe.
QString csv_text(QString v) {
    if (!v.isEmpty() && QStringLiteral("=+-@\t\r").contains(v.front()))
        v.prepend(QLatin1Char('\''));
    if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n')) ||
        v.contains(QLatin1Char('\r')))
        v = QLatin1Char('"') + v.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
    return v;
}

/// CSV cell for a number: full precision, empty when missing.
QString csv_number(double v) {
    return std::isnan(v) ? QString() : QString::number(v, 'g', 15);
}

void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

ScreenerScreen::ScreenerScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("screenerScreen");
    build_ui();
    apply_styles();
    retranslate();

    auto& svc = services::ScreenerService::instance();
    connect(&svc, &services::ScreenerService::results_ready, this, &ScreenerScreen::on_results_ready);
    connect(&svc, &services::ScreenerService::run_failed, this, &ScreenerScreen::on_run_failed);
    connect(&svc, &services::ScreenerService::catalog_ready, this, &ScreenerScreen::on_catalog_ready);
    connect(&svc, &services::ScreenerService::catalog_failed, this, &ScreenerScreen::on_catalog_failed);
    connect(&svc, &services::ScreenerService::saved_screens_changed, this, &ScreenerScreen::rebuild_tree);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_styles(); });

    update_actions(); // retranslate() already listed the live basket, usable before the catalogue arrives
}

// ── Construction ──────────────────────────────────────────────────────────────

void ScreenerScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    header_bar_ = new QWidget(this);
    header_bar_->setObjectName("screenerHeader");
    auto* hb = new QHBoxLayout(header_bar_);
    hb->setContentsMargins(14, 8, 14, 8);
    hb->setSpacing(10);
    auto* title_box = new QVBoxLayout;
    title_box->setSpacing(1);
    title_lbl_ = new QLabel(header_bar_);
    title_lbl_->setObjectName("screenerTitle");
    subtitle_lbl_ = new QLabel(header_bar_);
    subtitle_lbl_->setObjectName("screenerSubtitle");
    subtitle_lbl_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // long descriptions elide
    title_box->addWidget(title_lbl_);
    title_box->addWidget(subtitle_lbl_);
    hb->addLayout(title_box, 1);
    count_lbl_ = new QLabel(header_bar_);
    count_lbl_->setObjectName("screenerCount");
    hb->addWidget(count_lbl_);
    search_ = new QLineEdit(header_bar_);
    search_->setClearButtonEnabled(true);
    search_->setFixedWidth(230);
    hb->addWidget(search_);
    root->addWidget(header_bar_);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("screenerSplitter");
    splitter->setHandleWidth(1);
    splitter->addWidget(build_sidebar());
    splitter->addWidget(build_results_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({310, 900});
    root->addWidget(splitter, 1);

    // ── Wiring ──────────────────────────────────────────────────────────────
    connect(search_, &QLineEdit::textChanged, this, [this](const QString& text) {
        proxy_->setFilterFixedString(text.trimmed());
        update_count_label();
        notify_state_changed();
    });
    auto* clear_search = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
    clear_search->setContext(Qt::WidgetShortcut);
    connect(clear_search, &QShortcut::activated, search_, &QLineEdit::clear);

    for (const QKeySequence& seq :
         {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { run_current(false); });
    }

    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        const QString id = item ? item->data(0, Qt::UserRole).toString() : QString();
        if (!id.isEmpty())
            activate_view(id);
    });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) {
        const QString id = item ? item->data(0, Qt::UserRole).toString() : QString();
        if (!id.isEmpty())
            activate_view(id);
    });
    connect(tree_, &QWidget::customContextMenuRequested, this, &ScreenerScreen::show_tree_menu);

    connect(new_btn_, &QPushButton::clicked, this, &ScreenerScreen::new_screen);
    connect(catalog_retry_btn_, &QPushButton::clicked, this, [this]() {
        catalog_retry_btn_->hide();
        catalog_lbl_->setText(tr("Loading screener fields..."));
        services::ScreenerService::instance().load_catalog();
    });
    connect(builder_, &ScreenerCriteriaBuilder::edited, this, &ScreenerScreen::on_builder_edited);
    connect(builder_, &ScreenerCriteriaBuilder::run_requested, this, [this]() { run_current(false); });

    connect(run_btn_, &QPushButton::clicked, this, [this]() { run_current(false); });
    connect(save_btn_, &QPushButton::clicked, this, &ScreenerScreen::save_screen);
    connect(export_btn_, &QPushButton::clicked, this, &ScreenerScreen::export_csv);
    connect(watch_menu_, &QMenu::aboutToShow, this, [this]() {
        const auto items = selected_symbols();
        if (items.isEmpty()) {
            watch_menu_->clear();
            watch_menu_->addAction(tr("Select rows first (Ctrl / Shift-click for several)"))->setEnabled(false);
            return;
        }
        populate_watchlist_menu(watch_menu_, items);
    });
    connect(page_size_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        offset_ = 0;
        if (source_ == Source::Predefined || source_ == Source::Custom)
            run_current(false);
        notify_state_changed();
    });
    connect(prev_btn_, &QToolButton::clicked, this, [this]() { change_page(-1); });
    connect(next_btn_, &QToolButton::clicked, this, [this]() { change_page(+1); });
    connect(retry_btn_, &QPushButton::clicked, this, [this]() {
        auto& svc = services::ScreenerService::instance();
        if (!svc.has_catalog()) {
            catalog_retry_btn_->hide();
            catalog_lbl_->setText(tr("Loading screener fields..."));
            show_message(tr("Loading the screener..."), false);
            svc.load_catalog();
            return;
        }
        run_current(true);
    });

    connect(table_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        open_symbol_in(QStringLiteral("equity_research"), symbol_at(idx));
    });
    connect(table_, &QWidget::customContextMenuRequested, this, &ScreenerScreen::show_row_menu);
    // A user pick becomes the linked group's symbol. Programmatic selection (restoring the row
    // after a refresh, following the group) is fenced by selecting_ so it never re-publishes.
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (selecting_ || link_group_ == SymbolGroup::None)
                    return;
                const QString sym = symbol_at(current);
                if (!sym.isEmpty())
                    SymbolContext::instance().set_group_symbol(link_group_, SymbolRef::equity(sym), this);
            });
}

QWidget* ScreenerScreen::build_sidebar() {
    auto* side = new QWidget(this);
    side->setObjectName("screenerSidebar");
    side->setMinimumWidth(250);
    auto* v = new QVBoxLayout(side);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    screens_lbl_ = new QLabel(side);
    screens_lbl_->setObjectName("screenerSectionLabel");
    screens_lbl_->setContentsMargins(12, 8, 12, 4);
    v->addWidget(screens_lbl_);

    tree_ = new QTreeWidget(side);
    tree_->setObjectName("screenerTree");
    tree_->setHeaderHidden(true);
    tree_->setRootIsDecorated(false);
    tree_->setIndentation(12);
    tree_->setUniformRowHeights(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(tree_, 2);

    auto* builder_head = new QWidget(side);
    auto* bh = new QHBoxLayout(builder_head);
    bh->setContentsMargins(12, 8, 12, 2);
    bh->setSpacing(6);
    builder_lbl_ = new QLabel(builder_head);
    builder_lbl_->setObjectName("screenerSectionLabel");
    new_btn_ = new QPushButton(builder_head);
    new_btn_->setObjectName("screenerGhostBtn");
    new_btn_->setCursor(Qt::PointingHandCursor);
    new_btn_->setEnabled(false);
    bh->addWidget(builder_lbl_);
    bh->addStretch(1);
    bh->addWidget(new_btn_);
    v->addWidget(builder_head);

    catalog_lbl_ = new QLabel(side);
    catalog_lbl_->setObjectName("screenerCatalogStatus");
    catalog_lbl_->setWordWrap(true);
    catalog_lbl_->setContentsMargins(12, 4, 12, 4);
    v->addWidget(catalog_lbl_);
    catalog_retry_btn_ = new QPushButton(side);
    catalog_retry_btn_->setCursor(Qt::PointingHandCursor);
    catalog_retry_btn_->hide();
    auto* retry_row = new QHBoxLayout;
    retry_row->setContentsMargins(12, 0, 12, 6);
    retry_row->addWidget(catalog_retry_btn_);
    retry_row->addStretch(1);
    v->addLayout(retry_row);

    builder_scroll_ = new QScrollArea(side);
    builder_scroll_->setObjectName("screenerBuilderScroll");
    builder_scroll_->setWidgetResizable(true);
    builder_scroll_->setFrameShape(QFrame::NoFrame);
    builder_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    builder_ = new ScreenerCriteriaBuilder(builder_scroll_);
    builder_scroll_->setWidget(builder_);
    builder_scroll_->hide(); // until the catalogue arrives
    v->addWidget(builder_scroll_, 3);
    return side;
}

QWidget* ScreenerScreen::build_results_panel() {
    auto* panel = new QWidget(this);
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* bar = new QWidget(panel);
    bar->setObjectName("screenerToolbar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(6);
    run_btn_ = new QPushButton(bar);
    run_btn_->setObjectName("screenerPrimaryBtn");
    save_btn_ = new QPushButton(bar);
    export_btn_ = new QPushButton(bar);
    watch_btn_ = new QToolButton(bar);
    watch_btn_->setPopupMode(QToolButton::InstantPopup);
    watch_menu_ = new QMenu(watch_btn_);
    watch_btn_->setMenu(watch_menu_);
    for (QWidget* w : {static_cast<QWidget*>(run_btn_), static_cast<QWidget*>(save_btn_),
                       static_cast<QWidget*>(export_btn_), static_cast<QWidget*>(watch_btn_)}) {
        w->setCursor(Qt::PointingHandCursor);
        h->addWidget(w);
    }
    h->addStretch(1);
    page_size_lbl_ = new QLabel(bar);
    page_size_lbl_->setObjectName("screenerPageLabel");
    page_size_ = new QComboBox(bar);
    for (int n : kPageSizes)
        page_size_->addItem(QString::number(n), n);
    page_size_->setCurrentIndex(page_size_->findData(kDefaultPageSize));
    prev_btn_ = new QToolButton(bar);
    prev_btn_->setText(QString(QChar(0x25C0))); // ◀
    next_btn_ = new QToolButton(bar);
    next_btn_->setText(QString(QChar(0x25B6))); // ▶
    page_lbl_ = new QLabel(bar);
    page_lbl_->setObjectName("screenerPageLabel");
    page_lbl_->setMinimumWidth(110);
    page_lbl_->setAlignment(Qt::AlignCenter);
    h->addWidget(page_size_lbl_);
    h->addWidget(page_size_);
    h->addSpacing(8);
    h->addWidget(prev_btn_);
    h->addWidget(page_lbl_);
    h->addWidget(next_btn_);
    v->addWidget(bar);

    stack_ = new QStackedWidget(panel);
    model_ = new ScreenerResultsModel(this);
    proxy_ = new ScreenerSortProxy(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ScreenerResultsModel::SortRole);
    proxy_->setFilterRole(ScreenerResultsModel::SearchRole);
    proxy_->setFilterKeyColumn(ScreenerResultsModel::ColSymbol);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    table_ = new QTableView(stack_);
    table_->setObjectName("screenerTable");
    table_->setModel(proxy_);
    table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder); // Yahoo's ranking until a header click
    proxy_->sort(-1, Qt::AscendingOrder);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideRight);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(24);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionsMovable(true);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setMinimumSectionSize(48);

    message_page_ = new QWidget(stack_);
    auto* mv = new QVBoxLayout(message_page_);
    mv->setContentsMargins(24, 24, 24, 24);
    mv->addStretch(1);
    message_lbl_ = new QLabel(message_page_);
    message_lbl_->setObjectName("screenerMessage");
    message_lbl_->setAlignment(Qt::AlignCenter);
    message_lbl_->setWordWrap(true);
    mv->addWidget(message_lbl_);
    retry_btn_ = new QPushButton(message_page_);
    retry_btn_->setCursor(Qt::PointingHandCursor);
    retry_btn_->hide();
    auto* retry_row = new QHBoxLayout;
    retry_row->addStretch(1);
    retry_row->addWidget(retry_btn_);
    retry_row->addStretch(1);
    mv->addLayout(retry_row);
    mv->addStretch(1);

    stack_->addWidget(table_);
    stack_->addWidget(message_page_);
    stack_->setCurrentWidget(message_page_);
    v->addWidget(stack_, 1);

    status_lbl_ = new QLabel(panel);
    status_lbl_->setObjectName("screenerStatus");
    status_lbl_->setTextFormat(Qt::PlainText);
    status_lbl_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    v->addWidget(status_lbl_);
    return panel;
}

void ScreenerScreen::apply_styles() {
    // ONE stylesheet for the whole screen (children, the builder and criteria rows included):
    // object-name selectors instead of per-widget setStyleSheet calls (P7).
    QString css = QStringLiteral(
        "#screenerScreen { background: {bg_base}; }"
        "#screenerHeader { background: {bg_surface}; border-bottom: 1px solid {border_dim}; }"
        "#screenerTitle { color: {amber}; font-size: {fs_title}px; font-weight: 700; letter-spacing: 1px; }"
        "#screenerSubtitle, #screenerCount, #screenerPageLabel { color: {text3}; }"
        "#screenerSidebar { background: {bg_surface}; }"
        "QSplitter#screenerSplitter::handle { background: {border_dim}; }"
        "#screenerSectionLabel { color: {amber}; font-weight: 700; letter-spacing: 1px; font-size: {fs_tiny}px; }"
        "#screenerHint, #screenerUnit, #screenerCatalogStatus { color: {text3}; font-size: {fs_tiny}px; }"
        "QTreeWidget#screenerTree { background: {bg_surface}; color: {text}; border: none; outline: 0; }"
        "QTreeWidget#screenerTree::item { padding: 3px 4px; }"
        "QTreeWidget#screenerTree::item:hover { background: {bg_hover}; }"
        "QTreeWidget#screenerTree::item:selected { background: {accent_bg}; color: {amber}; }"
        "QScrollArea#screenerBuilderScroll, #screenerBuilder { background: {bg_surface}; border: none; }"
        "#screenerCriterionRow { background: {bg_raised}; border: 1px solid {border_dim}; }"
        "#screenerToolbar { background: {bg_surface}; border-bottom: 1px solid {border_dim}; }"
        "#screenerScreen QLineEdit { background: {bg_raised}; color: {text}; border: 1px solid {border_med};"
        " padding: 3px 6px; }"
        "#screenerScreen QLineEdit:focus { border-color: {amber}; }"
        "#screenerScreen QLineEdit[invalid=\"true\"] { border-color: {neg}; }"
        "#screenerScreen QComboBox { background: {bg_raised}; color: {text}; border: 1px solid {border_med};"
        " padding: 3px 6px; }"
        "#screenerScreen QComboBox:disabled { color: {text_dim}; }"
        "#screenerScreen QComboBox QAbstractItemView { background: {bg_raised}; color: {text};"
        " border: 1px solid {border_med}; selection-background-color: {accent_bg}; selection-color: {amber}; }"
        "#screenerScreen QPushButton { background: {bg_raised}; color: {text}; border: 1px solid {border_med};"
        " padding: 4px 12px; font-weight: 700; letter-spacing: 1px; }"
        "#screenerScreen QPushButton:hover { border-color: {amber}; }"
        "#screenerScreen QPushButton:disabled { color: {text_dim}; border-color: {border_dim}; }"
        "#screenerScreen QPushButton#screenerPrimaryBtn { background: {amber}; color: {on_amber};"
        " border: 1px solid {amber}; }"
        "#screenerScreen QPushButton#screenerPrimaryBtn:disabled { background: {bg_raised}; color: {text_dim};"
        " border-color: {border_dim}; }"
        "#screenerScreen QPushButton#screenerGhostBtn { background: transparent; color: {amber};"
        " border: 1px dashed {border_med}; padding: 3px 10px; }"
        "#screenerScreen QToolButton { background: {bg_raised}; color: {text}; border: 1px solid {border_med};"
        " padding: 3px 8px; font-weight: 700; }"
        "#screenerScreen QToolButton:hover { border-color: {amber}; }"
        "#screenerScreen QToolButton:disabled { color: {text_dim}; border-color: {border_dim}; }"
        "#screenerScreen QToolButton#screenerRemoveBtn { background: transparent; border: none; color: {text3}; }"
        "#screenerScreen QToolButton#screenerRemoveBtn:hover { color: {neg}; }"
        "QTableView#screenerTable { background: {bg_base}; alternate-background-color: {row_alt}; color: {text};"
        " border: none; selection-background-color: {accent_bg}; selection-color: {text}; }"
        "QTableView#screenerTable QHeaderView::section { background: {bg_surface}; color: {text3}; border: none;"
        " border-bottom: 1px solid {border_med}; border-right: 1px solid {border_dim}; padding: 4px 6px;"
        " font-weight: 700; }"
        "#screenerMessage { color: {text3}; font-size: {fs_body}px; }"
        "#screenerStatus { background: {bg_surface}; color: {text3}; border-top: 1px solid {border_dim};"
        " padding: 3px 10px; }"
        "#screenerStatus[warn=\"true\"] { color: {warn}; }");
    const std::pair<const char*, QString> tokens[] = {
        {"{bg_base}", ui::colors::BG_BASE()},
        {"{bg_surface}", ui::colors::BG_SURFACE()},
        {"{bg_raised}", ui::colors::BG_RAISED()},
        {"{bg_hover}", ui::colors::BG_HOVER()},
        {"{border_dim}", ui::colors::BORDER_DIM()},
        {"{border_med}", ui::colors::BORDER_MED()},
        {"{text}", ui::colors::TEXT_PRIMARY()},
        {"{text3}", ui::colors::TEXT_TERTIARY()},
        {"{text_dim}", ui::colors::TEXT_DIM()},
        {"{amber}", ui::colors::AMBER()},
        {"{on_amber}", ui::colors::TEXT_ON_ACCENT()},
        {"{accent_bg}", ui::colors::ACCENT_BG()},
        {"{row_alt}", ui::colors::ROW_ALT()},
        {"{neg}", ui::colors::NEGATIVE()},
        {"{warn}", ui::colors::WARNING()},
        {"{fs_title}", QString::number(ui::fonts::font_px(2))},
        {"{fs_body}", QString::number(ui::fonts::font_px(0))},
        {"{fs_tiny}", QString::number(ui::fonts::font_px(-2))},
    };
    for (const auto& [token, value] : tokens)
        css.replace(QLatin1String(token), value);
    setStyleSheet(css);
}

void ScreenerScreen::retranslate() {
    title_lbl_->setText(tr("EQUITY SCREENER"));
    search_->setPlaceholderText(tr("Filter results: symbol, name, sector..."));
    search_->setToolTip(tr("Filters the rows on screen (Esc clears). RUN asks Yahoo again."));
    screens_lbl_->setText(tr("SCREENS"));
    builder_lbl_->setText(tr("SCREEN BUILDER"));
    new_btn_->setText(tr("NEW"));
    new_btn_->setToolTip(tr("Start a new screen from the defaults"));
    catalog_retry_btn_->setText(tr("RETRY"));
    if (!builder_->has_catalog() && !catalog_retry_btn_->isVisible())
        catalog_lbl_->setText(tr("Loading screener fields..."));

    run_btn_->setText(pending_key_.isEmpty() ? tr("RUN") : tr("RUNNING..."));
    run_btn_->setToolTip(tr("Run the screen (Enter in a value field, Ctrl+Enter anywhere). F5 re-fetches "
                            "even when a result less than a minute old is cached."));
    save_btn_->setText(tr("SAVE SCREEN"));
    save_btn_->setToolTip(tr("Save the builder's criteria under a name"));
    export_btn_->setText(tr("EXPORT CSV"));
    export_btn_->setToolTip(tr("Export the rows on screen (filtered, in display order) with raw values"));
    watch_btn_->setText(tr("ADD TO WATCHLIST"));
    watch_btn_->setToolTip(tr("Add the selected rows to a watchlist"));
    page_size_lbl_->setText(tr("Rows"));
    page_size_->setToolTip(tr("Rows per page (Yahoo returns at most 250 per request)"));
    prev_btn_->setToolTip(tr("Previous page"));
    next_btn_->setToolTip(tr("Next page"));
    retry_btn_->setText(tr("RETRY"));

    model_->set_base_headers(
        {tr("SYMBOL"), tr("NAME"), tr("PRICE"), tr("CHG %"), tr("VOLUME"), tr("MKT CAP"), tr("P/E"), tr("SECTOR"),
         tr("EXCHANGE"), tr("CCY")},
        {tr("Yahoo Finance symbol"), tr("Company name (hover for the industry)"),
         tr("Last price in the listing's quote currency (CCY), exchange-delayed"),
         tr("Change vs. previous close, from the current quote"),
         tr("Shares traded this session (hover for the 3-month average)"),
         tr("Market cap in the listing's currency, not converted (hover for the exact figure)"),
         tr("Price / trailing 12-month EPS (hover for forward P/E)"), tr("Sector (hover for the industry)"),
         tr("Listing exchange (hover for Yahoo's code)"),
         tr("Quote currency. GBp, ZAc, ILA and KWF are minor units (pence, cents, agorot, fils).")});

    if (status_lbl_->text().isEmpty())
        set_status(tr("Pick a screen on the left or build one, then press RUN."));
    if (stack_->currentWidget() == message_page_ && message_lbl_->text().isEmpty())
        message_lbl_->setText(tr("Loading the screener..."));
    rebuild_tree();
    subtitle_lbl_->setText(active_title());
}

void ScreenerScreen::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e && e->type() == QEvent::LanguageChange)
        retranslate();
}

void ScreenerScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!catalog_requested_) {
        catalog_requested_ = true;
        auto& svc = services::ScreenerService::instance();
        if (svc.has_catalog())
            on_catalog_ready(); // another screener panel loaded it already
        else
            svc.load_catalog();
    }
    if (source_ == Source::Basket && !hub_active_)
        hub_subscribe_all();
}

void ScreenerScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void ScreenerScreen::refresh() {
    run_current(true);
}

// ── Catalogue + screens list ──────────────────────────────────────────────────

void ScreenerScreen::on_catalog_ready() {
    // Ignore a catalogue another panel requested before this one was ever shown (P2: no work for
    // a screen the user has not opened); showEvent picks it up.
    if (!catalog_requested_)
        return;
    if (builder_->has_catalog()) {
        rebuild_tree();
        return;
    }
    builder_->set_catalog(services::ScreenerService::instance().catalog());
    catalog_lbl_->hide();
    catalog_retry_btn_->hide();
    builder_scroll_->show();
    new_btn_->setEnabled(true);
    rebuild_tree();
    const QVariantMap state = pending_state_;
    pending_state_.clear();
    apply_state(state);
}

void ScreenerScreen::on_catalog_failed(const QString& error) {
    if (!catalog_requested_ || builder_->has_catalog())
        return;
    catalog_lbl_->setText(tr("Screener fields unavailable: %1").arg(error));
    catalog_lbl_->show();
    catalog_retry_btn_->show();
    if (source_ == Source::None) {
        show_message(tr("The screener could not start.\n%1").arg(error), true);
        set_status(tr("Screener unavailable: %1").arg(error), {}, true);
    }
}

void ScreenerScreen::rebuild_tree() {
    const QString keep = view_id();
    const QSignalBlocker block(tree_);
    tree_->clear();

    const auto add_group = [this](const QString& title) {
        auto* g = new QTreeWidgetItem(tree_);
        g->setText(0, title);
        g->setFlags(Qt::ItemIsEnabled); // a header: not selectable, never runs anything
        QFont f = g->font(0);
        f.setBold(true);
        g->setFont(0, f);
        g->setForeground(0, QColor(ui::colors::TEXT_TERTIARY()));
        return g;
    };
    const auto add_item = [](QTreeWidgetItem* parent, const QString& text, const QString& id, const QString& tip) {
        auto* it = new QTreeWidgetItem(parent);
        it->setText(0, text);
        it->setData(0, Qt::UserRole, id);
        it->setToolTip(0, tip);
        if (id.isEmpty())
            it->setFlags(Qt::NoItemFlags);
        return it;
    };

    QTreeWidgetItem* live = add_group(tr("LIVE"));
    add_item(live, tr("US Large-Cap Basket"), kBasketView,
             tr("%1 US large caps with streaming quotes (the screener's original view)").arg(kScreenerBasket.size()));

    const auto& svc = services::ScreenerService::instance();
    if (svc.has_catalog()) {
        QHash<QString, QTreeWidgetItem*> groups;
        for (const auto& v : svc.catalog().value("predefined").toArray()) {
            const QJsonObject p = v.toObject();
            const QString group = p.value("group").toString();
            if (!groups.contains(group))
                groups.insert(group, add_group(group.toUpper()));
            add_item(groups.value(group), p.value("title").toString(), kPredefinedPrefix + p.value("id").toString(),
                     p.value("description").toString());
        }
        QTreeWidgetItem* saved = add_group(tr("SAVED SCREENS"));
        const auto list = svc.saved_screens();
        if (list.is_err()) {
            add_item(saved, tr("Saved screens unavailable"), QString(), QString::fromStdString(list.error()));
        } else if (list.value().isEmpty()) {
            add_item(saved, tr("None yet: build a screen, then SAVE"), QString(), QString());
        } else {
            for (const auto& s : list.value())
                add_item(saved, s.name, kSavedPrefix + s.name, tr("Saved %1 UTC").arg(s.saved_at));
        }
    }
    tree_->expandAll();
    select_tree_view(keep);
}

void ScreenerScreen::select_tree_view(const QString& id) {
    const QSignalBlocker block(tree_);
    tree_->clearSelection();
    tree_->setCurrentItem(nullptr);
    if (id.isEmpty())
        return;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id) {
            tree_->setCurrentItem(*it);
            return;
        }
    }
}

void ScreenerScreen::show_tree_menu(const QPoint& pos) {
    QTreeWidgetItem* item = tree_->itemAt(pos);
    const QString id = item ? item->data(0, Qt::UserRole).toString() : QString();
    if (!id.startsWith(kSavedPrefix))
        return;
    const QString name = id.mid(kSavedPrefix.size());
    QMenu menu(this);
    connect(menu.addAction(tr("Run")), &QAction::triggered, this, [this, id]() { activate_view(id); });
    connect(menu.addAction(tr("Delete Saved Screen")), &QAction::triggered, this, [this, name]() {
        if (QMessageBox::question(this, tr("Delete Saved Screen"), tr("Delete the saved screen \"%1\"?").arg(name)) !=
            QMessageBox::Yes)
            return;
        const auto r = services::ScreenerService::instance().delete_screen(name);
        if (r.is_err()) {
            set_status(tr("Could not delete \"%1\": %2").arg(name, QString::fromStdString(r.error())), {}, true);
            return;
        }
        if (saved_name_.compare(name, Qt::CaseInsensitive) == 0) {
            saved_name_.clear();
            custom_modified_ = true;
        }
        set_status(tr("Deleted saved screen \"%1\".").arg(name));
        subtitle_lbl_->setText(active_title());
    });
    menu.exec(tree_->viewport()->mapToGlobal(pos));
}

// ── Running screens ───────────────────────────────────────────────────────────

QString ScreenerScreen::view_id() const {
    switch (source_) {
        case Source::Basket:
            return kBasketView;
        case Source::Predefined:
            return kPredefinedPrefix + predefined_id_;
        case Source::Custom:
            return !saved_name_.isEmpty() && !custom_modified_ ? kSavedPrefix + saved_name_ : kCustomView;
        case Source::None:
            break;
    }
    return {};
}

QString ScreenerScreen::active_title() const {
    switch (source_) {
        case Source::Basket:
            return tr("US Large-Cap Basket (live quotes)");
        case Source::Predefined: {
            for (const auto& v : services::ScreenerService::instance().catalog().value("predefined").toArray()) {
                const QJsonObject p = v.toObject();
                if (p.value("id").toString() == predefined_id_)
                    return p.value("title").toString() + QStringLiteral("  ·  ") +
                           p.value("description").toString();
            }
            return predefined_id_;
        }
        case Source::Custom:
            if (saved_name_.isEmpty())
                return tr("Custom screen");
            return custom_modified_ ? tr("%1 (modified)").arg(saved_name_) : saved_name_;
        case Source::None:
            break;
    }
    return tr("Pick a screen");
}

void ScreenerScreen::activate_view(const QString& id) {
    if (id == kBasketView) {
        source_ = Source::Basket;
        pending_key_.clear();
        set_running(false);
        showing_result_ = false;
        shown_screen_ = kBasketView;
        shown_ = services::ScreenerResult{};
        offset_ = 0;
        table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        proxy_->sort(-1, Qt::AscendingOrder);
        if (isVisible() && !hub_active_)
            hub_subscribe_all();
        rebuild_basket();
    } else {
        if (hub_active_)
            hub_unsubscribe_all();
        if (id.startsWith(kPredefinedPrefix)) {
            source_ = Source::Predefined;
            predefined_id_ = id.mid(kPredefinedPrefix.size());
        } else if (id.startsWith(kSavedPrefix)) {
            const QString name = id.mid(kSavedPrefix.size());
            const auto list = services::ScreenerService::instance().saved_screens();
            const services::SavedScreen* found = nullptr;
            if (list.is_ok()) {
                for (const auto& s : list.value()) {
                    if (s.name.compare(name, Qt::CaseInsensitive) == 0) {
                        found = &s;
                        break;
                    }
                }
            }
            if (!found) {
                set_status(tr("Saved screen \"%1\" was not found.").arg(name), {}, true);
                return;
            }
            builder_->load_query(found->query);
            builder_json_ = found->query.to_json();
            {
                const QSignalBlocker block(page_size_);
                const int i = page_size_->findData(found->query.count);
                if (i >= 0)
                    page_size_->setCurrentIndex(i);
            }
            source_ = Source::Custom;
            saved_name_ = found->name;
            custom_modified_ = false;
        } else if (id == kCustomView) {
            source_ = Source::Custom;
        } else {
            return;
        }
        offset_ = 0;
        run_current(false);
    }
    select_tree_view(view_id());
    subtitle_lbl_->setText(active_title());
    update_actions();
    notify_state_changed();
}

void ScreenerScreen::run_current(bool force) {
    if (source_ == Source::Basket) {
        refresh_basket();
        return;
    }
    services::ScreenerQuery q;
    if (source_ == Source::Predefined) {
        q.predefined_id = predefined_id_;
    } else if (source_ == Source::Custom) {
        if (!builder_->has_catalog()) {
            set_status(tr("The screener fields are still loading."), {}, true);
            return;
        }
        QString error;
        if (!builder_->build_query(&q, &error)) {
            set_status(error, {}, true);
            return;
        }
        builder_json_ = q.to_json();
    } else {
        return;
    }
    q.count = page_size_->currentData().toInt();
    q.offset = offset_;

    // A different screen clears the table instead of leaving the previous screen's rows under
    // the new title; a refresh or page turn of the same screen keeps them until the answer lands.
    if (screen_identity(q) != shown_screen_ || !showing_result_) {
        model_->set_result({}, {});
        showing_result_ = false;
        shown_screen_.clear();
        show_message(tr("Running %1...").arg(active_title()), false);
    }
    pending_key_ = services::ScreenerService::instance().run(q, force);
    set_running(true);
    set_status(tr("Running %1...").arg(active_title()));
    subtitle_lbl_->setText(active_title());
    notify_state_changed();
}

void ScreenerScreen::change_page(int direction) {
    if (!showing_result_ || !pending_key_.isEmpty())
        return;
    const int page = page_size_->currentData().toInt();
    const int next = offset_ + direction * page;
    if (next < 0 || (direction > 0 && next >= shown_.total))
        return;
    offset_ = next;
    run_current(false);
}

void ScreenerScreen::on_results_ready(const services::ScreenerResult& r) {
    if (r.key != pending_key_)
        return; // a run this panel no longer waits for (superseded, or another panel's)
    pending_key_.clear();
    set_running(false);
    show_result(r, QString());
}

void ScreenerScreen::on_run_failed(const QString& key, const QString& error) {
    if (key != pending_key_)
        return;
    pending_key_.clear();
    set_running(false);
    if (const auto* last = services::ScreenerService::instance().last_result(key)) {
        show_result(*last, error); // last-known-good, clearly labelled as such
        return;
    }
    model_->set_result({}, {});
    showing_result_ = false;
    shown_screen_.clear();
    shown_ = services::ScreenerResult{};
    show_message(tr("The screen could not be run.\n%1").arg(error), true);
    set_status(tr("Screen failed: %1").arg(error), {}, true);
    update_actions();
}

void ScreenerScreen::show_result(const services::ScreenerResult& r, const QString& failure) {
    const QString identity = screen_identity(r.query);
    const bool same_screen = showing_result_ && identity == shown_screen_;
    const QString keep_symbol = current_symbol().symbol;

    model_->set_result(r.rows, r.columns);
    if (!same_screen) {
        table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        proxy_->sort(-1, Qt::AscendingOrder); // Yahoo's ranking first; header clicks re-sort locally
        table_->resizeColumnsToContents();
        if (table_->columnWidth(ScreenerResultsModel::ColName) > kNameColumnMaxWidth)
            table_->setColumnWidth(ScreenerResultsModel::ColName, kNameColumnMaxWidth);
        table_->scrollToTop();
    }
    shown_screen_ = identity;
    shown_ = r;
    showing_result_ = true;
    offset_ = r.offset;
    if (!keep_symbol.isEmpty())
        select_symbol(keep_symbol);

    if (r.rows.isEmpty()) {
        show_message(r.total > 0 ? tr("No rows on this page (the screen has %1 matches).").arg(thousands(r.total))
                                 : tr("No stocks match these criteria. Loosen a filter and press RUN."),
                     false);
    } else {
        stack_->setCurrentWidget(table_);
    }

    QStringList parts;
    if (!r.rows.isEmpty())
        parts << tr("%1-%2 of %3 matches")
                     .arg(thousands(r.offset + 1), thousands(r.offset + r.rows.size()), thousands(r.total));
    else
        parts << tr("0 of %1 matches").arg(thousands(r.total));
    if (r.as_of > 0) {
        const QDateTime t = QDateTime::fromSecsSinceEpoch(r.as_of).toLocalTime();
        QString quotes = tr("quotes as of %1 %2").arg(t.toString(QStringLiteral("HH:mm:ss")), t.timeZoneAbbreviation());
        if (r.max_delay_min > 0)
            quotes += QLatin1Char(' ') + tr("(exchange delay up to %1 min)").arg(r.max_delay_min);
        parts << quotes;
    }
    const QDateTime fetched = r.fetched_at.toLocalTime();
    const qint64 age_s = r.fetched_at.secsTo(QDateTime::currentDateTimeUtc());
    parts << (age_s >= 5 ? tr("fetched %1, cached (F5 re-fetches)").arg(fetched.toString(QStringLiteral("HH:mm:ss")))
                         : tr("fetched %1").arg(fetched.toString(QStringLiteral("HH:mm:ss"))));
    parts << r.source;
    QString text = parts.join(QStringLiteral("  ·  "));
    bool warn = false;
    if (!failure.isEmpty()) {
        text = tr("Refresh failed (%1); showing the last good result.").arg(failure) + QStringLiteral("  ·  ") + text;
        warn = true;
    }
    if (!r.warnings.isEmpty()) {
        text += QStringLiteral("  ·  ") + r.warnings.join(QLatin1Char(' '));
        warn = true;
    }
    QStringList tip;
    if (!r.description.isEmpty())
        tip << r.description;
    if (!r.criteria_text.isEmpty())
        tip << tr("Criteria: %1").arg(r.criteria_text);
    if (!r.criteria_note.isEmpty())
        tip << r.criteria_note;
    tip << r.warnings;
    set_status(text, tip.join(QStringLiteral("\n\n")), warn);

    subtitle_lbl_->setText(source_ == Source::Custom && !r.criteria_text.isEmpty()
                               ? active_title() + QStringLiteral("  ·  ") + r.criteria_text
                               : active_title());
    update_actions();
    update_count_label();
}

void ScreenerScreen::show_message(const QString& text, bool offer_retry) {
    message_lbl_->setText(text);
    retry_btn_->setVisible(offer_retry);
    stack_->setCurrentWidget(message_page_);
}

void ScreenerScreen::set_running(bool running) {
    run_btn_->setEnabled(!running);
    run_btn_->setText(running ? tr("RUNNING...") : tr("RUN"));
    update_actions();
}

void ScreenerScreen::set_status(const QString& text, const QString& tooltip, bool warn) {
    status_lbl_->setText(text);
    status_lbl_->setToolTip(tooltip.isEmpty() ? text : tooltip);
    if (status_lbl_->property("warn").toBool() != warn) {
        status_lbl_->setProperty("warn", warn);
        repolish(status_lbl_);
    }
}

void ScreenerScreen::update_actions() {
    const bool has_rows = model_->rowCount() > 0;
    const bool idle = pending_key_.isEmpty();
    save_btn_->setEnabled(source_ == Source::Custom && builder_->has_catalog());
    export_btn_->setEnabled(has_rows);
    watch_btn_->setEnabled(has_rows);

    const bool pageable = source_ == Source::Predefined || source_ == Source::Custom;
    for (QWidget* w : {static_cast<QWidget*>(page_size_lbl_), static_cast<QWidget*>(page_size_),
                       static_cast<QWidget*>(prev_btn_), static_cast<QWidget*>(next_btn_),
                       static_cast<QWidget*>(page_lbl_)})
        w->setVisible(pageable);
    const int rows = static_cast<int>(shown_.rows.size());
    prev_btn_->setEnabled(pageable && showing_result_ && idle && offset_ > 0);
    next_btn_->setEnabled(pageable && showing_result_ && idle && offset_ + rows < shown_.total);
    page_lbl_->setText(showing_result_ && rows > 0 ? QStringLiteral("%1-%2 / %3")
                                                         .arg(thousands(offset_ + 1), thousands(offset_ + rows),
                                                              thousands(shown_.total))
                                                   : QStringLiteral("—"));
}

void ScreenerScreen::update_count_label() {
    const int all = model_->rowCount();
    const int visible = proxy_->rowCount();
    count_lbl_->setText(search_->text().trimmed().isEmpty() || all == 0
                            ? QString()
                            : tr("%1 of %2 rows").arg(visible).arg(all));
}

void ScreenerScreen::notify_state_changed() {
    ScreenStateManager::instance().notify_changed(this);
}

// ── Builder ───────────────────────────────────────────────────────────────────

void ScreenerScreen::on_builder_edited() {
    if (hub_active_)
        hub_unsubscribe_all();
    source_ = Source::Custom;
    custom_modified_ = true;
    services::ScreenerQuery q;
    QString ignored;
    if (builder_->build_query(&q, &ignored, /*mark_invalid=*/false))
        builder_json_ = q.to_json();
    select_tree_view(QString()); // the list no longer describes what RUN will do
    subtitle_lbl_->setText(active_title());
    if (pending_key_.isEmpty())
        set_status(tr("Criteria changed: press RUN (or Enter) to apply."));
    update_actions();
    notify_state_changed();
}

void ScreenerScreen::new_screen() {
    builder_->reset_to_default();
    saved_name_.clear();
    on_builder_edited();
    set_status(tr("New screen: adjust the criteria, then press RUN."));
}

void ScreenerScreen::save_screen() {
    if (source_ != Source::Custom || !builder_->has_catalog())
        return;
    services::ScreenerQuery q;
    QString error;
    if (!builder_->build_query(&q, &error)) {
        set_status(error, {}, true);
        return;
    }
    q.count = page_size_->currentData().toInt();

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Save Screen"), tr("Name:"), QLineEdit::Normal, saved_name_, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    auto& svc = services::ScreenerService::instance();
    const auto existing = svc.saved_screens();
    if (existing.is_ok()) {
        for (const auto& s : existing.value()) {
            if (s.name.compare(name, Qt::CaseInsensitive) == 0 && s.name.compare(saved_name_, Qt::CaseInsensitive) != 0) {
                if (QMessageBox::question(this, tr("Save Screen"),
                                          tr("A saved screen named \"%1\" already exists. Replace it?").arg(s.name)) !=
                    QMessageBox::Yes)
                    return;
                break;
            }
        }
    }
    const auto r = svc.save_screen(name, q);
    if (r.is_err()) {
        set_status(tr("Could not save the screen: %1").arg(QString::fromStdString(r.error())), {}, true);
        return;
    }
    saved_name_ = name;
    custom_modified_ = false;
    select_tree_view(view_id()); // saved_screens_changed already rebuilt the list
    subtitle_lbl_->setText(active_title());
    set_status(tr("Saved screen \"%1\".").arg(name));
    notify_state_changed();
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap ScreenerScreen::save_state() const {
    QVariantMap m;
    m.insert(QStringLiteral("view"), view_id());
    if (!builder_json_.isEmpty())
        m.insert(QStringLiteral("builder"),
                 QString::fromUtf8(QJsonDocument(builder_json_).toJson(QJsonDocument::Compact)));
    m.insert(QStringLiteral("page_size"), page_size_->currentData().toInt());
    m.insert(QStringLiteral("search"), search_->text());
    return m;
}

void ScreenerScreen::restore_state(const QVariantMap& state) {
    if (catalog_requested_ && builder_->has_catalog())
        apply_state(state);
    else
        pending_state_ = state; // applied once the catalogue has loaded (first show)
}

void ScreenerScreen::apply_state(const QVariantMap& state) {
    {
        const QSignalBlocker block(page_size_);
        const int i = page_size_->findData(state.value(QStringLiteral("page_size"), kDefaultPageSize).toInt());
        page_size_->setCurrentIndex(i >= 0 ? i : page_size_->findData(kDefaultPageSize));
    }
    {
        const QSignalBlocker block(search_);
        search_->setText(state.value(QStringLiteral("search")).toString());
    }
    proxy_->setFilterFixedString(search_->text().trimmed());

    const QJsonObject builder =
        QJsonDocument::fromJson(state.value(QStringLiteral("builder")).toString().toUtf8()).object();
    if (!builder.isEmpty()) {
        const auto q = services::ScreenerQuery::from_json(builder);
        if (!q.is_predefined()) {
            builder_->load_query(q);
            builder_json_ = q.to_json();
        }
    }

    QString view = state.value(QStringLiteral("view")).toString();
    if (view.startsWith(kPredefinedPrefix)) {
        bool known = false;
        for (const auto& v : services::ScreenerService::instance().catalog().value("predefined").toArray())
            known = known || kPredefinedPrefix + v.toObject().value("id").toString() == view;
        if (!known)
            view.clear();
    }
    if (view == kCustomView && builder_json_.isEmpty())
        view.clear();
    if (view.isEmpty())
        view = kPredefinedPrefix + kDefaultPredefined;

    if (view == kCustomView) {
        source_ = Source::Custom;
        saved_name_.clear();
        custom_modified_ = true;
        offset_ = 0;
        select_tree_view(QString());
        run_current(false);
        update_actions();
    } else {
        activate_view(view); // a saved screen that no longer exists reports it in the status line
    }
}

// ── Live large-cap basket ─────────────────────────────────────────────────────

void ScreenerScreen::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    basket_failed_.clear();
    for (const auto& sym : kScreenerBasket) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            basket_failed_.remove(sym);
            basket_quotes_.insert(sym, v.value<services::QuoteData>());
            schedule_basket_rebuild();
        });
        // A failed refresh used to be invisible; track it so the empty state can say so.
        hub.subscribe_errors(this, topic, [this, sym](const QString&) {
            basket_failed_.insert(sym);
            schedule_basket_rebuild();
        });
    }
    hub_active_ = true;

    // Company names (the quote feed only carries the ticker). Cached on disk by the service: the
    // first callback is synchronous with what is cached, the second lands after the lookup.
    QPointer<ScreenerScreen> self = this;
    services::MarketDataService::instance().resolve_names(kScreenerBasket, [self](const QHash<QString, QString>& m) {
        if (!self || m.isEmpty())
            return;
        bool changed = false;
        for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
            if (self->basket_names_.value(it.key()) != it.value()) {
                self->basket_names_.insert(it.key(), it.value());
                changed = true;
            }
        }
        if (changed && self->source_ == Source::Basket)
            self->schedule_basket_rebuild();
    });
}

void ScreenerScreen::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void ScreenerScreen::schedule_basket_rebuild() {
    // ~64 topics deliver separately per refresh sweep: coalesce into one model update on the next
    // event-loop turn. A render coalescer armed by hub deliveries, not a refresh cadence (D3).
    if (basket_rebuild_pending_)
        return;
    basket_rebuild_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        basket_rebuild_pending_ = false;
        if (source_ == Source::Basket)
            rebuild_basket();
    });
}

void ScreenerScreen::rebuild_basket() {
    QVector<services::ScreenerRow> rows;
    rows.reserve(kScreenerBasket.size());
    for (const auto& sym : kScreenerBasket) {
        const auto it = basket_quotes_.constFind(sym);
        if (it == basket_quotes_.constEnd())
            continue;
        const services::QuoteData& q = it.value();
        services::ScreenerRow r;
        r.symbol = sym;
        r.name = basket_names_.value(sym, q.name);
        // A quote with no price is "no data" (0 arrives for a missing figure): show dashes.
        const bool has_price = q.price > 0.0;
        r.price = has_price ? q.price : services::ScreenerRow::kMissing;
        r.change = has_price ? q.change : services::ScreenerRow::kMissing;
        r.change_pct = has_price ? q.change_pct : services::ScreenerRow::kMissing;
        r.volume = q.volume > 0.0 ? q.volume : services::ScreenerRow::kMissing;
        r.currency = QStringLiteral("USD");
        r.market_cap_currency = r.currency;
        rows.append(r);
    }
    const QString keep_symbol = current_symbol().symbol;
    model_->set_result(rows, {});
    if (!keep_symbol.isEmpty() && !table_->currentIndex().isValid())
        select_symbol(keep_symbol);

    const int failed = static_cast<int>(basket_failed_.size());
    if (rows.isEmpty()) {
        const bool all_failed = failed >= kScreenerBasket.size();
        show_message(all_failed ? tr("Quotes are unavailable right now. Press RUN to try again.")
                                : tr("Loading live quotes..."),
                     all_failed);
    } else {
        stack_->setCurrentWidget(table_);
    }
    QString text = tr("%1 of %2 symbols quoted").arg(rows.size()).arg(kScreenerBasket.size());
    if (failed > 0)
        text += QStringLiteral("  ·  ") + tr("%1 unavailable").arg(failed);
    text += QStringLiteral("  ·  ") + tr("live quotes via DataHub (Yahoo Finance, exchange-delayed)");
    set_status(text, tr("A fixed basket of US large caps; sort by clicking a column header. RUN re-requests "
                        "every quote."),
               failed > 0);
    update_actions();
    update_count_label();
}

void ScreenerScreen::refresh_basket() {
    QStringList topics;
    topics.reserve(kScreenerBasket.size());
    for (const auto& sym : kScreenerBasket)
        topics.append(QStringLiteral("market:quote:") + sym);
    datahub::DataHub::instance().request(topics, /*force=*/true); // user-triggered: bypass min_interval
    set_status(tr("Requesting fresh quotes for %1 symbols...").arg(kScreenerBasket.size()));
}

// ── Row actions ───────────────────────────────────────────────────────────────

QString ScreenerScreen::symbol_at(const QModelIndex& proxy_index) const {
    if (!proxy_index.isValid())
        return {};
    const auto* row = model_->row_at(proxy_->mapToSource(proxy_index));
    return row ? row->symbol : QString();
}

QVector<QPair<QString, QString>> ScreenerScreen::selected_symbols() const {
    QModelIndexList picked = table_->selectionModel()->selectedRows(ScreenerResultsModel::ColSymbol);
    std::sort(picked.begin(), picked.end(),
              [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); }); // display order
    QVector<QPair<QString, QString>> out;
    out.reserve(picked.size());
    for (const QModelIndex& idx : std::as_const(picked)) {
        if (const auto* row = model_->row_at(proxy_->mapToSource(idx)))
            out.append({row->symbol, row->name});
    }
    return out;
}

void ScreenerScreen::select_symbol(const QString& symbol) {
    for (int r = 0; r < proxy_->rowCount(); ++r) {
        const QModelIndex idx = proxy_->index(r, ScreenerResultsModel::ColSymbol);
        if (symbol_at(idx).compare(symbol, Qt::CaseInsensitive) == 0) {
            selecting_ = true;
            table_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect |
                                                               QItemSelectionModel::Rows);
            table_->scrollTo(idx, QAbstractItemView::EnsureVisible);
            selecting_ = false;
            return;
        }
    }
}

SymbolRef ScreenerScreen::current_symbol() const {
    const QString sym = symbol_at(table_->currentIndex());
    return sym.isEmpty() ? SymbolRef{} : SymbolRef::equity(sym);
}

void ScreenerScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (ref.is_valid())
        select_symbol(ref.symbol); // highlight it when it is in the results; a follower never re-publishes
}

void ScreenerScreen::open_symbol_in(const QString& screen_id, const QString& symbol) {
    if (symbol.isEmpty())
        return;
    // nav.open_symbol navigates first (constructing the target when needed), then delivers the
    // ticker through IGroupLinked.
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}});
}

void ScreenerScreen::show_row_menu(const QPoint& pos) {
    const QModelIndex idx = table_->indexAt(pos);
    if (!idx.isValid())
        return;
    if (!table_->selectionModel()->isRowSelected(idx.row(), QModelIndex()))
        table_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const QString symbol = symbol_at(idx);
    if (symbol.isEmpty())
        return;
    const auto items = selected_symbols();

    QMenu menu(this);
    connect(menu.addAction(tr("Open %1 in Equity Research").arg(symbol)), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("equity_research"), symbol); });
    connect(menu.addAction(tr("Open %1 in Equity Trading").arg(symbol)), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("equity_trading"), symbol); });
    connect(menu.addAction(tr("Open %1 in News").arg(symbol)), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("news"), symbol); });
    connect(menu.addAction(tr("Open %1 in Backtesting").arg(symbol)), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("backtesting"), symbol); });
    menu.addSeparator();
    QMenu* wl = menu.addMenu(items.size() > 1 ? tr("Add %1 Selected to Watchlist").arg(items.size())
                                              : tr("Add to Watchlist"));
    populate_watchlist_menu(wl, items);
    connect(menu.addAction(items.size() > 1 ? tr("Copy %1 Symbols").arg(items.size()) : tr("Copy Symbol")),
            &QAction::triggered, this, [items]() {
                QStringList symbols;
                for (const auto& p : items)
                    symbols.append(p.first);
                QApplication::clipboard()->setText(symbols.join(QLatin1Char('\n')));
            });
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void ScreenerScreen::populate_watchlist_menu(QMenu* menu, const QVector<QPair<QString, QString>>& items) {
    menu->clear();
    auto& svc = services::ScreenerService::instance();
    const auto lists = svc.watchlists();
    if (lists.is_err()) {
        menu->addAction(tr("Watchlists unavailable"))->setEnabled(false);
        return;
    }
    for (const auto& wl : lists.value()) {
        connect(menu->addAction(wl.name), &QAction::triggered, this,
                [this, id = wl.id, name = wl.name, items]() { add_to_watchlist(id, name, items); });
    }
    if (!lists.value().isEmpty())
        menu->addSeparator();
    connect(menu->addAction(tr("New Watchlist...")), &QAction::triggered, this, [this, items]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Watchlist"), tr("Name:"), QLineEdit::Normal,
                                                   tr("Screener"), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        const auto created = services::ScreenerService::instance().create_watchlist(name);
        if (created.is_err()) {
            set_status(tr("Could not create the watchlist: %1").arg(QString::fromStdString(created.error())), {}, true);
            return;
        }
        add_to_watchlist(created.value().id, created.value().name, items);
    });
}

void ScreenerScreen::add_to_watchlist(const QString& watchlist_id, const QString& list_name,
                                      const QVector<QPair<QString, QString>>& items) {
    if (items.isEmpty())
        return;
    const auto r = services::ScreenerService::instance().add_to_watchlist(watchlist_id, items);
    if (r.is_err()) {
        set_status(tr("Could not add to \"%1\": %2").arg(list_name, QString::fromStdString(r.error())), {}, true);
        return;
    }
    const auto& rep = r.value();
    QString text = tr("Added %1 to \"%2\".").arg(rep.added).arg(list_name);
    if (rep.already_present > 0)
        text += QLatin1Char(' ') + tr("%1 already on the list.").arg(rep.already_present);
    if (rep.failed > 0)
        text += QLatin1Char(' ') + tr("%1 could not be added.").arg(rep.failed);
    set_status(text, {}, rep.failed > 0);
}

void ScreenerScreen::export_csv() {
    if (proxy_->rowCount() == 0)
        return;
    const QString title = source_ == Source::Basket ? QStringLiteral("large_cap_basket")
                                                    : (shown_.title.isEmpty() ? QStringLiteral("screen") : shown_.title);
    QString slug = title.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    const QString default_name =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("screener_%1_%2.csv")
                          .arg(slug, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm"))));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), default_name, tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export CSV"), tr("Could not write %1:\n%2").arg(path, file.errorString()));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Provenance first: an export is useless without its source, criteria and time.
    out << "# source=" << (source_ == Source::Basket ? QStringLiteral("DataHub market:quote (Yahoo Finance)")
                                                     : shown_.source)
        << "; screen=" << title << "; exported_utc=" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        << "; rows=" << proxy_->rowCount();
    if (source_ != Source::Basket) {
        out << "; matches=" << shown_.total << "; offset=" << shown_.offset;
        if (!shown_.criteria_text.isEmpty())
            out << "; criteria=" << QString(shown_.criteria_text).replace(QLatin1Char('\n'), QLatin1Char(' '));
    }
    out << "\n";

    const auto& extra = model_->extra_columns();
    QStringList header = {"Symbol",     "Name",           "Exchange",            "Exchange Code", "Currency",
                          "Price",      "Change",         "Change %",            "Volume",        "Avg Volume 3M",
                          "Market Cap", "Market Cap Currency", "P/E (TTM)",      "Forward P/E",   "Beta",
                          "Sector",     "Industry",       "Reporting Currency"};
    for (const auto& c : extra)
        header.append(c.label);
    header.append(QStringLiteral("Quote Time (UTC)"));
    QStringList quoted;
    for (const auto& h : std::as_const(header))
        quoted.append(csv_text(h));
    out << quoted.join(QLatin1Char(',')) << "\n";

    for (int r = 0; r < proxy_->rowCount(); ++r) {
        const auto* row = model_->row_at(proxy_->mapToSource(proxy_->index(r, 0)));
        if (!row)
            continue;
        QStringList cells = {csv_text(row->symbol),
                             csv_text(row->name),
                             csv_text(row->exchange_name),
                             csv_text(row->exchange),
                             csv_text(row->currency),
                             csv_number(row->price),
                             csv_number(row->change),
                             csv_number(row->change_pct),
                             csv_number(row->volume),
                             csv_number(row->avg_volume_3m),
                             csv_number(row->market_cap),
                             csv_text(row->market_cap_currency),
                             csv_number(row->pe),
                             csv_number(row->forward_pe),
                             csv_number(row->beta),
                             csv_text(row->sector),
                             csv_text(row->industry),
                             csv_text(row->financial_currency)};
        for (const auto& c : extra)
            cells.append(csv_number(row->fields.value(c.id, services::ScreenerRow::kMissing)));
        cells.append(row->market_time > 0
                         ? QDateTime::fromSecsSinceEpoch(row->market_time, QTimeZone::UTC).toString(Qt::ISODate)
                         : QString());
        out << cells.join(QLatin1Char(',')) << "\n";
    }
    file.close();
    set_status(tr("Exported %1 rows to %2").arg(proxy_->rowCount()).arg(QDir::toNativeSeparators(path)));
}

} // namespace fincept::screens
