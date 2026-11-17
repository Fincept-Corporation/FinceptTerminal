// src/screens/ai_quant_lab/QuantModulePanel_AutoLoad.cpp
//
// The views of a module panel and the ONE result pane they share.
//
// A panel builder declares each sub-view once:
//
//   auto* tabs = make_view_tabs(w);
//   add_view_tab(tabs, {.key = "greeks", .cost = ViewCost::Free, .loading = tr("Pricing the option…")},
//                tr("Greeks"), [this](QWidget* page, QVBoxLayout* layout) {
//       ... the form ...
//       auto* run = make_run_button(tr("CALCULATE GREEKS"), page);
//       set_action(run, [this]() { ... validate → display_error(); show_loading(); service call ... });
//       layout->addWidget(run);
//   });
//   vl->addWidget(tabs);
//   add_result_pane(vl);
//
// and everything else is here:
//   * Pages are built on first view (the visible one at once). Their named inputs (the
//     text_inputs_ / double_inputs_ / … entries the form creates) are remembered per view
//     (ui::InputMemory "inputs/ai_quant_lab/<module>/<view>") and restored before anything runs.
//   * One ui::AutoLoadPane under the forms. A tab switch selects the view: its remembered result
//     at once with "as of", else its default action (Free / Fetch: on first show, through the
//     app-wide gate; Heavy: a designed preview with one primary button), a skeleton while it runs,
//     CANCEL after 3 s, an error page with RETRY, "Inputs changed — RUN AGAIN".
//   * The action is the RUN button's own lambda. It runs inside a RunContext and a service
//     RequestScope: display_error() fails the pane run ("Check the inputs"), show_loading() sets
//     the skeleton caption, and every script the service starts is tagged with a request id and a
//     cancel token. Replies come back on request_finished / request_failed and are delivered to the
//     pane run that asked (late replies of another tab or a cancelled run are dropped); leaving a
//     view cancels its default run (the script is killed). A result is kept per view.
//   * render = the module's existing display_*_result renderer into the pane's content.
#include "screens/ai_quant_lab/QuantModulePanel.h"

#include "core/logging/Logger.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"
#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/Requirements.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QLocale>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSet>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTimer>
#include <QVariantMap>

#include <utility>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

