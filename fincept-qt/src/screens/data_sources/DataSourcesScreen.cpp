// DataSourcesScreen.cpp — see DataSourcesScreen.h.

#include "screens/data_sources/DataSourcesScreen.h"

#include "core/logging/Logger.h"
#include "screens/data_sources/CatalogConnectorView.h"
#include "screens/data_sources/ConnectionDetailView.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "screens/data_sources/DataWorkspace.h"
#include "screens/data_sources/ImportExportConnections.h"
#include "services/data_sources/ConnectionDrivers.h"
#include "services/data_sources/ConnectionService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace fincept::screens::datasources {

namespace dsrc = fincept::services::datasources;

namespace {

struct DsSnapshot {
    QVector<DataSource> connections;
    QVector<SourceRow> rows;
    bool with_rows = false;
    QString error;
};

QString ds_driver_label(dsrc::DriverKind k) {
    switch (k) {
        case dsrc::DriverKind::Http:
            return QObject::tr("HTTP API");
        case dsrc::DriverKind::EventStream:
            return QObject::tr("Server-Sent Events");
        case dsrc::DriverKind::WebSocket:
            return QObject::tr("WebSocket sample");
        case dsrc::DriverKind::File:
            return QObject::tr("Local file (read-only)");
        case dsrc::DriverKind::Sql:
            return QObject::tr("SQL driver");
        case dsrc::DriverKind::Catalogue:
            return QObject::tr("Catalogue connector");
        case dsrc::DriverKind::Redis:
            return QObject::tr("Redis protocol");
        case dsrc::DriverKind::Memcached:
            return QObject::tr("Memcached protocol");
        case dsrc::DriverKind::None:
            break;
    }
    return QObject::tr("—");
}

QTableView* ds_make_view(QWidget* parent) {
    auto* v = new QTableView(parent);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setAlternatingRowColors(true);
    v->setSortingEnabled(true);
    v->setWordWrap(false);
    v->verticalHeader()->setVisible(false);
    v->horizontalHeader()->setHighlightSections(false);
    v->horizontalHeader()->setSectionsMovable(false);
    fincept::ui::metrics::follow_row_height(v, 26);
    return v;
}

} // namespace

DataSourcesScreen::DataSourcesScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("dwRoot"));
    setStyleSheet(workspace_qss()); // once, before any child exists
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    root_stack_ = new QStackedWidget(this);
    boot_skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, root_stack_);
    boot_skeleton_->set_message(tr("Opening Data Sources…"));
    root_stack_->addWidget(boot_skeleton_);
    vl->addWidget(root_stack_);
}

void DataSourcesScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Coming back (e.g. from Settings > Credentials after adding a key): re-read key presence
    // and driver availability off the GUI thread when the badges are more than a minute old.
    if (built_ && loaded_sources_ && QDateTime::currentMSecsSinceEpoch() - sources_loaded_ms_ > 60 * 1000)
        reload(true);
    if (!built_) {
        built_ = true;
        // Build after the skeleton has painted: the first show never waits for the workspace.
        QPointer<DataSourcesScreen> self(this);
        QTimer::singleShot(0, this, [self]() {
            if (!self)
                return;
            self->build_workspace();
            self->reload(true);
        });
    }
}

