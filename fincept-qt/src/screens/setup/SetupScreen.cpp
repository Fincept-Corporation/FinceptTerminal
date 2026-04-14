// src/screens/setup/SetupScreen.cpp
#include "screens/setup/SetupScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
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

static const QString kAccent = colors::AMBER;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    prefill_completed_steps();

    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::progress_changed, this,
            &SetupScreen::on_progress);
    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::setup_complete, this,
            &SetupScreen::on_setup_done);

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

    root->addStretch(2);

    auto* center = new QWidget(this);
    center->setFixedWidth(640);
    auto* cl = new QVBoxLayout(center);
    cl->setSpacing(8);

    // ── Title block ───────────────────────────────────────────────────────────
    auto* title = new QLabel("FINCEPT TERMINAL", center);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color:%1; font-family:%2; font-size:24px; font-weight:700; letter-spacing:3px;")
                             .arg(kAccent, fonts::DATA_FAMILY));
    cl->addWidget(title);

    subtitle_lbl_ = new QLabel("Getting your workspace ready", center);
    subtitle_lbl_->setAlignment(Qt::AlignCenter);
    subtitle_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:13px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    cl->addWidget(subtitle_lbl_);

    // Brief plain-English explanation
    auto* intro = new QLabel("We need to download a few tools and data libraries once.\n"
                             "This only happens the first time — future launches are instant.",
                             center);
    intro->setAlignment(Qt::AlignCenter);
    intro->setWordWrap(true);
    intro->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:4px; margin-bottom:12px;")
                             .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(intro);

    // ── Divider ───────────────────────────────────────────────────────────────
    auto* divider = new QFrame(center);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM));
    cl->addWidget(divider);
    cl->addSpacing(4);

    // ── Step rows — plain-English labels + technical key hidden inside ────────
    cl->addWidget(
        build_step_row("uv", "Download Installer", "Downloads the tool that manages everything else (~13 MB)"));
    cl->addWidget(
        build_step_row("python", "Install Python Runtime", "The programming language engine used for all analytics"));
    cl->addWidget(build_step_row("venv", "Create Isolated Workspaces",
                                 "Two sandboxed environments to keep library versions conflict-free"));
    cl->addWidget(build_step_row("packages-numpy1", "Install Trading Libraries",
                                 "Backtesting, portfolio optimization and legacy quant tools"));
    cl->addWidget(build_step_row("packages-numpy2", "Install Analytics Libraries",
                                 "Machine learning, data science and AI agent frameworks"));

    cl->addSpacing(16);

    // ── Action buttons ────────────────────────────────────────────────────────
    begin_btn_ = new QPushButton("BEGIN SETUP", center);
    begin_btn_->setFixedHeight(46);
    begin_btn_->setCursor(Qt::PointingHandCursor);
    begin_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:14px; font-weight:700; letter-spacing:2px; border-radius:3px; }"
                "QPushButton:hover { background:#b45309; }"
                "QPushButton:disabled { background:%4; color:%5; border-radius:3px; }")
            .arg(kAccent, colors::TEXT_PRIMARY, fonts::DATA_FAMILY, colors::BG_RAISED, colors::TEXT_TERTIARY));
    connect(begin_btn_, &QPushButton::clicked, this, &SetupScreen::on_begin_setup);
    cl->addWidget(begin_btn_);

    // Status label
    status_label_ = new QLabel({}, center);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;")
                                     .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(status_label_);

    summary_lbl_ = new QLabel({}, center);
    summary_lbl_->setAlignment(Qt::AlignCenter);
    summary_lbl_->setWordWrap(true);
    summary_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px;").arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(summary_lbl_);

    // Skip button
    skip_btn_ = new QPushButton("SKIP & CONTINUE", center);
    skip_btn_->setFixedHeight(36);
    skip_btn_->setCursor(Qt::PointingHandCursor);
    skip_btn_->setVisible(false);
    skip_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                " font-family:%2; font-size:12px; font-weight:600; letter-spacing:1px; border-radius:3px; }"
                "QPushButton:hover { background:%3; }")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY, colors::BG_RAISED));
    connect(skip_btn_, &QPushButton::clicked, this, &SetupScreen::on_skip_clicked);
    cl->addWidget(skip_btn_);

    // Install dir — small, dim, for power users
    auto* dir_label = new QLabel("Installing to: " + python::PythonSetupManager::instance().install_dir(), center);
    dir_label->setAlignment(Qt::AlignCenter);
    dir_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px; margin-top:6px;").arg(colors::TEXT_DIM, fonts::DATA_FAMILY));
    cl->addWidget(dir_label);

    auto* hcenter = new QHBoxLayout();
    hcenter->addStretch();
    hcenter->addWidget(center);
    hcenter->addStretch();

    root->addLayout(hcenter);
    root->addStretch(3);
}