namespace {

constexpr int kAqlFetchMaxAgeS = 6 * 3600;    // daily data: a remembered result is refreshed after 6 h
constexpr int kAqlPersistBytes = 1024 * 1024; // results kept across sessions up to 1 MB
constexpr int kAqlFreeTimeoutS = 90;
constexpr int kAqlFetchTimeoutS = 240;
constexpr int kAqlAutoDelayMs = 300;          // sub-tabs are flicked through

/// A Python failure as one plain sentence: the exception line of a traceback without its type,
/// capped. The whole text is in the log (AIQuantLabService logs every failure).
QString aql_plain_error(const QString& raw) {
    QString text = raw.trimmed();
    if (text.isEmpty())
        return QuantModulePanel::tr("The script ended without saying why.");
    if (text.contains(QLatin1String("Traceback (most recent call last)")) || text.count(QLatin1Char('\n')) > 6) {
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (int i = lines.size() - 1; i >= 0; --i) {
            const QString l = lines.at(i).trimmed();
            if (l.isEmpty() || l.startsWith(QLatin1String("File ")) || l.startsWith(QLatin1Char('^')) ||
                l.startsWith(QLatin1Char('~')))
                continue;
            text = l;
            break;
        }
        static const QRegularExpression exc(QStringLiteral("^[A-Za-z_.]*(Error|Exception)\\s*:\\s*"));
        text.remove(exc);
    }
    if (text.size() > 400)
        text = text.left(400) + QStringLiteral("…");
    return text;
}

/// The message of a {"success": false, …} reply.
QString aql_reply_error(const QJsonObject& data) {
    for (const char* key : {"error", "message", "detail", "reason"}) {
        const QJsonValue v = data.value(QLatin1String(key));
        if (v.isString() && !v.toString().trimmed().isEmpty())
            return v.toString();
        if (v.isObject()) {
            const QString inner = v.toObject().value(QStringLiteral("message")).toString();
            if (!inner.isEmpty())
                return inner;
        }
    }
    return QuantModulePanel::tr("The script reported a failure without a message.");
}

/// Current value of a form input, for the preview's "your inputs" lines.
QString aql_input_text(QWidget* w) {
    if (auto* le = qobject_cast<QLineEdit*>(w))
        return le->text().trimmed();
    if (auto* cb = qobject_cast<QComboBox*>(w))
        return cb->currentText();
    if (auto* sb = qobject_cast<QAbstractSpinBox*>(w))
        return sb->text();
    if (auto* te = qobject_cast<QTextEdit*>(w))
        return te->toPlainText().trimmed();
    if (auto* ck = qobject_cast<QCheckBox*>(w))
        return ck->isChecked() ? QuantModulePanel::tr("yes") : QuantModulePanel::tr("no");
    return {};
}

/// "2026-11-17T10:30:33" → the locale's short date and time; other text unchanged.
QString aql_when(const QString& iso) {
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? QLocale().toString(dt, QLocale::ShortFormat) : iso;
}

/// The tooltip of a saved-id combo item: what the model / pipeline is (list_trained_models /
/// list_pipelines rows).
QString aql_saved_id_tip(const QJsonObject& row, bool model) {
    QStringList lines;
    if (model) {
        QString head = row.value(QStringLiteral("model_type")).toString();
        const QString handler = row.value(QStringLiteral("handler")).toString();
        if (!handler.isEmpty())
            head += QStringLiteral(" · ") + handler;
        lines << head;
        const QString ds_id = row.value(QStringLiteral("dataset_id")).toString();
        const QString ds_name = row.value(QStringLiteral("dataset_name")).toString();
        QString ds = ds_name.isEmpty() || ds_name == ds_id ? ds_id : QStringLiteral("%1 (%2)").arg(ds_name, ds_id);
        if (!row.value(QStringLiteral("dataset_available")).toBool(true))
            ds = QuantModulePanel::tr("%1 — not on this machine any more").arg(ds);
        lines << QuantModulePanel::tr("Dataset: %1").arg(ds);
        QStringList names;
        for (const QJsonValue& v : row.value(QStringLiteral("instruments")).toArray())
            names << v.toString();
        const QString trained = aql_when(row.value(QStringLiteral("trained_at")).toString());
        if (!trained.isEmpty())
            lines << (names.isEmpty() ? QuantModulePanel::tr("Trained %1").arg(trained)
                                      : QuantModulePanel::tr("Trained %1 on %2")
                                            .arg(trained, names.size() > 8 ? names.mid(0, 8).join(QStringLiteral(", ")) +
                                                                                 QStringLiteral(", …")
                                                                           : names.join(QStringLiteral(", "))));
        const QString metrics = row.value(QStringLiteral("metrics_summary")).toString();
        if (!metrics.isEmpty())
            lines << metrics;
    } else {
        QStringList stages;
        for (const QJsonValue& v : row.value(QStringLiteral("processors")).toArray())
            stages << v.toString();
        lines << QuantModulePanel::tr("Stages: %1").arg(stages.join(QStringLiteral(" → ")));
        const QString when = aql_when(row.value(QStringLiteral("updated_at")).toString());
        if (!when.isEmpty())
            lines << QuantModulePanel::tr("Saved %1").arg(when);
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

namespace {
QSet<QString> aql_named_keys(const QHash<QString, QLineEdit*>& t, const QHash<QString, QDoubleSpinBox*>& d,
                             const QHash<QString, QComboBox*>& c, const QHash<QString, QSpinBox*>& i,
                             const QHash<QString, QDateEdit*>& dt);
} // namespace

// ── Declaring views ──────────────────────────────────────────────────────────

QTabWidget* QuantModulePanel::make_view_tabs(QWidget* parent) {
    auto* tabs = new QTabWidget(parent);
    tabs->setStyleSheet(tab_ss(ui::colors::legible(module_.color)));
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    tabs->setUsesScrollButtons(true);
    tabs->tabBar()->setExpanding(false);
    view_tabs_ = tabs;
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        const QString key = view_of_page_.value(tabs->widget(index));
        if (!key.isEmpty())
            select_view(key);
    });
    return tabs;
}

void QuantModulePanel::add_view_tab(QTabWidget* tabs, const ViewSpec& spec, const QString& title,
                                    ViewBuildFn build) {
    if (!tabs || spec.key.isEmpty() || views_.contains(spec.key))
        return;
    // The tab page holds one inner form widget. A QTabWidget sizes itself to its TALLEST page;
    // the forms of the tabs not on screen are hidden (sync_view_tab_pages), so the tab widget is
    // as tall as the visible form and no blank band opens under a short one.
    auto* page = new QWidget(tabs);
    auto* pl = new QVBoxLayout(page);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    auto* form = new QWidget(page);
    pl->addWidget(form);

    ViewState v;
    v.spec = spec;
    v.title = title;
    v.page = form;
    v.build = std::move(build);
    views_.insert(spec.key, v);
    view_order_.append(spec.key);
    view_of_page_.insert(page, spec.key);

    QString text = title;
    if (!text.contains(QLatin1String("&&")))
        text.replace(QLatin1Char('&'), QLatin1String("&&"));
    tabs->addTab(page, text); // the first tab becomes current here → select_view builds it
}

void QuantModulePanel::begin_view(const ViewSpec& spec) {
    if (spec.key.isEmpty())
        return;
    ViewState v;
    v.spec = spec;
    v.title = spec.title;
    v.built = true; // the form is built inline, right now
    views_.insert(spec.key, v);
    view_order_.append(spec.key);
    building_view_ = spec.key;
    inline_before_ = aql_named_keys(text_inputs_, double_inputs_, combo_inputs_, int_inputs_, date_inputs_).values();
}

void QuantModulePanel::end_view() {
    auto it = views_.find(building_view_);
    if (it != views_.end()) {
        const QSet<QString> before(inline_before_.cbegin(), inline_before_.cend());
        for (const QString& k : aql_named_keys(text_inputs_, double_inputs_, combo_inputs_, int_inputs_, date_inputs_))
            if (!before.contains(k))
                it->input_keys.append(k);
    }
    inline_before_.clear();
    building_view_.clear();
}

void QuantModulePanel::set_shared_action(QPushButton* button, std::function<void()> action, const QStringList& views) {
    if (!button || !action)
        return;
    for (const QString& key : views) {
        auto it = views_.find(key);
        if (it == views_.end())
            continue;
        it->action = action;
        it->buttons.append(button);
    }
    connect(button, &QPushButton::clicked, this, [this, action]() {
        if (!pane_) {
            action();
            return;
        }
        button_action_ = action;
        button_action_view_ = current_view_;
        pane_->run();
    });
}

void QuantModulePanel::set_action(QPushButton* button, std::function<void()> action, bool is_default,
                                  int timeout_s) {
    const QString key = building_view_;
    auto it = views_.find(key);
    if (!button || !action || it == views_.end()) {
        // Not inside a view (should not happen): keep the button working the old way.
        if (button && action)
            connect(button, &QPushButton::clicked, this, action);
        return;
    }
    it->buttons.append(button);
    if (is_default && !it->action)
        it->action = action;
    connect(button, &QPushButton::clicked, this, [this, key, action, timeout_s]() {
        if (!pane_) {
            action();
            return;
        }
        if (current_view_ != key)
            select_view(key);
        // The user's RUN: a new run of the view (it supersedes one in flight; a result on screen stays,
        // dimmed, until the new one arrives). start_view_run() picks this action up.
        button_action_ = action;
        button_action_view_ = key;
        if (timeout_s >= 0)
            pane_->set_timeout(timeout_s); // read when the run starts (below)…
        pane_->run();
        if (timeout_s >= 0)
            pane_->set_timeout(view_timeout(key)); // …the view's own again for its next run
    });
}

void QuantModulePanel::set_default_action(std::function<void()> action) {
    auto it = views_.find(building_view_);
    if (it != views_.end() && action)
        it->action = std::move(action);
}

QWidget* QuantModulePanel::named_input(const QString& name) const {
    if (QWidget* w = text_inputs_.value(name))
        return w;
    if (QWidget* w = double_inputs_.value(name))
        return w;
    if (QWidget* w = combo_inputs_.value(name))
        return w;
    if (QWidget* w = int_inputs_.value(name))
        return w;
    return date_inputs_.value(name);
}

int QuantModulePanel::view_timeout(const QString& view) const {
    const auto it = views_.constFind(view);
    if (it == views_.cend())
        return 0;
    return it->spec.cost == ViewCost::Free ? kAqlFreeTimeoutS : it->spec.cost == ViewCost::Fetch ? kAqlFetchTimeoutS : 0;
}

QString QuantModulePanel::pane_view_of(const QString& view) const {
    const auto it = views_.constFind(view);
    if (it == views_.cend() || it->spec.pane_view.isEmpty())
        return view;
    return it->spec.pane_view;
}

// ── Building views lazily ────────────────────────────────────────────────────

namespace {
QSet<QString> aql_named_keys(const QHash<QString, QLineEdit*>& t, const QHash<QString, QDoubleSpinBox*>& d,
                             const QHash<QString, QComboBox*>& c, const QHash<QString, QSpinBox*>& i,
                             const QHash<QString, QDateEdit*>& dt) {
    QSet<QString> out;
    for (auto it = t.cbegin(); it != t.cend(); ++it)
        out.insert(it.key());
    for (auto it = d.cbegin(); it != d.cend(); ++it)
        out.insert(it.key());
    for (auto it = c.cbegin(); it != c.cend(); ++it)
        out.insert(it.key());
    for (auto it = i.cbegin(); it != i.cend(); ++it)
        out.insert(it.key());
    for (auto it = dt.cbegin(); it != dt.cend(); ++it)
        out.insert(it.key());
    return out;
}
} // namespace

void QuantModulePanel::ensure_view_built(const QString& key) {
    auto it = views_.find(key);
    if (it == views_.end() || it->built)
        return;
    it->built = true;
    ViewBuildFn build = std::move(it->build);
    it->build = {};
    QPointer<QWidget> page = it->page;
    if (!build || !page)
        return;

    // Built while hidden, then shown once: children added to a visible page would only appear on
    // the next event-loop pass.
    const bool visible = page->isVisible();
    if (visible)
        page->hide();
    const QSet<QString> before = aql_named_keys(text_inputs_, double_inputs_, combo_inputs_, int_inputs_, date_inputs_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    const QString prev = std::exchange(building_view_, key);
    build(page, layout);
    building_view_ = prev;

    QStringList keys;
    for (const QString& k : aql_named_keys(text_inputs_, double_inputs_, combo_inputs_, int_inputs_, date_inputs_))
        if (!before.contains(k))
            keys.append(k);
    it = views_.find(key); // the build may have inserted views
    it->input_keys = keys;
    if (it->spec.qlib_dataset || module_.id == QLatin1String("factor_discovery") ||
        module_.id == QLatin1String("model_library") || module_.id == QLatin1String("live_signals") ||
        module_.id == QLatin1String("rolling_retraining"))
        apply_qlib_defaults_to_view(key);
    attach_memory(*it, {});
    if (visible)
        page->show();
}

void QuantModulePanel::build_view(ViewState& v) {
    ensure_view_built(v.spec.key);
}

void QuantModulePanel::attach_memory(ViewState& v, const QStringList& extra_keys) {
    if (v.memory || !v.page)
        return;
    const QString key = v.spec.key;
    auto* mem = new ui::InputMemory(QStringLiteral("ai_quant_lab"), module_.id + QLatin1Char('/') + key, v.page);
    auto track = [mem](const QString& name, QWidget* w) {
        if (w && !w->property("inputMemoryIgnore").toBool())
            mem->track(name, w);
    };
    QStringList names = v.input_keys + extra_keys;
    for (const QString& name : std::as_const(names)) {
        if (auto* w = text_inputs_.value(name))
            track(name, w);
        else if (auto* w2 = double_inputs_.value(name))
            track(name, w2);
        else if (auto* w3 = combo_inputs_.value(name))
            track(name, w3);
        else if (auto* w4 = int_inputs_.value(name))
            track(name, w4);
        else if (auto* w5 = date_inputs_.value(name))
            track(name, w5);
    }
    // The series source of the view (Source, ticker(s), period): see QuantModulePanel_Series.cpp.
    for (QWidget* w : v.page->findChildren<QWidget*>()) {
        const QString name = w->property("quantMemoryName").toString();
        if (!name.isEmpty())
            track(name, w);
    }
    mem->restore(); // restored text equal to a synthetic sample is flagged by make_sample_button's watcher
    v.memory = mem;
    sync_series_mode(key);
    connect(mem, &ui::InputMemory::changed, this, [this, key]() {
        if (pane_ && current_view_ == key)
            pane_->set_signature(view_signature(key));
    });
}

void QuantModulePanel::attach_inline_memory() {
    // The panel's own form memory: inputs no tab owns (the HFT exchange bar, CFA's symbol / period /
    // interval) and the views built inline (single-page modules, the CFA analyses), which share one
    // form. Those shared inputs belong to every view's signature.
    QSet<QString> claimed;
    for (const ViewState& v : std::as_const(views_))
        for (const QString& k : v.input_keys)
            claimed.insert(k);
    shared_keys_.clear();
    for (const QString& k : aql_named_keys(text_inputs_, double_inputs_, combo_inputs_, int_inputs_, date_inputs_))
        if (!claimed.contains(k))
            shared_keys_.append(k);
    QStringList names = shared_keys_;
    for (const ViewState& v : std::as_const(views_))
        if (!v.page)
            names += v.input_keys;
    if (names.isEmpty())
        return;
    auto* mem = new ui::InputMemory(QStringLiteral("ai_quant_lab"), module_.id + QStringLiteral("/form"), this);
    auto track = [mem](const QString& name, QWidget* w) {
        if (w && !w->property("inputMemoryIgnore").toBool())
            mem->track(name, w);
    };
    for (const QString& name : std::as_const(names)) {
        if (auto* w = text_inputs_.value(name))
            track(name, w);
        else if (auto* w2 = double_inputs_.value(name))
            track(name, w2);
        else if (auto* w3 = combo_inputs_.value(name))
            track(name, w3);
        else if (auto* w4 = int_inputs_.value(name))
            track(name, w4);
        else if (auto* w5 = date_inputs_.value(name))
            track(name, w5);
    }
    form_memory_ = mem;
    for (auto it = views_.begin(); it != views_.end(); ++it)
        if (!it->page)
            it->memory = mem;
    mem->restore();
    connect(mem, &ui::InputMemory::changed, this, [this]() {
        if (pane_ && !current_view_.isEmpty())
            pane_->set_signature(view_signature(current_view_));
    });
}

QString QuantModulePanel::view_signature(const QString& key) const {
    const auto it = views_.constFind(key);
    if (it == views_.cend())
        return {};
    QVariantMap sig;
    // Tabs sharing one result (HFT): the inputs of all of them, so switching between them is not
    // an "inputs changed".
    const QString pv = pane_view_of(key);
    for (auto v = views_.cbegin(); v != views_.cend(); ++v) {
        if (v.key() != key && (it->spec.pane_view.isEmpty() || pane_view_of(v.key()) != pv))
            continue;
        if (!v->memory)
            continue;
        const QVariantMap all = v->memory->values();
        const QStringList skip = series_signature_skip(v.key());
        for (auto x = all.cbegin(); x != all.cend(); ++x) {
            // An inline view's memory is the whole form: only this view's inputs count here.
            if (!v->page && !v->input_keys.contains(x.key()))
                continue;
            // Inputs of an action that is not the view's default (a create form under a list).
            if (const QWidget* in = named_input(x.key()); in && in->property("quantSignatureIgnore").toBool())
                continue;
            if (!skip.contains(x.key()))
                sig.insert(x.key(), x.value());
        }
    }
    // Inputs outside every tab (the HFT exchange bar, CFA's data row).
    if (form_memory_) {
        const QVariantMap form = form_memory_->values();
        for (const QString& k : shared_keys_)
            if (form.contains(k))
                sig.insert(k, form.value(k));
    }
    if (it->spec.qlib_dataset)
        sig.insert(QStringLiteral("dataset"), property("qlibDataset").toString());
    return ui::autoload::signature_of(sig);
}

// ── Selecting a view ─────────────────────────────────────────────────────────

void QuantModulePanel::select_view(const QString& key) {
    auto it = views_.find(key);
    if (it == views_.end())
        return;
    current_view_ = key;
    ensure_view_built(key);
    sync_view_tabs_policy();
    if (!pane_)
        return;
    it = views_.find(key);
    const ViewState& v = *it;
    pane_->setVisible(v.spec.pane);
    const bool manual = v.spec.cost == ViewCost::Heavy && !(v.spec.auto_when && v.spec.auto_when());
    // A view that calls an LLM spends model quota / Fincept credits without a credit price on the
    // pane (no set_cost_credits): its RUN / RETRY buttons carry "spend" for agents operating the UI.
    if (v.spec.llm)
        pane_->set_view_agent_risk(pane_view_of(key), QStringLiteral("spend"));
    if (manual)
        configure_preview(v);
    pane_->skeleton()->set_shape(v.spec.shape);
    pane_->set_loading_text(v.spec.loading.isEmpty() ? tr("Running %1…").arg(v.title) : v.spec.loading);
    int age = v.spec.max_age_s;
    if (age == -2)
        age = v.spec.cost == ViewCost::Fetch ? kAqlFetchMaxAgeS : ui::autoload::kNeverRefresh;
    pane_->set_max_age(age);
    pane_->set_timeout(view_timeout(key));
    pane_->set_view(pane_view_of(key), manual ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun);
    pane_->set_signature(view_signature(key));
    sync_result_host_policy();
}

void QuantModulePanel::open_view(const QString& key) {
    const auto it = views_.constFind(key);
    if (it == views_.cend())
        return;
    if (view_tabs_) {
        for (int i = 0; i < view_tabs_->count(); ++i) {
            if (view_of_page_.value(view_tabs_->widget(i)) == key) {
                view_tabs_->setCurrentIndex(i);
                return;
            }
        }
    }
    select_view(key);
}

bool QuantModulePanel::open_view_tab(const QString& key, bool select) {
    if (!view_tabs_ || key.isEmpty())
        return false;
    int index = -1;
    for (int i = 0; i < view_tabs_->count() && index < 0; ++i)
        if (view_of_page_.value(view_tabs_->widget(i)) == key)
            index = i;
    for (int i = 0; i < view_tabs_->count() && index < 0; ++i)
        if (view_of_page_.value(view_tabs_->widget(i)).compare(key, Qt::CaseInsensitive) == 0)
            index = i;
    if (index < 0)
        return false;
    if (select)
        view_tabs_->setCurrentIndex(index); // currentChanged → select_view (the tab click path)
    return true;
}

void QuantModulePanel::invalidate_view(const QString& key) {
    if (pane_ && current_view_ == key)
        pane_->invalidate();
}

void QuantModulePanel::configure_preview(const ViewState& v) {
    const QString title = v.spec.title.isEmpty() ? v.title : v.spec.title;
    ui::EmptyState::Spec spec = ui::EmptyState::preview_spec(
        title,
        v.spec.returns.isEmpty() ? tr("Runs only when you ask: it trains a model, calls an LLM or changes saved "
                                      "state.")
                                 : v.spec.returns,
        v.spec.run_text.isEmpty() ? tr("RUN") : v.spec.run_text, v.spec.cost_note, ui::EmptyState::Tone::Warning,
        v.spec.shape);
    // The inputs a run would use (the view's form, as it stands).
    QList<QPair<QString, QString>> details;
    if (series_source(v.spec.key) && series_ticker_mode(v.spec.key))
        details.append({tr("Data"), series_symbols(v.spec.key).join(QStringLiteral(", "))});
    for (const QString& name : v.input_keys) {
        QWidget* w = text_inputs_.value(name);
        if (!w)
            w = combo_inputs_.value(name);
        if (!w)
            w = int_inputs_.value(name);
        if (!w)
            w = double_inputs_.value(name);
        if (!w)
            w = date_inputs_.value(name);
        if (!w || !w->isEnabled())
            continue;
        QString value = aql_input_text(w);
        if (value.isEmpty() || value.size() > 60)
            continue;
        QString label = w->accessibleName();
        if (label.isEmpty())
            continue;
        details.append({label, value});
        if (details.size() >= 6)
            break;
    }
    spec.details = details;
    spec.details_heading = tr("WITH THESE INPUTS");
    pane_->preview()->apply(spec);
}

void QuantModulePanel::sync_view_tabs_policy() {
    if (!view_tabs_)
        return;
    const int current = view_tabs_->currentIndex();
    for (int i = 0; i < view_tabs_->count(); ++i) {
        QWidget* page = view_tabs_->widget(i);
        const QString key = view_of_page_.value(page);
        const auto it = views_.constFind(key);
        if (it == views_.cend() || !it->page)
            continue;
        const bool want = i == current;
        if (it->page->isHidden() == want)
            it->page->setVisible(want);
    }
}

void QuantModulePanel::sync_result_host_policy() {
    if (!result_host_)
        return;
    // The pane's stack is as tall as its tallest page. A result of another view, not on screen,
    // must not keep it tall under a short preview / error page (a blank band): Ignored while hidden.
    const QSizePolicy::Policy want = result_host_->isHidden() ? QSizePolicy::Ignored : QSizePolicy::Preferred;
    if (result_host_->sizePolicy().verticalPolicy() != want)
        result_host_->setSizePolicy(QSizePolicy::Preferred, want);
}

// ── The pane ─────────────────────────────────────────────────────────────────

void QuantModulePanel::add_result_pane(QVBoxLayout* layout) {
    if (pane_ || !layout)
        return;
    // The inputs of inline views come back first (a restored CFA analysis selects its view).
    attach_inline_memory();
    QWidget* parent = layout->parentWidget() ? layout->parentWidget() : this;
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, parent);
    pane_->setObjectName(QStringLiteral("quantResultPane"));
    result_host_ = new QWidget(pane_->content_parent());
    result_host_->setObjectName(QStringLiteral("quantResultHost"));
    results_layout_ = new QVBoxLayout(result_host_);
    results_layout_->setContentsMargins(0, 8, 0, 8);
    results_layout_->setSpacing(8);
    results_layout_->setAlignment(Qt::AlignTop);
    pane_->set_content(result_host_);
    layout->addWidget(pane_, 1);

    pane_->set_cache_key(QStringLiteral("aiquant/") + module_.id);
    pane_->set_persist_limit(kAqlPersistBytes);
    pane_->set_auto_delay(kAqlAutoDelayMs);
    pane_->set_status_label(status_label_);
    // Python for every view; an LLM profile for the agent views; a dataset for the qlib views.
    pane_->set_requirement_check([this]() -> ui::RequirementStatus {
        const auto it = views_.constFind(current_view_);
        QList<ui::Requirement> needs{ui::Requirement::python()};
        if (it != views_.cend() && it->spec.llm)
            needs.append(ui::Requirement::llm());
        ui::RequirementStatus st = ui::check_requirements(needs);
        if (!st.ok)
            return st;
        if (it != views_.cend() && it->spec.qlib_dataset &&
            property("qlibDatasetState").toString() == QLatin1String("none")) {
            st.ok = false;
            st.title = tr("No market dataset yet");
            st.message = tr("This view reads a local qlib dataset. Build one from Yahoo Finance with BUILD FROM "
                            "YAHOO in the dataset bar above (a few minutes the first time), or download the public "
                            "1999-2020 dataset in Model Library > Browse.");
            st.action_text.clear();
            st.link = {};
        }
        return st;
    });
    pane_->set_start([this](quint64 id) { start_view_run(id); });
    pane_->set_cancel([this](quint64 id) { cancel_view_run(id); });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_view_payload(payload); });
    connect(pane_, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State) { sync_result_host_policy(); });
    connect(pane_, &ui::AutoLoadPane::result_shown, this, [this](const QDateTime&, bool) { sync_result_host_policy(); });
    connect(pane_, &ui::AutoLoadPane::notice_action, this, [this]() { pane_->clear_notice(); });
    connect(pane_, &ui::AutoLoadPane::running_changed, this, [this](bool running) {
        // A Heavy run the user started (training, an agent) keeps its RUN button disabled: a second
        // click would supersede — kill — it. CANCEL is under the skeleton.
        if (running) {
            const QString pv = pane_->running_view();
            for (const ViewState& v : std::as_const(views_))
                if (v.spec.cost == ViewCost::Heavy && pane_view_of(v.spec.key) == pv)
                    set_view_buttons_busy(pv, true);
        } else {
            for (const ViewState& v : std::as_const(views_))
                set_view_buttons_busy(pane_view_of(v.spec.key), false);
        }
    });

    // Result views format their figures when they are built: a number-format / currency change
    // re-renders the result on screen from the pane's copy (no new run).
    ui::formatting::on_format_changed(this, [this]() {
        if (pane_ && results_layout_ && !rendering_ && pane_->has_result())
            render_view_payload(pane_->payload());
    });

    auto& svc = AIQuantLabService::instance();
    connect(&svc, &AIQuantLabService::request_finished, this, &QuantModulePanel::on_request_finished);
    connect(&svc, &AIQuantLabService::request_failed, this, &QuantModulePanel::on_request_failed);

    // The view on screen now.
    if (view_tabs_)
        select_view(view_of_page_.value(view_tabs_->currentWidget()));
    else if (!view_order_.isEmpty())
        select_view(current_view_.isEmpty() ? view_order_.first() : current_view_);
}

