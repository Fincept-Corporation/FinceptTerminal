// src/screens/gov_data/GovDataUKPanel.cpp
#include "screens/gov_data/GovDataUKPanel.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataProviderPanel.h"
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
const QString kGovDataUKScript = QStringLiteral("datagovuk_api.py");
const QString kGovUKPrefix = QStringLiteral("gov.uk");
// data.gov.uk identity colour as it reads on the dark theme; always drawn through colors::legible().
constexpr const char* kGovDataUKColor = "#10B981";
constexpr int kGovUKRefMaxAgeS = 24 * 3600;   // publishers, popular publishers, a dataset's resources
constexpr int kGovUKDataMaxAgeS = 6 * 3600;   // a publisher's datasets, search results
constexpr int kGovUKRecentMaxAgeS = 3600;     // "recently updated": changes through the day
} // namespace

using namespace fincept::ui;
namespace fmt = fincept::ui::formatting;

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataUKPanel::GovDataUKPanel(QWidget* parent) : QWidget(parent) {
    // UK has a unique #govPopularBtn — its style goes in as extra_qss (derived colours only, so the
    // central re-style pass carries it through a theme change).
    const QColor provider(kGovDataUKColor);
    QString uk_extra;
    uk_extra += QString("#govPopularBtn { background:transparent; color:%1; border:1px solid %1; font-weight:700;"
                        " padding:4px 10px; font-size:%2px; }")
                    .arg(colors::legible(provider))
                    .arg(fonts::px(-4));
    uk_extra += QString("#govPopularBtn:hover { background:%1; }").arg(colors::tint(provider, 0.12));
    setStyleSheet(make_gov_panel_style(kGovDataUKColor, uk_extra)); // once, before children
    build_ui();
    retranslateUi();
    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataUKPanel::on_result);
    // Item brushes are not reached by stylesheets: recolour once the new appearance has settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, &GovDataUKPanel::recolor_items);

    // The search term and the page on screen (publisher, dataset, list) come back with the panel.
    memory_ = new InputMemory(QStringLiteral("gov_data"), QStringLiteral("uk"), this);
    memory_->track(QStringLiteral("search"), search_input_);
    memory_->restore();
    const QString first_view = restore_navigation();

    go(first_view.isEmpty() ? QStringLiteral("publishers") : first_view, false); // loads when first shown
}

void GovDataUKPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ── UI Construction ──────────────────────────────────────────────────────────

