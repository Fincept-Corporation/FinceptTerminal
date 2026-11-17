// ConnectionDetailView.cpp — see ConnectionDetailView.h.

#include "screens/data_sources/ConnectionDetailView.h"

#include "screens/data_sources/ConnectionForm.h"
#include "screens/data_sources/ConnectorRegistry.h"
#include "screens/data_sources/DataSourcesHelpers.h"
#include "services/data_sources/ConnectionService.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens::datasources {

namespace dsrc = fincept::services::datasources;
namespace dm = fincept::services::datamapping;

namespace {
constexpr int kTabSettings = 0;
constexpr int kTabBrowse = 1;
constexpr int kTabData = 2;
} // namespace

ConnectionDetailView::ConnectionDetailView(QWidget* parent) : QWidget(parent) {
    build();
}

void ConnectionDetailView::build() {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* header = new QWidget(this);
    set_look(header, "bar");
    header->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(10);
    code_ = new QLabel(header);
    set_look(code_, "code");
    code_->setAlignment(Qt::AlignCenter);
    const int side = fontMetrics().height() * 2 + 4;
    code_->setFixedSize(side, side);
    hl->addWidget(code_);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    title_ = new QLabel(header);
    set_look(title_, "h1");
    title_->setTextFormat(Qt::PlainText);
    subtitle_ = new QLabel(header);
    set_look(subtitle_, "muted");
    subtitle_->setTextFormat(Qt::PlainText);
    titles->addWidget(title_);
    titles->addWidget(subtitle_);
    hl->addLayout(titles, 1);
    test_btn_ = new QPushButton(tr("TEST"), header);
    test_btn_->setToolTip(tr("Send a real request with these settings (a handshake, a login and a small query — "
                             "not just a socket)"));
    save_btn_ = new QPushButton(tr("SAVE"), header);
    set_look(save_btn_, "primary");
    dup_btn_ = new QPushButton(tr("DUPLICATE"), header);
    delete_btn_ = new QPushButton(tr("DELETE"), header);
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the connection
    set_look(delete_btn_, "danger");
    hl->addWidget(test_btn_);
    hl->addWidget(save_btn_);
    hl->addWidget(dup_btn_);
    hl->addWidget(delete_btn_);
    vl->addWidget(header);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    status_->setContentsMargins(12, 6, 12, 6);
    vl->addWidget(status_);
    support_banner_ = new QLabel(this);
    set_look(support_banner_, "banner");
    support_banner_->setWordWrap(true);
    support_banner_->setTextFormat(Qt::PlainText);
    support_banner_->hide();
    vl->addWidget(support_banner_);
    banner_ = new QLabel(this);
    set_look(banner_, "banner_info");
    banner_->setWordWrap(true);
    banner_->setTextFormat(Qt::PlainText);
    banner_->hide();
    vl->addWidget(banner_);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);

    // SETTINGS
    form_scroll_ = new QScrollArea(tabs_);
    form_scroll_->setWidgetResizable(true);
    form_scroll_->setFrameShape(QFrame::NoFrame);
    auto* form_host = new QWidget(form_scroll_);
    auto* fvl = new QVBoxLayout(form_host);
    fvl->setContentsMargins(14, 12, 14, 12);
    form_ = new ConnectionForm(form_host);
    fvl->addWidget(form_);
    fvl->addStretch(1);
    form_scroll_->setWidget(form_host);
    tabs_->addTab(form_scroll_, tr("SETTINGS"));

    // BROWSE
    auto* browse = new QWidget(tabs_);
    auto* bvl = new QVBoxLayout(browse);
    bvl->setContentsMargins(0, 0, 0, 0);
    bvl->setSpacing(0);
    auto* brow = new QHBoxLayout;
    brow->setContentsMargins(12, 6, 12, 6);
    list_info_ = new QLabel(browse);
    set_look(list_info_, "muted");
    brow->addWidget(list_info_, 1);
    auto* list_btn = new QPushButton(tr("LIST"), browse);
    brow->addWidget(list_btn);
    bvl->addLayout(brow);
    list_stack_ = new QStackedWidget(browse);
    list_state_ = new fincept::ui::EmptyState(list_stack_);
    list_skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, list_stack_);
    list_view_ = new QTableView(list_stack_);
    list_model_ = new TabularTableModel(this);
    list_view_->setModel(list_model_);
    list_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    list_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_view_->setAlternatingRowColors(true);
    list_view_->verticalHeader()->setVisible(false);
    list_view_->horizontalHeader()->setStretchLastSection(true);
    list_view_->setToolTip(tr("Double-click to load its rows"));
    fincept::ui::metrics::follow_row_height(list_view_, 24);
    list_stack_->addWidget(list_state_);
    list_stack_->addWidget(list_skeleton_);
    list_stack_->addWidget(list_view_);
    bvl->addWidget(list_stack_, 1);
    tabs_->addTab(browse, tr("BROWSE"));

    // DATA
    auto* data = new QWidget(tabs_);
    auto* dvl = new QVBoxLayout(data);
    dvl->setContentsMargins(0, 0, 0, 0);
    dvl->setSpacing(0);
    auto* drow = new QHBoxLayout;
    drow->setContentsMargins(12, 6, 12, 6);
    drow->setSpacing(6);
    entity_ = new QLineEdit(data);
    entity_->setClearButtonEnabled(true);
    drow->addWidget(entity_, 2);
    query_label_ = new QLabel(data);
    set_look(query_label_, "field");
    drow->addWidget(query_label_);
    query_ = new QLineEdit(data);
    query_->setClearButtonEnabled(true);
    drow->addWidget(query_, 3);
    auto* rows_lbl = new QLabel(tr("ROWS"), data);
    set_look(rows_lbl, "field");
    drow->addWidget(rows_lbl);
    limit_ = new QSpinBox(data);
    limit_->setRange(1, 50000);
    limit_->setValue(200);
    limit_->setAccelerated(true);
    drow->addWidget(limit_);
    fetch_btn_ = new QPushButton(tr("FETCH"), data);
    set_look(fetch_btn_, "primary");
    drow->addWidget(fetch_btn_);
    dvl->addLayout(drow);
    data_stack_ = new QStackedWidget(data);
    data_state_ = new fincept::ui::EmptyState(data_stack_);
    data_loading_ = new QWidget(data_stack_);
    auto* lvl = new QVBoxLayout(data_loading_);
    lvl->setContentsMargins(0, 0, 0, 8);
    data_skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, data_loading_);
    lvl->addWidget(data_skeleton_, 1);
    auto* cancel_row = new QHBoxLayout;
    cancel_row->addStretch(1);
    auto* cancel_btn = new QPushButton(tr("CANCEL"), data_loading_);
    cancel_row->addWidget(cancel_btn);
    cancel_row->addStretch(1);
    lvl->addLayout(cancel_row);
    result_ = new ResultPane(QStringLiteral("Data Sources"), data_stack_);
    data_stack_->addWidget(data_state_);
    data_stack_->addWidget(data_loading_);
    data_stack_->addWidget(result_);
    dvl->addWidget(data_stack_, 1);
    tabs_->addTab(data, tr("DATA"));
    vl->addWidget(tabs_, 1);

    retry_timer_ = new QTimer(this);
    retry_timer_->setInterval(1000);
    connect(retry_timer_, &QTimer::timeout, this, [this]() {
        if (--retry_left_ <= 0) {
            retry_timer_->stop();
            data_state_->set_primary_action(tr("RETRY"));
            data_state_->set_primary_enabled(true);
        } else {
            data_state_->set_primary_action(tr("RETRY IN %1 s").arg(retry_left_));
        }
    });

    connect(test_btn_, &QPushButton::clicked, this, &ConnectionDetailView::on_test);
    connect(save_btn_, &QPushButton::clicked, this, &ConnectionDetailView::on_save);
    connect(delete_btn_, &QPushButton::clicked, this, &ConnectionDetailView::on_delete);
    connect(dup_btn_, &QPushButton::clicked, this, [this]() {
        if (!id_.isEmpty())
            emit duplicate_requested(id_);
    });
    connect(list_btn, &QPushButton::clicked, this, &ConnectionDetailView::on_list);
    connect(list_state_, &fincept::ui::EmptyState::primary_clicked, this, &ConnectionDetailView::on_list);
    connect(list_view_, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        const auto& t = list_model_->table();
        int c = t.column_index(QStringLiteral("name"));
        if (c < 0)
            c = 0;
        const QString entity = dsrc::cell_text(t.cell(idx.row(), c));
        if (entity.isEmpty())
            return;
        entity_->setText(entity);
        tabs_->setCurrentIndex(kTabData);
        on_fetch(entity);
    });
    connect(fetch_btn_, &QPushButton::clicked, this, [this]() { on_fetch(); });
    connect(entity_, &QLineEdit::returnPressed, this, [this]() { on_fetch(); });
    connect(query_, &QLineEdit::returnPressed, this, [this]() { on_fetch(); });
    connect(cancel_btn, &QPushButton::clicked, this, [this]() {
        if (fetch_job_)
            dsrc::ConnectionService::instance().cancel(fetch_job_);
    });
    connect(data_state_, &fincept::ui::EmptyState::primary_clicked, this, [this]() { run_action(data_primary_); });
    connect(data_state_, &fincept::ui::EmptyState::secondary_clicked, this, [this]() { run_action(data_secondary_); });
    connect(form_, &ConnectionForm::edited, this, [this]() {
        if (!id_.isEmpty())
            set_status_line(tr("Edited — not saved. TEST checks the typed settings; SAVE keeps them."), "warn");
    });
}

