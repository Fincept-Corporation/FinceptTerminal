#include "ui/widgets/UpdateDialog.h"

#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "services/updater/UpdateService.h"
#include "ui/notifications/NotificationService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLocale>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::ui {

namespace {

using fincept::services::UpdateService;
using fincept::services::updater::UpdateStage;

constexpr const char* kUdlgTag = "UpdateDialog";
constexpr const char* kUdlgActionProp = "updAction";
constexpr const char* kUdlgToneProp = "tone";

constexpr int kUdlgMinWidth = 520;       // at the default font size; grows with the user's font
constexpr int kUdlgPreferredWidth = 560; // first-show width, same scaling
constexpr int kUdlgPendingRetryMs = 3000; // waiting for a terminal window to show a prompt over
constexpr int kUdlgLiveRefreshMs = 200;   // live text / progress: at most five updates a second
constexpr int kUdlgTickMs = 1000;         // download figures refresh even while no data arrives
constexpr qint64 kUdlgSampleSpacingMs = 100;
constexpr qint64 kUdlgSpeedWindowMs = 5000; // moving average of the download speed
constexpr qint64 kUdlgMinSpeedSpanMs = 1000;
constexpr int kUdlgProgressScale = 1000;
constexpr int kUdlgNotesMaxLines = 12;
constexpr int kUdlgNotesMaxChars = 900;

// ── Process-wide state (behaviour only — nothing appearance-derived) ─────────

/// The single update window (same pattern as UpgradeDialog / OnboardingTour).
QPointer<UpdateDialog>& udlg_instance() {
    static QPointer<UpdateDialog> dlg;
    return dlg;
}

/// The newer version the window has already offered in this run. "Later"
/// holds until the next start; the About screen can still reopen the window.
QString& udlg_prompted_version() {
    static QString version;
    return version;
}

/// Set once the application is on its way out (update restart or a normal
/// quit): from then on nothing opens or reopens the window.
bool& udlg_shutting_down() {
    static bool shutting_down = false;
    return shutting_down;
}

struct UdlgPending {
    bool prompt = false;     ///< a background check found a release the user has not been shown
    bool has_result = false; ///< the previous run's update result is still to be shown
    bool result_ok = false;
    QString result_version;
};

UdlgPending& udlg_pending() {
    static UdlgPending pending;
    return pending;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

QString udlg_app_version() {
    const QString v = QCoreApplication::applicationVersion();
#ifdef FINCEPT_VERSION_STRING
    return v.isEmpty() ? QStringLiteral(FINCEPT_VERSION_STRING) : v;
#else
    return v;
#endif
}

/// A platform plugin with no window system (headless CI, offscreen runs).
bool udlg_headless_platform() {
    const QString p = QGuiApplication::platformName();
    return p.isEmpty() || p == QLatin1String("offscreen") || p == QLatin1String("minimal");
}

/// Downloading → Restarting: the update is under way.
bool udlg_update_running(UpdateStage stage) {
    return stage == UpdateStage::Downloading || stage == UpdateStage::Verifying ||
           stage == UpdateStage::Installing || stage == UpdateStage::Restarting;
}

/// The terminal window `w` belongs to (a floating dock panel counts for its
/// frame); `w`'s own window when it belongs to none.
QWidget* udlg_frame_of(QWidget* w) {
    for (QWidget* p = w; p; p = p->parentWidget()) {
        if (auto* frame = qobject_cast<WindowFrame*>(p))
            return frame;
    }
    return w ? w->window() : nullptr;
}

/// The terminal window the user is working in, or null when the application
/// is not active / the active window is not part of a terminal window.
WindowFrame* udlg_active_frame() {
    for (QWidget* w = QApplication::activeWindow(); w; w = w->parentWidget()) {
        if (auto* frame = qobject_cast<WindowFrame*>(w))
            return frame;
    }
    return nullptr;
}

/// Any terminal window to show the window over: the active one, else the
/// first visible one, else the first one.
QWidget* udlg_any_host() {
    if (WindowFrame* frame = udlg_active_frame())
        return frame;
    const QList<WindowFrame*> frames = WindowRegistry::instance().frames();
    for (WindowFrame* frame : frames) {
        if (frame && frame->isVisible() && !frame->isMinimized())
            return frame;
    }
    return frames.isEmpty() ? nullptr : frames.first();
}

/// A window for an unrequested prompt: the active, visible, not minimised
/// terminal window, with no other dialog open over the application.
QWidget* udlg_prompt_host() {
    if (QApplication::activeModalWidget())
        return nullptr;
    WindowFrame* frame = udlg_active_frame();
    if (!frame || !frame->isVisible() || frame->isMinimized())
        return nullptr;
    return frame;
}

/// A background check found a release this run has not offered yet.
bool udlg_prompt_wanted() {
    const auto& svc = UpdateService::instance();
    return svc.stage() == UpdateStage::Available && svc.update_available() &&
           svc.latest_version() != udlg_prompted_version();
}

QString udlg_download_url() {
    const auto& info = UpdateService::instance().info();
    return info.release_url.isEmpty() ? UpdateService::releases_page_url() : info.release_url;
}

void udlg_open_url(const QString& url) {
    if (url.isEmpty())
        return;
    if (!QDesktopServices::openUrl(QUrl(url)))
        LOG_WARN(kUdlgTag, QString("Could not open %1 in the browser").arg(url));
}

/// The release's "what's new" text, cut to a few lines — the full notes are
/// one click away (Release notes ↗).
QString udlg_short_notes(const QString& notes) {
    QString text = notes.trimmed();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (text.isEmpty())
        return {};
    QStringList lines = text.split(QLatin1Char('\n'));
    bool cut = false;
    if (lines.size() > kUdlgNotesMaxLines) {
        lines = lines.mid(0, kUdlgNotesMaxLines);
        cut = true;
    }
    text = lines.join(QLatin1Char('\n'));
    if (text.size() > kUdlgNotesMaxChars) {
        text = text.left(kUdlgNotesMaxChars);
        cut = true;
    }
    if (cut)
        text = text.trimmed() + QStringLiteral("\n…");
    return text;
}

QString udlg_time_left(double seconds) {
    qint64 s = static_cast<qint64>(std::ceil(seconds));
    if (s < 1)
        s = 1;
    if (s < 60) {
        if (s > 10)
            s = (s + 4) / 5 * 5; // 5 s steps: a jittery countdown reads as noise
        return UpdateDialog::tr("about %n s left", nullptr, static_cast<int>(s));
    }
    const qint64 minutes = (s + 59) / 60;
    if (minutes < 120)
        return UpdateDialog::tr("about %n min left", nullptr, static_cast<int>(minutes));
    return UpdateDialog::tr("about %n h left", nullptr, static_cast<int>((minutes + 59) / 60));
}

/// Sets the stylesheet "tone" of `w` and re-evaluates its property selectors.
void udlg_set_tone(QWidget* w, const char* tone) {
    if (!w)
        return;
    const QString value = QString::fromLatin1(tone);
    if (w->property(kUdlgToneProp).toString() == value)
        return;
    w->setProperty(kUdlgToneProp, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ── ElidedLine ───────────────────────────────────────────────────────────────

/// One line of text that elides ("…") to the width it is given and keeps the
/// height of one line whatever it shows — so a status that changes several
/// times a second (installer output) never makes the window jump.
class UpdateDialog::ElidedLine final : public QLabel {
  public:
    explicit ElidedLine(QWidget* parent) : QLabel(parent) {
        setTextFormat(Qt::PlainText);
        setWordWrap(false);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    void set_full_text(const QString& text) {
        const QString line = text.simplified();
        if (line == full_ && !QLabel::text().isEmpty())
            return;
        full_ = line;
        refit();
    }

    QSize sizeHint() const override { return {0, line_height()}; }
    QSize minimumSizeHint() const override { return {0, line_height()}; }

  protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        refit();
    }
    void changeEvent(QEvent* event) override {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
            updateGeometry();
            refit();
        }
    }

  private:
    int line_height() const {
        const QMargins m = contentsMargins();
        return fontMetrics().height() + m.top() + m.bottom();
    }
    void refit() {
        // A no-break space keeps the line's height while there is nothing to say.
        const QString source = full_.isEmpty() ? QString(QChar(0x00A0)) : full_;
        const QString shown = fontMetrics().elidedText(source, Qt::ElideRight, std::max(0, contentsRect().width()));
        if (shown != QLabel::text())
            setText(shown);
        const QString tip = shown == source ? QString() : full_;
        if (tip != toolTip())
            setToolTip(tip);
    }

    QString full_;
};

// ── Construction ─────────────────────────────────────────────────────────────

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("updateDialog"));
    // Explicit hints so the close button can be withdrawn while the update is
    // verified / installed / restarting (apply_closable).
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                   Qt::WindowCloseButtonHint);
    setSizeGripEnabled(false);
    clock_.start();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(10);

    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("updTitle"));
    title_->setTextFormat(Qt::PlainText);
    title_->setWordWrap(true);
    root->addWidget(title_);

    body_ = new QLabel(this);
    body_->setObjectName(QStringLiteral("updBody"));
    body_->setTextFormat(Qt::PlainText);
    body_->setWordWrap(true);
    root->addWidget(body_);

    notes_ = new QLabel(this);
    notes_->setObjectName(QStringLiteral("updNotes"));
    notes_->setTextFormat(Qt::PlainText);
    notes_->setWordWrap(true);
    notes_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(notes_);

    notes_link_ = new QPushButton(this);
    notes_link_->setObjectName(QStringLiteral("updLink"));
    notes_link_->setCursor(Qt::PointingHandCursor);
    notes_link_->setAutoDefault(false);
    connect(notes_link_, &QPushButton::clicked, this,
            []() { udlg_open_url(UpdateService::instance().info().release_url); });
    root->addWidget(notes_link_, 0, Qt::AlignLeft);

    note_ = new QLabel(this);
    note_->setObjectName(QStringLiteral("updNote"));
    note_->setTextFormat(Qt::PlainText);
    note_->setWordWrap(true);
    root->addWidget(note_);

    progress_ = new QProgressBar(this);
    progress_->setObjectName(QStringLiteral("updProgress"));
    progress_->setTextVisible(false);
    progress_->setRange(0, 0);
    root->addWidget(progress_);

    status_ = new ElidedLine(this);
    status_->setObjectName(QStringLiteral("updStatus"));
    root->addWidget(status_);

    figures_ = new ElidedLine(this);
    figures_->setObjectName(QStringLiteral("updFigures"));
    root->addWidget(figures_);

    button_bar_ = new QWidget(this);
    auto* buttons = new QHBoxLayout(button_bar_);
    buttons->setContentsMargins(0, 8, 0, 0);
    buttons->setSpacing(8);
    tertiary_btn_ = new QPushButton(button_bar_);
    tertiary_btn_->setObjectName(QStringLiteral("updGhost"));
    buttons->addWidget(tertiary_btn_);
    buttons->addStretch();
    secondary_btn_ = new QPushButton(button_bar_);
    secondary_btn_->setObjectName(QStringLiteral("updGhost"));
    buttons->addWidget(secondary_btn_);
    primary_btn_ = new QPushButton(button_bar_);
    primary_btn_->setObjectName(QStringLiteral("updPrimary"));
    buttons->addWidget(primary_btn_);
    for (QPushButton* button : {tertiary_btn_, secondary_btn_, primary_btn_}) {
        button->setCursor(Qt::PointingHandCursor);
        // Only the primary button is ever the default (set per stage): Enter
        // must not cancel a download because Cancel happened to have focus.
        button->setAutoDefault(false);
        connect(button, &QPushButton::clicked, this, [this, button]() {
            run_action(static_cast<Action>(button->property(kUdlgActionProp).toInt()));
        });
    }
    root->addWidget(button_bar_);

    live_timer_ = new QTimer(this);
    live_timer_->setSingleShot(true);
    live_timer_->setInterval(kUdlgLiveRefreshMs);
    connect(live_timer_, &QTimer::timeout, this, &UpdateDialog::render_live);
    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(kUdlgTickMs);
    connect(tick_timer_, &QTimer::timeout, this, &UpdateDialog::render_live);

    auto& svc = UpdateService::instance();
    connect(&svc, &UpdateService::stage_changed, this, [this](UpdateStage) { on_stage_changed(); });
    connect(&svc, &UpdateService::progress_changed, this, &UpdateDialog::on_progress);
    connect(&svc, &UpdateService::status_text_changed, this, [this](const QString&) { schedule_live(); });
    connect(&svc, &UpdateService::handed_off, this,
            [this](const QString& message) { set_notice(Notice::HandedOff, message); });

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
    resize(metrics::width(kUdlgPreferredWidth), height());
}

