// DataMappingScreen.cpp — see DataMappingScreen.h.

#include "screens/data_mapping/DataMappingScreen.h"

#include "screens/data_mapping/MappingModels.h"
#include "screens/data_sources/DataWorkspace.h"
#include "services/connectors/ConnectorArgs.h"
#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorService.h"
#include "services/data_mapping/MappingInbox.h"
#include "services/data_mapping/MappingRunner.h"
#include "storage/repositories/DataSourceRepository.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens {

namespace dmap = fincept::services::datamapping;
namespace dsrc = fincept::services::datasources;
namespace conn = fincept::services::connectors;
using datasources::set_look;
using datasources::TabularFilterProxy;
using datasources::TabularTableModel;

namespace {

constexpr int kPreviewRows = 500;
constexpr int kRunRows = 100000;
constexpr int kKindConnection = 0;
constexpr int kKindConnector = 1;
constexpr int kKindFile = 2;
constexpr int kKindHttp = 3;

const char* dms_kind_id(int index) {
    switch (index) {
        case kKindConnection:
            return "connection";
        case kKindConnector:
            return "connector";
        case kKindFile:
            return "file";
        default:
            return "http";
    }
}

int dms_kind_index(const QString& kind) {
    if (kind == QLatin1String("connection"))
        return kKindConnection;
    if (kind == QLatin1String("connector"))
        return kKindConnector;
    if (kind == QLatin1String("file"))
        return kKindFile;
    if (kind == QLatin1String("http"))
        return kKindHttp;
    return kKindConnector;
}

dmap::TargetType dms_target_for(dsrc::ColumnType t) {
    switch (t) {
        case dsrc::ColumnType::Integer:
            return dmap::TargetType::Integer;
        case dsrc::ColumnType::Number:
            return dmap::TargetType::Number;
        case dsrc::ColumnType::Bool:
            return dmap::TargetType::Bool;
        case dsrc::ColumnType::Date:
            return dmap::TargetType::Date;
        case dsrc::ColumnType::DateTime:
            return dmap::TargetType::DateTime;
        case dsrc::ColumnType::Object:
            return dmap::TargetType::Json;
        default:
            return dmap::TargetType::String;
    }
}

QTableView* dms_make_view(QWidget* parent) {
    auto* v = new QTableView(parent);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setAlternatingRowColors(true);
    v->setWordWrap(false);
    v->verticalHeader()->setVisible(false);
    v->horizontalHeader()->setHighlightSections(false);
    fincept::ui::metrics::follow_row_height(v, 26);
    return v;
}

QString dms_default_dir() {
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docs.isEmpty() ? QDir::homePath() : docs;
}

QString dms_file_stem(QString s) {
    static const QRegularExpression kBad(QStringLiteral("[^A-Za-z0-9_]+"));
    s.replace(kBad, QStringLiteral("_"));
    return s.isEmpty() ? QStringLiteral("mapped") : s.left(60);
}

struct DmsChoices {
    QVector<QPair<QString, QString>> connections; ///< id, "name · type"
    QVector<QPair<QString, QString>> connectors;  ///< name, title
};

} // namespace

DataMappingScreen::DataMappingScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dwRoot"));
    setStyleSheet(datasources::workspace_qss()); // once, before any child exists
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    root_stack_ = new QStackedWidget(this);
    auto* sk = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, root_stack_);
    sk->set_message(tr("Opening Data Mapping…"));
    root_stack_->addWidget(sk);
    vl->addWidget(root_stack_);
}

void DataMappingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!built_) {
        built_ = true;
        QPointer<DataMappingScreen> self(this);
        QTimer::singleShot(0, this, [self]() {
            if (!self)
                return;
            self->build_workspace();
            self->reload_list();
            self->load_choices();
            self->take_handoff();
        });
        return;
    }
    take_handoff();
}

// ─────────────────────────────────────────────────────────────────────────────
// Build
// ─────────────────────────────────────────────────────────────────────────────

void DataMappingScreen::build_workspace() {
    auto* ws = new QWidget(root_stack_);
    auto* vl = new QVBoxLayout(ws);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    auto* header = new QWidget(ws);
    set_look(header, "bar");
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 8, 12, 8);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(1);
    auto* title = new QLabel(tr("DATA MAPPING"), header);
    set_look(title, "h1");
    auto* sub = new QLabel(tr("Map the rows of any source onto typed fields — every value cast by an explicit rule, every "
                              "failure shown with its reason"),
                           header);
    set_look(sub, "muted");
    titles->addWidget(title);
    titles->addWidget(sub);
    hl->addLayout(titles, 1);
    vl->addWidget(header);

    splitter_ = new QSplitter(Qt::Horizontal, ws);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(1);
    splitter_->addWidget(build_list_pane(splitter_));
    right_stack_ = new QStackedWidget(splitter_);
    intro_ = new fincept::ui::EmptyState(right_stack_);
    intro_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("Map a source onto typed fields"),
        tr("Pick a mapping on the left, or start one: choose a source (a saved connection, a catalogue connector, a "
           "file), load its rows, match columns to fields, check the preview, SAVE and RUN."),
        tr("+ NEW MAPPING")));
    right_stack_->addWidget(intro_);
    right_stack_->addWidget(build_editor(right_stack_));
    splitter_->addWidget(right_stack_);
    const int em = fontMetrics().horizontalAdvance(QLatin1Char('M'));
    splitter_->setSizes({em * 24, em * 80});
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 4);
    vl->addWidget(splitter_, 1);
    root_stack_->addWidget(ws);
    root_stack_->setCurrentWidget(ws);

    connect(intro_, &fincept::ui::EmptyState::primary_clicked, this, [this]() { new_mapping(nullptr); });
    connect(&dmap::MappingInbox::instance(), &dmap::MappingInbox::posted, this, [this]() {
        if (isVisible())
            take_handoff();
    });
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) {
                list_model_->refresh_colours();
                grid_model_->refresh_colours();
                preview_model_->refresh_colours();
            });
}

QWidget* DataMappingScreen::build_list_pane(QWidget* parent) {
    auto* pane = new QWidget(parent);
    set_look(pane, "pane");
    pane->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(pane);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(tr("MAPPINGS"), pane);
    set_look(title, "title");
    top->addWidget(title);
    top->addStretch(1);
    auto* new_btn = new QPushButton(tr("+ NEW"), pane);
    set_look(new_btn, "primary");
    top->addWidget(new_btn);
    vl->addLayout(top);
    list_stack_ = new QStackedWidget(pane);
    auto* sk = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, list_stack_);
    sk->set_message(tr("Loading mappings…"));
    list_model_ = new datamapping::MappingListModel(this);
    list_view_ = dms_make_view(list_stack_);
    list_view_->setModel(list_model_);
    list_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_view_->horizontalHeader()->setSectionResizeMode(datamapping::MappingListModel::ColName, QHeaderView::Stretch);
    list_view_->horizontalHeader()->setSectionResizeMode(datamapping::MappingListModel::ColSchema,
                                                         QHeaderView::ResizeToContents);
    list_view_->horizontalHeader()->setSectionResizeMode(datamapping::MappingListModel::ColRun,
                                                         QHeaderView::ResizeToContents);
    list_empty_ = new fincept::ui::EmptyState(list_stack_);
    list_empty_->set_compact(true);
    list_empty_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("No mappings yet"), tr("A mapping turns any source's rows into typed fields you can export and reuse."),
        tr("+ NEW MAPPING")));
    list_stack_->addWidget(sk);
    list_stack_->addWidget(list_view_);
    list_stack_->addWidget(list_empty_);
    vl->addWidget(list_stack_, 1);

    connect(new_btn, &QPushButton::clicked, this, [this]() { new_mapping(nullptr); });
    connect(list_empty_, &fincept::ui::EmptyState::primary_clicked, this, [this]() { new_mapping(nullptr); });
    connect(list_view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                if (!cur.isValid())
                    return;
                const QString id = list_model_->data(list_model_->index(cur.row(), 0), datamapping::MappingListModel::IdRole)
                                       .toString();
                if (id != current_id_)
                    open_mapping(id);
            });
    return pane;
}