void GovDataUKPanel::build_ui() {
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

void GovDataUKPanel::build_pane(QWidget* parent) {
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/uk"), as_of_label_);
    content_stack_ = new QStackedWidget(pane_->content_parent());

    auto wrap_with_pager = [this](QTableWidget*& table, ui::PaginationBar*& pager) {
        auto* w = new QWidget(content_stack_);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        table = new QTableWidget(w);
        vl->addWidget(table, 1);
        pager = new ui::PaginationBar(w);
        vl->addWidget(pager);
        return w;
    };

    // Publishers table (single stretch column, header set in retranslateUi)
    publishers_page_ = wrap_with_pager(publishers_table_, pub_pager_);
    publishers_table_->setColumnCount(1);
    publishers_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    configure_table(publishers_table_);
    connect(publishers_table_, &QTableWidget::cellDoubleClicked, this, &GovDataUKPanel::on_publisher_doubleclicked);
    content_stack_->addWidget(publishers_page_);
    connect(pub_pager_, &ui::PaginationBar::page_changed, this, [this]() { render_publishers_page(); });

    // Datasets table: TITLE | FILES | MODIFIED | TAGS (headers set in retranslateUi)
    datasets_page_ = wrap_with_pager(datasets_table_, ds_pager_);
    datasets_table_->setColumnCount(4);
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    datasets_table_->setColumnWidth(1, 55);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 200);
    configure_table(datasets_table_);
    connect(datasets_table_, &QTableWidget::cellDoubleClicked, this, &GovDataUKPanel::on_dataset_doubleclicked);
    content_stack_->addWidget(datasets_page_);
    connect(ds_pager_, &ui::PaginationBar::page_changed, this, [this]() { render_datasets_page(); });

    // Resources table: NAME | FORMAT | SIZE | MODIFIED | OPEN (headers set in retranslateUi)
    resources_page_ = wrap_with_pager(resources_table_, res_pager_);
    resources_table_->setColumnCount(5);
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 4; ++c)
        resources_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 70);
    resources_table_->setColumnWidth(2, 80);
    resources_table_->setColumnWidth(3, 100);
    resources_table_->setColumnWidth(4, 70);
    configure_table(resources_table_);
    connect(
        resources_table_, &QTableWidget::cellClicked, this,
        [this](int row, int col) {
            if (col != 4)
                return;
            auto* item = resources_table_->item(row, 4);
            if (!item)
                return;
            const QString url = item->data(Qt::UserRole).toString();
            if (!url.isEmpty())
                QDesktopServices::openUrl(QUrl(url));
        },
        Qt::UniqueConnection);
    content_stack_->addWidget(resources_page_);
    connect(res_pager_, &ui::PaginationBar::page_changed, this, [this]() { render_resources_page(); });

    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        switch (kind_of(pane_->view())) {
            case Kind::Publishers:
            case Kind::Popular:
                pane_->run();
                break;
            case Kind::Recent:
            case Kind::Publisher:
                go(QStringLiteral("publishers"), true);
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

QWidget* GovDataUKPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button — hidden until we drill in (text set in retranslateUi)
    back_btn_ = new QPushButton(bar);
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // Tab: Publishers (text set in retranslateUi)
    publishers_btn_ = new QPushButton(bar);
    publishers_btn_->setObjectName("govTabBtn");
    publishers_btn_->setCheckable(true);
    publishers_btn_->setChecked(true);
    publishers_btn_->setCursor(Qt::PointingHandCursor);
    connect(publishers_btn_, &QPushButton::clicked, this, [this]() { go(QStringLiteral("publishers"), true); });
    hl->addWidget(publishers_btn_);

    // Tab: Datasets — the last dataset list, else the recently updated datasets
    datasets_btn_ = new QPushButton(bar);
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, &GovDataUKPanel::go_datasets_tab);
    hl->addWidget(datasets_btn_);

    hl->addSpacing(6);

    // Popular shortcut (text set in retranslateUi)
    popular_btn_ = new QPushButton(bar);
    popular_btn_->setObjectName("govPopularBtn");
    popular_btn_->setCursor(Qt::PointingHandCursor);
    connect(popular_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_popular);
    hl->addWidget(popular_btn_);

    hl->addSpacing(10);

    // Search input (placeholder set in retranslateUi)
    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("govSearch");
    search_input_->setMinimumWidth(140);
    search_input_->setMaximumWidth(320);
    search_input_->setMinimumHeight(26);
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataUKPanel::on_search);
    hl->addWidget(search_input_, 1);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_fetch);
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataUKPanel::on_export_csv);
    hl->addWidget(export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataUKPanel::retranslateUi() {
    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (publishers_btn_)
        publishers_btn_->setText(tr("PUBLISHERS"));
    if (datasets_btn_)
        datasets_btn_->setText(tr("DATASETS"));
    if (popular_btn_)
        popular_btn_->setText(tr("POPULAR"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search datasets…"));

    if (publishers_table_)
        publishers_table_->setHorizontalHeaderLabels({tr("NAME")});
    if (datasets_table_)
        datasets_table_->setHorizontalHeaderLabels({tr("TITLE"), tr("FILES"), tr("MODIFIED"), tr("TAGS")});
    if (resources_table_)
        resources_table_->setHorizontalHeaderLabels({tr("NAME"), tr("FORMAT"), tr("SIZE"), tr("MODIFIED"), tr("OPEN")});

    // Re-render the breadcrumb so its current path picks up the new language.
    if (pane_)
        update_breadcrumb();
}

// ── Views and runs ───────────────────────────────────────────────────────────

GovDataUKPanel::Kind GovDataUKPanel::kind_of(const QString& key) {
    if (key == QLatin1String("popular"))
        return Kind::Popular;
    if (key == QLatin1String("recent"))
        return Kind::Recent;
    if (key.startsWith(QLatin1String("pub:")))
        return Kind::Publisher;
    if (key.startsWith(QLatin1String("search:")))
        return Kind::Search;
    if (key.startsWith(QLatin1String("res:")))
        return Kind::Resources;
    return Kind::Publishers;
}

QString GovDataUKPanel::arg_of(const QString& key) {
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    return colon < 0 ? QString() : key.mid(colon + 1);
}

bool GovDataUKPanel::valid_view(const QString& key) {
    switch (kind_of(key)) {
        case Kind::Publishers:
            return key == QLatin1String("publishers");
        case Kind::Popular:
        case Kind::Recent:
            return true;
        case Kind::Publisher:
        case Kind::Search:
        case Kind::Resources:
            return !arg_of(key).isEmpty();
    }
    return false;
}

QString GovDataUKPanel::restore_navigation() {
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
    if (valid_view(list) && (list_kind == Kind::Recent || list_kind == Kind::Publisher || list_kind == Kind::Search))
        last_list_view_ = list;
    const QString view = memory_->value(QStringLiteral("view")).toString();
    return valid_view(view) ? view : QString();
}

void GovDataUKPanel::remember_navigation() {
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

void GovDataUKPanel::go(const QString& key, bool user, bool force) {
    const Kind kind = kind_of(key);
    const QString arg = arg_of(key);
    if (kind == Kind::Recent || kind == Kind::Publisher || kind == Kind::Search)
        last_list_view_ = key;
    const bool reference = kind == Kind::Publishers || kind == Kind::Popular || kind == Kind::Resources;
    pane_->set_view_max_age(key, reference ? kGovUKRefMaxAgeS
                                 : kind == Kind::Recent ? kGovUKRecentMaxAgeS
                                                        : kGovUKDataMaxAgeS);
    pane_->set_view_skeleton(key, kind == Kind::Publishers || kind == Kind::Popular ? LoadingSkeleton::Shape::List
                                                                                    : LoadingSkeleton::Shape::Table);
    switch (kind) {
        case Kind::Publishers:
            pane_->set_loading_text(tr("Loading UK Government publishers…"));
            break;
        case Kind::Popular:
            pane_->set_loading_text(tr("Loading popular publishers…"));
            break;
        case Kind::Recent:
            pane_->set_loading_text(tr("Loading recently updated datasets from data.gov.uk…"));
            break;
        case Kind::Publisher:
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

void GovDataUKPanel::go_datasets_tab() {
    go(last_list_view_.isEmpty() ? QStringLiteral("recent") : last_list_view_, true);
}

void GovDataUKPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(QStringLiteral("publishers"), false);
}

void GovDataUKPanel::start_run(quint64 run) {
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString command;
    QStringList args;
    switch (kind_of(key)) {
        case Kind::Publishers:
            command = QStringLiteral("publishers");
            break;
        case Kind::Popular:
            command = QStringLiteral("popular-publishers");
            break;
        case Kind::Recent:
            command = QStringLiteral("recent-datasets");
            args << QStringLiteral("50");
            break;
        case Kind::Publisher:
            command = QStringLiteral("datasets");
            args << arg << QStringLiteral("100");
            break;
        case Kind::Search:
            command = QStringLiteral("search");
            args << arg << QStringLiteral("50");
            break;
        case Kind::Resources:
            command = QStringLiteral("resources");
            args << arg;
            break;
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(kGovDataUKScript, command, args, gov::request_id(kGovUKPrefix, run),
                                                 token);
}

void GovDataUKPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, kGovUKPrefix);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const Kind kind = kind_of(pane_->running_view());
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data")) {
            pane_->deliver(run, gov::list_payload(payload_kind(kind), QJsonArray()));
            return;
        }
        gov::fail_pane(pane_, run, result, QStringLiteral("data.gov.uk"));
        return;
    }
    // Shaped on the shaping thread (the publisher list has ~1,800 entries); dropped when superseded.
    gov::shape_async(
        this, [kind, data = result.data]() { return shape(kind, data); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run))
                pane_->deliver(run, payload);
        });
}

