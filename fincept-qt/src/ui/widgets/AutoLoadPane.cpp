// src/ui/widgets/AutoLoadPane.cpp — see the header.

#include "ui/widgets/AutoLoadPane.h"

#include "core/logging/Logger.h"
#include "services/billing/PlanAccess.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadGate.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace fincept::ui {

namespace {

constexpr int kAlpMaxSlots = 48; // views remembered per pane (LRU)
const QString kAlpCachePrefix = QStringLiteral("autoload/");

QString alp_when(const QDateTime& as_of) {
    const QDateTime local = as_of.toLocalTime();
    return local.date() == QDate::currentDate() ? QLocale().toString(local.time(), QLocale::ShortFormat)
                                                : QLocale().toString(local, QLocale::ShortFormat);
}

QString alp_tone_key(EmptyState::Tone tone) {
    switch (tone) {
        case EmptyState::Tone::Neutral:
            return QStringLiteral("neutral");
        case EmptyState::Tone::Info:
            return QStringLiteral("info");
        case EmptyState::Tone::Accent:
            return QStringLiteral("accent");
        case EmptyState::Tone::Warning:
            return QStringLiteral("warning");
        case EmptyState::Tone::Danger:
            return QStringLiteral("danger");
        case EmptyState::Tone::Success:
            return QStringLiteral("success");
    }
    return QStringLiteral("warning");
}

// Notice strip + loading footer. One selector stylesheet on the pane, set before
// any child exists; built from tokens, so the theme manager re-styles it.
// %1 raised, %2 warning, %3 info, %4 danger, %5 success, %6 border, %7 amber,
// %8 primary text, %9 secondary text; font sizes %10, %11 through px() (the
// number only: the "px" unit is in the format).
QString alp_style() {
    return QStringLiteral(
               "#feAlpNotice { background: %1; border: none; border-left: 3px solid %2; }"
               "#feAlpNotice[tone=\"info\"] { border-left-color: %3; }"
               "#feAlpNotice[tone=\"danger\"] { border-left-color: %4; }"
               "#feAlpNotice[tone=\"success\"] { border-left-color: %5; }"
               "#feAlpNotice[tone=\"neutral\"] { border-left-color: %6; }"
               "#feAlpNotice[tone=\"accent\"] { border-left-color: %7; }"
               "#feAlpNoticeText { color: %8; font-size: %10px; background: transparent; }"
               "#feAlpFooterText { color: %9; font-size: %10px; background: transparent; }"
               "#feAlpLink { background: transparent; color: %7; border: 1px solid %6; padding: 2px 10px; "
               "  font-size: %11px; font-weight: 700; }"
               "#feAlpLink:hover { border-color: %7; }")
        .arg(colors::BG_RAISED(), colors::WARNING(), colors::INFO(), colors::DANGER(), colors::SUCCESS(),
             colors::BORDER_MED(), colors::AMBER(), colors::TEXT_PRIMARY(), colors::TEXT_SECONDARY())
        .arg(fonts::px(-2))
        .arg(fonts::px(-3));
}

QString alp_button_text(const QString& text) {
    QString t = text;
    t.replace(QLatin1Char('&'), QStringLiteral("&&"));
    return t;
}

/// "30 s", "5 min", "2 h" — the feed stale threshold in words.
QString alp_duration(int seconds) {
    if (seconds < 120)
        return AutoLoadPane::tr("%n s", nullptr, seconds);
    if (seconds < 2 * 3600)
        return AutoLoadPane::tr("%n min", nullptr, seconds / 60);
    return AutoLoadPane::tr("%n h", nullptr, seconds / 3600);
}

bool alp_feed_may_go_stale(autoload::FeedReport report) {
    return report == autoload::FeedReport::Data || report == autoload::FeedReport::Waiting ||
           report == autoload::FeedReport::None;
}

/// Puts the overlay's CANCEL under LoadingOverlay's painted bars and caption (their
/// box ends ~50 px below the centre), at its size hint (font metrics).
void alp_place_overlay_cancel(QPushButton* btn) {
    const QWidget* overlay = btn ? btn->parentWidget() : nullptr;
    if (!overlay)
        return;
    const QSize sh = btn->sizeHint();
    const int y = std::max(0, std::min(overlay->height() / 2 + 58, overlay->height() - sh.height() - 8));
    btn->setGeometry((overlay->width() - sh.width()) / 2, y, sh.width(), sh.height());
}

/// Keeps the overlay's CANCEL placed when the overlay is resized or shown.
class AlpOverlayCancelPlacer final : public QObject {
  public:
    AlpOverlayCancelPlacer(QPushButton* btn, QObject* parent) : QObject(parent), btn_(btn) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((event->type() == QEvent::Resize || event->type() == QEvent::Show) && btn_ && btn_->isVisibleTo(btn_->parentWidget()))
            alp_place_overlay_cancel(btn_);
        return QObject::eventFilter(watched, event);
    }

  private:
    QPointer<QPushButton> btn_;
};

// ── Persisted copies, off the GUI thread (since round 6 (8)) ──────────────────
// cache.db (CacheDatabase) gives every pool thread its own cloned connection
// (WAL, busy timeout), so the SQL runs on the worker together with the JSON work.

/// How long the quit flush waits for writes already running before it writes the
/// waiting ones itself (skipping keys still running; one log line if any).
constexpr int kAlpFlushWaitMs = 1000;
/// Writes running at once, and keys waiting before the oldest waiting write is dropped.
constexpr int kAlpWritesInFlight = 2;
constexpr int kAlpWritesQueued = 64;

/// The process-wide write-behind: GUI thread, parented to the application (it
/// outlives every pane), flushed on aboutToQuit — which runs before cache.db is
/// closed (main.cpp closes it in a post-routine after every aboutToQuit handler).
autoload::WriteBehind& alp_writer() {
    static QPointer<autoload::WriteBehind> writer;
    if (!writer) {
        (void)CacheManager::instance(); // created on the GUI thread, never first on a worker
        QCoreApplication* app = QCoreApplication::instance();
        writer = new autoload::WriteBehind(nullptr, kAlpWritesInFlight, kAlpWritesQueued, app);
        if (app) {
            autoload::WriteBehind* w = writer.data();
            QObject::connect(app, &QCoreApplication::aboutToQuit, w, [w]() {
                const int waiting = w->queued_count();
                const int skipped = w->flush(kAlpFlushWaitMs);
                if (skipped > 0)
                    LOG_WARN("AutoLoad", QStringLiteral("Cached results not saved at quit skipped=%1 waiting=%2 "
                                                        "wait_ms=%3")
                                             .arg(skipped)
                                             .arg(waiting)
                                             .arg(kAlpFlushWaitMs));
            });
        }
    }
    return *writer;
}

/// Pool thread: serialise and store one result; the bytes written, or -1 when it
/// is not kept (empty or above the pane's persist limit).
std::function<qint64()> alp_put_work(const QString& pkey, const QJsonValue& payload, const QDateTime& at,
                                     const QString& signature, int ttl_seconds, int limit_bytes) {
    return [pkey, payload, at, signature, ttl_seconds, limit_bytes]() -> qint64 {
        const QByteArray text = autoload::serialize_remembered(payload, at, signature);
        if (text.isEmpty() || text.size() > limit_bytes)
            return -1;
        CacheManager::instance().put(pkey, QString::fromUtf8(text), ttl_seconds, QStringLiteral("autoload"));
        return text.size();
    };
}