bool ConnectionDetailView::has_unsaved_edits() const {
    return form_ && form_->is_dirty();
}

void ConnectionDetailView::set_type(const QString& type_id) {
    type_id_ = type_id;
    connector_ = find_connector_config(type_id);
    support_ = dsrc::effective_support(connector_ ? connector_->id : type_id);
    form_->set_connector(connector_);
    code_->setText(connector_ ? connector_code(*connector_) : QStringLiteral("?"));
    const QString label = support_.entity_label.isEmpty() ? tr("Entity") : support_.entity_label;
    entity_->setPlaceholderText(tr("%1 (blank = default sample)").arg(label));
    query_label_->setText(support_.query_label.toUpper());
    query_label_->setVisible(!support_.query_label.isEmpty());
    query_->setVisible(!support_.query_label.isEmpty());
    query_->setPlaceholderText(support_.query_hint);
    tabs_->setTabEnabled(kTabBrowse, support_.supported && support_.can_list);
    tabs_->setTabToolTip(kTabBrowse, support_.can_list ? QString() : tr("This type has no schema browser"));
    tabs_->setTabEnabled(kTabData, support_.supported && support_.can_fetch);
    tabs_->setTabToolTip(kTabData, support_.can_fetch ? QString() : tr("This type does not return rows"));
    if (!support_.supported) {
        support_banner_->setText(support_.fix.isEmpty() ? tr("Not available in this build: %1").arg(support_.reason)
                                                        : tr("Not available in this build: %1  What works: %2")
                                                              .arg(support_.reason, support_.fix));
        support_banner_->show();
    } else {
        support_banner_->hide();
    }
}

