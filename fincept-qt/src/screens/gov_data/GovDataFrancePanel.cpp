// src/screens/gov_data/GovDataFrancePanel.cpp
// France data.gouv.fr panel — Data Services | Datasets (search) | Geo Search
#include "screens/gov_data/GovDataFrancePanel.h"

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

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
const QString kGovDataFranceScript = QStringLiteral("french_gov_api.py");
const QString kGovFrancePrefix = QStringLiteral("gov.france");
// data.gouv.fr identity colour as it reads on the dark theme; always drawn through colors::legible().
constexpr const char* kGovDataFranceColor = "#2563EB";
constexpr int kGovFranceRefMaxAgeS = 24 * 3600;     // data services, a dataset's column schema
constexpr int kGovFranceDataMaxAgeS = 6 * 3600;     // a search's datasets
constexpr int kGovFranceCatalogMaxAgeS = 3600;      // the catalogue's newest datasets (empty term)
constexpr int kGovFranceGeoMaxAgeS = 7 * 24 * 3600; // municipalities: codes and names barely change
// The Geo tab's first search: the capital (any municipality name or postal code works).
const QString kGovFranceDefaultGeo = QStringLiteral("Paris");
} // namespace

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataFrancePanel::GovDataFrancePanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataFranceColor)); // once, before children
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataFrancePanel::on_result);
    // The tables are models whose ink is read from the live theme: no recolour walk on a theme change.

    // The tab, and the search term of each searchable tab, come back with the panel.
    tabs_memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("france"), this);
    tabs_memory_->track(QStringLiteral("tab_datasets"), datasets_btn_);
    tabs_memory_->track(QStringLiteral("tab_geo"), geo_btn_);
    tabs_memory_->restore();
    term_memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("france_datasets"), this);
    term_memory_->track(QStringLiteral("term"), search_input_);

    const View first = geo_btn_->isChecked() ? Geo : datasets_btn_->isChecked() ? Datasets : Services;
    if (first == Services) {
        go(view_key(Services), false);
    } else {
        load_term(first);
        go(view_key(first, search_input_->text().trimmed()), false);
    }
}

void GovDataFrancePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataFrancePanel::build_ui() {
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
    row_count_label_->setObjectName("govBreadcrumbActive");
    bcl->addWidget(row_count_label_);
    root->addWidget(breadcrumb_);

    build_pane(this);
    root->addWidget(pane_, 1);
}

void GovDataFrancePanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/france"), as_of_label_);
    content_stack_ = new QStackedWidget(pane_->content_parent());

    using Ink = GovTableModel::Ink;
    const QString color = QString::fromLatin1(kGovDataFranceColor);
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };
    const auto field_or_dash = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) {
            const QString s = r.value(k).toString();
            return s.isEmpty() ? fmt::placeholder() : s;
        };
    };
    const auto count_or_dash = [](const char* key, int min) {
        return [k = QString::fromLatin1(key), min](const QJsonObject& r) {
            const int n = r.value(k).toInt(-1);
            return n >= min ? fmt::quantity(n) : fmt::placeholder();
        };
    };
    const Qt::Alignment right = Qt::AlignRight | Qt::AlignVCenter;

    // Data Services table: NAME | VIEWS | FOLLOWERS | CREATED (headers set in retranslateUi). The
    // name is shown as published (French); the English title and the description are its tooltip.
    services_model_ = gov::make_model(color, this);
    services_model_->set_columns({
        {.text = field("name"), .tooltip = &gov::dataset_tooltip, .ink = Ink::Accent},
        {.text = count_or_dash("views", 0), .align = right},
        {.text = count_or_dash("followers", 0), .align = right},
        {.text = field("created"), .ink = Ink::Secondary},
    });
    services_table_ = gov::make_table_view(content_stack_, services_model_);
    services_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 3; ++c)
        services_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    services_table_->setColumnWidth(1, 80);
    services_table_->setColumnWidth(2, 90);
    services_table_->setColumnWidth(3, 100);
    services_table_->viewport()->setCursor(Qt::ArrowCursor);
    content_stack_->addWidget(services_table_);

    // Datasets table: TITLE | ORG | LICENSE | FILES | MODIFIED (headers set in retranslateUi). The
    // title is shown as published; "title_en" (a full English translation) and the description are
    // its tooltip.
    datasets_model_ = gov::make_model(color, this);
    datasets_model_->set_columns({
        {.text = field("title"), .tooltip = &gov::dataset_tooltip},
        {.text = field_or_dash("org"), .tooltip = field("org"), .ink = Ink::Secondary},
        {.text = field_or_dash("license"), .tooltip = field("license"), .ink = Ink::Secondary},
        {.text = count_or_dash("files", 0), .align = Qt::AlignCenter, .ink = Ink::Accent},
        {.text = field("modified")},
    });
    datasets_table_ = gov::make_table_view(content_stack_, datasets_model_);
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    for (int c = 2; c <= 4; ++c)
        datasets_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 150);
    datasets_table_->setColumnWidth(2, 130);
    datasets_table_->setColumnWidth(3, 55);
    datasets_table_->setColumnWidth(4, 100);
    connect(datasets_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_dataset_row_double_clicked(i.row()); });
    content_stack_->addWidget(datasets_table_);

    // Geo / Municipalities table: NAME | CODE | POSTAL | DEPT | REGION | POPULATION
    geo_model_ = gov::make_model(color, this);
    const auto postal_list = [](const QJsonObject& r) {
        QStringList postal;
        for (const QJsonValue& p : r.value(QStringLiteral("postal")).toArray())
            postal << p.toString();
        return postal;
    };
    geo_model_->set_columns({
        {.text = field("name"), .ink = Ink::Accent},
        {.text = field("code")},
        {.text = [postal_list](const QJsonObject& r) {
             const QStringList postal = postal_list(r);
             QString s = postal.mid(0, 3).join(QStringLiteral(", "));
             if (postal.size() > 3)
                 s += QStringLiteral(" +%1").arg(postal.size() - 3);
             return s;
         },
         .tooltip = [postal_list](const QJsonObject& r) { return postal_list(r).join(QStringLiteral(", ")); }},
        {.text = field("dept")},
        {.text = field("region")},
        {.text = count_or_dash("population", 1), .align = right},
    });
    geo_table_ = gov::make_table_view(content_stack_, geo_model_);
    geo_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 5; ++c)
        geo_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    geo_table_->setColumnWidth(1, 70);
    geo_table_->setColumnWidth(2, 110);
    geo_table_->setColumnWidth(3, 55);
    geo_table_->setColumnWidth(4, 65);
    geo_table_->setColumnWidth(5, 105);
    geo_table_->viewport()->setCursor(Qt::ArrowCursor);
    content_stack_->addWidget(geo_table_);

    // Resources / Column schema table: COLUMN | TYPE (headers set in retranslateUi)
    resources_model_ = gov::make_model(color, this);
    resources_model_->set_columns({
        {.text = field("name"), .ink = Ink::Accent},
        {.text = field_or_dash("format"), .align = Qt::AlignCenter, .ink = Ink::Secondary},
    });
    resources_table_ = gov::make_table_view(content_stack_, resources_model_);
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 120);
    resources_table_->viewport()->setCursor(Qt::ArrowCursor);
    content_stack_->addWidget(resources_table_);

    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        switch (current_view_) {
            case Datasets: // the term matched nothing: the catalogue's list
                search_input_->clear();
                go(view_key(Datasets), true);
                break;
            case Geo:
                search_input_->setText(kGovFranceDefaultGeo);
                go(view_key(Geo, kGovFranceDefaultGeo), true);
                break;
            case Resources:
                on_back();
                break;
            case Services:
                pane_->run();
                break;
        }
    });
    content_stack_->addWidget(no_data_page_);

    pane_->set_content(content_stack_);
    pane_->set_start([this](quint64 run) { start_run(run); });
    pane_->set_cancel([this](quint64 run) { gov::cancel_token(tokens_, run); });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
}