/// Queue the removal of a persisted copy — after any write of that key still running.
void alp_remove_persisted(const QString& pkey) {
    autoload::WriteBehind::Op op;
    op.key = pkey;
    op.remove = true;
    op.work = [pkey]() -> qint64 {
        CacheManager::instance().remove(pkey);
        return 0;
    };
    alp_writer().submit(std::move(op));
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

AutoLoadPane::AutoLoadPane(QWidget* parent) : AutoLoadPane(Policy::AutoRun, parent) {}

AutoLoadPane::AutoLoadPane(Policy policy, QWidget* parent) : QWidget(parent), policy_(policy) {
    setStyleSheet(alp_style()); // before any child exists
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    notice_ = new QWidget(this);
    notice_->setObjectName(QStringLiteral("feAlpNotice"));
    notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* nl = new QHBoxLayout(notice_);
    nl->setContentsMargins(10, 4, 8, 4);
    nl->setSpacing(8);
    notice_label_ = new QLabel(notice_);
    notice_label_->setObjectName(QStringLiteral("feAlpNoticeText"));
    notice_label_->setWordWrap(true);
    notice_label_->setTextFormat(Qt::PlainText);
    notice_btn_ = new QPushButton(notice_);
    notice_btn_->setObjectName(QStringLiteral("feAlpLink"));
    notice_btn_->setCursor(Qt::PointingHandCursor);
    nl->addWidget(notice_label_, 1);
    nl->addWidget(notice_btn_);
    notice_->hide();
    connect(notice_btn_, &QPushButton::clicked, this, [this]() {
        switch (notice_action_kind_) {
            case NoticeAction::Host:
                emit notice_action();
                break;
            case NoticeAction::Run:
                run();
                break;
            case NoticeAction::Setup:
                on_setup_action(true);
                break;
            case NoticeAction::None:
                break;
        }
    });
    vl->addWidget(notice_);

    // The host's notice (set_notice): its own strip under the pane's, so a
    // partial-result caveat never hides "Refresh failed" / "Inputs changed".
    host_notice_ = new QWidget(this);
    host_notice_->setObjectName(QStringLiteral("feAlpNotice"));
    host_notice_->setAttribute(Qt::WA_StyledBackground, true);
    auto* hl = new QHBoxLayout(host_notice_);
    hl->setContentsMargins(10, 4, 8, 4);
    hl->setSpacing(8);
    host_notice_label_ = new QLabel(host_notice_);
    host_notice_label_->setObjectName(QStringLiteral("feAlpNoticeText"));
    host_notice_label_->setWordWrap(true);
    host_notice_label_->setTextFormat(Qt::PlainText);
    host_notice_btn_ = new QPushButton(host_notice_);
    host_notice_btn_->setObjectName(QStringLiteral("feAlpLink"));
    host_notice_btn_->setCursor(Qt::PointingHandCursor);
    hl->addWidget(host_notice_label_, 1);
    hl->addWidget(host_notice_btn_);
    host_notice_->hide();
    connect(host_notice_btn_, &QPushButton::clicked, this, &AutoLoadPane::notice_action);
    vl->addWidget(host_notice_);

    stack_ = new QStackedWidget(this);
    vl->addWidget(stack_, 1);

    // Loading page: the skeleton and, under it, "Queued…" / CANCEL.
    loading_page_ = new QWidget(stack_);
    auto* ll = new QVBoxLayout(loading_page_);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    skeleton_ = new LoadingSkeleton(LoadingSkeleton::Shape::Result, loading_page_);
    ll->addWidget(skeleton_, 1);
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(12, 0, 12, 8);
    footer->setSpacing(8);
    footer_label_ = new QLabel(loading_page_);
    footer_label_->setObjectName(QStringLiteral("feAlpFooterText"));
    footer_label_->hide();
    cancel_btn_ = new QPushButton(tr("CANCEL"), loading_page_);
    cancel_btn_->setObjectName(QStringLiteral("feAlpLink"));
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    cancel_btn_->hide();
    connect(cancel_btn_, &QPushButton::clicked, this, &AutoLoadPane::cancel);
    footer->addWidget(footer_label_);
    footer->addStretch(1);
    footer->addWidget(cancel_btn_);
    ll->addLayout(footer);
    stack_->addWidget(loading_page_);

    preview_ = new EmptyState(stack_);
    preview_->set_glyph(QStringLiteral("▶"), EmptyState::Tone::Accent); // ▶
    preview_->set_title(tr("Not run yet"));
    preview_->set_primary_action(tr("RUN"));
    connect(preview_, &EmptyState::primary_clicked, this, &AutoLoadPane::run);
    stack_->addWidget(preview_);

    error_ = new EmptyState(stack_);
    error_->apply(EmptyState::error_spec(QString()));
    connect(error_, &EmptyState::primary_clicked, this, [this]() { on_error_action(true); });
    connect(error_, &EmptyState::secondary_clicked, this, [this]() { on_error_action(false); });
    connect(error_, &EmptyState::detail_clicked, this,
            [this](int index, const QString& key) { on_error_detail(index, key); });
    stack_->addWidget(error_);

    setup_ = new EmptyState(stack_);
    connect(setup_, &EmptyState::primary_clicked, this, [this]() { on_setup_action(true); });
    connect(setup_, &EmptyState::secondary_clicked, this, [this]() { on_setup_action(false); });
    stack_->addWidget(setup_);

    overlay_ = new LoadingOverlay(stack_);
    // CANCEL on the dimmed result while the user's re-run replaces it (the skeleton
    // page has its own): revealed like that one, same cancel path.
    overlay_cancel_btn_ = new QPushButton(tr("CANCEL"), overlay_);
    overlay_cancel_btn_->setObjectName(QStringLiteral("feAlpLink"));
    overlay_cancel_btn_->setCursor(Qt::PointingHandCursor);
    overlay_cancel_btn_->hide();
    connect(overlay_cancel_btn_, &QPushButton::clicked, this, &AutoLoadPane::cancel);
    overlay_->installEventFilter(new AlpOverlayCancelPlacer(overlay_cancel_btn_, overlay_));

    auto_timer_ = new QTimer(this);
    auto_timer_->setSingleShot(true);
    connect(auto_timer_, &QTimer::timeout, this, [this]() {
        if (!isVisible() || feed_mode_)
            return;
        if (const Slot* rs = find_slot(key_); rs && rs->reading)
            return; // the default run is decided once the remembered copy is read

        const RequirementStatus req = evaluate_requirements();
        if (autoload::should_auto_run(facts_for(slot(key_), req.ok)))
            request_auto_start();
    });
    reveal_timer_ = new QTimer(this);
    reveal_timer_->setSingleShot(true);
    reveal_timer_->setInterval(kCancelRevealMs);
    connect(reveal_timer_, &QTimer::timeout, this, &AutoLoadPane::update_footer);
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]() {
        if (current_id_ == 0)
            return;
        fail_current(tr("No answer after %1 seconds. The source may be slow or unreachable — try again.")
                         .arg(timeout_s_),
                     tr("Timed out"), true);
    });
    connect(&AutoLoadGate::instance(), &AutoLoadGate::queue_changed, this, [this]() {
        if (queued_)
            update_footer();
    });
    // Feed mode: fires when the data on screen is due to turn stale (runs only while visible).
    feed_stale_timer_ = new QTimer(this);
    feed_stale_timer_->setSingleShot(true);
    connect(feed_stale_timer_, &QTimer::timeout, this, [this]() {
        if (feed_mode_ && isVisible())
            feed_after_change(); // marker now, or re-armed for what is left after a newer update
    });

    stack_->setCurrentWidget(policy_ == Policy::AutoRun ? static_cast<QWidget*>(loading_page_) : preview_);
    slot(key_);
}

AutoLoadPane::~AutoLoadPane() {
    if (gate_ticket_ != 0)
        AutoLoadGate::instance().leave(std::exchange(gate_ticket_, 0));
    // A run still going when the pane dies: let the host stop it.
    if (current_id_ != 0 && cancel_) {
        const quint64 id = std::exchange(current_id_, 0);
        cancel_(id);
    }
}

// ── Configuration ────────────────────────────────────────────────────────────

QWidget* AutoLoadPane::content_parent() const {
    return stack_;
}

void AutoLoadPane::set_content(QWidget* content) {
    if (content_ == content || !content)
        return;
    if (content_)
        stack_->removeWidget(content_);
    content_ = content;
    stack_->insertWidget(0, content_); // already a child of stack_ when built under content_parent()
    rendered_key_.clear();
    if (isVisible())
        present();
}

void AutoLoadPane::set_policy(Policy policy) {
    if (policy_ == policy)
        return;
    policy_ = policy;
    if (isVisible())
        present();
}

void AutoLoadPane::set_view(const QString& key) {
    if (key == key_)
        return;
    auto_timer_->stop();
    // Leaving a view cancels its default run (waiting or running); a run the user started keeps going.
    if (queued_ && queued_key_ != key)
        leave_gate();
    if (current_id_ != 0 && current_auto_ && current_key_ != key)
        drop_running();
    if (feed_mode_) {
        // The host's subscription is for the view being left; present() asks again for the new one.
        feed_stale_timer_->stop();
        feed_set_subscribed(false);
    }
    abandon_read(); // the view being left reads again on its next visit
    key_ = key;
    ++visit_;
    slot(key_); // touch (LRU)
    overlay_->hide_loading();
    if (isVisible()) {
        present();
    } else {
        update_status();
        if (feed_mode_)
            feed_emit_stale();
        enforce_memory_budget(); // the view left may now be dropped
        refresh_agent_risk();    // the new view's kind (its own, else the pane's)
    }
}

void AutoLoadPane::set_view(const QString& key, Policy policy) {
    policy_ = policy;
    if (key == key_) {
        if (isVisible())
            present();
        return;
    }
    set_view(key);
}

void AutoLoadPane::set_view(const QString& key, Policy policy, const QString& signature) {
    // Before the switch: the view's first look (notice, default run) compares this one.
    slot(key).wanted_signature = signature;
    set_view(key, policy);
}

void AutoLoadPane::set_view_max_age(const QString& view, int seconds) {
    view_max_age_.insert(view, seconds < 0 ? autoload::kNeverRefresh : seconds);
    if (view == key_ && isVisible()) {
        update_notice();
        update_status();
        maybe_auto();
    }
}

void AutoLoadPane::clear_view_max_age(const QString& view) {
    view_max_age_.remove(view);
    if (view == key_ && isVisible()) {
        update_notice();
        update_status();
        maybe_auto();
    }
}

int AutoLoadPane::max_age_for(const QString& key) const {
    const auto it = view_max_age_.constFind(key);
    return it == view_max_age_.constEnd() ? max_age_s_ : it.value();
}

void AutoLoadPane::set_view_skeleton(const QString& view, LoadingSkeleton::Shape shape) {
    view_shapes_.insert(view, shape);
    if (view == key_ && stack_->currentWidget() == loading_page_)
        apply_view_skeleton();
}

void AutoLoadPane::clear_view_skeleton(const QString& view) {
    view_shapes_.remove(view);
    if (view == key_ && stack_->currentWidget() == loading_page_)
        apply_view_skeleton();
}

// ── Agent approval markers (since round 6 (7)) ───────────────────────────────

void AutoLoadPane::set_agent_risk(const QString& kind) {
    agent_risk_ = kind.trimmed(); // empty: the default rule
    refresh_agent_risk();
}

void AutoLoadPane::set_view_agent_risk(const QString& view, const QString& kind) {
    const QString k = kind.trimmed();
    if (k.isEmpty())
        view_agent_risk_.remove(view); // empty: the pane's kind
    else
        view_agent_risk_.insert(view, k);
    if (view == key_)
        refresh_agent_risk();
}

void AutoLoadPane::clear_view_agent_risk(const QString& view) {
    view_agent_risk_.remove(view);
    if (view == key_)
        refresh_agent_risk();
}

QString AutoLoadPane::effective_agent_risk() const {
    bool included = false;
    if (cost_credits_ > 0 && !cost_category_.isEmpty()) {
        const auto& plan = fincept::billing::PlanAccess::instance();
        included = plan.included(cost_category_) || plan.effective_cost(cost_category_, cost_credits_) <= 0;
    }
    return autoload::effective_agent_risk(agent_risk_, view_agent_risk_.value(key_), cost_credits_, included);
}

void AutoLoadPane::watch_plan_access() {
    if (plan_watch_ || cost_credits_ <= 0 || cost_category_.isEmpty())
        return;
    plan_watch_ = true;
    // A plan change can make the category included (or no longer): queued, once.
    connect(
        &fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, this,
        [this]() { refresh_agent_risk(); }, Qt::QueuedConnection);
}

void AutoLoadPane::refresh_agent_risk() {
    watch_plan_access();
    const QString kind = effective_agent_risk();
    if (kind == applied_risk_)
        return;
    applied_risk_ = kind;
    mark_agent_risk_widgets();
    update_notice();
}

