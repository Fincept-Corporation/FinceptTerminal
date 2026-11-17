// src/screens/gov_data/GovDataAustraliaPanel.cpp
#include "screens/gov_data/GovDataAustraliaPanel.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovTableModel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>

namespace fincept::screens {
namespace {
const QString kGovDataAustraliaScript = QStringLiteral("datagov_au_api.py");
const QString kGovAUPrefix = QStringLiteral("gov.au");
// data.gov.au identity colour as it reads on the dark theme; always drawn through colors::legible().
constexpr const char* kGovDataAustraliaColor = "#0EA5E9";
constexpr int kGovAURefMaxAgeS = 24 * 3600;   // agencies, a dataset's resources
constexpr int kGovAUDataMaxAgeS = 6 * 3600;   // an agency's datasets, search results
constexpr int kGovAURecentMaxAgeS = 3600;     // "recently updated": changes through the day
constexpr int kGovAURecentRows = 20;
} // namespace

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataAustraliaPanel::GovDataAustraliaPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataAustraliaColor)); // once, before children
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataAustraliaPanel::on_result);
    // The tables are models whose ink is read from the live theme: no recolour walk on a theme change.

    // The search term and the page on screen (agency, dataset, list) come back with the panel.
    memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("australia"), this);
    memory_->track(QStringLiteral("search"), search_input_);
    memory_->restore();
    const QString first_view = restore_navigation();

    go(first_view.isEmpty() ? QStringLiteral("agencies") : first_view, false); // loads when first shown
}

void GovDataAustraliaPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── UI Construction ──────────────────────────────────────────────────────────

void GovDataAustraliaPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Breadcrumb bar
    breadcrumb_ = new QWidget(this);
    breadcrumb_->setObjectName("govBreadcrumb");
    breadcrumb_->setMinimumHeight(24);
    auto* bcl = new QHBoxLayout(breadcrumb_);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(4);
    breadcrumb_label_ = new QLabel(breadcrumb_);
    breadcrumb_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(breadcrumb_label_);
    bcl->addStretch(1);
    as_of_label_ = gov::make_as_of_label(breadcrumb_);
    bcl->addWidget(as_of_label_);
    bcl->addSpacing(12);
    row_count_label_ = new QLabel(breadcrumb_);
    row_count_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    build_pane(this);
    root->addWidget(pane_, 1);
}

void GovDataAustraliaPanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/australia"), as_of_label_);
    pane_->set_persist_limit(1024 * 1024); // dataset lists carry their resources (no fetch to open one)
    content_stack_ = new QStackedWidget(pane_->content_parent());

    using Ink = GovTableModel::Ink;
    const QString color = QString::fromLatin1(kGovDataAustraliaColor);
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };
    const auto field_or_dash = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) {
            const QString s = r.value(k).toString();
            return s.isEmpty() ? fmt::placeholder() : s;
        };
    };

    // Agencies table: NAME | DESCRIPTION | DATASETS (headers set in retranslateUi)
    agencies_model_ = gov::make_model(color, this);
    agencies_model_->set_columns({
        {.text = field("name"), .tooltip = field("name"), .ink = Ink::Accent},
        {.text = [](const QJsonObject& r) {
             const QString desc = r.value(QStringLiteral("desc")).toString();
             return desc.length() > 80 ? desc.left(77) + QStringLiteral("…") : desc;
         },
         .tooltip = field("desc"),
         .ink = Ink::Secondary},
        // Dataset count (the script lists agencies from one facet query, largest first); -1 = unknown.
        {.text = [](const QJsonObject& r) {
             const int count = r.value(QStringLiteral("count")).toInt(-1);
             const QString text = count >= 0 ? fmt::quantity(count) : r.value(QStringLiteral("created")).toString();
             return text.isEmpty() ? fmt::placeholder() : text;
         },
         .align = Qt::AlignRight | Qt::AlignVCenter},
    });
    agencies_table_ = gov::make_table_view(content_stack_, agencies_model_);
    agencies_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    agencies_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    agencies_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    agencies_table_->setColumnWidth(0, 260);
    agencies_table_->setColumnWidth(2, 100);
    connect(agencies_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_agency_doubleclicked(i.row(), i.column()); });
    content_stack_->addWidget(agencies_table_);

    // Datasets table: TITLE | LICENSE | AUTHOR | RESOURCES | MODIFIED (headers set in retranslateUi)
    datasets_model_ = gov::make_model(color, this);
    datasets_model_->set_columns({
        {.text = field("title"), .tooltip = &gov::dataset_tooltip},
        {.text = field_or_dash("license"), .tooltip = field("license"), .ink = Ink::Secondary},
        {.text = field_or_dash("author"), .tooltip = field("author")},
        {.text = [](const QJsonObject& r) {
             const int files = r.value(QStringLiteral("files")).toInt(-1);
             return files >= 0 ? fmt::quantity(files) : fmt::placeholder();
         },
         .align = Qt::AlignCenter,
         .ink = Ink::Accent},
        {.text = field("modified"), .align = Qt::AlignCenter},
    });
    datasets_table_ = gov::make_table_view(content_stack_, datasets_model_);
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 130);
    datasets_table_->setColumnWidth(2, 140);
    datasets_table_->setColumnWidth(3, 85);
    datasets_table_->setColumnWidth(4, 100);
    connect(datasets_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_dataset_doubleclicked(i.row(), i.column()); });
    content_stack_->addWidget(datasets_table_);

    // Resources table: NAME | FORMAT | SIZE | OPEN (headers set in retranslateUi); the last column
    // opens the file (a click; a double-click or Enter on the row too).
    const auto has_url = [](const QJsonObject& r) { return !r.value(QStringLiteral("url")).toString().isEmpty(); };
    resources_model_ = gov::make_model(color, this);
    resources_model_->set_columns({
        {.text = field("name")},
        {.text = field_or_dash("format"), .align = Qt::AlignCenter, .ink = Ink::Accent},
        {.text = [](const QJsonObject& r) { return gov::file_size(r.value(QStringLiteral("size")).toDouble(0.0)); },
         .align = Qt::AlignRight | Qt::AlignVCenter},
        {.text = [has_url](const QJsonObject& r) {
             return has_url(r) ? GovDataAustraliaPanel::tr("↗ OPEN") : fmt::placeholder();
         },
         .tooltip = field("url"),
         .align = Qt::AlignCenter,
         .ink = Ink::Accent,
         .ink_if = has_url,
         .export_text = field("url")},
    });
    resources_table_ = gov::make_table_view(content_stack_, resources_model_);
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 3; ++c)
        resources_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 80);
    resources_table_->setColumnWidth(2, 90);
    resources_table_->setColumnWidth(3, 70);
    const auto open_resource = [this](int row) {
        const QString url = resources_model_->row(row).value(QStringLiteral("url")).toString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    };
    connect(resources_table_, &QTableView::clicked, this, [open_resource](const QModelIndex& i) {
        if (i.column() == 3)
            open_resource(i.row());
    });
    connect(resources_table_, &QTableView::activated, this, [open_resource](const QModelIndex& i) {
        if (i.column() != 3) // the OPEN cell already opened on the click
            open_resource(i.row());
    });
    content_stack_->addWidget(resources_table_);

    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        switch (kind_of(pane_->view())) {
            case Kind::Agencies:
                pane_->run();
                break;
            case Kind::Recent:
            case Kind::Agency:
                go(QStringLiteral("agencies"), true);
                break;
            case Kind::Search:
                go(QStringLiteral("recent"), true);
                break;
            case Kind::Resources:
                on_back();
                break;
        }
    });
    content_stack_->addWidget(no_data_page_);

    pane_->set_content(content_stack_);
    pane_->set_start([this](quint64 run) { start_run(run); });
    pane_->set_cancel([this](quint64 run) { gov::cancel_token(tokens_, run); });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
}