void QuantModulePanel::set_view_buttons_busy(const QString& pane_view, bool busy) {
    for (auto it = views_.begin(); it != views_.end(); ++it) {
        if (pane_view_of(it.key()) != pane_view)
            continue;
        for (const auto& b : std::as_const(it->buttons))
            if (b && b->property("quantBusyLock").toBool() != busy) {
                b->setProperty("quantBusyLock", busy);
                b->setEnabled(!busy);
            }
    }
}

bool QuantModulePanel::page_inputs_are_sample(const QWidget* page) const {
    if (!page)
        return false;
    for (auto* e : page->findChildren<QLineEdit*>()) {
        const QString sample = e->property("quantSampleText").toString();
        if (!sample.isEmpty() && e->isVisibleTo(page) && e->text() == sample)
            return true;
    }
    return false;
}

// ── Running ──────────────────────────────────────────────────────────────────

void QuantModulePanel::start_view_run(quint64 pane_id) {
    const QString view = current_view_;
    std::function<void()> action;
    if (button_action_ && button_action_view_ == view)
        action = std::move(button_action_);
    button_action_ = {};
    button_action_view_.clear();
    ensure_view_built(view);
    const auto it = views_.constFind(view);
    if (!action && it != views_.cend())
        action = it->action;
    if (!action || it == views_.cend()) {
        pane_->fail(pane_id, tr("This view has nothing to run."));
        return;
    }
    // Starts the action once its inputs are ready (the dataset is known, the prices are downloaded).
    auto proceed = [this, pane_id, view, action]() {
        if (!pane_ || !pane_->is_current(pane_id))
            return;
        if (series_source(view) && series_ticker_mode(view)) {
            fetch_series(pane_id, view, [this, pane_id, view, action](const QJsonObject& meta) {
                execute_view_action(pane_id, view, action, meta);
            });
            return;
        }
        QJsonObject meta;
        if (series_source(view)) // a series view on its Custom series source
            meta.insert(QStringLiteral("custom"), true);
        execute_view_action(pane_id, view, action, meta);
    };
    if (it->spec.qlib_dataset && property("qlibDatasetState").toString().isEmpty()) {
        // The dataset bar's probe has not answered yet: the run waits for it (qlib_dataset_known).
        pane_->set_loading_text(tr("Reading the local market datasets…"));
        qlib_waiting_.insert(pane_id, proceed);
        return;
    }
    proceed();
}