void AutoLoadPane::mark_agent_risk_widgets() {
    // Preview RUN (always run()): hosts fill that page, so the kind is its default —
    // a host spec naming its own kind wins.
    preview_->set_primary_agent_risk_default(applied_risk_);
    // Error page: only when its primary is the pane's RETRY (not a host action).
    if (error_reruns_)
        error_->set_primary_agent_risk(applied_risk_);
    // Needs-setup page: only RETRY (secure storage unreadable) re-runs; OPEN
    // SETTINGS…, SIGN IN and host fixes are never marked.
    if (setup_status_.retry)
        setup_->set_primary_agent_risk(applied_risk_);
}

void AutoLoadPane::apply_view_skeleton() {
    const auto it = view_shapes_.constFind(key_);
    if (it != view_shapes_.constEnd()) {
        if (!shape_overridden_) {
            pane_shape_ = skeleton_->shape(); // the host's shape, given back to views without an override
            shape_overridden_ = true;
        }
        skeleton_->set_shape(it.value());
    } else if (shape_overridden_) {
        skeleton_->set_shape(pane_shape_);
        shape_overridden_ = false;
    }
}

void AutoLoadPane::set_cache_key(const QString& key, int ttl_days) {
    cache_key_ = key;
    cache_ttl_days_ = qMax(1, ttl_days);
}

void AutoLoadPane::set_persist_limit(int bytes) {
    persist_limit_ = qBound(0, bytes, kPersistCeilingBytes);
}

void AutoLoadPane::set_remembered_source(RememberedFn source) {
    remembered_ = std::move(source);
    remembered_signed_ = nullptr;
}

void AutoLoadPane::set_remembered_signed_source(RememberedSignedFn source) {
    remembered_signed_ = std::move(source);
    remembered_ = nullptr;
}

void AutoLoadPane::set_max_age(int seconds) {
    max_age_s_ = seconds < 0 ? autoload::kNeverRefresh : seconds;
    if (isVisible()) {
        update_notice();
        update_status();
        maybe_auto();
    }
}

void AutoLoadPane::set_signature(const QString& signature) {
    Slot& s = slot(key_);
    // From inside StartFn the host describes the run being started: its result carries this.
    if (in_start_ && current_id_ != 0 && current_key_ == key_)
        current_sig_ = signature;
    if (s.wanted_signature == signature)
        return;
    s.wanted_signature = signature;
    if (isVisible()) {
        update_notice();
        if (!s.has_result)
            maybe_auto();
    }
}

QString AutoLoadPane::signature() const {
    const Slot* s = find_slot(key_);
    return s ? s->wanted_signature : QString();
}

void AutoLoadPane::set_start(StartFn start) {
    start_ = std::move(start);
}

void AutoLoadPane::set_cancel(CancelFn cancel) {
    cancel_ = std::move(cancel);
}

void AutoLoadPane::set_render(RenderFn render) {
    render_ = std::move(render);
    rendered_key_.clear();
}

void AutoLoadPane::set_auto_delay(int ms) {
    auto_delay_ms_ = qMax(0, ms);
}

void AutoLoadPane::set_timeout(int seconds) {
    timeout_s_ = qMax(0, seconds);
}

void AutoLoadPane::set_cancellable(bool cancellable) {
    cancellable_ = cancellable;
    update_footer();
}

void AutoLoadPane::set_gate_priority(int priority) {
    gate_priority_ = priority;
}

void AutoLoadPane::set_requirements(const QList<Requirement>& requirements) {
    requirements_ = requirements;
    if (isVisible())
        present();
}

void AutoLoadPane::set_requirement_check(RequirementCheckFn check) {
    requirement_check_ = std::move(check);
    if (isVisible())
        present();
}

void AutoLoadPane::set_cost_credits(double credits, const QString& plan_category) {
    cost_credits_ = qMax(0.0, credits);
    cost_category_ = plan_category;
    refresh_agent_risk(); // round 6 (7): a cost the plan does not include marks the re-run buttons "spend"
}

void AutoLoadPane::recheck_requirements() {
    if (isVisible())
        present();
}

void AutoLoadPane::set_setup_secondary_action(const QString& text, ActionFn action) {
    setup_secondary_text_ = text;
    setup_secondary_ = text.isEmpty() ? ActionFn() : std::move(action);
    if (isVisible() && state_ == State::NeedsSetup)
        apply_setup_page(setup_status_);
}

void AutoLoadPane::set_view_preview(const QString& view, const EmptyState::Spec& spec) {
    view_previews_.insert(view, spec);
    if (view == key_ && isVisible() && state_ == State::Preview)
        preview_->apply(spec);
}

void AutoLoadPane::clear_view_preview(const QString& view) {
    view_previews_.remove(view);
}

void AutoLoadPane::apply_setup_page(const RequirementStatus& req) {
    EmptyState::Spec spec = EmptyState::needs_setup_spec(req);
    if (!setup_secondary_text_.isEmpty())
        spec.secondary = setup_secondary_text_;
    // Only RETRY (a check that could not be made) leads back to a run; links never.
    spec.primary_agent_risk = req.retry ? applied_risk_ : QString();
    setup_->apply(spec);
}

void AutoLoadPane::on_setup_action(bool primary) {
    if (!primary) {
        if (!setup_secondary_text_.isEmpty()) {
            if (setup_secondary_)
                setup_secondary_();
            return;
        }
        open_setup_link(setup_status_.secondary_link, this);
        return;
    }
    // A requirement that could not be checked (secure storage unreadable): look again.
    if (setup_status_.retry) {
        recheck_requirements();
        return;
    }
    // The host's own fix for this requirement (Requirement::with_action), not a Settings link.
    if (has_host_action(setup_status_)) {
        const std::function<void()> fn = setup_status_.missing.action; // a copy: it may re-check
        fn();
        return;
    }
    open_setup_link(setup_status_.link, this);
}

void AutoLoadPane::show_error_page(const Slot& s) {
    EmptyState::Spec spec = s.has_error_page
                                ? s.error_page
                                : EmptyState::error_spec(s.last_error, s.last_error_title, QString(), s.last_error_details);
    // The primary re-runs the load when it is the default RETRY (no host action) and
    // the host named no kind of its own for it.
    const bool default_retry = !s.has_error_page || !s.error_primary;
    error_reruns_ = default_retry && spec.primary_agent_risk.isEmpty();
    if (error_reruns_)
        spec.primary_agent_risk = applied_risk_;
    error_->apply(spec);
    show_page(error_, State::Failed);
}

void AutoLoadPane::on_error_action(bool primary) {
    if (feed_mode_) {
        if (primary)
            feed_retry(); // RETRY = subscribe again
        return;
    }
    const Slot* s = find_slot(key_);
    if (s && s->has_error_page) {
        // Copied: the handler may run(), which clears the slot's page.
        const ActionFn fn = primary ? s->error_primary : s->error_secondary;
        if (fn) {
            fn();
            return;
        }
        if (!primary)
            return;
    } else if (!primary) {
        return;
    }
    run();
}

void AutoLoadPane::on_error_detail(int index, const QString& key) {
    if (feed_mode_)
        return; // the feed error page has no host rows
    const Slot* s = find_slot(key_);
    // Copied: the handler may run(), which clears the slot's page.
    const DetailFn fn = s && s->has_error_page ? s->error_detail : DetailFn();
    const QPointer<AutoLoadPane> self(this);
    if (fn)
        fn(index, key);
    if (self)
        emit error_detail_clicked(index, key);
}

void AutoLoadPane::show_checking_page(const RequirementStatus& req) {
    // Not missing, not met yet (a saved sign-in being confirmed): the skeleton with
    // what is being checked — never the setup page. Nothing runs until it settles.
    overlay_->hide_loading();
    apply_view_skeleton();
    skeleton_->set_message(req.title);
    skeleton_->clear_progress();
    show_page(loading_page_, State::Loading);
    update_footer();
}

void AutoLoadPane::watch_requirements(const RequirementStatus& req) {
    if (requirement_watch_ || req.ok)
        return;
    if (!req.pending && req.missing.kind != RequirementKind::SignedIn)
        return; // other requirements are re-checked on show / RETRY / recheck_requirements()
    requirement_watch_ = true;
    watch_requirement_changes(this, [this]() {
        // loading_changed and auth_state_changed arrive together: one re-check.
        if (requirement_recheck_queued_)
            return;
        requirement_recheck_queued_ = true;
        QTimer::singleShot(0, this, [this]() {
            requirement_recheck_queued_ = false;
            if (!isVisible() || setup_status_.ok)
                return; // hidden: the next show checks again
            if (setup_status_.pending || setup_status_.missing.kind == RequirementKind::SignedIn)
                present();
        });
    });
}

void AutoLoadPane::set_loading_text(const QString& text) {
    loading_text_ = text;
    skeleton_->set_message(text);
}

void AutoLoadPane::set_progress(quint64 request_id, double fraction, const QString& text) {
    if (!is_current(request_id) || feed_mode_)
        return; // stale: superseded, cancelled or ended
    progress_fraction_ = std::isnan(fraction) || fraction < 0 ? -1.0 : std::min(1.0, fraction);
    progress_text_ = text;
    apply_progress();
}

void AutoLoadPane::apply_progress() {
    // Only the run of the view on screen shows its progress there.
    const bool here = current_id_ != 0 && current_key_ == key_;
    const bool any = progress_fraction_ >= 0 || !progress_text_.isEmpty();
    if (here && any)
        skeleton_->set_progress(progress_fraction_, progress_text_);
    else
        skeleton_->clear_progress();
    // The user's re-run over a result: the dimmed overlay's caption carries it. Its
    // caption is re-set only when the words change (5 % steps), so its bars keep pulsing.
    if (here && any && !current_auto_ && overlay_->isVisibleTo(stack_)) {
        QStringList parts{loading_text_.isEmpty() ? tr("LOADING…") : loading_text_};
        if (progress_fraction_ >= 0)
            parts << tr("%1 %").arg(int(progress_fraction_ * 20.0 + 0.5) * 5);
        if (!progress_text_.isEmpty())
            parts << progress_text_;
        const QString caption = parts.join(QStringLiteral(" · "));
        if (caption != overlay_caption_) {
            overlay_caption_ = caption;
            overlay_->show_loading(caption);
        }
    }
}