QWidget* GovDataAustraliaPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button — hidden until drilling in (text set in retranslateUi)
    back_btn_ = new QPushButton(bar);
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // Tab: Agencies (text set in retranslateUi)
    agencies_btn_ = new QPushButton(bar);
    agencies_btn_->setObjectName("govTabBtn");
    agencies_btn_->setCheckable(true);
    agencies_btn_->setChecked(true);
    agencies_btn_->setCursor(Qt::PointingHandCursor);
    connect(agencies_btn_, &QPushButton::clicked, this, [this]() { go(QStringLiteral("agencies"), true); });
    hl->addWidget(agencies_btn_);

    // Tab: Datasets — the last dataset list, else the recently updated datasets
    datasets_btn_ = new QPushButton(bar);
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() {
        if (last_list_view_.isEmpty())
            on_recent();
        else
            go(last_list_view_, true);
    });
    hl->addWidget(datasets_btn_);

    // Tab: Recent (text set in retranslateUi)
    recent_btn_ = new QPushButton(bar);
    recent_btn_->setObjectName("govTabBtn");
    recent_btn_->setCheckable(true);
    recent_btn_->setCursor(Qt::PointingHandCursor);
    connect(recent_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_recent);
    hl->addWidget(recent_btn_);

    hl->addSpacing(10);

    // Search input (placeholder set in retranslateUi)
    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("govSearch");
    search_input_->setMinimumWidth(140);
    search_input_->setMaximumWidth(320);
    search_input_->setMinimumHeight(26);
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataAustraliaPanel::on_search);
    hl->addWidget(search_input_, 1);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataAustraliaPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataAustraliaPanel::retranslateUi() {
    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (agencies_btn_)
        agencies_btn_->setText(tr("AGENCIES"));
    if (datasets_btn_)
        datasets_btn_->setText(tr("DATASETS"));
    if (recent_btn_)
        recent_btn_->setText(tr("RECENT"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search datasets…"));

    if (agencies_model_)
        agencies_model_->set_headers({tr("NAME"), tr("DESCRIPTION"), tr("DATASETS")});
    if (datasets_model_)
        datasets_model_->set_headers({tr("TITLE"), tr("LICENSE"), tr("AUTHOR"), tr("RESOURCES"), tr("MODIFIED")});
    if (resources_model_)
        resources_model_->set_headers({tr("NAME"), tr("FORMAT"), tr("SIZE"), tr("OPEN")});

    // Re-render the breadcrumb so its current path picks up the new language.
    if (pane_)
        update_breadcrumb();
}

// ── Views and runs ───────────────────────────────────────────────────────────

GovDataAustraliaPanel::Kind GovDataAustraliaPanel::kind_of(const QString& key) {
    if (key == QLatin1String("recent"))
        return Kind::Recent;
    if (key.startsWith(QLatin1String("agency:")))
        return Kind::Agency;
    if (key.startsWith(QLatin1String("search:")))
        return Kind::Search;
    if (key.startsWith(QLatin1String("res:")))
        return Kind::Resources;
    return Kind::Agencies;
}

QString GovDataAustraliaPanel::arg_of(const QString& key) {
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    return colon < 0 ? QString() : key.mid(colon + 1);
}

bool GovDataAustraliaPanel::valid_view(const QString& key) {
    switch (kind_of(key)) {
        case Kind::Agencies:
            return key == QLatin1String("agencies");
        case Kind::Recent:
            return true;
        case Kind::Agency:
        case Kind::Search:
        case Kind::Resources:
            return !arg_of(key).isEmpty();
    }
    return false;
}

QString GovDataAustraliaPanel::restore_navigation() {
    if (!memory_)
        return {};
    const QVariantMap labels = memory_->value(QStringLiteral("labels")).toMap();
    for (auto it = labels.constBegin(); it != labels.constEnd(); ++it) {
        const QString label = it.value().toString();
        if (!it.key().isEmpty() && !label.isEmpty())
            labels_.insert(it.key(), label);
    }
    const QString list = memory_->value(QStringLiteral("list")).toString();
    const Kind list_kind = kind_of(list);
    if (valid_view(list) && (list_kind == Kind::Recent || list_kind == Kind::Agency || list_kind == Kind::Search))
        last_list_view_ = list;
    const QString view = memory_->value(QStringLiteral("view")).toString();
    return valid_view(view) ? view : QString();
}

void GovDataAustraliaPanel::remember_navigation() {
    const QString key = pane_ ? pane_->view() : QString();
    if (!memory_ || !valid_view(key))
        return;
    memory_->set_value(QStringLiteral("view"), key);
    memory_->set_value(QStringLiteral("list"), last_list_view_.isEmpty() ? QVariant() : QVariant(last_list_view_));
    QVariantMap labels; // only the names the remembered pages need (breadcrumb, captions)
    for (const QString& k : {key, last_list_view_}) {
        const QString label = labels_.value(k);
        if (!k.isEmpty() && !label.isEmpty())
            labels.insert(k, label);
    }
    memory_->set_value(QStringLiteral("labels"), labels.isEmpty() ? QVariant() : QVariant(labels));
}

void GovDataAustraliaPanel::go(const QString& key, bool user, bool force) {
    const Kind kind = kind_of(key);
    const QString arg = arg_of(key);
    if (kind == Kind::Recent || kind == Kind::Agency || kind == Kind::Search)
        last_list_view_ = key;
    pane_->set_view_max_age(key, kind == Kind::Agencies || kind == Kind::Resources ? kGovAURefMaxAgeS
                                 : kind == Kind::Recent                            ? kGovAURecentMaxAgeS
                                                                                   : kGovAUDataMaxAgeS);
    pane_->set_view_skeleton(key, kind == Kind::Agencies ? LoadingSkeleton::Shape::List : LoadingSkeleton::Shape::Table);
    switch (kind) {
        case Kind::Agencies:
            pane_->set_loading_text(tr("Loading Australian Government agencies…"));
            break;
        case Kind::Recent:
            pane_->set_loading_text(tr("Loading recently updated datasets from data.gov.au…"));
            break;
        case Kind::Agency:
            pane_->set_loading_text(tr("Loading datasets for \"%1\"…").arg(labels_.value(key, arg)));
            break;
        case Kind::Search:
            pane_->set_loading_text(tr("Searching for \"%1\"…").arg(arg));
            break;
        case Kind::Resources:
            pane_->set_loading_text(tr("Loading resources…"));
            break;
    }
    pane_->set_view(key);
    update_toolbar_state();
    update_breadcrumb();
    const bool running_here = pane_->is_running() && pane_->running_view() == key;
    if (force || (user && !pane_->has_result() && !running_here))
        pane_->run();
    remember_navigation();
}

void GovDataAustraliaPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(QStringLiteral("agencies"), false);
}

void GovDataAustraliaPanel::start_run(quint64 run) {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString command;
    QStringList args;
    switch (kind_of(key)) {
        case Kind::Agencies:
            command = QStringLiteral("organizations");
            break;
        case Kind::Recent:
            command = QStringLiteral("recent");
            args << QString::number(kGovAURecentRows);
            break;
        case Kind::Agency:
            command = QStringLiteral("org-datasets");
            args << arg << QStringLiteral("100");
            break;
        case Kind::Search:
            command = QStringLiteral("search");
            args << arg << QStringLiteral("50");
            break;
        case Kind::Resources: {
            // Resources come embedded in the dataset lists — no fetch when the list is at hand.
            const auto it = dataset_resources_.constFind(arg);
            if (it != dataset_resources_.constEnd()) {
                pane_->deliver(run, gov::list_payload(QStringLiteral("resources"), it.value()));
                return;
            }
            command = QStringLiteral("dataset-details");
            args << arg;
            break;
        }
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(kGovDataAustraliaScript, command, args,
                                                 gov::request_id(kGovAUPrefix, run), token);
}

void GovDataAustraliaPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovAUPrefix);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const Kind kind = kind_of(pane_->running_view());
    const QString payload_kind = kind == Kind::Agencies    ? QStringLiteral("agencies")
                                 : kind == Kind::Resources ? QStringLiteral("resources")
                                                           : QStringLiteral("datasets");
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data")) {
            pane_->deliver(run, gov::list_payload(payload_kind, QJsonArray()));
            return;
        }
        gov::fail_pane(pane_, run, result, QStringLiteral("data.gov.au"));
        LOG_DEBUG("GovDataAustraliaPanel", "Request " + request_id + " failed: " + result.error.left(200));
        return;
    }

    // Shaped on the shaping thread (a dataset list carries every dataset's resources); dropped when
    // superseded.
    gov::shape_async(
        this, [kind, data = result.data]() { return shape(kind, data); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run))
                pane_->deliver(run, payload);
        });
}

