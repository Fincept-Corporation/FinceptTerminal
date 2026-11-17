// src/ui/notifications/DesktopNotifier.cpp
#include "ui/notifications/DesktopNotifier.h"
#include "ui/theme/Theme.h"

#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "ui/notifications/NotificationService.h" // ToastService
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QScreen>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {

DesktopNotifier& DesktopNotifier::instance() {
    static DesktopNotifier s;
    return s;
}

void DesktopNotifier::init() {
    if (tray_)
        return;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        LOG_INFO("DesktopNotifier", "No system tray available — native notifications disabled");
        return;
    }

    tray_ = new QSystemTrayIcon(this);
    QIcon ico = QApplication::windowIcon();
    if (ico.isNull())
        ico = qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    tray_->setIcon(ico);
    tray_->setToolTip(QStringLiteral("Fincept Terminal"));

    auto* menu = new QMenu();
    menu->addAction(QStringLiteral("Show Fincept"), qApp, []() {
        // Bring up a real terminal window. The old "first top-level widget that
        // isn't a QMenu" walk could land on a toast / tooltip / hidden dialog
        // instead of a WindowFrame.
        const auto frames = WindowRegistry::instance().frames();
        if (!frames.isEmpty()) {
            QWidget* frame = frames.first();
            if (frame->isMinimized())
                frame->showNormal();
            else
                frame->show();
            frame->raise();
            frame->activateWindow();
            return;
        }
        for (auto* tw : qApp->topLevelWidgets()) {
            if (tw->isWindow() && !tw->inherits("QMenu")) {
                tw->showNormal();
                tw->raise();
                tw->activateWindow();
                break;
            }
        }
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);
    tray_->setContextMenu(menu);
    tray_->show();

    // The missing wire: ToastService::posted had no listener, so every alert
    // toast was silent. Mirror each one to a native OS notification.
    connect(&ToastService::instance(), &ToastService::posted, this, [this](const ToastService::Notification& n) {
        const QString title = n.source.startsWith(QStringLiteral("scan:")) ? QStringLiteral("Fincept Alert")
                                                                           : QStringLiteral("Fincept Terminal");
        notify(title, n.message, static_cast<int>(n.severity));
        show_inapp(title, n.message, static_cast<int>(n.severity));
    });

    LOG_DEBUG("DesktopNotifier", "Native desktop notifications enabled");
}

void DesktopNotifier::notify(const QString& title, const QString& body, int severity) {
    if (!tray_ || !QSystemTrayIcon::supportsMessages())
        return;
    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
    switch (static_cast<ToastService::Severity>(severity)) {
        case ToastService::Severity::Warning:
            icon = QSystemTrayIcon::Warning;
            break;
        case ToastService::Severity::Error:
            icon = QSystemTrayIcon::Critical;
            break;
        case ToastService::Severity::Info:
        case ToastService::Severity::Success:
        default:
            icon = QSystemTrayIcon::Information;
            break;
    }
    tray_->showMessage(title, body, icon, 30000);
}

void DesktopNotifier::show_inapp(const QString& title, const QString& body, int severity) {
    // Status colours (never the price-direction ones); the toast is rebuilt
    // from the active theme each time it is shown.
    QString accent = colors::CYAN();
    switch (static_cast<ToastService::Severity>(severity)) {
        case ToastService::Severity::Warning:
            accent = colors::WARNING();
            break;
        case ToastService::Severity::Error:
            accent = colors::DANGER();
            break;
        case ToastService::Severity::Success:
            accent = colors::SUCCESS();
            break;
        default:
            break;
    }
    if (!toast_) {
        toast_ = new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        toast_->setAttribute(Qt::WA_ShowWithoutActivating);
        auto* lay = new QVBoxLayout(toast_);
        lay->setContentsMargins(14, 12, 14, 12);
        toast_lbl_ = new QLabel(toast_);
        toast_lbl_->setTextFormat(Qt::RichText);
        toast_lbl_->setWordWrap(true);
        lay->addWidget(toast_lbl_);
        toast_timer_ = new QTimer(this);
        toast_timer_->setSingleShot(true);
        connect(toast_timer_, &QTimer::timeout, toast_, [this]() {
            if (toast_)
                toast_->hide();
        });
    }
    toast_->setFixedWidth(metrics::width(360)); // widened with the user's font
    toast_->setStyleSheet(
        QString("background:%1; border:1px solid %2; border-radius:8px;").arg(colors::BG_SURFACE(), accent));
    toast_lbl_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; background:transparent; border:none;")
                                  .arg(colors::TEXT_PRIMARY()));
    toast_lbl_->setText(QString("<b style='color:%1; font-size:" + fincept::ui::fonts::px(-1) + "px;'>%2</b><br><span>%3</span>")
                            .arg(accent, title.toHtmlEscaped(), body.toHtmlEscaped()));
    toast_->adjustSize();
    if (auto* scr = QGuiApplication::primaryScreen()) {
        const QRect g = scr->availableGeometry();
        toast_->move(g.right() - toast_->width() - 24, g.top() + 24);
    }
    toast_->show();
    toast_->raise();
    toast_timer_->start(20000); // stay ~20s on screen
}

} // namespace fincept::ui
