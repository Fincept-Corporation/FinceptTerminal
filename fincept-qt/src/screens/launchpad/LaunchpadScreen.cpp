#include "screens/launchpad/LaunchpadScreen.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "storage/workspace/WorkspaceDb.h"
#include "core/config/ProfileManager.h"
#include "core/keys/WindowCycler.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/LayoutTemplates.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/profile/ProfilePaths.h"
#include "core/window/WindowRegistry.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeyEvent>
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

    // "Continue from last session" — the most-common user intent on relaunch.
    // One click restores the most-recent saved or auto-snapshot layout.
    // Hidden when nothing restorable exists (first run / fresh profile).
    btn_continue_ = new QPushButton("Continue from last session");
    btn_continue_->setMinimumHeight(48);
    btn_continue_->setObjectName("launchpadContinueBtn");
    btn_continue_->setStyleSheet(
        "QPushButton#launchpadContinueBtn {"
        "  background: #d97706; color: #fff;"
        "  font-weight: 700; font-size: 13px;"
        "  border: none; border-radius: 4px;"
        "}"
        "QPushButton#launchpadContinueBtn:hover { background: #b45309; }");
    connect(btn_continue_, &QPushButton::clicked, this, &LaunchpadScreen::on_continue);
    vl->addWidget(btn_continue_);

    btn_new_window_ = new QPushButton("New Window");
    btn_new_window_->setMinimumHeight(36);
    connect(btn_new_window_, &QPushButton::clicked, this, &LaunchpadScreen::on_new_window);
    vl->addWidget(btn_new_window_);

    btn_open_layout_ = new QPushButton("Open Saved Layout…");
    btn_open_layout_->setMinimumHeight(36);
    // Enabled state is driven by refresh_recent_layouts() based on whether
    // any saved layouts exist.
    btn_open_layout_->setEnabled(false);
    connect(btn_open_layout_, &QPushButton::clicked, this, &LaunchpadScreen::on_open_layout);
    vl->addWidget(btn_open_layout_);

    btn_switch_profile_ = new QPushButton("Switch Profile…");
    btn_switch_profile_->setMinimumHeight(36);
    connect(btn_switch_profile_, &QPushButton::clicked, this, &LaunchpadScreen::on_switch_profile);
    vl->addWidget(btn_switch_profile_);

    recent_label_ = new QLabel("Recent Layouts");
    recent_label_->setStyleSheet("font-size: 11px; color: #9ca3af; margin-top: 8px;");
    vl->addWidget(recent_label_);

    // Phase L6: keyboard-first filter. Up/Down/Enter/Esc are intercepted
    // by eventFilter() and forwarded to recent_layouts_ / on_open_layout /
    // close so the user can keep their hands on the keyboard.
    filter_edit_ = new QLineEdit;
    filter_edit_->setPlaceholderText("Type to filter layouts…");
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setStyleSheet(
        "QLineEdit { background: #111827; border: 1px solid #374151;"
        "            color: #e5e7eb; padding: 4px 6px; }"
        "QLineEdit:focus { border-color: #d97706; }");
    filter_edit_->installEventFilter(this);
    connect(filter_edit_, &QLineEdit::textChanged, this, [this](const QString& q) {
        if (!recent_layouts_) return;
        for (int i = 0; i < recent_layouts_->count(); ++i) {
            auto* it = recent_layouts_->item(i);
            const bool match = q.isEmpty() ||
                               it->text().contains(q, Qt::CaseInsensitive);
            it->setHidden(!match);
        }
        // Auto-select the first visible match so Enter Just Works.
        for (int i = 0; i < recent_layouts_->count(); ++i) {
            if (!recent_layouts_->item(i)->isHidden()) {
                recent_layouts_->setCurrentRow(i);
                break;
            }
        }
    });
    vl->addWidget(filter_edit_);

    recent_layouts_ = new QListWidget;
    recent_layouts_->setStyleSheet(
        "QListWidget { background: #111827; border: 1px solid #374151; color: #e5e7eb; }"
        "QListWidget::item { padding: 6px 8px; }"
        "QListWidget::item:hover { background: #1f2937; }"
        "QListWidget::item:selected { background: #1f2937; color: #d97706; }");
    vl->addWidget(recent_layouts_, /*stretch=*/1);
    refresh_recent_layouts();

    // First-run template picker — 5 persona cards in a 3×2 grid (last cell empty).
    // Hidden by default; refresh_first_run_picker() shows it when no layouts
    // and no auto snapshot exist.
    template_picker_ = new QWidget;
    template_picker_->setVisible(false);
    auto* picker_root = new QVBoxLayout(template_picker_);
    picker_root->setContentsMargins(0, 0, 0, 0);
    picker_root->setSpacing(8);

    auto* picker_label = new QLabel("Pick a starting template:");
    picker_label->setStyleSheet("font-size: 11px; color: #9ca3af; margin-top: 8px;");
    picker_root->addWidget(picker_label);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    const auto personas = layout::LayoutTemplates::personas();
    for (int i = 0; i < personas.size(); ++i) {
        const auto& p = personas[i];
        auto* card = new QPushButton;
        card->setText(QString("%1\n\n%2").arg(p.display_name, p.description));
        card->setMinimumHeight(80);
        card->setStyleSheet(
            "QPushButton { background: #111827; border: 1px solid #374151;"
            "             color: #e5e7eb; padding: 10px; text-align: left;"
            "             font-size: 12px; font-weight: 600; }"
            "QPushButton:hover { border-color: #d97706; color: #d97706; }");
        const QString persona_id = p.id;
        connect(card, &QPushButton::clicked, this,
                [this, persona_id]() { on_template_picked(persona_id); });
        grid->addWidget(card, i / 2, i % 2);
    }
    picker_root->addLayout(grid);
    picker_root->addStretch();

    vl->addWidget(template_picker_, /*stretch=*/1);

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
    refresh_continue_visibility();
    refresh_first_run_picker();
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
    const layout::Workspace ws = r.value();
    LOG_INFO(kLaunchpadTag, QString("Opening layout '%1'").arg(ws.name));

    hide();
    // Defer apply via queued connection so the spawned WindowFrame is fully
    // materialised before WorkspaceShell::apply walks WindowRegistry. If
    // there are no frames yet (Launchpad-only state), spawn one first.
    QMetaObject::invokeMethod(this, [ws]() {
        if (fincept::WindowRegistry::instance().frames().isEmpty()) {
            auto* fr = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
            fr->setAttribute(Qt::WA_DeleteOnClose);
            fr->show();
        }
        layout::WorkspaceShell::apply(ws);
    }, Qt::QueuedConnection);
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

    // Phase L5: show thumbnail icons next to each layout name.
    recent_layouts_->setIconSize(QSize(64, 36));
    const QString layouts_dir = ProfilePaths::layouts_dir();
    for (const auto& e : r.value()) {
        auto* item = new QListWidgetItem(QString("%1  (%2)").arg(e.name, e.kind));
        item->setData(Qt::UserRole, e.id.to_string());
        if (!e.thumbnail_path.isEmpty()) {
            const QString abs = layouts_dir + "/" + e.thumbnail_path;
            if (QFileInfo::exists(abs))
                item->setIcon(QIcon(abs));
        }
        recent_layouts_->addItem(item);
    }
    if (btn_open_layout_)
        btn_open_layout_->setEnabled(!r.value().isEmpty());
}