void QuantModulePanel::execute_view_action(quint64 pane_id, const QString& view, const std::function<void()>& action,
                                           const QJsonObject& meta) {
    if (!pane_ || !pane_->is_current(pane_id))
        return;
    const auto vit = views_.constFind(view);
    PendingRun run;
    run.pane_id = pane_id;
    run.view = view;
    run.cancel = std::make_shared<std::atomic<bool>>(false);
    run.meta = meta;
    if (!run.meta.contains(QStringLiteral("source")) && vit != views_.cend() && page_inputs_are_sample(vit->page))
        run.meta.insert(QStringLiteral("sample"), true);
    else if (run.meta.value(QStringLiteral("custom")).toBool())
        run.meta.insert(QStringLiteral("source"), tr("Data: your custom series (the values pasted above)"));
    const quint64 request_id = AIQuantLabService::next_request_id();
    pending_.insert(request_id, run);

    RunContext ctx;
    ctx.pane_id = pane_id;
    ctx.view = view;
    RunContext* const outer = std::exchange(run_ctx_, &ctx);
    int dispatched = 0;
    {
        AIQuantLabService::RequestScope scope(request_id, run.cancel);
        action(); // the RUN button's own handler: checks, caption, service call
        dispatched = scope.dispatched();
    }
    run_ctx_ = outer;
    if (ctx.failed || dispatched == 0) {
        pending_.remove(request_id);
        if (!ctx.failed && pane_->is_current(pane_id))
            pane_->fail(pane_id, tr("Nothing was started — check the inputs above."), tr("Check the inputs"));
    }
}

