// src/ui/widgets/PythonSetupChip.cpp
#include "ui/widgets/PythonSetupChip.h"

#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QStyle>
#include <QTimer>

#include <algorithm>

namespace fincept::ui {

namespace {

constexpr int kPyChipDoneVisibleMs = 6000;

QString pychip_elapsed(qint64 started_ms, qint64 finished_ms) {
    if (started_ms <= 0)
        return {};
    const qint64 end = finished_ms > 0 ? finished_ms : QDateTime::currentMSecsSinceEpoch();
    const qint64 secs = std::max<qint64>(0, (end - started_ms) / 1000);
    return QStringLiteral("%1:%2").arg(secs / 60, 2, 10, QLatin1Char('0')).arg(secs % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

PythonSetupChip::PythonSetupChip(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("pySetupChip"));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(6);

    text_ = new QLabel(this);
    text_->setObjectName(QStringLiteral("pySetupChipText"));
    hl->addWidget(text_);

    bar_ = new QProgressBar(this);
    bar_->setObjectName(QStringLiteral("pySetupChipBar"));
    bar_->setRange(0, 100);
    bar_->setTextVisible(false);
    bar_->setFixedSize(56, 4);
    hl->addWidget(bar_, 0, Qt::AlignVCenter);

    auto& setup = python::PythonSetupManager::instance();
    connect(&setup, &python::PythonSetupManager::progress_changed, this,
            [this](const python::SetupProgress&) { refresh(); });
    connect(&setup, &python::PythonSetupManager::running_changed, this, [this](bool) { refresh(); });
    connect(&setup, &python::PythonSetupManager::setup_complete, this, [this](bool, const QString&) { refresh(); });
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });

    refresh_theme();
    refresh();
}

void PythonSetupChip::refresh_theme() {
    // One stylesheet for the whole chip; state colours via the chipState property.
    setStyleSheet(QString("#pySetupChipText { color:%1; background:transparent; font-weight:700; letter-spacing:1px; }"
                          "#pySetupChip[chipState=\"failed\"] #pySetupChipText { color:%2; }"
                          "#pySetupChip[chipState=\"done\"] #pySetupChipText { color:%3; }"
                          "#pySetupChipBar { background:%4; border:1px solid %5; }"
                          "#pySetupChipBar::chunk { background:%1; }")
                      .arg(colors::AMBER(), colors::NEGATIVE(), colors::POSITIVE(), colors::BG_RAISED(),
                           colors::BORDER_DIM()));
}

void PythonSetupChip::set_mode(Mode mode) {
    if (mode != mode_) {
        mode_ = mode;
        const QString state = mode == Mode::Failed ? QStringLiteral("failed")
                              : mode == Mode::Done ? QStringLiteral("done")
                                                   : QStringLiteral("running");
        setProperty("chipState", state);
        style()->unpolish(text_);
        style()->polish(text_);
        bar_->setVisible(mode == Mode::Running);
        setCursor(mode == Mode::Failed ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    setVisible(mode != Mode::Hidden);
}

void PythonSetupChip::refresh() {
    using python::SetupStage;
    using python::SetupStageState;
    const python::SetupSnapshot snap = python::PythonSetupManager::instance().snapshot();

    if (snap.running) {
        set_mode(Mode::Running);
        text_->setText(tr("PYTHON ENV %1%").arg(snap.overall_percent));
        bar_->setValue(snap.overall_percent);

        const QString names[] = {tr("RUNTIME"), tr("TRADING LIBRARIES"), tr("ANALYTICS CORE"), tr("ANALYTICS FULL")};
        QStringList lines;
        lines << tr("Python environment setup — %1%").arg(snap.overall_percent);
        for (int i = 0; i < python::kSetupStageCount; ++i) {
            const python::SetupStageInfo& s = snap.stage(static_cast<SetupStage>(i));
            QString state;
            switch (s.state) {
                case SetupStageState::Waiting:
                    state = tr("waiting");
                    break;
                case SetupStageState::Running:
                    state = tr("running %1").arg(pychip_elapsed(s.started_ms, 0));
                    break;
                case SetupStageState::Done:
                    state = tr("done");
                    break;
                case SetupStageState::Failed:
                    state = tr("failed");
                    break;
                case SetupStageState::Skipped:
                    state = tr("installed");
                    break;
            }
            QString line = QStringLiteral("%1: %2").arg(names[i], state);
            if (!s.detail.isEmpty() && s.state != SetupStageState::Skipped)
                line += QStringLiteral(" — ") + s.detail;
            lines << line;
        }
        lines << QString() << tr("Libraries install in the background. Features that need one that is not "
                                 "installed yet start working when setup finishes.");
        setToolTip(lines.join(QLatin1Char('\n')));
        return;
    }

    if (snap.finished && !snap.succeeded && !snap.cancelled) {
        set_mode(Mode::Failed);
        text_->setText(tr("PYTHON ENV FAILED · RETRY"));
        setToolTip(tr("Installing the Python environment failed — click to retry. Only the failed parts run "
                      "again; downloaded packages are cached.\n\n%1")
                       .arg(snap.error.left(1500)));
        return;
    }

    if (snap.finished && snap.succeeded && mode_ == Mode::Running) {
        set_mode(Mode::Done);
        text_->setText(tr("PYTHON ENV READY"));
        bar_->setValue(100);
        setToolTip(tr("All Python environments are installed and ready."));
        QTimer::singleShot(kPyChipDoneVisibleMs, this, [this]() {
            if (mode_ == Mode::Done)
                set_mode(Mode::Hidden);
        });
        return;
    }

    if (mode_ != Mode::Done)
        set_mode(Mode::Hidden);
}

void PythonSetupChip::mouseReleaseEvent(QMouseEvent* event) {
    if (mode_ == Mode::Failed && event->button() == Qt::LeftButton) {
        python::PythonSetupManager::instance().run_setup();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PythonSetupChip::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        refresh();
    QWidget::changeEvent(event);
}

} // namespace fincept::ui
