// src/screens/gov_data/GovDataHKPanel.cpp
// Hong Kong data.gov.hk panel
// Categories use publishers command (normalized string array).
// HK category_datasets may return empty — the no-data page offers the full dataset list.
#include "screens/gov_data/GovDataHKPanel.h"

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

namespace fincept::screens {
namespace {
const QString kGovDataHKScript = QStringLiteral("data_gov_hk_api.py");
const QString kGovHKPrefix = QStringLiteral("gov.hk");
// data.gov.hk identity colour as it reads on the dark theme; always drawn through colors::legible().
constexpr const char* kGovDataHKColor = "#F43F5E";
constexpr int kGovHKRefMaxAgeS = 24 * 3600; // categories, a dataset's resources
constexpr int kGovHKDataMaxAgeS = 6 * 3600; // dataset lists, the name list behind the filter
constexpr int kGovHKListRows = 100;
} // namespace

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

// ── Constructor ───────────────────────────────────────────────────────────────

GovDataHKPanel::GovDataHKPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(make_gov_panel_style(kGovDataHKColor)); // once, before children
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataHKPanel::on_result);
    // The tables are models whose ink is read from the live theme: no recolour walk on a theme change.

    // The filter term and the page on screen (category, dataset) come back with the panel.
    memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("hk"), this);
    memory_->track(QStringLiteral("filter"), search_input_);
    memory_->restore();
    const QString first_view = restore_navigation();

    go(first_view.isEmpty() ? QStringLiteral("categories") : first_view, false); // loads when first shown
}

void GovDataHKPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void GovDataHKPanel::build_ui() {
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

void GovDataHKPanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/hk"), as_of_label_);
    content_stack_ = new QStackedWidget(pane_->content_parent());

    using Ink = GovTableModel::Ink;
    const QString color = QString::fromLatin1(kGovDataHKColor);
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };
    const auto field_or_dash = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) {
            const QString s = r.value(k).toString();
            return s.isEmpty() ? fmt::placeholder() : s;
        };
    };

    // Categories table (no dataset count — always -1 from the HK API; header set in retranslateUi)
    categories_model_ = gov::make_model(color, this);
    categories_model_->set_columns({{.text = field("name"), .ink = Ink::Accent}});
    categories_table_ = gov::make_table_view(content_stack_, categories_model_);
    categories_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(categories_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_category_clicked(i.row()); });
    content_stack_->addWidget(categories_table_);

    // Datasets table: TITLE | RESOURCES | MODIFIED (headers set in retranslateUi)
    datasets_model_ = gov::make_model(color, this);
    datasets_model_->set_columns({
        {.text = field("title"), .tooltip = &gov::dataset_tooltip},
        {.text = [](const QJsonObject& r) {
             const int files = r.value(QStringLiteral("files")).toInt(-1);
             return files > 0 ? fmt::quantity(files) : fmt::placeholder();
         },
         .align = Qt::AlignCenter,
         .ink = Ink::Accent},
        {.text = field_or_dash("modified")},
    });
    datasets_table_ = gov::make_table_view(content_stack_, datasets_model_);
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->setColumnWidth(1, 90);
    datasets_table_->setColumnWidth(2, 110);
    connect(datasets_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_dataset_clicked(i.row()); });
    content_stack_->addWidget(datasets_table_);

    // Resources table: NAME | FORMAT | OPEN (headers set in retranslateUi); the third column opens
    // the file (a click; a double-click or Enter on the row too). Without a URL it shows the date.
    const auto open_text = [](const QJsonObject& r) {
        if (!r.value(QStringLiteral("url")).toString().isEmpty())
            return GovDataHKPanel::tr("↗ OPEN");
        const QString modified = r.value(QStringLiteral("modified")).toString();
        return modified.isEmpty() ? fmt::placeholder() : modified;
    };
    const auto has_url = [](const QJsonObject& r) { return !r.value(QStringLiteral("url")).toString().isEmpty(); };
    resources_model_ = gov::make_model(color, this);
    resources_model_->set_columns({
        {.text = field("name")},
        {.text = field_or_dash("format"), .align = Qt::AlignCenter, .ink = Ink::Accent},
        {.text = open_text,
         .tooltip = field("url"),
         .align = Qt::AlignCenter,
         .ink = Ink::Accent,
         .ink_if = has_url,
         .export_text = [open_text, has_url](const QJsonObject& r) {
             return has_url(r) ? r.value(QStringLiteral("url")).toString() : open_text(r);
         }},
    });
    resources_table_ = gov::make_table_view(content_stack_, resources_model_);
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resources_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resources_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 80);
    resources_table_->setColumnWidth(2, 110);
    const auto open_resource = [this](int row) {
        const QString url = resources_model_->row(row).value(QStringLiteral("url")).toString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    };
    connect(resources_table_, &QTableView::clicked, this, [open_resource](const QModelIndex& i) {
        if (i.column() == 2)
            open_resource(i.row());
    });
    connect(resources_table_, &QTableView::activated, this, [open_resource](const QModelIndex& i) {
        if (i.column() != 2) // the OPEN cell already opened on the click
            open_resource(i.row());
    });
    content_stack_->addWidget(resources_table_);

    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        switch (kind_of(pane_->view())) {
            case Kind::Categories:
            case Kind::All:
                pane_->run();
                break;
            case Kind::Category:
            case Kind::Filter:
                go(QStringLiteral("all"), true);
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

QWidget* GovDataHKPanel::build_toolbar() {
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
    connect(back_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    categories_btn_ = new QPushButton(bar);
    categories_btn_->setObjectName("govTabBtn");
    categories_btn_->setCheckable(true);
    categories_btn_->setChecked(true);
    categories_btn_->setCursor(Qt::PointingHandCursor);
    connect(categories_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(0); });
    hl->addWidget(categories_btn_);

    datasets_btn_ = new QPushButton(bar);
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, [this]() { on_tab_changed(1); });
    hl->addWidget(datasets_btn_);

    hl->addSpacing(10);

    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("govSearch");
    search_input_->setMinimumWidth(140);
    search_input_->setMaximumWidth(320);
    search_input_->setMinimumHeight(26);
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataHKPanel::on_fetch);
    hl->addWidget(search_input_, 1);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataHKPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataHKPanel::retranslateUi() {
    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (categories_btn_)
        categories_btn_->setText(tr("CATEGORIES"));
    if (datasets_btn_)
        datasets_btn_->setText(tr("DATASETS"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Filter datasets by name…"));

    if (categories_model_)
        categories_model_->set_headers({tr("CATEGORY")});
    if (datasets_model_)
        datasets_model_->set_headers({tr("TITLE"), tr("RESOURCES"), tr("MODIFIED")});
    if (resources_model_)
        resources_model_->set_headers({tr("NAME"), tr("FORMAT"), tr("OPEN")});
    if (pane_)
        update_breadcrumb();
}

// ── Views and runs ────────────────────────────────────────────────────────────

GovDataHKPanel::Kind GovDataHKPanel::kind_of(const QString& key) {
    if (key.startsWith(QLatin1String("cat:")))
        return Kind::Category;
    if (key == QLatin1String("all"))
        return Kind::All;
    if (key.startsWith(QLatin1String("filter:")))
        return Kind::Filter;
    if (key.startsWith(QLatin1String("res:")))
        return Kind::Resources;
    return Kind::Categories;
}

QString GovDataHKPanel::arg_of(const QString& key) {
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    return colon < 0 ? QString() : key.mid(colon + 1);
}

bool GovDataHKPanel::valid_view(const QString& key) {
    switch (kind_of(key)) {
        case Kind::Categories:
            return key == QLatin1String("categories");
        case Kind::All:
            return true;
        case Kind::Category:
        case Kind::Filter:
        case Kind::Resources:
            return !arg_of(key).isEmpty();
    }
    return false;
}

QString GovDataHKPanel::restore_navigation() {
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
    if (valid_view(list) && (list_kind == Kind::Category || list_kind == Kind::All || list_kind == Kind::Filter))
        last_list_view_ = list;
    const QString view = memory_->value(QStringLiteral("view")).toString();
    return valid_view(view) ? view : QString();
}

void GovDataHKPanel::remember_navigation() {
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

void GovDataHKPanel::go(const QString& key, bool user, bool force) {
    const Kind kind = kind_of(key);
    const QString arg = arg_of(key);
    if (kind == Kind::Category || kind == Kind::All || kind == Kind::Filter)
        last_list_view_ = key;
    pane_->set_view_max_age(key, kind == Kind::Categories || kind == Kind::Resources ? kGovHKRefMaxAgeS
                                                                                    : kGovHKDataMaxAgeS);
    pane_->set_view_skeleton(key, kind == Kind::Categories ? LoadingSkeleton::Shape::List
                                                           : LoadingSkeleton::Shape::Table);
    switch (kind) {
        case Kind::Categories:
            pane_->set_loading_text(tr("Loading categories from data.gov.hk…"));
            break;
        case Kind::Category:
            pane_->set_loading_text(tr("Loading datasets for \"%1\"…").arg(labels_.value(key, arg)));
            break;
        case Kind::All:
            pane_->set_loading_text(tr("Loading the data.gov.hk dataset list…"));
            break;
        case Kind::Filter:
            pane_->set_loading_text(tr("Matching dataset names against \"%1\"…").arg(arg));
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

void GovDataHKPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(QStringLiteral("categories"), false);
}

void GovDataHKPanel::on_tab_changed(int tab_index) {
    if (tab_index == 0) {
        go(QStringLiteral("categories"), true);
        return;
    }
    // DATASETS: the last dataset list, else the first category's datasets, else the full list.
    if (!last_list_view_.isEmpty()) {
        go(last_list_view_, true);
        return;
    }
    const QJsonObject first = categories_.isEmpty() ? QJsonObject() : categories_.first().toObject();
    const QString id = first.value("id").toString();
    if (!id.isEmpty()) {
        const QString key = QStringLiteral("cat:") + id;
        labels_.insert(key, first.value("name").toString(id));
        go(key, true);
    } else {
        go(QStringLiteral("all"), true);
    }
}

void GovDataHKPanel::on_fetch() {
    const QString query = search_input_->text().trimmed();
    if (!query.isEmpty())
        go(QStringLiteral("filter:") + query, true, true);
    else
        pane_->run(); // reload what is on screen
}

void GovDataHKPanel::on_category_clicked(int row) {
    const QJsonObject cat = categories_model_->row(row);
    const QString id = cat.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("cat:") + id;
    labels_.insert(key, cat.value(QStringLiteral("name")).toString());
    go(key, true);
}

void GovDataHKPanel::on_dataset_clicked(int row) {
    const QJsonObject dataset = datasets_model_->row(row);
    const QString id = dataset.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("res:") + id;
    labels_.insert(key, dataset.value(QStringLiteral("title")).toString());
    go(key, true);
}

void GovDataHKPanel::on_back() {
    const Kind kind = kind_of(pane_->view());
    if (kind == Kind::Resources && !last_list_view_.isEmpty())
        go(last_list_view_, true);
    else
        go(QStringLiteral("categories"), true);
}

QJsonArray GovDataHKPanel::filter_names(const QJsonArray& names, const QString& term) {
    QJsonArray matches;
    for (const QJsonValue& v : names) {
        const QString name = v.isObject() ? gov::text(v.toObject().value(QStringLiteral("name"))) : gov::text(v);
        if (name.contains(term, Qt::CaseInsensitive))
            matches.append(name);
    }
    return matches;
}

QJsonObject GovDataHKPanel::shape(const QString& key, const QJsonObject& data) {
    int total = -1;
    const QJsonArray rows = gov::list_rows(data, &total);
    switch (kind_of(key)) {
        case Kind::Categories: {
            QJsonArray cats;
            for (const QJsonValue& v : rows) {
                const QJsonObject o = v.toObject();
                QString name = gov::text(o.value(QStringLiteral("display_name")));
                if (name.isEmpty())
                    name = gov::text(o.value(QStringLiteral("name")));
                const QString id = gov::text(o.value(QStringLiteral("id"))).isEmpty()
                                       ? gov::text(o.value(QStringLiteral("name")))
                                       : gov::text(o.value(QStringLiteral("id")));
                cats.append(QJsonObject{{QStringLiteral("name"), name.isEmpty() ? id : name}, {QStringLiteral("id"), id}});
            }
            return gov::list_payload(QStringLiteral("categories"), cats);
        }
        case Kind::Category:
        case Kind::All:
            return gov::list_payload(QStringLiteral("datasets"), gov::compact_datasets(rows), total);
        case Kind::Filter:
            // `rows` is the full name list (every data.gov.hk dataset): matched here, off the GUI thread.
            return gov::list_payload(QStringLiteral("datasets"), gov::compact_datasets(filter_names(rows, arg_of(key))));
        case Kind::Resources:
            return gov::list_payload(QStringLiteral("resources"), gov::compact_resources(rows));
    }
    return {};
}

void GovDataHKPanel::deliver_shaped(quint64 run, std::function<QJsonObject()> shaper) {
    gov::shape_async(this, std::move(shaper), [this, run](const QJsonObject& payload) {
        if (pane_->is_current(run)) // superseded, cancelled or its view left meanwhile: dropped
            pane_->deliver(run, payload);
    });
}

void GovDataHKPanel::start_run(quint64 run) {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString command;
    QStringList args;
    switch (kind_of(key)) {
        case Kind::Categories:
            command = QStringLiteral("publishers");
            break;
        case Kind::Category:
            command = QStringLiteral("datasets");
            args << arg << QString::number(kGovHKListRows);
            break;
        case Kind::All:
            command = QStringLiteral("datasets_list");
            args << QString::number(kGovHKListRows);
            break;
        case Kind::Filter:
            // The name list is fetched once; later filters are matched against it (on the shaping thread).
            if (!all_dataset_names_.isEmpty()) {
                deliver_shaped(run, [names = all_dataset_names_, term = arg]() {
                    return gov::list_payload(QStringLiteral("datasets"), gov::compact_datasets(filter_names(names, term)));
                });
                return;
            }
            command = QStringLiteral("datasets_list");
            break;
        case Kind::Resources:
            command = QStringLiteral("resources");
            args << arg;
            break;
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(kGovDataHKScript, command, args, gov::request_id(kGovHKPrefix, run),
                                                 token);
}

void GovDataHKPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovHKPrefix);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const QString key = pane_->running_view();
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data") && kind_of(key) != Kind::Filter) {
            const QString kind = kind_of(key) == Kind::Categories  ? QStringLiteral("categories")
                                 : kind_of(key) == Kind::Resources ? QStringLiteral("resources")
                                                                   : QStringLiteral("datasets");
            pane_->deliver(run, gov::list_payload(kind, QJsonArray())); // an empty list: the no-data page
            return;
        }
        gov::fail_pane(pane_, run, result, QStringLiteral("data.gov.hk"));
        return;
    }
    if (kind_of(key) == Kind::Filter)
        all_dataset_names_ = gov::list_rows(result.data); // the full name list for later filters (shared, no copy)
    // The answer (a category's datasets is 2.5 MB of JSON) is shaped on the shaping thread.
    deliver_shaped(run, [key, data = result.data]() { return shape(key, data); });
}

// ── Render ────────────────────────────────────────────────────────────────────

void GovDataHKPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    const QJsonArray rows = p.value("rows").toArray();
    const QString key = pane_->view();
    if (kind == QLatin1String("categories"))
        categories_ = rows;
    if (rows.isEmpty()) {
        row_count_label_->clear();
        show_no_data(kind_of(key), arg_of(key));
        return;
    }
    if (kind == QLatin1String("categories")) {
        populate_categories(rows);
        content_stack_->setCurrentWidget(categories_table_);
        row_count_label_->setText(tr("%1 categories").arg(fmt::quantity(rows.size())));
    } else if (kind == QLatin1String("datasets")) {
        populate_datasets(rows);
        content_stack_->setCurrentWidget(datasets_table_);
        const int total = p.value("total").toInt(int(rows.size()));
        row_count_label_->setText(total > rows.size() ? tr("Showing %1 of %2").arg(fmt::quantity(rows.size()),
                                                                                     fmt::quantity(total))
                                                      : tr("%1 datasets").arg(fmt::quantity(rows.size())));
    } else if (kind == QLatin1String("resources")) {
        populate_resources(rows);
        content_stack_->setCurrentWidget(resources_table_);
        row_count_label_->setText(tr("%1 files").arg(fmt::quantity(rows.size())));
    }
}