void DataSourcesScreen::build_workspace() {
    auto* ws = new QWidget(root_stack_);
    auto* vl = new QVBoxLayout(ws);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(ws);
    set_look(header, "bar");
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(8);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(1);
    auto* title = new QLabel(tr("DATA SOURCES"), header);
    set_look(title, "h1");
    subtitle_ = new QLabel(header);
    set_look(subtitle_, "muted");
    titles->addWidget(title);
    titles->addWidget(subtitle_);
    hl->addLayout(titles, 1);
    auto* import_btn = new QPushButton(tr("IMPORT"), header);
    import_btn->setToolTip(tr("Import connections from a file (secrets are restored only with the file's passphrase)"));
    auto* export_btn = new QPushButton(tr("EXPORT"), header);
    export_btn->setToolTip(tr("Export every connection; secrets are left out unless you encrypt them with a passphrase"));
    auto* tpl_btn = new QPushButton(tr("TEMPLATE"), header);
    tpl_btn->setToolTip(tr("Save a blank file with every connection type, to fill in and import"));
    hl->addWidget(import_btn);
    hl->addWidget(export_btn);
    hl->addWidget(tpl_btn);
    vl->addWidget(header);

    notice_ = new QLabel(ws);
    set_look(notice_, "banner_info");
    notice_->setWordWrap(true);
    notice_->setTextFormat(Qt::PlainText);
    notice_->hide();
    vl->addWidget(notice_);
    notice_timer_ = new QTimer(this);
    notice_timer_->setSingleShot(true);
    notice_timer_->setInterval(9000);
    connect(notice_timer_, &QTimer::timeout, notice_, &QLabel::hide);

    splitter_ = new QSplitter(Qt::Horizontal, ws);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(1);
    splitter_->addWidget(build_sources_pane(splitter_));
    splitter_->addWidget(build_connections_pane(splitter_));
    splitter_->addWidget(build_detail_pane(splitter_));
    const int em = fontMetrics().horizontalAdvance(QLatin1Char('M'));
    splitter_->setSizes({em * 26, em * 30, em * 48});
    splitter_->setStretchFactor(0, 2);
    splitter_->setStretchFactor(1, 2);
    splitter_->setStretchFactor(2, 4);
    vl->addWidget(splitter_, 1);

    root_stack_->addWidget(ws);
    root_stack_->setCurrentWidget(ws);

    QPointer<DataSourcesScreen> self(this);
    auto transfer_done = [self](bool ok, int, const QString& message) {
        if (!self)
            return;
        if (!message.isEmpty())
            self->show_notice(message, !ok);
        if (ok)
            self->reload(false);
    };
    connect(import_btn, &QPushButton::clicked, this, [this, transfer_done]() { import_connections(this, transfer_done); });
    connect(export_btn, &QPushButton::clicked, this, [this, transfer_done]() { export_connections(this, transfer_done); });
    connect(tpl_btn, &QPushButton::clicked, this, [this]() {
        if (download_connector_template(this))
            show_notice(tr("Template saved. Fill in the settings and use IMPORT."));
    });

    health_debounce_ = new QTimer(this);
    health_debounce_->setSingleShot(true);
    health_debounce_->setInterval(400);
    connect(health_debounce_, &QTimer::timeout, this, [this]() { reload(false); });
    connect(&dsrc::ConnectionService::instance(), &dsrc::ConnectionService::health_changed, this,
            [this](const QString&, bool, const QString&) { health_debounce_->start(); });
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) {
                sources_model_->refresh_colours();
                conns_model_->refresh_colours();
            });
}

