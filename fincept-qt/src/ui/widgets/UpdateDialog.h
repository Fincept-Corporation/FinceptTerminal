#pragma once
// UpdateDialog — the window of the in-app updater (services/updater).
//
// It shows UpdateService's state and nothing else: every stage (checking, a
// newer release, downloading, verifying, installing, restarting, failed) has a
// rendering, so from "Update now" until the terminal restarts into the new
// version the user always sees what is going on. It can be opened in the middle
// of any stage (About ▸ UPDATE AVAILABLE while a download runs) and renders
// that stage. One instance at a time, window-modal to the terminal window.
//
// The prompting policy lives here too (like UpgradeDialog::maybe_show): when a
// background check finds a newer release, the window is offered once per
// version per run, over the active terminal window and never over another
// dialog or a minimised / inactive window — it waits until it can be shown.

#include "services/updater/UpdateTypes.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QEvent>
#include <QList>
#include <QString>

class QCloseEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QWidget;

namespace fincept::ui {

class UpdateDialog : public QDialog {
    Q_OBJECT
  public:
    /// Opens the update window over `parent`'s window (the active terminal
    /// window when null), or raises the one already open. Non-blocking.
    static UpdateDialog* present(QWidget* parent);

    /// Help ▸ Check for Updates and About ▸ Check for Updates: present() and
    /// start a user-initiated check, whose result the window reports.
    static void check_now(QWidget* parent);

    /// The open update window, or nullptr.
    static UpdateDialog* current();

    /// App-shell hook, called once when the first terminal window is up
    /// (interactive runs only). Offers the window when a background check
    /// finds a newer release, and brings it back whenever an update in
    /// progress has no window showing it.
    static void watch_service();

    /// The run after an update: says whether the update the previous run
    /// started reached `to_version` (UpdateService::take_last_update_result()).
    /// Success is a toast; failure is the update window, offering to try again.
    static void show_previous_update_result(const QString& to_version, bool succeeded);

  public slots:
    /// Esc / title-bar close. Ignored while the update is being verified,
    /// installed or restarted; during a download it cancels the download.
    void reject() override;

  protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    class ElidedLine;

    /// A message shown instead of the stage until the user acts on it.
    enum class Notice { None, HandedOff, PreviousFailed, Updated };
    /// What a button does; stored on the button so the row can be reused.
    enum class Action { None, Close, CheckNow, UpdateNow, OpenDownloadPage, CancelDownload, TryAgain, RetryPrevious };

    explicit UpdateDialog(QWidget* parent);
    ~UpdateDialog() override;

    static UpdateDialog* open_dialog(QWidget* parent, Notice notice, const QString& notice_arg);
    /// Shows what is waiting to be shown (a prompt, the previous run's
    /// result) once a terminal window can take it; retries until then.
    static void process_pending();
    /// Reopens the window when an update is running and none shows it.
    static void ensure_visible();

    void set_notice(Notice notice, const QString& arg);
    void on_stage_changed();
    void on_progress(qint64 received, qint64 total);
    void reset_progress();
    void schedule_live();
    /// Re-render everything for the current stage / notice.
    void render();
    /// Only the parts that change while a stage runs: status line, download
    /// figures and the progress bar value.
    void render_live();
    QString download_figures();
    void set_button(QPushButton* button, Action action, const QString& text);
    void run_action(Action action);
    bool closable() const;
    void apply_closable(bool closable);
    void fit_height();
    void refresh_theme();

    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
    QLabel* notes_ = nullptr;
    QPushButton* notes_link_ = nullptr;
    QLabel* note_ = nullptr;
    QProgressBar* progress_ = nullptr;
    ElidedLine* status_ = nullptr;
    ElidedLine* figures_ = nullptr;
    QWidget* button_bar_ = nullptr;
    QPushButton* tertiary_btn_ = nullptr;
    QPushButton* secondary_btn_ = nullptr;
    QPushButton* primary_btn_ = nullptr;
    QTimer* live_timer_ = nullptr;
    QTimer* tick_timer_ = nullptr;

    Notice notice_ = Notice::None;
    QString notice_arg_; ///< HandedOff: the message; PreviousFailed / Updated: the version
    bool closed_by_user_ = false;
    bool rendered_ = false;
    fincept::services::updater::UpdateStage rendered_stage_ = fincept::services::updater::UpdateStage::Idle;
    Notice rendered_notice_ = Notice::None;

    // Download progress, for the "x of y — speed — time left" line.
    struct Sample {
        qint64 t_ms = 0;
        qint64 bytes = 0;
    };
    bool have_progress_ = false;
    qint64 received_ = 0;
    qint64 total_ = 0;
    QList<Sample> samples_;
    QElapsedTimer clock_;
};

} // namespace fincept::ui
