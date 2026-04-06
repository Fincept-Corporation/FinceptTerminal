// src/screens/setup/SetupScreen.h
// First-run setup screen — shown when Python environment is not installed.
// Displays setup steps with progress bars and a BEGIN SETUP button.
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
    void on_whisper_progress(int percent);
    void on_whisper_ready();
    void on_whisper_error(const QString& message);
    void on_skip_clicked();       // user-initiated skip
    void on_setup_timeout();      // overall 15-min timeout fired
    void on_retry_whisper();      // retry voice model after failure

  private:
    void build_ui();
    QWidget* build_step_row(const QString& key, const QString& label, const QString& sublabel);
    void prefill_completed_steps();
    void mark_step_done(const QString& key);
    void mark_step_active(const QString& key);   // highlights the currently running step
    void start_pulse(const QString& key);        // starts indeterminate pulse on a bar
    void stop_pulse(const QString& key);         // stops pulse, keeps value

    QPushButton* begin_btn_        = nullptr;
    QPushButton* skip_btn_         = nullptr;
    QPushButton* retry_whisper_btn_= nullptr;
    QLabel*      status_label_     = nullptr;
    QLabel*      subtitle_lbl_     = nullptr;
    QLabel*      summary_lbl_      = nullptr;
    QTimer*      timeout_timer_    = nullptr;   // 15-min overall timeout
    bool         whisper_failed_   = false;

    // Step progress bars (keyed by step name)
    struct StepUI {
        QLabel*      label     = nullptr;
        QLabel*      sublabel  = nullptr;   // plain-English description below the label
        QProgressBar* bar      = nullptr;
        QLabel*      status    = nullptr;
        QTimer*      pulse     = nullptr;   // drives indeterminate animation
        int          pulse_val = 0;         // current pulse position 0-99
    };
    QMap<QString, StepUI> steps_;
};

} // namespace fincept::screens