QWidget* DataMappingScreen::build_editor(QWidget* parent) {
    editor_ = new QWidget(parent);
    auto* vl = new QVBoxLayout(editor_);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* bar = new QWidget(editor_);
    set_look(bar, "bar");
    bar->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 6, 12, 6);
    hl->setSpacing(6);
    name_ = new QLineEdit(bar);
    name_->setPlaceholderText(tr("Mapping name"));
    hl->addWidget(name_, 2);
    auto* schema_lbl = new QLabel(tr("SCHEMA"), bar);
    set_look(schema_lbl, "field");
    hl->addWidget(schema_lbl);
    schema_ = new QComboBox(bar);
    schema_->addItem(tr("CUSTOM (the source's columns)"), QStringLiteral("CUSTOM"));
    for (const dmap::TargetSchema& s : dmap::target_schemas())
        schema_->addItem(s.name, s.name);
    schema_->setToolTip(tr("The fields to map onto. CUSTOM keeps the source's own columns, with the types you choose."));
    hl->addWidget(schema_, 1);
    revision_ = new QComboBox(bar);
    revision_->setToolTip(tr("Every SAVE keeps a revision; pick an older one to bring it back"));
    revision_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    hl->addWidget(revision_);
    save_btn_ = new QPushButton(tr("SAVE"), bar);
    set_look(save_btn_, "primary");
    run_btn_ = new QPushButton(tr("RUN"), bar);
    run_btn_->setToolTip(tr("Map every row of the source, keep the output and publish it on DataHub"));
    export_btn_ = new QToolButton(bar);
    set_look(export_btn_, "button");
    export_btn_->setText(tr("EXPORT"));
    export_btn_->setPopupMode(QToolButton::InstantPopup);
    auto* export_menu = new QMenu(export_btn_);
    export_menu->addAction(tr("CSV…"), this, [this]() { on_export(0); });
    export_menu->addAction(tr("JSON…"), this, [this]() { on_export(1); });
    export_menu->addAction(tr("SQLite table…"), this, [this]() { on_export(2); });
    export_btn_->setMenu(export_menu);
    delete_btn_ = new QPushButton(tr("DELETE"), bar);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the mapping
    set_look(delete_btn_, "danger");
    hl->addWidget(save_btn_);
    hl->addWidget(run_btn_);
    hl->addWidget(export_btn_);
    hl->addWidget(delete_btn_);
    vl->addWidget(bar);

    status_ = new QLabel(editor_);
    set_look(status_, "muted");
    status_->setContentsMargins(12, 4, 12, 4);
    status_->setWordWrap(true);
    vl->addWidget(status_);
    banner_ = new QLabel(editor_);
    set_look(banner_, "banner_info");
    banner_->setWordWrap(true);
    banner_->setTextFormat(Qt::PlainText);
    banner_->hide();
    vl->addWidget(banner_);
    banner_timer_ = new QTimer(this);
    banner_timer_->setSingleShot(true);
    banner_timer_->setInterval(12000);
    connect(banner_timer_, &QTimer::timeout, banner_, &QLabel::hide);

    editor_split_ = new QSplitter(Qt::Vertical, editor_);
    editor_split_->setChildrenCollapsible(false);
    editor_split_->setHandleWidth(1);
    editor_split_->addWidget(build_source_box(editor_split_));
    editor_split_->addWidget(build_grid_box(editor_split_));
    editor_split_->addWidget(build_preview_box(editor_split_));
    editor_split_->setStretchFactor(0, 0);
    editor_split_->setStretchFactor(1, 2);
    editor_split_->setStretchFactor(2, 3);
    vl->addWidget(editor_split_, 1);

    preview_timer_ = new QTimer(this);
    preview_timer_->setSingleShot(true);
    preview_timer_->setInterval(250);
    connect(preview_timer_, &QTimer::timeout, this, [this]() {
        if (!has_source_ || source_table_ < 0 || source_table_ >= source_data_.tables.size())
            return;
        const quint64 gen = ++preview_gen_;
        QPointer<DataMappingScreen> self(this);
        dmap::MappingRunner::instance().apply_async(source_data_.tables[source_table_], spec_, kPreviewRows, this,
                                                    [self, gen](const dmap::MappingOutput& out) {
                                                        if (self && gen == self->preview_gen_)
                                                            self->apply_preview(out, false);
                                                    });
    });

    connect(name_, &QLineEdit::textEdited, this, [this]() { set_dirty(true); });
    connect(schema_, &QComboBox::currentIndexChanged, this, [this](int) { on_schema_changed(); });
    connect(revision_, &QComboBox::activated, this, &DataMappingScreen::on_revision_chosen);
    connect(save_btn_, &QPushButton::clicked, this, [this]() { on_save(false); });
    connect(run_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_run);
    connect(delete_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_delete);
    return editor_;
}

QWidget* DataMappingScreen::build_source_box(QWidget* parent) {
    auto* box = new QWidget(parent);
    set_look(box, "pane");
    box->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(tr("1 · SOURCE"), box);
    set_look(title, "title");
    top->addWidget(title);
    kind_ = new QComboBox(box);
    kind_->addItem(tr("Saved connection"));
    kind_->addItem(tr("Catalogue connector"));
    kind_->addItem(tr("Local file"));
    kind_->addItem(tr("HTTP request"));
    top->addWidget(kind_);
    top->addStretch(1);
    table_pick_ = new QComboBox(box);
    table_pick_->setToolTip(tr("The source returned several tables — pick the one to map"));
    table_pick_->hide();
    top->addWidget(table_pick_);
    load_btn_ = new QPushButton(tr("LOAD SOURCE"), box);
    set_look(load_btn_, "primary");
    load_btn_->setToolTip(tr("Fetch the first %1 rows to build and check the mapping").arg(kPreviewRows));
    top->addWidget(load_btn_);
    vl->addLayout(top);

    kind_pages_ = new QStackedWidget(box);
    // Saved connection
    auto* p_conn = new QWidget(kind_pages_);
    auto* g1 = new QHBoxLayout(p_conn);
    g1->setContentsMargins(0, 0, 0, 0);
    conn_pick_ = new QComboBox(p_conn);
    conn_pick_->setMinimumContentsLength(18);
    conn_pick_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    conn_entity_ = new QLineEdit(p_conn);
    conn_entity_->setPlaceholderText(tr("table / path / key (blank = default sample)"));
    conn_query_ = new QLineEdit(p_conn);
    conn_query_->setPlaceholderText(tr("query (SQL, PromQL, … — optional)"));
    g1->addWidget(conn_pick_, 2);
    g1->addWidget(conn_entity_, 2);
    g1->addWidget(conn_query_, 3);
    kind_pages_->addWidget(p_conn);
    // Catalogue connector
    auto* p_cat = new QWidget(kind_pages_);
    auto* g2 = new QHBoxLayout(p_cat);
    g2->setContentsMargins(0, 0, 0, 0);
    connector_pick_ = new QComboBox(p_cat);
    connector_pick_->setEditable(true);
    connector_pick_->setInsertPolicy(QComboBox::NoInsert);
    connector_pick_->setMinimumContentsLength(18);
    connector_pick_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    connector_pick_->completer()->setFilterMode(Qt::MatchContains);
    connector_pick_->completer()->setCompletionMode(QCompleter::PopupCompletion);
    command_pick_ = new QComboBox(p_cat);
    command_pick_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    args_edit_ = new QLineEdit(p_cat);
    g2->addWidget(connector_pick_, 3);
    g2->addWidget(command_pick_, 1);
    g2->addWidget(args_edit_, 3);
    kind_pages_->addWidget(p_cat);
    // Local file
    auto* p_file = new QWidget(kind_pages_);
    auto* g3 = new QHBoxLayout(p_file);
    g3->setContentsMargins(0, 0, 0, 0);
    file_path_ = new QLineEdit(p_file);
    file_path_->setPlaceholderText(tr("CSV, TSV, JSON, NDJSON, XML, Excel, Parquet or Arrow / Feather file"));
    auto* browse = new QPushButton(tr("BROWSE…"), p_file);
    file_detail_ = new QComboBox(p_file);
    file_detail_->setEditable(true);
    file_detail_->setInsertPolicy(QComboBox::NoInsert);
    file_detail_->lineEdit()->setPlaceholderText(tr("sheet / rows path (optional)"));
    file_detail_->setToolTip(tr("Excel: the worksheet (listed once the file is chosen). JSON / XML: where the rows are, "
                                "e.g. data.items"));
    g3->addWidget(file_path_, 4);
    g3->addWidget(browse);
    g3->addWidget(file_detail_, 2);
    kind_pages_->addWidget(p_file);
    // HTTP request
    auto* p_http = new QWidget(kind_pages_);
    auto* g4 = new QGridLayout(p_http);
    g4->setContentsMargins(0, 0, 0, 0);
    g4->setHorizontalSpacing(6);
    g4->setVerticalSpacing(6);
    http_method_ = new QComboBox(p_http);
    http_method_->addItems({QStringLiteral("GET"), QStringLiteral("POST")});
    http_base_ = new QLineEdit(p_http);
    http_base_->setPlaceholderText(tr("https://api.example.com"));
    http_endpoint_ = new QLineEdit(p_http);
    http_endpoint_->setPlaceholderText(tr("/v1/prices?symbol=AAPL"));
    http_auth_type_ = new QComboBox(p_http);
    http_auth_type_->addItems({QStringLiteral("None"), QStringLiteral("API Key"), QStringLiteral("Bearer Token"),
                               QStringLiteral("Basic Auth")});
    http_auth_value_ = new QLineEdit(p_http);
    http_auth_value_->setEchoMode(QLineEdit::Password);
    http_auth_value_->setPlaceholderText(tr("auth value — kept encrypted on this computer"));
    http_headers_ = new QLineEdit(p_http);
    http_headers_->setPlaceholderText(tr("extra headers: Name: value; Name2: value2 ({AUTH} = the auth value)"));
    http_rows_path_ = new QLineEdit(p_http);
    http_rows_path_->setPlaceholderText(tr("rows path, e.g. data.items (blank = detect)"));
    g4->addWidget(http_method_, 0, 0);
    g4->addWidget(http_base_, 0, 1);
    g4->addWidget(http_endpoint_, 0, 2);
    g4->addWidget(http_auth_type_, 1, 0);
    g4->addWidget(http_auth_value_, 1, 1);
    g4->addWidget(http_rows_path_, 1, 2);
    g4->addWidget(http_headers_, 2, 0, 1, 3);
    g4->setColumnStretch(1, 2);
    g4->setColumnStretch(2, 2);
    kind_pages_->addWidget(p_http);
    vl->addWidget(kind_pages_);
    source_info_ = new QLabel(box);
    set_look(source_info_, "note");
    source_info_->setWordWrap(true);
    vl->addWidget(source_info_);

    auto dirty = [this]() {
        if (!loading_ui_)
            set_dirty(true);
    };
    connect(kind_, &QComboBox::currentIndexChanged, this, [this, dirty](int) {
        update_kind_page();
        dirty();
    });
    connect(conn_pick_, &QComboBox::activated, this, dirty);
    connect(conn_entity_, &QLineEdit::textEdited, this, dirty);
    connect(conn_query_, &QLineEdit::textEdited, this, dirty);
    connect(connector_pick_, &QComboBox::currentIndexChanged, this, [this, dirty](int) {
        update_connector_commands();
        dirty();
    });
    connect(command_pick_, &QComboBox::activated, this, [this, dirty](int) {
        update_connector_commands();
        dirty();
    });
    connect(args_edit_, &QLineEdit::textEdited, this, dirty);
    connect(file_path_, &QLineEdit::textEdited, this, dirty);
    connect(file_path_, &QLineEdit::editingFinished, this, &DataMappingScreen::refresh_file_sheets);
    connect(file_detail_, &QComboBox::editTextChanged, this, dirty);
    for (QLineEdit* e : {http_base_, http_endpoint_, http_auth_value_, http_headers_, http_rows_path_})
        connect(e, &QLineEdit::textEdited, this, dirty);
    connect(http_method_, &QComboBox::activated, this, dirty);
    connect(http_auth_type_, &QComboBox::activated, this, dirty);
    connect(browse, &QPushButton::clicked, this, [this, dirty]() {
        const QString picked = QFileDialog::getOpenFileName(
            this, tr("Select a data file"), dms_default_dir(),
            tr("Data files (*.csv *.tsv *.txt *.json *.jsonl *.ndjson *.xml *.xlsx *.xlsm *.xls *.parquet *.feather "
               "*.arrow);;All files (*)"));
        if (!picked.isEmpty()) {
            file_path_->setText(QDir::toNativeSeparators(picked));
            dirty();
            refresh_file_sheets();
        }
    });
    connect(load_btn_, &QPushButton::clicked, this, &DataMappingScreen::on_load_source);
    connect(table_pick_, &QComboBox::activated, this, [this](int i) {
        show_source_table(table_pick_->itemData(i).toInt());
        set_dirty(true);
    });
    return box;
}

