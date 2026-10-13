// src/screens/setup/SetupScreen.cpp
#include "screens/setup/SetupScreen.h"

#include "core/logging/Logger.h"
#include "core/net/NetSpeedMeter.h"
#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/LanguageSwitcher.h"
#include "ui/widgets/SpeedSparkline.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace fincept::screens {

namespace {

using python::SetupStage;
using python::SetupStageState;

constexpr int kSetupAutoLaunchDelayMs = 1200;
/// Same weights the manager uses for its overall %, for the pre-run estimate.
constexpr std::array<int, 4> kSetupPrefillWeights{10, 25, 15, 50};

QString setup_mmss(qint64 ms) {
    const qint64 secs = std::max<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2").arg(secs / 60, 2, 10, QLatin1Char('0')).arg(secs % 60, 2, 10, QLatin1Char('0'));
}

const char* setup_state_name(SetupStageState s) {
    switch (s) {
        case SetupStageState::Waiting:
            return "waiting";
        case SetupStageState::Running:
            return "running";
        case SetupStageState::Done:
            return "done";
        case SetupStageState::Failed:
            return "failed";
        case SetupStageState::Skipped:
            return "skipped";
    }
    return "waiting";
}

/// Drives the `[state="…"]` selectors of the screen stylesheet; re-polishes only on change.
void setup_set_state(QWidget* w, const char* state) {
    if (!w || w->property("state").toString() == QLatin1String(state))
        return;
    w->setProperty("state", QString::fromLatin1(state));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("setupRoot"));
    setAttribute(Qt::WA_StyledBackground, true);

    // Filesystem-only status (no process spawned) — the rows render instantly.
    auto& psm = python::PythonSetupManager::instance();
    const python::SetupStatus st = psm.check_status();
    prefill_done_ = {st.uv_installed && st.python_installed, st.venv_numpy1_ready, st.core_ready,
                     st.venv_numpy2_ready};
    anything_installed_ = st.uv_installed || st.python_installed || st.venv_numpy1_created || st.venv_numpy2_created;

    tick_timer_ = new QTimer(this);
    tick_timer_->setInterval(1000);
    connect(tick_timer_, &QTimer::timeout, this, &SetupScreen::render);

    net_meter_ = new fincept::net::NetSpeedMeter(this);
    connect(net_meter_, &fincept::net::NetSpeedMeter::speed_changed, this, &SetupScreen::on_net_speed);

    build_ui();
    refresh_theme();
    retranslateUi();