QWidget* GovDataFrancePanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    back_btn_ = new QPushButton(bar);
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    services_btn_ = new QPushButton(bar);
    services_btn_->setObjectName("govTabBtn");
    services_btn_->setCheckable(true);
    services_btn_->setChecked(true);
    services_btn_->setCursor(Qt::PointingHandCursor);
    connect(services_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Services); });
    hl->addWidget(services_btn_);

    datasets_btn_ = new QPushButton(bar);
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Datasets); });
    hl->addWidget(datasets_btn_);

    geo_btn_ = new QPushButton(bar);
    geo_btn_->setObjectName("govTabBtn");
    geo_btn_->setCheckable(true);
    geo_btn_->setCursor(Qt::PointingHandCursor);
    connect(geo_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(Geo); });
    hl->addWidget(geo_btn_);

    hl->addSpacing(10);

    // Search input — placeholder set in retranslateUi / update_toolbar_state.
    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("govSearch");
    search_input_->setMinimumWidth(140);
    search_input_->setMaximumWidth(320);
    search_input_->setMinimumHeight(26);
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(search_input_, 1);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataFrancePanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataFrancePanel::retranslateUi() {
    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (services_btn_)
        services_btn_->setText(tr("DATA SERVICES"));
    if (datasets_btn_)
        datasets_btn_->setText(tr("DATASETS"));
    if (geo_btn_)
        geo_btn_->setText(tr("GEO SEARCH"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));

    if (services_model_)
        services_model_->set_headers({tr("NAME"), tr("VIEWS"), tr("FOLLOWERS"), tr("CREATED")});
    if (datasets_model_)
        datasets_model_->set_headers({tr("TITLE"), tr("ORG"), tr("LICENSE"), tr("FILES"), tr("MODIFIED")});
    if (geo_model_)
        geo_model_->set_headers({tr("NAME"), tr("CODE"), tr("POSTAL"), tr("DEPT"), tr("REGION"), tr("POPULATION")});
    if (resources_model_)
        resources_model_->set_headers({tr("COLUMN"), tr("TYPE")});

    if (pane_) {
        update_toolbar_state();
        update_breadcrumb();
    }
}

// ── Views and runs ────────────────────────────────────────────────────────────

QString GovDataFrancePanel::view_key(View v, const QString& arg) {
    switch (v) {
        case Services:
            return QStringLiteral("services");
        case Datasets:
            return QStringLiteral("datasets:") + arg;
        case Geo:
            return QStringLiteral("geo:") + arg;
        case Resources:
            return QStringLiteral("schema:") + arg;
    }
    return {};
}

GovDataFrancePanel::View GovDataFrancePanel::view_of(const QString& key) {
    if (key.startsWith(QLatin1String("datasets:")))
        return Datasets;
    if (key.startsWith(QLatin1String("geo:")))
        return Geo;
    if (key.startsWith(QLatin1String("schema:")))
        return Resources;
    return Services;
}

QString GovDataFrancePanel::arg_of(const QString& key) {
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    return colon < 0 ? QString() : key.mid(colon + 1);
}

void GovDataFrancePanel::load_term(View v) {
    if (v != Datasets && v != Geo)
        return;
    term_memory_->set_view(v == Geo ? QStringLiteral("france_geo") : QStringLiteral("france_datasets"));
    {
        const QSignalBlocker block(search_input_);
        search_input_->clear();
    }
    term_memory_->restore();
    if (v == Geo && search_input_->text().trimmed().isEmpty()) {
        const QSignalBlocker block(search_input_);
        search_input_->setText(kGovFranceDefaultGeo);
    }
}

