// src/screens/geopolitics/HDXDataPanel.cpp
#include "screens/geopolitics/HDXDataPanel.h"

#include "services/geopolitics/GeopoliticsService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QLocale>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QStackedWidget>
#include <QScrollArea>
#include <QUrl>

namespace fincept::screens {

using namespace fincept::services::geo;

/// Most datasets one view asks HDX for (LOAD MORE grows the limit in steps).
static constexpr int kHdxLoadStep = 20;
static constexpr int kHdxMaxLimit = 200;

/// HDX `dataset_date` is a Solr range "[2020-01-01T00:00:00 TO 2024-12-31T23:59:59]"
/// (an open end is "*"), sometimes a single date.
struct HdxDateRange {
    QDate from;
    QDate to;
    bool open_end = false;
};
static QDate hdx_parse_date(const QString& s) {
    const QString t = s.trimmed();
    const QDateTime dt = QDateTime::fromString(t, Qt::ISODate);
    return dt.isValid() ? dt.date() : QDate::fromString(t.left(10), Qt::ISODate);
}
static HdxDateRange hdx_date_range(const QString& raw) {
    QString t = raw.trimmed();
    if (t.startsWith(QLatin1Char('[')))
        t.remove(0, 1);
    if (t.endsWith(QLatin1Char(']')))
        t.chop(1);
    HdxDateRange r;
    const int sep = t.indexOf(QLatin1String(" TO "));
    if (sep < 0) {
        r.from = r.to = hdx_parse_date(t);
        return r;
    }
    r.from = hdx_parse_date(t.left(sep));
    const QString end = t.mid(sep + 4).trimmed();
    r.open_end = end == QLatin1String("*");
    r.to = r.open_end ? QDate() : hdx_parse_date(end);
    return r;
}

// Helper: the cyan accent at `a` opacity, as a stylesheet colour.
static QString cyan_wash(double a) {
    return ui::colors::alpha(ui::colors::CYAN, a);
}

/// A dataset as the AutoLoadPane payload: only what the table and its tooltips
/// use (the full CKAN record is large), so a view's result stays small enough
/// to be remembered across restarts.
static QJsonObject hdx_dataset_to_json(const HDXDataset& d) {
    QJsonObject o;
    o[QStringLiteral("id")] = d.id;
    o[QStringLiteral("title")] = d.title;
    o[QStringLiteral("organization")] = d.organization;
    o[QStringLiteral("notes")] = d.notes.left(600);
    o[QStringLiteral("dataset_date")] = d.date;
    o[QStringLiteral("num_resources")] = d.num_resources;
    o[QStringLiteral("tags")] = QJsonArray::fromStringList(d.tags);
    o[QStringLiteral("license_title")] = d.raw.value(QStringLiteral("license_title")).toString();
    o[QStringLiteral("name")] = d.raw.value(QStringLiteral("name")).toString();
    return o;
}

static HDXDataset hdx_dataset_from_json(const QJsonObject& o) {
    HDXDataset d;
    d.id = o.value(QStringLiteral("id")).toString();
    d.title = o.value(QStringLiteral("title")).toString();
    d.organization = o.value(QStringLiteral("organization")).toString();
    d.notes = o.value(QStringLiteral("notes")).toString();
    d.date = o.value(QStringLiteral("dataset_date")).toString();
    d.num_resources = o.value(QStringLiteral("num_resources")).toInt();
    for (const auto& t : o.value(QStringLiteral("tags")).toArray())
        d.tags.append(t.toString());
    d.raw.insert(QStringLiteral("license_title"), o.value(QStringLiteral("license_title")).toString());
    d.raw.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());
    return d;
}

HDXDataPanel::HDXDataPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
}

void HDXDataPanel::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::hdx_results_loaded, this, &HDXDataPanel::on_hdx_results);
    connect(&svc, &GeopoliticsService::hdx_total_count, this, &HDXDataPanel::on_hdx_total);
    // Without this the panel hangs on "Loading HDX data…" forever whenever the
    // fetch fails (and re-fires the failing request on every tab switch).
    connect(&svc, &GeopoliticsService::error_occurred, this, &HDXDataPanel::on_error);
}

