// src/screens/setup/SetupScreen.cpp
#include "screens/setup/SetupScreen.h"

#include "core/logging/Logger.h"
#include "core/net/NetSpeedMeter.h"
#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LanguageSwitcher.h"
#include "ui/widgets/SpeedSparkline.h"

#include <QDateTime>
#include <QEvent>

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens {

using namespace fincept::ui;

static const QString kAccent = colors::AMBER;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    retranslateUi();
    prefill_completed_steps();

    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::progress_changed, this,
            &SetupScreen::on_progress);
    connect(&python::PythonSetupManager::instance(), &python::PythonSetupManager::setup_complete, this,
            &SetupScreen::on_setup_done);

    timeout_timer_ = new QTimer(this);
    timeout_timer_->setSingleShot(true);
    timeout_timer_->setInterval(15 * 60 * 1000);
    connect(timeout_timer_, &QTimer::timeout, this, &SetupScreen::on_setup_timeout);

    // Reveal the Skip button ~45s in (not only after the full 15-min timeout) so
    // first-run users are never trapped waiting on the Python setup. Independent
    // of on_setup_timeout(), which still owns the real Timeout state at 15 min.
    QTimer::singleShot(45 * 1000, this, [this]() {
        if (skip_btn_)
            skip_btn_->setVisible(true);
    });

    net_meter_ = new fincept::net::NetSpeedMeter(this);
    connect(net_meter_, &fincept::net::NetSpeedMeter::speed_changed,
            this, &SetupScreen::on_net_speed);

    elapsed_timer_ = new QTimer(this);
    elapsed_timer_->setInterval(1000);
    connect(elapsed_timer_, &QTimer::timeout, this, &SetupScreen::on_elapsed_tick);
}

void SetupScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::build_ui() {
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE(), colors::TEXT_PRIMARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top-right language picker — visible during first-run setup so the user
    // can switch language before installation begins.
    auto* top_row = new QHBoxLayout;
    top_row->setContentsMargins(20, 20, 20, 0);
    top_row->addStretch();
    top_row->addWidget(new ui::LanguageSwitcher(this));
    root->addLayout(top_row);

    root->addStretch(2);

    auto* center = new QWidget(this);
    center->setFixedWidth(640);
    auto* cl = new QVBoxLayout(center);
    cl->setSpacing(8);

    // ── Title block ───────────────────────────────────────────────────────────
    // FINCEPT TERMINAL — brand, not translated.
    title_lbl_ = new QLabel(QStringLiteral("FINCEPT TERMINAL"), center);
    title_lbl_->setAlignment(Qt::AlignCenter);
    title_lbl_->setStyleSheet(QString("color:%1; font-family:%2; font-size:24px; font-weight:700; letter-spacing:3px;")
                                  .arg(kAccent, fonts::DATA_FAMILY));
    cl->addWidget(title_lbl_);

    subtitle_lbl_ = new QLabel(center);
    subtitle_lbl_->setAlignment(Qt::AlignCenter);
    subtitle_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:13px;").arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    cl->addWidget(subtitle_lbl_);

    intro_lbl_ = new QLabel(center);
    intro_lbl_->setAlignment(Qt::AlignCenter);
    intro_lbl_->setWordWrap(true);
    intro_lbl_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:4px; margin-bottom:12px;")
                                  .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    cl->addWidget(intro_lbl_);

    // ── Divider ───────────────────────────────────────────────────────────────
    auto* divider = new QFrame(center);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QString("color:%1;").arg(colors::BORDER_DIM()));
    cl->addWidget(divider);
    cl->addSpacing(4);

    // ── Step rows — labels and sublabels are translated in retranslateUi() ───
    cl->addWidget(build_step_row("uv"));
    cl->addWidget(build_step_row("python"));
    cl->addWidget(build_step_row("venv"));
    cl->addWidget(build_step_row("packages-numpy1"));
    cl->addWidget(build_step_row("packages-numpy2"));

    cl->addSpacing(16);

    begin_btn_ = new QPushButton(center);
    begin_btn_->setFixedHeight(46);
    begin_btn_->setCursor(Qt::PointingHandCursor);
    begin_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                " font-family:%3; font-size:14px; font-weight:700; letter-spacing:2px; border-radius:3px; }"
                "QPushButton:hover { background:#b45309; }"
                "QPushButton:disabled { background:%4; color:%5; border-radius:3px; }")
            .arg(kAccent, colors::TEXT_PRIMARY(), fonts::DATA_FAMILY, colors::BG_RAISED(), colors::TEXT_TERTIARY()));
    connect(begin_btn_, &QPushButton::clicked, this, &SetupScreen::on_begin_setup);
    cl->addWidget(begin_btn_);

    status_label_ = new QLabel(center);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setWordWrap(true);
    status_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;")
                                     .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    cl->addWidget(status_label_);

    // ── Liveness row ─────────────────────────────────────────────────────────
    live_row_ = new QWidget(center);
    live_row_->setVisible(false);
    auto* live_hl = new QHBoxLayout(live_row_);
    live_hl->setContentsMargins(0, 4, 0, 0);
    live_hl->setSpacing(12);

    const QString stat_css = QString("color:%1; font-family:%2; font-size:10px;")
                                 .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY);
    elapsed_lbl_ = new QLabel(live_row_);
    elapsed_lbl_->setStyleSheet(stat_css);
    elapsed_lbl_->setFixedWidth(120);
    live_hl->addWidget(elapsed_lbl_);

    down_lbl_ = new QLabel(QStringLiteral("↓ 0 B/s"), live_row_);
    down_lbl_->setStyleSheet(stat_css);
    down_lbl_->setFixedWidth(110);
    live_hl->addWidget(down_lbl_);

    up_lbl_ = new QLabel(QStringLiteral("↑ 0 B/s"), live_row_);
    up_lbl_->setStyleSheet(stat_css);
    up_lbl_->setFixedWidth(110);
    live_hl->addWidget(up_lbl_);

    sparkline_ = new fincept::ui::SpeedSparkline(live_row_);
    sparkline_->setMinimumHeight(18);
    sparkline_->set_line_color(QColor(kAccent));
    QColor accent_fill(kAccent);
    accent_fill.setAlpha(48);
    sparkline_->set_fill_color(accent_fill);
    live_hl->addWidget(sparkline_, 1);

    cl->addWidget(live_row_);

    summary_lbl_ = new QLabel(center);
    summary_lbl_->setAlignment(Qt::AlignCenter);
    summary_lbl_->setWordWrap(true);
    summary_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px;").arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    cl->addWidget(summary_lbl_);

    skip_btn_ = new QPushButton(center);
    skip_btn_->setFixedHeight(36);
    skip_btn_->setCursor(Qt::PointingHandCursor);
    skip_btn_->setVisible(false);
    skip_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                " font-family:%2; font-size:12px; font-weight:600; letter-spacing:1px; border-radius:3px; }"
                "QPushButton:hover { background:%3; }")
            .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY, colors::BG_RAISED()));
    connect(skip_btn_, &QPushButton::clicked, this, &SetupScreen::on_skip_clicked);
    cl->addWidget(skip_btn_);

    install_dir_lbl_ = new QLabel(center);
    install_dir_lbl_->setAlignment(Qt::AlignCenter);
    install_dir_lbl_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px; margin-top:6px;").arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    cl->addWidget(install_dir_lbl_);

    auto* hcenter = new QHBoxLayout();
    hcenter->addStretch();
    hcenter->addWidget(center);
    hcenter->addStretch();

    root->addLayout(hcenter);
    root->addStretch(3);
}