void GovDataFrancePanel::go(const QString& key, bool user, bool force) {
    current_view_ = view_of(key);
    const QString arg = arg_of(key);
    if (current_view_ == Datasets)
        last_datasets_view_ = key;
    int max_age = kGovFranceRefMaxAgeS;
    if (current_view_ == Datasets)
        max_age = arg.isEmpty() ? kGovFranceCatalogMaxAgeS : kGovFranceDataMaxAgeS;
    else if (current_view_ == Geo)
        max_age = kGovFranceGeoMaxAgeS;
    pane_->set_view_max_age(key, max_age);
    pane_->set_view_skeleton(key, current_view_ == Services || current_view_ == Resources ? LoadingSkeleton::Shape::List
                                                                                          : LoadingSkeleton::Shape::Table);
    switch (current_view_) {
        case Services:
            pane_->set_loading_text(tr("Loading data services from data.gouv.fr…"));
            break;
        case Datasets:
            pane_->set_loading_text(arg.isEmpty() ? tr("Loading datasets from data.gouv.fr…")
                                                  : tr("Searching datasets for \"%1\"…").arg(arg));
            break;
        case Geo:
            pane_->set_loading_text(tr("Searching municipalities for \"%1\"…").arg(arg));
            break;
        case Resources:
            pane_->set_loading_text(tr("Loading the column schema…"));
            break;
    }
    pane_->set_view(key);
    update_toolbar_state();
    update_breadcrumb();
    const bool running_here = pane_->is_running() && pane_->running_view() == key;
    if (force || (user && !pane_->has_result() && !running_here))
        pane_->run();
}

void GovDataFrancePanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(view_key(Services), false);
}

void GovDataFrancePanel::on_tab_changed(int tab_index) {
    const auto view = static_cast<View>(tab_index);
    if (view == Services) {
        go(view_key(Services), true);
        return;
    }
    load_term(view);
    go(view_key(view, search_input_->text().trimmed()), true);
}

void GovDataFrancePanel::on_fetch() {
    const QString query = search_input_->text().trimmed();
    switch (current_view_) {
        case Services:
        case Resources:
            pane_->run(); // reload what is on screen
            break;
        case Datasets:
            go(view_key(Datasets, query), true, true); // an empty term lists the catalogue
            break;
        case Geo:
            go(view_key(Geo, query.isEmpty() ? kGovFranceDefaultGeo : query), true, true);
            break;
    }
}

void GovDataFrancePanel::on_dataset_row_double_clicked(int row) {
    const QJsonObject dataset = datasets_model_->row(row);
    const QString id = dataset.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = view_key(Resources, id);
    labels_.insert(key, dataset.value(QStringLiteral("title")).toString());
    go(key, true);
}

void GovDataFrancePanel::on_back() {
    if (current_view_ == Resources)
        go(last_datasets_view_.isEmpty() ? view_key(Datasets) : last_datasets_view_, true);
}

void GovDataFrancePanel::start_run(quint64 run) {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString command;
    QStringList args;
    switch (view_of(key)) {
        case Services:
            command = QStringLiteral("publishers");
            break;
        case Datasets:
            // `datasets <query> <limit>`; an empty query lists the catalogue (newest first).
            command = QStringLiteral("datasets");
            args << arg << QStringLiteral("50");
            break;
        case Geo:
            command = QStringLiteral("municipalities");
            args << arg;
            break;
        case Resources:
            command = QStringLiteral("resources");
            args << arg;
            break;
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(kGovDataFranceScript, command, args,
                                                 gov::request_id(kGovFrancePrefix, run), token);
}

void GovDataFrancePanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovFrancePrefix);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const View view = view_of(pane_->running_view());
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data")) {
            // an empty list: the no-data page
            pane_->deliver(run, gov::list_payload(payload_kind(view), QJsonArray()));
            return;
        }
        gov::fail_pane(pane_, run, result, QStringLiteral("data.gouv.fr"));
        return;
    }
    // Shaped on the shaping thread (the 50-dataset answer is ~280 KB); dropped when superseded.
    gov::shape_async(
        this, [view, data = result.data]() { return shape(view, data); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run))
                pane_->deliver(run, payload);
        });
}