QWidget* DataMappingScreen::build_grid_box(QWidget* parent) {
    auto* box = new QWidget(parent);
    set_look(box, "pane");
    box->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    auto* title = new QLabel(tr("2 · MAPPING"), box);
    set_look(title, "title");
    top->addWidget(title);
    top->addStretch(1);
    auto_btn_ = new QPushButton(tr("AUTO-MATCH"), box);
    auto_btn_->setToolTip(tr("Match every field to the most likely source column (date ↔ timestamp, close ↔ price, …)"));
    add_field_btn_ = new QPushButton(tr("+ FIELD"), box);
    remove_field_btn_ = new QPushButton(tr("REMOVE FIELD"), box);
    top->addWidget(auto_btn_);
    top->addWidget(add_field_btn_);
    top->addWidget(remove_field_btn_);
    vl->addLayout(top);
    auto* opts = new QHBoxLayout;
    opts->setSpacing(6);
    auto* dec_lbl = new QLabel(tr("NUMBERS"), box);
    set_look(dec_lbl, "field");
    decimal_ = new QComboBox(box);
    decimal_->addItem(tr("auto-detect 1,234.5 / 1.234,5"), QStringLiteral("auto"));
    decimal_->addItem(tr("1,234.5 (dot decimals)"), QStringLiteral("dot"));
    decimal_->addItem(tr("1.234,5 (comma decimals)"), QStringLiteral("comma"));
    auto* dup_lbl = new QLabel(tr("DUPLICATES"), box);
    set_look(dup_lbl, "field");
    duplicates_ = new QComboBox(box);
    duplicates_->addItem(tr("keep"), QStringLiteral("keep"));
    duplicates_->addItem(tr("drop"), QStringLiteral("drop"));
    duplicates_->addItem(tr("flag"), QStringLiteral("flag"));
    key_fields_ = new QLineEdit(box);
    key_fields_->setPlaceholderText(tr("key fields, e.g. symbol, date"));
    drop_invalid_ = new QCheckBox(tr("Leave out rows with a failed cell"), box);
    opts->addWidget(dec_lbl);
    opts->addWidget(decimal_);
    opts->addWidget(dup_lbl);
    opts->addWidget(duplicates_);
    opts->addWidget(key_fields_, 1);
    opts->addWidget(drop_invalid_);
    vl->addLayout(opts);
    grid_stack_ = new QStackedWidget(box);
    grid_model_ = new datamapping::FieldGridModel(this);
    grid_view_ = dms_make_view(grid_stack_);
    grid_view_->setModel(grid_model_);
    grid_view_->setItemDelegate(new datamapping::FieldGridDelegate(grid_model_, grid_view_));
    grid_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    grid_view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked |
                                QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    grid_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    grid_view_->horizontalHeader()->setStretchLastSection(true);
    const int em = fontMetrics().horizontalAdvance(QLatin1Char('M'));
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColTarget, em * 10);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColType, em * 6);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColSource, em * 10);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColFormat, em * 7);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColScale, em * 4);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColDefault, em * 6);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColRequired, em * 3);
    grid_view_->setColumnWidth(datamapping::FieldGridModel::ColSample, em * 10);
    grid_empty_ = new fincept::ui::EmptyState(grid_stack_);
    grid_empty_->set_compact(true);
    grid_empty_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("No fields yet"), tr("LOAD SOURCE fills the fields from its columns (CUSTOM) or from the chosen schema."),
        tr("+ FIELD")));
    grid_stack_->addWidget(grid_view_);
    grid_stack_->addWidget(grid_empty_);
    vl->addWidget(grid_stack_, 1);

    connect(grid_model_, &datamapping::FieldGridModel::fields_edited, this, [this]() {
        spec_.fields = grid_model_->fields();
        grid_stack_->setCurrentWidget(spec_.fields.isEmpty() ? static_cast<QWidget*>(grid_empty_)
                                                             : static_cast<QWidget*>(grid_view_));
        if (!loading_ui_) {
            set_dirty(true);
            schedule_preview();
        }
    });
    connect(auto_btn_, &QPushButton::clicked, this, [this]() {
        if (!has_source_) {
            show_banner(tr("LOAD SOURCE first: matching needs the source's columns."), "banner_info");
            return;
        }
        QVector<dmap::FieldMapping> fields = grid_model_->fields();
        const QStringList cols = grid_model_->source_columns();
        for (auto& f : fields) {
            const QString s = dmap::suggest_source_column(f.target, cols);
            if (!s.isEmpty())
                f.source = s;
        }
        grid_model_->set_fields(fields, spec_.target_schema == QLatin1String("CUSTOM"));
        spec_.fields = fields;
        set_dirty(true);
        schedule_preview();
    });
    auto add_field = [this]() {
        dmap::FieldMapping f;
        int n = grid_model_->rowCount() + 1;
        f.target = QStringLiteral("field_%1").arg(n);
        while (spec_.index_of(f.target) >= 0)
            f.target = QStringLiteral("field_%1").arg(++n);
        grid_model_->add_field(f);
        const QModelIndex idx = grid_model_->index(grid_model_->rowCount() - 1, datamapping::FieldGridModel::ColTarget);
        grid_view_->setCurrentIndex(idx);
        grid_view_->edit(idx);
    };
    connect(add_field_btn_, &QPushButton::clicked, this, add_field);
    connect(grid_empty_, &fincept::ui::EmptyState::primary_clicked, this, add_field);
    connect(remove_field_btn_, &QPushButton::clicked, this, [this]() {
        const QModelIndex idx = grid_view_->currentIndex();
        if (idx.isValid())
            grid_model_->remove_field(idx.row());
    });
    auto opt_changed = [this]() {
        if (loading_ui_)
            return;
        read_options_from_ui();
        set_dirty(true);
        schedule_preview();
    };
    connect(decimal_, &QComboBox::activated, this, opt_changed);
    connect(duplicates_, &QComboBox::activated, this, opt_changed);
    connect(key_fields_, &QLineEdit::editingFinished, this, opt_changed);
    connect(drop_invalid_, &QCheckBox::toggled, this, opt_changed);
    return box;
}

QWidget* DataMappingScreen::build_preview_box(QWidget* parent) {
    auto* box = new QWidget(parent);
    set_look(box, "pane");
    box->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(box);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(tr("3 · PREVIEW"), box);
    set_look(title, "title");
    top->addWidget(title);
    top->addStretch(1);
    problems_only_ = new QCheckBox(tr("Only rows with problems"), box);
    top->addWidget(problems_only_);
    vl->addLayout(top);
    summary_ = new QLabel(box);
    summary_->setWordWrap(true);
    summary_->setTextFormat(Qt::PlainText);
    set_look(summary_, "muted");
    vl->addWidget(summary_);
    preview_stack_ = new QStackedWidget(box);
    preview_empty_ = new fincept::ui::EmptyState(preview_stack_);
    preview_skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, preview_stack_);
    preview_model_ = new TabularTableModel(this);
    preview_proxy_ = new TabularFilterProxy(this);
    preview_proxy_->setSourceModel(preview_model_);
    preview_view_ = dms_make_view(preview_stack_);
    preview_view_->setModel(preview_proxy_);
    preview_view_->setSortingEnabled(true);
    preview_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    preview_view_->horizontalHeader()->setStretchLastSection(true);
    preview_view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    preview_stack_->addWidget(preview_empty_);
    preview_stack_->addWidget(preview_skeleton_);
    preview_stack_->addWidget(preview_view_);
    vl->addWidget(preview_stack_, 1);
    connect(problems_only_, &QCheckBox::toggled, preview_proxy_, &TabularFilterProxy::set_marked_only);
    connect(preview_empty_, &fincept::ui::EmptyState::primary_clicked, this, &DataMappingScreen::on_load_source);
    return box;
}

