#include "screens/launchpad/LaunchpadScreen.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/config/ProfileManager.h"
#include "core/keys/WindowCycler.h"
#include "core/layout/LayoutCatalog.h"
#include "core/logging/Logger.h"

#include <QApplication>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

namespace {
constexpr const char* kLaunchpadTag = "Launchpad";
} // namespace

LaunchpadScreen* LaunchpadScreen::instance() {
    // Lazy singleton; lives until process exit. closeEvent calls
    // QCoreApplication::quit() so we don't need WA_DeleteOnClose.
    static LaunchpadScreen* s = new LaunchpadScreen(nullptr);
    return s;
}

LaunchpadScreen::LaunchpadScreen(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Fincept Launchpad");
    setMinimumSize(480, 320);
    resize(480, 320);

    auto* central = new QWidget(this);
    auto* vl = new QVBoxLayout(central);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(16);

    const QString profile = ProfileManager::instance().active();
    greeting_ = new QLabel(QString("Profile: %1").arg(profile));
    greeting_->setStyleSheet("font-size: 14px; color: #d97706; font-weight: 600;");
    vl->addWidget(greeting_);

    auto* sub = new QLabel(
        "All windows closed. Open a new window or pick a layout below.");
    sub->setStyleSheet("font-size: 11px; color: #9ca3af;");
    sub->setWordWrap(true);
    vl->addWidget(sub);

    btn_new_window_ = new QPushButton("New Window");
    btn_new_window_->setMinimumHeight(36);
    connect(btn_new_window_, &QPushButton::clicked, this, &LaunchpadScreen::on_new_window);
    vl->addWidget(btn_new_window_);

    btn_open_layout_ = new QPushButton("Open Saved Layout…");
    btn_open_layout_->setMinimumHeight(36);
    btn_open_layout_->setEnabled(false); // Phase 6 wires LayoutCatalog
    btn_open_layout_->setToolTip("Saved layouts arrive in Phase 6");
    connect(btn_open_layout_, &QPushButton::clicked, this, &LaunchpadScreen::on_open_layout);
    vl->addWidget(btn_open_layout_);

    btn_switch_profile_ = new QPushButton("Switch Profile…");
    btn_switch_profile_->setMinimumHeight(36);
    connect(btn_switch_profile_, &QPushButton::clicked, this, &LaunchpadScreen::on_switch_profile);
    vl->addWidget(btn_switch_profile_);

    auto* recent_label = new QLabel("Recent Layouts");
    recent_label->setStyleSheet("font-size: 11px; color: #9ca3af; margin-top: 8px;");
    vl->addWidget(recent_label);

    recent_layouts_ = new QListWidget;
    recent_layouts_->setStyleSheet(
        "QListWidget { background: #111827; border: 1px solid #374151; color: #e5e7eb; }"
        "QListWidget::item { padding: 6px 8px; }"
        "QListWidget::item:hover { background: #1f2937; }"
        "QListWidget::item:selected { background: #1f2937; color: #d97706; }");
    vl->addWidget(recent_layouts_, /*stretch=*/1);
    refresh_recent_layouts();

    setCentralWidget(central);

    // Centre on the primary screen at construction so the user always
    // sees the launchpad in the middle of their main display.
    if (auto* primary = QGuiApplication::primaryScreen()) {
        const QRect avail = primary->availableGeometry();
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }
}

void LaunchpadScreen::surface() {
    // Profile may have changed since construction (Switch Profile relaunches
    // a new process today, but this keeps the label honest if that ever
    // becomes in-process). Cheap to refresh on every surface.
    if (greeting_)
        greeting_->setText(QString("Profile: %1").arg(ProfileManager::instance().active()));

    if (isVisible() && !isMinimized()) {
        raise();
        activateWindow();
        return;
    }
    showNormal();
    raise();
    activateWindow();
    refresh_recent_layouts();
    LOG_INFO(kLaunchpadTag, "Surfaced");
}

void LaunchpadScreen::closeEvent(QCloseEvent* event) {
    // Reaching closeEvent always means the OS sent a window-close (X button
    // / Alt-F4 / system menu). Programmatic dismissals from button handlers
    // use hide() and never enter here. Treat every close as a user quit so
    // the app actually exits — otherwise lastWindowClosed re-surfaces this
    // window in an unkillable loop.
    LOG_INFO(kLaunchpadTag, "User closed Launchpad — quitting");
    event->accept();
    QCoreApplication::quit();
}