void AutoLoadPane::set_status_label(QLabel* label) {
    status_label_ = label;
    update_status();
}

void AutoLoadPane::set_notice(const QString& text, EmptyState::Tone tone, const QString& action_text) {
    notice_text_ = text;
    notice_tone_ = tone;
    notice_action_text_ = action_text;
    update_notice();
}

// ── Queries ──────────────────────────────────────────────────────────────────

bool AutoLoadPane::has_result() const {
    const Slot* s = find_slot(key_);
    return s && s->has_result;
}

QJsonValue AutoLoadPane::payload() const {
    const Slot* s = find_slot(key_);
    return s ? s->payload : QJsonValue();
}

QDateTime AutoLoadPane::as_of() const {
    const Slot* s = find_slot(key_);
    return s ? s->as_of : QDateTime();
}

bool AutoLoadPane::is_stale() const {
    const Slot* s = find_slot(key_);
    return s && s->has_result && autoload::is_stale(s->as_of, QDateTime::currentDateTimeUtc(), max_age_for(key_));
}

void AutoLoadPane::set_memory_budget(qint64 bytes) {
    memory_budget_ = bytes;
    enforce_memory_budget();
}

void AutoLoadPane::set_size_estimator(SizeFn estimator) {
    size_fn_ = std::move(estimator);
    for (auto it = slots_.begin(); it != slots_.end(); ++it)
        it->bytes = -1; // estimated again by the new rule
    enforce_memory_budget();
}

qint64 AutoLoadPane::memory_in_use() const {
    qint64 total = 0;
    for (auto it = slots_.cbegin(); it != slots_.cend(); ++it) {
        if (it->has_result && !it->host_held && it->bytes > 0) // host-held data is the host's memory
            total += it->bytes;
    }
    return total;
}

void AutoLoadPane::enforce_memory_budget(const QString& keep) {
    if (memory_budget_ <= 0)
        return;
    QList<autoload::MemorySlot> lru;
    lru.reserve(slot_order_.size());
    for (const QString& k : std::as_const(slot_order_)) { // least recently used first
        const auto it = slots_.find(k);
        if (it == slots_.end() || !it->has_result || it->host_held)
            continue; // host-held data (feed_data_ready) is neither counted nor dropped here
        Slot& s = it.value();
        if (s.bytes < 0) { // once per payload
            if (size_fn_) {
                s.bytes = std::max<qint64>(0, size_fn_(s.payload)); // the host's estimator (its cost)
            } else {
                // Never a walk of the payload here (GUI thread): an O(1) provisional
                // estimate now, the exact one from a worker when it lands (pushed data
                // keeps the provisional one — it changes per update and reloads anyway).
                s.bytes = autoload::shallow_json_bytes(s.payload);
                if (!feed_mode_)
                    request_exact_size(k, s);
            }
        }
        autoload::MemorySlot m;
        m.key = k;
        m.bytes = s.bytes;
        m.pinned = k == key_ || k == keep || (current_id_ != 0 && k == current_key_) || (queued_ && k == queued_key_);
        // Pushed data comes again from its source; a cached / host copy is read back on the next visit.
        m.reloadable = feed_mode_ || s.persisted || s.from_cache;
        m.manual = s.manual;
        lru.append(m);
    }
    const QStringList drop = autoload::choose_evictions(lru, memory_budget_);
    for (const QString& k : drop) {
        Slot& s = slots_[k];
        // Only the result goes: inputs, attempts and error state stay. The next visit
        // reads it back like a first show (cache_checked), which is not an automatic attempt.
        s.payload = QJsonValue();
        s.as_of = QDateTime();
        s.signature.clear();
        s.has_result = false;
        s.from_cache = false;
        s.persisted = false;
        s.refresh_failed = false;
        s.cache_checked = false;
        s.bytes = -1;
        if (rendered_key_ == k)
            rendered_key_.clear();
    }
}

bool AutoLoadPane::inputs_changed() const {
    const Slot* s = find_slot(key_);
    return s && s->has_result && !s->wanted_signature.isEmpty() && !s->signature.isEmpty() &&
           s->signature != s->wanted_signature;
}

QString AutoLoadPane::as_of_text(const QDateTime& as_of) {
    if (!as_of.isValid())
        return {};
    return tr("as of %1").arg(alp_when(as_of));
}

// ── Slots (one per view) ─────────────────────────────────────────────────────

AutoLoadPane::Slot& AutoLoadPane::slot(const QString& key) {
    slot_order_.removeOne(key);
    slot_order_.append(key);
    while (slot_order_.size() > kAlpMaxSlots) {
        // Evict the least recently used view that is neither on screen nor running.
        int victim = -1;
        for (int i = 0; i < slot_order_.size(); ++i) {
            const QString& k = slot_order_.at(i);
            if (k != key && k != key_ && !(current_id_ != 0 && k == current_key_)) {
                victim = i;
                break;
            }
        }
        if (victim < 0)
            break;
        slots_.remove(slot_order_.takeAt(victim));
    }
    return slots_[key];
}

const AutoLoadPane::Slot* AutoLoadPane::find_slot(const QString& key) const {
    const auto it = slots_.constFind(key);
    return it == slots_.constEnd() ? nullptr : &it.value();
}

QString AutoLoadPane::persist_key(const QString& key) const {
    if (cache_key_.isEmpty() || persist_limit_ <= 0)
        return {};
    if (key.isEmpty())
        return kAlpCachePrefix + cache_key_;
    // View keys can be long (inputs): hash them, the prefix stays readable.
    const QString view = key.size() <= 64
                             ? key
                             : QString::fromLatin1(
                                   QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
    return kAlpCachePrefix + cache_key_ + QLatin1Char('/') + view;
}

void AutoLoadPane::load_remembered(const QString& key, Slot& s) {
    if (remembered_signed_) {
        QJsonValue p;
        QDateTime at;
        QString sig;
        if (remembered_signed_(key, &p, &at, &sig) && at.isValid()) {
            s.payload = p;
            s.as_of = at;
            s.signature = sig;
            s.has_result = true;
            s.from_cache = true;
            s.bytes = -1;
            s.payload_seq = ++payload_seq_;
            s.manual = policy_ == Policy::Manual;
            return;
        }
    }
    if (remembered_) {
        QJsonValue p;
        QDateTime at;
        if (remembered_(key, &p, &at) && at.isValid()) {
            s.payload = p;
            s.as_of = at;
            s.has_result = true;
            s.from_cache = true;
            s.bytes = -1;
            s.payload_seq = ++payload_seq_;
            s.manual = policy_ == Policy::Manual;
            return;
        }
    }
    if (feed_mode_)
        return; // pushed data is never persisted, so there is nothing to read back
    const QString pkey = persist_key(key);
    if (pkey.isEmpty())
        return;
    // Read-your-writes: a write still waiting or running for this key is the newest copy.
    autoload::WriteBehind::Op pending;
    if (alp_writer().pending(pkey, &pending)) {
        if (pending.remove || !pending.as_of.isValid())
            return; // being removed: a miss
        s.payload = pending.payload;
        s.as_of = pending.as_of;
        s.signature = pending.signature;
        s.has_result = true;
        s.from_cache = true;
        s.persisted = false; // until that write lands
        s.bytes = -1;
        s.payload_seq = ++payload_seq_;
        s.manual = policy_ == Policy::Manual;
        return;
    }
    // The stored copy is read and parsed on a worker; the view shows the skeleton
    // meanwhile and decides its default run once the read resolves (on_read_done).
    abandon_read(); // one read per pane: the newest look wins
    s.reading = true;
    start_read(key, pkey);
}

void AutoLoadPane::start_read(const QString& key, const QString& pkey) {
    read_in_flight_ = true;
    reading_key_ = key;
    const quint64 gen = read_gen_;
    const qint64 ceiling = kPersistCeilingBytes;
    // Delivered back on this thread by the watcher, a child of the pane: a pane that
    // is gone never hears of it.
    auto* watcher = new QFutureWatcher<autoload::RememberedRead>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key, gen, pkey]() {
        const autoload::RememberedRead read = watcher->result();
        watcher->deleteLater();
        on_read_done(key, gen, read, pkey);
    });
    watcher->setFuture(QtConcurrent::run([pkey, ceiling = ceiling]() {
        // A pool thread: CacheDatabase gives it its own connection (per-thread clone).
        const std::optional<QString> text = CacheManager::instance().try_get(pkey);
        return text ? autoload::parse_remembered(*text, ceiling) : autoload::RememberedRead{};
    }));
}

void AutoLoadPane::abandon_read() {
    if (!read_in_flight_)
        return;
    ++read_gen_; // its answer is dropped (decide_read)
    read_in_flight_ = false;
    const auto it = slots_.find(reading_key_);
    if (it != slots_.end()) {
        it->reading = false;
        if (!it->has_result)
            it->cache_checked = false; // read again on the view's next look
    }
    reading_key_.clear();
}

void AutoLoadPane::on_read_done(const QString& key, quint64 gen, const autoload::RememberedRead& read,
                                const QString& pkey) {
    if (read_in_flight_ && gen == read_gen_ && reading_key_ == key) {
        read_in_flight_ = false;
        reading_key_.clear();
    }
    const auto it = slots_.find(key);
    autoload::ReadFacts f;
    f.started_gen = gen;
    f.current_gen = read_gen_;
    f.started_view = key;
    f.current_view = key_;
    f.visible = isVisible();
    f.run_for_view = current_id_ != 0 && current_key_ == key;
    f.has_result = it != slots_.end() && it->has_result;
    if (it == slots_.end() || autoload::decide_read(f) == autoload::ReadVerdict::Drop) {
        // Out of date (hidden, view left, invalidated, cleared, a run started or
        // delivered): nothing shown, no attempt spent; abandon_read() already left the
        // view to read again on its next look.
        if (it != slots_.end() && gen == read_gen_) {
            it->reading = false;
            if (!it->has_result)
                it->cache_checked = false;
        }
        return;
    }
    Slot& s = it.value();
    s.reading = false;
    switch (read.outcome) {
        case autoload::ReadOutcome::Hit:
            s.payload = read.payload;
            s.as_of = read.as_of;
            s.signature = read.signature;
            s.has_result = true;
            s.from_cache = true;
            s.persisted = true;
            s.manual = policy_ == Policy::Manual;
            s.payload_seq = ++payload_seq_;
            // The worker measured the text: no walk of the payload on this thread.
            s.bytes = size_fn_ ? -1 : autoload::bytes_from_json_text(read.text_bytes);
            break;
        case autoload::ReadOutcome::Corrupt:
            // Not a result: a cache miss, never an error page. The row goes (unless a
            // newer write of it is already on its way).
            if (!alp_writer().busy(pkey))
                alp_remove_persisted(pkey);
            break;
        case autoload::ReadOutcome::Miss:
            break;
    }
    present(); // the remembered result — or, on a miss, the default run (AutoRun) / preview (Manual)
}