QString GovDataUKPanel::payload_kind(Kind kind) {
    if (kind == Kind::Publishers || kind == Kind::Popular)
        return QStringLiteral("publishers");
    return kind == Kind::Resources ? QStringLiteral("resources") : QStringLiteral("datasets");
}

QJsonObject GovDataUKPanel::shape(Kind kind, const QJsonObject& data) {
    int total = -1;
    const QJsonArray rows = gov::list_rows(data, &total);
    const QString pkind = payload_kind(kind);
    if (pkind == QLatin1String("publishers")) {
        QJsonArray pubs;
        for (const QJsonValue& v : rows) {
            const QJsonObject o = v.toObject();
            const QString id = gov::text(o.value("id")).isEmpty() ? gov::text(o.value("name")) : gov::text(o.value("id"));
            QString name;
            for (const char* k : {"display_name", "title", "name"}) {
                name = gov::text(o.value(QLatin1String(k)));
                if (!name.isEmpty())
                    break;
            }
            pubs.append(QJsonObject{{"name", name.isEmpty() ? id : name}, {"id", id}});
        }
        return gov::list_payload(pkind, pubs);
    }
    if (pkind == QLatin1String("resources"))
        return gov::list_payload(pkind, gov::compact_resources(rows));
    return gov::list_payload(pkind, gov::compact_datasets(rows), total);
}