void ConnectionDetailView::reset_tabs() {
    ++gen_;
    cancel_jobs();
    retry_timer_->stop();
    banner_->hide();
    list_model_->clear();
    list_info_->setText(support_.entity_label.isEmpty() ? QString()
                                                        : tr("%1s of this connection").arg(support_.entity_label));
    list_state_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("Browse this connection"),
        tr("Lists what it exposes (%1s) — double-click one to load its rows.")
            .arg(support_.entity_label.isEmpty() ? tr("entitie") : support_.entity_label.toLower()),
        tr("LIST")));
    list_stack_->setCurrentWidget(list_state_);
    entity_->clear();
    query_->clear();
    result_->clear();
    data_primary_ = ErrorAction::Retry;
    data_secondary_ = ErrorAction::None;
    data_state_->apply(fincept::ui::EmptyState::no_data_spec(
        tr("Load a sample"), tr("Fetches the first rows — or the %1 / query above — from this connection.")
                                 .arg(support_.entity_label.isEmpty() ? tr("entity") : support_.entity_label.toLower()),
        tr("LOAD SAMPLE")));
    data_stack_->setCurrentWidget(data_state_);
    tabs_->setCurrentIndex(kTabSettings);
}

void ConnectionDetailView::show_connection(const DataSource& ds) {
    const bool same = ds.id == id_ && !id_.isEmpty();
    ds_ = ds;
    id_ = ds.id;
    if (!same) {
        set_type(ds.provider);
        reset_tabs();
        form_->load(ds);
    }
    dup_btn_->setEnabled(true);
    delete_btn_->setEnabled(true);
    save_btn_->setText(tr("SAVE"));
    update_header();
}