QWidget* SetupScreen::build_step_row(const QString& key) {
    auto* row = new QWidget(this);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 6, 0, 6);
    hl->setSpacing(12);

    auto* label_col = new QWidget(row);
    label_col->setFixedWidth(220);
    auto* label_vl = new QVBoxLayout(label_col);
    label_vl->setContentsMargins(0, 0, 0, 0);
    label_vl->setSpacing(2);

    auto* lbl = new QLabel(label_col);
    lbl->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:600;")
                           .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    label_vl->addWidget(lbl);

    auto* sub = new QLabel(label_col);
    sub->setWordWrap(true);
    sub->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    label_vl->addWidget(sub);

    hl->addWidget(label_col);

    auto* bar = new QProgressBar(row);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(10);
    bar->setStyleSheet(QString("QProgressBar { background:%1; border:1px solid %2; border-radius:5px; }"
                               "QProgressBar::chunk { background:%3; border-radius:5px; }")
                           .arg(colors::BG_RAISED(), colors::BORDER_DIM(), kAccent));
    hl->addWidget(bar, 1);

    auto* status = new QLabel(row);
    status->setFixedWidth(72);
    status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    status->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    hl->addWidget(status);

    auto* pulse = new QTimer(row);
    pulse->setInterval(100);
    pulse->setSingleShot(false);

    StepUI step;
    step.label = lbl;
    step.sublabel = sub;
    step.bar = bar;
    step.status = status;
    step.pulse = pulse;
    step.pulse_val = 0;
    step.status_state = StepStatus::Waiting;
    step.percent = 0;
    steps_[key] = step;

    connect(pulse, &QTimer::timeout, this, [this, key]() {
        if (!steps_.contains(key))
            return;
        auto& s = steps_[key];
        s.pulse_val = std::min(s.pulse_val + 1, 85);
        s.bar->setValue(s.pulse_val);
    });

    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Re-translation
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::retranslateUi() {
    if (intro_lbl_)
        intro_lbl_->setText(tr("We need to download a few tools and data libraries once.\n"
                               "This only happens the first time — future launches are instant."));

    // Step labels + sublabels.
    struct StepCopy { const char* key; QString label; QString sublabel; };
    const StepCopy copy[] = {
        {"uv",              tr("Download Installer"),
                            tr("Downloads the tool that manages everything else (~13 MB)")},
        {"python",          tr("Install Python Runtime"),
                            tr("The programming language engine used for all analytics")},
        {"venv",            tr("Create Isolated Workspaces"),
                            tr("Two sandboxed environments to keep library versions conflict-free")},
        {"packages-numpy1", tr("Install Trading Libraries"),
                            tr("Backtesting, portfolio optimization and legacy quant tools")},
        {"packages-numpy2", tr("Install Analytics Libraries"),
                            tr("Machine learning, data science and AI agent frameworks")},
    };
    for (const auto& c : copy) {
        const QString k = QString::fromLatin1(c.key);
        if (!steps_.contains(k)) continue;
        auto& s = steps_[k];
        if (s.label)    s.label->setText(c.label);
        if (s.sublabel) s.sublabel->setText(c.sublabel);
        update_step_status(k);
    }

    if (install_dir_lbl_) {
        install_dir_lbl_->setText(tr("Installing to: %1").arg(python::PythonSetupManager::instance().install_dir()));
    }

    update_subtitle();
    update_begin_button();
    update_status_label();
    update_elapsed_label();
    if (skip_btn_) skip_btn_->setText(tr("SKIP & CONTINUE"));
}

void SetupScreen::update_subtitle() {
    if (!subtitle_lbl_) return;
    switch (subtitle_state_) {
        case SubtitleState::Ready:             subtitle_lbl_->setText(tr("Getting your workspace ready")); break;
        case SubtitleState::AlreadyConfigured: subtitle_lbl_->setText(tr("Your workspace is fully configured")); break;
        case SubtitleState::Finishing:         subtitle_lbl_->setText(tr("Finishing your workspace setup")); break;
    }
}

void SetupScreen::update_begin_button() {
    if (!begin_btn_) return;
    switch (begin_btn_state_) {
        case BeginBtnState::Begin:           begin_btn_->setText(tr("BEGIN SETUP")); break;
        case BeginBtnState::SettingUp:       begin_btn_->setText(tr("SETTING UP...")); break;
        case BeginBtnState::Retry:           begin_btn_->setText(tr("RETRY SETUP")); break;
        case BeginBtnState::Launch:          begin_btn_->setText(tr("LAUNCH")); break;
        case BeginBtnState::Continue:        begin_btn_->setText(tr("CONTINUE SETUP")); break;
        case BeginBtnState::AlreadyComplete: begin_btn_->setText(tr("ALREADY COMPLETE")); break;
    }
}

void SetupScreen::update_status_label() {
    if (!status_label_) return;
    QString text;
    QString color = colors::TEXT_TERTIARY();
    switch (status_state_) {
        case StatusState::Idle:
            text  = tr("Takes about 3–5 minutes. Needs an internet connection.");
            break;
        case StatusState::InProgress:
            text  = tr("Setup in progress — please keep the application open");
            color = kAccent;
            break;
        case StatusState::AllDone:
            text  = tr("Everything is ready! Launching Fincept Terminal...");
            color = colors::GREEN();
            break;
        case StatusState::AnyDone:
            text  = tr("Only the missing pieces will be downloaded. Needs an internet connection.");
            break;
        case StatusState::Failed:
            text  = tr("Setup failed: %1").arg(status_detail_.isEmpty()
                                                    ? tr("Unknown error — see logs for details.")
                                                    : status_detail_);
            color = colors::RED();
            break;
        case StatusState::Timeout:
            text  = tr("Setup is taking longer than expected — possibly a slow internet connection.\n"
                       "You can wait or skip and continue with limited functionality.");
            break;
        case StatusState::Custom:
            text  = status_detail_;
            break;
    }
    status_label_->setText(text);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; margin-top:6px;").arg(color, fonts::DATA_FAMILY));
}