void HDXDataPanel::on_error(const QString& context, const QString& message) {
    // GeopoliticsService::error_occurred is a shared channel — an events or
    // categories failure used to blank this table and replace it with an "HDX
    // failed" message that had nothing to do with HDX. Only react to our own
    // contexts.
    static const QStringList kHdxContexts = {QStringLiteral("conflicts"), QStringLiteral("humanitarian"),
                                             QStringLiteral("search"), QStringLiteral("topic"),
                                             QStringLiteral("country"), QStringLiteral("organization")};
    if (!kHdxContexts.contains(context) && !context.startsWith(QStringLiteral("hdx")))
        return;

    // Failures arrive as "hdx_<context>" ("hdx_country"), results as "<context>".
    const QString ctx = context.startsWith(QLatin1String("hdx_")) ? context.mid(4) : context;
    // The pane shows it in place of the table with RETRY (or keeps a remembered
    // result and says the refresh failed).
    if (outstanding_.value(ctx) > 0 && --outstanding_[ctx] > 0)
        return; // a newer search of this context is still out
    const quint64 id = pending_.take(ctx);
    if (id != 0)
        pane_->fail(id, tr("HDX data could not be loaded: %1 — adjust the query or retry.")
                            .arg(message.simplified().left(160)));
}

void HDXDataPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header with view tabs
    auto* header = new QWidget(this);
    header->setMinimumHeight(48);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    hhl->setSpacing(8);

    title_lbl_ = new QLabel(tr("HDX HUMANITARIAN DATA"), header);
    title_lbl_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                                  .arg(ui::colors::CYAN())
                                  .arg(ui::fonts::TINY.get())
                                  .arg(ui::fonts::DATA_FAMILY()));
    hhl->addWidget(title_lbl_);

    auto* div = new QWidget(header);
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    hhl->addWidget(div);

    const QStringList views = {tr("Conflicts"), tr("Humanitarian"), tr("Explorer"), tr("Datasets")};
    view_labels_ = views;
    for (int i = 0; i < views.size(); ++i) {
        auto* btn = new QPushButton(views[i].toUpper(), header);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                   "padding:4px 12px; border:none; background:transparent;"
                                   "font-weight:400; }"
                                   "QPushButton:hover { color:%4; background:%5; }")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::TINY.get())
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::colors::CYAN())
                               .arg(cyan_wash(0.04)));
        connect(btn, &QPushButton::clicked, this, [this, i]() { on_view_changed(i); });
        hhl->addWidget(btn);
        view_buttons_.append(btn);
    }

    hhl->addStretch();

    search_edit_ = new QLineEdit(header);
    search_edit_->setPlaceholderText(tr("Search HDX datasets..."));
    search_edit_->setFixedWidth(
        std::max(240, QFontMetrics(ui::fonts::font(-1)).horizontalAdvance(QString(28, QLatin1Char('0'))) + 24));
    search_edit_->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                        "font-family:%4; font-size:%5px; padding:4px 8px; }"
                                        "QLineEdit:focus { border-color:%6; }")
                                    .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                    .arg(ui::fonts::DATA_FAMILY)
                                    .arg(ui::fonts::SMALL.get())
                                    .arg(ui::colors::CYAN()));
    connect(search_edit_, &QLineEdit::returnPressed, this, [this]() {
        auto q = search_edit_->text().trimmed();
        if (!q.isEmpty()) {
            // Results arrive as the "search" context, which only the Datasets view
            // renders - searching from any other tab left the panel on "Loading"
            // forever. Switch there first (without its default query).
            set_active_view(3);
            search_query_ = q;
            limits_[QStringLiteral("search")] = kHdxLoadStep;
            search_now();
        }
    });
    hhl->addWidget(search_edit_);

    dataset_count_ = new QLabel(tr("0 datasets"), header);
    dataset_count_->setFixedHeight(std::max(22, QFontMetrics(ui::fonts::font(-2, QFont::Bold)).height() + 8));
    dataset_count_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:2px 6px;"
                                          "background:%4; border:1px solid %5; font-weight:700;")
                                      .arg(ui::colors::CYAN())
                                      .arg(ui::fonts::TINY.get())
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(cyan_wash(0.08))
                                      .arg(cyan_wash(0.25)));
    hhl->addWidget(dataset_count_);

    // HDX matches far more datasets than one page shows; fetch the next slice.
    load_more_btn_ = new QPushButton(tr("LOAD MORE"), header);
    load_more_btn_->setCursor(Qt::PointingHandCursor);
    load_more_btn_->setToolTip(tr("Fetch %1 more datasets for this view").arg(kHdxLoadStep));
    load_more_btn_->setStyleSheet(QString("QPushButton { color:%1; background:transparent; border:1px solid %2;"
                                          "font-family:%3; font-size:%4px; font-weight:700; padding:3px 10px; }"
                                          "QPushButton:hover { background:%5; }")
                                      .arg(ui::colors::CYAN())
                                      .arg(cyan_wash(0.4))
                                      .arg(ui::fonts::DATA_FAMILY())
                                      .arg(ui::fonts::TINY.get())
                                      .arg(cyan_wash(0.08)));
    load_more_btn_->hide();
    connect(load_more_btn_, &QPushButton::clicked, this, [this]() {
        const QString ctx = current_context();
        limits_[ctx] = std::min(kHdxMaxLimit, limits_.value(ctx, kHdxLoadStep) + kHdxLoadStep);
        pane_->run(); // the larger slice replaces the view's result
    });
    hhl->addWidget(load_more_btn_);

    root->addWidget(header);

    // Datasets table, inside the shared AutoLoadPane (HDX is free public data:
    // AutoRun). Each view (and each Explorer / Datasets query) keeps its own
    // remembered result, persisted for a week: reopening shows it at once with
    // its "as of" and refreshes it quietly. Inside the pane's content, a query
    // without matches is a designed state instead of an empty grid.
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, this);
    content_stack_ = new QStackedWidget(pane_->content_parent());
    datasets_table_ = new QTableWidget(content_stack_);
    datasets_table_->setColumnCount(5);
    datasets_table_->setHorizontalHeaderLabels(
        {tr("Title"), tr("Organization"), tr("Date"), tr("Resources"), tr("Tags")});
    datasets_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    datasets_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    datasets_table_->setAlternatingRowColors(true);
    datasets_table_->verticalHeader()->setVisible(false);
    datasets_table_->setSortingEnabled(true);

    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    datasets_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    datasets_table_->verticalHeader()->setDefaultSectionSize(std::max(26, QFontMetrics(ui::fonts::font(-1)).height() + 8));

    datasets_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                "font-family:%4; font-size:%5px; border:none; }"
                "QTableWidget::item { padding:3px 8px; }"
                "QTableWidget::item:selected { background:%6; }"
                "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                "padding:5px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                "QTableWidget::item:alternate { background:%9; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL.get())
            .arg(cyan_wash(0.15))
            .arg(ui::colors::BG_RAISED())
            .arg(ui::colors::TEXT_SECONDARY())
            .arg(ui::colors::ROW_ALT()));

    datasets_table_->setAccessibleName(tr("HDX datasets"));
    datasets_table_->setAccessibleDescription(
        tr("Humanitarian Data Exchange datasets. Press Enter on a row to open it on data.humdata.org."));
    // Open the dataset's HDX page. Every CKAN dataset is addressable by its
    // name/id, so a row is actionable rather than a dead end.
    connect(datasets_table_, &QTableWidget::cellActivated, this, [this](int row, int) {
        if (row < 0)
            return;
        auto* it = datasets_table_->item(row, 0);
        if (!it)
            return;
        const QString url = it->data(Qt::UserRole).toString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });

    content_stack_->addWidget(datasets_table_);
    message_state_ = new ui::EmptyState(content_stack_);
    message_state_->set_compact(true);
    content_stack_->addWidget(message_state_);
    pane_->set_content(content_stack_);
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    pane_->skeleton()->set_row_count(12);
    pane_->set_loading_text(tr("SEARCHING THE HUMANITARIAN DATA EXCHANGE…"));
    pane_->set_cache_key(QStringLiteral("geopolitics/hdx"), 7);
    pane_->set_start([this](quint64 id) { start_search(id); });
    pane_->set_cancel([this](quint64 id) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it.value() == id) {
                pending_.erase(it);
                break;
            }
        }
    });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_payload(payload); });
    root->addWidget(pane_, 1);

    // Explorer filter bar
    explorer_bar_ = new QWidget(this);
    explorer_bar_->setMinimumHeight(44);
    explorer_bar_->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* ehl = new QHBoxLayout(explorer_bar_);
    ehl->setContentsMargins(16, 0, 16, 0);
    ehl->setSpacing(8);

    auto combo_style = QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; }")
                           .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL.get());

    auto bar_label_style = QString("color:%1; font-size:%2px; font-family:%3; font-weight:700; letter-spacing:1px;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::TINY.get())
                               .arg(ui::fonts::DATA_FAMILY);

    country_lbl_ = new QLabel(tr("COUNTRY:"), explorer_bar_);
    country_lbl_->setStyleSheet(bar_label_style);
    ehl->addWidget(country_lbl_);
    country_combo_ = new QComboBox(explorer_bar_);
    country_combo_->setStyleSheet(combo_style);
    country_combo_->setEditable(true);
    for (const auto& r : critical_regions())
        country_combo_->addItem(r);
    // Start empty: a pre-selected country always won over the topic box, so the
    // topic could never be used without first clearing the country by hand.
    country_combo_->setCurrentIndex(-1);
    if (country_combo_->lineEdit())
        country_combo_->lineEdit()->setPlaceholderText(tr("Select country"));
    country_combo_->setToolTip(tr("Country (or ISO3 code). Leave empty to search by topic instead."));
    ehl->addWidget(country_combo_);

    topic_lbl_ = new QLabel(tr("TOPIC:"), explorer_bar_);
    topic_lbl_->setStyleSheet(bar_label_style);
    ehl->addWidget(topic_lbl_);
    topic_combo_ = new QComboBox(explorer_bar_);
    topic_combo_->setStyleSheet(combo_style);
    // Topic values are search query keys passed to the HDX service — not UI labels.
    topic_combo_->addItems(
        {"conflict", "humanitarian", "displacement", "food security", "health", "education", "refugees"});
    ehl->addWidget(topic_combo_);

    explore_btn_ = new QPushButton(tr("SEARCH"), explorer_bar_);
    explore_btn_->setCursor(Qt::PointingHandCursor);
    explore_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3;"
                                        "font-size:%4px; font-weight:700; border:none; padding:6px 16px; }"
                                        "QPushButton:hover { background:%5; }")
                                    .arg(ui::colors::CYAN())
                                    .arg(ui::colors::TEXT_ON_ACCENT())
                                    .arg(ui::fonts::DATA_FAMILY())
                                    .arg(ui::fonts::SMALL.get())
                                    .arg(cyan_wash(0.85)));
    connect(explore_btn_, &QPushButton::clicked, this, [this]() {
        auto country = country_combo_->currentText().trimmed();
        if (!country.isEmpty()) {
            explorer_ctx_ = QStringLiteral("country");
            explorer_query_ = country;
        } else {
            // The topic combo always holds a topic (it is not editable).
            explorer_ctx_ = QStringLiteral("topic");
            explorer_query_ = topic_combo_->currentText().trimmed();
        }
        limits_[explorer_ctx_] = kHdxLoadStep;
        search_now();
    });
    ehl->addWidget(explore_btn_);
    ehl->addStretch();

    root->addWidget(explorer_bar_);

    // Source + licence attribution. HDX redistribution terms require the
    // source and the per-dataset licence to travel with the data; the panel
    // previously showed dataset rows with neither.
    attribution_lbl_ = new QLabel(this);
    attribution_lbl_->setTextFormat(Qt::RichText);
    attribution_lbl_->setOpenExternalLinks(true);
    attribution_lbl_->setWordWrap(true);
    attribution_lbl_->setText(tr("Source: <a href=\"https://data.humdata.org\">Humanitarian Data Exchange (HDX)</a>, "
                                 "OCHA. Each dataset carries its own licence and attribution — open the dataset page "
                                 "before redistributing or citing it."));
    attribution_lbl_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:4px 16px;"
                                            "background:%4; border-top:1px solid %5;")
                                        .arg(ui::colors::TEXT_TERTIARY())
                                        .arg(ui::fonts::TINY.get())
                                        .arg(ui::fonts::DATA_FAMILY())
                                        .arg(ui::colors::BG_SURFACE())
                                        .arg(ui::colors::BORDER_DIM()));
    root->addWidget(attribution_lbl_);

    // The pane runs the view's search on its first show (never for a panel
    // nobody has looked at).
    set_active_view(0);
    show_view();
}