void AutoLoadPane::request_exact_size(const QString& key, Slot& s) {
    if (s.size_task)
        return;
    s.size_task = true;
    const quint64 seq = s.payload_seq;
    const QJsonValue payload = s.payload; // implicitly shared: no copy of the data
    auto* watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key, seq]() {
        const qint64 bytes = watcher->result();
        watcher->deleteLater();
        const auto it = slots_.find(key);
        if (it == slots_.end())
            return;
        it->size_task = false;
        if (!it->has_result || it->host_held || size_fn_)
            return;
        if (it->payload_seq != seq) {
            if (!feed_mode_)
                request_exact_size(key, it.value()); // a newer payload: measure that one
            return;
        }
        it->bytes = bytes;
        enforce_memory_budget();
    });
    watcher->setFuture(QtConcurrent::run([payload]() { return autoload::approx_json_bytes(payload); }));
}

void AutoLoadPane::persist(const QString& key, const QJsonValue& payload, const QDateTime& at,
                           const QString& signature) {
    const QString pkey = persist_key(key);
    if (pkey.isEmpty())
        return;
    // Write-behind (alp_writer): serialised and stored on a pool thread; the latest
    // result per key wins, and the write outlives this pane.
    autoload::WriteBehind::Op op;
    op.key = pkey;
    op.payload = payload;
    op.as_of = at;
    op.signature = signature;
    op.work = alp_put_work(pkey, payload, at, signature, cache_ttl_days_ * 24 * 3600, persist_limit_);
    const QPointer<AutoLoadPane> self(this);
    op.done = [self, key, at](qint64 bytes) {
        if (!self || bytes < 0)
            return; // not kept (empty / over the limit), or the pane is gone
        const auto it = self->slots_.find(key);
        if (it == self->slots_.end() || !it->has_result || it->as_of != at)
            return;
        // This result now reloads without a run: the memory budget may drop it first;
        // and its size is known from the text the worker wrote.
        it->persisted = true;
        if (!self->size_fn_)
            it->bytes = autoload::bytes_from_json_text(bytes);
        self->enforce_memory_budget();
    };
    alp_writer().submit(std::move(op));
}

// ── Requirements ─────────────────────────────────────────────────────────────

RequirementStatus AutoLoadPane::evaluate_requirements() const {
    if (requirement_check_) {
        const RequirementStatus custom = requirement_check_();
        if (!custom.ok)
            return apply_host_action(custom); // a custom check may carry missing.action too
    }
    // What the plan really pays for a run (an included category costs a Pro
    // account nothing, so an empty balance must not block it).
    const double cost = cost_category_.isEmpty()
                            ? cost_credits_
                            : fincept::billing::PlanAccess::instance().effective_cost(cost_category_, cost_credits_);
    if (requirements_.isEmpty() && cost <= 0)
        return RequirementStatus{};
    QList<Requirement> all = requirements_;
    if (cost > 0)
        all.append(Requirement::credits(cost));
    return check_requirements(all);
}

// ── What the pane shows ──────────────────────────────────────────────────────

void AutoLoadPane::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    ++visit_;
    // Out of the show event (no layout work inside it); the host may still be
    // selecting the view.
    QTimer::singleShot(0, this, [this]() {
        if (isVisible())
            present();
    });
}

void AutoLoadPane::hideEvent(QHideEvent* e) {
    auto_timer_->stop();
    if (feed_mode_) {
        // Leaving: no stale clock while hidden, and the host may drop its subscription.
        feed_stale_timer_->stop();
        feed_set_subscribed(false);
        feed_retry_next_ = false; // the next show is a first show
    }
    // Leaving the view cancels its default run (waiting or running); it runs
    // again on the next show (a new visit).
    if (queued_)
        leave_gate();
    if (current_id_ != 0 && current_auto_)
        drop_running();
    abandon_read(); // hidden: its answer is dropped; the next show reads again
    QWidget::hideEvent(e);
}

void AutoLoadPane::show_page(QWidget* page, State state) {
    if (page && stack_->currentWidget() != page)
        stack_->setCurrentWidget(page);
    set_state(state);
}

void AutoLoadPane::show_loading_page(bool queued) {
    overlay_->hide_loading();
    apply_view_skeleton();
    skeleton_->set_message(loading_text_);
    apply_progress(); // the running request's progress, kept across hide / show
    show_page(loading_page_, queued ? State::Queued : State::Loading);
    update_footer();
}

autoload::ViewFacts AutoLoadPane::facts_for(const Slot& s, bool requirements_ok) const {
    autoload::ViewFacts f;
    f.auto_policy = policy_ == Policy::AutoRun && static_cast<bool>(start_);
    f.requirements_ok = requirements_ok;
    f.running = current_id_ != 0 || queued_;
    f.attempted_this_visit = s.attempted_visit == visit_;
    f.has_result = s.has_result;
    f.stale = s.has_result && autoload::is_stale(s.as_of, QDateTime::currentDateTimeUtc(), max_age_for(key_));
    f.signature_mismatch = s.has_result && !s.wanted_signature.isEmpty() && !s.signature.isEmpty() &&
                           s.signature != s.wanted_signature;
    f.forced = s.forced;
    return f;
}

void AutoLoadPane::present() {
    if (feed_mode_) {
        present_feed();
        return;
    }
    Slot& s = slot(key_);
    // Not while this view's run is in flight: it answers itself (and a read would only be dropped).
    if (!s.cache_checked && !(current_id_ != 0 && current_key_ == key_)) {
        s.cache_checked = true;
        if (!s.has_result)
            load_remembered(key_, s); // host copy at once; the persisted copy on a worker (s.reading)
    }
    const RequirementStatus req = evaluate_requirements();
    setup_status_ = req;
    watch_requirements(req);
    // Agent-approval kind of this view's re-run buttons (pages below are built with it).
    applied_risk_ = effective_agent_risk();
    mark_agent_risk_widgets();
    const bool running_here = current_id_ != 0 && current_key_ == key_;
    const bool queued_here = queued_ && queued_key_ == key_;

    if (s.has_result && content_) {
        // A remembered / fresh result: always shown (a missing requirement becomes a notice).
        if (rendered_key_ != key_ || rendered_as_of_ != s.as_of) {
            if (render_)
                render_(s.payload, s.as_of, s.from_cache);
            rendered_key_ = key_;
            rendered_as_of_ = s.as_of;
            emit result_shown(s.as_of, s.from_cache);
        }
        const bool dimmed = running_here && !current_auto_;
        if (!dimmed)
            overlay_->hide_loading();
        show_page(content_, dimmed ? State::Loading : State::Content);
    } else if (s.reading) {
        // The remembered copy is being read on a worker: the skeleton until it resolves
        // (then present() again decides: result, setup page, default run or preview).
        show_loading_page(false);
    } else if (!req.ok && !running_here) {
        overlay_->hide_loading();
        if (req.pending) {
            show_checking_page(req);
        } else {
            apply_setup_page(req);
            show_page(setup_, State::NeedsSetup);
        }
    } else if (running_here || queued_here) {
        show_loading_page(queued_here && !running_here);
    } else if (autoload::should_auto_run(facts_for(s, req.ok))) {
        show_loading_page(false); // the default run starts in a moment
    } else if (!s.last_error.isEmpty() || s.has_error_page) {
        overlay_->hide_loading();
        show_error_page(s);
    } else {
        overlay_->hide_loading();
        const auto vp = view_previews_.constFind(key_);
        if (vp != view_previews_.constEnd())
            preview_->apply(vp.value());
        show_page(preview_, State::Preview);
    }
    update_notice();
    update_status();
    enforce_memory_budget(); // a result read back just now may push older views out
    maybe_auto();
}

void AutoLoadPane::maybe_auto() {
    if (!isVisible() || feed_mode_)
        return;
    if (const Slot* rs = find_slot(key_); rs && rs->reading)
        return; // decided once the remembered copy is read (no attempt spent meanwhile)
    const RequirementStatus req = evaluate_requirements();
    if (!autoload::should_auto_run(facts_for(slot(key_), req.ok)))
        return;
    auto_timer_->start(auto_delay_ms_);
}

// ── Runs ─────────────────────────────────────────────────────────────────────

void AutoLoadPane::request_auto_start() {
    Slot& s = slot(key_);
    s.attempted_visit = visit_; // at most one automatic attempt per visit
    s.forced = false;
    const QString key = key_;
    quint64 ticket = 0;
    const bool now = AutoLoadGate::instance().enter(
        this,
        [this, key]() {
            queued_ = false;
            if (!isVisible() || key != key_ || current_id_ != 0) {
                leave_gate(); // the moment passed: hand the slot on
                if (isVisible())
                    present();
                return;
            }
            begin(true);
        },
        gate_priority_, &ticket);
    gate_ticket_ = ticket;
    if (now) {
        begin(true);
        return;
    }
    queued_ = true;
    queued_key_ = key;
    if (!s.has_result)
        show_loading_page(true);
    update_status();
}

void AutoLoadPane::run() {
    if (feed_mode_) {
        feed_retry(); // nothing to run: ask the source again
        return;
    }
    auto_timer_->stop();
    leave_gate(); // a user run never waits in the gate
    begin(false);
}