    connect(&psm, &python::PythonSetupManager::progress_changed, this,
            [this](const python::SetupProgress&) { render(); });
    connect(&psm, &python::PythonSetupManager::core_ready, this, &SetupScreen::render);
    connect(&psm, &python::PythonSetupManager::running_changed, this, [this](bool) {
        render();
        sync_activity();
    });
    connect(&psm, &python::PythonSetupManager::setup_complete, this, &SetupScreen::on_setup_done);
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

void SetupScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 18);
    root->setSpacing(12);

    // Language picker stays available before anything is installed.
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addStretch();
    top->addWidget(new ui::LanguageSwitcher(this));
    root->addLayout(top);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(16);

    // ── Left column: setup ───────────────────────────────────────────────────
    auto* main_panel = new QFrame(this);
    main_panel->setObjectName(QStringLiteral("setupMain"));
    auto* ml = new QVBoxLayout(main_panel);
    ml->setContentsMargins(28, 22, 28, 18);
    ml->setSpacing(0);

    title_lbl_ = new QLabel(main_panel);
    title_lbl_->setObjectName(QStringLiteral("setupTitle"));
    ml->addWidget(title_lbl_);
    ml->addSpacing(6);

    subtitle_lbl_ = new QLabel(main_panel);
    subtitle_lbl_->setObjectName(QStringLiteral("setupSubtitle"));
    subtitle_lbl_->setWordWrap(true);
    ml->addWidget(subtitle_lbl_);
    ml->addSpacing(16);

    auto* overall = new QHBoxLayout;
    overall->setContentsMargins(0, 0, 0, 0);
    overall->setSpacing(12);
    overall_bar_ = new QProgressBar(main_panel);
    overall_bar_->setObjectName(QStringLiteral("setupOverallBar"));
    overall_bar_->setRange(0, 100);
    overall_bar_->setTextVisible(false);
    overall_bar_->setFixedHeight(8);
    overall->addWidget(overall_bar_, 1);
    overall_pct_lbl_ = new QLabel(main_panel);
    overall_pct_lbl_->setObjectName(QStringLiteral("setupOverallPct"));
    overall_pct_lbl_->setMinimumWidth(44);
    overall_pct_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    overall->addWidget(overall_pct_lbl_);
    elapsed_lbl_ = new QLabel(main_panel);
    elapsed_lbl_->setObjectName(QStringLiteral("setupElapsed"));
    elapsed_lbl_->setMinimumWidth(96);
    elapsed_lbl_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    overall->addWidget(elapsed_lbl_);
    ml->addLayout(overall);
    ml->addSpacing(14);

    const auto hairline = [main_panel, ml]() {
        auto* line = new QFrame(main_panel);
        line->setObjectName(QStringLiteral("setupHairline"));
        line->setFixedHeight(1);
        ml->addWidget(line);
    };
    for (int i = 0; i < 4; ++i) {
        hairline();
        ml->addWidget(build_stage_row(main_panel, i));
    }
    hairline();
    ml->addSpacing(10);

    live_row_ = new QWidget(main_panel);
    auto* ll = new QHBoxLayout(live_row_);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(10);
    net_lbl_ = new QLabel(live_row_);
    net_lbl_->setObjectName(QStringLiteral("setupNet"));
    net_lbl_->setMinimumWidth(170);
    ll->addWidget(net_lbl_);
    sparkline_ = new fincept::ui::SpeedSparkline(live_row_);
    sparkline_->setMinimumHeight(20);
    ll->addWidget(sparkline_, 1);
    live_row_->setVisible(false);
    ml->addWidget(live_row_);

    ml->addStretch(1);

    error_box_ = new QPlainTextEdit(main_panel);
    error_box_->setObjectName(QStringLiteral("setupErrorBox"));
    error_box_->setReadOnly(true);
    error_box_->setMaximumHeight(120);
    error_box_->setVisible(false);
    ml->addWidget(error_box_);
    ml->addSpacing(12);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    primary_btn_ = new QPushButton(main_panel);
    primary_btn_->setObjectName(QStringLiteral("setupPrimaryBtn"));
    primary_btn_->setFixedHeight(40);
    primary_btn_->setMinimumWidth(230);
    primary_btn_->setCursor(Qt::PointingHandCursor);
    connect(primary_btn_, &QPushButton::clicked, this, &SetupScreen::on_primary_clicked);
    buttons->addWidget(primary_btn_);
    quit_btn_ = new QPushButton(main_panel);
    quit_btn_->setObjectName(QStringLiteral("setupQuitBtn"));
    quit_btn_->setFixedHeight(40);
    quit_btn_->setMinimumWidth(110);
    quit_btn_->setCursor(Qt::PointingHandCursor);
    connect(quit_btn_, &QPushButton::clicked, this, &QWidget::close);
    buttons->addWidget(quit_btn_);
    buttons->addStretch();
    link_btn_ = new QPushButton(main_panel);
    link_btn_->setObjectName(QStringLiteral("setupLinkBtn"));
    link_btn_->setFlat(true);
    link_btn_->setCursor(Qt::PointingHandCursor);
    link_btn_->setVisible(false);
    connect(link_btn_, &QPushButton::clicked, this, &SetupScreen::on_link_clicked);
    buttons->addWidget(link_btn_);
    ml->addLayout(buttons);
    ml->addSpacing(10);

    note_lbl_ = new QLabel(main_panel);
    note_lbl_->setObjectName(QStringLiteral("setupNote"));
    note_lbl_->setWordWrap(true);
    ml->addWidget(note_lbl_);
    ml->addSpacing(4);

    install_dir_lbl_ = new QLabel(main_panel);
    install_dir_lbl_->setObjectName(QStringLiteral("setupInstallDir"));
    install_dir_lbl_->setWordWrap(true);
    install_dir_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ml->addWidget(install_dir_lbl_);

    body->addWidget(main_panel, 1);

    // ── Right column: Enterprise showcase (draws its own frame) ──────────────
    auto* side = new QWidget(this);
    side->setMinimumWidth(360);
    side->setMaximumWidth(420);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->addWidget(fincept::ui::make_enterprise_showcase(side), 1);
    body->addWidget(side);

    root->addLayout(body, 1);
}