UpdateDialog::~UpdateDialog() {
    // Destroyed with its terminal window (that window was closed from another
    // one) rather than closed by the user: if an update is still running, it
    // must not continue unseen.
    if (!closed_by_user_ && !udlg_shutting_down() && !QCoreApplication::closingDown())
        QTimer::singleShot(0, qApp, &UpdateDialog::ensure_visible);
}

// ── Static entry points ──────────────────────────────────────────────────────

UpdateDialog* UpdateDialog::current() {
    return udlg_instance().data();
}

UpdateDialog* UpdateDialog::present(QWidget* parent) {
    return open_dialog(parent, Notice::None, {});
}

void UpdateDialog::check_now(QWidget* parent) {
    UpdateDialog* dlg = present(parent);
    if (dlg && dlg->notice_ != Notice::None)
        dlg->set_notice(Notice::None, {});
    UpdateService::instance().check_for_updates(/*user_initiated=*/true);
}

UpdateDialog* UpdateDialog::open_dialog(QWidget* parent, Notice notice, const QString& notice_arg) {
    if (UpdateDialog* open = current()) {
        if (notice != Notice::None)
            open->set_notice(notice, notice_arg);
        open->raise();
        open->activateWindow();
        return open;
    }
    QWidget* host = parent ? udlg_frame_of(parent) : udlg_any_host();
    auto* dlg = new UpdateDialog(host);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    udlg_instance() = dlg;
    dlg->notice_ = notice;
    dlg->notice_arg_ = notice_arg;
    dlg->render();
    // open() rather than exec(): callers run inside signal handlers and timer
    // callbacks, where exec() would spin a nested event loop.
    dlg->open();
    return dlg;
}