QWidget* DataSourcesScreen::build_sources_pane(QWidget* parent) {
    auto* pane = new QWidget(parent);
    set_look(pane, "pane");
    pane->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(pane);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(tr("SOURCES"), pane);
    set_look(title, "title");
    sources_count_ = new QLabel(pane);
    set_look(sources_count_, "note");
    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(sources_count_);
    vl->addLayout(top);
    source_search_ = new QLineEdit(pane);
    source_search_->setPlaceholderText(tr("search types and connectors…"));
    source_search_->setClearButtonEnabled(true);
    vl->addWidget(source_search_);
    auto* frow = new QHBoxLayout;
    frow->setSpacing(6);
    kind_filter_ = new QComboBox(pane);
    kind_filter_->addItem(tr("All sources"), int(SourceFilterProxy::KindFilter::All));
    kind_filter_->addItem(tr("Connection types"), int(SourceFilterProxy::KindFilter::Types));
    kind_filter_->addItem(tr("Catalogue connectors"), int(SourceFilterProxy::KindFilter::Connectors));
    category_filter_ = new QComboBox(pane);
    category_filter_->addItem(tr("All categories"), QString());
    category_filter_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    category_filter_->setMinimumContentsLength(10);
    frow->addWidget(kind_filter_, 1);
    frow->addWidget(category_filter_, 1);
    vl->addLayout(frow);
    usable_only_ = new QCheckBox(tr("Only what works here"), pane);
    usable_only_->setToolTip(tr("Hide types this build cannot reach and connectors whose upstream is broken"));
    vl->addWidget(usable_only_);

    sources_stack_ = new QStackedWidget(pane);
    auto* sk = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, sources_stack_);
    sk->set_message(tr("Checking what works on this computer…"));
    sources_model_ = new SourceCatalogModel(this);
    sources_proxy_ = new SourceFilterProxy(this);
    sources_proxy_->setSourceModel(sources_model_);
    sources_view_ = ds_make_view(sources_stack_);
    sources_view_->setModel(sources_proxy_);
    sources_view_->sortByColumn(SourceCatalogModel::ColName, Qt::AscendingOrder);
    sources_view_->horizontalHeader()->setSectionResizeMode(SourceCatalogModel::ColName, QHeaderView::Stretch);
    sources_view_->horizontalHeader()->setSectionResizeMode(SourceCatalogModel::ColCategory, QHeaderView::Interactive);
    sources_view_->horizontalHeader()->setSectionResizeMode(SourceCatalogModel::ColStatus, QHeaderView::ResizeToContents);
    sources_view_->setColumnWidth(SourceCatalogModel::ColCategory, fontMetrics().horizontalAdvance(QStringLiteral("Data Warehouses")));
    sources_empty_ = new fincept::ui::EmptyState(sources_stack_);
    sources_empty_->set_compact(true);
    sources_empty_->apply(fincept::ui::EmptyState::no_data_spec(tr("No source matches"),
                                                                tr("Change the search or the filters."),
                                                                tr("CLEAR FILTERS")));
    sources_stack_->addWidget(sk);
    sources_stack_->addWidget(sources_view_);
    sources_stack_->addWidget(sources_empty_);
    vl->addWidget(sources_stack_, 1);

    auto refilter = [this]() {
        sources_count_->setText(tr("%1 shown").arg(sources_proxy_->rowCount()));
        if (loaded_sources_)
            sources_stack_->setCurrentWidget(sources_proxy_->rowCount() > 0 ? static_cast<QWidget*>(sources_view_)
                                                                            : static_cast<QWidget*>(sources_empty_));
    };
    connect(source_search_, &QLineEdit::textChanged, this, [this, refilter](const QString& t) {
        sources_proxy_->set_search(t);
        refilter();
    });
    connect(kind_filter_, &QComboBox::currentIndexChanged, this, [this, refilter](int) {
        sources_proxy_->set_kind(SourceFilterProxy::KindFilter(kind_filter_->currentData().toInt()));
        refilter();
    });
    connect(category_filter_, &QComboBox::currentIndexChanged, this, [this, refilter](int) {
        sources_proxy_->set_category(category_filter_->currentData().toString());
        refilter();
    });
    connect(usable_only_, &QCheckBox::toggled, this, [this, refilter](bool on) {
        sources_proxy_->set_usable_only(on);
        refilter();
    });
    connect(sources_empty_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        source_search_->clear();
        kind_filter_->setCurrentIndex(0);
        category_filter_->setCurrentIndex(0);
        usable_only_->setChecked(false);
    });
    connect(sources_view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { on_source_selected(); });
    // Clicking the highlighted row again brings its card back (after a connection was opened).
    connect(sources_view_, &QTableView::clicked, this, [this](const QModelIndex&) { on_source_selected(); });
    return pane;
}

QWidget* DataSourcesScreen::build_connections_pane(QWidget* parent) {
    auto* pane = new QWidget(parent);
    set_look(pane, "pane");
    pane->setAttribute(Qt::WA_StyledBackground, true);
    auto* vl = new QVBoxLayout(pane);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(6);
    auto* top = new QHBoxLayout;
    auto* title = new QLabel(tr("SAVED CONNECTIONS"), pane);
    set_look(title, "title");
    conns_count_ = new QLabel(pane);
    set_look(conns_count_, "note");
    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(conns_count_);
    vl->addLayout(top);
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    conn_search_ = new QLineEdit(pane);
    conn_search_->setPlaceholderText(tr("filter connections…"));
    conn_search_->setClearButtonEnabled(true);
    row->addWidget(conn_search_, 1);
    new_btn_ = new QPushButton(tr("+ NEW"), pane);
    set_look(new_btn_, "primary");
    new_btn_->setEnabled(false);
    new_btn_->setToolTip(tr("Pick a connection type on the left first"));
    row->addWidget(new_btn_);
    vl->addLayout(row);
    only_type_ = new QCheckBox(tr("Only the selected type"), pane);
    only_type_->setEnabled(false);
    vl->addWidget(only_type_);

    conns_stack_ = new QStackedWidget(pane);
    auto* sk = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, conns_stack_);
    sk->set_message(tr("Loading saved connections…"));
    conns_model_ = new ConnectionListModel(this);
    conns_proxy_ = new ConnectionFilterProxy(this);
    conns_proxy_->setSourceModel(conns_model_);
    conns_view_ = ds_make_view(conns_stack_);
    conns_view_->setModel(conns_proxy_);
    conns_view_->sortByColumn(ConnectionListModel::ColName, Qt::AscendingOrder);
    conns_view_->horizontalHeader()->setSectionResizeMode(ConnectionListModel::ColName, QHeaderView::Stretch);
    for (int c : {int(ConnectionListModel::ColType), int(ConnectionListModel::ColStatus), int(ConnectionListModel::ColTested),
                  int(ConnectionListModel::ColLatency)})
        conns_view_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    conns_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    conns_empty_ = new fincept::ui::EmptyState(conns_stack_);
    conns_empty_->set_compact(true);
    conns_stack_->addWidget(sk);
    conns_stack_->addWidget(conns_view_);
    conns_stack_->addWidget(conns_empty_);
    vl->addWidget(conns_stack_, 1);

    connect(conn_search_, &QLineEdit::textChanged, this, [this](const QString& t) {
        conns_proxy_->set_search(t);
        update_counts();
    });
    connect(only_type_, &QCheckBox::toggled, this, [this](bool on) {
        conns_proxy_->set_provider(on ? current_type_ : QString());
        update_counts();
    });
    connect(new_btn_, &QPushButton::clicked, this, [this]() {
        if (current_type_.isEmpty() || !confirm_leave_edits())
            return;
        conns_view_->clearSelection();
        shown_connection_.clear();
        conn_view_->show_new(current_type_);
        detail_stack_->setCurrentWidget(conn_view_);
    });
    connect(conns_empty_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
        if (!current_type_.isEmpty()) {
            new_btn_->click();
            return;
        }
        kind_filter_->setCurrentIndex(1);
        source_search_->setFocus();
    });
    connect(conns_view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex&, const QModelIndex&) { on_connection_selected(); });
    connect(conns_view_, &QTableView::clicked, this, [this](const QModelIndex&) { on_connection_selected(); });
    connect(conns_view_, &QTableView::customContextMenuRequested, this, &DataSourcesScreen::show_connection_menu);
    return pane;
}

