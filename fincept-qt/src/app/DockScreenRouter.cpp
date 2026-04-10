#include "app/DockScreenRouter.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "screens/IStatefulScreen.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

namespace fincept {

QString DockScreenRouter::title_for_id(const QString& id) {
    static const QHash<QString, QString> titles = {
        {"dashboard", "Dashboard"},
        {"markets", "Markets"},
        {"crypto_trading", "Crypto Trading"},
        {"equity_trading", "Equity Trading"},
        {"algo_trading", "Algo Trading"},
        {"backtesting", "Backtesting"},
        {"portfolio", "Portfolio"},
        {"watchlist", "Watchlist"},
        {"news", "News"},
        {"ai_chat", "AI Chat"},
        {"equity_research", "Equity Research"},
        {"economics", "Economics"},
        {"dbnomics", "DBnomics"},
        {"akshare", "AkShare Data"},
        {"asia_markets", "Asia Markets"},
        {"geopolitics", "Geopolitics"},
        {"gov_data", "Gov Data"},
        {"maritime", "Maritime"},
        {"polymarket", "Polymarket"},
        {"relationship_map", "Relationship Map"},
        {"derivatives", "Derivatives"},
        {"alt_investments", "Alt Investments"},
        {"ma_analytics", "M&A Analytics"},
        {"surface_analytics", "Surface Analytics"},
        {"quantlib", "QuantLib"},
        {"ai_quant_lab", "AI Quant Lab"},
        {"alpha_arena", "Alpha Arena"},
        {"agent_config", "Agent Config"},
        {"mcp_servers", "MCP Servers"},
        {"node_editor", "Node Editor"},
        {"code_editor", "Code Editor"},
        {"excel", "Excel"},
        {"report_builder", "Report Builder"},
        {"trade_viz", "Trade Viz"},
        {"data_sources", "Data Sources"},
        {"data_mapping", "Data Mapping"},
        {"file_manager", "File Manager"},
        {"notes", "Notes"},
        {"forum", "Forum"},
        {"docs", "Docs"},
        {"support", "Support"},
        {"about", "About"},
        {"profile", "Profile"},
        {"settings", "Settings"},
        {"contact", "Contact"},
        {"terms", "Terms"},
        {"privacy", "Privacy"},
        {"trademarks", "Trademarks"},
        {"help", "Help"},
    };
    return titles.value(id, id);
}

DockScreenRouter::DockScreenRouter(ads::CDockManager* manager, QObject* parent) : QObject(parent), manager_(manager) {
}

void DockScreenRouter::register_screen(const QString& id, QWidget* screen) {
    // Store the screen widget — the CDockWidget is only created on first navigate().
    // This prevents all eagerly registered screens from appearing at startup.
    screens_[id] = screen;
    factories_.remove(id);
}

void DockScreenRouter::register_factory(const QString& id, ScreenFactory factory) {
    factories_[id] = std::move(factory);
}

void DockScreenRouter::navigate(const QString& id, bool exclusive) {
    LOG_INFO("DockRouter",
             QString(">>> navigate('%1', exclusive=%2) panel_count=%3").arg(id).arg(exclusive).arg(panel_count_));
    auto* dw = find_dock_widget(id);

    bool needs_add = false;
    if (!dw) {
        // Screen not yet in dock manager — check if we know how to create it
        if (!factories_.contains(id) && !screens_.contains(id)) {
            LOG_WARN("DockRouter", "Unknown screen: " + id);
            return;
        }
        dw = create_dock_widget(id);
        needs_add = true;
    } else if (!dw->dockManager()) {
        // Widget exists but was never added to the dock manager (pre-created
        // by ensure_all_registered for state restore, but restoreState was
        // skipped or didn't place this widget).
        needs_add = true;
    }

    if (screens_.contains(id) && dw->widget() != screens_[id]) {
        QWidget* old = dw->widget();
        dw->setWidget(screens_[id]);
        if (old) {
            old->setParent(nullptr);
            old->deleteLater();
        }
    }

    materialize_screen(id);

    if (exclusive) {
        for (auto* area : manager_->openedDockAreas()) {
            for (auto* other : area->openedDockWidgets()) {
                if (other != dw)
                    other->toggleView(false);
            }
        }
        for (auto* other : manager_->dockWidgetsMap()) {
            if (other != dw && other->isAutoHide())
                other->toggleView(false);
        }
        grid_top_left_ = grid_top_right_ = grid_bottom_left_ = grid_bottom_right_ = nullptr;
        panel_count_ = 0;
    }

    // Determine whether this widget needs to be physically placed into a new
    // split area, or whether it already has its own dedicated area.
    //
    // ensure_all_registered() pre-creates every dock widget and groups them all
    // into a SINGLE shared dock area (all hidden). We detect this by checking
    // whether dw's area contains other dock widgets (shared) vs being solo.
    // We check dockWidgets() (all, not just open) because ensure_all_registered
    // hides them all — openedDockWidgets() would be empty and miss the poisoning.
    auto* existing_area = dw->dockAreaWidget();
    const bool area_is_poisoned = existing_area && existing_area->dockWidgets().size() > 1;
    const bool must_place = needs_add || !existing_area || area_is_poisoned;

    // ── Helper: sync grid tracking from currently open areas ────────────────
    // After restoreState(), exclusive navigate, or user drag, the grid pointers
    // may be stale. This lambda rebuilds them from whatever ADS actually has.
    auto sync_grid_from_reality = [this]() {
        const auto opened = manager_->openedDockAreas();
        panel_count_ = std::min(static_cast<int>(opened.size()), 4);

        // Invalidate pointers that no longer exist
        if (grid_top_left_ && !opened.contains(grid_top_left_))
            grid_top_left_ = nullptr;
        if (grid_top_right_ && !opened.contains(grid_top_right_))
            grid_top_right_ = nullptr;
        if (grid_bottom_left_ && !opened.contains(grid_bottom_left_))
            grid_bottom_left_ = nullptr;
        if (grid_bottom_right_ && !opened.contains(grid_bottom_right_))
            grid_bottom_right_ = nullptr;

        // Fill in missing pointers from opened areas
        int idx = 0;
        if (!grid_top_left_ && idx < opened.size())
            grid_top_left_ = opened.at(idx);
        if (grid_top_left_)
            idx = opened.indexOf(grid_top_left_) + 1;
        if (!grid_top_right_ && idx < opened.size())
            grid_top_right_ = opened.at(idx);
        if (grid_top_right_)
            idx = opened.indexOf(grid_top_right_) + 1;
        if (!grid_bottom_left_ && idx < opened.size())
            grid_bottom_left_ = opened.at(idx);
        if (grid_bottom_left_)
            idx = opened.indexOf(grid_bottom_left_) + 1;
        if (!grid_bottom_right_ && idx < opened.size())
            grid_bottom_right_ = opened.at(idx);

        LOG_INFO("DockRouter", QString("GRID synced: panel_count=%1 opened=%2 TL=%3 TR=%4 BL=%5 BR=%6")
                                   .arg(panel_count_)
                                   .arg(opened.size())
                                   .arg(grid_top_left_ ? "ok" : "-")
                                   .arg(grid_top_right_ ? "ok" : "-")
                                   .arg(grid_bottom_left_ ? "ok" : "-")
                                   .arg(grid_bottom_right_ ? "ok" : "-"));
    };

    if (exclusive) {
        // Exclusive: place into full container (always, regardless of must_place)
        auto* area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        grid_top_left_ = area;
        grid_top_right_ = grid_bottom_left_ = grid_bottom_right_ = nullptr;
        panel_count_ = 1;

    } else if (must_place) {
        // Sync grid from reality before placing
        sync_grid_from_reality();

        LOG_INFO("DockRouter", QString("GRID [%1] must_place panel_count=%2").arg(id).arg(panel_count_));

        if (panel_count_ == 0) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 1 (full)").arg(id));
            auto* area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            grid_top_left_ = area;
            panel_count_ = 1;

        } else if (panel_count_ == 1 && grid_top_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 2 (right of TL)").arg(id));
            auto* area = manager_->addDockWidget(ads::RightDockWidgetArea, dw, grid_top_left_);
            grid_top_right_ = area;
            panel_count_ = 2;

        } else if (panel_count_ == 2 && grid_top_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 3 (below TL)").arg(id));
            auto* area = manager_->addDockWidget(ads::BottomDockWidgetArea, dw, grid_top_left_);
            grid_bottom_left_ = area;
            panel_count_ = 3;

        } else if (panel_count_ == 3 && grid_bottom_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 4 (right of BL)").arg(id));
            auto* area = manager_->addDockWidget(ads::RightDockWidgetArea, dw, grid_bottom_left_);
            grid_bottom_right_ = area;
            panel_count_ = 4;

        } else {
            // 5th+ panel or missing grid pointer: tab into last area
            LOG_INFO("DockRouter", QString("GRID [%1] -> tab into last (panel_count=%2)").arg(id).arg(panel_count_));
            ads::CDockAreaWidget* target =
                grid_bottom_right_
                    ? grid_bottom_right_
                    : (manager_->openedDockAreas().isEmpty() ? nullptr : manager_->openedDockAreas().last());
            if (target)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            panel_count_++;
        }

    } else {
        // Widget already has a dedicated area — just show it.
        // But sync grid tracking so it knows about this area for future placements.
        LOG_INFO("DockRouter", QString("GRID [%1] already placed, syncing grid").arg(id));
        sync_grid_from_reality();
    }

