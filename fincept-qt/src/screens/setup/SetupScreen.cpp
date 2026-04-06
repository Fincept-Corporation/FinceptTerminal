// src/screens/setup/SetupScreen.cpp
#include "screens/setup/SetupScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
#include "services/stt/WhisperService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kAccent = "#d97706"; // Amber

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    prefill_completed_steps();

    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::progress_changed,
            this, &SetupScreen::on_progress);
    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::setup_complete,
            this, &SetupScreen::on_setup_done);

    auto& ws = services::WhisperService::instance();
    connect(&ws, &services::WhisperService::model_download_progress,
            this, &SetupScreen::on_whisper_progress);
    connect(&ws, &services::WhisperService::model_ready,
            this, &SetupScreen::on_whisper_ready);
    connect(&ws, &services::WhisperService::error_occurred,
            this, &SetupScreen::on_whisper_error);

    // 15-minute overall timeout — shows skip button if setup is stuck
    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    timeout_timer_->setInterval(15 * 60 * 1000);
    connect(timeout_timer_, &QTimer::timeout, this, &SetupScreen::on_setup_timeout);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::build_ui() {
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE, colors::TEXT_PRIMARY));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Center everything
    root->addStretch(2);

    auto* center = new QWidget(this);
    center->setFixedWidth(600);
    auto* cl = new QVBoxLayout(center);
    cl->setSpacing(20);

    // Title
    auto* title = new QLabel("FINCEPT TERMINAL", center);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:24px; font-weight:700;").arg(kAccent, fonts::DATA_FAMILY));
    cl->addWidget(title);

    subtitle_lbl_ = new QLabel("First-Time Environment Setup", center);
    subtitle_lbl_->setAlignment(Qt::AlignCenter);
    subtitle_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:13px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    cl->addWidget(subtitle_lbl_);


    cl->addSpacing(10);

    // Step progress rows — UV-first optimized flow
    cl->addWidget(build_step_row("UV Package Manager", "uv"));
    cl->addWidget(build_step_row("Python 3.12 (via UV)", "python"));
    cl->addWidget(build_step_row("Virtual Environments", "venv"));
    cl->addWidget(build_step_row("NumPy 1.x Packages", "packages-numpy1"));
    cl->addWidget(build_step_row("NumPy 2.x Packages", "packages-numpy2"));
    cl->addWidget(build_step_row("Whisper Voice Model", "whisper"));

    cl->addSpacing(20);

    // Begin setup button
    begin_btn_ = new QPushButton("BEGIN SETUP", center);
    begin_btn_->setFixedHeight(44);
    begin_btn_->setCursor(Qt::PointingHandCursor);
    begin_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:14px; font-weight:700; letter-spacing:2px; }"
                "QPushButton:hover { background:#b45309; }"
                "QPushButton:disabled { background:%4; color:%5; }")
            .arg(kAccent, colors::TEXT_PRIMARY, fonts::DATA_FAMILY, colors::BG_RAISED, colors::TEXT_TERTIARY));
    connect(begin_btn_, &QPushButton::clicked, this, &SetupScreen::on_begin_setup);
    cl->addWidget(begin_btn_);

    // Status label — text set by prefill_completed_steps()
    status_label_ = new QLabel({}, center);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;")
                                     .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(status_label_);

    summary_lbl_ = new QLabel({}, center);
    summary_lbl_->setAlignment(Qt::AlignCenter);
    summary_lbl_->setWordWrap(true);
    summary_lbl_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px;")
                                    .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(summary_lbl_);

    // Skip button — hidden until timeout fires or Whisper fails
    skip_btn_ = new QPushButton("SKIP & CONTINUE", center);
    skip_btn_->setFixedHeight(36);
    skip_btn_->setCursor(Qt::PointingHandCursor);
    skip_btn_->setVisible(false);
    skip_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                " font-family:%2; font-size:12px; font-weight:600; letter-spacing:1px; }"
                "QPushButton:hover { background:%3; }")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY, colors::BG_RAISED));
    connect(skip_btn_, &QPushButton::clicked, this, &SetupScreen::on_skip_clicked);
    cl->addWidget(skip_btn_);

    // Retry voice model button — hidden until Whisper fails
    retry_whisper_btn_ = new QPushButton("RETRY VOICE MODEL", center);
    retry_whisper_btn_->setFixedHeight(36);
    retry_whisper_btn_->setCursor(Qt::PointingHandCursor);
    retry_whisper_btn_->setVisible(false);
    retry_whisper_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                " font-family:%2; font-size:12px; font-weight:600; letter-spacing:1px; }"
                "QPushButton:hover { background:%3; }")
            .arg(kAccent, fonts::DATA_FAMILY, colors::BG_RAISED));
    connect(retry_whisper_btn_, &QPushButton::clicked, this, &SetupScreen::on_retry_whisper);
    cl->addWidget(retry_whisper_btn_);

    // Install dir info
    auto* dir_label = new QLabel("Install directory: " + python::PythonSetupManager::instance().install_dir(), center);
    dir_label->setAlignment(Qt::AlignCenter);
    dir_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px; margin-top:4px;").arg(colors::TEXT_DIM, fonts::DATA_FAMILY));
    cl->addWidget(dir_label);

    // Wrap center widget in horizontal centering
    auto* hcenter = new QHBoxLayout();
    hcenter->addStretch();
    hcenter->addWidget(center);
    hcenter->addStretch();

    root->addLayout(hcenter);
    root->addStretch(3);
}