void UpdateDialog::watch_service() {
    static bool watching = false;
    if (std::exchange(watching, true))
        return;

    auto& svc = UpdateService::instance();
    // A check found a newer release. A window that is open already shows the
    // stage — unless it is showing a notice, after which the offer follows.
    QObject::connect(&svc, &UpdateService::check_finished, qApp, [](bool update_found) {
        if (!update_found)
            return;
        if (UpdateDialog* open = current(); open && open->notice_ == Notice::None)
            return;
        udlg_pending().prompt = true;
        QTimer::singleShot(0, qApp, &UpdateDialog::process_pending);
    });
    // An update in progress always has a window.
    QObject::connect(&svc, &UpdateService::stage_changed, qApp,
                     [](UpdateStage) { QTimer::singleShot(0, qApp, &UpdateDialog::ensure_visible); });
    QObject::connect(&svc, &UpdateService::handed_off, qApp, [](const QString& message) {
        if (current() || udlg_shutting_down())
            return; // the open window shows it
        open_dialog(udlg_any_host(), Notice::HandedOff, message);
    });
    QObject::connect(&svc, &UpdateService::restart_requested, qApp, []() { udlg_shutting_down() = true; });
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, []() { udlg_shutting_down() = true; });
}

void UpdateDialog::show_previous_update_result(const QString& to_version, bool succeeded) {
    const QString version = to_version.isEmpty() ? udlg_app_version() : to_version;
    if (succeeded) {
        LOG_INFO(kUdlgTag, QString("The previous run updated Fincept Terminal to %1").arg(version));
        // The toast tray only exists where the system has a tray (DesktopNotifier).
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            ToastService::instance().post(ToastService::Severity::Success,
                                          tr("Fincept Terminal was updated to version %1.").arg(version),
                                          QStringLiteral("updater"));
            return;
        }
    } else {
        LOG_WARN(kUdlgTag, QString("The update to %1 started by the previous run did not complete").arg(version));
    }
    UdlgPending& pending = udlg_pending();
    pending.has_result = true;
    pending.result_ok = succeeded;
    pending.result_version = version;
    process_pending();
}