QWidget* SetupScreen::build_stage_row(QWidget* parent, int index) {
    auto* row = new QWidget(parent);
    auto* v = new QVBoxLayout(row);
    v->setContentsMargins(0, 9, 0, 9);
    v->setSpacing(4);

    StageRow& r = rows_[static_cast<std::size_t>(index)];

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(10);
    auto* names = new QVBoxLayout;
    names->setContentsMargins(0, 0, 0, 0);
    names->setSpacing(1);
    r.name = new QLabel(row);
    r.name->setObjectName(QStringLiteral("setupStageName"));
    names->addWidget(r.name);
    r.desc = new QLabel(row);
    r.desc->setObjectName(QStringLiteral("setupStageDesc"));
    r.desc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    names->addWidget(r.desc);
    head->addLayout(names, 1);

    r.status = new QLabel(row);
    r.status->setObjectName(QStringLiteral("setupStageStatus"));
    r.status->setMinimumWidth(86);
    r.status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(r.status, 0, Qt::AlignTop);
    r.elapsed = new QLabel(row);
    r.elapsed->setObjectName(QStringLiteral("setupStageTime"));
    r.elapsed->setMinimumWidth(44);
    r.elapsed->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(r.elapsed, 0, Qt::AlignTop);
    v->addLayout(head);

    r.bar = new QProgressBar(row);
    r.bar->setObjectName(QStringLiteral("setupStageBar"));
    r.bar->setRange(0, 100);
    r.bar->setTextVisible(false);
    r.bar->setFixedHeight(4);
    v->addWidget(r.bar);

    r.detail = new QLabel(row);
    r.detail->setObjectName(QStringLiteral("setupStageDetail"));
    // Ignored width: long uv lines are elided in render() instead of widening the window.
    r.detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    v->addWidget(r.detail);
    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Theme / language
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::refresh_theme() {
    // The whole screen is styled by this one stylesheet (objectName selectors;
    // stage state through the dynamic `state` property).
    QString css = QStringLiteral(R"(
#setupRoot { background: @bg_base; }
QFrame#setupMain { background: @bg_surface; border: 1px solid @border_dim; }
QLabel#setupTitle { color: @amber_main; font-family: @font; font-size: 16px; font-weight: 700; letter-spacing: 2px; background: transparent; border: none; }
QLabel#setupSubtitle { color: @text_secondary; font-family: @font; font-size: 12px; background: transparent; border: none; }
QProgressBar#setupOverallBar { background: @bg_raised; border: 1px solid @border_dim; }
QProgressBar#setupOverallBar::chunk { background: @amber_main; }
QLabel#setupOverallPct { color: @amber_main; font-family: @font; font-size: 14px; font-weight: 700; background: transparent; border: none; }
QLabel#setupElapsed { color: @text_tertiary; font-family: @font; font-size: 10px; letter-spacing: 1px; background: transparent; border: none; }
QFrame#setupHairline { background: @border_dim; border: none; }
QLabel#setupStageName { color: @text_primary; font-family: @font; font-size: 11px; font-weight: 700; letter-spacing: 1.5px; background: transparent; border: none; }
QLabel#setupStageDesc { color: @text_dim; font-family: @font; font-size: 10px; background: transparent; border: none; }
QLabel#setupStageTime { color: @text_tertiary; font-family: @font; font-size: 10px; background: transparent; border: none; }
QLabel#setupStageStatus { color: @text_tertiary; font-family: @font; font-size: 10px; font-weight: 700; letter-spacing: 1px; background: transparent; border: none; }
QLabel#setupStageStatus[state="running"] { color: @amber_main; }
QLabel#setupStageStatus[state="done"], QLabel#setupStageStatus[state="skipped"] { color: @positive; }
QLabel#setupStageStatus[state="failed"] { color: @negative; }
QProgressBar#setupStageBar { background: @bg_raised; border: none; }
QProgressBar#setupStageBar::chunk { background: @amber_main; }
QProgressBar#setupStageBar[state="done"]::chunk, QProgressBar#setupStageBar[state="skipped"]::chunk { background: @positive; }
QProgressBar#setupStageBar[state="failed"]::chunk { background: @negative; }
QLabel#setupStageDetail { color: @text_secondary; font-family: @font; font-size: 11px; background: transparent; border: none; }
QLabel#setupStageDetail[state="failed"] { color: @negative; }
QLabel#setupNet { color: @text_secondary; font-family: @font; font-size: 10px; letter-spacing: 1px; background: transparent; border: none; }
QPlainTextEdit#setupErrorBox { background: @bg_base; color: @text_secondary; border: 1px solid @negative; font-family: @font; font-size: 11px; padding: 6px; }
QPushButton#setupPrimaryBtn { background: @amber_main; color: @on_amber; border: 1px solid @amber_main; font-family: @font; font-size: 13px; font-weight: 700; letter-spacing: 2px; padding: 0px 22px; }
QPushButton#setupPrimaryBtn:hover { background: @amber_dim; border-color: @amber_dim; }
QPushButton#setupPrimaryBtn:disabled { background: @bg_raised; color: @text_tertiary; border: 1px solid @border_med; }
QPushButton#setupQuitBtn { background: transparent; color: @text_secondary; border: 1px solid @border_med; font-family: @font; font-size: 12px; font-weight: 700; letter-spacing: 1.5px; padding: 0px 18px; }
QPushButton#setupQuitBtn:hover { color: @text_primary; border-color: @text_tertiary; }
QPushButton#setupLinkBtn { background: transparent; color: @amber_main; border: none; font-family: @font; font-size: 11px; text-decoration: underline; padding: 0px 4px; }
QPushButton#setupLinkBtn:hover { color: @text_primary; }
QLabel#setupNote { color: @text_tertiary; font-family: @font; font-size: 11px; background: transparent; border: none; }
QLabel#setupInstallDir { color: @text_dim; font-family: @font; font-size: 10px; background: transparent; border: none; }
)");
    // No token is a prefix of another, so plain replace() is safe.
    const std::pair<const char*, QString> tokens[] = {
        {"@bg_base", QString::fromLatin1(ui::colors::BG_BASE())},
        {"@bg_surface", QString::fromLatin1(ui::colors::BG_SURFACE())},
        {"@bg_raised", QString::fromLatin1(ui::colors::BG_RAISED())},
        {"@border_dim", QString::fromLatin1(ui::colors::BORDER_DIM())},
        {"@border_med", QString::fromLatin1(ui::colors::BORDER_MED())},
        {"@amber_main", QString::fromLatin1(ui::colors::AMBER())},
        {"@amber_dim", QString::fromLatin1(ui::colors::AMBER_DIM())},
        {"@on_amber", QString::fromLatin1(ui::colors::TEXT_ON_ACCENT())},
        {"@text_primary", QString::fromLatin1(ui::colors::TEXT_PRIMARY())},
        {"@text_secondary", QString::fromLatin1(ui::colors::TEXT_SECONDARY())},
        {"@text_tertiary", QString::fromLatin1(ui::colors::TEXT_TERTIARY())},
        {"@text_dim", QString::fromLatin1(ui::colors::TEXT_DIM())},
        {"@positive", QString::fromLatin1(ui::colors::POSITIVE())},
        {"@negative", QString::fromLatin1(ui::colors::NEGATIVE())},
        {"@font", QString(ui::fonts::DATA_FAMILY)},
    };
    for (const auto& [token, value] : tokens)
        css.replace(QLatin1String(token), value);
    setStyleSheet(css);

    if (sparkline_) {
        QColor accent(QString::fromLatin1(ui::colors::AMBER()));
        sparkline_->set_line_color(accent);
        accent.setAlpha(48);
        sparkline_->set_fill_color(accent);
    }
}

void SetupScreen::retranslateUi() {
    setWindowTitle(tr("Fincept Terminal — First-Time Setup"));
    title_lbl_->setText(tr("FINCEPT TERMINAL — FIRST-TIME SETUP"));
    quit_btn_->setText(tr("QUIT"));
    install_dir_lbl_->setText(
        tr("Installing to: %1").arg(QDir::toNativeSeparators(python::PythonSetupManager::instance().install_dir())));

    const QString names[] = {tr("RUNTIME"), tr("TRADING LIBRARIES"), tr("ANALYTICS CORE"), tr("ANALYTICS FULL")};
    const QString descs[] = {
        tr("uv package manager + Python 3.11"),
        tr("Backtesting and portfolio tools (NumPy 1.x environment)"),
        tr("Market data, news and research essentials — unlocks LAUNCH"),
        tr("Machine learning, AI agents and quant libraries — finishes in the background"),
    };
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        rows_[i].name->setText(names[i]);
        rows_[i].desc->setText(descs[i]);
        rows_[i].desc->setToolTip(descs[i]);
    }
    render();
}

void SetupScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::render() {
    auto& psm = python::PythonSetupManager::instance();
    const python::SetupSnapshot snap = psm.snapshot();
    const bool has_run = snap.running || snap.finished;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    int overall = 0;
    qint64 last_finish = snap.started_ms;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        StageRow& row = rows_[i];
        SetupStageState state = SetupStageState::Waiting;
        int percent = 0;
        QString detail;
        qint64 started = 0;
        qint64 finished = 0;
        if (has_run) {
            const python::SetupStageInfo& info = snap.stages[i];
            state = info.state;
            percent = info.percent;
            detail = info.detail;
            started = info.started_ms;
            finished = info.finished_ms;
            last_finish = std::max(last_finish, finished);
        } else if (prefill_done_[i]) {
            state = SetupStageState::Skipped;
            percent = 100;
            detail = tr("Already installed");
            overall += kSetupPrefillWeights[i];
        }

        QString status;
        switch (state) {
            case SetupStageState::Waiting:
                status = tr("WAITING");
                break;
            case SetupStageState::Running:
                status = tr("RUNNING");
                break;
            case SetupStageState::Done:
                status = tr("DONE");
                break;
            case SetupStageState::Failed:
                status = tr("FAILED");
                break;
            case SetupStageState::Skipped:
                status = tr("INSTALLED");
                break;
        }
        row.status->setText(status);
        setup_set_state(row.status, setup_state_name(state));
        setup_set_state(row.bar, setup_state_name(state));
        setup_set_state(row.detail, state == SetupStageState::Failed ? "failed" : "normal");

        // Indeterminate only while running without a real fraction (never a fake fill).
        if (state == SetupStageState::Running && percent < 0) {
            if (row.bar->maximum() != 0)
                row.bar->setRange(0, 0);
        } else {
            if (row.bar->maximum() != 100)
                row.bar->setRange(0, 100);
            const bool complete = state == SetupStageState::Done || state == SetupStageState::Skipped;
            row.bar->setValue(complete ? 100 : std::clamp(percent, 0, 100));
        }

        row.elapsed->setText(started > 0 && state != SetupStageState::Waiting
                                 ? setup_mmss((finished > 0 ? finished : now) - started)
                                 : QString());

        const int width = std::max(120, row.detail->width());
        row.detail->setText(row.detail->fontMetrics().elidedText(detail, Qt::ElideRight, width));
        row.detail->setToolTip(detail);
    }

    if (has_run)
        overall = snap.overall_percent;
    overall_bar_->setValue(std::clamp(overall, 0, 100));
    overall_pct_lbl_->setText(QStringLiteral("%1%").arg(overall));
    if (has_run && snap.started_ms > 0)
        elapsed_lbl_->setText(tr("ELAPSED %1").arg(setup_mmss((snap.running ? now : last_finish) - snap.started_ms)));
    else
        elapsed_lbl_->clear();

    live_row_->setVisible(snap.running);
    render_buttons(has_run, snap.running, snap.succeeded, psm.is_core_ready(), snap.error);
}