QJsonObject GovDataAustraliaPanel::shape(Kind kind, const QJsonObject& data) {
    if (kind == Kind::Agencies) {
        // {data:[{id, name, title, description, created, dataset_count, ...}]} — largest first
        QJsonArray agencies;
        for (const QJsonValue& v : gov::list_rows(data)) {
            const QJsonObject o = v.toObject();
            QString name;
            for (const char* k : {"display_name", "title", "name"}) {
                name = gov::text(o.value(QLatin1String(k)));
                if (!name.isEmpty())
                    break;
            }
            const QString id = gov::text(o.value("id")).isEmpty() ? gov::text(o.value("name")) : gov::text(o.value("id"));
            agencies.append(QJsonObject{{"name", name.isEmpty() ? id : name},
                                        {"id", id},
                                        {"desc", gov::text(o.value("description")).left(300)},
                                        {"count", gov::integer(o.value("dataset_count"))},
                                        {"created", gov::text(o.value("created")).left(10)}});
        }
        return gov::list_payload(QStringLiteral("agencies"), agencies);
    }
    if (kind == Kind::Resources) {
        // dataset-details: {data: {..., resources: [...]}}
        const QJsonArray resources = data.value("data").toObject().value("resources").toArray();
        return gov::list_payload(QStringLiteral("resources"), gov::compact_resources(resources));
    }
    // org-datasets and search: {data:{datasets:[...], count:N}}; recent may be a flat array.
    int total = -1;
    const QJsonArray rows = gov::list_rows(data, &total);
    return gov::list_payload(QStringLiteral("datasets"), gov::compact_datasets(rows, true), total);
}