void SetupScreen::update_step_status(const QString& key) {
    if (!steps_.contains(key)) return;
    auto& s = steps_[key];
    if (!s.status) return;
    switch (s.status_state) {
        case StepStatus::Waiting:
            s.status->setText(tr("Waiting"));
            s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;")
                                        .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
            break;
        case StepStatus::Working:
            s.status->setText(tr("Working..."));
            s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;")
                                        .arg(kAccent, fonts::DATA_FAMILY));
            break;
        case StepStatus::Done:
            s.status->setText(tr("DONE"));
            s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;")
                                        .arg(colors::GREEN(), fonts::DATA_FAMILY));
            break;
        case StepStatus::Failed:
            s.status->setText(tr("FAILED"));
            s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;")
                                        .arg(colors::RED(), fonts::DATA_FAMILY));
            break;
        case StepStatus::Percent:
            // "%1%" is a locale-neutral numeric format — no translation needed.
            s.status->setText(QStringLiteral("%1%").arg(s.percent));
            s.status->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;")
                                        .arg(kAccent, fonts::DATA_FAMILY));
            break;
    }
}

void SetupScreen::update_elapsed_label() {
    if (!elapsed_lbl_) return;
    if (elapsed_secs_ < 60)
        elapsed_lbl_->setText(tr("Elapsed: %1s").arg(elapsed_secs_));
    else
        elapsed_lbl_->setText(tr("Elapsed: %1m %2s").arg(elapsed_secs_ / 60).arg(elapsed_secs_ % 60));
}

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
    s.status_state = StepStatus::Working;
    update_step_status(key);
    start_pulse(key);
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::on_begin_setup() {
    begin_btn_->setEnabled(false);
    begin_btn_state_ = BeginBtnState::SettingUp;
    update_begin_button();
    status_state_ = StatusState::InProgress;
    update_status_label();

    setup_started_ms_ = QDateTime::currentMSecsSinceEpoch();
    if (live_row_)
        live_row_->setVisible(true);
    elapsed_secs_ = 0;
    update_elapsed_label();
    if (down_lbl_)
        down_lbl_->setText(QStringLiteral("↓ 0 B/s"));
    if (up_lbl_)
        up_lbl_->setText(QStringLiteral("↑ 0 B/s"));
    if (net_meter_)
        net_meter_->start(1000);
    if (elapsed_timer_)
        elapsed_timer_->start();

    if (timeout_timer_)
        timeout_timer_->start();
    python::PythonSetupManager::instance().run_setup();
}

void SetupScreen::on_net_speed(qint64 down_bps, qint64 up_bps) {
    down_bps_ = down_bps;
    up_bps_ = up_bps;
    if (down_lbl_)
        down_lbl_->setText(QStringLiteral("↓ ") + fincept::net::NetSpeedMeter::format_bps(down_bps));
    if (up_lbl_)
        up_lbl_->setText(QStringLiteral("↑ ") + fincept::net::NetSpeedMeter::format_bps(up_bps));
    if (sparkline_)
        sparkline_->push(down_bps);
}

void SetupScreen::on_elapsed_tick() {
    if (setup_started_ms_ == 0)
        return;
    elapsed_secs_ = (QDateTime::currentMSecsSinceEpoch() - setup_started_ms_) / 1000;
    update_elapsed_label();
}

void SetupScreen::on_progress(const python::SetupProgress& progress) {
    const QString& key = progress.step;

    if (steps_.contains(key)) {
        auto& s = steps_[key];

        if (progress.is_error) {
            stop_pulse(key);
            s.bar->setValue(s.pulse_val > 0 ? s.pulse_val : 0);
            s.status_state = StepStatus::Failed;
            update_step_status(key);
        } else if (progress.progress == 0) {
            mark_step_active(key);
        } else if (progress.progress >= 100) {
            mark_step_done(key);
        } else {
            stop_pulse(key);
            s.bar->setValue(progress.progress);
            s.percent = progress.progress;
            s.status_state = StepStatus::Percent;
            update_step_status(key);
        }
    }

    // Plain-English status line — keep raw, since the upstream messages can
    // include identifiers like venv-numpy1 that need substitution.
    QString msg = progress.message;
    msg.replace(QLatin1String("venv-numpy1"), tr("Trading workspace"));
    msg.replace(QLatin1String("venv-numpy2"), tr("Analytics workspace"));
    msg.replace(QLatin1String("requirements-numpy1.txt"), tr("trading library list"));
    msg.replace(QLatin1String("requirements-numpy2.txt"), tr("analytics library list"));
    status_state_ = StatusState::Custom;
    status_detail_ = msg;
    update_status_label();
}