QWidget* DataSourcesScreen::build_detail_pane(QWidget* parent) {
    detail_stack_ = new QStackedWidget(parent);
    intro_ = new fincept::ui::EmptyState(detail_stack_);
    intro_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("Pick a source"),
        tr("Left: connection types (databases, APIs, files, streams, cloud stores) and catalogue connectors "
           "(ready-made public data). Middle: your saved connections — TEST sends a real request, not a socket "
           "check.")));
    type_card_ = new fincept::ui::EmptyState(detail_stack_);
    conn_view_ = new ConnectionDetailView(detail_stack_);
    connector_view_ = new CatalogConnectorView(detail_stack_);
    detail_stack_->addWidget(intro_);
    detail_stack_->addWidget(type_card_);
    detail_stack_->addWidget(conn_view_);
    detail_stack_->addWidget(connector_view_);

    connect(type_card_, &fincept::ui::EmptyState::primary_clicked, this, [this]() { new_btn_->click(); });
    connect(type_card_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() { only_type_->setChecked(true); });
    connect(conn_view_, &ConnectionDetailView::saved, this, [this](const QString& id) {
        pending_select_connection_ = id;
        shown_connection_ = id;
        reload(false);
    });
    connect(conn_view_, &ConnectionDetailView::deleted, this, [this](const QString&) {
        shown_connection_.clear();
        show_notice(tr("Connection deleted."));
        if (!current_type_.isEmpty())
            show_type_card(current_type_);
        else
            detail_stack_->setCurrentWidget(intro_);
        reload(false);
    });
    connect(conn_view_, &ConnectionDetailView::duplicate_requested, this, [this](const QString& id) {
        if (const DataSource* ds = conns_model_->find(id)) {
            const DataSource copy = *ds;
            conns_view_->clearSelection();
            shown_connection_.clear();
            conn_view_->show_new(copy.provider, &copy);
            detail_stack_->setCurrentWidget(conn_view_);
        }
    });
    return detail_stack_;
}

