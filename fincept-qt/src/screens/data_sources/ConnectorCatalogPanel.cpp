// ConnectorCatalogPanel.cpp — Data Sources › CATALOG tab (see ConnectorCatalogPanel.h).

#include "screens/data_sources/ConnectorCatalogPanel.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/ConnectorCatalogModels.h"
#include "services/connectors/ConnectorCatalog.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::datasources {

namespace conn = fincept::services::connectors;
namespace col = fincept::ui::colors;

namespace {

constexpr int kCatalogMaxColumnWidth = 320;
constexpr int kCatalogTabTable = 0;
constexpr int kCatalogTabJson = 1;
constexpr int kCatalogTabRaw = 2;

QString catalog_csv_field(const QString& s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) ||
        s.contains(QLatin1Char('\r'))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

/// Quote an argument for the args box so it survives QProcess::splitCommand round-trips.
QString catalog_quote_arg(const QString& a) {
    if (a.isEmpty() || a.contains(QLatin1Char(' ')) || a.contains(QLatin1Char('"')))
        return QLatin1Char('"') + QString(a).replace(QLatin1Char('"'), QStringLiteral("\"\"\"")) + QLatin1Char('"');
    return a;
}

} // namespace

ConnectorCatalogPanel::ConnectorCatalogPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dcatRoot"));
    build_ui();
    refresh_theme();

    auto& svc = conn::ConnectorService::instance();
    connect(&svc, &conn::ConnectorService::result_ready, this, &ConnectorCatalogPanel::on_result);
    connect(&svc, &conn::ConnectorService::error_occurred, this, &ConnectorCatalogPanel::on_error);
    connect(&svc, &conn::ConnectorService::key_status_changed, this, [this]() {
        if (list_model_)
            list_model_->refresh_key_states();
        update_key_status();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorCatalogPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("dcatSplitter"));
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(build_list_pane());
    splitter->addWidget(build_detail_pane());
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({460, 700});
    root->addWidget(splitter);

    // Ctrl+Enter runs the selected command from anywhere in the tab.
    for (const QKeySequence& seq : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, &ConnectorCatalogPanel::on_run);
    }
}

QWidget* ConnectorCatalogPanel::build_list_pane() {
    auto* pane = new QWidget(this);
    pane->setObjectName(QStringLiteral("dcatLeft"));
    auto* vl = new QVBoxLayout(pane);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(pane);
    hdr->setObjectName(QStringLiteral("dcatBar"));
    hdr->setFixedHeight(30);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    list_title_ = new QLabel(tr("CONNECTOR CATALOG"), hdr);
    list_title_->setObjectName(QStringLiteral("dcatBarTitle"));
    hl->addWidget(list_title_);
    hl->addStretch();
    count_label_ = new QLabel(hdr);
    count_label_->setObjectName(QStringLiteral("dcatCount"));
    hl->addWidget(count_label_);
    vl->addWidget(hdr);

    auto* filters = new QWidget(pane);
    filters->setObjectName(QStringLiteral("dcatFilters"));
    auto* fl = new QVBoxLayout(filters);
    fl->setContentsMargins(10, 8, 10, 8);
    fl->setSpacing(6);
    search_ = new QLineEdit(filters);
    search_->setObjectName(QStringLiteral("dcatSearch"));
    search_->setClearButtonEnabled(true);
    search_->setPlaceholderText(tr("Search name, description, category, region, key, command…"));
    search_->setAccessibleName(tr("Search data connectors"));
    fl->addWidget(search_);

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    category_ = new QComboBox(filters);
    category_->setObjectName(QStringLiteral("dcatCombo"));
    category_->setAccessibleName(tr("Connector category"));
    category_->addItem(tr("All categories"), QString());
    row->addWidget(category_, 1);
    keyless_only_ = new QCheckBox(tr("Keyless only"), filters);
    keyless_only_->setObjectName(QStringLiteral("dcatCheck"));
    keyless_only_->setToolTip(tr("Only connectors that need no API key"));
    row->addWidget(keyless_only_);
    configured_only_ = new QCheckBox(tr("Configured keys"), filters);
    configured_only_->setObjectName(QStringLiteral("dcatCheck"));
    configured_only_->setToolTip(tr("Only connectors whose API keys are already stored"));
    row->addWidget(configured_only_);
    fl->addLayout(row);
    vl->addWidget(filters);

    list_model_ = new CatalogListModel(this);
    proxy_ = new CatalogFilterProxy(this);
    proxy_->setSourceModel(list_model_);

    list_ = new QTableView(pane);
    list_->setObjectName(QStringLiteral("dcatList"));
    list_->setModel(proxy_);
    list_->setSortingEnabled(true);
    list_->sortByColumn(-1, Qt::AscendingOrder); // keep catalogue order (category, title) until asked
    list_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setShowGrid(false);
    list_->setAlternatingRowColors(true);
    list_->setWordWrap(false);
    list_->verticalHeader()->setVisible(false);
    list_->verticalHeader()->setDefaultSectionSize(24);
    list_->horizontalHeader()->setHighlightSections(false);
    list_->horizontalHeader()->setSectionResizeMode(CatalogListModel::ColConnector, QHeaderView::Stretch);
    list_->horizontalHeader()->setSectionResizeMode(CatalogListModel::ColCategory, QHeaderView::Interactive);
    list_->horizontalHeader()->setSectionResizeMode(CatalogListModel::ColRegion, QHeaderView::Interactive);
    list_->horizontalHeader()->setSectionResizeMode(CatalogListModel::ColKey, QHeaderView::Interactive);
    list_->setColumnWidth(CatalogListModel::ColCategory, 150);
    list_->setColumnWidth(CatalogListModel::ColRegion, 100);
    list_->setColumnWidth(CatalogListModel::ColKey, 92);
    list_->setAccessibleName(tr("Data connectors"));
    vl->addWidget(list_, 1);

    list_empty_ = new QLabel(pane);
    list_empty_->setObjectName(QStringLiteral("dcatHintText"));
    list_empty_->setWordWrap(true);
    list_empty_->setContentsMargins(12, 10, 12, 10);
    list_empty_->hide();
    vl->addWidget(list_empty_);

    connect(search_, &QLineEdit::textChanged, this, [this](const QString& t) {
        proxy_->set_search(t);
        update_count_label();
    });
    connect(category_, &QComboBox::currentIndexChanged, this, [this](int) {
        proxy_->set_category(category_->currentData().toString());
        update_count_label();
    });
    connect(keyless_only_, &QCheckBox::toggled, this, [this](bool on) {
        proxy_->set_keyless_only(on);
        update_count_label();
    });
    connect(configured_only_, &QCheckBox::toggled, this, [this](bool on) {
        proxy_->set_configured_only(on);
        update_count_label();
    });
    connect(list_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { on_selection_changed(); });
    return pane;
}