void UpdateDialog::process_pending() {
    static QPointer<QTimer> retry;
    UdlgPending& pending = udlg_pending();
    if (udlg_shutting_down() || udlg_headless_platform())
        pending = UdlgPending{};
    if (pending.prompt && !udlg_prompt_wanted())
        pending.prompt = false;

    const auto keep_waiting = []() {
        if (!retry) {
            retry = new QTimer(qApp);
            retry->setInterval(kUdlgPendingRetryMs);
            QObject::connect(retry.data(), &QTimer::timeout, retry.data(), &UpdateDialog::process_pending);
        }
        if (!retry->isActive())
            retry->start();
    };

    if (!pending.prompt && !pending.has_result) {
        if (retry)
            retry->stop();
        return;
    }
    // The window is up: whatever else is pending follows once it is closed.
    // Otherwise wait for a terminal window the user is looking at, with no
    // other dialog over it.
    QWidget* host = current() ? nullptr : udlg_prompt_host();
    if (!host) {
        keep_waiting();
        return;
    }
    if (pending.has_result) {
        pending.has_result = false;
        open_dialog(host, pending.result_ok ? Notice::Updated : Notice::PreviousFailed, pending.result_version);
    } else {
        pending.prompt = false;
        LOG_INFO(kUdlgTag,
                 QString("Offering the update to %1").arg(UpdateService::instance().latest_version()));
        open_dialog(host, Notice::None, {});
    }
    if (pending.prompt || pending.has_result)
        keep_waiting();
    else if (retry)
        retry->stop();
}

