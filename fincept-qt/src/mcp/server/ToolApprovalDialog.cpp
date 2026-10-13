// ToolApprovalDialog.cpp — see ToolApprovalDialog.h.

#include "mcp/server/ToolApprovalDialog.h"

#include "core/logging/Logger.h"
#include "ui/notifications/DesktopNotifier.h"
#include "ui/notifications/NotificationService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QJsonDocument>
#include <QLabel>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWaitCondition>

#include <algorithm>
#include <memory>

namespace fincept::mcp {

namespace tool_approval_detail {

constexpr const char* kTag = "ToolApproval";

// Countdown / arm-delay tick. 4 Hz is plenty for a seconds counter.
constexpr int kTickMs = 250;

// How long the worker keeps waiting past the deadline. The dialog's own
// countdown resolves at the deadline (and tears the dialog down); the slack only
// matters when the dialog could not tick — e.g. it was hidden with a minimised
// parent — in which case the worker gives up and tells the GUI to clean up.
constexpr int kWorkerSlackMs = 1500;

/// One outstanding approval. Shared between the waiting worker and the GUI queue.
struct Pending {
    QMutex mutex;
    QWaitCondition cond;
    bool done = false;
    ToolApprovalVerdict verdict = ToolApprovalVerdict::Denied;
    ToolApprovalRequest request;
    qint64 deadline_ms = 0;
};
using PendingPtr = std::shared_ptr<Pending>;

/// GUI-thread-only queue state.
struct QueueState {
    QList<PendingPtr> queue;
    PendingPtr current;
    QPointer<ToolApprovalDialog> dialog;
};

QueueState& queue_state() {
    static QueueState s;
    return s;
}

QMutex& grants_mutex() {
    static QMutex m;
    return m;
}

/// (client, tool) → expiry (ms since epoch). Caller holds grants_mutex().
QHash<QString, qint64>& grants_locked() {
    static QHash<QString, qint64> g;
    return g;
}

QString grant_key(const QString& client, const QString& tool) {
    return client + QChar(0x1f) + tool;
}

qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

void resolve(const PendingPtr& p, ToolApprovalVerdict verdict) {
    QMutexLocker lock(&p->mutex);
    if (!p->done) {
        p->done = true;
        p->verdict = verdict;
    }
    p->cond.wakeAll();
}

bool is_done(const PendingPtr& p) {
    QMutexLocker lock(&p->mutex);
    return p->done;
}

void add_grant(const QString& client, const QString& tool) {
    QMutexLocker lock(&grants_mutex());
    grants_locked().insert(grant_key(client, tool),
                           now_ms() + static_cast<qint64>(ToolApprovalDialog::kGrantMinutes) * 60 * 1000);
}

struct HostWindow {
    bool any_visible = false; ///< false → nothing on screen to ask in (headless)
    QWidget* parent = nullptr; ///< non-minimised window to stack above; nullptr → show unparented
};

bool is_host_candidate(QWidget* w) {
    if (!w || !w->isWindow() || !w->isVisible())
        return false;
    if (qobject_cast<ToolApprovalDialog*>(w))
        return false;
    const Qt::WindowType type = w->windowType();
    return type == Qt::Window || type == Qt::Dialog;
}

HostWindow find_host_window() {
    HostWindow out;
    QWidget* active = QApplication::activeWindow();
    if (is_host_candidate(active) && !active->isMinimized()) {
        out.any_visible = true;
        out.parent = active;
        return out;
    }
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* w : tops) {
        if (!is_host_candidate(w))
            continue;
        out.any_visible = true;
        // A dialog owned by a minimised window is hidden along with it, so only a
        // window the user can actually see may be the parent.
        if (!out.parent && !w->isMinimized())
            out.parent = w;
    }
    return out;
}

void show_next();

void on_verdict(const PendingPtr& p, ToolApprovalDialog* dlg, ToolApprovalVerdict verdict) {
    auto& st = queue_state();
    if (verdict == ToolApprovalVerdict::Approved && dlg->remember_for_session()) {
        add_grant(p->request.client, p->request.tool);
        LOG_INFO(kTag, QString("User allowed '%1' for client '%2' for %3 minutes")
                           .arg(p->request.tool, p->request.client)
                           .arg(ToolApprovalDialog::kGrantMinutes));
    }
    resolve(p, verdict);
    if (st.current == p) {
        st.current.reset();
        st.dialog = nullptr;
    }
    dlg->deleteLater();
    // Queued, not inline: we are inside the dialog's own signal emission. A fresh
    // grant may also cover requests queued behind this one (show_next approves them).
    QTimer::singleShot(0, qApp, []() { show_next(); });
}

void show_next() {
    auto& st = queue_state();
    if (st.dialog)
        return; // one dialog at a time
    while (!st.queue.isEmpty()) {
        PendingPtr p = st.queue.takeFirst();
        if (is_done(p))
            continue; // the worker already gave up on it
        if (ToolApprovalDialog::has_active_grant(p->request.client, p->request.tool)) {
            resolve(p, ToolApprovalVerdict::Approved);
            continue;
        }
        if (now_ms() >= p->deadline_ms) {
            resolve(p, ToolApprovalVerdict::TimedOut);
            continue;
        }
        const HostWindow host = find_host_window();
        if (!host.any_visible) {
            LOG_WARN(kTag, QString("No visible terminal window to ask about '%1' from '%2' — denied")
                               .arg(p->request.tool, p->request.client));
            resolve(p, ToolApprovalVerdict::Unavailable);
            continue;
        }

        auto* dlg = new ToolApprovalDialog(p->request, p->deadline_ms, host.parent);
        st.current = p;
        st.dialog = dlg;
        QObject::connect(dlg, &ToolApprovalDialog::verdict_reached, dlg,
                         [p, dlg](ToolApprovalVerdict verdict) { on_verdict(p, dlg, verdict); });
        // Destroyed without a verdict (its parent window closed, app shutdown):
        // nobody approved it, so release the waiting worker with a denial.
        QObject::connect(dlg, &QObject::destroyed, qApp, [p]() {
            auto& s = queue_state();
            resolve(p, ToolApprovalVerdict::Denied); // no-op once a verdict was given
            if (s.current == p) {
                s.current.reset();
                s.dialog = nullptr;
                QTimer::singleShot(0, qApp, []() { show_next(); });
            }
        });
        dlg->show();
        dlg->raise();
        QApplication::alert(host.parent ? host.parent : static_cast<QWidget*>(dlg));
        ui::DesktopNotifier::instance().notify(
            ToolApprovalDialog::tr("Approval needed — Fincept Terminal"),
            ToolApprovalDialog::tr("%1 wants to run \"%2\". Approve or deny it in Fincept Terminal.")
                .arg(p->request.client, p->request.tool),
            static_cast<int>(ui::ToastService::Severity::Warning));
        LOG_INFO(kTag, QString("Asking the user to approve '%1' requested by '%2'").arg(p->request.tool,
                                                                                       p->request.client));
        return;
    }
}

void enqueue(const PendingPtr& p) {
    queue_state().queue.append(p);
    show_next();
}

/// The worker stopped waiting (deadline + slack). Drop the request from the
/// queue, or close its dialog if it is the one on screen (a hidden dialog
/// cannot tick its own countdown).
void abandon(const PendingPtr& p) {
    auto& st = queue_state();
    st.queue.removeAll(p);
    if (st.current == p && st.dialog)
        st.dialog->force_verdict(ToolApprovalVerdict::TimedOut);
}

} // namespace tool_approval_detail