    // Always call toggleView(true) — addDockWidget() marks the widget as
    // not-closed but does NOT make the tab visible. Without this unconditional
    // call, the tab stays hidden because isClosed() returns false and the
    // previous conditional `if (isClosed()) toggleView(true)` was skipped.
    dw->toggleView(true);
    // ADS does not restore CDockWidgetTab visibility when toggleView(true)
    // re-opens a previously hidden widget — the tab stays hidden from the
    // ensure_all_registered() phase where all widgets were toggleView(false)'d.
    // Force the tab visible so the label text appears in the title bar.
    if (auto* tab = dw->tabWidget()) {
        if (tab->isHidden())
            tab->setVisible(true);
    }
    dw->raise();
    dw->setAsCurrentTab();

    apply_ads_theme();

    current_id_ = id;
    SessionManager::instance().set_last_screen(id);
    LOG_INFO("DockRouter", "Navigated to: " + id);
    emit screen_changed(id);
}

void DockScreenRouter::tab_into(const QString& id) {
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter", "Unknown screen: " + id);
        return;
    }

    auto* dw = find_dock_widget(id);
    bool needs_add = !dw || !dw->dockManager();

    if (!dw) {
        dw = create_dock_widget(id);
        needs_add = true;
    }

    materialize_screen(id);

    // If already visible somewhere just raise it — no need to re-tab.
    if (!dw->isClosed() && dw->dockAreaWidget()) {
        dw->raise();
        dw->setAsCurrentTab();
        current_id_ = id;
        emit screen_changed(id);
        return;
    }

    // Find the focused/active dock area to tab into.
    // Prefer the focused widget's area; fall back to the first open area.
    ads::CDockAreaWidget* target_area = nullptr;
    if (auto* focused = manager_->focusedDockWidget())
        target_area = focused->dockAreaWidget();
    if (!target_area) {
        const auto opened = manager_->openedDockAreas();
        if (!opened.isEmpty())
            target_area = opened.first();
    }

    if (needs_add || !dw->dockAreaWidget()) {
        if (target_area)
            // Tab into the existing area — CenterDockWidgetArea = same area, new tab
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target_area);
        else
            // No area open yet — open as the sole screen
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
    }

    dw->toggleView(true);
    if (auto* tab = dw->tabWidget()) {
        if (tab->isHidden())
            tab->setVisible(true);
    }
    dw->raise();
    dw->setAsCurrentTab();

    current_id_ = id;
    SessionManager::instance().set_last_screen(id);
    LOG_INFO("DockRouter", "Tabbed into: " + id);
    emit screen_changed(id);
}