void GovDataHKPanel::show_no_data(Kind kind, const QString& arg) {
    EmptyState::Spec spec;
    switch (kind) {
        case Kind::Categories:
            spec = EmptyState::no_data_spec(tr("No categories listed"),
                                            tr("data.gov.hk returned an empty list. Try again later."), tr("RELOAD"));
            break;
        case Kind::Category:
            spec = EmptyState::no_data_spec(tr("No datasets in \"%1\"").arg(labels_.value(pane_->view(), arg)),
                                            tr("data.gov.hk lists few datasets under some categories."),
                                            tr("SHOW ALL DATASETS"));
            break;
        case Kind::All:
            spec = EmptyState::no_data_spec(tr("No datasets listed"), tr("data.gov.hk returned an empty list."),
                                            tr("RELOAD"));
            break;
        case Kind::Filter:
            spec = EmptyState::no_data_spec(tr("No dataset name contains \"%1\"").arg(arg),
                                            tr("The filter matches dataset names (English, lower case)."),
                                            tr("SHOW ALL DATASETS"));
            break;
        case Kind::Resources:
            spec = EmptyState::no_data_spec(tr("No files in this dataset"),
                                            tr("The dataset lists no downloadable resources."), tr("BACK"));
            break;
    }
    spec.glyph = QStringLiteral("∅");
    gov::show_no_data(content_stack_, no_data_page_, spec);
}