// ─────────────────────────────────────────────────────────────────────────────
// List / choices
// ─────────────────────────────────────────────────────────────────────────────

void DataMappingScreen::reload_list(const QString& select_id) {
    QPointer<DataMappingScreen> self(this);
    (void)QtConcurrent::run([self, select_id]() {
        auto r = fincept::DataMappingRepository::instance().list_all();
        QVector<fincept::DataMapping> rows = r.is_ok() ? r.value() : QVector<fincept::DataMapping>();
        const QString err = r.is_err() ? QString::fromStdString(r.error()) : QString();
        QMetaObject::invokeMethod(
            qApp,
            [self, rows, err, select_id]() {
                if (!self)
                    return;
                {
                    const QSignalBlocker block(self->list_view_->selectionModel());
                    self->list_model_->set_mappings(rows);
                }
                self->list_stack_->setCurrentWidget(rows.isEmpty() ? static_cast<QWidget*>(self->list_empty_)
                                                                   : static_cast<QWidget*>(self->list_view_));
                if (!err.isEmpty())
                    self->show_banner(tr("Could not read the mappings: %1").arg(err), "banner_error");
                QString want = select_id.isEmpty() ? self->current_id_ : select_id;
                if (want.isEmpty() && self->pending_restore_.contains("mapping"))
                    want = self->pending_restore_.value("mapping").toString();
                if (self->pending_restore_.contains("splitter"))
                    self->splitter_->restoreState(QByteArray::fromBase64(self->pending_restore_.value("splitter").toByteArray()));
                if (self->pending_restore_.contains("editor_split"))
                    self->editor_split_->restoreState(
                        QByteArray::fromBase64(self->pending_restore_.value("editor_split").toByteArray()));
                self->pending_restore_.clear();
                const int row = self->list_model_->row_of(want);
                if (row >= 0) {
                    const QSignalBlocker block(self->list_view_->selectionModel());
                    self->list_view_->setCurrentIndex(self->list_model_->index(row, 0));
                    if (want != self->current_id_)
                        self->open_mapping(want);
                    else if (const fincept::DataMapping* m = self->list_model_->find(want))
                        self->current_ = *m;
                }
                self->update_header();
                self->list_loaded_ = true;
                if (self->has_pending_link_) {
                    // A deep link that arrived before the list: after the restore, so it wins.
                    self->has_pending_link_ = false;
                    self->open_section(self->pending_link_section_, self->pending_link_focus_);
                }
            },
            Qt::QueuedConnection);
    });
}

void DataMappingScreen::load_choices() {
    QPointer<DataMappingScreen> self(this);
    (void)QtConcurrent::run([self]() {
        DmsChoices c;
        const auto r = fincept::DataSourceRepository::instance().list_all();
        if (r.is_ok())
            for (const auto& ds : r.value())
                c.connections.append({ds.id, QStringLiteral("%1 · %2").arg(ds.display_name.isEmpty() ? ds.alias : ds.display_name,
                                                                             ds.provider)});
        auto& catalog = conn::ConnectorCatalog::instance();
        if (catalog.ensure_loaded())
            for (const auto& info : catalog.connectors())
                c.connectors.append({info.name, QStringLiteral("%1 (%2)").arg(info.title, info.name)});
        QMetaObject::invokeMethod(
            qApp,
            [self, c]() {
                if (!self)
                    return;
                self->loading_ui_ = true;
                const QString conn_cur = self->conn_pick_->currentData().toString();
                const QString cat_cur = self->connector_pick_->currentData().toString();
                self->conn_pick_->clear();
                for (const auto& p : c.connections)
                    self->conn_pick_->addItem(p.second, p.first);
                if (c.connections.isEmpty())
                    self->conn_pick_->setPlaceholderText(tr("no saved connections — add one in Data Sources"));
                self->connector_pick_->clear();
                for (const auto& p : c.connectors)
                    self->connector_pick_->addItem(p.second, p.first);
                // Re-select what the editor shows (it may have been set before the lists arrived);
                // nothing else in the editor is touched (a name or path being typed stays).
                const dmap::SourceRef& s = self->spec_.source;
                const QString want_conn = s.kind == QLatin1String("connection") ? s.ref : conn_cur;
                const QString want_cat = s.kind == QLatin1String("connector") ? s.ref : cat_cur;
                if (!want_conn.isEmpty()) {
                    int i = self->conn_pick_->findData(want_conn);
                    if (i < 0) {
                        self->conn_pick_->addItem(tr("%1 (deleted?)").arg(want_conn), want_conn);
                        i = self->conn_pick_->count() - 1;
                    }
                    self->conn_pick_->setCurrentIndex(i);
                }
                if (!want_cat.isEmpty()) {
                    int i = self->connector_pick_->findData(want_cat);
                    if (i < 0) {
                        self->connector_pick_->addItem(want_cat, want_cat);
                        i = self->connector_pick_->count() - 1;
                    }
                    self->connector_pick_->setCurrentIndex(i);
                }
                self->update_connector_commands();
                if (s.kind == QLatin1String("connector"))
                    self->command_pick_->setCurrentIndex(std::max(0, self->command_pick_->findData(s.entity)));
                self->loading_ui_ = false;
            },
            Qt::QueuedConnection);
    });
}