// ── Static API ───────────────────────────────────────────────────────────────

ToolApprovalVerdict ToolApprovalDialog::request_approval(const ToolApprovalRequest& request, int timeout_ms) {
    using namespace tool_approval_detail;
    QCoreApplication* app = QCoreApplication::instance();
    if (!app)
        return ToolApprovalVerdict::Unavailable;
    if (QThread::currentThread() == app->thread()) {
        // Blocking here would freeze the UI that has to show the question.
        LOG_WARN(kTag, "request_approval() called on the GUI thread — refusing instead of blocking it");
        return ToolApprovalVerdict::Unavailable;
    }
    if (has_active_grant(request.client, request.tool))
        return ToolApprovalVerdict::Approved;

    auto p = std::make_shared<Pending>();
    p->request = request;
    p->deadline_ms = now_ms() + std::max(1000, timeout_ms);
    QMetaObject::invokeMethod(app, [p]() { enqueue(p); }, Qt::QueuedConnection);

    bool gave_up = false;
    {
        QMutexLocker lock(&p->mutex);
        const qint64 hard_deadline = p->deadline_ms + kWorkerSlackMs;
        while (!p->done) {
            const qint64 remaining = hard_deadline - now_ms();
            if (remaining <= 0)
                break;
            p->cond.wait(&p->mutex, QDeadlineTimer(remaining));
        }
        if (!p->done) {
            p->done = true;
            p->verdict = ToolApprovalVerdict::TimedOut;
            gave_up = true;
        }
    }
    if (gave_up)
        QMetaObject::invokeMethod(app, [p]() { abandon(p); }, Qt::QueuedConnection);

    QMutexLocker lock(&p->mutex);
    return p->verdict;
}