void SetupScreen::render_buttons(bool has_run, bool running, bool succeeded, bool core_ready, const QString& error) {
    QString primary;
    QString subtitle;
    QString note;
    link_action_ = LinkAction::None;

    if (!has_run) {
        primary_action_ = PrimaryAction::Begin;
        primary = anything_installed_ ? tr("CONTINUE SETUP") : tr("BEGIN SETUP");
        subtitle = anything_installed_
                       ? tr("Part of the Python environment is already in place — only the missing pieces are installed.")
                       : tr("Fincept Terminal installs its own Python runtime and analytics libraries once. Later "
                            "launches start instantly.");
        note = tr("Needs an internet connection. The terminal opens as soon as the analytics core is installed; "
                  "the remaining libraries finish in the background.");
    } else if (running) {
        if (core_ready) {
            primary_action_ = PrimaryAction::Launch;
            primary = tr("LAUNCH TERMINAL");
            subtitle = tr("The analytics core is ready — you can start using the terminal now.");
            note = tr("Remaining analytics libraries keep installing in the background.");
        } else {
            primary_action_ = PrimaryAction::Wait;
            primary = tr("INSTALLING…");
            subtitle = tr("Installing the Python runtime and the analytics core…");
            note = tr("LAUNCH TERMINAL unlocks as soon as the analytics core is installed.");
        }
    } else if (succeeded) {
        primary_action_ = PrimaryAction::Launch;
        primary = tr("LAUNCH TERMINAL");
        subtitle = tr("Everything is installed.");
        note = tr("Launching Fincept Terminal…");
    } else if (core_ready) {
        primary_action_ = PrimaryAction::Launch;
        primary = tr("LAUNCH TERMINAL");
        link_action_ = LinkAction::RetryFailed;
        subtitle = tr("Some libraries could not be installed — the terminal can still launch.");
        note = tr("Retry now, or later from the PYTHON ENV indicator in the status bar.");
    } else {
        primary_action_ = PrimaryAction::Retry;
        primary = tr("RETRY SETUP");
        link_action_ = LinkAction::ContinueLimited;
        subtitle = tr("Setup did not finish — the details are below.");
        note = tr("A retry resumes where it stopped: everything already downloaded is cached.");
    }

    primary_btn_->setText(primary);
    primary_btn_->setEnabled(primary_action_ != PrimaryAction::Wait && !launched_);
    subtitle_lbl_->setText(subtitle);
    note_lbl_->setText(note);

    switch (link_action_) {
        case LinkAction::None:
            break;
        case LinkAction::RetryFailed:
            link_btn_->setText(tr("Retry failed steps"));
            break;
        case LinkAction::ContinueLimited:
            link_btn_->setText(tr("Continue without Python (limited)"));
            break;
    }
    link_btn_->setVisible(link_action_ != LinkAction::None);

    const bool show_error = has_run && !running && !succeeded && !error.isEmpty();
    if (show_error && error_box_->toPlainText() != error)
        error_box_->setPlainText(error);
    error_box_->setVisible(show_error);
}