void QuantModulePanel::cancel_view_run(quint64 pane_id) {
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->pane_id == pane_id) {
            if (it->cancel)
                it->cancel->store(true); // the runner kills the script (or never starts it)
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
    cancel_series_fetch(pane_id);
    qlib_waiting_.remove(pane_id);
}

void QuantModulePanel::qlib_dataset_known() {
    if (pane_)
        pane_->recheck_requirements();
    const auto waiting = std::exchange(qlib_waiting_, {});
    for (auto it = waiting.cbegin(); it != waiting.cend(); ++it) {
        if (!pane_ || !pane_->is_current(it.key()))
            continue;
        if (property("qlibDatasetState").toString() == QLatin1String("none")) {
            pane_->fail(it.key(),
                        tr("There is no market dataset yet. Use BUILD FROM YAHOO in the dataset bar above."),
                        tr("No market dataset yet"));
            continue;
        }
        it.value()();
    }
}

void QuantModulePanel::on_request_finished(quint64 request_id, const QString& module_id, const QString& command,
                                           const QJsonObject& data) {
    auto it = pending_.find(request_id);
    if (it == pending_.end())
        return; // another panel's, or a run its view gave up on
    const PendingRun run = it.value();
    pending_.erase(it);
    if (!pane_)
        return;
    const auto vit = views_.constFind(run.view);
    const QJsonValue ok = data.value(QStringLiteral("success"));
    if (ok.isBool() && !ok.toBool()) {
        const QString why = aql_plain_error(aql_reply_error(data));
        LOG_WARN("AIQuantLab", QStringLiteral("[%1/%2] reported a failure: %3").arg(module_id, command, why.left(200)));
        if (vit != views_.cend() && vit->spec.after)
            vit->spec.after(false, command, data);
        pane_->fail(run.pane_id, why, tr("The analysis did not complete"));
        return;
    }
    QJsonObject payload{{QStringLiteral("command"), command},
                        {QStringLiteral("data"), data},
                        {QStringLiteral("meta"), run.meta}};
    pane_->deliver(run.pane_id, payload);
    if (vit != views_.cend() && vit->spec.after)
        vit->spec.after(true, command, data);
}