void UpdateDialog::ensure_visible() {
    if (udlg_shutting_down() || QCoreApplication::closingDown() || current())
        return;
    const auto& svc = UpdateService::instance();
    const UpdateStage stage = svc.stage();
    if (!udlg_update_running(stage) && !(stage == UpdateStage::Failed && svc.failed_during_update()))
        return;
    LOG_INFO(kUdlgTag, "An update is in progress and no window shows it — opening the update window");
    // With no terminal window left (all closed to the Launchpad) it opens on its own.
    open_dialog(udlg_any_host(), Notice::None, {});
}

// ── State ────────────────────────────────────────────────────────────────────

void UpdateDialog::set_notice(Notice notice, const QString& arg) {
    notice_ = notice;
    notice_arg_ = arg;
    render();
}

void UpdateDialog::on_stage_changed() {
    const UpdateStage stage = UpdateService::instance().stage();
    // A running update or a failure replaces any notice.
    if (udlg_update_running(stage) || stage == UpdateStage::Failed) {
        notice_ = Notice::None;
        notice_arg_.clear();
    }
    if (stage == UpdateStage::Downloading && rendered_stage_ != UpdateStage::Downloading)
        reset_progress();
    render();
}

void UpdateDialog::reset_progress() {
    have_progress_ = false;
    received_ = 0;
    total_ = 0;
    samples_.clear();
    clock_.restart();
}

void UpdateDialog::on_progress(qint64 received, qint64 total) {
    if (received < received_)
        samples_.clear(); // the download started over
    have_progress_ = true;
    received_ = std::max<qint64>(0, received);
    total_ = std::max<qint64>(0, total);
    const qint64 now = clock_.elapsed();
    if (samples_.isEmpty() || now - samples_.constLast().t_ms >= kUdlgSampleSpacingMs)
        samples_.append(Sample{now, received_});
    schedule_live();
}

void UpdateDialog::schedule_live() {
    if (!live_timer_->isActive())
        live_timer_->start();
}

// ── Rendering ────────────────────────────────────────────────────────────────

