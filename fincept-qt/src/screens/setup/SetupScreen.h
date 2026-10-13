// src/screens/setup/SetupScreen.h
// First-run setup window — shown when no usable Python environment exists.
//
// Pure view over PythonSetupManager (which owns the pipeline): four stage rows
// (runtime, trading libraries, analytics core, analytics full) with real
// progress, live per-stage detail and elapsed time, the machine download rate,
// and the Enterprise showcase in the right column. LAUNCH TERMINAL becomes
// available as soon as the analytics core is installed; the remaining
// libraries keep installing in the background after the hand-off.
//
// Internationalised: every string flows through tr(); retranslateUi() rebuilds
// the static texts and render() the state-dependent ones.
#pragma once

#include <QString>
#include <QWidget>

#include <array>

class QCloseEvent;
class QEvent;
class QHideEvent;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QTimer;

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
    /// Hand off to the main window. Emitted by LAUNCH TERMINAL (available from
    /// "core ready"), by "Continue without Python", and automatically shortly
    /// after a complete success. Fires at most once.
    void launch_requested();

  protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    struct StageRow {
        QLabel* name = nullptr;
        QLabel* desc = nullptr;
        QLabel* status = nullptr;
        QLabel* elapsed = nullptr;
        QProgressBar* bar = nullptr;
        QLabel* detail = nullptr; // elided live line; full text in the tooltip
    };
    enum class PrimaryAction { Begin, Wait, Launch, Retry };
    enum class LinkAction { None, RetryFailed, ContinueLimited };

    void build_ui();
    QWidget* build_stage_row(QWidget* parent, int index);
    void refresh_theme();
    void retranslateUi();
    /// Re-render every state-dependent widget from PythonSetupManager.
    void render();
    void render_buttons(bool has_run, bool running, bool succeeded, bool core_ready, const QString& error);
    /// Tick timer + network meter run only while a setup run is active and the window is visible.
    void sync_activity();
    void on_primary_clicked();
    void on_link_clicked();
    void on_setup_done(bool success, const QString& error);
    void on_net_speed(qint64 down_bps, qint64 up_bps);
    void start_setup();
    void launch();

    std::array<StageRow, 4> rows_{};
    std::array<bool, 4> prefill_done_{}; // what the cheap status check found already installed
    bool anything_installed_ = false;

    QLabel* title_lbl_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QProgressBar* overall_bar_ = nullptr;
    QLabel* overall_pct_lbl_ = nullptr;
    QLabel* elapsed_lbl_ = nullptr;
    QWidget* live_row_ = nullptr;
    QLabel* net_lbl_ = nullptr;
    fincept::ui::SpeedSparkline* sparkline_ = nullptr;
    QPlainTextEdit* error_box_ = nullptr;
    QPushButton* primary_btn_ = nullptr;
    QPushButton* quit_btn_ = nullptr;
    QPushButton* link_btn_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QLabel* install_dir_lbl_ = nullptr;

    QTimer* tick_timer_ = nullptr;
    fincept::net::NetSpeedMeter* net_meter_ = nullptr;

    PrimaryAction primary_action_ = PrimaryAction::Begin;
    LinkAction link_action_ = LinkAction::None;
    bool launched_ = false;
    bool auto_launch_scheduled_ = false;
    bool net_running_ = false;
};

} // namespace fincept::screens