bool ToolApprovalDialog::has_active_grant(const QString& client, const QString& tool) {
    using namespace tool_approval_detail;
    QMutexLocker lock(&grants_mutex());
    auto& g = grants_locked();
    const auto it = g.find(grant_key(client, tool));
    if (it == g.end())
        return false;
    if (it.value() <= now_ms()) {
        g.erase(it);
        return false;
    }
    return true;
}

void ToolApprovalDialog::clear_grants() {
    using namespace tool_approval_detail;
    QMutexLocker lock(&grants_mutex());
    grants_locked().clear();
}

void ToolApprovalDialog::deny_all_pending() {
    using namespace tool_approval_detail;
    auto& st = queue_state();
    const QList<PendingPtr> queued = st.queue;
    st.queue.clear();
    for (const auto& p : queued)
        resolve(p, ToolApprovalVerdict::Denied);
    if (st.current)
        resolve(st.current, ToolApprovalVerdict::Denied);
    if (st.dialog)
        st.dialog->force_verdict(ToolApprovalVerdict::Denied); // its handler resets the slot + deletes it
}

// ── Dialog ───────────────────────────────────────────────────────────────────

ToolApprovalDialog::ToolApprovalDialog(const ToolApprovalRequest& request, qint64 deadline_ms, QWidget* parent)
    : QDialog(parent), request_(request), deadline_ms_(deadline_ms) {
    setObjectName(QStringLiteral("mcpApprovalDialog"));
    setModal(false);
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    // Never steal keyboard focus from the app the user is typing in — the
    // request usually arrives while they are in a terminal running the agent.
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setMinimumWidth(540);

    // A blanket 15-minute grant is never offered for state-changing live-trading
    // tools: each real-money order / cancel / square-off is approved on its own.
    grant_allowed_ = !(request_.live_trading && request_.destructive);

    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(tool_approval_detail::kTickMs);
    connect(tick_timer_, &QTimer::timeout, this, &ToolApprovalDialog::on_tick);

    build_ui();
    apply_theme();
    retranslate_ui();
}

ToolApprovalDialog::~ToolApprovalDialog() = default;

bool ToolApprovalDialog::remember_for_session() const {
    return grant_allowed_ && remember_box_ && remember_box_->isChecked();
}

void ToolApprovalDialog::force_verdict(ToolApprovalVerdict verdict) {
    finish(verdict);
}

void ToolApprovalDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("mcpApprovalHeader"));
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);
    title_label_ = new QLabel(header);
    title_label_->setObjectName(QStringLiteral("mcpApprovalTitle"));
    hl->addWidget(title_label_);
    hl->addStretch(1);
    status_label_ = new QLabel(header);
    status_label_->setObjectName(request_.live_trading && request_.destructive
                                     ? QStringLiteral("mcpApprovalStatusDanger")
                                     : QStringLiteral("mcpApprovalStatus"));
    hl->addWidget(status_label_);
    root->addWidget(header);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("mcpApprovalBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(18, 14, 18, 14);
    bl->setSpacing(10);

    lede_label_ = new QLabel(body);
    lede_label_->setObjectName(QStringLiteral("mcpApprovalLede"));
    lede_label_->setWordWrap(true);
    bl->addWidget(lede_label_);

    auto* rows = new QFrame(body);
    rows->setObjectName(QStringLiteral("mcpApprovalRows"));
    auto* grid = new QVBoxLayout(rows);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setSpacing(6);

    auto add_row = [rows, grid](QLabel*& caption, QLabel*& value, const QString& value_text,
                                const QString& value_object_name) {
        auto* line = new QHBoxLayout;
        line->setSpacing(10);
        caption = new QLabel(rows);
        caption->setObjectName(QStringLiteral("mcpApprovalCaption"));
        caption->setFixedWidth(110);
        caption->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        value = new QLabel(value_text, rows);
        value->setObjectName(value_object_name);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setTextFormat(Qt::PlainText);
        line->addWidget(caption);
        line->addWidget(value, 1);
        grid->addLayout(line);
    };
    add_row(client_caption_, client_value_, request_.client.isEmpty() ? QStringLiteral("unknown") : request_.client,
            QStringLiteral("mcpApprovalValue"));
    add_row(tool_caption_, tool_value_, request_.tool, QStringLiteral("mcpApprovalToolValue"));
    add_row(desc_caption_, desc_value_, request_.description, QStringLiteral("mcpApprovalValue"));
    bl->addWidget(rows);

    args_caption_ = new QLabel(body);
    args_caption_->setObjectName(QStringLiteral("mcpApprovalCaption"));
    bl->addWidget(args_caption_);

    args_view_ = new QPlainTextEdit(body);
    args_view_->setObjectName(QStringLiteral("mcpApprovalArgs"));
    args_view_->setReadOnly(true);
    args_view_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    args_view_->setMaximumHeight(180);
    args_view_->setPlainText(argument_preview(request_.args));
    bl->addWidget(args_view_);

    remember_box_ = new QCheckBox(body);
    remember_box_->setObjectName(QStringLiteral("mcpApprovalRemember"));
    remember_box_->setEnabled(grant_allowed_);
    bl->addWidget(remember_box_);
    root->addWidget(body, 1);

    // Footer
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("mcpApprovalFooter"));
    footer->setFixedHeight(50);
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(14, 8, 14, 8);
    fl->setSpacing(8);
    countdown_label_ = new QLabel(footer);
    countdown_label_->setObjectName(QStringLiteral("mcpApprovalCountdown"));
    fl->addWidget(countdown_label_);
    fl->addStretch(1);

    deny_button_ = new QPushButton(footer);
    deny_button_->setObjectName(QStringLiteral("mcpApprovalDeny"));
    deny_button_->setFixedHeight(30);
    deny_button_->setCursor(Qt::PointingHandCursor);
    // DENY is the default button: a stray Enter can only ever refuse.
    deny_button_->setAutoDefault(true);
    deny_button_->setDefault(true);

    approve_button_ = new QPushButton(footer);
    approve_button_->setObjectName(request_.live_trading && request_.destructive
                                       ? QStringLiteral("mcpApprovalApproveDanger")
                                       : QStringLiteral("mcpApprovalApprove"));
    approve_button_->setFixedHeight(30);
    approve_button_->setCursor(Qt::PointingHandCursor);
    approve_button_->setAutoDefault(false);
    approve_button_->setDefault(false);
    approve_button_->setEnabled(false); // armed kArmDelayMs after show

    fl->addWidget(deny_button_);
    fl->addWidget(approve_button_);
    root->addWidget(footer);

    connect(deny_button_, &QPushButton::clicked, this, [this]() { finish(ToolApprovalVerdict::Denied); });
    connect(approve_button_, &QPushButton::clicked, this, [this]() { finish(ToolApprovalVerdict::Approved); });
}