// ── Render ────────────────────────────────────────────────────────────────────

void GovDataUKPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value("kind").toString();
    const QJsonArray rows = p.value("rows").toArray();
    const QString key = pane_->view();
    if (rows.isEmpty()) {
        row_count_label_->clear();
        show_no_data(kind_of(key), arg_of(key));
        return;
    }
    if (kind == QLatin1String("publishers")) {
        populate_publishers(rows);
        content_stack_->setCurrentWidget(publishers_page_);
    } else if (kind == QLatin1String("datasets")) {
        populate_datasets(rows, p.value("total").toInt(int(rows.size())));
        content_stack_->setCurrentWidget(datasets_page_);
    } else if (kind == QLatin1String("resources")) {
        populate_resources(rows);
        content_stack_->setCurrentWidget(resources_page_);
    }
}

void GovDataUKPanel::show_no_data(Kind kind, const QString& arg) {
    EmptyState::Spec spec;
    switch (kind) {
        case Kind::Publishers:
        case Kind::Popular:
            spec = EmptyState::no_data_spec(tr("No publishers listed"),
                                            tr("data.gov.uk returned an empty list. Try again later."), tr("RELOAD"));
            break;
        case Kind::Recent:
            spec = EmptyState::no_data_spec(tr("No recently updated datasets"),
                                            tr("data.gov.uk lists no recent datasets right now."),
                                            tr("SHOW PUBLISHERS"));
            break;
        case Kind::Publisher:
            spec = EmptyState::no_data_spec(tr("No datasets for %1").arg(labels_.value(pane_->view(), arg)),
                                            tr("This publisher has nothing on data.gov.uk."),
                                            tr("BACK TO PUBLISHERS"));
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

void GovDataUKPanel::populate_publishers(const QJsonArray& json) {
    current_publishers_ = json;
    pub_pager_->set_total(int(json.size()));
    render_publishers_page();
    row_count_label_->setText(tr("%1 publishers").arg(fmt::quantity(json.size())));
}

void GovDataUKPanel::render_publishers_page() {
    const int start = pub_pager_->offset();
    const int count = std::max(0, std::min<int>(pub_pager_->page_size(), int(current_publishers_.size()) - start));
    const QColor accent = colors::legible_color(QColor(kGovDataUKColor));
    gov::TableFill fill(publishers_table_, count);
    for (int i = 0; i < count; ++i) {
        const auto obj = current_publishers_[start + i].toObject();
        auto* name_item = new QTableWidgetItem(obj.value("name").toString());
        name_item->setData(Qt::UserRole, obj.value("id").toString());
        name_item->setData(Qt::UserRole + 1, obj.value("name").toString());
        name_item->setForeground(accent);
        publishers_table_->setItem(i, 0, name_item);
    }
}

void GovDataUKPanel::populate_datasets(const QJsonArray& json, int total_count) {
    current_datasets_ = json;
    ds_pager_->set_total(int(json.size()));
    render_datasets_page();
    row_count_label_->setText(
        tr("Showing %1 of %2").arg(fmt::quantity(json.size()), fmt::quantity(std::max<int>(total_count, json.size()))));
}

void GovDataUKPanel::render_datasets_page() {
    const int start = ds_pager_->offset();
    const int count = std::max(0, std::min<int>(ds_pager_->page_size(), int(current_datasets_.size()) - start));
    const QColor accent = colors::legible_color(QColor(kGovDataUKColor));
    const QColor secondary(colors::TEXT_SECONDARY());
    gov::TableFill fill(datasets_table_, count);
    for (int i = 0; i < count; ++i) {
        const auto obj = current_datasets_[start + i].toObject();
        auto* title_item = new QTableWidgetItem(obj.value("title").toString());
        title_item->setData(Qt::UserRole, obj.value("id").toString());
        const QString tip = gov::dataset_tooltip(obj);
        if (!tip.isEmpty())
            title_item->setToolTip(tip);
        datasets_table_->setItem(i, 0, title_item);

        const int files = obj.value("files").toInt(-1);
        auto* res_item = new QTableWidgetItem(files >= 0 ? fmt::quantity(files) : fmt::placeholder());
        res_item->setTextAlignment(Qt::AlignCenter);
        res_item->setForeground(accent);
        datasets_table_->setItem(i, 1, res_item);

        datasets_table_->setItem(i, 2, new QTableWidgetItem(obj.value("modified").toString()));

        QStringList tags;
        for (const QJsonValue& t : obj.value("tags").toArray())
            tags << t.toString();
        QString tags_str = tags.mid(0, 3).join(", ");
        if (tags.size() > 3)
            tags_str += QString(" +%1").arg(tags.size() - 3);
        auto* tag_item = new QTableWidgetItem(tags_str);
        tag_item->setForeground(secondary);
        tag_item->setToolTip(tags.join(", "));
        datasets_table_->setItem(i, 3, tag_item);
    }
}

void GovDataUKPanel::populate_resources(const QJsonArray& json) {
    current_resources_ = json;
    res_pager_->set_total(int(json.size()));
    render_resources_page();
    row_count_label_->setText(tr("%1 files").arg(fmt::quantity(json.size())));
}

void GovDataUKPanel::render_resources_page() {
    const int start = res_pager_->offset();
    const int count = std::max(0, std::min<int>(res_pager_->page_size(), int(current_resources_.size()) - start));
    const QColor accent = colors::legible_color(QColor(kGovDataUKColor));
    gov::TableFill fill(resources_table_, count);
    for (int i = 0; i < count; ++i) {
        const auto obj = current_resources_[start + i].toObject();
        resources_table_->setItem(i, 0, new QTableWidgetItem(obj.value("name").toString()));

        const QString format = obj.value("format").toString();
        auto* fmt_item = new QTableWidgetItem(format.isEmpty() ? fmt::placeholder() : format);
        fmt_item->setForeground(accent);
        fmt_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 1, fmt_item);

        auto* sz_item = new QTableWidgetItem(gov::file_size(obj.value("size").toDouble(0.0)));
        sz_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        resources_table_->setItem(i, 2, sz_item);

        resources_table_->setItem(i, 3, new QTableWidgetItem(obj.value("modified").toString()));

        const QString url = obj.value("url").toString();
        auto* url_item = new QTableWidgetItem(url.isEmpty() ? fmt::placeholder() : tr("↗ OPEN"));
        url_item->setData(Qt::UserRole, url);
        if (!url.isEmpty()) {
            url_item->setForeground(accent);
            url_item->setToolTip(url);
        }
        url_item->setTextAlignment(Qt::AlignCenter);
        resources_table_->setItem(i, 4, url_item);
    }
}

void GovDataUKPanel::recolor_items() {
    const QColor accent = colors::legible_color(QColor(kGovDataUKColor));
    recolor_column(publishers_table_, 0, accent);
    recolor_column(datasets_table_, 1, accent);
    recolor_column(datasets_table_, 3, QColor(colors::TEXT_SECONDARY()));
    recolor_column(resources_table_, 1, accent);
    recolor_column(resources_table_, 4, accent);
}

// ── Navigation slots ──────────────────────────────────────────────────────────

void GovDataUKPanel::on_publisher_doubleclicked(int row, int /*col*/) {
    auto* item = publishers_table_->item(row, 0);
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("pub:") + id;
    const QString name = item->data(Qt::UserRole + 1).toString();
    labels_.insert(key, name.isEmpty() ? item->text() : name);
    go(key, true);
}

void GovDataUKPanel::on_dataset_doubleclicked(int row, int /*col*/) {
    auto* item = datasets_table_->item(row, 0);
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty())
        return;
    const QString key = QStringLiteral("res:") + id;
    labels_.insert(key, item->text());
    go(key, true);
}