void HDXDataPanel::on_view_changed(int index) {
    set_active_view(index);
    show_view();
}

void HDXDataPanel::set_active_view(int index) {
    active_view_ = index;
    for (int i = 0; i < view_buttons_.size(); ++i) {
        const bool active = (i == index);
        if (active) {
            view_buttons_[i]->setStyleSheet(
                QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                        "padding:4px 12px;"
                        "border-bottom:2px solid %1; border-top:none; border-left:none; border-right:none;"
                        "background:%4; font-weight:700; }"
                        "QPushButton:hover { background:%5; }")
                    .arg(ui::colors::CYAN())
                    .arg(ui::fonts::TINY.get())
                    .arg(ui::fonts::DATA_FAMILY)
                    .arg(cyan_wash(0.06))
                    .arg(cyan_wash(0.10)));
        } else {
            view_buttons_[i]->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                                    "padding:4px 12px; border:none; background:transparent;"
                                                    "font-weight:400; }"
                                                    "QPushButton:hover { color:%4; background:%5; }")
                                                .arg(ui::colors::TEXT_SECONDARY())
                                                .arg(ui::fonts::TINY.get())
                                                .arg(ui::fonts::DATA_FAMILY)
                                                .arg(ui::colors::CYAN())
                                                .arg(cyan_wash(0.04)));
        }
    }

    if (explorer_bar_)
        explorer_bar_->setVisible(index == 2);
}