// ─────────────────────────────────────────────────────────────────────────────
// Activity (timers only while visible AND running)
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::sync_activity() {
    const bool active = isVisible() && !launched_ && python::PythonSetupManager::instance().is_running();
    if (active) {
        if (!tick_timer_->isActive())
            tick_timer_->start();
        if (!net_running_) {
            net_running_ = true;
            net_lbl_->setText(tr("NETWORK ↓ %1").arg(fincept::net::NetSpeedMeter::format_bps(0)));
            net_meter_->start(1000);
        }
    } else {
        tick_timer_->stop();
        if (net_running_) {
            net_running_ = false;
            net_meter_->stop();
        }
    }
}

void SetupScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    sync_activity();
    render();
}

void SetupScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    sync_activity();
}

void SetupScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    render(); // re-elide the stage details for the new width
}

void SetupScreen::on_net_speed(qint64 down_bps, qint64 up_bps) {
    Q_UNUSED(up_bps);
    net_lbl_->setText(tr("NETWORK ↓ %1").arg(fincept::net::NetSpeedMeter::format_bps(down_bps)));
    sparkline_->push(down_bps);
}

// ─────────────────────────────────────────────────────────────────────────────
// Actions
// ─────────────────────────────────────────────────────────────────────────────

void SetupScreen::on_primary_clicked() {
    switch (primary_action_) {
        case PrimaryAction::Begin:
        case PrimaryAction::Retry:
            start_setup();
            break;
        case PrimaryAction::Launch:
            launch();
            break;
        case PrimaryAction::Wait:
            break;
    }
}