void DockScreenRouter::add_alongside(const QString& primary, const QString& secondary) {
    // If primary is already open, just add secondary into the next grid slot
    // without resetting the layout. This allows building up to a 2x2 grid
    // (4 panels) incrementally via repeated "add" commands.
    auto* primary_dw = find_dock_widget(primary);
    bool primary_visible = primary_dw && !primary_dw->isClosed() && primary_dw->dockAreaWidget();

    LOG_INFO("DockRouter", QString("add_alongside: primary=%1 visible=%2 secondary=%3 panel_count=%4")
                               .arg(primary)
                               .arg(primary_visible)
                               .arg(secondary)
                               .arg(panel_count_));

    if (primary_visible) {
        // Primary already showing — add secondary non-exclusively
        navigate(secondary, false);
    } else {
        // Primary not showing — open it exclusively, then split secondary alongside
        navigate(primary, true);
        navigate(secondary, false);
    }

    // Re-focus the primary so it stays the "main" panel — the user typed
    // "primary add secondary", meaning primary is what they're working in.
    primary_dw = find_dock_widget(primary);
    if (primary_dw && !primary_dw->isClosed()) {
        primary_dw->raise();
        primary_dw->setAsCurrentTab();
        current_id_ = primary;
        emit screen_changed(primary);
    }
}