void ToolApprovalDialog::apply_theme() {
    using namespace ui::colors;
    const QString font = QStringLiteral("'%1','Consolas','Cascadia Mono','JetBrains Mono',monospace")
                             .arg(QString::fromLatin1(ui::fonts::DATA_FAMILY.get()));

    const QString ss =
        QStringLiteral(
            "QDialog#mcpApprovalDialog { background:%1; }"
            "QWidget#mcpApprovalHeader { background:%2; border-bottom:1px solid %3; }"
            "QLabel#mcpApprovalTitle { color:%4; font-family:%5; font-size:12px; font-weight:700;"
            "  letter-spacing:1.4px; background:transparent; }"
            "QLabel#mcpApprovalStatus { color:%4; font-family:%5; font-size:10px; font-weight:700;"
            "  letter-spacing:1.2px; background:transparent; }"
            "QLabel#mcpApprovalStatusDanger { color:%6; font-family:%5; font-size:10px; font-weight:700;"
            "  letter-spacing:1.2px; background:transparent; }"
            "QWidget#mcpApprovalBody { background:%1; }"
            "QLabel#mcpApprovalLede { color:%7; font-family:%5; font-size:12px; background:transparent; }"
            "QFrame#mcpApprovalRows { background:%8; border:1px solid %3; }"
            "QLabel#mcpApprovalCaption { color:%9; font-family:%5; font-size:10px; font-weight:700;"
            "  letter-spacing:1.2px; background:transparent; }"
            "QLabel#mcpApprovalValue { color:%10; font-family:%5; font-size:12px; background:transparent; }"
            "QLabel#mcpApprovalToolValue { color:%4; font-family:%5; font-size:12px; font-weight:700;"
            "  background:transparent; }"
            "QPlainTextEdit#mcpApprovalArgs { background:%2; color:%10; border:1px solid %3; font-family:%5;"
            "  font-size:11px; }"
            "QCheckBox#mcpApprovalRemember { color:%7; font-family:%5; font-size:11px; background:transparent; }"
            "QCheckBox#mcpApprovalRemember:disabled { color:%11; }"
            "QLabel#mcpApprovalCountdown { color:%9; font-family:%5; font-size:10px; font-weight:700;"
            "  letter-spacing:1.2px; background:transparent; }"
            "QWidget#mcpApprovalFooter { background:%2; border-top:1px solid %3; }"
            "QPushButton#mcpApprovalDeny { background:%8; color:%10; border:1px solid %12; font-family:%5;"
            "  font-size:11px; font-weight:700; letter-spacing:1px; padding:0 16px; }"
            "QPushButton#mcpApprovalDeny:hover { border-color:%13; }"
            "QPushButton#mcpApprovalApprove { background:%4; color:%14; border:1px solid %4; font-family:%5;"
            "  font-size:11px; font-weight:700; letter-spacing:1.2px; padding:0 16px; }"
            "QPushButton#mcpApprovalApprove:hover { background:%15; border-color:%15; }"
            "QPushButton#mcpApprovalApproveDanger { background:%6; color:%10; border:1px solid %6; font-family:%5;"
            "  font-size:11px; font-weight:700; letter-spacing:1.2px; padding:0 16px; }"
            "QPushButton#mcpApprovalApprove:disabled, QPushButton#mcpApprovalApproveDanger:disabled {"
            "  background:%8; color:%11; border-color:%3; }")
            .arg(BG_BASE(),        // %1
                 BG_SURFACE(),     // %2
                 BORDER_DIM(),     // %3
                 AMBER(),          // %4
                 font,             // %5
                 NEGATIVE(),       // %6
                 TEXT_SECONDARY(), // %7
                 BG_RAISED(),      // %8
                 TEXT_TERTIARY())  // %9
            .arg(TEXT_PRIMARY(),   // %10
                 TEXT_DIM(),       // %11
                 BORDER_MED(),     // %12
                 BORDER_BRIGHT(),  // %13
                 TEXT_ON_ACCENT(), // %14
                 AMBER_DIM());     // %15
    setStyleSheet(ss);
}