QWidget* SetupScreen::build_step_row(const QString& label, const QString& object_name) {
    auto* row = new QWidget(this);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 4, 0, 4);
    hl->setSpacing(10);

    auto* lbl = new QLabel(label, row);
    lbl->setFixedWidth(180);
    lbl->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:600;")
                           .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    hl->addWidget(lbl);

    auto* bar = new QProgressBar(row);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(14);
    bar->setStyleSheet(QString("QProgressBar { background:%1; border:1px solid %2; }"
                               "QProgressBar::chunk { background:%3; }")
                           .arg(colors::BG_RAISED, colors::BORDER_DIM, kAccent));
    hl->addWidget(bar, 1);

    auto* status = new QLabel("Pending", row);
    status->setFixedWidth(80);
    status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    status->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    hl->addWidget(status);

    StepUI step;
    step.label = lbl;
    step.bar = bar;
    step.status = status;
    steps_[object_name] = step;

    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::on_begin_setup() {
    begin_btn_->setEnabled(false);
    begin_btn_->setText("SETTING UP...");
    status_label_->setText("Setup in progress — please don't close the application");
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(kAccent, fonts::DATA_FAMILY));

    // Start overall timeout — shows skip button if setup is stuck after 15 min
    if (timeout_timer_) timeout_timer_->start();

    python::PythonSetupManager::instance().run_setup();
}

void SetupScreen::on_progress(const python::SetupProgress& progress) {
    // Update the step's progress bar and status
    // Map step names: "python", "uv", "venv-numpy1", "packages-numpy1", "venv-numpy2", "packages-numpy2"
    QString step_key = progress.step;

    // venv creation progress goes to the venv row
    if (step_key == "venv-numpy1" || step_key == "venv-numpy2") {
        // handled below
    }

    if (steps_.contains(step_key)) {
        auto& ui = steps_[step_key];
        ui.bar->setValue(progress.progress);

        if (progress.is_error) {
            ui.status->setText("FAILED");
            ui.status->setStyleSheet(
                QString("color:%1; font-family:%2; font-size:9px;").arg(colors::RED, fonts::DATA_FAMILY));
        } else if (progress.progress >= 100) {
            ui.status->setText("DONE");
            ui.status->setStyleSheet(
                QString("color:%1; font-family:%2; font-size:9px;").arg(colors::GREEN, fonts::DATA_FAMILY));
        } else {
            ui.status->setText(QString("%1%").arg(progress.progress));
            ui.status->setStyleSheet(
                QString("color:%1; font-family:%2; font-size:9px;").arg(kAccent, fonts::DATA_FAMILY));
        }
    }

    // Update main status
    status_label_->setText(progress.message);
}

void SetupScreen::on_setup_done(bool success, const QString& error) {
    if (success) {
        LOG_INFO("SetupScreen", "Python setup completed — starting Whisper model download");
        status_label_->setText("Downloading Whisper voice model...");
        status_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(kAccent, fonts::DATA_FAMILY));

        // Kick off whisper model download as the final setup step.
        // on_whisper_ready() will emit setup_complete() when done.
        services::WhisperService::instance().download_for_setup();
    } else {
        begin_btn_->setEnabled(true);
        begin_btn_->setText("RETRY SETUP");
        status_label_->setText("Setup failed: " + error);
        status_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(colors::RED, fonts::DATA_FAMILY));

        LOG_ERROR("SetupScreen", "Setup failed: " + error);
    }
}

void SetupScreen::on_whisper_progress(int percent) {
    if (!steps_.contains("whisper")) return;
    auto& ui = steps_["whisper"];
    ui.bar->setValue(percent);
    if (percent < 100) {
        ui.status->setText(QString("%1%").arg(percent));
        ui.status->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:9px;").arg(kAccent, fonts::DATA_FAMILY));
    }
    status_label_->setText(QString("Downloading Whisper voice model... %1%").arg(percent));
}