void DockScreenRouter::remove_screen(const QString& primary) {
    // Close all panels except primary, which expands to fill the area.
    auto* keep = find_dock_widget(primary);
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (dw != keep && !dw->isClosed())
            dw->toggleView(false);
    }
    if (keep) {
        if (keep->isClosed())
            keep->toggleView(true);
        keep->raise();
        keep->setAsCurrentTab();
        current_id_ = primary;
        emit screen_changed(primary);
    }
}

void DockScreenRouter::replace_screen(const QString& primary, const QString& secondary) {
    // Navigate to primary (ensures it exists), close everything else, then
    // navigate to secondary in exclusive mode so it fills the full area.
    Q_UNUSED(primary)
    navigate(secondary, true);
}

void DockScreenRouter::ensure_all_registered() {
    // Pre-create CDockWidgets for ALL known screens and register them
    // with the dock manager so restoreState() can find them by objectName
    // in the DockWidgetsMap. All widgets are added as tabs into a single
    // dock area (not separate splits!) and immediately closed. restoreState()
    // will reopen and reposition only those that were visible in the saved layout.
    QStringList all_ids;
    for (auto it = factories_.cbegin(); it != factories_.cend(); ++it)
        all_ids.append(it.key());
    for (auto it = screens_.cbegin(); it != screens_.cend(); ++it) {
        if (!all_ids.contains(it.key()))
            all_ids.append(it.key());
    }

    ads::CDockAreaWidget* first_area = nullptr;
    for (const QString& id : all_ids) {
        if (!dock_widgets_.contains(id)) {
            auto* dw = create_dock_widget(id);
            // Add all widgets as tabs in the SAME dock area to avoid
            // creating 47 vertical splits. The first widget creates the area,
            // all subsequent ones tab into it.
            if (first_area)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, first_area);
            else
                first_area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            dw->toggleView(false); // start hidden
        }
    }
    // Apply theme directly to all title bars and tabs created above.
    apply_ads_theme();
}

ads::CDockWidget* DockScreenRouter::find_dock_widget(const QString& id) const {
    return dock_widgets_.value(id, nullptr);
}

QStringList DockScreenRouter::all_screen_ids() const {
    QStringList ids;
    ids.reserve(dock_widgets_.size() + factories_.size());
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it)
        ids.append(it.key());
    for (auto it = factories_.cbegin(); it != factories_.cend(); ++it) {
        if (!ids.contains(it.key()))
            ids.append(it.key());
    }
    return ids;
}