QString ToolApprovalDialog::status_text() const {
    if (request_.live_trading && request_.destructive)
        return tr("LIVE TRADING · REAL MONEY");
    if (request_.live_trading)
        return tr("LIVE BROKER ACCOUNT");
    if (request_.destructive)
        return tr("CHANGES TERMINAL STATE");
    return tr("CONFIRMATION REQUIRED");
}

void ToolApprovalDialog::retranslate_ui() {
    setWindowTitle(tr("Approve external agent request"));
    title_label_->setText(tr("EXTERNAL AGENT REQUEST"));
    status_label_->setText(status_text());
    lede_label_->setText(tr("An AI agent connected to Fincept Terminal over MCP wants to run a tool that needs "
                            "your approval. Nothing runs unless you approve it."));
    client_caption_->setText(tr("CLIENT"));
    tool_caption_->setText(tr("TOOL"));
    desc_caption_->setText(tr("WHAT IT DOES"));
    args_caption_->setText(tr("ARGUMENTS"));
    remember_box_->setText(tr("Allow this tool for this client for %1 minutes").arg(kGrantMinutes));
    remember_box_->setToolTip(grant_allowed_
                                  ? tr("Session only. Further calls of this tool from this client run without asking "
                                       "until the time is up or the terminal restarts.")
                                  : tr("Not available for live-trading actions: every real-money action is approved "
                                       "individually."));
    deny_button_->setText(tr("DENY"));
    approve_button_->setText(tr("APPROVE"));
    update_countdown();
}

QString ToolApprovalDialog::argument_preview(const QJsonObject& args) {
    if (args.isEmpty())
        return QStringLiteral("{}");
    const QString full = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Indented)).trimmed();
    if (full.size() <= kArgPreviewMaxChars)
        return full;
    return full.left(kArgPreviewMaxChars) + QStringLiteral("\n…\n") +
           tr("[preview truncated — %1 more characters not shown]").arg(full.size() - kArgPreviewMaxChars);
}

void ToolApprovalDialog::update_countdown() {
    if (!countdown_label_)
        return;
    const qint64 remaining_ms = std::max<qint64>(0, deadline_ms_ - tool_approval_detail::now_ms());
    const qint64 secs = (remaining_ms + 999) / 1000;
    countdown_label_->setText(tr("AUTO-DENY IN %1s").arg(secs));
}

void ToolApprovalDialog::on_tick() {
    const qint64 now = tool_approval_detail::now_ms();
    if (now >= deadline_ms_) {
        finish(ToolApprovalVerdict::TimedOut);
        return;
    }
    if (!approve_button_->isEnabled() && now - shown_at_ms_ >= kArmDelayMs)
        approve_button_->setEnabled(true);
    update_countdown();
}

void ToolApprovalDialog::finish(ToolApprovalVerdict verdict) {
    if (finished_)
        return;
    finished_ = true;
    tick_timer_->stop();
    emit verdict_reached(verdict);
    QDialog::done(verdict == ToolApprovalVerdict::Approved ? QDialog::Accepted : QDialog::Rejected);
}

void ToolApprovalDialog::reject() {
    finish(ToolApprovalVerdict::Denied);
}

void ToolApprovalDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (finished_)
        return;
    if (shown_at_ms_ == 0) {
        shown_at_ms_ = tool_approval_detail::now_ms();
        approve_button_->setEnabled(false);
    }
    update_countdown();
    tick_timer_->start();
}

void ToolApprovalDialog::hideEvent(QHideEvent* event) {
    tick_timer_->stop();
    QDialog::hideEvent(event);
}

void ToolApprovalDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate_ui();
    QDialog::changeEvent(event);
}

} // namespace fincept::mcp
