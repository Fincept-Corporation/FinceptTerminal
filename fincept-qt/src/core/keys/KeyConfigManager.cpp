// src/core/keys/KeyConfigManager.cpp
#include "core/keys/KeyConfigManager.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <optional>

namespace fincept {

// Storage key format: "key.refresh", "key.toggle_chat", etc.
static QString storage_key(KeyAction a) {
    switch (a) {
        case KeyAction::Refresh:    return "key.refresh";
        case KeyAction::ToggleChat: return "key.toggle_chat";
        case KeyAction::FocusMode:  return "key.focus_mode";
        case KeyAction::Fullscreen: return "key.fullscreen";
        case KeyAction::Screenshot: return "key.screenshot";
        case KeyAction::NavNext:    return "key.nav_next";
        case KeyAction::NavPrev:    return "key.nav_prev";
        case KeyAction::NavAccept:  return "key.nav_accept";
        case KeyAction::NavEscape:  return "key.nav_escape";
        case KeyAction::NewsNext:   return "key.news_next";
        case KeyAction::NewsPrev:   return "key.news_prev";
        case KeyAction::NewsOpen:   return "key.news_open";
        case KeyAction::NewsClose:  return "key.news_close";
        case KeyAction::RunCell:    return "key.run_cell";
        case KeyAction::RunAndNext: return "key.run_and_next";
        case KeyAction::RenameCell: return "key.rename_cell";
    }
    return {};
}

KeyConfigManager::KeyConfigManager() {
    // Define defaults
    defaults_[KeyAction::Refresh]    = QKeySequence(Qt::Key_F5);
    defaults_[KeyAction::ToggleChat] = QKeySequence(Qt::Key_F9);
    defaults_[KeyAction::FocusMode]  = QKeySequence(Qt::Key_F10);
    defaults_[KeyAction::Fullscreen] = QKeySequence(Qt::Key_F11);
    defaults_[KeyAction::Screenshot] = QKeySequence(Qt::CTRL | Qt::Key_P);
    defaults_[KeyAction::NavNext]    = QKeySequence(Qt::Key_Down);
    defaults_[KeyAction::NavPrev]    = QKeySequence(Qt::Key_Up);
    defaults_[KeyAction::NavAccept]  = QKeySequence(Qt::Key_Tab);
    defaults_[KeyAction::NavEscape]  = QKeySequence(Qt::Key_Escape);
    defaults_[KeyAction::NewsNext]   = QKeySequence("J");
    defaults_[KeyAction::NewsPrev]   = QKeySequence("K");
    defaults_[KeyAction::NewsOpen]   = QKeySequence(Qt::Key_Return);
    defaults_[KeyAction::NewsClose]  = QKeySequence(Qt::Key_Escape);
    defaults_[KeyAction::RunCell]    = QKeySequence(Qt::CTRL | Qt::Key_Return);
    defaults_[KeyAction::RunAndNext] = QKeySequence(Qt::SHIFT | Qt::Key_Return);
    defaults_[KeyAction::RenameCell] = QKeySequence(Qt::Key_F2);

    // Create QActions with default sequences
    for (auto it = defaults_.begin(); it != defaults_.end(); ++it) {
        auto* act = new QAction(this);
        act->setShortcut(it.value());
        act->setShortcutContext(Qt::WindowShortcut);
        actions_[it.key()] = act;
    }

    load_from_storage();
}

KeyConfigManager& KeyConfigManager::instance() {
    static KeyConfigManager s;
    return s;
}

void KeyConfigManager::load_from_storage() {
    auto result = SettingsRepository::instance().get_by_category("keybindings");
    if (!result.is_ok())
        return;
    for (const auto& setting : result.value()) {
        for (auto it = defaults_.begin(); it != defaults_.end(); ++it) {
            if (storage_key(it.key()) == setting.key) {
                QKeySequence seq(setting.value);
                if (!seq.isEmpty()) {
                    actions_[it.key()]->setShortcut(seq);
                    LOG_DEBUG("KeyConfig", QString("Loaded %1 = %2").arg(setting.key, setting.value));
                }
                break;
            }
        }
    }
}

void KeyConfigManager::persist(KeyAction a, const QKeySequence& seq) {
    SettingsRepository::instance().set(storage_key(a), seq.toString(), "keybindings");
}

QAction* KeyConfigManager::action(KeyAction a) const {
    return actions_.value(a, nullptr);
}

QKeySequence KeyConfigManager::key(KeyAction a) const {
    if (auto* act = actions_.value(a, nullptr))
        return act->shortcut();
    return {};
}

QKeySequence KeyConfigManager::default_key(KeyAction a) const {
    return defaults_.value(a);
}

QString KeyConfigManager::display_name(KeyAction a) const {
    switch (a) {
        case KeyAction::Refresh:    return "Refresh Screen";
        case KeyAction::ToggleChat: return "Toggle Chat";
        case KeyAction::FocusMode:  return "Focus Mode";
        case KeyAction::Fullscreen: return "Fullscreen";
        case KeyAction::Screenshot: return "Screenshot";
        case KeyAction::NavNext:    return "Next Suggestion";
        case KeyAction::NavPrev:    return "Previous Suggestion";
        case KeyAction::NavAccept:  return "Autocomplete";
        case KeyAction::NavEscape:  return "Dismiss";
        case KeyAction::NewsNext:   return "Next Article";
        case KeyAction::NewsPrev:   return "Previous Article";
        case KeyAction::NewsOpen:   return "Open Article";
        case KeyAction::NewsClose:  return "Close Panel";
        case KeyAction::RunCell:    return "Run Cell";
        case KeyAction::RunAndNext: return "Run & Move Next";
        case KeyAction::RenameCell: return "Rename Cell";
    }
    return {};
}

QString KeyConfigManager::group_name(KeyAction a) const {
    switch (a) {
        case KeyAction::Refresh:
        case KeyAction::ToggleChat:
        case KeyAction::FocusMode:
        case KeyAction::Fullscreen:
        case KeyAction::Screenshot:
            return "Global";
        case KeyAction::NavNext:
        case KeyAction::NavPrev:
        case KeyAction::NavAccept:
        case KeyAction::NavEscape:
            return "Navigation";
        case KeyAction::NewsNext:
        case KeyAction::NewsPrev:
        case KeyAction::NewsOpen:
        case KeyAction::NewsClose:
            return "News";
        case KeyAction::RunCell:
        case KeyAction::RunAndNext:
        case KeyAction::RenameCell:
            return "Code Editor";
    }
    return {};
}

QList<KeyAction> KeyConfigManager::all_actions() const {
    return {
        KeyAction::Refresh, KeyAction::ToggleChat, KeyAction::FocusMode,
        KeyAction::Fullscreen, KeyAction::Screenshot,
        KeyAction::NavNext, KeyAction::NavPrev, KeyAction::NavAccept, KeyAction::NavEscape,
        KeyAction::NewsNext, KeyAction::NewsPrev, KeyAction::NewsOpen, KeyAction::NewsClose,
        KeyAction::RunCell, KeyAction::RunAndNext, KeyAction::RenameCell,
    };
}

void KeyConfigManager::set_key(KeyAction a, const QKeySequence& seq) {
    if (auto* act = actions_.value(a, nullptr)) {
        act->setShortcut(seq);
        persist(a, seq);
        emit key_changed(a, seq);
        LOG_INFO("KeyConfig", QString("Rebound %1 to %2").arg(display_name(a), seq.toString()));
    }
}

void KeyConfigManager::reset_key(KeyAction a) {
    const QKeySequence def = defaults_.value(a);
    if (auto* act = actions_.value(a, nullptr)) {
        act->setShortcut(def);
        SettingsRepository::instance().remove(storage_key(a));
        emit key_changed(a, def);
    }
}

void KeyConfigManager::reset_all() {
    SettingsRepository::instance().clear_category("keybindings");
    for (auto it = defaults_.begin(); it != defaults_.end(); ++it) {
        if (auto* act = actions_.value(it.key(), nullptr)) {
            act->setShortcut(it.value());
            emit key_changed(it.key(), it.value());
        }
    }
    LOG_INFO("KeyConfig", "All keybindings reset to defaults");
}

std::optional<KeyAction> KeyConfigManager::find_conflict(const QKeySequence& seq, KeyAction exclude) const {
    for (auto it = actions_.begin(); it != actions_.end(); ++it) {
        if (it.key() == exclude)
            continue;
        if (it.value()->shortcut() == seq)
            return it.key();
    }
    return std::nullopt;
}

} // namespace fincept