void LaunchpadScreen::on_continue() {
    LOG_INFO(kLaunchpadTag, "Continue from last session clicked");
    hide();
    // Defer the actual restore so the Launchpad's hide animation completes
    // first and so any frame WorkspaceShell::apply spawns lands cleanly on
    // the event loop.
    QMetaObject::invokeMethod(this, []() {
        const int n = layout::WorkspaceShell::load_last_or_default();
        if (n == 0) {
            // Fallback: nothing was restored (bad state, deleted files).
            // Spawn an empty frame so the user isn't left with no window.
            auto* w = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
        }
    }, Qt::QueuedConnection);
}

void LaunchpadScreen::on_template_picked(const QString& persona_id) {
    LOG_INFO(kLaunchpadTag, QString("Template picked: %1").arg(persona_id));
    layout::Workspace ws = layout::LayoutTemplates::make(persona_id);
    auto sr = LayoutCatalog::instance().save_workspace(ws);
    if (sr.is_err()) {
        LOG_WARN(kLaunchpadTag,
                 QString("Failed to save template '%1': %2")
                     .arg(persona_id, QString::fromStdString(sr.error())));
        return;
    }
    // save_workspace mints a fresh id if one wasn't set; pull the saved id
    // so the WorkspaceShell::apply path pins the right layout.
    ws.id = sr.value();

    hide();
    QMetaObject::invokeMethod(this, [ws]() {
        if (fincept::WindowRegistry::instance().frames().isEmpty()) {
            auto* fr = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
            fr->setAttribute(Qt::WA_DeleteOnClose);
            fr->show();
        }
        layout::WorkspaceShell::apply(ws);
    }, Qt::QueuedConnection);
}

void LaunchpadScreen::refresh_first_run_picker() {
    if (!template_picker_) return;

    // Show the picker iff the user has nothing to come back to:
    //   - no saved/builtin layouts in the catalog, AND
    //   - no auto snapshot from a prior session
    bool has_anything = false;
    auto layouts = LayoutCatalog::instance().recent_layouts(/*limit=*/1, /*include_auto=*/false);
    if (layouts.is_ok() && !layouts.value().isEmpty())
        has_anything = true;

    if (!has_anything) {
        if (auto* ring = TerminalShell::instance().snapshot_ring()) {
            auto snaps = ring->latest_auto(/*limit=*/1);
            if (snaps.is_ok() && !snaps.value().isEmpty())
                has_anything = true;
        }
    }

    const bool first_run = !has_anything;
    template_picker_->setVisible(first_run);
    if (recent_layouts_) recent_layouts_->setVisible(!first_run);
    if (recent_label_)   recent_label_->setVisible(!first_run);
}

void LaunchpadScreen::refresh_continue_visibility() {
    if (!btn_continue_) return;

    // Show the button iff something restorable exists. Two sources of truth:
    //   1. last_loaded_uuid pin (set by every successful WorkspaceShell::apply
    //      of a non-auto layout) — the explicit "what was open last time"
    //   2. most recent kind='auto' snapshot from the ring — the "auto-saved
    //      state from a session that ended without a named save"
    bool has_restorable = false;

    auto last = LayoutCatalog::instance().last_loaded_id();
    if (last.is_ok() && !last.value().is_null())
        has_restorable = true;

    if (!has_restorable) {
        if (auto* ring = TerminalShell::instance().snapshot_ring()) {
            auto snaps = ring->latest_auto(/*limit=*/1);
            if (snaps.is_ok() && !snaps.value().isEmpty())
                has_restorable = true;
        }
    }

    btn_continue_->setVisible(has_restorable);
}

} // namespace fincept::screens