bool DockScreenRouter::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() != QEvent::MouseButtonDblClick)
        return QObject::eventFilter(obj, event);

    auto* lbl = qobject_cast<QLabel*>(obj);
    if (!lbl)
        return QObject::eventFilter(obj, event);

    const QString id = lbl->property("dock_id").toString();
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return QObject::eventFilter(obj, event);

    // Show a QLineEdit overlaid on the label for inline rename.
    // Parent it to the label's parent (the tab frame) so geometry is correct.
    QWidget* tab_frame = lbl->parentWidget();
    auto* editor = new QLineEdit(tab_frame);
    editor->setText(dw->windowTitle());
    editor->selectAll();
    editor->setFrame(false);
    editor->setStyleSheet("QLineEdit{"
                          "  background:#1a1a1a;"
                          "  color:#ffffff;"
                          "  border:1px solid #d97706;"
                          "  padding:0 4px;"
                          "  font-size:11px;"
                          "  font-family:'Consolas',monospace;"
                          "}");
    // Map label rect into the tab frame's coordinate space
    editor->setGeometry(tab_frame->mapFromGlobal(lbl->mapToGlobal(QPoint(0, 0))).x(), 2, lbl->width(),
                        tab_frame->height() - 4);
    editor->show();
    editor->raise();
    editor->setFocus();

    auto commit = [dw, editor]() {
        const QString name = editor->text().trimmed();
        if (!name.isEmpty()) {
            // setWindowTitle on CDockWidget propagates to the tab label via
            // Qt's QEvent::WindowTitleChange handling inside CDockWidget.
            dw->setWindowTitle(name);
        }
        editor->deleteLater();
    };

    connect(editor, &QLineEdit::returnPressed, editor, commit);
    connect(editor, &QLineEdit::editingFinished, editor, commit);

    // Cancel on Escape via a local event filter
    struct EscFilter : QObject {
        QLineEdit* ed;
        explicit EscFilter(QLineEdit* e) : QObject(e), ed(e) {}
        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
                ed->deleteLater();
                return true;
            }
            return false;
        }
    };
    editor->installEventFilter(new EscFilter(editor));

    return true; // consume double-click so ADS doesn't float the widget
}

ads::CDockWidget* DockScreenRouter::create_dock_widget(const QString& id) {
    if (dock_widgets_.contains(id))
        return dock_widgets_[id];

    auto* dw = new ads::CDockWidget(title_for_id(id));
    dw->setObjectName(id);

    // Pinnable is intentionally excluded: the auto-hide pin button converts panels
    // to collapsible sidebars, which causes them to disappear when another panel
    // is opened alongside them. Movable/Floatable/Closable/Focusable are kept.
    dw->setFeatures(ads::CDockWidget::DockWidgetMovable | ads::CDockWidget::DockWidgetFloatable |
                    ads::CDockWidget::DockWidgetClosable | ads::CDockWidget::DockWidgetFocusable);

    // Use dock-widget minimum size (not content size) so screens with large
    // internal canvases (like DashboardCanvas) don't prevent side-by-side splits.
    dw->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    dw->setMinimumWidth(200);

    // Screen widgets must allow shrinking so ADS can create side-by-side splits.
    // Factory screens get a lightweight placeholder here; the real widget is swapped
    // in on first navigation via materialize_screen().
    if (screens_.contains(id)) {
        screens_[id]->setMinimumWidth(0);
        dw->setWidget(screens_[id]);
    } else {
        auto* placeholder = new QWidget;
        auto* lbl = new QLabel(title_for_id(id));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color:#555;font-size:16px;");
        auto* vl = new QVBoxLayout(placeholder);
        vl->addWidget(lbl);
        dw->setWidget(placeholder);
    }

    // Intercept double-click on the title label inside CDockWidgetTab to allow
    // inline rename. We use the label (objectName "dockWidgetTabLabel"), NOT the
    // tab frame — ADS already uses tab frame double-click to undock/float.
    if (auto* tab = dw->tabWidget()) {
        if (auto* lbl = tab->findChild<QLabel*>("dockWidgetTabLabel")) {
            lbl->installEventFilter(this);
            lbl->setProperty("dock_id", id);
        }
    }

    // Restore a previously saved custom title (user may have renamed this tab).
    const QString saved_title = load_tab_title(id);
    if (!saved_title.isEmpty())
        dw->setWindowTitle(saved_title);

    // Persist the title whenever the user renames it via the inline editor.
    connect(dw, &ads::CDockWidget::titleChanged, this, [this, id](const QString& title) {
        if (title != title_for_id(id)) // only save if user actually changed it
            save_tab_title(id, title);
    });

    // Materialize on first show; save state on hide.
    // Grid tracking (panel_count_, grid_*) is NOT updated here — hide signals
    // fire during exclusive resets and would corrupt the counter before the
    // new panel is placed. Grid is updated only inside navigate() itself.
    connect(dw, &ads::CDockWidget::visibilityChanged, this, [this, id](bool visible) {
        if (visible) {
            materialize_screen(id);
        } else {
            save_screen_state(id);
        }
    });

    dock_widgets_[id] = dw;
    return dw;
}