void SetupScreen::on_whisper_ready() {
    mark_step_done("whisper");
    status_label_->setText("All components installed! Starting Fincept Terminal...");
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;")
                                     .arg(colors::GREEN, fonts::DATA_FAMILY));
    begin_btn_->setText("SETUP COMPLETE");

    LOG_INFO("SetupScreen", "Whisper model ready — setup fully complete");
    if (timeout_timer_) timeout_timer_->stop();
    QTimer::singleShot(1500, this, [this]() { emit setup_complete(); });
}

void SetupScreen::on_whisper_error(const QString& message) {
    if (steps_.contains("whisper")) {
        auto& ui = steps_["whisper"];
        ui.status->setText("FAILED");
        ui.status->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:9px;").arg(colors::RED, fonts::DATA_FAMILY));
    }
    // Non-fatal — voice commands won't work but app can still launch.
    LOG_WARN("SetupScreen", "Whisper model download failed: " + message);
    whisper_failed_ = true;
    status_label_->setText("Voice model download failed — voice commands unavailable.");
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    begin_btn_->setText("SETUP COMPLETE");

    // Show retry and skip buttons so user has explicit choices
    if (retry_whisper_btn_) retry_whisper_btn_->setVisible(true);
    if (skip_btn_) {
        skip_btn_->setVisible(true);
        skip_btn_->setText("SKIP & CONTINUE WITHOUT VOICE");
    }
}

void SetupScreen::on_skip_clicked() {
    LOG_INFO("SetupScreen", "User skipped setup — launching app");
    if (timeout_timer_) timeout_timer_->stop();
    emit setup_complete();
}

void SetupScreen::on_setup_timeout() {
    LOG_WARN("SetupScreen", "Setup timed out after 15 minutes — showing skip option");
    status_label_->setText("Setup is taking longer than expected. You can skip and continue.");
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;")
            .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    if (skip_btn_) skip_btn_->setVisible(true);
}

void SetupScreen::on_retry_whisper() {
    whisper_failed_ = false;
    if (retry_whisper_btn_) retry_whisper_btn_->setVisible(false);
    if (skip_btn_)          skip_btn_->setVisible(false);
    if (steps_.contains("whisper")) {
        auto& ui = steps_["whisper"];
        ui.bar->setValue(0);
        ui.status->setText("Retrying...");
        ui.status->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:9px;").arg(kAccent, fonts::DATA_FAMILY));
    }
    status_label_->setText("Retrying Whisper voice model download...");
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(kAccent, fonts::DATA_FAMILY));
    LOG_INFO("SetupScreen", "User requested Whisper retry");
    services::WhisperService::instance().download_for_setup();
}

void SetupScreen::mark_step_done(const QString& key) {
    if (!steps_.contains(key)) return;
    auto& ui = steps_[key];
    ui.bar->setValue(100);
    ui.status->setText("DONE");
    ui.status->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::GREEN, fonts::DATA_FAMILY));
}

void SetupScreen::prefill_completed_steps() {
    const auto status  = python::PythonSetupManager::instance().check_status();
    const bool whisper = services::WhisperService::is_model_downloaded();

    // Pre-fill completed steps as green DONE
    if (status.uv_installed)        mark_step_done("uv");
    if (status.python_installed)    mark_step_done("python");
    if (status.venv_numpy1_ready && status.venv_numpy2_ready) mark_step_done("venv");
    if (status.venv_numpy1_ready)   mark_step_done("packages-numpy1");
    if (status.venv_numpy2_ready)   mark_step_done("packages-numpy2");
    if (whisper)                    mark_step_done("whisper");

    const bool any_done = status.uv_installed || status.python_installed
                          || status.venv_numpy1_ready || status.venv_numpy2_ready
                          || whisper;
    const bool all_done = status.uv_installed && status.python_installed
                          && status.venv_numpy1_ready && status.venv_numpy2_ready
                          && whisper;

    summary_lbl_->setText({});

    if (all_done) {
        subtitle_lbl_->setText("Environment Ready");
        begin_btn_->setEnabled(false);
        begin_btn_->setText("ALREADY COMPLETE");
        status_label_->setText("All components are installed.");
    } else if (any_done) {
        subtitle_lbl_->setText("Completing Environment Setup");
        begin_btn_->setText("COMPLETE SETUP");
        status_label_->setText("Only missing components will be downloaded. Internet connection required.");
    } else {
        subtitle_lbl_->setText("First-Time Environment Setup");
        begin_btn_->setText("BEGIN SETUP");
        status_label_->setText("Estimated time: 3–5 minutes. Internet connection required.");
    }
}

} // namespace fincept::screens
