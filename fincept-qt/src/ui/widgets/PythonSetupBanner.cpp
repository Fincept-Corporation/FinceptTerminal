// src/ui/widgets/PythonSetupBanner.cpp
#include "ui/widgets/PythonSetupBanner.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

namespace fincept::ui {

PythonSetupBanner::PythonSetupBanner(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("pySetupBanner"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAccessibleName(tr("Python setup notice"));

    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(12, 6, 8, 6);
    hl->setSpacing(12);

    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("pySetupBannerTitle"));
    hl->addWidget(title_);

    body_ = new QLabel(this);
    body_->setObjectName(QStringLiteral("pySetupBannerBody"));
    body_->setWordWrap(true);
    hl->addWidget(body_, 1);

    action_btn_ = new QPushButton(this);
    action_btn_->setObjectName(QStringLiteral("pySetupBannerAction"));
    action_btn_->setCursor(Qt::PointingHandCursor);
    hl->addWidget(action_btn_);

    close_btn_ = new QPushButton(QString(QChar(0x2715)), this);
    close_btn_->setObjectName(QStringLiteral("pySetupBannerClose"));
    close_btn_->setCursor(Qt::PointingHandCursor);
    hl->addWidget(close_btn_);

    connect(action_btn_, &QPushButton::clicked, this, &PythonSetupBanner::on_action_clicked);
    connect(close_btn_, &QPushButton::clicked, this, [this]() {
        dismissed_ = true;
        LOG_INFO("PythonSetupBanner", "Python setup notice dismissed for this session");
        refresh();
    });

    auto& setup = python::PythonSetupManager::instance();
    connect(&setup, &python::PythonSetupManager::progress_changed, this,
            [this](const python::SetupProgress&) { refresh(); });
    connect(&setup, &python::PythonSetupManager::running_changed, this, [this](bool) { refresh(); });
    connect(&setup, &python::PythonSetupManager::core_ready, this, [this]() { refresh(); });
    connect(&setup, &python::PythonSetupManager::setup_complete, this, [this](bool success, const QString&) {
        // A new failure while the essentials are still missing is worth showing again.
        if (!success && !python::PythonSetupManager::instance().is_core_ready())
            dismissed_ = false;
        refresh();
    });
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });

    refresh_theme();
    refresh();
}

void PythonSetupBanner::refresh_theme() {
    // One stylesheet for the whole strip; state colours via the bannerState property.
    const QString frame =
        QStringLiteral("#pySetupBanner { background:%1; border:none; border-bottom:1px solid %2;"
                       " border-left:3px solid %3; }"
                       "#pySetupBanner[bannerState=\"failed\"] { background:%4; border-left:3px solid %5; }"
                       "#pySetupBannerTitle { color:%3; background:transparent; font-weight:700;"
                       " letter-spacing:1px; }"
                       "#pySetupBanner[bannerState=\"failed\"] #pySetupBannerTitle { color:%5; }")
            .arg(colors::ACCENT_BG(), colors::BORDER_DIM(), colors::AMBER(), colors::NEGATIVE_BG(), colors::NEGATIVE());
    const QString controls =
        QStringLiteral("#pySetupBannerBody { color:%1; background:transparent; }"
                       "#pySetupBannerAction { background:%2; color:%3; border:none; padding:4px 12px;"
                       " font-weight:700; }"
                       "#pySetupBannerAction:hover { background:%4; }"
                       "#pySetupBannerClose { background:transparent; color:%5; border:none; padding:2px 6px; }"
                       "#pySetupBannerClose:hover { color:%6; }")
            .arg(colors::TEXT_SECONDARY(), colors::AMBER(), colors::TEXT_ON_ACCENT(), colors::AMBER_DIM(),
                 colors::TEXT_TERTIARY(), colors::TEXT_PRIMARY());
    setStyleSheet(frame + controls);
}

void PythonSetupBanner::set_mode(Mode mode) {
    if (mode != mode_) {
        mode_ = mode;
        setProperty("bannerState", mode == Mode::Failed ? QStringLiteral("failed") : QStringLiteral("normal"));
        for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(title_)}) {
            style()->unpolish(w);
            style()->polish(w);
        }
        action_btn_->setVisible(mode == Mode::Offer || mode == Mode::Failed);
    }
    setVisible(mode != Mode::Hidden);
}

void PythonSetupBanner::refresh() {
    auto& mgr = python::PythonSetupManager::instance();
    if (mgr.is_core_ready()) {
        set_mode(Mode::Hidden);
        return;
    }
    if (dismissed_) {
        set_mode(Mode::Hidden);
        return;
    }

    const python::SetupSnapshot snap = mgr.snapshot();
    close_btn_->setToolTip(tr("Hide this notice until the next start"));
    close_btn_->setAccessibleName(tr("Dismiss Python setup notice"));

    if (snap.running) {
        set_mode(Mode::Running);
        title_->setText(tr("INSTALLING PYTHON ESSENTIALS %1%").arg(snap.overall_percent));
        body_->setText(tr("Market data, charts and analytics start loading as soon as this finishes — keep "
                          "working meanwhile."));
        body_->setToolTip(QString());
        return;
    }

    if (snap.finished && !snap.succeeded && !snap.cancelled) {
        set_mode(Mode::Failed);
        title_->setText(tr("PYTHON SETUP FAILED"));
        body_->setText(tr("Market data, charts and analytics stay unavailable until the essential Python "
                          "libraries are installed. Retry — finished downloads are cached."));
        body_->setToolTip(snap.error.left(1500));
        action_btn_->setText(tr("RETRY"));
        return;
    }

    set_mode(Mode::Offer);
    title_->setText(tr("PYTHON NOT SET UP"));
    body_->setText(tr("Market data, charts and analytics won't load until the essential Python libraries are "
                      "installed — about 100 MB, usually a minute. It runs in the background."));
    body_->setToolTip(QString());
    action_btn_->setText(tr("INSTALL ESSENTIALS"));
}

void PythonSetupBanner::on_action_clicked() {
    auto& mgr = python::PythonSetupManager::instance();
    if (mgr.is_running())
        return;
    LOG_INFO("PythonSetupBanner",
             mode_ == Mode::Failed ? "Retrying Python setup from the banner" : "Starting Python setup from the banner");
    mgr.set_background_mode();
    mgr.run_setup();
    refresh();
}

void PythonSetupBanner::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        refresh();
    QWidget::changeEvent(event);
}

} // namespace fincept::ui