void AutoLoadPane::begin(bool automatic) {
    auto_timer_->stop();
    // Every run path comes here (RUN, RETRY, RUN AGAIN, REFRESH, the default run,
    // its turn in the gate): a missing requirement shows the setup page / notice
    // and nothing starts — a key removed since the last look included.
    const RequirementStatus req = start_ ? evaluate_requirements() : RequirementStatus{};
    switch (autoload::gate_run(static_cast<bool>(start_), req.ok)) {
        case autoload::RunGate::NoAction:
            leave_gate();
            return;
        case autoload::RunGate::NeedsSetup:
            leave_gate();
            setup_status_ = req;
            // No automatic retry in this visit — unless the check is only pending (a
            // sign-in being confirmed): the default run then starts once it settles.
            if (!req.pending)
                slot(key_).attempted_visit = visit_;
            if (isVisible())
                present();
            else
                update_status();
            return;
        case autoload::RunGate::Start:
            break;
    }
    abandon_read(); // a run started: its answer wins over a remembered copy still being read
    if (current_id_ != 0) {
        // Superseded: stop the old run (keep this run's gate slot when automatic).
        const quint64 old = std::exchange(current_id_, 0);
        if (cancel_)
            cancel_(old);
        emit running_changed(false);
    }
    Slot& s = slot(key_);
    current_id_ = ++next_id_;
    current_key_ = key_;
    current_auto_ = automatic;
    current_sig_ = s.wanted_signature;
    queued_ = false;
    s.attempted_visit = visit_;
    s.forced = false;
    s.refresh_failed = false;
    s.last_error.clear();
    s.last_error_details.clear();
    s.has_error_page = false;
    s.error_primary = nullptr;
    s.error_secondary = nullptr;
    s.error_detail = nullptr;
    if (!automatic)
        leave_gate();
    // A new run starts without progress (set_progress); the overlay caption below is fresh.
    progress_fraction_ = -1;
    progress_text_.clear();
    overlay_caption_.clear();
    skeleton_->clear_progress();
    if (!s.has_result || !content_) {
        show_loading_page(false);
    } else if (!automatic) {
        // The user asked: dim the result on screen until the new one arrives.
        show_page(content_, State::Loading);
        overlay_->show_loading(loading_text_.isEmpty() ? tr("LOADING…") : loading_text_);
    }
    // (an automatic run over a remembered result is a quiet refresh)
    if (timeout_s_ > 0)
        timeout_timer_->start(timeout_s_ * 1000);
    reveal_timer_->start();
    update_footer();
    update_notice();
    update_status();
    emit running_changed(true);
    const quint64 id = current_id_;
    // set_signature() from inside StartFn describes this run (in_start_).
    const bool outer = in_start_;
    in_start_ = true;
    const StartFn start = start_; // a copy: StartFn may replace itself
    start(id);                    // may deliver synchronously
    in_start_ = outer;
}

void AutoLoadPane::leave_gate() {
    queued_ = false;
    if (gate_ticket_ != 0)
        AutoLoadGate::instance().leave(std::exchange(gate_ticket_, 0));
}

void AutoLoadPane::end_run() {
    current_id_ = 0;
    current_auto_ = false;
    // Progress belongs to the run: gone with it.
    progress_fraction_ = -1;
    progress_text_.clear();
    overlay_caption_.clear();
    skeleton_->clear_progress();
    reveal_timer_->stop();
    timeout_timer_->stop();
    leave_gate();
    update_footer();
    emit running_changed(false);
}

void AutoLoadPane::drop_running() {
    if (current_id_ == 0)
        return;
    const quint64 id = current_id_;
    end_run();
    if (cancel_)
        cancel_(id);
}

void AutoLoadPane::cancel() {
    if (feed_mode_)
        return; // nothing runs in feed mode
    if (queued_) {
        leave_gate();
        slot(queued_key_).attempted_visit = visit_; // declined: no automatic retry in this visit
    }
    if (current_id_ != 0) {
        slot(current_key_).attempted_visit = visit_;
        drop_running();
    }
    if (isVisible())
        present();
    else
        update_status();
}

void AutoLoadPane::invalidate() {
    if (feed_mode_) {
        feed_retry();
        return;
    }
    abandon_read(); // a read in flight is dropped; the view reads again, then the forced run follows
    Slot& s = slot(key_);
    s.forced = true;
    s.attempted_visit = 0;
    if (isVisible()) {
        if (!s.has_result)
            present();
        else
            maybe_auto();
    }
}

void AutoLoadPane::clear_result() {
    abandon_read(); // a read in flight must not bring the copy back
    Slot& s = slot(key_);
    const QString pkey = persist_key(key_);
    if (!pkey.isEmpty())
        alp_remove_persisted(pkey); // queued after any write of that key still running
    const QString wanted = s.wanted_signature;
    s = Slot{};
    s.cache_checked = true;
    s.wanted_signature = wanted;
    rendered_key_.clear();
    if (isVisible())
        present();
}

void AutoLoadPane::deliver(quint64 request_id, const QJsonValue& payload) {
    deliver(request_id, payload, QDateTime::currentDateTimeUtc());
}

void AutoLoadPane::deliver(quint64 request_id, const QJsonValue& payload, const QDateTime& as_of) {
    if (!is_current(request_id))
        return; // stale: superseded, cancelled or its view was left
    const QString key = current_key_;
    const QString sig = current_sig_;
    end_run();
    Slot& s = slot(key);
    s.payload = payload;
    s.as_of = as_of.isValid() ? as_of.toUTC() : QDateTime::currentDateTimeUtc();
    s.signature = sig;
    s.has_result = true;
    s.from_cache = false;
    s.refresh_failed = false;
    s.last_error.clear();
    s.last_error_details.clear();
    s.has_error_page = false;
    s.error_primary = nullptr;
    s.error_secondary = nullptr;
    s.error_detail = nullptr;
    s.cache_checked = true;
    s.bytes = -1; // provisional estimate in the budget check; exact one from a worker (round 6 (8))
    s.payload_seq = ++payload_seq_;
    s.persisted = false; // until the cache write lands (persist)
    s.manual = policy_ == Policy::Manual;
    persist(key, payload, s.as_of, sig);
    enforce_memory_budget(key); // the result just made stays; the least recently used go first
    if (key == key_ && content_) {
        overlay_->hide_loading();
        if (render_)
            render_(s.payload, s.as_of, false);
        rendered_key_ = key;
        rendered_as_of_ = s.as_of;
        show_page(content_, State::Content);
        emit result_shown(s.as_of, false);
        update_notice();
        update_status();
    }
    // The view on screen may still be waiting for its own default run.
    if (key != key_ && isVisible())
        present();
}

void AutoLoadPane::deliver_partial(quint64 request_id, const QJsonValue& payload) {
    if (!is_current(request_id))
        return;
    const QString key = current_key_;
    Slot& s = slot(key);
    s.payload = payload;
    s.as_of = QDateTime::currentDateTimeUtc();
    s.signature = current_sig_;
    s.has_result = true;
    s.from_cache = false;
    s.cache_checked = true;
    s.bytes = -1;
    s.payload_seq = ++payload_seq_;
    s.persisted = false;
    s.manual = policy_ == Policy::Manual;
    enforce_memory_budget(key); // the running view is pinned anyway
    if (key == key_ && content_) {
        overlay_->hide_loading();
        if (render_)
            render_(s.payload, s.as_of, false);
        rendered_key_ = key;
        rendered_as_of_ = s.as_of;
        show_page(content_, State::Content);
        emit result_shown(s.as_of, false);
        update_notice();
        update_status();
    }
}

void AutoLoadPane::fail(quint64 request_id, const QString& message, const QString& title) {
    if (!is_current(request_id))
        return;
    fail_current(message, title, false);
}

void AutoLoadPane::fail(quint64 request_id, const QString& message, const QString& title, const QString& details) {
    if (!is_current(request_id))
        return;
    fail_current(message, title, false, details);
}

void AutoLoadPane::fail(quint64 request_id, const EmptyState::Spec& page, ActionFn primary, ActionFn secondary) {
    fail(request_id, page, std::move(primary), std::move(secondary), DetailFn());
}

void AutoLoadPane::fail(quint64 request_id, const EmptyState::Spec& page, ActionFn primary, ActionFn secondary,
                        DetailFn on_detail, const QString& details_agent_risk) {
    if (!is_current(request_id))
        return;
    Slot& s = slot(current_key_);
    s.has_error_page = true;
    s.error_page = page;
    if (on_detail)
        s.error_page.details_clickable = true; // the rows are the actions
    if (!details_agent_risk.isEmpty())
        s.error_page.details_agent_risk = details_agent_risk; // every row (page.details_agent_risks still win per row)
    s.error_primary = std::move(primary);
    s.error_secondary = std::move(secondary);
    s.error_detail = std::move(on_detail);
    // The plain text goes to the log and to the "refresh failed" path.
    fail_current(page.message, page.title, false);
}

void AutoLoadPane::fail_current(const QString& message, const QString& title, bool timed_out,
                                const QString& details) {
    const QString key = current_key_;
    const quint64 id = current_id_;
    end_run();
    if (timed_out && cancel_)
        cancel_(id); // the host stops the work that never answered
    Slot& s = slot(key);
    LOG_WARN("AutoLoad", QStringLiteral("%1%2 failed: %3")
                             .arg(cache_key_.isEmpty() ? QStringLiteral("view") : cache_key_,
                                  key.isEmpty() ? QString() : QStringLiteral(" / ") + key.left(80), message.left(300)));
    if (s.has_result) {
        s.refresh_failed = true;
        if (key == key_) {
            overlay_->hide_loading();
            show_page(content_, State::Content);
            emit refresh_failed(message);
        }
    } else {
        s.last_error = message.isEmpty() ? tr("The request failed.") : message;
        s.last_error_title = title;
        s.last_error_details = details;
        if (key == key_) {
            overlay_->hide_loading();
            show_error_page(s);
        }
    }
    update_notice();
    update_status();
    if (key != key_ && isVisible())
        present();
}

// ── Chrome: footer, notice, status ───────────────────────────────────────────