void DataSourcesScreen::reload(bool with_sources) {
    if (loading_) {
        reload_again_ = true;
        return;
    }
    loading_ = true;
    QPointer<DataSourcesScreen> self(this);
    (void)QtConcurrent::run([self, with_sources]() {
        DsSnapshot snap;
        snap.with_rows = with_sources;
        const auto r = DataSourceRepository::instance().list_all();
        if (r.is_ok())
            snap.connections = r.value();
        else
            snap.error = QString::fromStdString(r.error());
        if (with_sources)
            snap.rows = build_source_rows(snap.connections);
        QMetaObject::invokeMethod(
            qApp,
            [self, snap = std::move(snap)]() {
                if (!self)
                    return;
                self->loading_ = false;
                if (!snap.error.isEmpty())
                    self->show_notice(tr("Could not read the saved connections: %1").arg(snap.error), true);
                if (snap.with_rows) {
                    {
                        const QSignalBlocker block(self->sources_view_->selectionModel());
                        self->sources_model_->set_rows(snap.rows);
                        // Keep the highlighted row across the refresh.
                        if (self->shown_source_.size() > 2) {
                            const SourceKind kind = self->shown_source_.startsWith(QLatin1String("t:")) ? SourceKind::Type
                                                                                                         : SourceKind::Connector;
                            const int row = self->sources_model_->row_of(kind, self->shown_source_.mid(2));
                            const QModelIndex idx = self->sources_proxy_->mapFromSource(self->sources_model_->index(row, 0));
                            if (idx.isValid())
                                self->sources_view_->selectionModel()->setCurrentIndex(
                                    idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                        }
                    }
                    self->sources_loaded_ms_ = QDateTime::currentMSecsSinceEpoch();
                    self->connector_view_->refresh_key_state();
                    self->loaded_sources_ = true;
                    const QString cur = self->category_filter_->currentData().toString();
                    {
                        const QSignalBlocker block(self->category_filter_);
                        while (self->category_filter_->count() > 1)
                            self->category_filter_->removeItem(1);
                        for (const QString& c : self->sources_model_->categories())
                            self->category_filter_->addItem(c, c);
                        self->category_filter_->setCurrentIndex(std::max(0, self->category_filter_->findData(cur)));
                    }
                    self->sources_stack_->setCurrentWidget(self->sources_proxy_->rowCount() > 0
                                                               ? static_cast<QWidget*>(self->sources_view_)
                                                               : static_cast<QWidget*>(self->sources_empty_));
                    self->sources_count_->setText(tr("%1 shown").arg(self->sources_proxy_->rowCount()));
                }
                self->apply_connections(snap.connections);
                self->apply_restore();
                if (self->has_pending_link_ && self->loaded_sources_) {
                    // A deep link that arrived before the lists: after the restore, so it wins.
                    self->has_pending_link_ = false;
                    self->open_section(self->pending_link_section_, self->pending_link_focus_);
                }
                if (self->reload_again_) {
                    self->reload_again_ = false;
                    self->reload(false);
                }
            },
            Qt::QueuedConnection);
    });
}

void DataSourcesScreen::apply_connections(const QVector<DataSource>& rows) {
    const QString keep = !pending_select_connection_.isEmpty() ? pending_select_connection_ : shown_connection_;
    {
        const QSignalBlocker block(conns_view_->selectionModel());
        conns_model_->set_connections(rows);
    }
    sources_model_->set_saved_counts(rows);
    update_counts();
    if (!keep.isEmpty()) {
        if (const DataSource* ds = conns_model_->find(keep)) {
            conn_view_->update_from(*ds);
            const QModelIndex src = conns_model_->index(conns_model_->row_of(keep), 0);
            const QModelIndex idx = conns_proxy_->mapFromSource(src);
            if (idx.isValid()) {
                const QSignalBlocker block(conns_view_->selectionModel());
                conns_view_->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect |
                                                                        QItemSelectionModel::Rows);
                conns_view_->scrollTo(idx);
            }
            if (!pending_select_connection_.isEmpty()) {
                shown_connection_ = keep;
                conn_view_->show_connection(*ds);
                detail_stack_->setCurrentWidget(conn_view_);
            }
        }
    }
    pending_select_connection_.clear();
}

void DataSourcesScreen::update_counts() {
    const int total = conns_model_->rowCount();
    const int shown = conns_proxy_->rowCount();
    conns_count_->setText(shown == total ? tr("%1").arg(total) : tr("%1 of %2").arg(shown).arg(total));
    if (shown == 0) {
        if (total == 0)
            conns_empty_->apply(fincept::ui::EmptyState::no_data_spec(
                tr("No saved connections yet"),
                tr("Pick a connection type on the left (a database, an API, a file…), then + NEW. TEST checks it for "
                   "real."),
                current_type_.isEmpty() ? tr("SHOW CONNECTION TYPES") : tr("+ NEW CONNECTION")));
        else
            conns_empty_->apply(fincept::ui::EmptyState::no_data_spec(tr("No connection matches"),
                                                                      tr("Change the filter, or add one of this type."),
                                                                      current_type_.isEmpty() ? QString() : tr("+ NEW CONNECTION")));
        conns_stack_->setCurrentWidget(conns_empty_);
    } else {
        conns_stack_->setCurrentWidget(conns_view_);
    }
    const int types = ConnectorRegistry::instance().count();
    int connectors = 0;
    for (int i = 0; i < sources_model_->rowCount(); ++i)
        if (const SourceRow* r = sources_model_->row_at(i); r && r->kind == SourceKind::Connector)
            ++connectors;
    subtitle_->setText(tr("%1 connection types · %2 catalogue connectors · %3 saved connections")
                           .arg(types)
                           .arg(connectors)
                           .arg(total));
}