QString GovDataFrancePanel::payload_kind(View v) {
    switch (v) {
        case Services:
            return QStringLiteral("services");
        case Datasets:
            return QStringLiteral("datasets");
        case Geo:
            return QStringLiteral("geo");
        case Resources:
            break;
    }
    return QStringLiteral("schema");
}

QJsonObject GovDataFrancePanel::shape(View view, const QJsonObject& data) {
    // Codes, counts and populations may arrive as numbers or as numeric strings: read both.
    const QJsonArray raw = gov::list_rows(data);
    QJsonArray rows;
    switch (view) {
        case Services:
            for (const QJsonValue& v : raw) {
                const QJsonObject o = v.toObject();
                QString name;
                for (const char* k : {"display_name", "title", "name"}) {
                    name = gov::text(o.value(QLatin1String(k)));
                    if (!name.isEmpty())
                        break;
                }
                const QJsonObject metrics = o.value("metrics").toObject();
                QJsonObject row{{"name", name},
                                {"desc", gov::text(o.value("description")).left(400)},
                                {"views", gov::integer(metrics.value("views"))},
                                {"followers", gov::integer(metrics.value("followers"))},
                                {"created", gov::text(o.value("created_at")).left(10)}};
                // The name as published (French); a full English translation goes in the tooltip.
                const QString title_en = gov::text(o.value("title_en"));
                if (!title_en.isEmpty() && title_en != name)
                    row.insert("title_en", title_en);
                rows.append(row);
            }
            break;
        case Datasets:
            rows = gov::compact_datasets(raw);
            break;
        case Geo:
            for (const QJsonValue& v : raw) {
                const QJsonObject o = v.toObject();
                QJsonArray postal;
                for (const QJsonValue& p : o.value("postal_codes").toArray())
                    postal.append(gov::text(p));
                rows.append(QJsonObject{{"name", gov::text(o.value("name"))},
                                        {"code", gov::text(o.value("code"))},
                                        {"postal", postal},
                                        {"dept", gov::text(o.value("department_code"))},
                                        {"region", gov::text(o.value("region_code"))},
                                        {"population", gov::integer(o.value("population"), 0)}});
            }
            break;
        case Resources:
            for (const QJsonValue& v : raw) {
                const QJsonObject o = v.toObject();
                rows.append(QJsonObject{{"name", gov::text(o.value("name"))}, {"format", gov::text(o.value("format"))}});
            }
            break;
    }
    return gov::list_payload(payload_kind(view), rows);
}

// ── Render ────────────────────────────────────────────────────────────────────

void GovDataFrancePanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    const QJsonArray rows = p.value("rows").toArray();
    const QString key = pane_->view();
    if (rows.isEmpty()) {
        row_count_label_->clear();
        show_no_data(view_of(key), arg_of(key));
        return;
    }
    if (kind == QLatin1String("services")) {
        populate_services(rows);
        content_stack_->setCurrentWidget(services_table_);
        row_count_label_->setText(tr("%1 services").arg(fmt::quantity(rows.size())));
    } else if (kind == QLatin1String("datasets")) {
        populate_datasets(rows);
        content_stack_->setCurrentWidget(datasets_table_);
        row_count_label_->setText(tr("%1 datasets").arg(fmt::quantity(rows.size())));
    } else if (kind == QLatin1String("geo")) {
        populate_geo(rows);
        content_stack_->setCurrentWidget(geo_table_);
        row_count_label_->setText(tr("%1 municipalities").arg(fmt::quantity(rows.size())));
    } else if (kind == QLatin1String("schema")) {
        populate_resources(rows);
        content_stack_->setCurrentWidget(resources_table_);
        row_count_label_->setText(tr("%1 columns").arg(fmt::quantity(rows.size())));
    }
}