// ── Toolbar / breadcrumb ──────────────────────────────────────────────────────

void GovDataHKPanel::update_toolbar_state() {
    const Kind kind = kind_of(pane_->view());
    categories_btn_->setChecked(kind == Kind::Categories);
    datasets_btn_->setChecked(kind != Kind::Categories);
    back_btn_->setVisible(kind != Kind::Categories);
}

void GovDataHKPanel::update_breadcrumb() {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    switch (kind_of(key)) {
        case Kind::Categories:
            breadcrumb_label_->setText(tr("Categories"));
            break;
        case Kind::Category:
            breadcrumb_label_->setText(tr("Datasets  ›  %1").arg(labels_.value(key, arg)));
            break;
        case Kind::All:
            breadcrumb_label_->setText(tr("Datasets  ›  all"));
            break;
        case Kind::Filter:
            breadcrumb_label_->setText(tr("Datasets  ›  Filter: \"%1\"").arg(arg));
            break;
        case Kind::Resources:
            breadcrumb_label_->setText(tr("Datasets  ›  Resources  ›  %1").arg(labels_.value(key).left(80)));
            break;
    }
}

// ── Populate tables (model resets: no per-row work on the GUI thread) ─────────

void GovDataHKPanel::populate_categories(const QJsonArray& json) {
    gov::show_rows(categories_table_, categories_model_, json);
    LOG_DEBUG("GovHK", QString("Populated %1 categories").arg(json.size()));
}

void GovDataHKPanel::populate_datasets(const QJsonArray& json) {
    gov::show_rows(datasets_table_, datasets_model_, json);
    LOG_DEBUG("GovHK", QString("Populated %1 datasets").arg(json.size()));
}

void GovDataHKPanel::populate_resources(const QJsonArray& json) {
    gov::show_rows(resources_table_, resources_model_, json);
    LOG_DEBUG("GovHK", QString("Populated %1 resources").arg(json.size()));
}

// ── CSV export ────────────────────────────────────────────────────────────────

void GovDataHKPanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == categories_table_)
        export_table_to_csv(categories_table_, "hk_categories.csv", this);
    else if (page == datasets_table_)
        export_table_to_csv(datasets_table_, "hk_datasets.csv", this);
    else if (page == resources_table_)
        export_table_to_csv(resources_table_, "hk_resources.csv", this);
}

} // namespace fincept::screens