void GovDataUKPanel::on_fetch() {
    // Search text: search; otherwise reload what is on screen.
    const QString query = search_input_->text().trimmed();
    if (!query.isEmpty())
        go(QStringLiteral("search:") + query, true, true);
    else
        pane_->run();
}

void GovDataUKPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty())
        return;
    go(QStringLiteral("search:") + query, true);
}

void GovDataUKPanel::on_popular() {
    go(QStringLiteral("popular"), true);
}

void GovDataUKPanel::on_back() {
    const Kind kind = kind_of(pane_->view());
    if (kind == Kind::Resources && !last_list_view_.isEmpty())
        go(last_list_view_, true);
    else
        go(QStringLiteral("publishers"), true);
}

// ── Toolbar / breadcrumb state ────────────────────────────────────────────────

void GovDataUKPanel::update_toolbar_state() {
    const Kind kind = kind_of(pane_->view());
    back_btn_->setVisible(kind != Kind::Publishers && kind != Kind::Popular);
    publishers_btn_->setChecked(kind == Kind::Publishers || kind == Kind::Popular);
    datasets_btn_->setChecked(kind == Kind::Recent || kind == Kind::Publisher || kind == Kind::Search ||
                              kind == Kind::Resources);
}

void GovDataUKPanel::update_breadcrumb() {
    // The "  ›  " separator is wrapped together with the surrounding segments
    // so translators control spacing and punctuation per language.
    const QString key = pane_->view();
    const QString arg = arg_of(key);
    QString text;
    switch (kind_of(key)) {
        case Kind::Publishers:
            text = tr("All Publishers");
            break;
        case Kind::Popular:
            text = tr("All Publishers  ›  Popular");
            break;
        case Kind::Recent:
            text = tr("Datasets  ›  Recently updated");
            break;
        case Kind::Publisher:
            text = tr("All Publishers  ›  %1  ›  Datasets").arg(labels_.value(key, arg));
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

void GovDataUKPanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == publishers_page_)
        export_table_to_csv(publishers_table_, "uk_publishers.csv", this);
    else if (page == datasets_page_)
        export_table_to_csv(datasets_table_, "uk_datasets.csv", this);
    else if (page == resources_page_)
        export_table_to_csv(resources_table_, "uk_resources.csv", this);
}

} // namespace fincept::screens
