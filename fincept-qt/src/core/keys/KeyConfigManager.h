// src/core/keys/KeyConfigManager.h
#pragma once
#include "core/keys/KeyActions.h"

#include <QAction>
#include <QKeySequence>
#include <QMap>
#include <QObject>
#include <QString>
#include <optional>

namespace fincept {

/// Singleton that owns one QAction per KeyAction.
/// Widgets connect to action(a)->triggered instead of QShortcut::activated.
/// Calling set_key() updates the QAction shortcut live and persists to SettingsRepository.
class KeyConfigManager : public QObject {
    Q_OBJECT
  public:
    static KeyConfigManager& instance();

    /// The QAction for this action. Widgets connect to its triggered() signal.
    QAction* action(KeyAction a) const;

    /// Current key sequence for this action.
    QKeySequence key(KeyAction a) const;

    /// Default key sequence for this action.
    QKeySequence default_key(KeyAction a) const;

    /// Human-readable name shown in the Settings UI.
    QString display_name(KeyAction a) const;

    /// Group name for the Settings UI ("Global", "News", "Code Editor", "Navigation").
    QString group_name(KeyAction a) const;

    /// All actions in definition order.
    QList<KeyAction> all_actions() const;

    /// Update the key for an action. Updates QAction shortcut and persists to DB.
    void set_key(KeyAction a, const QKeySequence& seq);

    /// Reset one action to its default.
    void reset_key(KeyAction a);

    /// Reset all actions to defaults and clear the "keybindings" category in DB.
    void reset_all();

    /// Returns the KeyAction that currently uses seq, or nullopt if none.
    /// Pass exclude to skip the action being edited (conflict check).
    std::optional<KeyAction> find_conflict(const QKeySequence& seq, KeyAction exclude) const;

  signals:
    void key_changed(fincept::KeyAction a, QKeySequence seq);

  private:
    KeyConfigManager();
    void load_from_storage();
    void persist(KeyAction a, const QKeySequence& seq);

    QMap<KeyAction, QAction*>     actions_;
    QMap<KeyAction, QKeySequence> defaults_;
};

} // namespace fincept