QString HDXDataPanel::view_key() const {
    switch (active_view_) {
        case 0:
            return QStringLiteral("conflicts");
        case 1:
            return QStringLiteral("humanitarian");
        case 2:
            return QStringLiteral("explorer/%1/%2").arg(explorer_ctx_, explorer_query_.trimmed().toLower());
        default:
            return QStringLiteral("search/%1").arg(search_query_.trimmed().toLower());
    }
}

void HDXDataPanel::show_view() {
    // Every view opens on something: the Explorer on the first watched crisis
    // region, Datasets on a broad humanitarian query (free; the user changes them).
    if (active_view_ == 2 && explorer_query_.trimmed().isEmpty()) {
        explorer_ctx_ = QStringLiteral("country");
        explorer_query_ = critical_regions().value(0);
        if (country_combo_)
            country_combo_->setCurrentText(explorer_query_);
    }
    if (active_view_ == 3 && search_query_.trimmed().isEmpty())
        search_query_ = QStringLiteral("humanitarian crisis conflict displacement");
    // The pane shows the view's remembered result (and refreshes it quietly) or
    // runs it behind a skeleton — on its first show only for a hidden panel.
    pane_->set_view(view_key());
}

void HDXDataPanel::search_now() {
    show_view();
    pane_->run();
}