void AutoLoadPane::update_footer() {
    QString text;
    if (queued_) {
        const int pos = AutoLoadGate::instance().position(gate_ticket_);
        text = pos > 0 ? tr("Queued — %n other load(s) ahead", nullptr, pos) : tr("Queued — starting next");
    } else if (current_id_ != 0 && !reveal_timer_->isActive()) {
        text = tr("Still working…");
    }
    if (footer_label_->text() != text)
        footer_label_->setText(text);
    footer_label_->setVisible(!text.isEmpty());
    const bool can_cancel = cancellable_ && ((current_id_ != 0 && !reveal_timer_->isActive()) || queued_);
    cancel_btn_->setVisible(can_cancel);
    // The user's re-run over a result on screen (dimmed by the overlay): CANCEL there too.
    const bool overlay_cancel = cancellable_ && current_id_ != 0 && !current_auto_ && !reveal_timer_->isActive() &&
                                overlay_->isVisibleTo(stack_);
    if (overlay_cancel_btn_->isVisibleTo(overlay_) != overlay_cancel) {
        overlay_cancel_btn_->setVisible(overlay_cancel);
        if (overlay_cancel)
            alp_place_overlay_cancel(overlay_cancel_btn_);
    }
}

void AutoLoadPane::update_notice() {
    const Slot* s = find_slot(key_);
    const bool content_shown = content_ && stack_->currentWidget() == content_;
    QString text;
    QString action;
    EmptyState::Tone tone = EmptyState::Tone::Warning;
    NoticeAction kind = NoticeAction::None;
    // The host's notice has its own strip (below): both can show at once.
    {
        const QString host_tone = alp_tone_key(notice_tone_);
        if (host_notice_label_->text() != notice_text_)
            host_notice_label_->setText(notice_text_);
        host_notice_btn_->setText(alp_button_text(notice_action_text_));
        host_notice_btn_->setVisible(!notice_action_text_.isEmpty());
        if (host_notice_->property("tone").toString() != host_tone) {
            host_notice_->setProperty("tone", host_tone);
            host_notice_->style()->unpolish(host_notice_);
            host_notice_->style()->polish(host_notice_);
        }
        host_notice_->setVisible(!notice_text_.isEmpty());
    }
    if (content_shown && !setup_status_.ok) {
        // Pending (a saved sign-in being confirmed): nothing to fix and nothing runs
        // meanwhile — no notice over the result until it settles.
        if (!setup_status_.pending) {
            text = setup_status_.title + QStringLiteral(" — ") + setup_status_.message;
            action = setup_status_.action_text;
            kind = action.isEmpty() ? NoticeAction::None : NoticeAction::Setup;
        }
    } else if (feed_mode_) {
        // Pushed data: what the source does, under the data it last sent.
        if (content_shown && s) {
            switch (feed_decision(*s).marker) {
                case autoload::FeedMarker::Stopped:
                    text = tr("Live updates stopped — %1").arg(s->feed_message);
                    tone = EmptyState::Tone::Danger;
                    if (feed_subscribe_) {
                        action = tr("RETRY");
                        kind = NoticeAction::Run; // run() re-subscribes in feed mode
                    }
                    break;
                case autoload::FeedMarker::Idle:
                    text = s->feed_message.isEmpty() ? s->feed_title
                                                     : tr("%1 — %2").arg(s->feed_title, s->feed_message);
                    tone = EmptyState::Tone::Neutral;
                    break;
                case autoload::FeedMarker::Stale:
                    text = tr("No update for over %1 — showing the last data received.")
                               .arg(alp_duration(feed_stale_after_s_));
                    tone = EmptyState::Tone::Info;
                    if (feed_subscribe_) {
                        action = tr("RECONNECT");
                        kind = NoticeAction::Run;
                    }
                    break;
                case autoload::FeedMarker::Connecting:
                case autoload::FeedMarker::None:
                    break;
            }
        }
    } else if (content_shown && inputs_changed() && !(current_id_ != 0 && current_key_ == key_)) {
        text = tr("Inputs changed — this result is for the previous inputs.");
        action = tr("RUN AGAIN");
        kind = NoticeAction::Run;
        tone = EmptyState::Tone::Info;
    } else if (content_shown && s && s->refresh_failed) {
        text = tr("Refresh failed — showing the result %1.").arg(as_of_text(s->as_of));
        action = tr("RETRY");
        kind = NoticeAction::Run;
        tone = EmptyState::Tone::Danger;
    } else if (content_shown && policy_ == Policy::Manual && is_stale() &&
               !(current_id_ != 0 && current_key_ == key_)) {
        text = tr("Saved result %1 — it may be out of date.").arg(as_of_text(s ? s->as_of : QDateTime()));
        action = tr("REFRESH");
        kind = NoticeAction::Run;
    }
    notice_action_kind_ = kind;
    if (notice_label_->text() != text)
        notice_label_->setText(text);
    notice_btn_->setText(alp_button_text(action));
    notice_btn_->setVisible(!action.isEmpty());
    // RUN AGAIN / RETRY / REFRESH / RECONNECT (and the setup RETRY) re-run the load:
    // the agent-approval kind; links to Settings / sign-in carry none.
    const bool reruns = kind == NoticeAction::Run || (kind == NoticeAction::Setup && setup_status_.retry);
    EmptyState::mark_agent_risk(notice_btn_, reruns ? applied_risk_ : QString());
    const QString tone_key = alp_tone_key(tone);
    if (notice_->property("tone").toString() != tone_key) {
        notice_->setProperty("tone", tone_key);
        notice_->style()->unpolish(notice_);
        notice_->style()->polish(notice_);
    }
    notice_->setVisible(!text.isEmpty());
}

void AutoLoadPane::set_state(State state) {
    if (state_ == state)
        return;
    state_ = state;
    emit state_changed(state);
}

void AutoLoadPane::update_status() {
    if (!status_label_)
        return;
    if (feed_mode_) {
        const Slot* fs = find_slot(key_);
        QString text;
        if (fs && fs->has_result && content_) {
            text = as_of_text(fs->as_of);
            switch (feed_decision(*fs).marker) {
                case autoload::FeedMarker::Connecting:
                    text = tr("%1 · connecting…").arg(text);
                    break;
                case autoload::FeedMarker::Stale:
                    text = tr("%1 · no update for over %2").arg(text, alp_duration(feed_stale_after_s_));
                    break;
                case autoload::FeedMarker::Stopped:
                    text = tr("%1 · updates stopped").arg(text);
                    break;
                case autoload::FeedMarker::Idle:
                    text = tr("%1 · %2").arg(text, fs->feed_title);
                    break;
                case autoload::FeedMarker::None:
                    break;
            }
        } else if (fs && fs->feed_report == autoload::FeedReport::Error) {
            text = tr("Failed");
        } else if (fs && fs->feed_report == autoload::FeedReport::Idle) {
            text = fs->feed_title;
        } else if (setup_status_.ok) {
            text = tr("Waiting for data…");
        } else if (setup_status_.pending) {
            text = setup_status_.title; // "Checking sign-in…"
        }
        if (fs && fs->feed_quiet)
            text.clear(); // feed_reset(): nothing to say until the next report
        if (status_label_->text() != text)
            status_label_->setText(text);
        return;
    }
    const Slot* s = find_slot(key_);
    const bool running_here = current_id_ != 0 && current_key_ == key_;
    QString text;
    if (s && s->has_result) {
        text = as_of_text(s->as_of);
        if (running_here)
            text = tr("%1 · refreshing…").arg(text);
        else if (s->refresh_failed)
            text = tr("%1 · refresh failed").arg(text);
        else if (is_stale() && policy_ == Policy::Manual)
            text = tr("%1 · out of date").arg(text);
    } else if (running_here) {
        text = tr("Loading…");
    } else if (queued_ && queued_key_ == key_) {
        text = tr("Queued…");
    } else if (!setup_status_.ok && setup_status_.pending) {
        text = setup_status_.title; // "Checking sign-in…"
    } else if (s && !s->last_error.isEmpty()) {
        text = tr("Failed");
    }
    if (status_label_->text() != text)
        status_label_->setText(text);
}

// ── Feed mode: data pushed by a hub / adapter ────────────────────────────────

void AutoLoadPane::set_feed_mode(bool on) {
    if (feed_mode_ == on)
        return;
    if (on) {
        // Nothing runs in feed mode: a run in flight or waiting is stopped (CancelFn).
        auto_timer_->stop();
        leave_gate();
        drop_running();
        abandon_read(); // the persisted copy is not used in feed mode
        overlay_->hide_loading();
        feed_mode_ = true;
    } else {
        feed_stale_timer_->stop();
        feed_set_subscribed(false);
        feed_mode_ = false;
        feed_emit_stale();
    }
    if (isVisible())
        present();
    else
        update_status();
}

void AutoLoadPane::set_feed_subscription(ActionFn subscribe, ActionFn unsubscribe) {
    SubscribeFn wrapped; // the retry flag is simply not passed on
    if (subscribe)
        wrapped = [fn = std::move(subscribe)](bool) { fn(); };
    set_feed_subscription(std::move(wrapped), std::move(unsubscribe));
}

void AutoLoadPane::set_feed_subscription(SubscribeFn subscribe, ActionFn unsubscribe) {
    const bool had_subscribe = static_cast<bool>(feed_subscribe_);
    feed_subscribe_ = std::move(subscribe);
    feed_unsubscribe_ = std::move(unsubscribe);
    // Wanted earlier with nothing to call: call the new one now (present_feed).
    if (!had_subscribe)
        feed_subscribed_ = false;
    if (feed_mode_ && isVisible())
        present_feed();
}

void AutoLoadPane::set_feed_stale_after(int seconds) {
    feed_stale_after_s_ = std::max(0, seconds);
    if (feed_mode_ && isVisible()) {
        feed_arm_stale_timer(true);
        update_notice();
        update_status();
        feed_emit_stale();
    }
}

void AutoLoadPane::feed_waiting() {
    if (!feed_mode_)
        set_feed_mode(true);
    Slot& s = slot(key_);
    s.feed_report = autoload::FeedReport::Waiting;
    s.feed_quiet = false;
    s.feed_title.clear();
    s.feed_message.clear();
    s.feed_details.clear();
    s.feed_glyph.clear();
    if (isVisible())
        present_feed();
    else
        update_status();
}