void DockScreenRouter::materialize_screen(const QString& id) {
    if (screens_.contains(id))
        return;

    auto fit = factories_.find(id);
    if (fit == factories_.end())
        return;

    LOG_INFO("DockRouter", "Lazy-constructing screen: " + id);
    QWidget* screen = nullptr;
    try {
        screen = fit.value()();
    } catch (const std::exception& e) {
        LOG_ERROR("DockRouter", QString("Factory threw for '%1': %2").arg(id, e.what()));
    } catch (...) {
        LOG_ERROR("DockRouter", QString("Factory threw unknown exception for '%1'").arg(id));
    }
    if (!screen) {
        LOG_ERROR("DockRouter", "Factory returned null for: " + id);
        return;
    }
    screen->setMinimumWidth(0);
    factories_.erase(fit);

    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return;

    QWidget* old = dw->widget();
    dw->setWidget(screen);
    if (old) {
        old->setParent(nullptr);
        old->deleteLater();
    }
    screens_[id] = screen;
    restore_screen_state(id);
}

// ── Tab title persistence ─────────────────────────────────────────────────────

void DockScreenRouter::save_tab_title(const QString& id, const QString& title) {
    SessionManager::instance().save_tab_state("title/" + id, {{"title", title}});
}

QString DockScreenRouter::load_tab_title(const QString& id) const {
    const auto state = SessionManager::instance().load_tab_state("title/" + id);
    return state.value("title").toString();
}

// ── Screen state persistence (IStatefulScreen opt-in) ────────────────────────

void DockScreenRouter::save_screen_state(const QString& id) {
    auto* screen = screens_.value(id, nullptr);
    if (!screen)
        return;
    auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen);
    if (!stateful)
        return;
    ScreenStateManager::instance().save_now(stateful);
}

void DockScreenRouter::restore_screen_state(const QString& id) {
    auto* screen = screens_.value(id, nullptr);
    if (!screen)
        return;
    auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen);
    if (!stateful)
        return;
    ScreenStateManager::instance().restore(stateful);
}

void DockScreenRouter::apply_ads_theme() {
    // ADS's CSS (build_ads_qss on CDockManager) handles all ADS widget styling.
    // This function ensures tab widgets and labels don't have stale widget-level
    // QSS that could interfere with ADS's CSS cascade, and forces hidden tabs
    // visible (they can get stuck hidden after ensure_all_registered()).
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (auto* tab = dw->tabWidget()) {
            if (!tab->styleSheet().isEmpty())
                tab->setStyleSheet(QString());
            if (tab->isHidden() && !dw->isClosed())
                tab->setVisible(true);
            for (auto* lbl : tab->findChildren<QLabel*>()) {
                if (!lbl->styleSheet().isEmpty())
                    lbl->setStyleSheet(QString());
            }
        }
    }
}

} // namespace fincept
