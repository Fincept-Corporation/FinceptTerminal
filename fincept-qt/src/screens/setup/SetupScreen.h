// src/screens/setup/SetupScreen.h
// First-run setup screen — shown when Python environment is not installed.
// Displays setup steps with progress bars and a BEGIN SETUP button.
//
// Internationalised: all user-facing strings flow through tr(). State-driven
// labels (subtitle, primary button, status line) are derived from member
// state enums so retranslateUi() can reapply the right text after a language
// switch without losing the current state.
#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::python {
struct SetupProgress;
}

namespace fincept::net {
class NetSpeedMeter;
}

namespace fincept::ui {
class SpeedSparkline;
}

namespace fincept::screens {

class SetupScreen : public QWidget {
    Q_OBJECT
  public:
    explicit SetupScreen(QWidget* parent = nullptr);

  signals:
    void setup_complete();

  private slots:
    void on_begin_setup();
    void on_progress(const fincept::python::SetupProgress& progress);
    void on_setup_done(bool success, const QString& error);
    void on_skip_clicked();
    void on_setup_timeout();
    void on_net_speed(qint64 down_bps, qint64 up_bps);
    void on_elapsed_tick();

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    QWidget* build_step_row(const QString& key);
    void prefill_completed_steps();
    void mark_step_done(const QString& key);
    void mark_step_active(const QString& key);
    void start_pulse(const QString& key);
    void stop_pulse(const QString& key);

    /// Reapply translated text to every widget cached as a member pointer.
    /// State-dependent labels are re-derived from member state enums so
    /// retranslating mid-flow doesn't lose the current screen state.
    void retranslateUi();
    void update_subtitle();
    void update_begin_button();
    void update_status_label();
    void update_step_status(const QString& key);
    void update_elapsed_label();

    // ── State enums for runtime-translatable dynamic text ───────────────────
    enum class SubtitleState { Ready, AlreadyConfigured, Finishing };
    enum class BeginBtnState { Begin, SettingUp, Retry, Launch, Continue, AlreadyComplete };
    enum class StatusState   { Idle, InProgress, AllDone, AnyDone, Failed, Timeout, Custom };

    SubtitleState subtitle_state_ = SubtitleState::Ready;
    BeginBtnState begin_btn_state_ = BeginBtnState::Begin;
    StatusState   status_state_    = StatusState::Idle;
    /// Detail string baked into the current status — e.g. the python error
    /// surfaced for StatusState::Failed, or a raw progress message used for
    /// StatusState::Custom. Stored verbatim and re-rendered with the active
    /// language template.
    QString status_detail_;

    QPushButton* begin_btn_ = nullptr;
    QPushButton* skip_btn_ = nullptr;
    QLabel* title_lbl_ = nullptr;       // FINCEPT TERMINAL — brand, not translated
    QLabel* status_label_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QLabel* intro_lbl_ = nullptr;
    QLabel* summary_lbl_ = nullptr;
    QLabel* install_dir_lbl_ = nullptr;
    QTimer* timeout_timer_ = nullptr;

    QWidget* live_row_ = nullptr;
    QLabel* down_lbl_ = nullptr;
    QLabel* up_lbl_ = nullptr;
    QLabel* elapsed_lbl_ = nullptr;
    fincept::ui::SpeedSparkline* sparkline_ = nullptr;
    fincept::net::NetSpeedMeter* net_meter_ = nullptr;
    QTimer* elapsed_timer_ = nullptr;
    qint64 setup_started_ms_ = 0;
    qint64 elapsed_secs_ = 0;
    qint64 down_bps_ = 0;
    qint64 up_bps_ = 0;

    /// Per-step status state. "Waiting" is the default; transitions to
    /// "Working...", an integer percentage string, "DONE", or "FAILED".
    enum class StepStatus { Waiting, Working, Done, Failed, Percent };

    struct StepUI {
        QLabel* label = nullptr;
        QLabel* sublabel = nullptr;
        QProgressBar* bar = nullptr;
        QLabel* status = nullptr;
        QTimer* pulse = nullptr;
        int pulse_val = 0;
        StepStatus status_state = StepStatus::Waiting;
        int percent = 0;
    };
    QMap<QString, StepUI> steps_;
};

} // namespace fincept::screens