// ── Render ────────────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    const QJsonArray rows = p.value("rows").toArray();
    const QString key = pane_->view();
    if (rows.isEmpty()) {
        row_count_label_->clear();
        show_no_data(kind_of(key), arg_of(key));
        return;
    }
    if (kind == QLatin1String("agencies")) {
        populate_agencies(rows);
        content_stack_->setCurrentWidget(agencies_table_);
    } else if (kind == QLatin1String("datasets")) {
        populate_datasets(rows, p.value("total").toInt(int(rows.size())));
        content_stack_->setCurrentWidget(datasets_table_);
    } else if (kind == QLatin1String("resources")) {
        populate_resources(rows);
        content_stack_->setCurrentWidget(resources_table_);
    }
}

void GovDataAustraliaPanel::show_no_data(Kind kind, const QString& arg) {
    EmptyState::Spec spec;
    switch (kind) {
        case Kind::Agencies:
            spec = EmptyState::no_data_spec(tr("No agencies listed"),
                                            tr("data.gov.au returned an empty list. Try again later."), tr("RELOAD"));
            break;
        case Kind::Recent:
            spec = EmptyState::no_data_spec(tr("No recently updated datasets"),
                                            tr("data.gov.au lists no recent datasets right now."), tr("SHOW AGENCIES"));
            break;
        case Kind::Agency:
            spec = EmptyState::no_data_spec(tr("No datasets for %1").arg(labels_.value(pane_->view(), arg)),
                                            tr("This agency has nothing on data.gov.au."), tr("BACK TO AGENCIES"));
            break;
        case Kind::Search:
            spec = EmptyState::no_data_spec(tr("No datasets match \"%1\"").arg(arg),
                                            tr("Try a shorter or a different term."), tr("SHOW RECENT DATASETS"));
            break;
        case Kind::Resources:
            spec = EmptyState::no_data_spec(tr("No files in this dataset"),
                                            tr("The dataset lists no downloadable resources."), tr("BACK"));
            break;
    }
    spec.glyph = QStringLiteral("∅");
    gov::show_no_data(content_stack_, no_data_page_, spec);
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataAustraliaPanel::populate_agencies(const QJsonArray& json) {
    gov::show_rows(agencies_table_, agencies_model_, json); // a model reset: cells are formatted when shown
    row_count_label_->setText(tr("%1 agencies").arg(fmt::quantity(json.size())));
}

void GovDataAustraliaPanel::populate_datasets(const QJsonArray& json, int total_count) {
    // Resources are embedded: kept per dataset so opening one needs no fetch.
    for (const QJsonValue& v : json) {
        const QJsonObject obj = v.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            dataset_resources_.insert(id, obj.value(QStringLiteral("resources")).toArray());
    }
    gov::show_rows(datasets_table_, datasets_model_, json);
    row_count_label_->setText(
        tr("Showing %1 of %2").arg(fmt::quantity(json.size()), fmt::quantity(std::max<int>(total_count, json.size()))));
}

void GovDataAustraliaPanel::populate_resources(const QJsonArray& resources) {
    gov::show_rows(resources_table_, resources_model_, resources);
    row_count_label_->setText(tr("%1 files").arg(fmt::quantity(resources.size())));
}

// ── Navigation slots ──────────────────────────────────────────────────────────

void GovDataAustraliaPanel::on_agency_doubleclicked(int row, int /*col*/) {
    const QJsonObject agency = agencies_model_->row(row);
    const QString id = agency.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("agency:") + id;
    labels_.insert(key, agency.value(QStringLiteral("name")).toString());
    go(key, true);
}

void GovDataAustraliaPanel::on_dataset_doubleclicked(int row, int /*col*/) {
    const QJsonObject dataset = datasets_model_->row(row);
    const QString id = dataset.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("res:") + id;
    labels_.insert(key, dataset.value(QStringLiteral("title")).toString());
    go(key, true);
}

void GovDataAustraliaPanel::on_fetch() {
    const QString query = search_input_->text().trimmed();
    if (!query.isEmpty())
        go(QStringLiteral("search:") + query, true, true);
    else
        pane_->run(); // reload what is on screen
}

void GovDataAustraliaPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty())
        return;
    go(QStringLiteral("search:") + query, true);
}