void UpdateDialog::render() {
    const auto& svc = UpdateService::instance();
    const UpdateStage stage = svc.stage();
    const QString installed = udlg_app_version();
    const QString latest = svc.latest_version();

    QString title;
    QString body;
    QString notes;
    QString note;
    const char* body_tone = "";
    const char* note_tone = "";
    bool body_selectable = false;
    bool show_link = false;
    bool show_progress = false;
    bool show_status = false;
    bool show_figures = false;
    Action tertiary = Action::None;
    Action secondary = Action::None;
    Action primary = Action::None;
    QString tertiary_text;
    QString secondary_text;
    QString primary_text;

    if (notice_ == Notice::HandedOff) {
        title = tr("Finish the update");
        body = notice_arg_;
        body_selectable = true;
        primary = Action::Close;
        primary_text = tr("Close");
    } else if (notice_ == Notice::PreviousFailed) {
        title = tr("The update did not complete");
        body = tr("The update to Fincept Terminal %1 did not complete.").arg(notice_arg_);
        note = tr("The version you had before (%1) is still installed and works as before.").arg(installed);
        secondary = Action::Close;
        secondary_text = tr("Close");
        primary = Action::RetryPrevious;
        primary_text = tr("Try again");
    } else if (notice_ == Notice::Updated) {
        title = tr("Fincept Terminal updated");
        body = tr("Fincept Terminal was updated to version %1.").arg(notice_arg_);
        primary = Action::Close;
        primary_text = tr("Close");
    } else {
        switch (stage) {
            case UpdateStage::Idle:
                title = tr("Software update");
                body = tr("You're running Fincept Terminal %1.").arg(installed);
                secondary = Action::Close;
                secondary_text = tr("Close");
                primary = Action::CheckNow;
                primary_text = tr("Check for updates");
                break;
            case UpdateStage::Checking:
                title = tr("Checking for updates…");
                body = tr("You're running Fincept Terminal %1.").arg(installed);
                show_progress = true;
                show_status = true;
                secondary = Action::Close;
                secondary_text = tr("Close");
                break;
            case UpdateStage::UpToDate:
                title = tr("You're up to date");
                body = tr("You're running the latest version (%1).").arg(installed);
                primary = Action::Close;
                primary_text = tr("Close");
                break;
            case UpdateStage::Available: {
                title = tr("Update available");
                body = tr("Fincept Terminal %1 is available — you have %2.").arg(latest, installed);
                notes = udlg_short_notes(svc.info().notes);
                show_link = !svc.info().release_url.isEmpty();
                secondary = Action::Close;
                secondary_text = tr("Later");
                if (svc.can_self_update() && svc.info().has_asset()) {
                    note = tr("The update installs over this installation. Your settings and data are kept, "
                              "and the terminal restarts by itself.");
                    primary = Action::UpdateNow;
                    primary_text = tr("Update now");
                } else {
                    note = svc.blocked_reason().trimmed();
                    if (note.isEmpty())
                        note = tr("This installation cannot be updated from inside the terminal. Download the "
                                  "new version from the release page.");
                    note_tone = "warn";
                    primary = Action::OpenDownloadPage;
                    primary_text = tr("Open download page");
                }
                break;
            }
            case UpdateStage::Downloading:
                title = tr("Downloading update");
                body = tr("Downloading Fincept Terminal %1. The terminal restarts by itself once the update is "
                          "installed.")
                           .arg(latest);
                show_progress = true;
                show_status = true;
                show_figures = true;
                secondary = Action::CancelDownload;
                secondary_text = tr("Cancel");
                break;
            case UpdateStage::Verifying:
                title = tr("Verifying update");
                body = tr("Making sure the download is complete and genuine.");
                show_progress = true;
                show_status = true;
                break;
            case UpdateStage::Installing:
                title = tr("Installing update");
                body = tr("Installing Fincept Terminal %1. Please wait — the terminal restarts by itself when it "
                          "is done.")
                           .arg(latest);
                show_progress = true;
                show_status = true;
                break;
            case UpdateStage::Restarting:
                title = tr("Restarting Fincept Terminal…");
                body = tr("The update is ready. Fincept Terminal closes and opens again as version %1.").arg(latest);
                show_progress = true;
                show_status = true;
                break;
            case UpdateStage::Failed: {
                const bool during_update = svc.failed_during_update();
                title = during_update ? tr("The update did not complete") : tr("Could not check for updates");
                body = svc.error_text().trimmed();
                if (body.isEmpty())
                    body = during_update ? tr("Something went wrong while updating.")
                                         : tr("Something went wrong while checking for updates.");
                body_tone = "error";
                body_selectable = true;
                note = during_update
                           ? tr("Fincept Terminal %1 is still installed and works as before.").arg(installed)
                           : tr("Fincept Terminal %1 keeps working as before.").arg(installed);
                tertiary = Action::OpenDownloadPage;
                tertiary_text = tr("Open download page");
                secondary = Action::Close;
                secondary_text = tr("Close");
                primary = Action::TryAgain;
                primary_text = tr("Try again");
                break;
            }
        }
    }

    setWindowTitle(tr("Fincept Terminal Update"));
    title_->setText(title);
    body_->setText(body);
    body_->setVisible(!body.isEmpty());
    body_->setTextInteractionFlags(body_selectable ? Qt::TextSelectableByMouse : Qt::NoTextInteraction);
    udlg_set_tone(body_, body_tone);
    notes_->setText(notes);
    notes_->setVisible(!notes.isEmpty());
    notes_link_->setText(tr("Release notes ↗"));
    notes_link_->setToolTip(svc.info().release_url);
    notes_link_->setVisible(show_link);
    note_->setText(note);
    note_->setVisible(!note.isEmpty());
    udlg_set_tone(note_, note_tone);

    progress_->setVisible(show_progress);
    if (show_progress) {
        if (stage == UpdateStage::Restarting) {
            progress_->setRange(0, 1);
            progress_->setValue(1);
        } else if (stage != UpdateStage::Downloading) {
            progress_->setRange(0, 0); // indeterminate; Downloading is set by render_live()
        }
    }
    status_->setVisible(show_status);
    figures_->setVisible(show_figures);

    set_button(tertiary_btn_, tertiary, tertiary_text);
    set_button(secondary_btn_, secondary, secondary_text);
    set_button(primary_btn_, primary, primary_text);
    primary_btn_->setDefault(primary != Action::None);
    button_bar_->setVisible(tertiary != Action::None || secondary != Action::None || primary != Action::None);

    if (notice_ == Notice::None && stage == UpdateStage::Downloading) {
        if (!tick_timer_->isActive())
            tick_timer_->start();
    } else {
        tick_timer_->stop();
    }
    render_live();
    apply_closable(closable());

    // The window offered this version: "Later" (or closing) holds for this run.
    if (notice_ == Notice::None && stage == UpdateStage::Available && !latest.isEmpty())
        udlg_prompted_version() = latest;

    const bool changed = !rendered_ || stage != rendered_stage_ || notice_ != rendered_notice_;
    rendered_ = true;
    rendered_stage_ = stage;
    rendered_notice_ = notice_;
    if (changed)
        fit_height();
}