void GovDataFrancePanel::show_no_data(View v, const QString& arg) {
    EmptyState::Spec spec;
    switch (v) {
        case Services:
            spec = EmptyState::no_data_spec(tr("No data services listed"),
                                            tr("data.gouv.fr returned an empty list. Try again later."), tr("RELOAD"));
            break;
        case Datasets:
            spec = arg.isEmpty() ? EmptyState::no_data_spec(tr("No datasets listed"),
                                                            tr("data.gouv.fr returned an empty list."), tr("RELOAD"))
                                 : EmptyState::no_data_spec(tr("No datasets match \"%1\"").arg(arg),
                                                            tr("Try a shorter term, or French words (« budget », "
                                                               "« population »)."),
                                                            tr("SHOW ALL DATASETS"));
            break;
        case Geo:
            spec = EmptyState::no_data_spec(tr("No municipality named \"%1\"").arg(arg),
                                            tr("Search by name or by postal code (75001)."), tr("SEARCH PARIS"));
            break;
        case Resources:
            spec = EmptyState::no_data_spec(tr("No column schema for this dataset"),
                                            tr("data.gouv.fr has no tabular profile of its files."), tr("BACK"));
            break;
    }
    spec.glyph = QStringLiteral("∅");
    gov::show_no_data(content_stack_, no_data_page_, spec);
}

// ── Toolbar / breadcrumb ──────────────────────────────────────────────────────

void GovDataFrancePanel::update_toolbar_state() {
    services_btn_->setChecked(current_view_ == Services);
    datasets_btn_->setChecked(current_view_ == Datasets || current_view_ == Resources);
    geo_btn_->setChecked(current_view_ == Geo);
    back_btn_->setVisible(current_view_ == Resources);
    // The search box belongs to Datasets and Geo; on Data Services FETCH reloads the list.
    search_input_->setEnabled(current_view_ == Datasets || current_view_ == Geo);
    search_input_->setPlaceholderText(current_view_ == Geo ? tr("Municipality or postal code…")
                                                           : tr("Search datasets… (empty = all)"));
}

void GovDataFrancePanel::update_breadcrumb() {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    switch (current_view_) {
        case Services:
            breadcrumb_label_->setText(tr("Data Services"));
            break;
        case Datasets:
            breadcrumb_label_->setText(arg.isEmpty() ? tr("Datasets  ›  all") : tr("Datasets  ›  \"%1\"").arg(arg));
            break;
        case Geo:
            breadcrumb_label_->setText(tr("Municipalities  ›  \"%1\"").arg(arg));
            break;
        case Resources:
            breadcrumb_label_->setText(tr("Datasets  ›  Column Schema  ›  %1").arg(labels_.value(key).left(80)));
            break;
    }
}

// ── Populate tables ───────────────────────────────────────────────────────────

void GovDataFrancePanel::populate_services(const QJsonArray& json) {
    gov::show_rows(services_table_, services_model_, json); // a model reset: cells are formatted when shown
    LOG_DEBUG("GovFrance", QString("Populated %1 json services").arg(json.size()));
}

void GovDataFrancePanel::populate_datasets(const QJsonArray& json) {
    gov::show_rows(datasets_table_, datasets_model_, json);
    LOG_DEBUG("GovFrance", QString("Populated %1 datasets").arg(json.size()));
}

void GovDataFrancePanel::populate_geo(const QJsonArray& json) {
    gov::show_rows(geo_table_, geo_model_, json);
    LOG_DEBUG("GovFrance", QString("Populated %1 municipalities").arg(json.size()));
}

void GovDataFrancePanel::populate_resources(const QJsonArray& json) {
    gov::show_rows(resources_table_, resources_model_, json);
    LOG_DEBUG("GovFrance", QString("Populated %1 schema columns").arg(json.size()));
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataFrancePanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == services_table_)
        export_table_to_csv(services_table_, "france_data_services.csv", this);
    else if (page == datasets_table_)
        export_table_to_csv(datasets_table_, "france_datasets.csv", this);
    else if (page == geo_table_)
        export_table_to_csv(geo_table_, "france_municipalities.csv", this);
    else if (page == resources_table_)
        export_table_to_csv(resources_table_, "france_schema.csv", this);
}

} // namespace fincept::screens