void HDXDataPanel::explore_country(const QString& country) {
    if (country.trimmed().isEmpty())
        return;
    set_active_view(2);
    country_combo_->setCurrentText(country.trimmed());
    explorer_ctx_ = QStringLiteral("country");
    explorer_query_ = country.trimmed();
    search_now();
}

void HDXDataPanel::explore_topic(const QString& topic) {
    if (topic.trimmed().isEmpty())
        return;
    set_active_view(2);
    country_combo_->setCurrentText(QString()); // topic search runs only with no country
    const int idx = topic_combo_->findText(topic.trimmed(), Qt::MatchFixedString);
    if (idx >= 0)
        topic_combo_->setCurrentIndex(idx);
    explorer_ctx_ = QStringLiteral("topic");
    explorer_query_ = topic.trimmed();
    search_now();
}

void HDXDataPanel::explore_organization(const QString& org_slug) {
    if (org_slug.trimmed().isEmpty())
        return;
    set_active_view(2);
    country_combo_->setCurrentText(QString());
    explorer_ctx_ = QStringLiteral("organization");
    explorer_query_ = org_slug.trimmed();
    search_now();
}

QString HDXDataPanel::current_context() const {
    switch (active_view_) {
        case 0:
            return QStringLiteral("conflicts");
        case 1:
            return QStringLiteral("humanitarian");
        case 2:
            return explorer_ctx_;
        default:
            return QStringLiteral("search");
    }
}