void UpdateDialog::render_live() {
    if (notice_ != Notice::None)
        return;
    const auto& svc = UpdateService::instance();
    status_->set_full_text(svc.status_text());
    if (svc.stage() != UpdateStage::Downloading)
        return;
    if (total_ > 0) {
        if (progress_->maximum() != kUdlgProgressScale)
            progress_->setRange(0, kUdlgProgressScale);
        const qint64 done = std::clamp<qint64>(received_, 0, total_);
        progress_->setValue(static_cast<int>(done * kUdlgProgressScale / total_));
    } else if (progress_->maximum() != 0) {
        progress_->setRange(0, 0); // size not known (yet)
    }
    figures_->set_full_text(download_figures());
}

QString UpdateDialog::download_figures() {
    if (!have_progress_)
        return {};
    const QLocale locale;
    const auto fmt_bytes = [&locale](qint64 bytes) {
        return locale.formattedDataSize(bytes, 1, QLocale::DataSizeSIFormat);
    };
    QStringList parts;
    parts << (total_ > 0 ? tr("%1 of %2").arg(fmt_bytes(received_), fmt_bytes(total_))
                         : tr("%1 downloaded").arg(fmt_bytes(received_)));

    // Moving average over the last few seconds, measured up to now, so a
    // stalled download reads as stalled rather than at its last speed.
    const qint64 now = clock_.elapsed();
    while (samples_.size() > 1 && now - samples_.constFirst().t_ms > kUdlgSpeedWindowMs)
        samples_.removeFirst();
    if (!samples_.isEmpty()) {
        const qint64 span = now - samples_.constFirst().t_ms;
        if (span >= kUdlgMinSpeedSpanMs) {
            const double speed =
                static_cast<double>(received_ - samples_.constFirst().bytes) * 1000.0 / static_cast<double>(span);
            if (speed >= 1.0) {
                parts << tr("%1/s").arg(fmt_bytes(static_cast<qint64>(speed)));
                if (total_ > received_)
                    parts << udlg_time_left(static_cast<double>(total_ - received_) / speed);
            } else {
                parts << tr("waiting for data…");
            }
        }
    }
    return parts.join(QStringLiteral(" — "));
}

void UpdateDialog::set_button(QPushButton* button, Action action, const QString& text) {
    button->setProperty(kUdlgActionProp, static_cast<int>(action));
    button->setText(text);
    button->setVisible(action != Action::None);
}

void UpdateDialog::run_action(Action action) {
    auto& svc = UpdateService::instance();
    switch (action) {
        case Action::None:
            return;
        case Action::Close:
            reject();
            return;
        case Action::CheckNow:
            svc.check_for_updates(/*user_initiated=*/true);
            break;
        case Action::UpdateNow:
            LOG_INFO(kUdlgTag, QString("User started the update to %1").arg(svc.latest_version()));
            svc.start_update();
            break;
        case Action::OpenDownloadPage:
            udlg_open_url(udlg_download_url());
            reject();
            return;
        case Action::CancelDownload:
            LOG_INFO(kUdlgTag, "User cancelled the update download");
            svc.cancel();
            break;
        case Action::TryAgain:
            if (svc.failed_during_update())
                svc.start_update();
            else
                svc.check_for_updates(/*user_initiated=*/true);
            break;
        case Action::RetryPrevious:
            notice_ = Notice::None;
            notice_arg_.clear();
            svc.check_for_updates(/*user_initiated=*/true);
            break;
    }
    // The service normally answers with stage_changed (already rendered); this
    // covers a request it ignored.
    render();
}

// ── Closing ──────────────────────────────────────────────────────────────────

bool UpdateDialog::closable() const {
    // The application is quitting (for the restart into the update): never
    // stand in the way of the quit's close of every window.
    if (notice_ != Notice::None || udlg_shutting_down())
        return true;
    const UpdateStage stage = UpdateService::instance().stage();
    return stage != UpdateStage::Verifying && stage != UpdateStage::Installing && stage != UpdateStage::Restarting;
}