bool DataMappingScreen::confirm_discard() {
    if (!dirty_ || right_stack_->currentWidget() != editor_)
        return true;
    return QMessageBox::question(this, tr("Unsaved changes"),
                                 tr("The mapping on screen has unsaved changes. Leave it without saving?"),
                                 QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
}

void DataMappingScreen::open_mapping(const QString& id) {
    const fincept::DataMapping* m = list_model_->find(id);
    if (!m)
        return;
    if (!confirm_discard()) {
        const int row = list_model_->row_of(current_id_);
        const QSignalBlocker block(list_view_->selectionModel());
        if (row >= 0)
            list_view_->setCurrentIndex(list_model_->index(row, 0));
        else
            list_view_->clearSelection();
        return;
    }
    ++gen_;
    if (load_job_)
        dmap::MappingRunner::instance().cancel(load_job_);
    busy_ = false;
    current_ = *m;
    current_id_ = m->id;
    auth_unreadable_ = m->auth_unreadable;
    auth_error_ = m->auth_read_error;
    if (m->spec_version >= 2 && !m->spec_json.isEmpty()) {
        spec_ = dmap::MappingSpec::from_json(QJsonDocument::fromJson(m->spec_json.toUtf8()).object());
    } else {
        dmap::LegacyMapping legacy;
        legacy.schema_name = m->schema_name;
        legacy.base_url = m->base_url;
        legacy.endpoint = m->endpoint;
        legacy.method = m->method;
        legacy.auth_type = m->auth_type;
        legacy.headers = m->headers;
        legacy.body = m->body;
        legacy.parser = m->parser;
        legacy.field_mappings = QJsonDocument::fromJson(m->field_mappings_json.toUtf8()).array();
        spec_ = dmap::spec_from_legacy(legacy);
    }
    has_source_ = false;
    source_data_ = dsrc::TabularResult();
    has_preview_ = false;
    preview_is_run_ = false;
    apply_spec_to_ui();
    http_auth_value_->setText(m->auth_token);
    http_auth_value_->setPlaceholderText(auth_unreadable_ ? tr("could not read the saved value — type it again to replace it")
                                                          : tr("auth value — kept encrypted on this computer"));
    source_info_->setText(tr("Source not loaded yet — LOAD SOURCE fetches its first %1 rows.").arg(kPreviewRows));
    set_dirty(false);
    right_stack_->setCurrentWidget(editor_);
    banner_->hide();
    if (m->spec_version < 2)
        show_banner(tr("Made before mappings had versions: converted to the new format without losing anything. SAVE to "
                       "keep it in the new format."),
                    "banner_info");
    if (auth_unreadable_)
        show_banner(tr("Could not read secure storage: %1. The saved auth value is kept; type it again only to replace "
                       "it.")
                        .arg(auth_error_),
                    "banner_error");

    // Revisions and the last stored output arrive off the GUI thread; until then the preview
    // says what to do instead of staying blank.
    show_preview_state(tr("Load the source"), tr("LOAD SOURCE fetches the first rows and checks every field."),
                       tr("LOAD SOURCE"));
    revision_->clear();
    revision_->addItem(m->spec_version >= 2 ? tr("rev %1").arg(m->revision) : tr("not versioned"), m->revision);
    QPointer<DataMappingScreen> self(this);
    const quint64 gen = gen_;
    (void)QtConcurrent::run([self, gen, id]() {
        auto& repo = fincept::DataMappingRepository::instance();
        const auto revs = repo.list_revisions(id);
        const auto out = repo.get_output(id);
        dsrc::DataTable last;
        QString produced;
        int errors = 0;
        if (out) {
            last.columns.clear();
            for (const QJsonValue& v : QJsonDocument::fromJson(out->columns_json.toUtf8()).array())
                last.columns << v.toString();
            for (const QJsonValue& v : QJsonDocument::fromJson(out->types_json.toUtf8()).array())
                last.types << dsrc::column_type_from_name(v.toString());
            for (const QJsonValue& v : QJsonDocument::fromJson(out->rows_json.toUtf8()).array())
                last.rows << v.toArray();
            produced = out->produced_at;
            errors = out->error_cells;
        }
        const QVector<fincept::DataMappingRevision> rev_list = revs.is_ok() ? revs.value() : QVector<fincept::DataMappingRevision>();
        const bool has_out = out.has_value();
        QMetaObject::invokeMethod(
            qApp,
            [self, gen, rev_list, last, produced, errors, has_out]() {
                if (!self || gen != self->gen_)
                    return;
                self->revision_->clear();
                for (const auto& r : rev_list)
                    self->revision_->addItem(r.revision == self->current_.revision
                                                 ? tr("rev %1 (current) · %2").arg(r.revision).arg(
                                                       datasources::relative_time_text(r.saved_at))
                                                 : tr("rev %1 · %2").arg(r.revision).arg(datasources::relative_time_text(r.saved_at)),
                                             r.revision);
                for (int i = 0; i < rev_list.size(); ++i)
                    self->revision_->setItemData(i, rev_list[i].spec_json, Qt::UserRole + 1);
                if (rev_list.isEmpty())
                    self->revision_->addItem(tr("not versioned"), self->current_.revision);
                const int cur = self->revision_->findData(self->current_.revision);
                self->revision_->setCurrentIndex(std::max(0, cur));
                if (has_out && !self->has_preview_ && !last.columns.isEmpty()) {
                    dmap::MappingOutput o;
                    o.table = last;
                    o.rows_in = o.rows_out = last.row_count();
                    o.error_cells = errors;
                    self->preview_ = o;
                    self->has_preview_ = true;
                    self->preview_is_run_ = true;
                    self->preview_model_->set_table(last);
                    self->preview_stack_->setCurrentWidget(self->preview_view_);
                    self->summary_->setText(tr("Output of the last RUN (%1): %2 rows, %3 cell problem(s). LOAD SOURCE to "
                                               "check the mapping against fresh rows.")
                                                .arg(datasources::relative_time_text(produced))
                                                .arg(last.row_count())
                                                .arg(errors));
                    set_look(self->summary_, errors > 0 ? "warn" : "muted");
                }
            },
            Qt::QueuedConnection);
    });
}

void DataMappingScreen::new_mapping(const dmap::MappingHandoff* from) {
    if (!confirm_discard())
        return;
    ++gen_;
    if (load_job_)
        dmap::MappingRunner::instance().cancel(load_job_);
    busy_ = false;
    current_ = fincept::DataMapping();
    current_.name = from && !from->label.isEmpty() ? from->label : tr("New mapping");
    current_id_.clear();
    auth_unreadable_ = false;
    auth_error_.clear();
    spec_ = dmap::MappingSpec();
    spec_.target_schema = QStringLiteral("CUSTOM");
    if (from) {
        spec_.source = from->source;
    } else {
        spec_.source.kind = conn_pick_->count() > 0 ? QStringLiteral("connection") : QStringLiteral("connector");
    }
    has_source_ = false;
    source_data_ = dsrc::TabularResult();
    has_preview_ = false;
    preview_is_run_ = false;
    {
        const QSignalBlocker block(list_view_->selectionModel());
        list_view_->clearSelection();
    }
    apply_spec_to_ui();
    http_auth_value_->clear();
    revision_->clear();
    revision_->addItem(tr("new"), 0);
    right_stack_->setCurrentWidget(editor_);
    banner_->hide();
    if (from && !from->sample.is_empty()) {
        set_source_data(from->sample, {}, from->label);
        auto_fields(true);
        show_banner(tr("From Data Sources: %1. Keep CUSTOM or pick a schema, check each field's type and format, then "
                       "SAVE.")
                        .arg(from->label),
                    "banner_info");
    } else {
        source_info_->setText(tr("Choose a source, then LOAD SOURCE."));
        show_preview_state(tr("Load the source"), tr("Choose the source above, then LOAD SOURCE: the grid fills from its "
                                                     "columns and every value is checked here."),
                           tr("LOAD SOURCE"));
    }
    set_dirty(true);
    name_->setFocus();
    name_->selectAll();
}

void DataMappingScreen::take_handoff() {
    if (!built_ || !editor_ || !dmap::MappingInbox::instance().has_pending())
        return;
    const auto h = dmap::MappingInbox::instance().take();
    if (h)
        new_mapping(&*h);
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor <-> spec
// ─────────────────────────────────────────────────────────────────────────────

void DataMappingScreen::apply_spec_to_ui() {
    if (!editor_)
        return;
    loading_ui_ = true;
    name_->setText(current_.name);
    const QString schema = spec_.target_schema.isEmpty() ? QStringLiteral("CUSTOM") : spec_.target_schema;
    schema_->setCurrentIndex(std::max(0, schema_->findData(schema)));
    const dmap::SourceRef& s = spec_.source;
    kind_->setCurrentIndex(dms_kind_index(s.kind));
    update_kind_page();
    if (s.kind == QLatin1String("connection")) {
        int i = conn_pick_->findData(s.ref);
        if (i < 0 && !s.ref.isEmpty()) {
            conn_pick_->addItem(tr("%1 (not loaded)").arg(s.ref), s.ref);
            i = conn_pick_->count() - 1;
        }
        conn_pick_->setCurrentIndex(i);
        conn_entity_->setText(s.entity);
        conn_query_->setText(s.params.value(QStringLiteral("query")).toString());
    } else if (s.kind == QLatin1String("connector")) {
        int i = connector_pick_->findData(s.ref);
        if (i < 0 && !s.ref.isEmpty()) {
            connector_pick_->addItem(s.ref, s.ref);
            i = connector_pick_->count() - 1;
        }
        connector_pick_->setCurrentIndex(i);
        update_connector_commands();
        command_pick_->setCurrentIndex(std::max(0, command_pick_->findData(s.entity)));
        args_edit_->setText(conn::join_args_text(s.args));
    } else if (s.kind == QLatin1String("file")) {
        file_path_->setText(QDir::toNativeSeparators(s.ref));
        file_detail_->setEditText(!s.entity.isEmpty() ? s.entity : s.params.value(QStringLiteral("rows_path")).toString());
    } else if (s.kind == QLatin1String("http")) {
        const QJsonObject& h = s.http;
        http_base_->setText(h.value(QStringLiteral("base_url")).toString());
        http_endpoint_->setText(h.value(QStringLiteral("endpoint")).toString());
        http_method_->setCurrentText(h.value(QStringLiteral("method")).toString(QStringLiteral("GET")));
        const int ai = http_auth_type_->findText(h.value(QStringLiteral("auth_type")).toString(), Qt::MatchFixedString);
        http_auth_type_->setCurrentIndex(std::max(0, ai));
        http_headers_->setText(h.value(QStringLiteral("headers")).toString());
        http_rows_path_->setText(h.value(QStringLiteral("rows_path")).toString());
    }
    decimal_->setCurrentIndex(std::max(0, decimal_->findData(spec_.options.decimal)));
    duplicates_->setCurrentIndex(std::max(0, duplicates_->findData(spec_.options.duplicates)));
    key_fields_->setText(spec_.options.key_fields.join(QStringLiteral(", ")));
    drop_invalid_->setChecked(spec_.options.drop_invalid_rows);
    const bool custom = schema == QLatin1String("CUSTOM");
    grid_model_->set_fields(spec_.fields, custom);
    add_field_btn_->setEnabled(custom);
    remove_field_btn_->setEnabled(custom);
    grid_stack_->setCurrentWidget(spec_.fields.isEmpty() ? static_cast<QWidget*>(grid_empty_)
                                                         : static_cast<QWidget*>(grid_view_));
    grid_empty_->set_primary_action(custom ? tr("+ FIELD") : QString());
    loading_ui_ = false;
}

void DataMappingScreen::read_source_from_ui() {
    dmap::SourceRef s;
    s.kind = QString::fromLatin1(dms_kind_id(kind_->currentIndex()));
    s.table = spec_.source.table;
    switch (kind_->currentIndex()) {
        case kKindConnection:
            s.ref = conn_pick_->currentData().toString();
            s.entity = conn_entity_->text().trimmed();
            if (!conn_query_->text().trimmed().isEmpty())
                s.params.insert(QStringLiteral("query"), conn_query_->text().trimmed());
            break;
        case kKindConnector:
            s.ref = connector_pick_->currentData().toString();
            if (s.ref.isEmpty()) // typed into the editable box without picking
                s.ref = connector_pick_->currentText().section(QLatin1Char('('), -1).remove(QLatin1Char(')')).trimmed();
            s.entity = command_pick_->currentData().toString();
            s.args = conn::split_args_text(args_edit_->text());
            break;
        case kKindFile:
            s.ref = QDir::fromNativeSeparators(file_path_->text().trimmed());
            if (s.ref.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) ||
                s.ref.endsWith(QLatin1String(".xlsm"), Qt::CaseInsensitive) ||
                s.ref.endsWith(QLatin1String(".xls"), Qt::CaseInsensitive))
                s.entity = file_detail_->currentText().trimmed();
            else if (!file_detail_->currentText().trimmed().isEmpty())
                s.params.insert(QStringLiteral("rows_path"), file_detail_->currentText().trimmed());
            break;
        default: {
            QJsonObject h = spec_.source.http; // keeps body / document_mode of converted mappings
            h.insert(QStringLiteral("base_url"), http_base_->text().trimmed());
            h.insert(QStringLiteral("endpoint"), http_endpoint_->text().trimmed());
            h.insert(QStringLiteral("method"), http_method_->currentText());
            h.insert(QStringLiteral("auth_type"), http_auth_type_->currentText());
            h.insert(QStringLiteral("headers"), http_headers_->text().trimmed());
            h.insert(QStringLiteral("rows_path"), http_rows_path_->text().trimmed());
            if (!http_rows_path_->text().trimmed().isEmpty())
                h.insert(QStringLiteral("document_mode"), false);
            s.http = h;
            s.ref = http_base_->text().trimmed();
            break;
        }
    }
    spec_.source = s;
}

void DataMappingScreen::read_options_from_ui() {
    spec_.options.decimal = decimal_->currentData().toString();
    spec_.options.duplicates = duplicates_->currentData().toString();
    spec_.options.key_fields.clear();
    for (const QString& k : key_fields_->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
        if (!k.trimmed().isEmpty())
            spec_.options.key_fields << k.trimmed();
    spec_.options.drop_invalid_rows = drop_invalid_->isChecked();
}

void DataMappingScreen::set_dirty(bool dirty) {
    dirty_ = dirty;
    update_header();
}

void DataMappingScreen::update_header() {
    if (!editor_)
        return;
    QStringList parts;
    if (current_id_.isEmpty()) {
        parts << tr("New mapping — not saved");
    } else {
        parts << (current_.spec_version >= 2 ? tr("Revision %1").arg(current_.revision) : tr("Not versioned yet"));
        if (!current_.updated_at.isEmpty())
            parts << tr("saved %1").arg(datasources::relative_time_text(current_.updated_at));
        if (!current_.last_run_at.isEmpty())
            parts << tr("last run %1: %2 rows, %3 cell problem(s)")
                         .arg(datasources::relative_time_text(current_.last_run_at))
                         .arg(current_.last_run_rows)
                         .arg(current_.last_run_errors);
        parts << tr("DataHub topic %1").arg(dmap::MappingRunner::topic_for(current_id_));
    }
    if (dirty_)
        parts << tr("unsaved changes");
    status_->setText(parts.join(QStringLiteral(" · ")));
    set_look(status_, dirty_ ? "warn" : "muted");
    save_btn_->setEnabled(!busy_);
    run_btn_->setEnabled(!busy_);
    run_btn_->setText(current_id_.isEmpty() || dirty_ ? tr("SAVE + RUN") : tr("RUN"));
    delete_btn_->setEnabled(!current_id_.isEmpty() && !busy_);
    load_btn_->setEnabled(!busy_);
}

void DataMappingScreen::update_kind_page() {
    kind_pages_->setCurrentIndex(kind_->currentIndex());
}

void DataMappingScreen::update_connector_commands() {
    const QString name = connector_pick_->currentData().toString();
    const conn::ConnectorInfo* c = conn::ConnectorCatalog::instance().find(name);
    const QString keep = command_pick_->currentData().toString();
    {
        const QSignalBlocker block(command_pick_);
        command_pick_->clear();
        if (c)
            for (const QString& cmd : c->command_names())
                command_pick_->addItem(cmd, cmd);
        int i = command_pick_->findData(keep);
        if (i < 0 && c)
            i = command_pick_->findData(c->default_command);
        command_pick_->setCurrentIndex(std::max(0, i));
    }
    const QString hint = c ? c->args_hint(command_pick_->currentData().toString()) : QString();
    args_edit_->setPlaceholderText(hint.isEmpty() ? tr("arguments (none needed)") : tr("arguments: %1").arg(hint));
}

void DataMappingScreen::refresh_file_sheets() {
    const QString path = file_path_->text().trimmed();
    const bool excel = path.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive) ||
                       path.endsWith(QLatin1String(".xlsm"), Qt::CaseInsensitive) ||
                       path.endsWith(QLatin1String(".xls"), Qt::CaseInsensitive);
    auto& svc = conn::ConnectorService::instance();
    if (!sheets_request_.isEmpty()) {
        svc.cancel(sheets_request_);
        sheets_request_.clear();
    }
    if (!excel)
        return;
    QPointer<DataMappingScreen> self(this);
    sheets_request_ = svc.run_with(QStringLiteral("data_local_file"), QStringLiteral("sheets"), QStringList{path},
                                   conn::ConnectorService::RunOptions{}, [self](const conn::ConnectorResult& r) {
                                       if (!self)
                                           return;
                                       self->sheets_request_.clear();
                                       if (!r.success || r.tabular.tables.isEmpty())
                                           return;
                                       const dsrc::DataTable& t = r.tabular.tables.first();
                                       int col = t.column_index(QStringLiteral("sheet"));
                                       if (col < 0)
                                           col = 0;
                                       // Filling the list is not an edit of the mapping.
                                       self->loading_ui_ = true;
                                       const QString keep = self->file_detail_->currentText();
                                       self->file_detail_->clear();
                                       for (int i = 0; i < t.row_count(); ++i)
                                           self->file_detail_->addItem(dsrc::cell_text(t.cell(i, col)));
                                       self->file_detail_->setEditText(keep);
                                       self->loading_ui_ = false;
                                   });
}

void DataMappingScreen::show_banner(const QString& text, const char* look) {
    banner_->setText(text);
    set_look(banner_, look);
    banner_->setVisible(!text.isEmpty());
    banner_timer_->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// Source / preview
// ─────────────────────────────────────────────────────────────────────────────

void DataMappingScreen::on_load_source() {
    if (busy_)
        return;
    read_source_from_ui();
    if (!spec_.source.is_set()) {
        show_banner(tr("Choose the source first: a saved connection, a connector, a file or an HTTP address."),
                    "banner_error");
        return;
    }
    if (spec_.source.kind == QLatin1String("http") && auth_unreadable_ && http_auth_value_->text().isEmpty() &&
        http_auth_type_->currentText() != QLatin1String("None")) {
        show_banner(tr("Could not read secure storage: %1. Type the auth value again to load this source.").arg(auth_error_),
                    "banner_error");
        return;
    }
    busy_ = true;
    update_header();
    preview_skeleton_->set_message(tr("Loading the first %1 rows of %2…").arg(kPreviewRows).arg(spec_.source.describe()));
    preview_stack_->setCurrentWidget(preview_skeleton_);
    const quint64 gen = gen_;
    QPointer<DataMappingScreen> self(this);
    const QString label = spec_.source.describe();
    load_job_ = dmap::MappingRunner::instance().load_source(
        spec_.source, http_auth_value_->text(), kPreviewRows, this, [self, gen, label](const dmap::SourceData& d) {
            if (!self || gen != self->gen_)
                return;
            self->busy_ = false;
            self->load_job_ = 0;
            self->update_header();
            if (d.error.cancelled)
                return;
            if (!d.ok) {
                const auto page = datasources::error_page(d.error, self->spec_.source.kind == QLatin1String("connector"));
                self->show_preview_state(page.spec.title, page.spec.message, tr("TRY AGAIN"));
                if (page.primary == datasources::ErrorAction::AddKey)
                    self->show_banner(tr("%1 Add the key in Settings > Credentials (%2), then LOAD SOURCE again.")
                                          .arg(d.error.message, page.env_key),
                                      "banner_error");
                return;
            }
            self->set_source_data(d.data, d.notes, label);
            if (self->spec_.fields.isEmpty())
                self->auto_fields(true);
            self->schedule_preview();
        });
}

void DataMappingScreen::set_source_data(const dsrc::TabularResult& data, const QStringList& notes, const QString& label) {
    source_data_ = data;
    has_source_ = !data.tables.isEmpty();
    source_label_ = label;
    {
        const QSignalBlocker block(table_pick_);
        table_pick_->clear();
        for (int i = 0; i < data.tables.size(); ++i) {
            const auto& t = data.tables[i];
            table_pick_->addItem(tr("%1 · %2 rows").arg(t.name.isEmpty() ? tr("table %1").arg(i + 1) : t.name).arg(t.row_count()),
                                 i);
        }
        table_pick_->setVisible(data.tables.size() > 1);
    }
    int idx = spec_.source.table.isEmpty() ? 0 : data.index_of(spec_.source.table);
    if (idx < 0)
        idx = 0;
    {
        const QSignalBlocker block(table_pick_);
        table_pick_->setCurrentIndex(idx);
    }
    show_source_table(idx);
    if (!notes.isEmpty())
        show_banner(notes.join(QLatin1Char('\n')), "banner_info");
}

void DataMappingScreen::show_source_table(int index) {
    source_table_ = index;
    if (!has_source_ || index < 0 || index >= source_data_.tables.size()) {
        grid_model_->set_source_columns({});
        return;
    }
    const dsrc::DataTable& t = source_data_.tables[index];
    if (source_data_.tables.size() > 1)
        spec_.source.table = t.name;
    grid_model_->set_source_columns(t.columns);
    source_info_->setText(tr("%1 rows × %2 columns loaded%3 from %4 · columns: %5")
                              .arg(t.row_count())
                              .arg(t.column_count())
                              .arg(source_data_.truncated ? tr(" (more at the source; RUN reads them all)") : QString())
                              .arg(source_label_)
                              .arg(t.columns.mid(0, 12).join(QStringLiteral(", ")) +
                                   (t.columns.size() > 12 ? QStringLiteral(", …") : QString())));
    schedule_preview();
}

void DataMappingScreen::auto_fields(bool replace) {
    if (!has_source_ || source_table_ >= source_data_.tables.size())
        return;
    const dsrc::DataTable& t = source_data_.tables[source_table_];
    QVector<dmap::FieldMapping> fields = replace ? QVector<dmap::FieldMapping>() : spec_.fields;
    if (spec_.target_schema.isEmpty() || spec_.target_schema == QLatin1String("CUSTOM")) {
        for (int c = 0; c < t.column_count(); ++c) {
            dmap::FieldMapping f;
            f.target = t.columns[c];
            f.source = t.columns[c];
            f.type = dms_target_for(t.types.value(c));
            if (spec_.index_of(f.target) < 0 || replace)
                fields.append(f);
        }
    } else if (const dmap::TargetSchema* s = dmap::find_target_schema(spec_.target_schema)) {
        for (const dmap::SchemaField& sf : s->fields) {
            dmap::FieldMapping f;
            f.target = sf.name;
            f.type = sf.type;
            f.required = sf.required;
            f.source = dmap::suggest_source_column(sf.name, t.columns);
            fields.append(f);
        }
    }
    spec_.fields = fields;
    loading_ui_ = true;
    grid_model_->set_fields(fields, spec_.target_schema == QLatin1String("CUSTOM"));
    loading_ui_ = false;
    grid_stack_->setCurrentWidget(fields.isEmpty() ? static_cast<QWidget*>(grid_empty_) : static_cast<QWidget*>(grid_view_));
    set_dirty(true);
    schedule_preview();
}

void DataMappingScreen::on_schema_changed() {
    if (loading_ui_)
        return;
    const QString schema = schema_->currentData().toString();
    const QString previous = spec_.target_schema.isEmpty() ? QStringLiteral("CUSTOM") : spec_.target_schema;
    if (schema == previous)
        return;
    if (!spec_.fields.isEmpty() &&
        QMessageBox::question(this, tr("Change schema"),
                              tr("Replace the %n field(s) on screen with the %1 fields?", "", int(spec_.fields.size())).arg(schema),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes) {
        const QSignalBlocker block(schema_);
        schema_->setCurrentIndex(std::max(0, schema_->findData(previous)));
        return;
    }
    spec_.target_schema = schema;
    const bool custom = schema == QLatin1String("CUSTOM");
    add_field_btn_->setEnabled(custom);
    remove_field_btn_->setEnabled(custom);
    grid_empty_->set_primary_action(custom ? tr("+ FIELD") : QString());
    if (has_source_) {
        auto_fields(true);
    } else {
        QVector<dmap::FieldMapping> fields;
        if (const dmap::TargetSchema* s = dmap::find_target_schema(schema))
            for (const dmap::SchemaField& sf : s->fields) {
                dmap::FieldMapping f;
                f.target = sf.name;
                f.type = sf.type;
                f.required = sf.required;
                fields.append(f);
            }
        spec_.fields = fields;
        grid_model_->set_fields(fields, custom);
        grid_stack_->setCurrentWidget(fields.isEmpty() ? static_cast<QWidget*>(grid_empty_) : static_cast<QWidget*>(grid_view_));
        set_dirty(true);
    }
}

void DataMappingScreen::schedule_preview() {
    if (!has_source_)
        return;
    if (spec_.fields.isEmpty()) {
        show_preview_state(tr("No fields to map"), tr("Add fields to the grid (or AUTO-MATCH a schema) to see the mapped rows."),
                           QString());
        return;
    }
    if (preview_stack_->currentWidget() != preview_view_) {
        preview_skeleton_->set_message(tr("Mapping the loaded rows…"));
        preview_stack_->setCurrentWidget(preview_skeleton_);
    }
    preview_timer_->start();
}

void DataMappingScreen::show_preview_state(const QString& title, const QString& message, const QString& action) {
    preview_empty_->apply(fincept::ui::EmptyState::no_data_spec(title, message, action));
    preview_stack_->setCurrentWidget(preview_empty_);
    summary_->clear();
}

void DataMappingScreen::apply_preview(const dmap::MappingOutput& out, bool full_run) {
    preview_ = out;
    has_preview_ = true;
    preview_is_run_ = full_run;
    preview_view_->setSortingEnabled(false);
    preview_model_->set_table(out.table);
    // Mark every failed cell with its reason and the input text that failed.
    QHash<qint64, TabularTableModel::CellMark> marks;
    const dsrc::DataTable* input = has_source_ && source_table_ < source_data_.tables.size() ? &source_data_.tables[source_table_]
                                                                                           : nullptr;
    for (auto it = out.issue_index.constBegin(); it != out.issue_index.constEnd(); ++it) {
        const int out_row = int(it.key() >> 20);
        const int column = int(it.key() & 0xFFFFF);
        const dmap::CellIssue& issue = out.issues.at(it.value());
        TabularTableModel::CellMark m;
        m.message = issue.message;
        if (input && column < spec_.fields.size()) {
            const int src = input->column_index(spec_.fields[column].source);
            if (src >= 0)
                m.raw = dsrc::cell_text(input->cell(issue.row, src)).left(60);
        }
        marks.insert(TabularTableModel::mark_key(out_row, column), m);
    }
    preview_model_->set_marks(marks);
    preview_proxy_->sort(-1);
    preview_view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    preview_view_->setSortingEnabled(true);
    grid_model_->set_preview(out);

    QStringList parts;
    parts << tr("Rows in %1 → out %2").arg(out.rows_in).arg(out.rows_out);
    int failed_fields = 0;
    for (const auto& st : out.field_stats)
        if (st.errors > 0)
            ++failed_fields;
    parts << (out.error_cells > 0 ? tr("%1 cell(s) could not be cast in %2 field(s) — marked below").arg(out.error_cells).arg(failed_fields)
                                   : tr("every cell cast"));
    if (out.duplicates > 0)
        parts << tr("%1 duplicate row(s)%2").arg(out.duplicates).arg(out.rows_dropped_duplicate > 0 ? tr(" dropped") : QString());
    if (out.rows_dropped_invalid > 0)
        parts << tr("%1 row(s) left out for failed cells").arg(out.rows_dropped_invalid);
    if (!full_run && out.rows_in >= kPreviewRows)
        parts << tr("preview of the first %1 rows — RUN maps all").arg(kPreviewRows);
    QString text = parts.join(QStringLiteral(" · "));
    if (!out.spec_problems.isEmpty())
        text += QStringLiteral("\n") + tr("Mapping problems: %1").arg(out.spec_problems.join(QStringLiteral("; ")));
    summary_->setText(text);
    set_look(summary_, !out.spec_problems.isEmpty() || out.rows_out == 0 ? "error" : out.error_cells > 0 ? "warn" : "ok");
    if (out.rows_out == 0 && out.table.row_count() == 0) {
        show_preview_state(tr("No rows after mapping"),
                           out.spec_problems.isEmpty() ? tr("Every row was left out — check the options above.")
                                                       : out.spec_problems.join(QStringLiteral(" ")),
                           QString());
        summary_->setText(text);
        return;
    }
    preview_stack_->setCurrentWidget(preview_view_);
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

fincept::DataMapping DataMappingScreen::mapping_for_save() const {
    fincept::DataMapping dm = current_;
    dm.id = current_id_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : current_id_;
    dm.name = name_->text().trimmed();
    dm.schema_name = spec_.target_schema.isEmpty() ? QStringLiteral("CUSTOM") : spec_.target_schema;
    dm.source_id = spec_.source.kind == QLatin1String("connection") ? spec_.source.ref : QString();
    dm.spec_json = QString::fromUtf8(QJsonDocument(spec_.to_json()).toJson(QJsonDocument::Compact));
    // Columns read by tools that predate v062 (and the inline-HTTP request itself).
    QJsonArray legacy_fields;
    for (const auto& f : spec_.fields)
        legacy_fields.append(QJsonObject{{"target", f.target},
                                         {"expression", f.source.isEmpty() ? QString() : QStringLiteral("$.") + f.source},
                                         {"transform", f.transform},
                                         {"default_val", f.default_value}});
    dm.field_mappings_json = QString::fromUtf8(QJsonDocument(legacy_fields).toJson(QJsonDocument::Compact));
    if (spec_.source.kind == QLatin1String("http")) {
        const QJsonObject& h = spec_.source.http;
        dm.base_url = h.value(QStringLiteral("base_url")).toString();
        dm.endpoint = h.value(QStringLiteral("endpoint")).toString();
        dm.method = h.value(QStringLiteral("method")).toString(QStringLiteral("GET"));
        dm.auth_type = h.value(QStringLiteral("auth_type")).toString(QStringLiteral("None"));
        dm.headers = h.value(QStringLiteral("headers")).toString();
        dm.body = h.value(QStringLiteral("body")).toString();
        // The value goes to SecureStorage in the repository; the row keeps "". An unreadable
        // stored value with an empty field stays as it is (never deleted on a failed read).
        dm.auth_token = http_auth_value_->text();
    } else {
        dm.base_url.clear();
        dm.endpoint.clear();
        dm.auth_type = QStringLiteral("None");
        dm.auth_token.clear();
    }
    return dm;
}

void DataMappingScreen::on_save(bool then_run) {
    if (busy_)
        return;
    if (name_->text().trimmed().isEmpty()) {
        show_banner(tr("Give the mapping a name."), "banner_error");
        name_->setFocus();
        return;
    }
    read_source_from_ui();
    read_options_from_ui();
    spec_.fields = grid_model_->fields();
    if (spec_.fields.isEmpty()) {
        show_banner(tr("Add at least one field before saving."), "banner_error");
        return;
    }
    const fincept::DataMapping dm = mapping_for_save();
    busy_ = true;
    update_header();
    QPointer<DataMappingScreen> self(this);
    (void)QtConcurrent::run([self, dm, then_run]() {
        const auto r = fincept::DataMappingRepository::instance().save_revision(dm);
        const int rev = r.is_ok() ? r.value() : 0;
        const QString err = r.is_err() ? QString::fromStdString(r.error()) : QString();
        QMetaObject::invokeMethod(
            qApp,
            [self, dm, rev, err, then_run]() {
                if (!self)
                    return;
                self->busy_ = false;
                if (!err.isEmpty()) {
                    self->update_header();
                    self->show_banner(tr("Not saved: %1").arg(err), "banner_error");
                    return;
                }
                self->current_ = dm;
                self->current_.revision = rev;
                self->current_.spec_version = 2;
                self->current_id_ = dm.id;
                self->set_dirty(false);
                self->show_banner(tr("Saved as revision %1.").arg(rev), "banner_info");
                self->reload_list(dm.id);
                // Refresh the revision list.
                const quint64 gen = ++self->gen_;
                const QString id = dm.id;
                (void)QtConcurrent::run([self, gen, id, rev]() {
                    const auto revs = fincept::DataMappingRepository::instance().list_revisions(id);
                    const QVector<fincept::DataMappingRevision> list = revs.is_ok() ? revs.value()
                                                                                    : QVector<fincept::DataMappingRevision>();
                    QMetaObject::invokeMethod(
                        qApp,
                        [self, gen, list, rev]() {
                            if (!self || gen != self->gen_)
                                return;
                            self->revision_->clear();
                            for (int i = 0; i < list.size(); ++i) {
                                self->revision_->addItem(list[i].revision == rev
                                                             ? tr("rev %1 (current)").arg(list[i].revision)
                                                             : tr("rev %1 · %2").arg(list[i].revision).arg(
                                                                   datasources::relative_time_text(list[i].saved_at)),
                                                         list[i].revision);
                                self->revision_->setItemData(i, list[i].spec_json, Qt::UserRole + 1);
                            }
                            self->revision_->setCurrentIndex(std::max(0, self->revision_->findData(rev)));
                        },
                        Qt::QueuedConnection);
                });
                if (then_run)
                    self->on_run();
            },
            Qt::QueuedConnection);
    });
}

void DataMappingScreen::on_run() {
    if (busy_)
        return;
    if (current_id_.isEmpty() || dirty_) {
        on_save(true);
        return;
    }
    read_source_from_ui();
    if (!spec_.source.is_set()) {
        show_banner(tr("Choose the source first."), "banner_error");
        return;
    }
    busy_ = true;
    update_header();
    preview_skeleton_->set_message(tr("Running: reading every row of %1…").arg(spec_.source.describe()));
    preview_stack_->setCurrentWidget(preview_skeleton_);
    const quint64 gen = gen_;
    const QString id = current_id_;
    const int revision = current_.revision;
    const QString schema = spec_.target_schema;
    const dmap::MappingSpec spec = spec_;
    const QString label = spec_.source.describe();
    QPointer<DataMappingScreen> self(this);
    load_job_ = dmap::MappingRunner::instance().load_source(
        spec_.source, http_auth_value_->text(), kRunRows, this,
        [self, gen, id, revision, schema, spec, label](const dmap::SourceData& d) {
            if (!self || gen != self->gen_)
                return;
            self->load_job_ = 0;
            if (!d.ok || d.data.tables.isEmpty()) {
                self->busy_ = false;
                self->update_header();
                if (d.error.cancelled)
                    return;
                const auto page = datasources::error_page(d.error, spec.source.kind == QLatin1String("connector"));
                self->show_preview_state(page.spec.title, page.spec.message, tr("TRY AGAIN"));
                return;
            }
            self->set_source_data(d.data, d.notes, label);
            dmap::MappingRunner::instance().apply_async(
                d.data.tables.first(), spec, -1, self,
                [self, gen, id, revision, schema, label](const dmap::MappingOutput& out) {
                    if (!self || gen != self->gen_)
                        return;
                    self->busy_ = false;
                    dmap::MappingRunner::instance().store_output(id, revision, schema, out, label);
                    self->apply_preview(out, true);
                    self->current_.last_run_at = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                    self->current_.last_run_rows = out.rows_out;
                    self->current_.last_run_errors = out.error_cells;
                    self->update_header();
                    self->show_banner(tr("Run stored: %1 rows, %2 cell problem(s). Published on DataHub topic %3.")
                                          .arg(out.rows_out)
                                          .arg(out.error_cells)
                                          .arg(dmap::MappingRunner::topic_for(id)),
                                      out.error_cells > 0 ? "banner" : "banner_info");
                    QPointer<DataMappingScreen> again(self);
                    QTimer::singleShot(800, self, [again]() {
                        if (again)
                            again->reload_list();
                    });
                });
        });
}

void DataMappingScreen::on_delete() {
    if (current_id_.isEmpty() || busy_)
        return;
    if (QMessageBox::question(this, tr("Delete mapping"),
                              tr("Delete \"%1\" with its revisions and stored output?").arg(current_.name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    const QString id = current_id_;
    ++gen_;
    QPointer<DataMappingScreen> self(this);
    (void)QtConcurrent::run([self, id]() {
        const auto r = fincept::DataMappingRepository::instance().remove(id);
        const QString err = r.is_err() ? QString::fromStdString(r.error()) : QString();
        QMetaObject::invokeMethod(
            qApp,
            [self, err]() {
                if (!self)
                    return;
                if (!err.isEmpty()) {
                    self->show_banner(tr("Not deleted: %1").arg(err), "banner_error");
                    return;
                }
                self->current_id_.clear();
                self->current_ = fincept::DataMapping();
                self->dirty_ = false;
                self->right_stack_->setCurrentWidget(self->intro_);
                self->reload_list();
            },
            Qt::QueuedConnection);
    });
}

void DataMappingScreen::on_export(int kind) {
    if (!has_preview_ || preview_.table.columns.isEmpty()) {
        show_banner(tr("Nothing to export yet: LOAD SOURCE (or RUN) first."), "banner_info");
        return;
    }
    const dsrc::DataTable table = preview_.table;
    const QString stem = dms_file_stem(name_->text());
    const char* ext = kind == 0 ? ".csv" : kind == 1 ? ".json" : ".db";
    const QString filter = kind == 0   ? tr("CSV files (*.csv)")
                           : kind == 1 ? tr("JSON files (*.json)")
                                       : tr("SQLite databases (*.db *.sqlite *.sqlite3)");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export mapped rows"),
                                                      QDir(dms_default_dir()).filePath(stem + QLatin1String(ext)), filter);
    if (path.isEmpty())
        return;
    QString table_name;
    if (kind == 2) {
        bool ok = false;
        table_name = QInputDialog::getText(this, tr("SQLite table"), tr("Table name (replaced when it exists):"),
                                           QLineEdit::Normal, stem, &ok);
        if (!ok || table_name.trimmed().isEmpty())
            return;
    }
    const bool partial = !preview_is_run_;
    QPointer<DataMappingScreen> self(this);
    auto done = [self, path, partial](const dsrc::SourceError& e) {
        if (!self)
            return;
        if (!e.ok()) {
            self->show_banner(e.message, "banner_error");
            return;
        }
        self->show_banner(partial ? tr("Saved the preview rows to %1 — RUN first to export every row.").arg(QDir::toNativeSeparators(path))
                                  : tr("Saved %1").arg(QDir::toNativeSeparators(path)),
                          "banner_info");
    };
    auto& runner = dmap::MappingRunner::instance();
    if (kind == 0)
        runner.export_csv(table, path, this, done);
    else if (kind == 1)
        runner.export_json(table, path, this, done);
    else
        runner.export_sqlite(table, path, table_name.trimmed(), this, done);
}

void DataMappingScreen::on_revision_chosen(int index) {
    const int rev = revision_->itemData(index).toInt();
    const QString json = revision_->itemData(index, Qt::UserRole + 1).toString();
    if (json.isEmpty() || rev == current_.revision)
        return;
    if (!confirm_discard())
        return;
    spec_ = dmap::MappingSpec::from_json(QJsonDocument::fromJson(json.toUtf8()).object());
    const QString keep_name = name_->text();
    apply_spec_to_ui();
    name_->setText(keep_name);
    set_dirty(true);
    show_banner(tr("Revision %1 loaded — SAVE makes it the newest revision.").arg(rev), "banner_info");
    if (has_source_) {
        show_source_table(std::max(0, source_data_.index_of(spec_.source.table)));
        schedule_preview();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

QVariantMap DataMappingScreen::save_state() const {
    if (!splitter_)
        return pending_restore_;
    QVariantMap s;
    s["mapping"] = current_id_;
    s["splitter"] = splitter_->saveState().toBase64();
    if (editor_split_)
        s["editor_split"] = editor_split_->saveState().toBase64();
    return s;
}

void DataMappingScreen::restore_state(const QVariantMap& state) {
    pending_restore_ = state;
    if (splitter_)
        reload_list(state.value("mapping").toString());
}

bool DataMappingScreen::open_section(const QString& section, const QString& focus) {
    // + NEW (new_mapping) and a click in the mapping list (currentRowChanged → open_mapping).
    // Validated before anything changes.
    const QString raw = section.trimmed();
    const QString key = raw.toLower();
    QString id;
    if (key.startsWith(QLatin1String("mapping:"))) {
        id = raw.mid(8).trimmed();
        if (id.isEmpty())
            return false;
    } else if (key.isEmpty()) {
        id = focus.trimmed();
        if (id.isEmpty())
            return true;
    } else if (key != QLatin1String("new")) {
        return false;
    }
    if (!list_loaded_ || !list_model_ || !list_view_) {
        // Applied by reload_list() once the list is in (the workspace is built after the first show).
        pending_link_section_ = section;
        pending_link_focus_ = focus;
        has_pending_link_ = true;
        return true;
    }
    if (key == QLatin1String("new")) {
        new_mapping(nullptr);
        return true;
    }
    const int row = list_model_->row_of(id);
    if (row < 0)
        return false; // no such mapping: nothing changes
    if (id == current_id_)
        return true;
    if (list_view_->currentIndex().row() == row)
        open_mapping(id); // already the current row: what the list's own handler would do
    else
        list_view_->setCurrentIndex(list_model_->index(row, 0));
    return true;
}

} // namespace fincept::screens