void AutoLoadPane::feed_data(const QJsonValue& payload, const QDateTime& as_of) {
    feed_store(payload, as_of, false);
}

void AutoLoadPane::feed_data_ready(const QDateTime& as_of) {
    // The host keeps the data: the RenderFn gets a null value and draws from its own copy.
    feed_store(QJsonValue(), as_of, true);
}

void AutoLoadPane::feed_reset() {
    if (!feed_mode_)
        set_feed_mode(true);
    Slot& s = slot(key_);
    s.payload = QJsonValue();
    s.as_of = QDateTime();
    s.signature.clear();
    s.has_result = false;
    s.from_cache = false;
    s.host_held = false;
    s.persisted = false;
    s.bytes = -1;
    s.cache_checked = true; // "nothing yet": not the host's remembered copy again
    s.feed_report = autoload::FeedReport::None;
    s.feed_received = QDateTime();
    s.feed_title.clear();
    s.feed_message.clear();
    s.feed_details.clear();
    s.feed_glyph.clear();
    s.feed_quiet = true; // empty status label until the next report
    rendered_key_.clear();
    rendered_as_of_ = QDateTime();
    feed_stale_timer_->stop();
    // The subscription stays: present_feed() asks for it only when it is not held.
    if (isVisible()) {
        present_feed(); // the waiting skeleton
    } else {
        update_notice();
        update_status();
        feed_emit_stale();
    }
}

void AutoLoadPane::feed_store(const QJsonValue& payload, const QDateTime& as_of, bool host_held) {
    if (!feed_mode_)
        set_feed_mode(true);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    Slot& s = slot(key_);
    s.payload = payload;
    s.as_of = as_of.isValid() ? as_of.toUTC() : now;
    s.has_result = true;
    s.from_cache = false;
    s.host_held = host_held;
    s.feed_quiet = false;
    s.cache_checked = true;
    s.feed_report = autoload::FeedReport::Data;
    s.feed_received = now;
    s.bytes = host_held ? 0 : -1; // JSON estimated when the budget is next checked (not per update)
    s.payload_seq = ++payload_seq_;
    s.feed_title.clear();
    s.feed_message.clear();
    s.feed_details.clear();
    s.feed_glyph.clear();
    if (!isVisible()) {
        rendered_key_.clear(); // drawn on the next show (an update can keep the same as-of)
        update_status();
        return;
    }
    // The next update of data already on screen: draw it and refresh the markers —
    // no requirement check and no page switch per tick.
    if (content_ && stack_->currentWidget() == content_ && rendered_key_ == key_) {
        if (render_)
            render_(s.payload, s.as_of, false);
        rendered_as_of_ = s.as_of;
        feed_after_change();
        return;
    }
    present_feed();
}

void AutoLoadPane::feed_error(const QString& message, const QString& details, const QString& title) {
    if (!feed_mode_)
        set_feed_mode(true);
    Slot& s = slot(key_);
    s.feed_report = autoload::FeedReport::Error;
    s.feed_quiet = false;
    s.feed_title = title; // empty: error_spec's "Could not load"
    s.feed_message = message.isEmpty() ? tr("The data source reported an error.") : message;
    s.feed_details = details;
    s.feed_glyph.clear();
    if (isVisible())
        present_feed();
    else
        update_status();
}

void AutoLoadPane::feed_idle(const QString& title, const QString& message, const QString& glyph) {
    if (!feed_mode_)
        set_feed_mode(true);
    Slot& s = slot(key_);
    s.feed_report = autoload::FeedReport::Idle;
    s.feed_quiet = false;
    s.feed_title = title.isEmpty() ? tr("Not updating") : title;
    s.feed_message = message;
    s.feed_details.clear();
    s.feed_glyph = glyph;
    if (isVisible())
        present_feed();
    else
        update_status();
}

bool AutoLoadPane::feed_stale() const {
    const Slot* s = find_slot(key_);
    return feed_mode_ && s && feed_decision(*s).marker == autoload::FeedMarker::Stale;
}

autoload::FeedFacts AutoLoadPane::feed_facts(const Slot& s, bool requirements_ok) const {
    autoload::FeedFacts f;
    f.requirements_ok = requirements_ok;
    f.has_data = s.has_result && content_ != nullptr;
    f.report = s.feed_report;
    f.last_update = s.feed_received.isValid() ? s.feed_received : s.as_of;
    f.now = QDateTime::currentDateTimeUtc();
    f.stale_after_seconds = feed_stale_after_s_;
    return f;
}

autoload::FeedDecision AutoLoadPane::feed_decision(const Slot& s) const {
    return autoload::decide_feed(feed_facts(s, setup_status_.ok));
}

void AutoLoadPane::present_feed() {
    Slot& s = slot(key_);
    if (!s.cache_checked) {
        s.cache_checked = true;
        if (!s.has_result) {
            load_remembered(key_, s); // a host's remembered copy (never the persistent cache)
            if (s.has_result)
                s.feed_received = s.as_of;
        }
    }
    const RequirementStatus req = evaluate_requirements();
    setup_status_ = req;
    watch_requirements(req);
    applied_risk_ = effective_agent_risk(); // RETRY / RECONNECT re-subscribe
    mark_agent_risk_widgets();
    const autoload::FeedDecision d = autoload::decide_feed(feed_facts(s, req.ok));
    overlay_->hide_loading();
    switch (d.page) {
        case autoload::FeedPage::Content: {
            bool drawn = false;
            if (rendered_key_ != key_ || rendered_as_of_ != s.as_of) {
                if (render_)
                    render_(s.payload, s.as_of, s.from_cache);
                rendered_key_ = key_;
                rendered_as_of_ = s.as_of;
                drawn = true;
            }
            const bool newly = stack_->currentWidget() != content_;
            show_page(content_, State::Content);
            if (drawn || newly)
                emit result_shown(s.as_of, s.from_cache);
            break;
        }
        case autoload::FeedPage::NeedsSetup:
            if (req.pending) {
                show_checking_page(req); // no subscription until it settles
                break;
            }
            apply_setup_page(req);
            show_page(setup_, State::NeedsSetup);
            break;
        case autoload::FeedPage::Waiting:
            show_loading_page(false);
            skeleton_->set_message(loading_text_.isEmpty() ? tr("Waiting for data…") : loading_text_);
            break;
        case autoload::FeedPage::Error: {
            EmptyState::Spec spec = EmptyState::error_spec(s.feed_message, s.feed_title, QString(), s.feed_details);
            if (!feed_subscribe_)
                spec.primary.clear(); // nothing to call: no RETRY that does nothing
            error_reruns_ = static_cast<bool>(feed_subscribe_); // RETRY = subscribe again
            spec.primary_agent_risk = error_reruns_ ? applied_risk_ : QString();
            error_->apply(spec);
            show_page(error_, State::Failed);
            break;
        }
        case autoload::FeedPage::Idle:
            feed_show_idle_page(s);
            break;
    }
    update_notice();
    update_status();
    feed_arm_stale_timer(true);
    feed_emit_stale();
    enforce_memory_budget();
    // Last: the host's subscribe may report at once (a cached value), which presents again.
    feed_set_subscribed(d.subscribe && isVisible());
    feed_retry_next_ = false; // a retry that did not subscribe (requirements) is spent
}

void AutoLoadPane::feed_show_idle_page(const Slot& s) {
    if (!idle_) {
        idle_ = new EmptyState(stack_); // built in its final parent, on first use
        stack_->addWidget(idle_);
    }
    EmptyState::Spec spec = EmptyState::no_data_spec(s.feed_title, s.feed_message);
    if (!s.feed_glyph.isEmpty())
        spec.glyph = s.feed_glyph;
    idle_->apply(spec);
    show_page(idle_, State::Idle);
}

void AutoLoadPane::feed_set_subscribed(bool subscribed) {
    if (feed_subscribed_ == subscribed)
        return;
    feed_subscribed_ = subscribed; // first: the call may report back synchronously
    if (subscribed) {
        const bool retry = std::exchange(feed_retry_next_, false);
        const SubscribeFn fn = feed_subscribe_; // a copy: it may replace itself
        if (fn)
            fn(retry);
        return;
    }
    const ActionFn fn = feed_unsubscribe_; // a copy: it may replace itself
    if (fn)
        fn();
}

void AutoLoadPane::feed_retry() {
    // RETRY / RECONNECT / run() / invalidate(): the next subscribe is a retry
    // (only while shown — a later show is a first show again).
    feed_retry_next_ = isVisible();
    Slot& s = slot(key_);
    s.feed_report = autoload::FeedReport::Waiting;
    s.feed_quiet = false;
    s.feed_title.clear();
    s.feed_message.clear();
    s.feed_details.clear();
    s.feed_glyph.clear();
    feed_set_subscribed(false); // the host drops the old subscription …
    if (isVisible())
        present_feed(); // … and is asked again (requirements permitting)
    else
        update_status();
}

void AutoLoadPane::feed_after_change() {
    update_notice();
    update_status();
    feed_arm_stale_timer(false);
    feed_emit_stale();
}

void AutoLoadPane::feed_arm_stale_timer(bool restart) {
    qint64 ms = -1;
    const Slot* s = find_slot(key_);
    if (feed_mode_ && isVisible() && setup_status_.ok && s && s->has_result && alp_feed_may_go_stale(s->feed_report)) {
        const QDateTime last = s->feed_received.isValid() ? s->feed_received : s->as_of;
        ms = autoload::feed_ms_until_stale(last, QDateTime::currentDateTimeUtc(), feed_stale_after_s_);
    }
    if (ms <= 0) {
        feed_stale_timer_->stop(); // never, or already stale (the marker is on)
        return;
    }
    // A newer update moves the deadline later: an armed timer fires early and re-arms for the rest.
    if (restart || !feed_stale_timer_->isActive())
        feed_stale_timer_->start(int(std::min<qint64>(ms, std::numeric_limits<int>::max())));
}

void AutoLoadPane::feed_emit_stale() {
    const bool stale = feed_stale();
    if (stale == feed_stale_shown_)
        return;
    feed_stale_shown_ = stale;
    emit feed_stale_changed(stale);
}

} // namespace fincept::ui