void UpdateDialog::apply_closable(bool closable) {
    const Qt::WindowFlags current_flags = windowFlags();
    Qt::WindowFlags wanted = current_flags;
    wanted.setFlag(Qt::WindowCloseButtonHint, closable);
    if (wanted == current_flags)
        return;
    QWindow* handle = windowHandle();
    if (isVisible() && handle) {
        // setWindowFlags() would hide and re-create a visible window; update
        // the native window in place and keep the widget's record in step.
        overrideWindowFlags(wanted);
        handle->setFlags(wanted);
    } else {
        setWindowFlags(wanted);
    }
}

void UpdateDialog::reject() {
    if (!closable())
        return; // verification, installation and the restart cannot be interrupted
    auto& svc = UpdateService::instance();
    if (notice_ == Notice::None && svc.stage() == UpdateStage::Downloading) {
        LOG_INFO(kUdlgTag, "Update window closed during the download — cancelling the download");
        svc.cancel();
    }
    if (notice_ == Notice::PreviousFailed && !notice_arg_.isEmpty())
        udlg_prompted_version() = notice_arg_; // declined: no offer of the same version this run
    closed_by_user_ = true;
    QDialog::reject();
}

void UpdateDialog::closeEvent(QCloseEvent* event) {
    if (!closable()) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

// ── Appearance / language ────────────────────────────────────────────────────

void UpdateDialog::fit_height() {
    QLayout* lay = layout();
    if (!lay)
        return;
    // Hug the content of the stage now shown (word-wrapped text included) at
    // the current width, so stages with less to say do not leave a gap.
    ensurePolished();
    lay->invalidate();
    lay->activate();
    const int w = std::max(width(), minimumWidth());
    const int h = lay->hasHeightForWidth() ? lay->totalHeightForWidth(w) : lay->totalSizeHint().height();
    resize(w, std::max(h, lay->totalMinimumSize().height()));
}

void UpdateDialog::refresh_theme() {
    setMinimumWidth(metrics::width(kUdlgMinWidth));
    const QString text_css =
        QString("#updateDialog{background:%1;}"
                "QLabel{background:transparent;}"
                "#updTitle{color:%2;font-size:" + fonts::px(4) + "px;font-weight:700;}"
                "#updBody{color:%2;font-size:" + fonts::px(-1) + "px;}"
                "#updBody[tone=\"error\"]{border-left:2px solid %3;padding-left:10px;}"
                "#updNotes{color:%4;font-size:" + fonts::px(-2) + "px;background:%5;border:1px solid %6;"
                "  padding:8px 10px;}"
                "#updNote{color:%4;font-size:" + fonts::px(-2) + "px;}"
                "#updNote[tone=\"warn\"]{color:%7;}"
                "#updStatus{color:%4;font-size:" + fonts::px(-2) + "px;}"
                "#updFigures{color:%8;font-size:" + fonts::px(-2) + "px;}")
            .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::DANGER(), colors::TEXT_SECONDARY(),
                 colors::BG_RAISED(), colors::BORDER_DIM(), colors::WARNING(), colors::TEXT_TERTIARY());
    // Accent fills carry TEXT_ON_ACCENT.
    const QString control_css =
        QString("QProgressBar#updProgress{background:%1;border:1px solid %2;border-radius:2px;"
                "  min-height:6px;max-height:6px;}"
                "QProgressBar#updProgress::chunk{background:%3;border-radius:2px;}"
                "QPushButton#updLink{background:transparent;color:%3;border:none;padding:0;font-weight:700;"
                "  text-align:left;font-size:" + fonts::px(-2) + "px;}"
                "QPushButton#updLink:hover{color:%4;text-decoration:underline;}"
                "QPushButton#updGhost{background:transparent;color:%5;border:1px solid %6;"
                "  padding:6px 16px;font-weight:700;}"
                "QPushButton#updGhost:hover{color:%3;border-color:%3;}"
                "QPushButton#updPrimary{background:%3;color:%7;border:1px solid %3;padding:6px 18px;font-weight:700;}"
                "QPushButton#updPrimary:hover{border-color:%4;}"
                "QPushButton#updPrimary:pressed{background:%8;border-color:%8;color:%7;}")
            .arg(colors::BG_RAISED(), colors::BORDER_DIM(), colors::AMBER(), colors::TEXT_PRIMARY(),
                 colors::TEXT_SECONDARY(), colors::BORDER_MED(), colors::TEXT_ON_ACCENT(), colors::AMBER_DIM());
    setStyleSheet(text_css + control_css);
    if (rendered_) {
        // Font size may have changed: re-fit once the new style has settled.
        QTimer::singleShot(0, this, &UpdateDialog::fit_height);
    }
}

void UpdateDialog::changeEvent(QEvent* event) {
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && rendered_) {
        render();
        fit_height();
    }
}

} // namespace fincept::ui