QWidget* ConnectorCatalogPanel::build_detail_pane() {
    auto* pane = new QWidget(this);
    pane->setObjectName(QStringLiteral("dcatRight"));
    auto* vl = new QVBoxLayout(pane);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* hdr = new QWidget(pane);
    hdr->setObjectName(QStringLiteral("dcatBar"));
    hdr->setFixedHeight(30);
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(12, 0, 12, 0);
    detail_title_ = new QLabel(tr("CONNECTOR"), hdr);
    detail_title_->setObjectName(QStringLiteral("dcatBarTitle"));
    hl->addWidget(detail_title_);
    hl->addStretch();
    vl->addWidget(hdr);

    // ── Identity ──
    auto* info = new QWidget(pane);
    info->setObjectName(QStringLiteral("dcatInfo"));
    auto* il = new QVBoxLayout(info);
    il->setContentsMargins(14, 10, 14, 10);
    il->setSpacing(4);
    title_ = new QLabel(tr("Select a connector"), info);
    title_->setObjectName(QStringLiteral("dcatTitle"));
    title_->setWordWrap(true);
    il->addWidget(title_);
    meta_ = new QLabel(info);
    meta_->setObjectName(QStringLiteral("dcatMeta"));
    meta_->setWordWrap(true);
    meta_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    il->addWidget(meta_);
    desc_ = new QLabel(tr("Pick a connector on the left to see its commands, key requirements and to run it."), info);
    desc_->setObjectName(QStringLiteral("dcatDesc"));
    desc_->setWordWrap(true);
    il->addWidget(desc_);
    docs_ = new QLabel(info);
    docs_->setObjectName(QStringLiteral("dcatDocs"));
    docs_->setOpenExternalLinks(true);
    docs_->setTextFormat(Qt::RichText);
    docs_->hide();
    il->addWidget(docs_);
    broken_ = new QLabel(info);
    broken_->setObjectName(QStringLiteral("dcatBroken"));
    broken_->setWordWrap(true);
    broken_->hide();
    il->addWidget(broken_);
    vl->addWidget(info);

    // ── API key ──
    key_box_ = new QFrame(pane);
    key_box_->setObjectName(QStringLiteral("dcatKeyBox"));
    auto* kl = new QVBoxLayout(key_box_);
    kl->setContentsMargins(14, 8, 14, 8);
    kl->setSpacing(6);
    key_status_ = new QLabel(key_box_);
    key_status_->setObjectName(QStringLiteral("dcatKeyStatus"));
    key_status_->setWordWrap(true);
    kl->addWidget(key_status_);
    key_rows_ = new QVBoxLayout;
    key_rows_->setSpacing(4);
    kl->addLayout(key_rows_);
    key_box_->hide();
    vl->addWidget(key_box_);

    // ── Command + args + actions ──
    auto* run = new QWidget(pane);
    run->setObjectName(QStringLiteral("dcatRun"));
    auto* rl = new QVBoxLayout(run);
    rl->setContentsMargins(14, 8, 14, 8);
    rl->setSpacing(6);

    auto* cmd_row = new QHBoxLayout;
    cmd_row->setSpacing(8);
    command_label_ = new QLabel(tr("COMMAND"), run);
    command_label_->setObjectName(QStringLiteral("dcatLabel"));
    cmd_row->addWidget(command_label_);
    command_ = new QComboBox(run);
    command_->setObjectName(QStringLiteral("dcatCombo"));
    command_->setMinimumWidth(200);
    command_->setAccessibleName(tr("Connector command"));
    cmd_row->addWidget(command_);
    hint_ = new QLabel(run);
    hint_->setObjectName(QStringLiteral("dcatHint"));
    hint_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cmd_row->addWidget(hint_, 1);
    rl->addLayout(cmd_row);

    args_label_ = new QLabel(tr("ARGUMENTS — space-separated, one per line, or a JSON object"), run);
    args_label_->setObjectName(QStringLiteral("dcatLabel"));
    rl->addWidget(args_label_);
    args_ = new QPlainTextEdit(run);
    args_->setObjectName(QStringLiteral("dcatArgs"));
    args_->setFixedHeight(52);
    args_->setTabChangesFocus(true);
    args_->setAccessibleName(tr("Command arguments"));
    rl->addWidget(args_);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(6);
    run_btn_ = new QPushButton(tr("RUN"), run);
    run_btn_->setObjectName(QStringLiteral("dcatBtnAccent"));
    run_btn_->setFixedHeight(26);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setToolTip(tr("Run the command (Ctrl+Enter)"));
    run_btn_->setEnabled(false);
    btns->addWidget(run_btn_);
    force_ = new QCheckBox(tr("Bypass cache"), run);
    force_->setObjectName(QStringLiteral("dcatCheck"));
    force_->setToolTip(tr("Fetch fresh data even when a cached result is still valid"));
    btns->addWidget(force_);
    btns->addStretch();
    export_btn_ = new QPushButton(tr("EXPORT CSV"), run);
    export_btn_->setObjectName(QStringLiteral("dcatBtn"));
    export_btn_->setFixedHeight(26);
    export_btn_->setEnabled(false);
    btns->addWidget(export_btn_);
    copy_btn_ = new QPushButton(tr("COPY AS AI TOOL CALL"), run);
    copy_btn_->setObjectName(QStringLiteral("dcatBtn"));
    copy_btn_->setFixedHeight(26);
    copy_btn_->setEnabled(false);
    copy_btn_->setToolTip(tr("Copy the equivalent MCP tool call (tool name + arguments) to the clipboard"));
    btns->addWidget(copy_btn_);
    rl->addLayout(btns);

    status_ = new QLabel(run);
    status_->setObjectName(QStringLiteral("dcatStatus"));
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rl->addWidget(status_);
    vl->addWidget(run);

    // ── Results ──
    result_stack_ = new QStackedWidget(pane);
    result_stack_->setObjectName(QStringLiteral("dcatResults"));

    empty_label_ = new QLabel(tr("Select a connector, pick a command and press RUN (Ctrl+Enter)."), result_stack_);
    empty_label_->setObjectName(QStringLiteral("dcatPlaceholder"));
    empty_label_->setAlignment(Qt::AlignCenter);
    empty_label_->setWordWrap(true);
    result_stack_->addWidget(empty_label_);

    loading_label_ = new QLabel(result_stack_);
    loading_label_->setObjectName(QStringLiteral("dcatPlaceholder"));
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_label_->setWordWrap(true);
    result_stack_->addWidget(loading_label_);

    tabs_ = new QTabWidget(result_stack_);
    tabs_->setObjectName(QStringLiteral("dcatTabs"));
    tabs_->setDocumentMode(true);
    result_model_ = new ConnectorResultModel(this);
    result_proxy_ = new QSortFilterProxyModel(this);
    result_proxy_->setSourceModel(result_model_);
    result_proxy_->setSortRole(ConnectorResultModel::kSortRole);
    result_table_ = new QTableView(tabs_);
    result_table_->setObjectName(QStringLiteral("dcatResultTable"));
    result_table_->setModel(result_proxy_);
    result_table_->setSortingEnabled(true);
    result_table_->setAlternatingRowColors(true);
    result_table_->setShowGrid(false);
    result_table_->setWordWrap(false);
    result_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result_table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    result_table_->verticalHeader()->setDefaultSectionSize(22);
    result_table_->horizontalHeader()->setResizeContentsPrecision(100);
    result_table_->horizontalHeader()->setHighlightSections(false);
    tabs_->addTab(result_table_, tr("TABLE"));
    json_view_ = new QPlainTextEdit(tabs_);
    json_view_->setObjectName(QStringLiteral("dcatText"));
    json_view_->setReadOnly(true);
    json_view_->setLineWrapMode(QPlainTextEdit::NoWrap);
    tabs_->addTab(json_view_, tr("JSON"));
    raw_view_ = new QPlainTextEdit(tabs_);
    raw_view_->setObjectName(QStringLiteral("dcatText"));
    raw_view_->setReadOnly(true);
    raw_view_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    tabs_->addTab(raw_view_, tr("RAW"));
    result_stack_->addWidget(tabs_);

    error_label_ = new QLabel(result_stack_);
    error_label_->setObjectName(QStringLiteral("dcatErrorText"));
    error_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    error_label_->setWordWrap(true);
    error_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error_label_->setContentsMargins(14, 12, 14, 12);
    result_stack_->addWidget(error_label_);
    vl->addWidget(result_stack_, 1);

    connect(command_, &QComboBox::currentIndexChanged, this, [this](int) { on_command_changed(); });
    connect(run_btn_, &QPushButton::clicked, this, &ConnectorCatalogPanel::on_run);
    connect(export_btn_, &QPushButton::clicked, this, &ConnectorCatalogPanel::on_export_csv);
    connect(copy_btn_, &QPushButton::clicked, this, &ConnectorCatalogPanel::on_copy_tool_call);
    // JSON / RAW text can be ~1 MB each — fill a tab only when it is first opened for a result.
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int idx) {
        if (!has_result_)
            return;
        if (idx == kCatalogTabJson && json_view_->document()->isEmpty())
            json_view_->setPlainText(last_result_.pretty_text);
        else if (idx == kCatalogTabRaw && raw_view_->document()->isEmpty())
            raw_view_->setPlainText(last_result_.raw_text);
    });
    set_result_state(ResultState::Empty);
    return pane;
}