void QuantModulePanel::on_request_failed(quint64 request_id, const QString& module_id, const QString& message) {
    auto it = pending_.find(request_id);
    if (it == pending_.end())
        return;
    const PendingRun run = it.value();
    pending_.erase(it);
    if (!pane_ || (run.cancel && run.cancel->load()))
        return;
    Q_UNUSED(module_id)
    const auto vit = views_.constFind(run.view);
    if (vit != views_.cend() && vit->spec.after)
        vit->spec.after(false, QString(), QJsonObject{{QStringLiteral("error"), message}});
    pane_->fail(run.pane_id, aql_plain_error(message));
}

// ── Rendering ────────────────────────────────────────────────────────────────

void QuantModulePanel::render_view_payload(const QJsonValue& payload) {
    if (!results_layout_)
        return;
    const QJsonObject p = payload.toObject();
    const QString command = p.value(QStringLiteral("command")).toString();
    const QJsonObject data = p.value(QStringLiteral("data")).toObject();
    const QJsonObject meta = p.value(QStringLiteral("meta")).toObject();
    QScopedValueRollback<bool> guard(rendering_, true);
    // One pass, no intermediate repaints / layouts of the half-built result.
    result_host_->setUpdatesEnabled(false);
    render_result(command, data);
    decorate_result(command, data, meta);
    result_host_->setUpdatesEnabled(true);
}