void DataSourcesScreen::show_notice(const QString& text, bool error) {
    if (!notice_)
        return;
    notice_->setText(text);
    set_look(notice_, error ? "banner_error" : "banner_info");
    notice_->setVisible(!text.isEmpty());
    notice_timer_->start();
}

bool DataSourcesScreen::confirm_leave_edits() {
    if (!conn_view_ || detail_stack_->currentWidget() != conn_view_ || !conn_view_->has_unsaved_edits())
        return true;
    return QMessageBox::question(this, tr("Unsaved changes"),
                                 tr("The connection on the right has unsaved changes. Leave it without saving?"),
                                 QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
}

void DataSourcesScreen::on_source_selected() {
    const QModelIndex idx = sources_proxy_->mapToSource(sources_view_->currentIndex());
    const SourceRow* r = sources_model_->row_at(idx.row());
    if (!r)
        return;
    const QString key = (r->kind == SourceKind::Type ? QStringLiteral("t:") : QStringLiteral("c:")) + r->id;
    QWidget* page = r->kind == SourceKind::Type ? static_cast<QWidget*>(type_card_) : static_cast<QWidget*>(connector_view_);
    if (key == shown_source_ && detail_stack_->currentWidget() == page)
        return; // the same row clicked again: nothing to reload
    if (!confirm_leave_edits())
        return;
    shown_source_ = key;
    if (r->kind == SourceKind::Type) {
        current_type_ = r->id;
        new_btn_->setEnabled(true);
        new_btn_->setToolTip(tr("New %1 connection").arg(r->title));
        only_type_->setEnabled(true);
        only_type_->setText(tr("Only %1").arg(r->title));
        if (only_type_->isChecked())
            conns_proxy_->set_provider(current_type_);
        update_counts();
        show_type_card(r->id);
    } else {
        connector_view_->show_connector(r->id);
        detail_stack_->setCurrentWidget(connector_view_);
    }
    shown_connection_.clear();
}

void DataSourcesScreen::show_type_card(const QString& type_id) {
    const ConnectorConfig* cfg = find_connector_config(type_id);
    if (!cfg)
        return;
    const dsrc::TypeSupport s = dsrc::effective_support(cfg->id);
    fincept::ui::EmptyState::Spec spec;
    spec.glyph = connector_code(*cfg);
    spec.glyph_tone = s.supported ? fincept::ui::EmptyState::Tone::Accent : fincept::ui::EmptyState::Tone::Neutral;
    spec.title = cfg->name;
    spec.message = cfg->description;
    const int saved = total_connections_for_provider(conns_model_->connections(), cfg->id);
    spec.details = {
        {tr("Works in this build"), s.supported ? tr("yes") : tr("no")},
        {tr("Reached through"), ds_driver_label(s.driver)},
        {tr("Browse"), s.can_list ? (s.entity_label.isEmpty() ? tr("yes") : s.entity_label + QStringLiteral("s")) : tr("—")},
        {tr("Query"), s.query_label.isEmpty() ? tr("—") : s.query_label},
        {tr("Saved connections"), QString::number(saved)},
    };
    spec.details_heading = tr("THIS TYPE");
    if (!s.supported) {
        spec.note = s.fix.isEmpty() ? s.reason : tr("%1 — %2").arg(s.reason, s.fix);
        spec.note_tone = fincept::ui::EmptyState::Tone::Warning;
    } else if (!s.parity.isEmpty()) {
        spec.note = tr("Same target from outside: %1").arg(s.parity);
        spec.note_tone = fincept::ui::EmptyState::Tone::Info;
    }
    spec.primary = s.supported ? tr("+ NEW CONNECTION") : QString();
    spec.secondary = saved > 0 && !only_type_->isChecked() ? tr("SHOW ITS CONNECTIONS") : QString();
    type_card_->apply(spec);
    detail_stack_->setCurrentWidget(type_card_);
}

void DataSourcesScreen::on_connection_selected() {
    const QModelIndex idx = conns_proxy_->mapToSource(conns_view_->currentIndex());
    if (!idx.isValid())
        return;
    const QString id = conns_model_->data(idx, ConnectionListModel::IdRole).toString();
    if (id == shown_connection_ && detail_stack_->currentWidget() == conn_view_)
        return;
    if (!confirm_leave_edits())
        return;
    if (const DataSource* ds = conns_model_->find(id)) {
        shown_connection_ = id;
        conn_view_->show_connection(*ds);
        detail_stack_->setCurrentWidget(conn_view_);
    }
}

void DataSourcesScreen::select_source(SourceKind kind, const QString& id) {
    const int row = sources_model_->row_of(kind, id);
    if (row < 0)
        return;
    const QModelIndex idx = sources_proxy_->mapFromSource(sources_model_->index(row, 0));
    if (!idx.isValid())
        return;
    sources_view_->setCurrentIndex(idx);
    sources_view_->scrollTo(idx);
}

void DataSourcesScreen::select_connection(const QString& id) {
    const int row = conns_model_->row_of(id);
    if (row < 0)
        return;
    const QModelIndex idx = conns_proxy_->mapFromSource(conns_model_->index(row, 0));
    if (idx.isValid()) {
        conns_view_->setCurrentIndex(idx);
        conns_view_->scrollTo(idx);
    }
}

void DataSourcesScreen::show_connection_menu(const QPoint& pos) {
    const QModelIndex idx = conns_proxy_->mapToSource(conns_view_->indexAt(pos));
    if (!idx.isValid())
        return;
    const QString id = conns_model_->data(idx, ConnectionListModel::IdRole).toString();
    const DataSource* ds = conns_model_->find(id);
    if (!ds)
        return;
    const bool enabled = ds->enabled;
    QMenu menu(this);
    QAction* open = menu.addAction(tr("Open"));
    QAction* dup = menu.addAction(tr("Duplicate"));
    QAction* toggle = menu.addAction(enabled ? tr("Disable") : tr("Enable"));
    menu.addSeparator();
    QAction* del = menu.addAction(tr("Delete…"));
    del->setProperty("fincept_agent_risk", "delete"); // agents: deletes the connection
    QAction* chosen = menu.exec(conns_view_->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == open) {
        select_connection(id);
    } else if (chosen == dup) {
        select_connection(id);
        emit conn_view_->duplicate_requested(id);
    } else if (chosen == toggle) {
        QPointer<DataSourcesScreen> self(this);
        (void)QtConcurrent::run([self, id, enabled]() {
            DataSourceRepository::instance().set_enabled(id, !enabled);
            QMetaObject::invokeMethod(
                qApp,
                [self]() {
                    if (self)
                        self->reload(false);
                },
                Qt::QueuedConnection);
        });
    } else if (chosen == del) {
        select_connection(id);
        if (detail_stack_->currentWidget() == conn_view_ && conn_view_->connection_id() == id)
            conn_view_->delete_with_confirm();
    }
}

void DataSourcesScreen::apply_restore() {
    if (pending_restore_.isEmpty() || !loaded_sources_)
        return;
    const QVariantMap s = pending_restore_;
    pending_restore_.clear();
    source_search_->setText(s.value("search").toString());
    kind_filter_->setCurrentIndex(std::clamp(s.value("kind").toInt(), 0, 2));
    const int ci = category_filter_->findData(s.value("category").toString());
    category_filter_->setCurrentIndex(ci >= 0 ? ci : 0);
    usable_only_->setChecked(s.value("usable").toBool());
    conn_search_->setText(s.value("conn_search").toString());
    const QString src_id = s.value("source_id").toString();
    if (!src_id.isEmpty())
        select_source(SourceKind(s.value("source_kind").toInt()), src_id);
    only_type_->setChecked(s.value("only_type").toBool() && !current_type_.isEmpty());
    const QString conn_id = s.value("connection").toString();
    if (!conn_id.isEmpty())
        select_connection(conn_id);
    if (splitter_ && s.contains("splitter"))
        splitter_->restoreState(QByteArray::fromBase64(s.value("splitter").toByteArray()));
}

QVariantMap DataSourcesScreen::save_state() const {
    if (!built_ || !sources_view_)
        return pending_restore_;
    QVariantMap s;
    s["search"] = source_search_->text();
    s["kind"] = kind_filter_->currentIndex();
    s["category"] = category_filter_->currentData().toString();
    s["usable"] = usable_only_->isChecked();
    s["conn_search"] = conn_search_->text();
    s["only_type"] = only_type_->isChecked();
    const QModelIndex idx = sources_proxy_->mapToSource(sources_view_->currentIndex());
    if (const SourceRow* r = sources_model_->row_at(idx.row())) {
        s["source_kind"] = int(r->kind);
        s["source_id"] = r->id;
    }
    s["connection"] = shown_connection_;
    if (splitter_)
        s["splitter"] = splitter_->saveState().toBase64();
    return s;
}

void DataSourcesScreen::restore_state(const QVariantMap& state) {
    pending_restore_ = state;
    if (loaded_sources_)
        apply_restore();
}

// ── ISectionLinked ───────────────────────────────────────────────────────────

bool DataSourcesScreen::open_section(const QString& section, const QString& focus) {
    // A row selected in its list as a click does (select_source / select_connection →
    // on_source_selected / on_connection_selected: the detail pane follows, with its own
    // unsaved-edits question). Validated before anything changes.
    enum class Act { None, Source, Connection, SourcesList, ConnectionsList };
    const QString raw = section.trimmed();
    const QString key = raw.toLower();
    const QString want = focus.trimmed();
    Act act = Act::None;
    SourceKind kind = SourceKind::Connector;
    QString id = want;
    int kind_index = -1; // kind_filter_: 1 = connection types, 2 = catalogue connectors
    if (key.startsWith(QLatin1String("connector:"))) {
        act = Act::Source;
        id = raw.mid(10).trimmed();
    } else if (key.startsWith(QLatin1String("type:"))) {
        act = Act::Source;
        kind = SourceKind::Type;
        id = raw.mid(5).trimmed();
    } else if (key.startsWith(QLatin1String("connection:"))) {
        act = Act::Connection;
        id = raw.mid(11).trimmed();
    } else if (key.isEmpty() || key == QLatin1String("connector") || key == QLatin1String("sources")) {
        act = !want.isEmpty() ? Act::Source : key.isEmpty() ? Act::None : Act::SourcesList;
    } else if (key == QLatin1String("catalog") || key == QLatin1String("connectors")) {
        kind_index = 2;
        act = want.isEmpty() ? Act::SourcesList : Act::Source;
    } else if (key == QLatin1String("types")) {
        kind_index = 1;
        kind = SourceKind::Type;
        act = want.isEmpty() ? Act::SourcesList : Act::Source;
    } else if (key == QLatin1String("connections")) {
        act = want.isEmpty() ? Act::ConnectionsList : Act::Connection;
    } else {
        return false;
    }
    if ((act == Act::Source || act == Act::Connection) && id.isEmpty())
        return false;
    if (act == Act::None)
        return true;
    if (!loaded_sources_ || !sources_view_) {
        // Applied by reload() once the lists are in (the workspace is built after the first show).
        pending_link_section_ = section;
        pending_link_focus_ = focus;
        has_pending_link_ = true;
        return true;
    }
    if (act == Act::Source) {
        const int row = sources_model_->row_of(kind, id);
        if (row < 0)
            return false; // not in the catalogue: nothing changes
        if (kind_index >= 0)
            kind_filter_->setCurrentIndex(kind_index);
        QModelIndex idx = sources_proxy_->mapFromSource(sources_model_->index(row, 0));
        if (!idx.isValid()) {
            // A filter hides it: show every source, as the user would to find it.
            source_search_->clear();
            kind_filter_->setCurrentIndex(0);
            category_filter_->setCurrentIndex(0);
            usable_only_->setChecked(false);
            idx = sources_proxy_->mapFromSource(sources_model_->index(row, 0));
        }
        if (idx.isValid() && sources_view_->currentIndex() == idx)
            on_source_selected(); // already current: what a click on it does
        else
            select_source(kind, id);
        return true;
    }
    if (act == Act::Connection) {
        const int row = conns_model_->row_of(id);
        if (row < 0)
            return false;
        QModelIndex idx = conns_proxy_->mapFromSource(conns_model_->index(row, 0));
        if (!idx.isValid()) {
            conn_search_->clear();
            only_type_->setChecked(false);
            idx = conns_proxy_->mapFromSource(conns_model_->index(row, 0));
        }
        if (idx.isValid() && conns_view_->currentIndex() == idx)
            on_connection_selected();
        else
            select_connection(id);
        return true;
    }
    if (act == Act::SourcesList) {
        if (kind_index >= 0)
            kind_filter_->setCurrentIndex(kind_index);
        source_search_->setFocus(Qt::OtherFocusReason);
        source_search_->selectAll();
        return true;
    }
    conns_view_->setFocus(Qt::OtherFocusReason); // Act::ConnectionsList
    return true;
}

} // namespace fincept::screens::datasources