void ConnectionDetailView::show_new(const QString& type_id, const DataSource* duplicate_of) {
    id_.clear();
    ds_ = DataSource();
    set_type(type_id);
    reset_tabs();
    if (duplicate_of)
        form_->load(*duplicate_of, true);
    else
        form_->load_defaults();
    dup_btn_->setEnabled(false);
    delete_btn_->setEnabled(false);
    save_btn_->setText(tr("SAVE CONNECTION"));
    update_header();
}

void ConnectionDetailView::update_from(const DataSource& ds) {
    if (ds.id != id_ || id_.isEmpty())
        return;
    ds_ = ds;
    if (!form_->is_dirty())
        update_header();
}

void ConnectionDetailView::update_header() {
    const QString type_name = connector_ ? connector_->name : type_id_;
    if (id_.isEmpty()) {
        title_->setText(form_->current_name().isEmpty() ? tr("New %1 connection").arg(type_name)
                                                        : tr("New: %1").arg(form_->current_name()));
        subtitle_->setText(connector_ ? QStringLiteral("%1 · %2").arg(type_name, category_label(connector_->category))
                                      : type_name);
        set_status_line(tr("Not saved yet. TEST sends a real request with the typed settings; SAVE keeps them."), "idle");
        return;
    }
    title_->setText(ds_.display_name.isEmpty() ? ds_.alias : ds_.display_name);
    subtitle_->setText(QStringLiteral("%1 · %2%3").arg(type_name, connector_ ? category_label(connector_->category) : ds_.category,
                                                       ds_.enabled ? QString() : tr(" · disabled")));
    if (ds_.last_status.isEmpty()) {
        set_status_line(tr("Not tested yet — TEST sends a real request (not a socket check)."), "idle");
    } else if (ds_.last_status == QLatin1String("ok")) {
        QString t = tr("● OK · tested %1").arg(relative_time_text(ds_.last_tested_at));
        if (ds_.last_latency_ms >= 0)
            t += tr(" · %1 ms").arg(ds_.last_latency_ms);
        if (!ds_.last_message.isEmpty())
            t += QStringLiteral(" · ") + ds_.last_message;
        set_status_line(t, "ok");
    } else {
        set_status_line(tr("● %1 · tested %2 · %3")
                            .arg(ds_.last_code.isEmpty() ? tr("ERROR") : ds_.last_code.toUpper(),
                                 relative_time_text(ds_.last_tested_at), ds_.last_message),
                        "error");
    }
    if (ds_.secrets_unreadable)
        show_banner(tr("Could not read secure storage: %1. The saved credentials are kept — type new values only "
                       "to replace them.")
                        .arg(ds_.secrets_error),
                    "banner_error");
}

void ConnectionDetailView::set_status_line(const QString& text, const char* look) {
    status_->setText(text);
    set_look(status_, look);
}

void ConnectionDetailView::show_banner(const QString& text, const char* look) {
    banner_->setText(text);
    set_look(banner_, look);
    banner_->setVisible(!text.isEmpty());
}