void QuantModulePanel::decorate_result(const QString& command, const QJsonObject& payload, const QJsonObject& meta) {
    if (!results_layout_)
        return;
    // Result tables are sized with setMaximumHeight() only. Once the results are taller than the
    // viewport the scroll area shrinks every widget toward its minimum, and a table's minimum is
    // its header row: the rows vanished (CFA forecast steps, PCA loadings). Pin each table at the
    // height it was sized for; the scroll area scrolls instead.
    if (QWidget* host = results_layout_->parentWidget()) {
        const auto tables = host->findChildren<QTableWidget*>();
        for (QTableWidget* t : tables)
            if (t->maximumHeight() < QWIDGETSIZE_MAX && t->minimumHeight() == 0)
                t->setMinimumHeight(t->maximumHeight());
    }
    // A result that carries a "warning" (trained on synthetic data, a benchmark that failed to
    // resolve, an ensemble that is only a definition) says so where the user reads it.
    const QString warning = payload.value(QStringLiteral("warning")).toString().trimmed();
    if (!warning.isEmpty() && command != QLatin1String("get_data")) {
        auto* banner = gs_section_header(QString::fromUtf8("⚠ ") + warning, ui::colors::WARNING());
        banner->setWordWrap(true);
        results_layout_->insertWidget(0, banner);
    }
    // Computed on LOAD SYNTHETIC SAMPLE data: say so above the result, every time.
    if (meta.value(QStringLiteral("sample")).toBool()) {
        auto* banner = gs_section_header(tr("SAMPLE INPUT — computed on generated (synthetic) data from LOAD "
                                            "SYNTHETIC SAMPLE, not on market data. Pick a ticker as the source for a "
                                            "real analysis."),
                                         ui::colors::WARNING());
        banner->setWordWrap(true);
        results_layout_->insertWidget(0, banner);
    }
    // Which data the result was computed on (D1): symbol(s), period, observations, last date.
    const QString source = meta.value(QStringLiteral("source")).toString();
    if (!source.isEmpty()) {
        auto* line = new QLabel(source, result_host_);
        line->setObjectName(QStringLiteral("quantSourceLine"));
        line->setWordWrap(true);
        line->setTextInteractionFlags(Qt::TextSelectableByMouse);
        line->setStyleSheet(QStringLiteral("color:%1; font-size:%2px; background:transparent; padding:2px 0;")
                                .arg(ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3)));
        results_layout_->insertWidget(0, line);
    }
}

// ── Saved-id pickers ─────────────────────────────────────────────────────────
// The Model ID of Backtest / Feature Importance / Model Signals and the Pipeline ID of Process Data
// are editable combos of what the scripts saved: qlib_service.py list_trained_models (newest first,
// tooltip: type, dataset, training date, validation IC, last backtest) and qlib_data_processors.py
// list_pipelines. A list is one untagged run_module() per kind and panel, answered on result_ready
// (on_result → apply_saved_ids); it is read when its first combo is built and again after a train /
// create / backtest. Typing an id still works; the view's InputMemory remembers the value.

QString QuantModulePanel::saved_id(const QComboBox* combo) {
    return combo ? combo->currentText().trimmed() : QString();
}