void ConnectorCatalogPanel::refresh_theme() {
    // The one stylesheet for the whole tab (objectName selectors, no per-widget calls).
    const QString qss =
        QStringLiteral(
            "#dcatRoot { background:%1; }"
            "#dcatRoot QLabel, #dcatRoot QCheckBox, #dcatRoot QLineEdit, #dcatRoot QComboBox, #dcatRoot QPlainTextEdit,"
            " #dcatRoot QTableView, #dcatRoot QPushButton { font-family:'Consolas','Courier New',monospace; }"
            "#dcatLeft { background:%1; border-right:1px solid %4; }"
            "#dcatRight { background:%2; }"
            "#dcatBar { background:%3; border-bottom:1px solid %4; }"
            "#dcatBarTitle { color:%5; font-size:10px; font-weight:700; letter-spacing:1px; background:transparent; }"
            "#dcatCount { color:%8; font-size:11px; background:transparent; }"
            "#dcatFilters { background:%2; border-bottom:1px solid %4; }"
            "#dcatSearch, #dcatArgs { background:%1; color:%10; border:1px solid %4; padding:3px 8px; font-size:12px; }"
            "#dcatSearch:focus, #dcatArgs:focus { border-color:%5; }"
            "#dcatCombo { background:%1; color:%10; border:1px solid %4; padding:2px 6px; font-size:11px; }"
            "#dcatCombo QAbstractItemView { background:%2; color:%10; selection-background-color:%9; }"
            "#dcatCheck { color:%6; font-size:11px; background:transparent; }"
            "#dcatCheck::indicator { width:12px; height:12px; border:1px solid %7; background:%1; }"
            "#dcatCheck::indicator:checked { background:%5; border-color:%5; }"
            "#dcatList, #dcatResultTable { background:%1; alternate-background-color:%11; color:%10; border:none;"
            " gridline-color:transparent; font-size:11px; selection-background-color:%12; selection-color:%10; }"
            "#dcatList::item, #dcatResultTable::item { padding:0 6px; border-bottom:1px solid %4; }"
            "#dcatList QHeaderView::section, #dcatResultTable QHeaderView::section { background:%2; color:%8;"
            " font-size:10px; font-weight:700; letter-spacing:0.5px; border:none; border-bottom:1px solid %4;"
            " border-right:1px solid %4; padding:0 6px; height:24px; }"
            "#dcatResultTable QTableCornerButton::section { background:%2; border:none; }"
            "#dcatHintText, #dcatPlaceholder { color:%8; font-size:12px; background:transparent; }"
            "#dcatInfo, #dcatRun { background:%2; border-bottom:1px solid %4; }"
            "#dcatTitle { color:%10; font-size:15px; font-weight:700; background:transparent; }"
            "#dcatMeta { color:%8; font-size:11px; background:transparent; }"
            "#dcatDesc { color:%6; font-size:12px; background:transparent; }"
            "#dcatDocs { font-size:11px; background:transparent; }"
            "#dcatBroken { color:%13; font-size:11px; background:%14; border:1px solid %13; padding:6px 8px; }"
            "#dcatKeyBox { background:%3; border-bottom:1px solid %4; }"
            "#dcatKeyStatus { font-size:11px; font-weight:700; background:transparent; color:%6; }"
            "#dcatKeyStatus[state=\"ready\"] { color:%15; }"
            "#dcatKeyStatus[state=\"missing\"] { color:%16; }"
            "#dcatKeyName { color:%10; font-size:11px; font-weight:700; background:transparent; }"
            "#dcatKeyEdit { background:%1; color:%10; border:1px solid %4; padding:3px 6px; font-size:11px; }"
            "#dcatKeyEdit:focus { border-color:%5; }"
            "#dcatLabel { color:%5; font-size:10px; font-weight:700; letter-spacing:1px; background:transparent; }"
            "#dcatHint { color:%8; font-size:11px; background:transparent; }"
            "#dcatBtn { background:%2; color:%6; border:1px solid %4; padding:0 10px; font-size:11px; font-weight:700;"
            " letter-spacing:0.5px; }"
            "#dcatBtn:hover:enabled { background:%9; color:%10; border-color:%7; }"
            "#dcatBtn:disabled { color:%8; }"
            "#dcatBtnAccent { background:%5; color:%17; border:1px solid %5; padding:0 16px; font-size:11px;"
            " font-weight:700; letter-spacing:1px; }"
            "#dcatBtnAccent:hover:enabled { background:%18; border-color:%18; }"
            "#dcatBtnAccent:disabled { background:%3; color:%8; border-color:%4; }"
            "#dcatStatus { color:%8; font-size:11px; background:transparent; }"
            "#dcatStatus[state=\"ok\"] { color:%15; }"
            "#dcatStatus[state=\"warning\"] { color:%16; }"
            "#dcatStatus[state=\"error\"] { color:%13; }"
            "#dcatStatus[state=\"busy\"] { color:%5; }"
            "#dcatResults { background:%1; }"
            "#dcatTabs::pane { border:none; border-top:1px solid %4; }"
            "#dcatTabs QTabBar::tab { background:transparent; color:%6; padding:6px 16px; font-size:11px;"
            " font-weight:700; letter-spacing:0.5px; border:none; border-bottom:2px solid transparent; }"
            "#dcatTabs QTabBar::tab:selected { color:%5; border-bottom:2px solid %5; }"
            "#dcatTabs QTabBar::tab:hover:!selected { color:%10; }"
            "#dcatText { background:%1; color:%10; border:none; font-size:11px; }"
            "#dcatErrorText { color:%13; font-size:12px; background:%1; }"
            "#dcatSplitter::handle { background:%4; }")
            .arg(col::BG_BASE(), col::BG_SURFACE(), col::BG_RAISED(), col::BORDER_DIM(), col::AMBER(),
                 col::TEXT_SECONDARY(), col::BORDER_MED(), col::TEXT_TERTIARY(), col::BG_HOVER())
            .arg(col::TEXT_PRIMARY(), col::ROW_ALT(), col::ACCENT_BG(), col::NEGATIVE(), col::NEGATIVE_BG(),
                 col::POSITIVE(), col::WARNING(), col::TEXT_ON_ACCENT(), col::AMBER_DIM());
    setStyleSheet(qss);
}

void ConnectorCatalogPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate_ui();
    QWidget::changeEvent(event);
}

void ConnectorCatalogPanel::retranslate_ui() {
    list_title_->setText(tr("CONNECTOR CATALOG"));
    search_->setPlaceholderText(tr("Search name, description, category, region, key, command…"));
    search_->setAccessibleName(tr("Search data connectors"));
    if (category_->count() > 0)
        category_->setItemText(0, tr("All categories"));
    keyless_only_->setText(tr("Keyless only"));
    keyless_only_->setToolTip(tr("Only connectors that need no API key"));
    configured_only_->setText(tr("Configured keys"));
    configured_only_->setToolTip(tr("Only connectors whose API keys are already stored"));
    detail_title_->setText(tr("CONNECTOR"));
    command_label_->setText(tr("COMMAND"));
    args_label_->setText(tr("ARGUMENTS — space-separated, one per line, or a JSON object"));
    run_btn_->setText(tr("RUN"));
    run_btn_->setToolTip(tr("Run the command (Ctrl+Enter)"));
    force_->setText(tr("Bypass cache"));
    force_->setToolTip(tr("Fetch fresh data even when a cached result is still valid"));
    export_btn_->setText(tr("EXPORT CSV"));
    copy_btn_->setText(tr("COPY AS AI TOOL CALL"));
    copy_btn_->setToolTip(tr("Copy the equivalent MCP tool call (tool name + arguments) to the clipboard"));
    empty_label_->setText(tr("Select a connector, pick a command and press RUN (Ctrl+Enter)."));
    tabs_->setTabText(kCatalogTabTable, tr("TABLE"));
    tabs_->setTabText(kCatalogTabJson, tr("JSON"));
    tabs_->setTabText(kCatalogTabRaw, tr("RAW"));
    emit list_model_->headerDataChanged(Qt::Horizontal, 0, CatalogListModel::ColCount - 1);
    update_count_label();
    show_connector(conn::ConnectorCatalog::instance().find(current_name_));
}