void HDXDataPanel::start_search(quint64 id) {
    auto& svc = GeopoliticsService::instance();
    const QString ctx = current_context();
    const int limit = limits_.value(ctx, kHdxLoadStep);
    pending_[ctx] = id;
    ++outstanding_[ctx];
    if (ctx == QLatin1String("conflicts"))
        svc.search_hdx_conflicts(limit);
    else if (ctx == QLatin1String("humanitarian"))
        svc.search_hdx_humanitarian(limit);
    else if (ctx == QLatin1String("country"))
        svc.search_hdx_by_country(explorer_query_, limit);
    else if (ctx == QLatin1String("topic"))
        svc.search_hdx_by_topic(explorer_query_, limit);
    else if (ctx == QLatin1String("organization"))
        svc.search_hdx_by_organization(explorer_query_, limit);
    else
        svc.search_hdx_advanced(search_query_, limit);
}

void HDXDataPanel::on_hdx_total(const QString& context, int total) {
    totals_[context] = total;
}

void HDXDataPanel::on_hdx_results(const QString& context, QVector<HDXDataset> datasets) {
    // Answers carry only their context, not the query: while a newer search of
    // the same context is still out, this one is the older answer — drop it.
    if (outstanding_.value(context) > 0 && --outstanding_[context] > 0)
        return;
    // Only answers to this panel's own runs (the signal is service-wide); the
    // pane drops an answer whose run was superseded or cancelled.
    const quint64 id = pending_.take(context);
    if (id == 0)
        return;
    QJsonArray arr;
    for (const auto& d : datasets)
        arr.append(hdx_dataset_to_json(d));
    QJsonObject payload;
    payload[QStringLiteral("datasets")] = arr;
    payload[QStringLiteral("total")] = totals_.value(context, -1);
    payload[QStringLiteral("limit")] = limits_.value(context, kHdxLoadStep);
    pane_->deliver(id, payload);
}

void HDXDataPanel::render_payload(const QJsonValue& payload) {
    const QJsonObject o = payload.toObject();
    const QString ctx = current_context();
    const int total = o.value(QStringLiteral("total")).toInt(-1);
    if (total >= 0)
        totals_[ctx] = total;
    limits_[ctx] = o.value(QStringLiteral("limit")).toInt(kHdxLoadStep);
    QVector<HDXDataset> datasets;
    const QJsonArray arr = o.value(QStringLiteral("datasets")).toArray();
    datasets.reserve(arr.size());
    for (const auto& v : arr)
        datasets.append(hdx_dataset_from_json(v.toObject()));
    populate_table(datasets);
}