QComboBox* QuantModulePanel::make_saved_id_combo(SavedIds kind, const QString& key, const QString& fallback,
                                                 QWidget* parent, bool prefer_feature_importance) {
    auto* combo = new QComboBox(parent);
    combo->setProperty("quantSavedIdTrees", prefer_feature_importance);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert); // a typed id is used as typed, never added to the list
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(24);
    combo->setStyleSheet(combo_ss());
    if (QLineEdit* edit = combo->lineEdit())
        edit->setPlaceholderText(kind == SavedIds::TrainedModels ? tr("Pick a trained model, or type its ID")
                                                                 : tr("Pick a saved pipeline, or type its ID"));
    // The picked item's description (type, dataset, metrics / stages) on the combo itself.
    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), combo, [combo](int index) {
        combo->setToolTip(index >= 0 ? combo->itemData(index, Qt::ToolTipRole).toString() : QString());
    });
    combo_inputs_.insert(key, combo);

    SavedIdList& list = saved_ids_[static_cast<int>(kind)];
    list.combos.append(combo);
    if (list.known) {
        // Listed already (another tab of this panel): the remembered fallback only when it still exists.
        fill_saved_id_combo(combo, kind, saved_id_listed(kind, fallback) ? fallback : QString());
    } else {
        if (!fallback.isEmpty())
            combo->setEditText(fallback);
        refresh_saved_ids(kind);
    }

    // A model trained or backtested in any panel (Model Library trains; Live Signals reads the same
    // models): read the list again, the new model becoming the value of the combos.
    if (kind == SavedIds::TrainedModels && !saved_ids_watch_) {
        saved_ids_watch_ = true;
        connect(&AIQuantLabService::instance(), &AIQuantLabService::request_finished, this,
                [this](quint64, const QString& module_id, const QString& command, const QJsonObject& data) {
                    if (module_id != QLatin1String("model_library") ||
                        !data.value(QStringLiteral("success")).toBool())
                        return;
                    if (command == QLatin1String("train_model"))
                        refresh_saved_ids(SavedIds::TrainedModels, data.value(QStringLiteral("model_id")).toString());
                    else if (command == QLatin1String("run_backtest")) // its figures join the model's tooltip
                        refresh_saved_ids(SavedIds::TrainedModels);
                });
    }
    return combo;
}

void QuantModulePanel::refresh_saved_ids(SavedIds kind, const QString& select) {
    SavedIdList& list = saved_ids_[static_cast<int>(kind)];
    if (!select.isEmpty())
        list.select = select;
    if (list.pending)
        return; // the reply on its way applies `select`
    list.pending = true;
    const QString command = kind == SavedIds::TrainedModels ? QStringLiteral("list_trained_models")
                                                            : QStringLiteral("list_pipelines");
    // Next event-loop pass: called from inside a view's run (a RequestScope), the request would be
    // tagged as that run's and delivered as its result.
    QTimer::singleShot(0, this, [this, command]() {
        AIQuantLabService::instance().run_module(module_.id, command, QJsonObject{});
    });
}

bool QuantModulePanel::apply_saved_ids(const QString& command, const QJsonObject& data) {
    SavedIds kind;
    if (command == QLatin1String("list_trained_models"))
        kind = SavedIds::TrainedModels;
    else if (command == QLatin1String("list_pipelines"))
        kind = SavedIds::Pipelines;
    else
        return false;
    SavedIdList& list = saved_ids_[static_cast<int>(kind)];
    list.pending = false;
    const QString select = std::exchange(list.select, QString());
    if (!data.value(QStringLiteral("success")).toBool()) {
        // The combos keep what they show; a typed id still runs.
        LOG_WARN("AIQuantLab", QStringLiteral("[%1/%2] could not list the saved ids: %3")
                                   .arg(module_.id, command, aql_reply_error(data).left(200)));
        return true;
    }
    list.rows = data.value(kind == SavedIds::TrainedModels ? QStringLiteral("models") : QStringLiteral("pipelines"))
                    .toArray();
    list.known = true;
    list.combos.removeAll(QPointer<QComboBox>());
    const QList<QPointer<QComboBox>> combos = list.combos;
    for (const QPointer<QComboBox>& combo : combos)
        if (combo)
            fill_saved_id_combo(combo, kind, select);
    return true;
}

void QuantModulePanel::fill_saved_id_combo(QComboBox* combo, SavedIds kind, const QString& select) {
    const auto it = saved_ids_.constFind(static_cast<int>(kind));
    if (!combo || it == saved_ids_.cend())
        return;
    const bool models = kind == SavedIds::TrainedModels;
    const QString before = saved_id(combo);
    int first_tree = -1;
    {
        // The list is replaced without a "changed" for the value on screen (it stays as it is).
        const QSignalBlocker block(combo);
        combo->clear();
        for (const QJsonValue& v : it->rows) {
            const QJsonObject row = v.toObject();
            const QString id = row.value(QStringLiteral("id")).toString();
            if (id.isEmpty())
                continue;
            combo->addItem(id, id);
            combo->setItemData(combo->count() - 1, aql_saved_id_tip(row, models), Qt::ToolTipRole);
            if (first_tree < 0 && row.value(QStringLiteral("supports_feature_importance")).toBool())
                first_tree = combo->count() - 1;
        }
        const int keep = before.isEmpty() ? -1 : combo->findData(before);
        combo->setCurrentIndex(keep);
        if (keep < 0)
            combo->setEditText(before);
    }
    // The value now: the id just trained / created; with nothing entered, the newest (Feature
    // Importance: the newest model that has feature importances). Set with signals on, so the view's
    // InputMemory keeps it. An empty list leaves the combo as it is: the view shows its preview.
    QString want = select;
    if (want.isEmpty() && before.isEmpty() && combo->count() > 0) {
        const bool trees = combo->property("quantSavedIdTrees").toBool();
        want = combo->itemData(trees && first_tree >= 0 ? first_tree : 0).toString();
    }
    if (!want.isEmpty() && want != before) {
        const int idx = combo->findData(want);
        if (idx >= 0)
            combo->setCurrentIndex(idx);
        else
            combo->setEditText(want);
    }
    const int cur = combo->currentIndex();
    combo->setToolTip(cur >= 0 ? combo->itemData(cur, Qt::ToolTipRole).toString() : QString());
}

bool QuantModulePanel::saved_id_listed(SavedIds kind, const QString& id) const {
    const auto it = saved_ids_.constFind(static_cast<int>(kind));
    if (it == saved_ids_.cend() || !it->known)
        return true;
    for (const QJsonValue& v : it->rows)
        if (v.toObject().value(QStringLiteral("id")).toString() == id)
            return true;
    return false;
}

} // namespace fincept::screens