bool ConnectionDetailView::use_saved_config() const {
    return !id_.isEmpty() && !form_->is_dirty();
}

dsrc::SourceRequest ConnectionDetailView::unsaved_request(dsrc::SourceOp op) const {
    dsrc::SourceRequest req;
    req.op = op;
    req.type_id = connector_ ? connector_->id : type_id_;
    req.config = form_->current_config();
    req.timeout_ms = op == dsrc::SourceOp::Fetch ? 60000 : 20000;
    return req;
}

void ConnectionDetailView::cancel_jobs() {
    auto& svc = dsrc::ConnectionService::instance();
    for (quint64 j : {test_job_, list_job_, fetch_job_})
        if (j)
            svc.cancel(j);
    test_job_ = list_job_ = fetch_job_ = 0;
    test_btn_->setEnabled(true);
}

void ConnectionDetailView::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // A user request keeps running while the tab is hidden; nothing automatic runs here.
}

void ConnectionDetailView::on_test() {
    auto& svc = dsrc::ConnectionService::instance();
    const bool saved_cfg = use_saved_config();
    const quint64 gen = gen_;
    QPointer<ConnectionDetailView> self(this);
    auto cb = [self, gen, saved_cfg](const dsrc::SourceReply& reply) {
        if (!self || gen != self->gen_)
            return;
        self->test_job_ = 0;
        self->test_btn_->setEnabled(true);
        if (reply.error.cancelled) {
            self->update_header();
            return;
        }
        const QString where = saved_cfg ? QString() : tr(" (typed settings, not saved)");
        if (reply.ok) {
            self->set_status_line(tr("● OK%1 · just now · %2 ms%3")
                                      .arg(where)
                                      .arg(reply.latency_ms)
                                      .arg(reply.summary.isEmpty() ? QString() : QStringLiteral(" · ") + reply.summary),
                                  "ok");
            if (!reply.data.tables.isEmpty() && !reply.data.is_empty() && !self->result_->has_result()) {
                self->result_->set_result(reply.data, tr("%1 · sample").arg(self->form_->current_name()));
                self->data_stack_->setCurrentWidget(self->result_);
            }
        } else {
            QString t = tr("● %1%2 · %3").arg(dsrc::error_code_name(reply.error.code).toUpper(), where, reply.error.message);
            if (!reply.error.fix.isEmpty())
                t += QStringLiteral("  ") + reply.error.fix;
            self->set_status_line(t, "error");
        }
        if (!reply.notes.isEmpty())
            self->show_banner(reply.notes.join(QLatin1Char('\n')), "banner_info");
    };
    set_status_line(tr("Testing… (a real request — up to 20 s)"), "idle");
    test_btn_->setEnabled(false);
    test_job_ = saved_cfg ? svc.test_connection(id_, this, cb) : svc.run(unsaved_request(dsrc::SourceOp::Test), this, cb);
}

