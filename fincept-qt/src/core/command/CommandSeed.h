#pragma once
// CommandSeed — the rows the search box knows without any screen registering them:
// every screen (title, the old command bar's aliases, keywords), the screens' own
// sub-tab / panel / module ids, Settings sections and fields, Docs topics and help
// pages, and the shell's menu actions. Qt Core only (tests link it); titles are
// translated at call time (QT_TRANSLATE_NOOP tables, context "CommandSeed").
//
// Sub-tab rows publish nav.switch_screen {screen_id, section, focus}; until a screen
// implements ISectionLinked the link opens the screen on its current tab (Settings
// already honours `section`). A screen that registers the same row id replaces the
// seeded row (CommandRegistry: same id = replace).
//
// Action rows the shell runs are data here: args {"toolbar_action": "<id>"} (the
// ToolBar's action_triggered ids), {"theme": "Light|Dark|System"}.
// tests/tst_command_seed.cpp: every DockScreenRouter screen id has a row.

#include "core/command/CommandRegistry.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace fincept::command::seed {

struct ScreenInfo {
    QString id;
    QString title;       ///< translated
    QStringList aliases; ///< short codes typed exactly (the old command bar's aliases)
    QStringList keywords;
    QString description; ///< translated, plain words
};

/// Every screen the router registers, plus "chat_mode" (a window mode the app opens by screen id).
QList<ScreenInfo> screens();
/// Translated title of a screen id ("" when unknown).
QString screen_title(const QString& id);

QList<CommandItem> screen_items();  ///< group screen, one per screen (id "screen:<id>")
QList<CommandItem> section_items(); ///< group screen, sub-tabs / panels / modules (id "<screen>.<section>")
QList<CommandItem> setting_items(); ///< group setting (id "settings.<section>[.<field>]")
QList<CommandItem> help_items();    ///< group help (docs topics, help pages)
QList<CommandItem> action_items();  ///< group action (shell menu actions, theme, common intents)
QList<CommandItem> all();

/// Owner string of every seeded row ("seed"): unregister_owner("seed") removes them.
inline constexpr char kOwner[] = "seed";

} // namespace fincept::command::seed