QWidget* SetupScreen::build_step_row(const QString& key, const QString& label, const QString& sublabel) {
    auto* row = new QWidget(this);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 6, 0, 6);
    hl->setSpacing(12);

    // Left: label stack (name + description)
    auto* label_col = new QWidget(row);
    label_col->setFixedWidth(220);
    auto* label_vl = new QVBoxLayout(label_col);
    label_vl->setContentsMargins(0, 0, 0, 0);
    label_vl->setSpacing(2);

    auto* lbl = new QLabel(label, label_col);
    lbl->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:600;")
                           .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    label_vl->addWidget(lbl);

    auto* sub = new QLabel(sublabel, label_col);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_DIM, fonts::DATA_FAMILY));
    label_vl->addWidget(sub);

    hl->addWidget(label_col);

    // Middle: progress bar
    auto* bar = new QProgressBar(row);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(10);
    bar->setStyleSheet(QString("QProgressBar { background:%1; border:1px solid %2; border-radius:5px; }"
                               "QProgressBar::chunk { background:%3; border-radius:5px; }")
                           .arg(colors::BG_RAISED, colors::BORDER_DIM, kAccent));
    hl->addWidget(bar, 1);

    // Right: status badge
    auto* status = new QLabel("Waiting", row);
    status->setFixedWidth(72);
    status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    status->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    hl->addWidget(status);

    // Pulse timer — drives the indeterminate animation while step is in-flight
    auto* pulse = new QTimer(row);
    pulse->setInterval(40); // ~25 fps — smooth but cheap
    pulse->setSingleShot(false);

    StepUI step;
    step.label = lbl;
    step.sublabel = sub;
    step.bar = bar;
    step.status = status;
    step.pulse = pulse;
    step.pulse_val = 0;
    steps_[key] = step;

    // Connect pulse tick — captures key by value so it's safe after row moves
    connect(pulse, &QTimer::timeout, this, [this, key]() {
        if (!steps_.contains(key))
            return;
        auto& s = steps_[key];
        // Smooth bouncing fill: ramps 0→85 then stays — gives "working" feel
        // without implying false completion.
        s.pulse_val = std::min(s.pulse_val + 1, 85);
        s.bar->setValue(s.pulse_val);
    });

    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Pulse helpers
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::start_pulse(const QString& key) {
    if (!steps_.contains(key))
        return;
    auto& s = steps_[key];
    s.pulse_val = 0;
    s.bar->setValue(0);
    if (s.pulse)
        s.pulse->start();
}

void SetupScreen::stop_pulse(const QString& key) {
    if (!steps_.contains(key))
        return;
    auto& s = steps_[key];
    if (s.pulse)
        s.pulse->stop();
}

