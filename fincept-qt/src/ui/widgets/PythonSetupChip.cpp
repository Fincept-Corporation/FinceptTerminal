// src/ui/widgets/PythonSetupChip.cpp
#include "ui/widgets/PythonSetupChip.h"

#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
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
    // Install on demand: a feature needed a library the full set would bring.
    connect(&setup, &python::PythonSetupManager::libraries_missing, this, [this](const QString&, const QString&) {
        full_offer_ = true;
        refresh();
    });
    full_offer_ = !setup.last_missing_module().isEmpty(); // reported before this window existed
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });

    refresh_theme();
    refresh();
}

void PythonSetupChip::refresh_theme() {
    // One stylesheet for the whole chip; state colours via the chipState property.
    setStyleSheet(
        QString("#pySetupChipText { color:%1; background:transparent; font-weight:700; letter-spacing:1px; }"
                "#pySetupChip[chipState=\"failed\"] #pySetupChipText { color:%2; }"
                "#pySetupChip[chipState=\"done\"] #pySetupChipText { color:%3; }"
                "#pySetupChip[chipState=\"offer\"] #pySetupChipText { text-decoration:underline; }"
                "#pySetupChipBar { background:%4; border:1px solid %5; }"
                "#pySetupChipBar::chunk { background:%1; }")
            .arg(colors::AMBER(), colors::NEGATIVE(), colors::POSITIVE(), colors::BG_RAISED(), colors::BORDER_DIM()));
}

void PythonSetupChip::set_mode(Mode mode) {
    if (mode != mode_) {
        mode_ = mode;
        const bool offer = mode == Mode::OfferSetup || mode == Mode::OfferFull;
        const QString state = mode == Mode::Failed ? QStringLiteral("failed")
                              : mode == Mode::Done ? QStringLiteral("done")
                              : offer              ? QStringLiteral("offer")
                                                   : QStringLiteral("running");
        setProperty("chipState", state);
        style()->unpolish(text_);
        style()->polish(text_);
        bar_->setVisible(mode == Mode::Running);
        setCursor(mode == Mode::Failed || offer ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    setVisible(mode != Mode::Hidden);
}

void PythonSetupChip::refresh() {
    using python::SetupStage;
    using python::SetupStageState;
    auto& mgr = python::PythonSetupManager::instance();
    const python::SetupSnapshot snap = mgr.snapshot();
    const bool full_scope = snap.scope == python::SetupScope::Full;

    if (snap.running) {
        set_mode(Mode::Running);
        text_->setText(tr("PYTHON ENV %1%").arg(snap.overall_percent));
        bar_->setValue(snap.overall_percent);

        // Indexed by SetupStage (Runtime, Trading, AnalyticsCore, AnalyticsFull).
        const QString names[] = {tr("RUNTIME"), tr("TRADING LIBRARIES"), tr("ESSENTIAL LIBRARIES"),
                                 tr("FULL LIBRARIES")};
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
                case SetupStageState::Deferred:
                    state = tr("optional, not installed");
                    break;
            }
            QString line = QStringLiteral("%1: %2").arg(names[i], state);
            if (!s.detail.isEmpty() && s.state != SetupStageState::Skipped && s.state != SetupStageState::Deferred)
                line += QStringLiteral(" — ") + s.detail;
            lines << line;
        }
        lines << QString()
              << (full_scope ? tr("Libraries install in the background. Features that need one that is not "
                                  "installed yet start working when setup finishes.")
                             : tr("The essential libraries install in the background. Install the full set any "
                                  "time from Settings → Python Env."));
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
        setToolTip(full_scope ? tr("All Python environments are installed and ready.")
                              : tr("The essential Python libraries are installed. Install the full set any time "
                                   "from Settings → Python Env."));
        QTimer::singleShot(kPyChipDoneVisibleMs, this, [this]() {
            if (mode_ == Mode::Done) {
                set_mode(Mode::Hidden);
                refresh(); // an on-demand offer may be due
            }
        });
        return;
    }
    if (mode_ == Mode::Done)
        return; // the READY flash hides itself

    // Install on demand — nothing running and nothing failed.
    if (!mgr.is_core_ready()) {
        set_mode(Mode::OfferSetup);
        text_->setText(tr("PYTHON ENV · SET UP"));
        setToolTip(tr("Python is not set up yet — market data, charts and analytics need it.\nClick to install "
                      "the essentials in the background (about a minute)."));
        return;
    }
    if (full_offer_ && !mgr.is_full_installed()) {
        set_mode(Mode::OfferFull);
        text_->setText(tr("PYTHON LIBS · INSTALL"));
        setToolTip(tr("A feature needs a Python library that is not installed yet (%1).\nClick to install the full "
                      "library set in the background — you can keep working.")
                       .arg(mgr.last_missing_module()));
        return;
    }
    set_mode(Mode::Hidden);
}

void PythonSetupChip::on_offer_clicked() {
    auto& mgr = python::PythonSetupManager::instance();
    if (mode_ == Mode::OfferSetup) {
        const auto answer = QMessageBox::question(
            window(), tr("Set up Python"),
            tr("Install Python and the essential market-data libraries now?\n\nAbout 100 MB, usually done in about a "
               "minute. It runs in the background — keep working meanwhile."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            mgr.set_background_mode();
            mgr.run_setup();
        }
    } else if (mode_ == Mode::OfferFull) {
        const auto answer = QMessageBox::question(
            window(), tr("Install all Python libraries"),
            tr("Install the full Python library set — AI agents, machine learning, quant and backtesting "
               "libraries?\n\nIt downloads several GB and runs in the background, so you can keep using the "
               "terminal. Progress shows in the status bar."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes)
            mgr.request_full_install();
    }
}

void PythonSetupChip::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (mode_ == Mode::Failed) {
            python::PythonSetupManager::instance().run_setup();
            event->accept();
            return;
        }
        if (mode_ == Mode::OfferSetup || mode_ == Mode::OfferFull) {
            on_offer_clicked();
            event->accept();
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void PythonSetupChip::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        refresh();
    QWidget::changeEvent(event);
}

} // namespace fincept::ui