void SetupScreen::on_setup_done(bool success, const QString& error) {
    if (net_meter_)
        net_meter_->stop();
    if (elapsed_timer_)
        elapsed_timer_->stop();

    if (success) {
        LOG_INFO("SetupScreen", "Python setup completed — all steps done");
        status_state_ = StatusState::AllDone;
        status_detail_.clear();
        update_status_label();
        begin_btn_state_ = BeginBtnState::Launch;
        update_begin_button();
        if (timeout_timer_)
            timeout_timer_->stop();
        QTimer::singleShot(1500, this, [this]() { emit setup_complete(); });
    } else {
        for (auto it = steps_.keyBegin(); it != steps_.keyEnd(); ++it)
            stop_pulse(*it);

        begin_btn_->setEnabled(true);
        begin_btn_state_ = BeginBtnState::Retry;
        update_begin_button();
        // Surface the underlying error so users can act on package-name or
        // network hints instead of getting a generic "check your internet".
        status_state_ = StatusState::Failed;
        status_detail_ = error.trimmed().left(240);
        update_status_label();
        status_label_->setWordWrap(true);
        LOG_ERROR("SetupScreen", "Setup failed: " + error);
    }
}

void SetupScreen::on_skip_clicked() {
    LOG_INFO("SetupScreen", "User skipped setup — launching app");
    if (timeout_timer_)
        timeout_timer_->stop();
    if (net_meter_)
        net_meter_->stop();
    if (elapsed_timer_)
        elapsed_timer_->stop();
    for (auto it = steps_.keyBegin(); it != steps_.keyEnd(); ++it)
        stop_pulse(*it);
    emit setup_complete();
}

void SetupScreen::on_setup_timeout() {
    LOG_WARN("SetupScreen", "Setup timed out after 15 minutes — showing skip option");
    status_state_ = StatusState::Timeout;
    update_status_label();
    if (skip_btn_)
        skip_btn_->setVisible(true);
}

void SetupScreen::mark_step_done(const QString& key) {
    if (!steps_.contains(key))
        return;
    stop_pulse(key);
    auto& s = steps_[key];
    s.bar->setValue(100);
    s.status_state = StepStatus::Done;
    update_step_status(key);
}

void SetupScreen::prefill_completed_steps() {
    // check_status() slow path can spawn Python processes (up to ~15s on first run).
    // Run it on a background thread and post results back to the UI thread — P1/P8.
    QPointer<SetupScreen> self = this;
    (void)QtConcurrent::run([self]() {
        const auto status = python::PythonSetupManager::instance().check_status();
        QMetaObject::invokeMethod(self, [self, status]() {
            if (!self)
                return;

            if (status.uv_installed)
                self->mark_step_done("uv");
            if (status.python_installed)
                self->mark_step_done("python");
            if (status.venv_numpy1_created && status.venv_numpy2_created)
                self->mark_step_done("venv");
            if (status.venv_numpy1_ready)
                self->mark_step_done("packages-numpy1");
            if (status.venv_numpy2_ready)
                self->mark_step_done("packages-numpy2");

            const bool any_done = status.uv_installed || status.python_installed ||
                                  status.venv_numpy1_created || status.venv_numpy2_created ||
                                  status.venv_numpy1_ready || status.venv_numpy2_ready;
            const bool all_done = status.uv_installed && status.python_installed &&
                                  status.venv_numpy1_ready && status.venv_numpy2_ready;

            self->summary_lbl_->setText({});

            if (all_done) {
                self->subtitle_state_ = SubtitleState::AlreadyConfigured;
                self->begin_btn_state_ = BeginBtnState::AlreadyComplete;
                self->begin_btn_->setEnabled(false);
                self->status_state_ = StatusState::Custom;
                self->status_detail_ = self->tr("Everything is installed and ready to go.");
            } else if (any_done) {
                self->subtitle_state_ = SubtitleState::Finishing;
                self->begin_btn_state_ = BeginBtnState::Continue;
                self->status_state_ = StatusState::AnyDone;
                self->status_detail_.clear();
            } else {
                self->subtitle_state_ = SubtitleState::Ready;
                self->begin_btn_state_ = BeginBtnState::Begin;
                self->status_state_ = StatusState::Idle;
                self->status_detail_.clear();
            }
            self->update_subtitle();
            self->update_begin_button();
            self->update_status_label();
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