void HDXDataPanel::populate_table(const QVector<HDXDataset>& datasets) {
    datasets_table_->setSortingEnabled(false);
    datasets_table_->setRowCount(datasets.size());

    for (int i = 0; i < datasets.size(); ++i) {
        const auto& d = datasets[i];
        auto* title_item = new QTableWidgetItem(d.title);
        // Per-dataset licence + link. HDX carries licence metadata on the CKAN
        // record; surface whatever the payload has so the user can see the
        // terms without leaving the terminal, and stash the dataset URL for
        // the row-activation handler.
        const QString licence = d.raw.value(QStringLiteral("license_title")).toString();
        const QString name = d.raw.value(QStringLiteral("name")).toString();
        const QString url = !name.isEmpty() ? QStringLiteral("https://data.humdata.org/dataset/") + name
                            : !d.id.isEmpty() ? QStringLiteral("https://data.humdata.org/dataset/") + d.id
                                              : QString();
        title_item->setData(Qt::UserRole, url);
        QString tip = d.notes;
        if (!licence.isEmpty())
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n\n")) + tr("Licence: %1").arg(licence);
        if (!url.isEmpty())
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + url;
        title_item->setToolTip(tip);
        datasets_table_->setItem(i, 0, title_item);

        auto* org_item = new QTableWidgetItem(d.organization);
        if (!licence.isEmpty())
            org_item->setToolTip(tr("Licence: %1").arg(licence));
        datasets_table_->setItem(i, 1, org_item);

        // "Jan 1, 2020 → Dec 31, 2024" from the raw Solr range; sorts by the end
        // date (an open-ended range counts as today).
        namespace fmtx = fincept::ui::formatting;
        const HdxDateRange range = hdx_date_range(d.date);
        QString date_text;
        if (range.from.isValid() && range.open_end)
            date_text = tr("since %1").arg(fmtx::calendar_date(range.from));
        else if (range.from.isValid() && range.to.isValid() && range.from != range.to)
            date_text = fmtx::calendar_date(range.from) + QStringLiteral(" → ") + fmtx::calendar_date(range.to);
        else if (range.to.isValid() || range.from.isValid())
            date_text = fmtx::calendar_date(range.to.isValid() ? range.to : range.from);
        else
            date_text = d.date;
        const QDate sort_date = range.open_end ? QDate::currentDate() : (range.to.isValid() ? range.to : range.from);
        auto* date_item = new ui::NumericTableWidgetItem(
            date_text, sort_date.isValid() ? static_cast<double>(sort_date.toJulianDay()) : 0.0);
        date_item->setToolTip(d.date);
        datasets_table_->setItem(i, 2, date_item);

        auto* res_item = new ui::NumericTableWidgetItem(QLocale().toString(d.num_resources), d.num_resources);
        res_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        datasets_table_->setItem(i, 3, res_item);

        datasets_table_->setItem(i, 4, new QTableWidgetItem(d.tags.join(", ")));
    }

    datasets_table_->setSortingEnabled(true);
    // "20 of 1,768 datasets": HDX reports how many the search matched in all.
    const QString ctx = current_context();
    const int total = totals_.value(ctx, -1);
    const int shown = static_cast<int>(datasets.size());
    if (total > shown)
        dataset_count_->setText(tr("%1 of %2 datasets").arg(QLocale().toString(shown), QLocale().toString(total)));
    else
        dataset_count_->setText(tr("%1 datasets").arg(QLocale().toString(shown)));
    if (load_more_btn_)
        load_more_btn_->setVisible(total > shown && limits_.value(ctx, kHdxLoadStep) < kHdxMaxLimit);

    // Empty result is a state, not a silent blank grid.
    if (datasets.isEmpty()) {
        message_state_->set_glyph(QStringLiteral("∅"), ui::EmptyState::Tone::Info);
        message_state_->set_title(tr("No HDX datasets matched this query"));
        message_state_->set_message(tr("Try a broader term or a different country."));
        message_state_->set_note(tr("Free public data — no Fincept credits"), ui::EmptyState::Tone::Info);
        content_stack_->setCurrentWidget(message_state_);
    } else {
        content_stack_->setCurrentWidget(datasets_table_);
    }
}

void HDXDataPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void HDXDataPanel::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("HDX HUMANITARIAN DATA"));
    if (search_edit_)
        search_edit_->setPlaceholderText(tr("Search HDX datasets..."));
    if (pane_)
        pane_->set_loading_text(tr("SEARCHING THE HUMANITARIAN DATA EXCHANGE…"));

    // View-tab buttons — re-apply the fixed labels (upper-cased) in order.
    const QStringList views = {tr("Conflicts"), tr("Humanitarian"), tr("Explorer"), tr("Datasets")};
    view_labels_ = views;
    for (int i = 0; i < view_buttons_.size() && i < views.size(); ++i)
        if (view_buttons_[i])
            view_buttons_[i]->setText(views[i].toUpper());

    // Explorer filter bar
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY:"));
    if (topic_lbl_)
        topic_lbl_->setText(tr("TOPIC:"));
    if (country_combo_ && country_combo_->lineEdit())
        country_combo_->lineEdit()->setPlaceholderText(tr("Select country"));
    if (explore_btn_)
        explore_btn_->setText(tr("SEARCH"));

    // Table headers
    if (datasets_table_)
        datasets_table_->setHorizontalHeaderLabels(
            {tr("Title"), tr("Organization"), tr("Date"), tr("Resources"), tr("Tags")});

    if (attribution_lbl_)
        attribution_lbl_->setText(
            tr("Source: <a href=\"https://data.humdata.org\">Humanitarian Data Exchange (HDX)</a>, "
               "OCHA. Each dataset carries its own licence and attribution — open the dataset page "
               "before redistributing or citing it."));

    // dataset_count_ reflects the last populate_table() and refreshes on next load.
}

} // namespace fincept::screens