// ─────────────────────────────────────────────────────────────────────────────
// Catalogue loading / list
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorCatalogPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!loaded_)
        load_catalog();
    // Key presence is read off the GUI thread; the list repaints on key_status_changed.
    conn::ConnectorService::instance().refresh_key_status();
}

void ConnectorCatalogPanel::load_catalog() {
    loaded_ = true;
    auto& catalog = conn::ConnectorCatalog::instance();
    if (!catalog.ensure_loaded()) {
        list_empty_->setText(tr("The connector catalogue could not be loaded: %1").arg(catalog.load_error()));
        list_empty_->show();
        count_label_->setText(tr("UNAVAILABLE"));
        return;
    }
    list_model_->reload();
    populate_categories();
    update_count_label();
    if (!pending_select_.isEmpty()) {
        const QString target = pending_select_;
        pending_select_.clear();
        select_connector(target);
    }
}

void ConnectorCatalogPanel::populate_categories() {
    const QString keep = category_->currentData().toString();
    category_->blockSignals(true);
    while (category_->count() > 1)
        category_->removeItem(1);
    for (const QString& cat : conn::ConnectorCatalog::instance().categories())
        category_->addItem(cat, cat);
    const int idx = category_->findData(keep);
    category_->setCurrentIndex(idx >= 0 ? idx : 0);
    category_->blockSignals(false);
}

void ConnectorCatalogPanel::update_count_label() {
    if (!loaded_ || !list_model_)
        return;
    const int total = list_model_->rowCount();
    const int shown = proxy_->rowCount();
    int working = 0;
    for (const auto& c : conn::ConnectorCatalog::instance().connectors())
        if (c.broken.isEmpty())
            ++working;
    count_label_->setText(shown == total ? tr("%1 CONNECTORS · %2 WORKING").arg(total).arg(working)
                                         : tr("%1 OF %2").arg(shown).arg(total));
    const bool none = total > 0 && shown == 0;
    list_empty_->setText(tr("No connector matches the search and filters."));
    list_empty_->setVisible(none);
}

