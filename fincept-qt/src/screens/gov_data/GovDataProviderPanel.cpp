// src/screens/gov_data/GovDataProviderPanel.cpp
#include "screens/gov_data/GovDataProviderPanel.h"

#include "core/logging/Logger.h"
#include "services/gov_data/GovDataService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QFuture>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPoint>
#include <QPointer>
#include <QSignalBlocker>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

namespace fincept::screens {

using namespace fincept::ui;

namespace {

// How long a remembered view stays fresh before a quiet refresh on the next visit:
constexpr int kGovPortalListMaxAgeS = 7 * 24 * 3600; // the script's own portal table: changes with an app update
constexpr int kGovRefMaxAgeS = 24 * 3600;            // reference lists (publishers, categories, resources)
constexpr int kGovDataMaxAgeS = 6 * 3600;            // a publisher's datasets, search results
constexpr int kGovRecentMaxAgeS = 3600;              // "recently updated": changes through the day
constexpr int kGovRecentRows = 50;

QString gov_tr(const char* text) {
    return QCoreApplication::translate("fincept::screens::GovData", text);
}

/// The panels' shaping thread (gov::shape_async): one thread, so results keep their order. Owned by
/// the application object, whose destruction waits for a shaping in flight.
QThreadPool* gov_panel_shape_pool() {
    static QThreadPool* const pool = [] {
        auto* p = new QThreadPool(QCoreApplication::instance());
        p->setObjectName(QStringLiteral("GovDataShape"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

/// The requirement environment of a failure that says a key is missing: the
/// key is absent (whatever the store says), everything else is fine.
class GovKeyAbsentEnv final : public RequirementEnvironment {
  public:
    bool signed_in() const override { return true; }
    bool has_secret(const QString&) const override { return false; }
    bool llm_configured() const override { return true; }
    bool python_ready() const override { return true; }
    bool broker_connected(const QString&) const override { return true; }
    double credit_balance() const override { return -1; }
};

} // namespace

// ── Shared panel stylesheet ──────────────────────────────────────────────────

QString make_gov_panel_style(const QString& color, const QString& extra_qss) {
    const auto& t = ThemeManager::instance().tokens();
    // `color` is the provider's identity colour, picked against the dark theme. legible() keeps it
    // readable on the light theme and tint() washes it into the background; both are recorded, so
    // the central re-style pass re-derives them on a theme change (no theme_changed handler).
    const QColor provider(color);
    const QString accent = colors::legible(provider);
    const QString wash = colors::tint(provider, 0.12);
    const QString fill = colors::tint(provider, 0.22);
    const QString fill_hover = colors::tint(provider, 0.34);

    QString s;
    // Toolbar
    s += QString("#govPanelToolbar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_raised, t.border_dim);
    s += QString("#govToolbarSep { background:%1; }").arg(t.border_dim);
    s += QString("#govFilterLabel { color:%1; font-weight:700; letter-spacing:0.5px; background:transparent;"
                 " font-size:%2px; }")
             .arg(t.text_secondary)
             .arg(fonts::px(-5));

    // Tab buttons
    s += QString("#govTabBtn { background:transparent; color:%1; border:1px solid %2;"
                 " font-weight:700; padding:4px 12px; letter-spacing:0.5px; font-size:%3px; }")
             .arg(t.text_secondary, t.border_dim)
             .arg(fonts::px(-4));
    s += QString("#govTabBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);
    s += QString("#govTabBtn:checked { background:%1; color:%2; border:1px solid %2; }").arg(wash, accent);

    // Back button
    s += QString("#govBackBtn { background:transparent; color:%1; border:1px solid %2;"
                 " font-weight:700; padding:4px 10px; font-size:%3px; }")
             .arg(t.text_secondary, t.border_dim)
             .arg(fonts::px(-4));
    s += QString("#govBackBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Fetch / action buttons — a tinted fill with the accent as ink: both derived colours, so the
    // button follows a theme change without being rebuilt.
    s += QString("#govFetchBtn { background:%1; color:%2; border:1px solid %2;"
                 " font-weight:700; padding:4px 14px; font-size:%3px; }")
             .arg(fill, accent)
             .arg(fonts::px(-4));
    s += QString("#govFetchBtn:hover { background:%1; }").arg(fill_hover);
    s += QString("#govFetchBtn:disabled { background:%1; color:%2; border:1px solid %3; }")
             .arg(t.bg_surface, t.text_tertiary, t.border_dim);
    s += QString("#govCsvBtn { background:transparent; color:%1; border:1px solid %2;"
                 " font-weight:700; padding:4px 10px; font-size:%3px; }")
             .arg(t.text_secondary, t.border_dim)
             .arg(fonts::px(-4));
    s += QString("#govCsvBtn:hover { color:%1; background:%2; }").arg(t.text_primary, t.bg_hover);

    // Per-row "OPEN" link button in the resources table (styled here, never per row).
    s += QString("#govOpenBtn { color:%1; font-weight:700; background:transparent; border:none; padding:2px 6px;"
                 " font-size:%2px; }"
                 "#govOpenBtn:hover { color:%1; text-decoration:underline; }")
             .arg(accent)
             .arg(fonts::px(-4));

    // Search
    s += QString("#govSearch { background:%1; color:%2; border:none; border-bottom:1px solid %3; padding:4px 10px;"
                 " font-size:%4px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim)
             .arg(fonts::px(-3));
    s += QString("#govSearch:focus { border-bottom:1px solid %1; }").arg(accent);

    // Inputs of the filter bars (dates, combos, spin boxes)
    s += QString("QDateEdit, QSpinBox, QComboBox { background:%1; color:%2; border:1px solid %3; padding:2px 6px;"
                 " font-size:%4px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim)
             .arg(fonts::px(-3));
    s += "QDateEdit::drop-down, QComboBox::drop-down { border:none; }";
    s += QString("QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; }")
             .arg(t.bg_raised, t.text_primary, t.border_dim);

    // Tables (QTableView: the model-backed tables and the remaining QTableWidgets alike)
    s += QString("QTableView { background:%1; color:%2; border:none; gridline-color:%3;"
                 " alternate-background-color:%4; font-size:%5px; }")
             .arg(t.bg_base, t.text_primary, t.border_dim, t.bg_surface)
             .arg(fonts::px(-3));
    s += QString("QTableView::item { padding:5px 8px; border-bottom:1px solid %1; }").arg(t.border_dim);
    s += QString("QTableView::item:selected { background:%1; color:%2; }").arg(wash, accent);
    s += QString("QHeaderView::section { background:%1; color:%2; border:none; border-bottom:2px solid %3;"
                 " border-right:1px solid %3; padding:5px 8px; font-weight:700; letter-spacing:0.5px;"
                 " font-size:%4px; }")
             .arg(t.bg_raised, t.text_secondary, t.border_dim)
             .arg(fonts::px(-4));

    // Portal selector bar (multi-portal providers)
    s += QString("#govPortalBar { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govPortalLabel { color:%1; font-weight:700; letter-spacing:0.5px; background:transparent;"
                 " font-size:%2px; }")
             .arg(t.text_secondary)
             .arg(fonts::px(-5));
    s += QString("#govPortalCombo { background:%1; color:%2; border:1px solid %3; font-weight:700; padding:2px 8px;"
                 " font-size:%4px; }"
                 "#govPortalCombo::drop-down { border:none; width:18px; }")
             .arg(t.bg_raised, accent, t.border_dim)
             .arg(fonts::px(-4));
    s += QString("#govPortalCombo QAbstractItemView { background:%1; color:%2; selection-background-color:%3;"
                 " border:1px solid %4; }")
             .arg(t.bg_raised, t.text_primary, wash, t.border_dim);

    // Breadcrumb: clickable segments, the current one, separators, row count and "as of"
    s += QString("#govBreadcrumb { background:%1; border-bottom:1px solid %2; }").arg(t.bg_surface, t.border_dim);
    s += QString("#govBreadcrumbText { color:%1; background:transparent; font-size:%2px; }")
             .arg(t.text_secondary)
             .arg(fonts::px(-5));
    s += QString("#govBreadcrumbActive, #govCrumbActive { color:%1; font-weight:700; background:transparent;"
                 " font-size:%2px; }")
             .arg(accent)
             .arg(fonts::px(-5));
    s += QString("#govCrumb { color:%1; background:transparent; border:none; padding:0 2px; font-size:%2px; }"
                 "#govCrumb:hover { color:%3; text-decoration:underline; }")
             .arg(t.text_secondary)
             .arg(fonts::px(-5))
             .arg(t.text_primary);
    s += QString("#govCrumbSep { color:%1; background:transparent; padding:0 3px; font-size:%2px; }")
             .arg(t.text_dim)
             .arg(fonts::px(-5));
    s += QString("#govAsOf { color:%1; background:transparent; font-size:%2px; }")
             .arg(t.text_tertiary)
             .arg(fonts::px(-5));

    // Section heading inside a page (e.g. the Treasury summary breakdown)
    s += QString("#govSectionHdr { color:%1; font-weight:700; letter-spacing:1px; background:transparent;"
                 " font-size:%2px; }")
             .arg(accent)
             .arg(fonts::px(-5));
    s += QString("#govSummaryPage { background:%1; }").arg(t.bg_base);

    // Watermark bar (e.g. Swiss "opendata.swiss")
    s += QString("#govWatermarkBar { background:%1; border-top:1px solid %2; }").arg(t.bg_raised, t.border_dim);
    s += QString("#govWatermark { color:%1; background:transparent; font-size:%2px; }")
             .arg(t.text_secondary)
             .arg(fonts::px(-5));

    // Scrollbar
    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(t.bg_base);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(t.border_dim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    if (!extra_qss.isEmpty())
        s += extra_qss;
    return s;
}

// ── Shared helpers (namespace gov) ───────────────────────────────────────────

namespace gov {

QString file_size(double bytes) {
    if (!(bytes > 0))
        return formatting::placeholder();
    if (bytes < 1024.0)
        return gov_tr("%1 B").arg(formatting::number(bytes, 0));
    if (bytes < 1024.0 * 1024.0)
        return gov_tr("%1 KB").arg(formatting::number(bytes / 1024.0, 1));
    return gov_tr("%1 MB").arg(formatting::number(bytes / (1024.0 * 1024.0), 1));
}

AutoLoadPane* make_pane(QWidget* parent, const QString& cache_key, QLabel* status_label) {
    auto* pane = new AutoLoadPane(AutoLoadPane::Policy::AutoRun, parent);
    pane->set_cache_key(cache_key, 30);
    pane->skeleton()->set_shape(LoadingSkeleton::Shape::Table);
    pane->skeleton()->set_row_count(12);
    pane->set_auto_delay(200); // tabs users flick through do not each start a script
    pane->set_timeout(150);
    // Every view runs a Python script: without the environment the pane says so instead of failing.
    pane->set_requirements({Requirement::python()});
    if (status_label)
        pane->set_status_label(status_label);
    return pane;
}

QLabel* make_as_of_label(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("govAsOf"));
    return l;
}

void fail_pane(AutoLoadPane* pane, quint64 run, const services::GovDataResult& r, const QString& source) {
    if (!pane)
        return;
    const QString who = source.isEmpty() ? gov_tr("The portal") : source;
    const QString& code = r.error_code;
    if (code == QLatin1String("missing_key")) {
        if (!r.missing_key.isEmpty()) {
            const GovKeyAbsentEnv env;
            const RequirementStatus st = check_requirements({Requirement::api_key(r.missing_key, who)}, env);
            const SetupLink link = st.link;
            QPointer<QWidget> ctx(pane);
            pane->fail(run, EmptyState::needs_setup_spec(st), [link, ctx]() { open_setup_link(link, ctx); });
            return;
        }
        pane->fail(run, gov_tr("%1 needs an API key. Add it in Settings > Credentials.").arg(who),
                   gov_tr("Needs an API key"));
        return;
    }
    if (code == QLatin1String("invalid_key")) {
        // A key is saved but the source refused it: send the user to where it is kept.
        EmptyState::Spec spec;
        spec.glyph = QStringLiteral("!");
        spec.glyph_tone = EmptyState::Tone::Warning;
        spec.title = gov_tr("%1 rejected the API key").arg(who);
        spec.message = r.missing_key.isEmpty()
                           ? gov_tr("Check the key saved in Settings > Credentials, then try again.")
                           : gov_tr("Check %1 in Settings > Credentials, then try again.").arg(r.missing_key);
        spec.primary = gov_tr("OPEN SETTINGS > CREDENTIALS");
        spec.secondary = gov_tr("RETRY");
        SetupLink link;
        link.screen_id = QStringLiteral("settings");
        link.section = QStringLiteral("credentials");
        link.focus = r.missing_key;
        QPointer<AutoLoadPane> target(pane);
        pane->fail(
            run, spec, [link, target]() { open_setup_link(link, target); },
            [target]() {
                if (target)
                    target->run();
            });
        return;
    }
    if (code == QLatin1String("rate_limited")) {
        pane->fail(run,
                   r.retry_after_s > 0
                       ? gov_tr("%1 is limiting requests. Try again in %2 s.").arg(who).arg(r.retry_after_s)
                       : gov_tr("%1 is limiting requests. Wait a minute, then try again.").arg(who),
                   gov_tr("Too many requests"));
        return;
    }
    if (code == QLatin1String("timeout")) {
        pane->fail(run, gov_tr("%1 did not answer in time. Try again in a moment.").arg(who), gov_tr("Timed out"));
        return;
    }
    if (code == QLatin1String("unreachable")) {
        pane->fail(run, gov_tr("Could not reach %1. Check the internet connection and try again.").arg(who),
                   gov_tr("Source unreachable"));
        return;
    }
    const QString plain = plain_error(r.error);
    if (code == QLatin1String("not_supported")) {
        // Not offered by the source: a designed page without RETRY (it would fail the same way).
        EmptyState::Spec spec;
        spec.glyph = QStringLiteral("⊘");
        spec.glyph_tone = EmptyState::Tone::Warning;
        spec.title = gov_tr("Not available from %1").arg(who);
        spec.message = plain.isEmpty() ? gov_tr("The source does not offer this data.") : plain;
        pane->fail(run, spec, AutoLoadPane::ActionFn());
        return;
    }
    if (code == QLatin1String("bad_args")) {
        pane->fail(run, plain.isEmpty() ? gov_tr("The source rejected these inputs.") : plain,
                   gov_tr("Check the inputs"));
        return;
    }
    pane->fail(run, plain.isEmpty() ? gov_tr("%1 returned an error.").arg(who) : plain);
}

std::shared_ptr<std::atomic<bool>> start_token(QHash<quint64, std::shared_ptr<std::atomic<bool>>>& tokens,
                                               quint64 run) {
    auto token = std::make_shared<std::atomic<bool>>(false);
    tokens.insert(run, token);
    return token;
}

void cancel_token(QHash<quint64, std::shared_ptr<std::atomic<bool>>>& tokens, quint64 run) {
    if (auto token = tokens.take(run))
        token->store(true);
}

TableFill::TableFill(QTableWidget* table, int rows) : table_(table) {
    table_->setUpdatesEnabled(false);
    table_->setRowCount(0);
    table_->setRowCount(rows);
}

TableFill::~TableFill() {
    table_->scrollToTop();
    table_->setUpdatesEnabled(true);
}

EmptyState* make_no_data_page(QWidget* parent) {
    auto* page = new EmptyState(parent);
    page->setObjectName(QStringLiteral("govNoData"));
    return page;
}

void show_no_data(QStackedWidget* stack, EmptyState* page, const EmptyState::Spec& spec) {
    page->apply(spec);
    if (stack->currentWidget() != page)
        stack->setCurrentWidget(page);
}

GovTableModel* make_model(const QString& provider_color, QObject* parent) {
    auto* model = new GovTableModel(parent);
    const QColor provider(provider_color);
    // Resolved at paint time (GUI thread), so the ink always follows the active theme.
    model->set_ink([provider](GovTableModel::Ink ink) {
        return ink == GovTableModel::Ink::Accent ? colors::legible_color(provider) : QColor(colors::TEXT_SECONDARY());
    });
    // The visible cells repaint with the new ink once the appearance has settled (no item walk).
    QObject::connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, model,
                     [model]() { model->refresh_ink(); });
    return model;
}

QTableView* make_table_view(QWidget* parent, GovTableModel* model) {
    auto* view = new QTableView(parent);
    view->setModel(model);
    configure_table(view);
    return view;
}

void show_rows(QTableView* view, GovTableModel* model, const QJsonArray& rows) {
    model->set_rows(rows);
    view->scrollToTop();
}

QString tags_text(const QJsonObject& row) {
    const QJsonArray tags = row.value(QStringLiteral("tags")).toArray();
    QStringList first;
    for (qsizetype i = 0; i < tags.size() && i < 3; ++i)
        first << tags.at(i).toString();
    QString s = first.join(QStringLiteral(", "));
    if (tags.size() > 3)
        s += QStringLiteral(" +%1").arg(tags.size() - 3);
    return s;
}

QString dataset_tooltip(const QJsonObject& row) {
    const QString title_en = row.value(QStringLiteral("title_en")).toString();
    const QString desc = row.value(QStringLiteral("desc")).toString();
    if (title_en.isEmpty())
        return desc;
    const QString english = gov_tr("English: %1").arg(title_en);
    return desc.isEmpty() ? english : english + QStringLiteral("\n\n") + desc;
}

void shape_async(QObject* context, std::function<QJsonObject()> shape, std::function<void(const QJsonObject&)> done) {
    QtConcurrent::run(gov_panel_shape_pool(), [shape = std::move(shape)]() { return shape(); })
        .then(context, [done = std::move(done)](const QJsonObject& payload) { done(payload); });
}

} // namespace gov

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataProviderPanel::GovDataProviderPanel(const QString& script, const QString& provider_color,
                                           const QString& org_label, const GovProviderOptions& options, QWidget* parent)
    : QWidget(parent), script_(script), color_(provider_color), org_label_(org_label), options_(options) {
    prefix_ = QStringLiteral("gov.") + script_;
    setStyleSheet(make_gov_panel_style(color_)); // once, before any child exists
    build_ui();
    retranslateUi();

    connect(&services::GovDataService::instance(), &services::GovDataService::result_ready, this,
            &GovDataProviderPanel::on_result);
    // The tables are models whose ink is read from the live theme: no recolour walk on a theme change.

    // The last search term, the portal and the page on screen (list, publisher, dataset) come back
    // with the panel.
    QString memory_view = script_;
    if (memory_view.endsWith(QLatin1String(".py")))
        memory_view.chop(3);
    memory_ = new InputMemory(QStringLiteral("gov_data"), memory_view, this);
    memory_->track(QStringLiteral("search"), search_input_);
    memory_->restore();
    const QString first_view = restore_navigation();

    // The remembered page, else the landing view; it loads when the pane is first shown (nothing
    // starts here). A multi-portal panel shows its portal list first and opens the page after it.
    go(first_view.isEmpty() ? landing_view() : first_view, false);
}

// ── Remembered navigation ────────────────────────────────────────────────────

bool GovDataProviderPanel::valid_view(const QString& key) const {
    if (key.isEmpty() || key == QLatin1String("portals"))
        return false;
    if (multi_portal() && (portal_.isEmpty() || !key.startsWith(portal_ + QLatin1Char('|'))))
        return false;
    const ViewRef v = parse_view(key);
    switch (v.kind) {
        case Kind::Orgs:
            return true;
        case Kind::Recent:
            return has_recent();
        case Kind::Org:
        case Kind::Search:
        case Kind::Resources:
            return !v.arg.isEmpty();
        case Kind::Portals:
            break;
    }
    return false;
}

QString GovDataProviderPanel::restore_navigation() {
    if (!memory_)
        return {};
    const QVariantMap labels = memory_->value(QStringLiteral("labels")).toMap();
    for (auto it = labels.constBegin(); it != labels.constEnd(); ++it) {
        const QString label = it.value().toString();
        if (!it.key().isEmpty() && !label.isEmpty())
            labels_.insert(it.key(), label);
    }
    if (multi_portal()) {
        // The portal list loads first; populate_portals() selects this portal when it is offered,
        // and the page opens after the list (render of "portals").
        portal_ = memory_->value(QStringLiteral("portal")).toString();
        restore_view_ = memory_->value(QStringLiteral("view")).toString();
        restore_list_ = memory_->value(QStringLiteral("list")).toString();
        return {};
    }
    const QString list = memory_->value(QStringLiteral("list")).toString();
    if (valid_view(list)) {
        const Kind k = parse_view(list).kind;
        if (k == Kind::Recent || k == Kind::Org || k == Kind::Search)
            list_view_ = list;
    }
    const QString view = memory_->value(QStringLiteral("view")).toString();
    return valid_view(view) ? view : QString();
}

void GovDataProviderPanel::remember_navigation() {
    const QString key = pane_ ? pane_->view() : QString();
    if (!memory_ || !valid_view(key))
        return; // the portal list or a page of another portal: keep what is stored
    memory_->set_value(QStringLiteral("view"), key);
    memory_->set_value(QStringLiteral("list"), list_view_.isEmpty() ? QVariant() : QVariant(list_view_));
    // Only the names the remembered pages need (breadcrumb, captions).
    QVariantMap labels;
    for (const QString& k : {key, list_view_}) {
        const QString label = labels_.value(k);
        if (!k.isEmpty() && !label.isEmpty())
            labels.insert(k, label);
    }
    memory_->set_value(QStringLiteral("labels"), labels.isEmpty() ? QVariant() : QVariant(labels));
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataProviderPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Optional portal selector bar (multi-portal scripts, e.g. universal CKAN).
    if (multi_portal()) {
        auto* portal_bar = new QWidget(this);
        portal_bar->setObjectName("govPortalBar");
        portal_bar->setMinimumHeight(32);
        auto* phl = new QHBoxLayout(portal_bar);
        phl->setContentsMargins(14, 0, 14, 0);
        phl->setSpacing(8);
        portal_label_ = new QLabel(portal_bar);
        portal_label_->setObjectName("govPortalLabel");
        phl->addWidget(portal_label_);
        portal_combo_ = new QComboBox(portal_bar);
        portal_combo_->setObjectName("govPortalCombo");
        portal_combo_->setEnabled(false); // until the portal list has loaded
        portal_combo_->setAccessibleName(tr("CKAN portal"));
        portal_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        // activated = user picks only; populate_portals() fills the list without triggering a load.
        connect(portal_combo_, &QComboBox::activated, this, &GovDataProviderPanel::on_portal_changed);
        phl->addWidget(portal_combo_);
        phl->addStretch(1);
        root->addWidget(portal_bar);
    }

    root->addWidget(build_toolbar());

    // Breadcrumb bar — fixed segments, re-labelled on navigation
    auto* crumbs = new QWidget(this);
    crumbs->setObjectName("govBreadcrumb");
    crumbs->setMinimumHeight(24);
    auto* bcl = new QHBoxLayout(crumbs);
    bcl->setContentsMargins(14, 0, 14, 0);
    bcl->setSpacing(0);
    crumb_root_ = new QPushButton(crumbs);
    crumb_root_->setObjectName("govCrumb");
    crumb_root_->setFlat(true);
    crumb_root_->setCursor(Qt::PointingHandCursor);
    connect(crumb_root_, &QPushButton::clicked, this, [this]() { go(view_key(Kind::Orgs), true); });
    crumb_sep1_ = new QLabel(QStringLiteral("›"), crumbs);
    crumb_sep1_->setObjectName("govCrumbSep");
    crumb_list_ = new QPushButton(crumbs);
    crumb_list_->setObjectName("govCrumb");
    crumb_list_->setFlat(true);
    crumb_list_->setCursor(Qt::PointingHandCursor);
    connect(crumb_list_, &QPushButton::clicked, this, [this]() {
        if (!list_view_.isEmpty())
            go(list_view_, true);
    });
    crumb_sep2_ = new QLabel(QStringLiteral("›"), crumbs);
    crumb_sep2_->setObjectName("govCrumbSep");
    crumb_leaf_ = new QLabel(crumbs);
    crumb_leaf_->setObjectName("govCrumbActive");
    bcl->addWidget(crumb_root_);
    bcl->addWidget(crumb_sep1_);
    bcl->addWidget(crumb_list_);
    bcl->addWidget(crumb_sep2_);
    bcl->addWidget(crumb_leaf_);
    bcl->addStretch(1);
    as_of_label_ = gov::make_as_of_label(crumbs);
    bcl->addWidget(as_of_label_);
    bcl->addSpacing(12);
    row_count_label_ = new QLabel(crumbs);
    row_count_label_->setObjectName("govBreadcrumbText");
    bcl->addWidget(row_count_label_);
    root->addWidget(crumbs);

    build_pane(this);
    root->addWidget(pane_, 1);

    // Optional watermark bar at the bottom (e.g. Swiss "opendata.swiss"); styled by the panel sheet.
    if (!options_.watermark_text.isEmpty()) {
        auto* wm_bar = new QWidget(this);
        wm_bar->setObjectName("govWatermarkBar");
        wm_bar->setMinimumHeight(20);
        auto* whl = new QHBoxLayout(wm_bar);
        whl->setContentsMargins(14, 0, 14, 0);
        whl->addStretch(1);
        auto* wm_lbl = new QLabel(options_.watermark_text, wm_bar);
        wm_lbl->setObjectName("govWatermark");
        whl->addWidget(wm_lbl);
        root->addWidget(wm_bar);
    }
}

void GovDataProviderPanel::build_pane(QWidget* parent) {
    QString cache = script_;
    if (cache.endsWith(QLatin1String(".py")))
        cache.chop(3);
    pane_ = gov::make_pane(parent, QStringLiteral("gov_data/") + cache, as_of_label_);
    content_stack_ = new QStackedWidget(pane_->content_parent());

    using Ink = GovTableModel::Ink;
    const auto field = [](const char* key) {
        return [k = QString::fromLatin1(key)](const QJsonObject& r) { return r.value(k).toString(); };
    };

    // Orgs table: NAME | DATASETS (headers set in retranslateUi). Portals list hundreds of publishers.
    orgs_model_ = gov::make_model(color_, this);
    orgs_model_->set_columns({
        {.text = field("name"), .ink = Ink::Accent},
        {.text = [](const QJsonObject& r) {
             const int count = r.value(QStringLiteral("count")).toInt(-1);
             return count >= 0 ? formatting::quantity(count) : formatting::placeholder();
         },
         .align = Qt::AlignRight | Qt::AlignVCenter},
    });
    orgs_table_ = gov::make_table_view(content_stack_, orgs_model_);
    orgs_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    orgs_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    orgs_table_->setColumnWidth(1, 90);
    connect(orgs_table_, &QTableView::doubleClicked, this, [this](const QModelIndex& i) { on_org_clicked(i.row()); });
    content_stack_->addWidget(orgs_table_);

    // Datasets table: TITLE | FILES | MODIFIED | TAGS (headers set in retranslateUi)
    datasets_model_ = gov::make_model(color_, this);
    datasets_model_->set_columns({
        {.text = field("title"), .tooltip = &gov::dataset_tooltip},
        {.text = [](const QJsonObject& r) {
             const int files = r.value(QStringLiteral("files")).toInt(-1);
             return files >= 0 ? formatting::quantity(files) : formatting::placeholder();
         },
         .align = Qt::AlignCenter,
         .ink = Ink::Accent},
        {.text = field("modified")},
        {.text = &gov::tags_text,
         .tooltip = [](const QJsonObject& r) {
             QStringList tags;
             for (const QJsonValue& t : r.value(QStringLiteral("tags")).toArray())
                 tags << t.toString();
             return tags.join(QStringLiteral(", "));
         },
         .ink = Ink::Secondary},
    });
    datasets_table_ = gov::make_table_view(content_stack_, datasets_model_);
    datasets_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    datasets_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    datasets_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    datasets_table_->setColumnWidth(1, 60);
    datasets_table_->setColumnWidth(2, 100);
    datasets_table_->setColumnWidth(3, 200);
    connect(datasets_table_, &QTableView::doubleClicked, this,
            [this](const QModelIndex& i) { on_dataset_clicked(i.row()); });
    content_stack_->addWidget(datasets_table_);

    // Resources table: NAME | FORMAT | SIZE | MODIFIED | OPEN (headers set in retranslateUi). The OPEN
    // cell is a link (one click opens the file; a double-click or Enter on the row does too) — no
    // button widget per row.
    resources_model_ = gov::make_model(color_, this);
    resources_model_->set_columns({
        {.text = field("name")},
        {.text = [](const QJsonObject& r) {
             const QString format = r.value(QStringLiteral("format")).toString();
             return format.isEmpty() ? formatting::placeholder() : format;
         },
         .align = Qt::AlignCenter,
         .ink = Ink::Accent},
        {.text = [](const QJsonObject& r) { return gov::file_size(r.value(QStringLiteral("size")).toDouble(0)); },
         .align = Qt::AlignRight | Qt::AlignVCenter},
        {.text = field("modified")},
        {.text = [](const QJsonObject& r) {
             return r.value(QStringLiteral("url")).toString().isEmpty() ? formatting::placeholder()
                                                                        : GovDataProviderPanel::tr("↗ OPEN");
         },
         .tooltip = field("url"),
         .align = Qt::AlignCenter,
         .ink = Ink::Accent,
         .ink_if = [](const QJsonObject& r) { return !r.value(QStringLiteral("url")).toString().isEmpty(); },
         .export_text = field("url")},
    });
    resources_table_ = gov::make_table_view(content_stack_, resources_model_);
    resources_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 4; ++c)
        resources_table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    resources_table_->setColumnWidth(1, 70);
    resources_table_->setColumnWidth(2, 80);
    resources_table_->setColumnWidth(3, 100);
    resources_table_->setColumnWidth(4, 70);
    const auto open_resource = [this](int row) {
        const QString url = resources_model_->row(row).value(QStringLiteral("url")).toString();
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    };
    connect(resources_table_, &QTableView::clicked, this, [open_resource](const QModelIndex& i) {
        if (i.column() == 4)
            open_resource(i.row());
    });
    connect(resources_table_, &QTableView::activated, this, [open_resource](const QModelIndex& i) {
        if (i.column() != 4) // the OPEN cell already opened on the click
            open_resource(i.row());
    });
    content_stack_->addWidget(resources_table_);

    // A list that is legitimately empty says so, with the way out
    no_data_page_ = gov::make_no_data_page(content_stack_);
    connect(no_data_page_, &EmptyState::primary_clicked, this, [this]() {
        switch (parse_view(pane_->view()).kind) {
            case Kind::Orgs:
            case Kind::Portals:
                pane_->run();
                break;
            case Kind::Recent:
            case Kind::Org:
                go(view_key(Kind::Orgs), true);
                break;
            case Kind::Search:
                if (has_recent())
                    go(view_key(Kind::Recent), true);
                break;
            case Kind::Resources:
                on_back();
                break;
        }
    });
    content_stack_->addWidget(no_data_page_);

    if (multi_portal()) {
        opening_page_ = new LoadingSkeleton(LoadingSkeleton::Shape::Table, content_stack_);
        content_stack_->addWidget(opening_page_);
    }

    pane_->set_content(content_stack_);
    pane_->set_start([this](quint64 run) { start_run(run); });
    pane_->set_cancel([this](quint64 run) { gov::cancel_token(tokens_, run); });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
}

QWidget* GovDataProviderPanel::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("govPanelToolbar");
    bar->setMinimumHeight(36);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(5);

    // Back button (text set in retranslateUi)
    back_btn_ = new QPushButton(bar);
    back_btn_->setObjectName("govBackBtn");
    back_btn_->setVisible(false);
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_back);
    hl->addWidget(back_btn_);

    hl->addSpacing(4);

    // View tabs (no SEARCH tab — search is always available via the input)
    orgs_btn_ = new QPushButton(bar);
    orgs_btn_->setObjectName("govTabBtn");
    orgs_btn_->setCheckable(true);
    orgs_btn_->setCursor(Qt::PointingHandCursor);
    connect(orgs_btn_, &QPushButton::clicked, this, [this]() { go(view_key(Kind::Orgs), true); });
    hl->addWidget(orgs_btn_);

    datasets_btn_ = new QPushButton(bar);
    datasets_btn_->setObjectName("govTabBtn");
    datasets_btn_->setCheckable(true);
    datasets_btn_->setCursor(Qt::PointingHandCursor);
    connect(datasets_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::go_datasets_tab);
    hl->addWidget(datasets_btn_);

    // Separator between tab group and search/action group
    auto* sep = new QFrame(bar);
    sep->setObjectName("govToolbarSep");
    sep->setFrameShape(QFrame::NoFrame);
    sep->setFixedWidth(1);
    sep->setFixedHeight(20);
    hl->addSpacing(6);
    hl->addWidget(sep);
    hl->addSpacing(6);

    // Search input — always visible and active, press Enter to search
    search_input_ = new QLineEdit(bar);
    search_input_->setObjectName("govSearch");
    search_input_->setMinimumWidth(140);
    search_input_->setMaximumWidth(320);
    search_input_->setMinimumHeight(24);
    search_input_->setAccessibleName(tr("Search datasets"));
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::returnPressed, this, &GovDataProviderPanel::on_search);
    hl->addWidget(search_input_, 1);

    fetch_btn_ = new QPushButton(bar);
    fetch_btn_->setObjectName("govFetchBtn");
    fetch_btn_->setAccessibleName(tr("Fetch"));
    fetch_btn_->setCursor(Qt::PointingHandCursor);
    connect(fetch_btn_, &QPushButton::clicked, this, [this]() {
        const QString q = search_input_->text().trimmed();
        if (!q.isEmpty() && pane_->view() != view_key(Kind::Search, q))
            on_search();
        else
            pane_->run(); // refresh what is on screen
    });
    hl->addWidget(fetch_btn_);

    hl->addStretch(1);

    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName("govCsvBtn");
    export_btn_->setAccessibleName(tr("Export current view as CSV"));
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &GovDataProviderPanel::on_export_csv);
    hl->addWidget(export_btn_);

    // Explicit keyboard traversal: tabs → search → FETCH → CSV.
    QWidget::setTabOrder(orgs_btn_, datasets_btn_);
    QWidget::setTabOrder(datasets_btn_, search_input_);
    QWidget::setTabOrder(search_input_, fetch_btn_);
    QWidget::setTabOrder(fetch_btn_, export_btn_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

QString GovDataProviderPanel::org_label_translated() const {
    if (org_label_ == "Organizations")
        return tr("Organizations");
    return tr("Publishers");
}

QString GovDataProviderPanel::org_label_translated_upper() const {
    if (org_label_ == "Organizations")
        return tr("ORGANIZATIONS");
    return tr("PUBLISHERS");
}

QString GovDataProviderPanel::org_label_translated_lower() const {
    if (org_label_ == "Organizations")
        return tr("organizations");
    return tr("publishers");
}

QString GovDataProviderPanel::all_orgs_breadcrumb() const {
    if (org_label_ == "Organizations")
        return tr("All Organizations");
    return tr("All Publishers");
}

void GovDataProviderPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void GovDataProviderPanel::retranslateUi() {
    if (portal_label_)
        portal_label_->setText(tr("CKAN PORTAL:"));

    if (back_btn_)
        back_btn_->setText(tr("← BACK"));
    if (orgs_btn_)
        orgs_btn_->setText(org_label_translated_upper());
    if (datasets_btn_)
        datasets_btn_->setText(tr("DATASETS"));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search datasets…  ↵"));
    if (fetch_btn_)
        fetch_btn_->setText(tr("FETCH"));
    if (export_btn_)
        export_btn_->setText(tr("CSV"));

    if (orgs_model_)
        orgs_model_->set_headers({org_label_translated_upper(), tr("DATASETS")});
    if (datasets_model_)
        datasets_model_->set_headers({tr("TITLE"), tr("FILES"), tr("MODIFIED"), tr("TAGS")});
    if (resources_model_)
        resources_model_->set_headers({tr("NAME"), tr("FORMAT"), tr("SIZE"), tr("MODIFIED"), tr("OPEN")});

    if (pane_)
        update_breadcrumb();
}

// ── Views ────────────────────────────────────────────────────────────────────

QString GovDataProviderPanel::view_key(Kind kind, const QString& arg) const {
    QString k;
    switch (kind) {
        case Kind::Portals:
            return QStringLiteral("portals");
        case Kind::Orgs:
            k = QStringLiteral("orgs");
            break;
        case Kind::Recent:
            k = QStringLiteral("recent");
            break;
        case Kind::Org:
            k = QStringLiteral("org:") + arg;
            break;
        case Kind::Search:
            k = QStringLiteral("search:") + arg;
            break;
        case Kind::Resources:
            k = QStringLiteral("res:") + arg;
            break;
    }
    return multi_portal() ? portal_ + QLatin1Char('|') + k : k;
}

GovDataProviderPanel::ViewRef GovDataProviderPanel::parse_view(const QString& key) const {
    ViewRef v;
    if (key == QLatin1String("portals")) {
        v.kind = Kind::Portals;
        return v;
    }
    QString k = key;
    if (multi_portal()) {
        const qsizetype bar = k.indexOf(QLatin1Char('|'));
        if (bar >= 0)
            k = k.mid(bar + 1);
    }
    const qsizetype colon = k.indexOf(QLatin1Char(':'));
    const QString head = colon < 0 ? k : k.left(colon);
    v.arg = colon < 0 ? QString() : k.mid(colon + 1);
    if (head == QLatin1String("recent"))
        v.kind = Kind::Recent;
    else if (head == QLatin1String("org"))
        v.kind = Kind::Org;
    else if (head == QLatin1String("search"))
        v.kind = Kind::Search;
    else if (head == QLatin1String("res"))
        v.kind = Kind::Resources;
    else
        v.kind = Kind::Orgs;
    return v;
}

QString GovDataProviderPanel::landing_view() const {
    if (multi_portal() && !portals_loaded_)
        return view_key(Kind::Portals);
    if (options_.land_on_recent && has_recent())
        return view_key(Kind::Recent);
    return view_key(Kind::Orgs);
}

QString GovDataProviderPanel::list_label(const QString& key) const {
    const ViewRef v = parse_view(key);
    switch (v.kind) {
        case Kind::Recent:
            return tr("Recently updated");
        case Kind::Search:
            return tr("Search: \"%1\"").arg(v.arg);
        case Kind::Org:
            return labels_.value(key, v.arg);
        default:
            return {};
    }
}

void GovDataProviderPanel::go(const QString& key, bool user) {
    const ViewRef v = parse_view(key);
    // Age, skeleton and caption of the view's kind — set before the switch shows anything.
    int max_age = kGovDataMaxAgeS;
    LoadingSkeleton::Shape shape = LoadingSkeleton::Shape::Table;
    QString caption;
    switch (v.kind) {
        case Kind::Portals:
            max_age = kGovPortalListMaxAgeS;
            shape = LoadingSkeleton::Shape::List;
            caption = tr("Loading the portal list…");
            break;
        case Kind::Orgs:
            max_age = kGovRefMaxAgeS;
            shape = LoadingSkeleton::Shape::List;
            caption = tr("Loading %1…").arg(org_label_translated_lower());
            break;
        case Kind::Recent:
            max_age = kGovRecentMaxAgeS;
            caption = tr("Loading recently updated datasets…");
            break;
        case Kind::Org:
            caption = tr("Loading datasets for %1…").arg(labels_.value(key, v.arg));
            break;
        case Kind::Search:
            caption = tr("Searching for \"%1\"…").arg(v.arg);
            break;
        case Kind::Resources:
            max_age = kGovRefMaxAgeS;
            caption = tr("Loading resources…");
            break;
    }
    if (v.kind == Kind::Recent || v.kind == Kind::Org || v.kind == Kind::Search)
        list_view_ = key;
    pane_->set_view_max_age(key, max_age);
    pane_->set_view_skeleton(key, shape);
    pane_->set_loading_text(caption);
    pane_->set_view(key);
    // The user asked for it: a view with nothing remembered runs now (not after the auto delay
    // or behind other automatic loads).
    if (user && !pane_->has_result() && !(pane_->is_running() && pane_->running_view() == key))
        pane_->run();
    update_toolbar_state();
    update_breadcrumb();
    remember_navigation();
}

void GovDataProviderPanel::go_datasets_tab() {
    if (!list_view_.isEmpty()) {
        go(list_view_, true);
        return;
    }
    if (has_recent()) {
        go(view_key(Kind::Recent), true);
        return;
    }
    // No dataset list to show yet and no "recent" command: say how to get one.
    update_toolbar_state();
    QToolTip::showText(datasets_btn_->mapToGlobal(QPoint(0, datasets_btn_->height())),
                       tr("Double-click a publisher or search to list datasets"), datasets_btn_);
}

void GovDataProviderPanel::load_initial_data() {
    if (pane_->view().isEmpty())
        go(landing_view(), false);
}

void GovDataProviderPanel::start_run(quint64 run) {
    const ViewRef v = parse_view(pane_->view());
    QString command;
    QStringList args;
    switch (v.kind) {
        case Kind::Portals:
            command = options_.portals_command;
            break;
        case Kind::Orgs:
            command = QStringLiteral("publishers");
            break;
        case Kind::Recent:
            command = options_.recent_command;
            args << QString::number(kGovRecentRows);
            break;
        case Kind::Org:
            command = QStringLiteral("datasets");
            args << v.arg << QStringLiteral("100");
            break;
        case Kind::Search:
            command = QStringLiteral("search");
            args << v.arg << QStringLiteral("50");
            break;
        case Kind::Resources:
            command = QStringLiteral("resources");
            args << v.arg;
            break;
    }
    if (command.isEmpty()) {
        pane_->fail(run, tr("This portal has no list of recently updated datasets."));
        return;
    }
    if (multi_portal() && v.kind != Kind::Portals) {
        if (portal_.isEmpty()) {
            pane_->fail(run, tr("Choose a portal first."));
            return;
        }
        args.prepend(portal_);
    }
    const auto token = gov::start_token(tokens_, run);
    services::GovDataService::instance().execute(script_, command, args, gov::request_id(prefix_, run), token);
}

void GovDataProviderPanel::on_result(const QString& request_id, const services::GovDataResult& result) {
    const quint64 run = gov::run_of(request_id, prefix_);
    if (run == 0)
        return;
    tokens_.remove(run);
    if (!pane_->is_current(run))
        return; // superseded, cancelled, or its view was left
    const ViewRef v = parse_view(pane_->running_view());
    if (!result.success) {
        if (result.error_code == QLatin1String("no_data") && v.kind != Kind::Portals) {
            pane_->deliver(run, payload_for(v.kind, QJsonObject())); // an empty list: the no-data page
            return;
        }
        gov::fail_pane(pane_, run, result, options_.source_label);
        return;
    }
    // The answer is shaped (rows picked, compacted) on the shaping thread; a run superseded by then
    // is dropped (is_current).
    gov::shape_async(
        this, [kind = v.kind, data = result.data]() { return payload_for(kind, data); },
        [this, run](const QJsonObject& payload) {
            if (pane_->is_current(run))
                pane_->deliver(run, payload);
        });
}

QJsonObject GovDataProviderPanel::payload_for(Kind kind, const QJsonObject& data) {
    int total = -1;
    const QJsonArray rows = gov::list_rows(data, &total);
    switch (kind) {
        case Kind::Portals:
            return gov::list_payload(QStringLiteral("portals"), rows);
        case Kind::Orgs: {
            QJsonArray orgs;
            for (const QJsonValue& r : rows) {
                const QJsonObject o = r.toObject();
                const QString id = gov::first_string(o, {"id", "name"});
                QString name = gov::first_string(o, {"display_name", "title", "name"});
                if (name.isEmpty())
                    name = id;
                orgs.append(QJsonObject{{QStringLiteral("name"), name},
                                        {QStringLiteral("id"), id},
                                        {QStringLiteral("count"), gov::integer(o.value(QStringLiteral("dataset_count")))}});
            }
            return gov::list_payload(QStringLiteral("orgs"), orgs);
        }
        case Kind::Recent:
        case Kind::Org:
        case Kind::Search:
            return gov::list_payload(QStringLiteral("datasets"), gov::compact_datasets(rows), total);
        case Kind::Resources:
            return gov::list_payload(QStringLiteral("resources"), gov::compact_resources(rows));
    }
    return {};
}

void GovDataProviderPanel::render(const QJsonValue& payload) {
    const QJsonObject p = payload.toObject();
    const QString kind = p.value(QStringLiteral("kind")).toString();
    const QJsonArray rows = p.value(QStringLiteral("rows")).toArray();
    const ViewRef v = parse_view(pane_->view());

    if (kind == QLatin1String("portals")) {
        populate_portals(rows);
        if (!portals_loaded_) {
            gov::show_no_data(content_stack_, no_data_page_,
                              EmptyState::no_data_spec(tr("No portals"), tr("The script listed no CKAN portals."),
                                                       tr("RELOAD")));
            return;
        }
        if (opening_page_)
            content_stack_->setCurrentWidget(opening_page_);
        // Out of the render: open the page remembered for this portal, else its landing view.
        QTimer::singleShot(0, this, [this]() {
            if (parse_view(pane_->view()).kind != Kind::Portals)
                return;
            const QString remembered = restore_view_;
            const QString remembered_list = restore_list_;
            restore_view_.clear();
            restore_list_.clear();
            if (valid_view(remembered_list) && list_view_.isEmpty()) {
                const Kind k = parse_view(remembered_list).kind;
                if (k == Kind::Recent || k == Kind::Org || k == Kind::Search)
                    list_view_ = remembered_list;
            }
            go(valid_view(remembered) ? remembered : landing_view(), false);
        });
        return;
    }

    if (rows.isEmpty()) {
        row_count_label_->clear();
        show_no_data(v);
        update_toolbar_state();
        update_breadcrumb();
        return;
    }
    if (kind == QLatin1String("orgs")) {
        populate_orgs(rows);
        content_stack_->setCurrentWidget(orgs_table_);
    } else if (kind == QLatin1String("datasets")) {
        populate_datasets(rows, p.value(QStringLiteral("total")).toInt(int(rows.size())));
        content_stack_->setCurrentWidget(datasets_table_);
    } else if (kind == QLatin1String("resources")) {
        populate_resources(rows);
        content_stack_->setCurrentWidget(resources_table_);
    }
    update_toolbar_state();
    update_breadcrumb();
}

void GovDataProviderPanel::show_no_data(const ViewRef& v) {
    EmptyState::Spec spec;
    switch (v.kind) {
        case Kind::Portals:
        case Kind::Orgs:
            spec = EmptyState::no_data_spec(tr("No %1 listed").arg(org_label_translated_lower()),
                                            tr("The portal returned an empty list. Try again later."), tr("RELOAD"));
            break;
        case Kind::Recent:
            spec = EmptyState::no_data_spec(tr("No recently updated datasets"),
                                            tr("The portal lists no recent datasets right now. Browse the %1 instead.")
                                                .arg(org_label_translated_lower()),
                                            tr("SHOW %1").arg(org_label_translated_upper()));
            break;
        case Kind::Org:
            spec = EmptyState::no_data_spec(tr("No datasets for %1").arg(labels_.value(pane_->view(), v.arg)),
                                            tr("Nothing is published under this name on the portal."),
                                            tr("BACK TO %1").arg(org_label_translated_upper()));
            break;
        case Kind::Search:
            spec = EmptyState::no_data_spec(tr("No datasets match \"%1\"").arg(v.arg),
                                            tr("Try a shorter or a different term."),
                                            has_recent() ? tr("SHOW RECENT DATASETS") : QString());
            break;
        case Kind::Resources:
            spec = EmptyState::no_data_spec(tr("No files in this dataset"),
                                            tr("The dataset lists no downloadable resources."), tr("BACK"));
            break;
    }
    spec.glyph = QStringLiteral("∅");
    gov::show_no_data(content_stack_, no_data_page_, spec);
}

void GovDataProviderPanel::populate_portals(const QJsonArray& portals) {
    if (!portal_combo_)
        return;
    QSignalBlocker block(portal_combo_);
    portal_combo_->clear();
    int default_index = -1;
    for (const QJsonValue& v : portals) {
        const QJsonObject p = v.toObject();
        const QString code = p.value("code").toString();
        if (code.isEmpty())
            continue;
        portal_combo_->addItem(p.value("name").toString(code), code);
        if (p.value("default").toBool(false))
            default_index = portal_combo_->count() - 1;
    }
    if (portal_combo_->count() == 0)
        return;
    // Keep the user's portal across a reload of the list; else the script's default; else the first.
    int idx = portal_.isEmpty() ? -1 : portal_combo_->findData(portal_);
    if (idx < 0)
        idx = default_index >= 0 ? default_index : 0;
    portal_combo_->setCurrentIndex(idx);
    portal_combo_->setEnabled(true);
    portal_ = portal_combo_->itemData(idx).toString();
    portals_loaded_ = true;
}

void GovDataProviderPanel::on_portal_changed(int index) {
    if (!portal_combo_ || index < 0)
        return;
    const QString code = portal_combo_->itemData(index).toString();
    if (code.isEmpty() || code == portal_)
        return;
    portal_ = code;
    list_view_.clear(); // belonged to the previous portal
    restore_view_.clear();
    restore_list_.clear();
    if (memory_)
        memory_->set_value(QStringLiteral("portal"), code);
    go(has_recent() ? view_key(Kind::Recent) : view_key(Kind::Orgs), true);
}

// ── Populate tables ──────────────────────────────────────────────────────────

void GovDataProviderPanel::populate_orgs(const QJsonArray& json) {
    gov::show_rows(orgs_table_, orgs_model_, json); // a model reset: no per-row work here
    // org_label_ resolves to either "Publishers" or "Organizations" — separate
    // tr() calls let translators provide proper plural forms per language.
    if (org_label_ == "Organizations")
        row_count_label_->setText(tr("%1 organizations").arg(formatting::quantity(json.size())));
    else
        row_count_label_->setText(tr("%1 publishers").arg(formatting::quantity(json.size())));
}

void GovDataProviderPanel::populate_datasets(const QJsonArray& json, int total_count) {
    gov::show_rows(datasets_table_, datasets_model_, json);
    row_count_label_->setText(
        tr("Showing %1 of %2").arg(formatting::quantity(json.size()), formatting::quantity(total_count)));
}

void GovDataProviderPanel::populate_resources(const QJsonArray& json) {
    gov::show_rows(resources_table_, resources_model_, json);
    row_count_label_->setText(tr("%1 files").arg(formatting::quantity(json.size())));
}

// ── View navigation ──────────────────────────────────────────────────────────

void GovDataProviderPanel::on_org_clicked(int row) {
    const QJsonObject org = orgs_model_->row(row);
    const QString id = org.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = view_key(Kind::Org, id);
    labels_.insert(key, org.value(QStringLiteral("name")).toString());
    go(key, true);
}

void GovDataProviderPanel::on_dataset_clicked(int row) {
    const QJsonObject dataset = datasets_model_->row(row);
    const QString id = dataset.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;
    const QString key = view_key(Kind::Resources, id);
    labels_.insert(key, dataset.value(QStringLiteral("title")).toString());
    go(key, true);
}

void GovDataProviderPanel::on_search() {
    const QString query = search_input_->text().trimmed();
    if (query.isEmpty())
        return;
    go(view_key(Kind::Search, query), true);
}

void GovDataProviderPanel::on_back() {
    const Kind kind = parse_view(pane_->view()).kind;
    if (kind == Kind::Resources && !list_view_.isEmpty())
        go(list_view_, true);
    else
        go(view_key(Kind::Orgs), true);
}

void GovDataProviderPanel::update_toolbar_state() {
    const Kind kind = parse_view(pane_->view()).kind;
    back_btn_->setVisible(kind == Kind::Recent || kind == Kind::Org || kind == Kind::Search ||
                          kind == Kind::Resources);
    // The tab highlight follows the page actually on screen (the tabs are checkable buttons that
    // toggle themselves on click).
    orgs_btn_->setChecked(kind == Kind::Orgs);
    datasets_btn_->setChecked(kind == Kind::Recent || kind == Kind::Org);
}

void GovDataProviderPanel::update_breadcrumb() {
    const QString key = pane_->view();
    const Kind kind = parse_view(key).kind;
    crumb_root_->setText(all_orgs_breadcrumb());
    const bool at_root = kind == Kind::Orgs || kind == Kind::Portals;
    const bool at_list = kind == Kind::Recent || kind == Kind::Org || kind == Kind::Search;
    const bool at_res = kind == Kind::Resources;
    crumb_root_->setVisible(!at_root);
    crumb_sep1_->setVisible(!at_root);
    crumb_list_->setVisible(at_res && !list_view_.isEmpty());
    crumb_sep2_->setVisible(at_res && !list_view_.isEmpty());
    if (at_root)
        crumb_leaf_->setText(all_orgs_breadcrumb());
    else if (at_list)
        crumb_leaf_->setText(list_label(key));
    else if (at_res) {
        crumb_list_->setText(list_label(list_view_));
        const QString title = labels_.value(key);
        crumb_leaf_->setText(title.isEmpty() ? tr("Resources") : tr("Resources · %1").arg(title.left(60)));
    }
}

// ── CSV export ───────────────────────────────────────────────────────────────

void GovDataProviderPanel::on_export_csv() {
    QWidget* page = content_stack_->currentWidget();
    if (page == orgs_table_)
        export_table_to_csv(orgs_table_, "gov_" + org_label_.toLower() + ".csv", this);
    else if (page == datasets_table_)
        export_table_to_csv(datasets_table_, "gov_datasets.csv", this);
    else if (page == resources_table_)
        export_table_to_csv(resources_table_, "gov_resources.csv", this);
}

// ── Shared CSV export utility (used by all gov_data panels) ─────────────────

void export_table_to_csv(QAbstractItemView* table, const QString& default_name, QWidget* parent) {
    QAbstractItemModel* model = table ? table->model() : nullptr;
    if (!model || model->rowCount() == 0)
        return;

    const QString path = QFileDialog::getSaveFileName(
        parent, QCoreApplication::translate("GovDataProviderPanel", "Export CSV"), default_name,
        QCoreApplication::translate("GovDataProviderPanel", "CSV Files (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_WARN("GovDataProviderPanel",
                 logf("CSV export failed", {{"path", log_path(path)}, {"error", file.errorString()}}));
        QMessageBox::warning(parent, QCoreApplication::translate("GovDataProviderPanel", "Export failed"),
                             QCoreApplication::translate("GovDataProviderPanel", "Unable to open file for writing."));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const int columns = model->columnCount();
    QStringList headers;
    for (int c = 0; c < columns; ++c) {
        const QString h = model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
        headers << (h.isEmpty() ? QString::number(c) : h);
    }
    out << headers.join(",") << "\n";

    const int rows = model->rowCount();
    for (int r = 0; r < rows; ++r) {
        QStringList row;
        for (int c = 0; c < columns; ++c) {
            const QModelIndex idx = model->index(r, c);
            // A model cell may carry its own export text (the URL behind "↗ OPEN").
            const QVariant exported = model->data(idx, GovTableModel::ExportRole);
            QString val = exported.isValid() ? exported.toString() : model->data(idx, Qt::DisplayRole).toString();
            if (val.contains(',') || val.contains('"') || val.contains('\n'))
                val = "\"" + val.replace("\"", "\"\"") + "\"";
            row << val;
        }
        out << row.join(",") << "\n";
    }
}

void configure_table(QTableView* table) {
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    // 34px rows at the default font size, growing with the user's font size.
    const auto row_height = [] { return qMax(34, QFontMetrics(fonts::font(-3)).height() + 18); };
    table->verticalHeader()->setDefaultSectionSize(row_height());
    // A size change re-fits on appearance_settled, never inside a theme_changed handler.
    QObject::connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, table,
                     [table, row_height]() { table->verticalHeader()->setDefaultSectionSize(row_height()); });
    table->verticalHeader()->setMinimumSectionSize(30);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->viewport()->setCursor(Qt::PointingHandCursor);
}

void recolor_column(QTableWidget* table, int column, const QColor& color) {
    if (!table)
        return;
    for (int r = 0; r < table->rowCount(); ++r) {
        auto* item = table->item(r, column);
        if (item && item->data(Qt::ForegroundRole).isValid()) // only items that carry a brush
            item->setForeground(color);
    }
}

} // namespace fincept::screens