void LaunchpadScreen::on_new_window() {
    // Spawn a fresh WindowFrame. Reuses WindowCycler's smart-monitor
    // placement so the window lands somewhere visible.
    LOG_INFO(kLaunchpadTag, "New Window button clicked");
    hide();
    WindowCycler::instance().new_window_on_next_monitor();
}

void LaunchpadScreen::on_switch_profile() {
    // Show known profiles + a "Create new…" entry. Picking an existing
    // profile relaunches with --profile <name>. Picking the create entry
    // falls through to a text prompt.
    LOG_INFO(kLaunchpadTag, "Switch Profile button clicked");

    static const QString kCreateNew = QStringLiteral("+ Create new profile…");
    QStringList items = ProfileManager::instance().list_profiles();
    items.removeDuplicates();
    items.append(kCreateNew);

    const QString current = ProfileManager::instance().active();
    int current_idx = items.indexOf(current);
    if (current_idx < 0) current_idx = 0;

    bool ok = false;
    const QString picked = QInputDialog::getItem(
        this, "Switch Profile", "Pick a profile or create one:",
        items, current_idx, /*editable=*/false, &ok);
    if (!ok)
        return;

    QString target = picked;
    if (picked == kCreateNew) {
        bool name_ok = false;
        target = QInputDialog::getText(
            this, "Create Profile", "New profile name:",
            QLineEdit::Normal, QString(), &name_ok).trimmed();
        if (!name_ok || target.isEmpty())
            return;
    }

    if (target == current) {
        LOG_INFO(kLaunchpadTag, "Switch Profile: target equals current — no-op");
        return;
    }

    // Process-level switch today (set_active + relaunch). In-process switch
    // lands with Phase 1b's auth lift.
    const QString exe = QCoreApplication::applicationFilePath();
    QProcess::startDetached(exe, {"--profile", target});
    QCoreApplication::quit();
}

void LaunchpadScreen::on_open_layout() {
    if (!recent_layouts_)
        return;
    auto* item = recent_layouts_->currentItem();
    if (!item) {
        LOG_INFO(kLaunchpadTag, "Open Layout: no item selected");
        return;
    }
    const QString id_str = item->data(Qt::UserRole).toString();
    if (id_str.isEmpty())
        return;

    LayoutId id = LayoutId::from_string(id_str);
    auto r = LayoutCatalog::instance().load_workspace(id);
    if (r.is_err()) {
        LOG_WARN(kLaunchpadTag,
                 QString("Open Layout failed: %1").arg(QString::fromStdString(r.error())));
        return;
    }
    LOG_INFO(kLaunchpadTag,
             QString("Loaded layout '%1'. Spawning a frame; full restore arrives with Phase 6 capture_layout.")
                 .arg(r.value().name));
    // For v1 we just spawn a fresh frame — full FrameLayout restoration
    // (which requires WindowFrame::apply_layout) is the deeper Phase 6
    // follow-up. The user gets a window; the catalogue entry confirms.
    hide();
    WindowCycler::instance().new_window_on_next_monitor();
}

void LaunchpadScreen::refresh_recent_layouts() {
    if (!recent_layouts_)
        return;
    recent_layouts_->clear();

    // Phase 6: pull from LayoutCatalog. include_auto=false so the
    // continuously-saved "auto" workspace doesn't clutter the list — the
    // user wants to see saved/named layouts here, not the snapshot.
    auto r = LayoutCatalog::instance().recent_layouts(/*limit=*/8, /*include_auto=*/false);
    if (r.is_err() || r.value().isEmpty()) {
        auto* item = new QListWidgetItem("(No saved layouts yet — use 'layout save \"<name>\"' to save the current state)");
        item->setFlags(Qt::NoItemFlags);
        recent_layouts_->addItem(item);
        // Enable the "Open Saved Layout" button if there are any layouts
        // (it's just empty); disabled when none exist.
        if (btn_open_layout_)
            btn_open_layout_->setEnabled(false);
        return;
    }

    for (const auto& e : r.value()) {
        auto* item = new QListWidgetItem(QString("%1  (%2)").arg(e.name, e.kind));
        item->setData(Qt::UserRole, e.id.to_string());
        recent_layouts_->addItem(item);
    }
    if (btn_open_layout_)
        btn_open_layout_->setEnabled(!r.value().isEmpty());
}

} // namespace fincept::screens