void SetupScreen::on_link_clicked() {
    switch (link_action_) {
        case LinkAction::RetryFailed:
            start_setup();
            break;
        case LinkAction::ContinueLimited:
            LOG_WARN("SetupScreen", "User continued without a complete Python environment");
            launch();
            break;
        case LinkAction::None:
            break;
    }
}

void SetupScreen::start_setup() {
    auto& psm = python::PythonSetupManager::instance();
    if (psm.is_running())
        return; // the manager ignores re-entry too; this keeps the UI from double-starting
    LOG_INFO("SetupScreen", "Starting Python environment setup");
    error_box_->clear();
    psm.run_setup();
    render();
    sync_activity();
}

void SetupScreen::launch() {
    if (launched_)
        return;
    launched_ = true;
    sync_activity();
    primary_btn_->setEnabled(false);
    LOG_INFO("SetupScreen", python::PythonSetupManager::instance().is_running()
                                ? "Launching the terminal — remaining installs continue in the background"
                                : "Launching the terminal");
    emit launch_requested();
}

void SetupScreen::on_setup_done(bool success, const QString& error) {
    render();
    sync_activity();
    if (success) {
        if (!launched_ && !auto_launch_scheduled_) {
            auto_launch_scheduled_ = true;
            QTimer::singleShot(kSetupAutoLaunchDelayMs, this, [this]() {
                if (!launched_)
                    launch();
            });
        }
    } else {
        LOG_WARN("SetupScreen", "Setup ended without success: " + error.left(500));
    }
}

void SetupScreen::closeEvent(QCloseEvent* event) {
    if (launched_) {
        QWidget::closeEvent(event);
        return;
    }
    // Before the hand-off this window IS the application (quit-on-last-window is
    // off app-wide), so closing it must quit — never leave an invisible process.
    auto& psm = python::PythonSetupManager::instance();
    if (psm.is_running()) {
        const auto answer =
            QMessageBox::question(this, tr("Quit Fincept Terminal"), tr("Setup is still running. Quit Fincept Terminal?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        psm.cancel();
    }
    event->accept();
    QCoreApplication::quit();
}

} // namespace fincept::screens