void ConnectionDetailView::on_save() {
    if (saving_ || !connector_)
        return;
    QJsonObject cfg;
    QString name;
    QString tags;
    QString error;
    bool enabled = true;
    if (!form_->collect(&cfg, &name, &enabled, &tags, &error)) {
        tabs_->setCurrentIndex(kTabSettings);
        show_banner(error, "banner_error");
        return;
    }
    DataSource ds = id_.isEmpty() ? DataSource() : ds_;
    if (ds.id.isEmpty())
        ds.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (ds.alias.isEmpty())
        ds.alias = connector_->id + QLatin1Char('_') + ds.id.left(8);
    ds.display_name = name;
    ds.description = connector_->description;
    ds.type = persistence_type(*connector_);
    ds.provider = connector_->id;
    ds.category = category_str(connector_->category);
    ds.config = QString::fromUtf8(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
    ds.enabled = enabled;
    ds.tags = tags;

    saving_ = true;
    save_btn_->setEnabled(false);
    show_banner(tr("Saving…"), "banner_info");
    QPointer<ConnectionDetailView> self(this);
    (void)QtConcurrent::run([self, ds]() {
        const auto r = DataSourceRepository::instance().save(ds);
        const QString err = r.is_err() ? QString::fromStdString(r.error()) : QString();
        QMetaObject::invokeMethod(
            qApp,
            [self, ds, err]() {
                if (!self)
                    return;
                self->saving_ = false;
                self->save_btn_->setEnabled(true);
                if (!err.isEmpty()) {
                    self->show_banner(tr("Not saved: %1").arg(err), "banner_error");
                    return;
                }
                self->id_ = ds.id;
                self->ds_ = ds;
                self->form_->mark_clean();
                self->dup_btn_->setEnabled(true);
                self->delete_btn_->setEnabled(true);
                self->save_btn_->setText(tr("SAVE"));
                self->show_banner(tr("Saved. TEST checks it for real."), "banner_info");
                self->update_header();
                emit self->saved(ds.id);
            },
            Qt::QueuedConnection);
    });
}

void ConnectionDetailView::on_delete() {
    if (id_.isEmpty())
        return;
    const QString name = ds_.display_name.isEmpty() ? ds_.alias : ds_.display_name;
    if (QMessageBox::question(this, tr("Delete connection"),
                              tr("Delete \"%1\"? Its stored credentials are removed too.").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    const QString id = id_;
    ++gen_;
    cancel_jobs();
    QPointer<ConnectionDetailView> self(this);
    (void)QtConcurrent::run([self, id]() {
        const auto r = DataSourceRepository::instance().remove(id);
        const QString err = r.is_err() ? QString::fromStdString(r.error()) : QString();
        QMetaObject::invokeMethod(
            qApp,
            [self, id, err]() {
                if (!self)
                    return;
                if (!err.isEmpty()) {
                    self->show_banner(tr("Not deleted: %1").arg(err), "banner_error");
                    return;
                }
                emit self->deleted(id);
            },
            Qt::QueuedConnection);
    });
}

void ConnectionDetailView::on_list() {
    auto& svc = dsrc::ConnectionService::instance();
    if (list_job_)
        svc.cancel(list_job_);
    const quint64 gen = gen_;
    QPointer<ConnectionDetailView> self(this);
    auto cb = [self, gen](const dsrc::SourceReply& reply) {
        if (!self || gen != self->gen_)
            return;
        self->list_job_ = 0;
        if (reply.error.cancelled) {
            self->list_stack_->setCurrentWidget(self->list_model_->rowCount() > 0
                                                    ? static_cast<QWidget*>(self->list_view_)
                                                    : static_cast<QWidget*>(self->list_state_));
            return;
        }
        if (!reply.ok) {
            self->show_list_error(reply.error);
            return;
        }
        const dsrc::DataTable t = reply.data.tables.value(0);
        self->list_model_->set_table(t);
        self->list_view_->resizeColumnsToContents();
        self->list_info_->setText(tr("%1 %2(s)%3")
                                      .arg(t.row_count())
                                      .arg(self->support_.entity_label.isEmpty() ? tr("entry")
                                                                                 : self->support_.entity_label.toLower(),
                                           reply.data.truncated ? tr(" · more exist") : QString()));
        if (t.row_count() == 0) {
            self->list_state_->apply(fincept::ui::EmptyState::no_data_spec(
                tr("Nothing to list"), tr("The connection answered but exposes no %1s.")
                                           .arg(self->support_.entity_label.toLower()),
                tr("LIST AGAIN")));
            self->list_stack_->setCurrentWidget(self->list_state_);
        } else {
            self->list_stack_->setCurrentWidget(self->list_view_);
        }
    };
    list_skeleton_->set_message(tr("Listing…"));
    list_stack_->setCurrentWidget(list_skeleton_);
    list_job_ = use_saved_config() ? svc.list_connection(id_, this, cb) : svc.run(unsaved_request(dsrc::SourceOp::List), this, cb);
}

void ConnectionDetailView::show_list_error(const dsrc::SourceError& e) {
    ErrorPage p = error_page(e, false);
    p.spec.primary = tr("LIST AGAIN");
    p.spec.secondary.clear();
    list_state_->apply(p.spec);
    list_stack_->setCurrentWidget(list_state_);
}

void ConnectionDetailView::on_fetch(const QString& entity_override) {
    auto& svc = dsrc::ConnectionService::instance();
    if (fetch_job_)
        svc.cancel(fetch_job_);
    retry_timer_->stop();
    const QString entity = entity_override.isEmpty() ? entity_->text().trimmed() : entity_override;
    const QString query = query_->isVisible() ? query_->text().trimmed() : QString();
    const int limit = limit_->value();
    const bool saved_cfg = use_saved_config();
    const QString id = id_;
    const QString name = form_->current_name();
    const quint64 gen = gen_;
    QPointer<ConnectionDetailView> self(this);
    auto cb = [self, gen, saved_cfg, id, entity, query, name](const dsrc::SourceReply& reply) {
        if (!self || gen != self->gen_)
            return;
        self->fetch_job_ = 0;
        if (reply.error.cancelled) {
            self->data_stack_->setCurrentWidget(self->result_->has_result() ? static_cast<QWidget*>(self->result_)
                                                                            : static_cast<QWidget*>(self->data_state_));
            return;
        }
        if (!reply.ok) {
            self->show_fetch_error(reply.error);
            return;
        }
        self->result_->set_result(reply.data, entity.isEmpty() ? tr("%1 · sample").arg(name)
                                                               : QStringLiteral("%1 · %2").arg(name, entity));
        if (saved_cfg) {
            dm::SourceRef src;
            src.kind = QStringLiteral("connection");
            src.ref = id;
            src.entity = entity;
            if (!query.isEmpty())
                src.params.insert(QStringLiteral("query"), query);
            self->result_->set_mapping_source(src, entity.isEmpty() ? name : QStringLiteral("%1 · %2").arg(name, entity));
        } else {
            self->result_->set_mapping_source(dm::SourceRef(), QString());
        }
        QStringList notes = reply.notes;
        if (!saved_cfg)
            notes.prepend(tr("Fetched with the typed settings — SAVE the connection to send it to Data Mapping."));
        self->result_->set_note(notes.join(QLatin1Char('\n')));
        self->data_stack_->setCurrentWidget(self->result_);
    };
    data_skeleton_->set_message(entity.isEmpty() ? tr("Fetching a sample…") : tr("Fetching %1…").arg(entity));
    data_stack_->setCurrentWidget(data_loading_);
    if (saved_cfg) {
        fetch_job_ = svc.fetch_connection(id_, entity, query, limit, this, cb);
    } else {
        dsrc::SourceRequest req = unsaved_request(dsrc::SourceOp::Fetch);
        req.entity = entity;
        req.query = query;
        req.limit = limit;
        fetch_job_ = svc.run(req, this, cb);
    }
}

void ConnectionDetailView::show_fetch_error(const dsrc::SourceError& e) {
    const ErrorPage p = error_page(e, false);
    data_primary_ = p.primary;
    data_secondary_ = p.secondary;
    data_state_->apply(p.spec);
    if (p.retry_after_s > 0 && p.primary == ErrorAction::Retry) {
        retry_left_ = p.retry_after_s;
        data_state_->set_primary_action(tr("RETRY IN %1 s").arg(retry_left_));
        data_state_->set_primary_enabled(false);
        retry_timer_->start();
    }
    data_stack_->setCurrentWidget(data_state_);
}

void ConnectionDetailView::run_action(ErrorAction action) {
    switch (action) {
        case ErrorAction::Retry:
            on_fetch();
            break;
        case ErrorAction::EditSettings:
            tabs_->setCurrentIndex(kTabSettings);
            break;
        case ErrorAction::EditInputs:
            entity_->setFocus(Qt::OtherFocusReason);
            break;
        case ErrorAction::AddKey:
            open_credentials(QString(), this);
            break;
        case ErrorAction::None:
            break;
    }
}

} // namespace fincept::screens::datasources