void SetupScreen::mark_step_active(const QString& key) {
    if (!steps_.contains(key))
        return;
    auto& s = steps_[key];
    s.status->setText("Working...");
    s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;").arg(kAccent, fonts::DATA_FAMILY));
    start_pulse(key);
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::on_begin_setup() {
    begin_btn_->setEnabled(false);
    begin_btn_->setText("SETTING UP...");
    status_label_->setText("Setup in progress — please keep the application open");
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;").arg(kAccent, fonts::DATA_FAMILY));

    if (timeout_timer_)
        timeout_timer_->start();
    python::PythonSetupManager::instance().run_setup();
}

void SetupScreen::on_progress(const python::SetupProgress& progress) {
    const QString& key = progress.step;

    if (steps_.contains(key)) {
        auto& s = steps_[key];

        if (progress.is_error) {
            // Stop pulse, show red failed badge
            stop_pulse(key);
            s.bar->setValue(s.pulse_val > 0 ? s.pulse_val : 0);
            s.status->setText("FAILED");
            s.status->setStyleSheet(
                QString("color:%1; font-family:%2; font-size:9px;").arg(colors::RED, fonts::DATA_FAMILY));
        } else if (progress.progress == 0) {
            // Step just started — kick off the pulse animation
            mark_step_active(key);
        } else if (progress.progress >= 100) {
            // Step finished — stop pulse and snap to full
            mark_step_done(key);
        } else {
            // Mid-step real progress (pass-2 per-package installs send real %)
            stop_pulse(key);
            s.bar->setValue(progress.progress);
            s.status->setText(QString("%1%").arg(progress.progress));
            s.status->setStyleSheet(
                QString("color:%1; font-family:%2; font-size:9px;").arg(kAccent, fonts::DATA_FAMILY));
        }
    }

    // Plain-English status line — strip internal technical messages
    QString msg = progress.message;
    // Replace internal step identifiers users don't need to see
    msg.replace("venv-numpy1", "Trading workspace");
    msg.replace("venv-numpy2", "Analytics workspace");
    msg.replace("requirements-numpy1.txt", "trading library list");
    msg.replace("requirements-numpy2.txt", "analytics library list");
    status_label_->setText(msg);
}

void SetupScreen::on_setup_done(bool success, const QString& error) {
    if (success) {
        LOG_INFO("SetupScreen", "Python setup completed — all steps done");
        status_label_->setText("Everything is ready! Launching Fincept Terminal...");
        status_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;").arg(colors::GREEN, fonts::DATA_FAMILY));
        begin_btn_->setText("LAUNCH");
        if (timeout_timer_)
            timeout_timer_->stop();
        QTimer::singleShot(1500, this, [this]() { emit setup_complete(); });
    } else {
        // Stop all running pulses
        for (auto it = steps_.keyBegin(); it != steps_.keyEnd(); ++it)
            stop_pulse(*it);

        begin_btn_->setEnabled(true);
        begin_btn_->setText("RETRY SETUP");
        status_label_->setText("Something went wrong during setup. Check your internet connection and try again.");
        status_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;").arg(colors::RED, fonts::DATA_FAMILY));
        LOG_ERROR("SetupScreen", "Setup failed: " + error);
    }
}

void SetupScreen::on_skip_clicked() {
    LOG_INFO("SetupScreen", "User skipped setup — launching app");
    if (timeout_timer_)
        timeout_timer_->stop();
    for (auto it = steps_.keyBegin(); it != steps_.keyEnd(); ++it)
        stop_pulse(*it);
    emit setup_complete();
}

void SetupScreen::on_setup_timeout() {
    LOG_WARN("SetupScreen", "Setup timed out after 15 minutes — showing skip option");
    status_label_->setText("Setup is taking longer than expected — possibly a slow internet connection.\n"
                           "You can wait or skip and continue with limited functionality.");
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;")
                                     .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    if (skip_btn_)
        skip_btn_->setVisible(true);
}

void SetupScreen::mark_step_done(const QString& key) {
    if (!steps_.contains(key))
        return;
    stop_pulse(key);
    auto& s = steps_[key];
    s.bar->setValue(100);
    s.status->setText("DONE");
    s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;").arg(colors::GREEN, fonts::DATA_FAMILY));
}

void SetupScreen::prefill_completed_steps() {
    const auto status = python::PythonSetupManager::instance().check_status();

    if (status.uv_installed)
        mark_step_done("uv");
    if (status.python_installed)
        mark_step_done("python");
    if (status.venv_numpy1_ready && status.venv_numpy2_ready)
        mark_step_done("venv");
    if (status.venv_numpy1_ready)
        mark_step_done("packages-numpy1");
    if (status.venv_numpy2_ready)
        mark_step_done("packages-numpy2");

    const bool any_done = status.uv_installed || status.python_installed || status.venv_numpy1_ready ||
                          status.venv_numpy2_ready;
    const bool all_done = status.uv_installed && status.python_installed && status.venv_numpy1_ready &&
                          status.venv_numpy2_ready;

    summary_lbl_->setText({});

    if (all_done) {
        subtitle_lbl_->setText("Your workspace is fully configured");
        begin_btn_->setEnabled(false);
        begin_btn_->setText("ALREADY COMPLETE");
        status_label_->setText("Everything is installed and ready to go.");
    } else if (any_done) {
        subtitle_lbl_->setText("Finishing your workspace setup");
        begin_btn_->setText("CONTINUE SETUP");
        status_label_->setText("Only the missing pieces will be downloaded. Needs an internet connection.");
    } else {
        subtitle_lbl_->setText("Getting your workspace ready");
        begin_btn_->setText("BEGIN SETUP");
        status_label_->setText("Takes about 3–5 minutes. Needs an internet connection.");
    }
}

} // namespace fincept::screens