void ConnectorCatalogPanel::select_connector(const QString& name) {
    if (name.isEmpty())
        return;
    if (!loaded_) {
        pending_select_ = name;
        return;
    }
    const int src_row = list_model_->row_of(name);
    if (src_row < 0)
        return;
    const QModelIndex pidx = proxy_->mapFromSource(list_model_->index(src_row, 0));
    if (!pidx.isValid())
        return; // filtered out
    list_->selectionModel()->setCurrentIndex(pidx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    list_->scrollTo(pidx);
}

void ConnectorCatalogPanel::on_selection_changed() {
    const QModelIndex pidx = list_->selectionModel()->currentIndex();
    const QModelIndex sidx = proxy_->mapToSource(pidx);
    show_connector(sidx.isValid() ? list_model_->connector_at(sidx.row()) : nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Detail
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorCatalogPanel::show_connector(const conn::ConnectorInfo* c) {
    const bool changed = !c || c->name != current_name_;
    current_name_ = c ? c->name : QString();
    run_btn_->setEnabled(c != nullptr);
    copy_btn_->setEnabled(c != nullptr && c->mcp && c->broken.isEmpty());
    if (!c) {
        title_->setText(tr("Select a connector"));
        meta_->clear();
        desc_->setText(tr("Pick a connector on the left to see its commands, key requirements and to run it."));
        docs_->hide();
        broken_->hide();
        key_box_->hide();
        command_->clear();
        hint_->clear();
        args_->clear();
        return;
    }

    title_->setText(c->title);
    meta_->setText(QStringLiteral("%1  ·  %2  ·  %3  ·  %4").arg(c->name, c->script, c->category, c->region));
    desc_->setText(c->description);
    if (!c->docs_url.isEmpty()) {
        docs_->setText(QStringLiteral("<a style=\"color:%1;text-decoration:none\" href=\"%2\">%3</a>")
                           .arg(QString(col::AMBER()), c->docs_url.toHtmlEscaped(), tr("Provider docs / get a key ↗")));
        docs_->show();
    } else {
        docs_->hide();
    }
    if (!c->broken.isEmpty()) {
        broken_->setText(
            tr("KNOWN ISSUE — %1. You can still run it here; it is not offered to the AI.").arg(c->broken));
        broken_->show();
    } else {
        broken_->hide();
    }
    if (!c->mcp && c->broken.isEmpty())
        copy_btn_->setToolTip(tr("Not an AI tool: a dedicated tool already covers this provider"));
    else
        copy_btn_->setToolTip(tr("Copy the equivalent MCP tool call (tool name + arguments) to the clipboard"));

    if (changed) {
        rebuild_key_rows(c);
        command_->blockSignals(true);
        command_->clear();
        for (const auto& cmd : c->commands) {
            command_->addItem(cmd.name, cmd.name);
            command_->setItemData(command_->count() - 1, cmd.args_hint.isEmpty() ? tr("No arguments") : cmd.args_hint,
                                  Qt::ToolTipRole);
        }
        const int idx = command_->findData(c->default_command);
        command_->setCurrentIndex(idx >= 0 ? idx : 0);
        command_->blockSignals(false);
        on_command_changed();
        // A new connector starts from a clean result area.
        pending_request_.clear();
        has_result_ = false;
        result_model_->clear();
        json_view_->clear();
        raw_view_->clear();
        export_btn_->setEnabled(false);
        set_status(QString(), QStringLiteral("idle"));
        set_result_state(ResultState::Empty);
    }
    update_key_status();
}

void ConnectorCatalogPanel::rebuild_key_rows(const conn::ConnectorInfo* c) {
    // Drop the previous connector's rows (a connector has at most a couple of keys).
    while (QLayoutItem* item = key_rows_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    if (!c || c->env_keys.isEmpty()) {
        key_box_->setVisible(c != nullptr);
        return;
    }
    for (const QString& env_key : c->env_keys) {
        auto* row = new QWidget(key_box_);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(6);
        auto* name = new QLabel(env_key, row);
        name->setObjectName(QStringLiteral("dcatKeyName"));
        name->setMinimumWidth(190);
        hl->addWidget(name);
        auto* field = new QLineEdit(row);
        field->setObjectName(QStringLiteral("dcatKeyEdit"));
        field->setEchoMode(QLineEdit::Password);
        field->setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
        field->setDragEnabled(false);
        field->setAcceptDrops(false);
        field->setAccessibleName(tr("%1 value").arg(env_key));
        hl->addWidget(field, 1);
        auto* save = new QPushButton(tr("SAVE"), row);
        save->setObjectName(QStringLiteral("dcatBtnAccent"));
        save->setFixedHeight(24);
        save->setCursor(Qt::PointingHandCursor);
        save->setAccessibleName(tr("Save %1").arg(env_key));
        hl->addWidget(save);
        connect(save, &QPushButton::clicked, this, [this, env_key, field]() { save_key(env_key, field); });
        connect(field, &QLineEdit::returnPressed, save, &QPushButton::click);
        key_rows_->addWidget(row);
    }
    key_box_->show();
}

void ConnectorCatalogPanel::update_key_status() {
    const conn::ConnectorInfo* c = conn::ConnectorCatalog::instance().find(current_name_);
    if (!c) {
        key_box_->hide();
        return;
    }
    key_box_->show();
    auto& catalog = conn::ConnectorCatalog::instance();
    const conn::KeyState ks = catalog.key_state(*c);
    QString text;
    QString state = QStringLiteral("none");
    switch (ks) {
        case conn::KeyState::NotNeeded:
            text = tr("No API key needed.");
            break;
        case conn::KeyState::Ready:
            text = tr("● API key configured (%1).").arg(c->env_keys.join(QStringLiteral(", ")));
            state = QStringLiteral("ready");
            break;
        case conn::KeyState::Partial:
            text = tr("◐ Some keys are missing (%1).").arg(c->env_keys.join(QStringLiteral(", ")));
            state = QStringLiteral("missing");
            break;
        case conn::KeyState::Missing:
            text = c->key_required ? tr("○ Needs an API key: %1. Paste it below — it is stored encrypted and passed "
                                        "to the connector.")
                                         .arg(c->env_keys.join(QStringLiteral(", ")))
                                   : tr("○ Optional key: %1 — the connector works without it.")
                                         .arg(c->env_keys.join(QStringLiteral(", ")));
            state = c->key_required ? QStringLiteral("missing") : QStringLiteral("none");
            break;
        case conn::KeyState::Unknown:
            text = tr("Checking stored keys…");
            break;
    }
    if (!c->key_note.isEmpty())
        text += QLatin1Char(' ') + c->key_note;
    key_status_->setText(text);
    key_status_->setProperty("state", state);
    key_status_->style()->unpolish(key_status_);
    key_status_->style()->polish(key_status_);

    // Field placeholders say whether a value is stored — never what it is.
    for (int i = 0; i < key_rows_->count(); ++i) {
        QWidget* row = key_rows_->itemAt(i)->widget();
        if (!row)
            continue;
        auto* field = row->findChild<QLineEdit*>();
        if (!field || i >= c->env_keys.size())
            continue;
        const auto present = catalog.cached_key_present(c->env_keys[i]);
        field->setPlaceholderText(present.value_or(false) ? tr("•••••••• stored — paste a new value to replace")
                                                          : tr("paste key…"));
    }
}

void ConnectorCatalogPanel::save_key(const QString& env_key, QLineEdit* field) {
    if (!field)
        return;
    const QString value = field->text().trimmed();
    if (value.isEmpty()) {
        set_status(tr("Paste the %1 value first.").arg(env_key), QStringLiteral("warning"));
        return;
    }
    QString error;
    if (conn::ConnectorService::instance().store_key(env_key, value, &error)) {
        field->clear();
        set_status(tr("%1 saved. Python connectors receive it from the next run.").arg(env_key), QStringLiteral("ok"));
    } else {
        set_status(error, QStringLiteral("error"));
    }
    update_key_status();
}

void ConnectorCatalogPanel::on_command_changed() {
    const conn::ConnectorInfo* c = conn::ConnectorCatalog::instance().find(current_name_);
    if (!c)
        return;
    const QString cmd = command_->currentData().toString();
    const QString hint = c->args_hint(cmd);
    hint_->setText(hint.isEmpty() ? tr("no arguments") : tr("args: %1").arg(hint));
    args_->setPlaceholderText(hint.isEmpty() ? tr("No arguments needed") : hint);
    if (cmd == c->default_command && !c->default_args.isEmpty()) {
        QStringList quoted;
        for (const QString& a : c->default_args)
            quoted << catalog_quote_arg(a);
        args_->setPlainText(quoted.join(QLatin1Char(' ')));
    } else {
        args_->clear();
    }
}

QStringList ConnectorCatalogPanel::current_args() const {
    const QString t = args_->toPlainText().trimmed();
    if (t.isEmpty())
        return {};
    if (t.startsWith(QLatin1Char('{')) || t.startsWith(QLatin1Char('[')))
        return {t}; // a JSON payload is one argument
    if (t.contains(QLatin1Char('\n'))) {
        QStringList out;
        for (const QString& line : t.split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty())
                out << line.trimmed();
        return out;
    }
    return QProcess::splitCommand(t);
}

// ─────────────────────────────────────────────────────────────────────────────
// Run / results
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorCatalogPanel::on_run() {
    const conn::ConnectorInfo* c = conn::ConnectorCatalog::instance().find(current_name_);
    if (!c || !isVisible())
        return;
    const QString cmd = command_->currentData().toString();
    if (cmd.isEmpty())
        return;
    const QStringList args = current_args();
    pending_request_ = conn::ConnectorService::instance().run(c->name, cmd, args, force_->isChecked());
    const QString line = (QStringList{c->name, cmd} + args).join(QLatin1Char(' '));
    set_status(tr("Running %1 …").arg(line), QStringLiteral("busy"));
    set_result_state(ResultState::Loading, tr("RUNNING  %1").arg(line));
    LOG_INFO("DataSources", QString("Catalog run: %1 %2").arg(c->name, cmd));
}

void ConnectorCatalogPanel::on_result(const QString& request_id, const conn::ConnectorResult& result) {
    if (request_id != pending_request_)
        return;
    pending_request_.clear();
    last_result_ = result;
    has_result_ = true;

    result_table_->setSortingEnabled(false);
    result_model_->set_table(result.table, result.columns, result.table_kind);
    result_proxy_->sort(-1);
    result_table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    result_table_->setSortingEnabled(true);
    result_table_->resizeColumnsToContents();
    for (int i = 0; i < result_model_->columnCount(); ++i)
        if (result_table_->columnWidth(i) > kCatalogMaxColumnWidth)
            result_table_->setColumnWidth(i, kCatalogMaxColumnWidth);
    json_view_->clear();
    raw_view_->clear();
    const int tab = tabs_->currentIndex();
    if (tab == kCatalogTabJson)
        json_view_->setPlainText(result.pretty_text);
    else if (tab == kCatalogTabRaw)
        raw_view_->setPlainText(result.raw_text);
    export_btn_->setEnabled(result.table.size() > 0);

    QString text = tr("%n row(s)", "", int(result.table.size()));
    if (!result.table_path.isEmpty() && result.table_path != QLatin1String("(root)"))
        text += tr(" from '%1'").arg(result.table_path);
    text += QStringLiteral("  ·  ") + tr("%1 ms").arg(result.duration_ms);
    text += QStringLiteral("  ·  ") +
            (result.from_cache
                 ? tr("cached %1 ago").arg(age_text(QDateTime::currentMSecsSinceEpoch() - result.fetched_at_ms))
                 : tr("fresh"));
    QString state = QStringLiteral("ok");
    if (!result.warning.isEmpty()) {
        text += QStringLiteral("  ·  ") + tr("⚠ upstream: %1").arg(result.warning);
        state = QStringLiteral("warning");
    }
    set_status(text, state);
    set_result_state(ResultState::Results);
}

void ConnectorCatalogPanel::on_error(const QString& request_id, const QString& message) {
    if (request_id != pending_request_)
        return;
    pending_request_.clear();
    set_status(tr("Failed"), QStringLiteral("error"));
    set_result_state(ResultState::Error, message);
}

void ConnectorCatalogPanel::set_result_state(ResultState state, const QString& message) {
    switch (state) {
        case ResultState::Empty:
            result_stack_->setCurrentWidget(empty_label_);
            break;
        case ResultState::Loading:
            loading_label_->setText(message);
            result_stack_->setCurrentWidget(loading_label_);
            break;
        case ResultState::Results:
            result_stack_->setCurrentWidget(tabs_);
            break;
        case ResultState::Error:
            error_label_->setText(tr("ERROR\n\n%1").arg(message));
            result_stack_->setCurrentWidget(error_label_);
            break;
    }
}

void ConnectorCatalogPanel::set_status(const QString& text, const QString& state) {
    status_->setText(text);
    status_->setProperty("state", state);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

QString ConnectorCatalogPanel::age_text(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms / 1000);
    if (s < 60)
        return tr("%1 s").arg(s);
    if (s < 3600)
        return tr("%1 min").arg(s / 60);
    return tr("%1 h").arg(s / 3600);
}

// ─────────────────────────────────────────────────────────────────────────────
// Export / copy
// ─────────────────────────────────────────────────────────────────────────────

void ConnectorCatalogPanel::on_export_csv() {
    if (!has_result_ || result_model_->rowCount() == 0)
        return;
    const QString suggested =
        QDir::homePath() + QLatin1Char('/') + last_result_.connector + QLatin1Char('_') + last_result_.command + ".csv";
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), suggested, tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        set_status(tr("Cannot write %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    const QStringList cols = result_model_->columns();
    QStringList header;
    for (const QString& c : cols)
        header << catalog_csv_field(c);
    file.write(header.join(QLatin1Char(',')).toUtf8() + '\n');
    // Rows in the order the table currently shows them.
    const int rows = result_proxy_->rowCount();
    for (int r = 0; r < rows; ++r) {
        const int src = result_proxy_->mapToSource(result_proxy_->index(r, 0)).row();
        QStringList line;
        line.reserve(cols.size());
        for (int c = 0; c < cols.size(); ++c)
            line << catalog_csv_field(result_model_->cell_text(src, c));
        file.write(line.join(QLatin1Char(',')).toUtf8() + '\n');
    }
    if (!file.commit()) {
        set_status(tr("Cannot write %1: %2").arg(path, file.errorString()), QStringLiteral("error"));
        return;
    }
    set_status(tr("Exported %1 row(s) to %2").arg(rows).arg(QDir::toNativeSeparators(path)), QStringLiteral("ok"));
}

void ConnectorCatalogPanel::on_copy_tool_call() {
    const conn::ConnectorInfo* c = conn::ConnectorCatalog::instance().find(current_name_);
    if (!c)
        return;
    const QJsonObject call{
        {"tool", c->name},
        {"arguments", QJsonObject{{"command", command_->currentData().toString()},
                                  {"args", QJsonArray::fromStringList(current_args())}}},
    };
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(call).toJson(QJsonDocument::Compact)));
    set_status(tr("Copied the %1 tool call to the clipboard.").arg(c->name), QStringLiteral("ok"));
}

} // namespace fincept::screens::datasources
