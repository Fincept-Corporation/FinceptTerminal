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
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kAccent = "#d97706"; // Amber

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::progress_changed, this,
            &SetupScreen::on_progress);

    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::setup_complete, this,
            &SetupScreen::on_setup_done);
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

    auto* subtitle = new QLabel("First-Time Environment Setup", center);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:13px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    cl->addWidget(subtitle);

    auto* desc = new QLabel("Fincept Terminal requires Python and several analytics libraries.\n"
                            "This one-time setup uses UV (ultra-fast package manager) to download\n"
                            "and configure everything automatically. Internet connection required.\n"
                            "Estimated time: 3-5 minutes.",
                            center);
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; margin:10px 0;")
                            .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(desc);

    cl->addSpacing(10);

    // Step progress rows — UV-first optimized flow
    cl->addWidget(build_step_row("UV Package Manager", "uv"));
    cl->addWidget(build_step_row("Python 3.12 (via UV)", "python"));
    cl->addWidget(build_step_row("Virtual Environments", "venv"));
    cl->addWidget(build_step_row("NumPy 1.x Packages", "packages-numpy1"));
    cl->addWidget(build_step_row("NumPy 2.x Packages", "packages-numpy2"));

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

    // Status label
    status_label_ = new QLabel("Click BEGIN SETUP to start", center);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;")
                                     .arg(colors::TEXT_TERTIARY, fonts::DATA_FAMILY));
    cl->addWidget(status_label_);

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
        status_label_->setText("Setup complete! Starting Fincept Terminal...");
        status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;")
                                         .arg(colors::GREEN, fonts::DATA_FAMILY));
        begin_btn_->setText("SETUP COMPLETE");

        LOG_INFO("SetupScreen", "Setup completed successfully");

        // Emit signal after a short delay so user sees the success message
        QTimer::singleShot(1500, this, [this]() { emit setup_complete(); });
    } else {
        begin_btn_->setEnabled(true);
        begin_btn_->setText("RETRY SETUP");
        status_label_->setText("Setup failed: " + error);
        status_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; margin-top:8px;").arg(colors::RED, fonts::DATA_FAMILY));

        LOG_ERROR("SetupScreen", "Setup failed: " + error);
    }
}

} // namespace fincept::screens