void GovDataAustraliaPanel::on_recent() {
    go(QStringLiteral("recent"), true);
}

void GovDataAustraliaPanel::on_back() {
    const Kind kind = kind_of(pane_->view());
    if (kind == Kind::Resources && !last_list_view_.isEmpty())
        go(last_list_view_, true);
    else
        go(QStringLiteral("agencies"), true);
}

// ── Toolbar / breadcrumb state ────────────────────────────────────────────────

void GovDataAustraliaPanel::update_toolbar_state() {
    const Kind kind = kind_of(pane_->view());
    back_btn_->setVisible(kind != Kind::Agencies);
    agencies_btn_->setChecked(kind == Kind::Agencies);
    datasets_btn_->setChecked(kind == Kind::Agency || kind == Kind::Search || kind == Kind::Resources);
    recent_btn_->setChecked(kind == Kind::Recent);
}

void GovDataAustraliaPanel::update_breadcrumb() {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString text;
    switch (kind_of(key)) {
        case Kind::Agencies:
            text = tr("All Agencies");
            break;
        case Kind::Recent:
            text = tr("Recent Datasets");
            break;
        case Kind::Agency:
            text = tr("All Agencies  ›  %1  ›  Datasets").arg(labels_.value(key, arg));
            break;
        case Kind::Search:
            text = tr("Search Results  ›  \"%1\"").arg(arg);
            break;
        case Kind::Resources:
            text = tr("Datasets  ›  %1  ›  Resources").arg(labels_.value(key).left(80));
            break;
    }
    breadcrumb_label_->setText(text);
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataAustraliaPanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == agencies_table_)
        export_table_to_csv(agencies_table_, "au_agencies.csv", this);
    else if (page == datasets_table_)
        export_table_to_csv(datasets_table_, kind_of(pane_->view()) == Kind::Recent ? "au_recent_datasets.csv"
                                                                                    : "au_datasets.csv",
                            this);
    else if (page == resources_table_)
        export_table_to_csv(resources_table_, "au_resources.csv", this);
}

} // namespace fincept::screens
